// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "IPythonScriptPlugin.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleManager.h"
#include "PythonAPI/UInputService.h"
#include "PythonAPI/UPerformanceService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
	struct FWorkflowScenario
	{
		FString Id;
		TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Steps;
		TArray<TSharedPtr<FJsonValue>> Results;
		int32 StepIndex = 0;
		double StartedSeconds = 0.0;
		double StepStartedSeconds = 0.0;
		double WaitUntil = 0.0;
		int32 LogStartChars = -1;
		bool bOwnsPIE = false;
		bool bStopPIE = true;
		bool bTerminal = false;
		int32 AssertionsDeclared = 0;
		int32 AssertionsEvaluated = 0;
		bool bSmoke = false;
	};

	TMap<FString, TSharedPtr<FWorkflowScenario>> GWorkflowScenarios;

	FString ScenarioDir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Scenarios")); }
	FString ScenarioPath(const FString& Id) { return FPaths::Combine(ScenarioDir(), Id + TEXT(".json")); }
	FString ProjectLogPath() { return FPlatformOutputDevices::GetAbsoluteLogFilename(); }

	FString SerializeScenario(const TSharedRef<FJsonObject>& Object)
	{
		FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
	}

	FString ScenarioError(const FString& Message)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), Message); return SerializeScenario(Root);
	}

	bool FingerprintFile(const FString& Path, FString& Hash)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
		if (!Reader) { return false; }
		FSHA1 Digest;
		uint8 Buffer[65536];
		while (Reader->Tell() < Reader->TotalSize() && !Reader->IsError())
		{
			const int64 Count = FMath::Min<int64>(sizeof(Buffer), Reader->TotalSize() - Reader->Tell());
			Reader->Serialize(Buffer, Count); Digest.Update(Buffer, static_cast<uint32>(Count));
		}
		if (Reader->IsError()) { return false; }
		Digest.Final(); uint8 Bytes[20]; Digest.GetHash(Bytes); Hash = BytesToHex(Bytes, 20); return true;
	}

	bool IsAssertion(const FString& Action)
	{
		return Action == TEXT("assert_log") || Action == TEXT("python_assert") || Action == TEXT("python_assert_number") || Action == TEXT("wait_until");
	}

	// Evaluates one Python expression through the editor's Python bridge and returns its repr (the bridge only
	// hands back PyObject_Repr text) or, when evaluation fails, the error text. Shared by python_assert,
	// python_assert_number, click_widget and wait_until.
	static bool EvalPythonRepr(const FString& Expr, FString& OutRepr)
	{
		FPythonCommandEx Command; Command.Command = Expr; Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
		IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
		const bool bExecuted = Python && Python->ExecPythonCommandEx(Command);
		OutRepr = Command.CommandResult;
		return bExecuted;
	}

	// click_widget targets come back as a Python str repr: accept only a plain single-quoted one (no backslash,
	// so no escapes to undo) and strip the quotes.
	bool UnquoteWidgetPathRepr(const FString& Repr, FString& OutPath)
	{
		if (Repr.Len() < 3 || !Repr.StartsWith(TEXT("'")) || !Repr.EndsWith(TEXT("'")) || Repr.Contains(TEXT("\\"))) { return false; }
		OutPath = Repr.Mid(1, Repr.Len() - 2);
		return !OutPath.Contains(TEXT("'"));
	}

	FString HashText(const FString& Text)
	{
		FTCHARToUTF8 Utf8(*Text); uint8 Bytes[20];
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Bytes); return BytesToHex(Bytes, 20);
	}

	// Historical outcome is preserved on disk; callers receive a current validity assessment.
	FString CurrentScenarioReport(const TSharedRef<FJsonObject>& Report)
	{
		TSharedPtr<FJsonObject> Copy;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SerializeScenario(Report)), Copy);
		if (Copy->GetStringField(TEXT("status")) == TEXT("running")) { return SerializeScenario(Copy.ToSharedRef()); }
		const TSharedPtr<FJsonObject>* Provenance = nullptr;
		FString Validity = TEXT("untracked");
		if (Copy->TryGetObjectField(TEXT("provenance"), Provenance))
		{
			const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
			if ((*Provenance)->TryGetArrayField(TEXT("files"), Files) && Files->Num() > 0)
			{
				Validity = TEXT("current");
				for (const auto& Value : *Files)
				{
					const auto File = Value->AsObject(); FString Hash;
					if (!File || !FingerprintFile(File->GetStringField(TEXT("path")), Hash) || Hash != File->GetStringField(TEXT("sha1")))
					{ Validity = TEXT("stale"); break; }
				}
				if ((*Provenance)->GetStringField(TEXT("engineVersion")) != FEngineVersion::Current().ToString()) { Validity = TEXT("stale"); }
			}
		}
		Copy->SetStringField(TEXT("validity"), Validity);
		bool Passed = false; Copy->TryGetBoolField(TEXT("passed"), Passed);
		Copy->SetBoolField(TEXT("historicalPassed"), Passed);
		Copy->SetBoolField(TEXT("verifiedCurrent"), Passed && Validity == TEXT("current"));
		if (Passed && Validity == TEXT("stale"))
		{
			Copy->SetBoolField(TEXT("passed"), false); Copy->SetStringField(TEXT("status"), TEXT("stale"));
		}
		return SerializeScenario(Copy.ToSharedRef());
	}

	void SaveScenario(const FWorkflowScenario& Scenario)
	{
		IFileManager::Get().MakeDirectory(*ScenarioDir(), true);
		const FString Path = ScenarioPath(Scenario.Id), Temp = Path + TEXT(".tmp");
		FFileHelper::SaveStringToFile(SerializeScenario(Scenario.Report), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		IFileManager::Get().Move(*Path, *Temp, true, true, false, true);
	}

	FString ReadScenarioLogDelta(const FWorkflowScenario& Scenario, bool* bReadable = nullptr)
	{
		if (GLog) { GLog->FlushThreadedLogs(); GLog->Flush(); }
		FString Log;
		const bool bOk = FFileHelper::LoadFileToString(Log, *ProjectLogPath(), FFileHelper::EHashOptions::None, FILEREAD_AllowWrite) &&
			Scenario.LogStartChars >= 0 && Scenario.LogStartChars <= Log.Len();
		if (bReadable) { *bReadable = bOk; }
		return bOk ? Log.Mid(Scenario.LogStartChars) : FString();
	}

	bool JsonSuccess(const FString& Json, FString& OutError)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
		{
			OutError = TEXT("operation returned invalid JSON"); return false;
		}
		bool bSuccess = false;
		if (!Object->TryGetBoolField(TEXT("success"), bSuccess)) { bSuccess = !Object->HasField(TEXT("error")); }
		if (!bSuccess)
		{
			if (!Object->TryGetStringField(TEXT("error_message"), OutError)) { Object->TryGetStringField(TEXT("error"), OutError); }
		}
		return bSuccess;
	}

	void FinishScenario(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Status, const FString& Error)
	{
		if (Scenario->bTerminal) { return; }
		Scenario->bTerminal = true;
		bool bTeardownOk = true;
		if (Scenario->bStopPIE && GEditor && GEditor->PlayWorld)
		{
			FString StopError; bTeardownOk = JsonSuccess(UPerformanceService::StopPIE(), StopError);
		}
		const bool bVerified = Scenario->AssertionsDeclared > 0 && Scenario->AssertionsEvaluated == Scenario->AssertionsDeclared;
		const FString FinalStatus = !bTeardownOk ? TEXT("failed") : Status == TEXT("passed") && !bVerified ? (Scenario->bSmoke ? TEXT("smoke_passed") : TEXT("failed")) : Status;
		Scenario->Report->SetStringField(TEXT("status"), FinalStatus);
		Scenario->Report->SetBoolField(TEXT("passed"), FinalStatus == TEXT("passed"));
		Scenario->Report->SetNumberField(TEXT("assertionsDeclared"), Scenario->AssertionsDeclared);
		Scenario->Report->SetNumberField(TEXT("assertionsEvaluated"), Scenario->AssertionsEvaluated);
		Scenario->Report->SetArrayField(TEXT("steps"), Scenario->Results);
		Scenario->Report->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
		Scenario->Report->SetNumberField(TEXT("durationMs"), (FPlatformTime::Seconds() - Scenario->StartedSeconds) * 1000.0);
		Scenario->Report->SetBoolField(TEXT("teardownSucceeded"), bTeardownOk);
		if (!Error.IsEmpty()) { Scenario->Report->SetStringField(TEXT("error"), Error); }
		Scenario->Report->SetStringField(TEXT("logDelta"), ReadScenarioLogDelta(*Scenario).Right(50000));
		SaveScenario(*Scenario);
		UWorkflowService::AttachActiveRunArtifact(ScenarioPath(Scenario->Id), TEXT("pie-scenario"));
	}

	void AddScenarioStepResult(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Action, bool bPassed,
		const FString& Error = FString(), const FString& Actual = FString(), const FString& Expected = FString())
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("index"), Scenario->StepIndex); Result->SetStringField(TEXT("action"), Action);
		if (IsAssertion(Action)) { ++Scenario->AssertionsEvaluated; }
		Result->SetStringField(TEXT("status"), bPassed ? TEXT("passed") : TEXT("failed"));
		Result->SetNumberField(TEXT("durationMs"), (FPlatformTime::Seconds() - Scenario->StepStartedSeconds) * 1000.0);
		if (!Error.IsEmpty()) { Result->SetStringField(TEXT("error"), Error); }
		if (!Actual.IsEmpty()) { Result->SetStringField(TEXT("actual"), Actual); }
		if (!Expected.IsEmpty()) { Result->SetStringField(TEXT("expected"), Expected); }
		Scenario->Results.Add(MakeShared<FJsonValueObject>(Result)); Scenario->Report->SetArrayField(TEXT("steps"), Scenario->Results);
		SaveScenario(*Scenario);
		if (!bPassed) { FinishScenario(Scenario, TEXT("failed"), Error); }
		else { ++Scenario->StepIndex; Scenario->StepStartedSeconds = 0.0; Scenario->WaitUntil = 0.0; }
	}

	bool TickWorkflowScenario(float)
	{
		TArray<FString> Ids; GWorkflowScenarios.GetKeys(Ids);
		for (const FString& Id : Ids)
		{
			const TSharedPtr<FWorkflowScenario> Scenario = GWorkflowScenarios.FindChecked(Id);
			if (Scenario->bTerminal) { continue; }
			if (Scenario->StepIndex >= Scenario->Steps.Num()) { FinishScenario(Scenario, TEXT("passed"), FString()); continue; }
			const TSharedPtr<FJsonObject> Step = Scenario->Steps[Scenario->StepIndex]->AsObject();
			if (!Step.IsValid()) { FinishScenario(Scenario, TEXT("failed"), TEXT("step must be a JSON object")); continue; }
			const FString Action = Step->GetStringField(TEXT("action")).ToLower();
			const double Now = FPlatformTime::Seconds();
			if (Scenario->StepStartedSeconds == 0.0) { Scenario->StepStartedSeconds = Now; }

			if (Action == TEXT("start_pie"))
			{
				if (GEditor && GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true, FString(), TEXT("already running")); }
				else { FString Error; const bool bOk = JsonSuccess(UPerformanceService::StartPIE(), Error); Scenario->bOwnsPIE = bOk; AddScenarioStepResult(Scenario, Action, bOk, Error); }
			}
			else if (Action == TEXT("wait_for_pie"))
			{
				if (GEditor && GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true); }
				else
				{
					double Timeout = 30.0; Step->TryGetNumberField(TEXT("timeout_seconds"), Timeout);
					if (Now - Scenario->StepStartedSeconds > Timeout) { AddScenarioStepResult(Scenario, Action, false, TEXT("timed out waiting for PIE readiness")); }
				}
			}
			else if (Action == TEXT("wait"))
			{
				double Seconds = 0.0; Step->TryGetNumberField(TEXT("seconds"), Seconds);
				if (Scenario->WaitUntil == 0.0) { Scenario->WaitUntil = Now + FMath::Clamp(Seconds, 0.0, 300.0); }
				if (Now >= Scenario->WaitUntil) { AddScenarioStepResult(Scenario, Action, true); }
			}
			else if (Action == TEXT("inject_action"))
			{
				double X = 1.0, Y = 0.0, Z = 0.0; Step->TryGetNumberField(TEXT("x"), X); Step->TryGetNumberField(TEXT("y"), Y); Step->TryGetNumberField(TEXT("z"), Z);
				FString Error; const bool bOk = JsonSuccess(UInputService::InjectAction(Step->GetStringField(TEXT("path")), X, Y, Z), Error);
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("inject_key"))
			{
				FString Event = TEXT("tap"); Step->TryGetStringField(TEXT("event"), Event); FString Error;
				double HoldSeconds = 0.0; Step->TryGetNumberField(TEXT("hold_seconds"), HoldSeconds);
				// focus:"keep" leaves Slate/UMG focus where it is (a focused menu gets the key); default refocuses the game viewport.
				FString Focus; Step->TryGetStringField(TEXT("focus"), Focus); const bool bFocusGameViewport = !Focus.Equals(TEXT("keep"), ESearchCase::IgnoreCase);
				const bool bOk = JsonSuccess(UInputService::InjectKey(Step->GetStringField(TEXT("key")), Event, static_cast<float>(HoldSeconds), bFocusGameViewport), Error);
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("click_widget"))
			{
				FString Expression, Button = TEXT("left"); Step->TryGetStringField(TEXT("expression"), Expression); Step->TryGetStringField(TEXT("button"), Button);
				bool bDouble = false; Step->TryGetBoolField(TEXT("double"), bDouble);
				const FString Expected = TEXT("str repr of a live PIE widget object path");
				FString Repr, Path;
				if (!EvalPythonRepr(Expression, Repr))
				{
					AddScenarioStepResult(Scenario, Action, false, TEXT("TARGET_NOT_STRING: target expression raised"), Repr, Expected);
				}
				else if (!UnquoteWidgetPathRepr(Repr, Path))
				{
					AddScenarioStepResult(Scenario, Action, false, TEXT("TARGET_NOT_STRING: target expression did not return a plain single-quoted str"), Repr, Expected);
				}
				else
				{
					const FString ClickJson = UInputService::InjectClick(Path, Button, bDouble); FString Error;
					const bool bOk = JsonSuccess(ClickJson, Error);
					FString Code; TSharedPtr<FJsonObject> ClickObj;
					if (!bOk && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ClickJson), ClickObj) && ClickObj.IsValid()) { ClickObj->TryGetStringField(TEXT("error_code"), Code); }
					AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : (Code.IsEmpty() ? Error : Code + TEXT(": ") + Error),
						Code == TEXT("WIDGET_NOT_FOUND") ? Repr : ClickJson, bOk ? FString() : Expected);
				}
			}
			else if (Action == TEXT("wait_until"))
			{
				// Multi-tick: re-evaluates every poll_seconds (no result = still pending) until the repr equals
				// expected (passed) or timeout_seconds elapses (failed, with the last repr or error as actual).
				if (Now >= Scenario->WaitUntil)
				{
					FString Expression, Expected; Step->TryGetStringField(TEXT("expression"), Expression); Step->TryGetStringField(TEXT("expected"), Expected);
					double Timeout = 0.0, Poll = 0.1; Step->TryGetNumberField(TEXT("timeout_seconds"), Timeout); Step->TryGetNumberField(TEXT("poll_seconds"), Poll);
					Timeout = FMath::Clamp(Timeout, 0.0, 300.0); Poll = FMath::Clamp(Poll, 0.01, 10.0);
					FString Repr; const bool bExecuted = EvalPythonRepr(Expression, Repr);
					const double Deadline = Scenario->StepStartedSeconds + Timeout;
					if (bExecuted && Repr == Expected) { AddScenarioStepResult(Scenario, Action, true, FString(), Repr, Expected); }
					else if (Now >= Deadline)
					{
						AddScenarioStepResult(Scenario, Action, false, FString::Printf(TEXT("wait_until timed out after %.2fs%s"), Timeout, bExecuted ? TEXT("") : TEXT(" (last evaluation raised)")), Repr, Expected);
					}
					else { Scenario->WaitUntil = FMath::Min(Now + Poll, Deadline); }
				}
			}
			else if (Action == TEXT("assert_log"))
			{
				FString Contains, NotContains; Step->TryGetStringField(TEXT("contains"), Contains); Step->TryGetStringField(TEXT("not_contains"), NotContains);
				bool bReadable = false; const FString Delta = ReadScenarioLogDelta(*Scenario, &bReadable);
				const bool bOk = bReadable && (Contains.IsEmpty() || Delta.Contains(Contains)) && (NotContains.IsEmpty() || !Delta.Contains(NotContains));
				const FString Expected = TEXT("contains: ") + Contains + TEXT("; does not contain: ") + NotContains;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : bReadable ? TEXT("log assertion failed") : TEXT("scenario log unavailable or truncated"), Delta.Right(2000), Expected);
			}
			else if (Action == TEXT("python_assert"))
			{
				FString Expression, Expected; Step->TryGetStringField(TEXT("expression"), Expression); Step->TryGetStringField(TEXT("expected"), Expected);
				FString Repr; const bool bExecuted = EvalPythonRepr(Expression, Repr);
				const bool bOk = bExecuted && Repr == Expected;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("Python assertion failed"), Repr, Expected);
			}
			else if (Action == TEXT("python_assert_number"))
			{
				FString Repr; const bool bExecuted = EvalPythonRepr(Step->GetStringField(TEXT("expression")), Repr);
				double Actual = 0.0; const double Expected = Step->GetNumberField(TEXT("expected"));
				double Tolerance = 0.0; Step->TryGetNumberField(TEXT("tolerance"), Tolerance);
				const FString Op = Step->GetStringField(TEXT("operator"));
				// Unreal's JSON reader expects a container at the root. Wrapping also rejects
				// Python strings, booleans, NaN/Inf and multiple values instead of coercing them.
				TArray<TSharedPtr<FJsonValue>> Numbers;
				const bool bNumeric = bExecuted && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Repr + TEXT("]")), Numbers) &&
					Numbers.Num() == 1 && Numbers[0]->Type == EJson::Number && Numbers[0]->TryGetNumber(Actual) && FMath::IsFinite(Actual);
				const bool bOk = bNumeric && (Op == TEXT("eq") ? FMath::Abs(Actual - Expected) <= Tolerance :
					Op == TEXT("lt") ? Actual < Expected : Op == TEXT("le") ? Actual <= Expected :
					Op == TEXT("gt") ? Actual > Expected : Actual >= Expected);
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("numeric Python assertion failed"), Repr,
					FString::Printf(TEXT("%s %.17g (tolerance %.17g)"), *Op, Expected, Tolerance));
			}
			else if (Action == TEXT("capture_game"))
			{
				FString Name = FString::Printf(TEXT("scenario-%s-step-%d"), *Scenario->Id, Scenario->StepIndex); Step->TryGetStringField(TEXT("name"), Name);
				const FString Path = FPaths::Combine(ScenarioDir(), Scenario->Id + TEXT("-") + Name + TEXT(".png"));
				FScreenshotRequest::RequestScreenshot(Path, true, false, false, FIntRect(), true);
				TArray<TSharedPtr<FJsonValue>> Captures = Scenario->Report->GetArrayField(TEXT("captures")); Captures.Add(MakeShared<FJsonValueString>(Path)); Scenario->Report->SetArrayField(TEXT("captures"), Captures);
				AddScenarioStepResult(Scenario, Action, true, FString(), Path);
			}
			else if (Action == TEXT("console_command"))
			{
				FString Command; Step->TryGetStringField(TEXT("command"), Command); UWorld* World = GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
				const bool bOk = GEngine && World && !Command.IsEmpty(); if (bOk) { GEngine->Exec(World, *Command); }
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("no world or empty command"));
			}
			else if (Action == TEXT("stop_pie"))
			{
				if (!GEditor || !GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true); }
				else { FString Error; AddScenarioStepResult(Scenario, Action, JsonSuccess(UPerformanceService::StopPIE(), Error), Error); }
			}
			else { AddScenarioStepResult(Scenario, Action, false, TEXT("unsupported scenario action: ") + Action); }
		}
		return true;
	}

	FTSTicker::FDelegateHandle GScenarioTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&TickWorkflowScenario), 0.01f);
}

FString UWorkflowService::RunScenario(const FString& ScenarioJson)
{
	TSharedPtr<FJsonObject> Spec;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ScenarioJson), Spec) || !Spec.IsValid()) { return ScenarioError(TEXT("scenario_json must be an object")); }
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Spec->TryGetArrayField(TEXT("steps"), Steps) || Steps->Num() == 0) { return ScenarioError(TEXT("scenario requires a non-empty steps array")); }
	int32 AssertionsDeclared = 0;
	for (const auto& Value : *Steps)
	{
		if (!Value || Value->Type != EJson::Object) { return ScenarioError(TEXT("each step must be an object")); }
		const auto Step = Value->AsObject(); FString Action;
		if (!Step || !Step->TryGetStringField(TEXT("action"), Action)) { return ScenarioError(TEXT("each step requires an action")); }
		Action = Action.ToLower();
		if (Action == TEXT("click_widget"))
		{
			FString Expression, Button = TEXT("left"); bool bDouble = false;
			if (!Step->TryGetStringField(TEXT("expression"), Expression) || Expression.TrimStartAndEnd().IsEmpty()) { return ScenarioError(TEXT("click_widget requires expression")); }
			if (Step->HasField(TEXT("button")) && (!Step->TryGetStringField(TEXT("button"), Button) ||
				!(Button.Equals(TEXT("left"), ESearchCase::IgnoreCase) || Button.Equals(TEXT("right"), ESearchCase::IgnoreCase) || Button.Equals(TEXT("middle"), ESearchCase::IgnoreCase))))
			{ return ScenarioError(TEXT("click_widget button must be left, right or middle")); }
			if (Step->HasField(TEXT("double")) && (!Step->HasTypedField<EJson::Boolean>(TEXT("double")) || !Step->TryGetBoolField(TEXT("double"), bDouble))) { return ScenarioError(TEXT("click_widget double must be boolean")); }
			continue;
		}
		if (Action == TEXT("inject_key") && Step->HasField(TEXT("focus")))
		{
			FString Focus;
			if (!Step->TryGetStringField(TEXT("focus"), Focus) || !(Focus.Equals(TEXT("keep"), ESearchCase::IgnoreCase) || Focus.Equals(TEXT("viewport"), ESearchCase::IgnoreCase)))
			{ return ScenarioError(TEXT("inject_key focus must be keep or viewport")); }
		}
		if (!IsAssertion(Action)) { continue; }
		++AssertionsDeclared;
		FString Expression, Expected, Contains, NotContains;
		if (Action == TEXT("assert_log"))
		{
			Step->TryGetStringField(TEXT("contains"), Contains); Step->TryGetStringField(TEXT("not_contains"), NotContains);
			if (Contains.IsEmpty() && NotContains.IsEmpty()) { return ScenarioError(TEXT("assert_log requires contains or not_contains")); }
		}
		else
		{
			if (!Step->TryGetStringField(TEXT("expression"), Expression) || Expression.TrimStartAndEnd().IsEmpty()) { return ScenarioError(TEXT("assertion requires expression")); }
			if (Action == TEXT("python_assert"))
			{
				if (!Step->TryGetStringField(TEXT("expected"), Expected)) { return ScenarioError(TEXT("python_assert requires string expected")); }
			}
			else if (Action == TEXT("wait_until"))
			{
				double Timeout = 0.0, Poll = 0.1;
				if (!Step->HasTypedField<EJson::String>(TEXT("expected")) || !Step->TryGetStringField(TEXT("expected"), Expected)) { return ScenarioError(TEXT("wait_until requires string expected")); }
				if (!Step->HasTypedField<EJson::Number>(TEXT("timeout_seconds")) || !Step->TryGetNumberField(TEXT("timeout_seconds"), Timeout) || !FMath::IsFinite(Timeout) || Timeout <= 0.0 || Timeout > 300.0)
				{ return ScenarioError(TEXT("wait_until requires timeout_seconds in (0, 300]")); }
				if (Step->HasField(TEXT("poll_seconds")) && (!Step->HasTypedField<EJson::Number>(TEXT("poll_seconds")) || !Step->TryGetNumberField(TEXT("poll_seconds"), Poll) || !FMath::IsFinite(Poll) || Poll < 0.01 || Poll > 10.0))
				{ return ScenarioError(TEXT("wait_until poll_seconds must be in [0.01, 10]")); }
			}
			else
			{
				double Number, Tolerance = 0.0; FString Op;
				if (!Step->HasTypedField<EJson::Number>(TEXT("expected")) || !Step->TryGetNumberField(TEXT("expected"), Number) || !FMath::IsFinite(Number) ||
					!Step->TryGetStringField(TEXT("operator"), Op) || !(Op == TEXT("eq") || Op == TEXT("lt") || Op == TEXT("le") || Op == TEXT("gt") || Op == TEXT("ge")))
				{ return ScenarioError(TEXT("numeric assertion requires finite expected and operator eq/lt/le/gt/ge")); }
				if (Step->HasField(TEXT("tolerance")) && (!Step->HasTypedField<EJson::Number>(TEXT("tolerance")) || !Step->TryGetNumberField(TEXT("tolerance"), Tolerance) || !FMath::IsFinite(Tolerance) || Tolerance < 0 || Op != TEXT("eq")))
				{ return ScenarioError(TEXT("tolerance must be finite, nonnegative, and only used with eq")); }
			}
		}
	}
	bool bSmoke = false;
	if (Spec->HasField(TEXT("smoke")) && (!Spec->HasTypedField<EJson::Boolean>(TEXT("smoke")) || !Spec->TryGetBoolField(TEXT("smoke"), bSmoke))) { return ScenarioError(TEXT("smoke must be boolean")); }
	if (AssertionsDeclared == 0 && !bSmoke) { return ScenarioError(TEXT("scenario requires an assertion; use smoke:true for boot-only checks")); }
	TArray<TSharedPtr<FJsonValue>> Fingerprints;
	const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
	if (Spec->HasField(TEXT("dependencies")))
	{
		if (!Spec->TryGetArrayField(TEXT("dependencies"), Dependencies)) { return ScenarioError(TEXT("dependencies must be an array of file paths")); }
		for (const auto& Value : *Dependencies)
		{
			FString Path, Hash;
			if (!Value || Value->Type != EJson::String || !Value->TryGetString(Path) || Path.IsEmpty()) { return ScenarioError(TEXT("dependency must be a file path")); }
			if (FPaths::IsRelative(Path)) { Path = FPaths::Combine(FPaths::ProjectDir(), Path); }
			Path = FPaths::ConvertRelativePathToFull(Path);
			if (!FingerprintFile(Path, Hash)) { return ScenarioError(TEXT("cannot fingerprint dependency: ") + Path); }
			auto File = MakeShared<FJsonObject>(); File->SetStringField(TEXT("path"), Path); File->SetStringField(TEXT("sha1"), Hash);
			Fingerprints.Add(MakeShared<FJsonValueObject>(File));
		}
	}
	TSharedPtr<FWorkflowScenario> Scenario = MakeShared<FWorkflowScenario>();
	Scenario->AssertionsDeclared = AssertionsDeclared; Scenario->bSmoke = bSmoke;
	auto Provenance = MakeShared<FJsonObject>(); Provenance->SetArrayField(TEXT("files"), Fingerprints);
	Provenance->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	Provenance->SetStringField(TEXT("scenarioSha1"), HashText(ScenarioJson));
	Scenario->Report->SetObjectField(TEXT("provenance"), Provenance);
	Scenario->Id = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	Scenario->Spec = Spec.ToSharedRef(); Scenario->Steps = *Steps; Scenario->StartedSeconds = FPlatformTime::Seconds();
	if (GLog) { GLog->FlushThreadedLogs(); GLog->Flush(); }
	FString CurrentLog;
	if (FFileHelper::LoadFileToString(CurrentLog, *ProjectLogPath(), FFileHelper::EHashOptions::None, FILEREAD_AllowWrite)) { Scenario->LogStartChars = CurrentLog.Len(); }
	const TSharedPtr<FJsonObject>* Teardown = nullptr; if (Spec->TryGetObjectField(TEXT("teardown"), Teardown)) { (*Teardown)->TryGetBoolField(TEXT("stop_pie"), Scenario->bStopPIE); }
	Scenario->Report->SetStringField(TEXT("schema"), TEXT("vibeue.scenario.v1")); Scenario->Report->SetStringField(TEXT("id"), Scenario->Id);
	FString Name = Scenario->Id; Spec->TryGetStringField(TEXT("name"), Name); Scenario->Report->SetStringField(TEXT("name"), Name);
	Scenario->Report->SetStringField(TEXT("status"), TEXT("running")); Scenario->Report->SetStringField(TEXT("startedAtIso"), FDateTime::UtcNow().ToIso8601());
	Scenario->Report->SetArrayField(TEXT("steps"), {}); Scenario->Report->SetArrayField(TEXT("captures"), {});
	TArray<TSharedPtr<FJsonValue>> CompileResults;
	bool bCompileOk = true;
	const TSharedPtr<FJsonObject>* Preflight = nullptr;
	if (Spec->TryGetObjectField(TEXT("preflight"), Preflight))
	{
		bool bSave = false; (*Preflight)->TryGetBoolField(TEXT("save_dirty_assets"), bSave);
		const TArray<TSharedPtr<FJsonValue>>* Blueprints = nullptr;
		if ((*Preflight)->TryGetArrayField(TEXT("compile_blueprints"), Blueprints))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Blueprints)
			{
				const FString Path = Value->AsString(); FString ObjectPath = Path;
				if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPaths::GetBaseFilename(Path); }
				UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
				TSharedRef<FJsonObject> Compile = MakeShared<FJsonObject>(); Compile->SetStringField(TEXT("path"), Path);
				if (!Blueprint) { Compile->SetBoolField(TEXT("success"), false); Compile->SetStringField(TEXT("error"), TEXT("Blueprint not found")); bCompileOk = false; }
				else
				{
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
					const bool bOk = Blueprint->Status != BS_Error; Compile->SetBoolField(TEXT("success"), bOk); bCompileOk &= bOk;
					if (bSave && bOk) { Compile->SetBoolField(TEXT("saved"), UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false)); }
				}
				CompileResults.Add(MakeShared<FJsonValueObject>(Compile));
			}
		}
	}
	// Capture after preflight saves, so the baseline describes the files actually tested.
	for (const auto& Value : Fingerprints)
	{
		const auto File = Value->AsObject(); FString Hash;
		if (!FingerprintFile(File->GetStringField(TEXT("path")), Hash)) { bCompileOk = false; }
		else { File->SetStringField(TEXT("sha1"), Hash); }
	}
	if (Fingerprints.Num() > 0)
	{
		const FString ModulePath = FPaths::ConvertRelativePathToFull(FModuleManager::Get().GetModuleFilename(TEXT("VibeUE"))); FString Hash;
		if (!FingerprintFile(ModulePath, Hash)) { bCompileOk = false; }
		else
		{
			auto File = MakeShared<FJsonObject>(); File->SetStringField(TEXT("path"), ModulePath); File->SetStringField(TEXT("sha1"), Hash);
			Fingerprints.Add(MakeShared<FJsonValueObject>(File));
		}
	}
	Provenance->SetArrayField(TEXT("files"), Fingerprints);
	Scenario->Report->SetArrayField(TEXT("compileResults"), CompileResults); SaveScenario(*Scenario);
	GWorkflowScenarios.Add(Scenario->Id, Scenario);
	if (!bCompileOk) { FinishScenario(Scenario, TEXT("failed"), TEXT("preflight compile or provenance capture failed")); return SerializeScenario(Scenario->Report); }
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), true); Root->SetStringField(TEXT("scenarioId"), Scenario->Id);
	Root->SetStringField(TEXT("status"), TEXT("running")); Root->SetStringField(TEXT("reportPath"), ScenarioPath(Scenario->Id));
	return SerializeScenario(Root);
}

FString UWorkflowService::GetScenario(const FString& ScenarioId)
{
	if (const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId)) { return CurrentScenarioReport((*Found)->Report); }
	FString Text; TSharedPtr<FJsonObject> Report;
	if (!FFileHelper::LoadFileToString(Text, *ScenarioPath(ScenarioId)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Report) || !Report)
	{ return ScenarioError(TEXT("scenario not found or invalid")); }
	return CurrentScenarioReport(Report.ToSharedRef());
}

FString UWorkflowService::CancelScenario(const FString& ScenarioId)
{
	const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId);
	if (!Found) { return ScenarioError(TEXT("scenario not found")); }
	FinishScenario(*Found, TEXT("cancelled"), TEXT("cancelled by caller")); return SerializeScenario((*Found)->Report);
}
