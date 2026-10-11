// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWidgetService.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"
#include "Components/NamedSlotInterface.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/RichTextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/InputKeySelector.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/BackgroundBlur.h"
#include "Components/SafeZone.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/InvalidationBox.h"
#include "Components/RetainerBox.h"
#include "Components/Throbber.h"
#include "Components/CircularThrobber.h"
#include "Components/ListView.h"
#include "Components/TreeView.h"
#include "Components/TileView.h"
#include "Components/ExpandableArea.h"
#include "Components/MenuAnchor.h"
#include "Components/NativeWidgetHost.h"
#include "Components/PanelSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Animation/MovieSceneMarginTrack.h"
#include "Animation/MovieSceneMarginSection.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "PlayInEditorDataTypes.h"
#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SVirtualWindow.h"
#include "Input/HittestGrid.h"
#include "Layout/ArrangedWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "GameFramework/PlayerController.h"
#include "TextureCompiler.h"
#include "Styling/SlateBrush.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "UObject/PropertyIterator.h"
#include "UObject/UnrealType.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMPropertyPath.h"
#include "MVVMViewModelBase.h"
#include "Types/MVVMFieldVariant.h"
#include "ViewModel/MVVMViewModelBlueprint.h"
#include "WidgetBlueprintExtension.h"

// Static list of available widget types
static const TArray<FString> GAvailableWidgetTypes = {
	TEXT("TextBlock"),
	TEXT("Button"),
	TEXT("EditableText"),
	TEXT("EditableTextBox"),
	TEXT("RichTextBlock"),
	TEXT("CheckBox"),
	TEXT("Slider"),
	TEXT("ProgressBar"),
	TEXT("Image"),
	TEXT("Spacer"),
	TEXT("CanvasPanel"),
	TEXT("Overlay"),
	TEXT("HorizontalBox"),
	TEXT("VerticalBox"),
	TEXT("ScrollBox"),
	TEXT("GridPanel"),
	TEXT("WidgetSwitcher"),
	TEXT("ComboBoxString"),
	TEXT("SpinBox"),
	TEXT("MultiLineEditableText"),
	TEXT("MultiLineEditableTextBox"),
	TEXT("InputKeySelector"),
	TEXT("Border"),
	TEXT("SizeBox"),
	TEXT("ScaleBox"),
	TEXT("BackgroundBlur"),
	TEXT("SafeZone"),
	TEXT("UniformGridPanel"),
	TEXT("WrapBox"),
	TEXT("InvalidationBox"),
	TEXT("RetainerBox"),
	TEXT("Throbber"),
	TEXT("CircularThrobber"),
	TEXT("ListView"),
	TEXT("TreeView"),
	TEXT("TileView"),
	TEXT("ExpandableArea"),
	TEXT("MenuAnchor"),
	TEXT("NativeWidgetHost")
};

namespace
{
	enum class EWidgetAnimationTrackType : uint8
	{
		Float,
		Color,
		Margin
	};

	struct FResolvedWidgetProperty
	{
		UObject* TargetObject = nullptr;
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ResolvedPath;
		// The property name with any "Slot." prefix stripped — what value aliases key off.
		FString AliasName;
		// True when the name also matched the slot but the widget's own property won (issue #553).
		bool bAmbiguous = false;
	};

	struct FWidgetAnimationPropertyTarget
	{
		UObject* AnimatedObject = nullptr;
		EWidgetAnimationTrackType TrackType = EWidgetAnimationTrackType::Float;
		FName PropertyName;
		FString PropertyPath;
		int32 MarginChannelIndex = INDEX_NONE;
	};

	TMap<FString, TWeakObjectPtr<UUserWidget>> GPIEWidgetInstances;

	FProperty* FindPropertyCaseInsensitive(UStruct* StructType, const FString& PropertyName)
	{
		if (!StructType)
		{
			return nullptr;
		}

		if (FProperty* Property = StructType->FindPropertyByName(FName(*PropertyName)))
		{
			return Property;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}

		return nullptr;
	}

	bool ResolvePropertyPath(UStruct* StructType, void* ContainerPtr, const FString& PropertyPath, FProperty*& OutProperty, void*& OutValuePtr)
	{
		OutProperty = nullptr;
		OutValuePtr = nullptr;

		if (!StructType || !ContainerPtr || PropertyPath.IsEmpty())
		{
			return false;
		}

		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			return false;
		}

		UStruct* CurrentStruct = StructType;
		void* CurrentContainer = ContainerPtr;

		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			FProperty* Property = FindPropertyCaseInsensitive(CurrentStruct, Segments[Index]);
			if (!Property)
			{
				return false;
			}

			void* NextValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
			if (Index == Segments.Num() - 1)
			{
				OutProperty = Property;
				OutValuePtr = NextValuePtr;
				return true;
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrentStruct = StructProperty->Struct;
				CurrentContainer = NextValuePtr;
				continue;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				UObject* NextObject = ObjectProperty->GetObjectPropertyValue(NextValuePtr);
				if (!NextObject)
				{
					return false;
				}

				CurrentStruct = NextObject->GetClass();
				CurrentContainer = NextObject;
				continue;
			}

			return false;
		}

		return false;
	}

	bool ResolveSpecialWidgetProperty(UWidget* Widget, const FString& PropertyName, UObject*& OutTargetObject, FString& OutResolvedPath)
	{
		if (!Widget)
		{
			return false;
		}

		if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Left");
			return true;
		}

		if (PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Top");
			return true;
		}

		if (PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Right");
			return true;
		}

		if (PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Bottom");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Min X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Minimum.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Min Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Minimum.Y");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Max X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Maximum.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Max Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Maximum.Y");
			return true;
		}

		// Canvas pivot alignment. With anchors at a point (e.g. 0.5,0.5), Alignment 0.5,0.5
		// centers the widget on that point (top-left otherwise).
		if (PropertyName.Equals(TEXT("Alignment X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Alignment.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Alignment Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Alignment.Y");
			return true;
		}

		// Canvas Z-order (#424). UCanvasPanelSlot::ZOrder is a direct int32 UPROPERTY.
		if ((PropertyName.Equals(TEXT("ZOrder"), ESearchCase::IgnoreCase) ||
			 PropertyName.Equals(TEXT("Z Order"), ESearchCase::IgnoreCase)) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("ZOrder");
			return true;
		}

		// Box / Overlay slot alignment + padding + size (#423). These are UPROPERTYs shared by
		// UVerticalBoxSlot / UHorizontalBoxSlot / UOverlaySlot (alignment/padding) and the box slots
		// (Size). Routing by property name keeps this slot-type agnostic.
		if (Widget->Slot)
		{
			if (PropertyName.Equals(TEXT("Horizontal Alignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("HorizontalAlignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("H Align"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("HAlign"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("HorizontalAlignment");
				return true;
			}
			if (PropertyName.Equals(TEXT("Vertical Alignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("VerticalAlignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("V Align"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("VAlign"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("VerticalAlignment");
				return true;
			}
			if (PropertyName.Equals(TEXT("Padding"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("Padding");
				return true;
			}
			if (PropertyName.Equals(TEXT("Padding Left"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Left"); return true; }
			if (PropertyName.Equals(TEXT("Padding Top"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Top"); return true; }
			if (PropertyName.Equals(TEXT("Padding Right"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Right"); return true; }
			if (PropertyName.Equals(TEXT("Padding Bottom"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Bottom"); return true; }
			if (PropertyName.Equals(TEXT("Size Rule"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Size.SizeRule"); return true; }
			if (PropertyName.Equals(TEXT("Size Value"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Size.Value"); return true; }
		}

		return false;
	}

	// When a property name fails to resolve, suggest the alias that actually works. Whole-struct
	// guesses like "Anchors"/"Alignment"/"Size" are the common false-negative that makes an agent
	// wrongly conclude "no such capability exists" — point it at the scalar slot aliases instead.
	// Returns an empty string when there's no specific suggestion.
	FString BuildSlotPropertyHint(UWidget* Widget, const FString& PropertyName)
	{
		const FString Name = PropertyName.TrimStartAndEnd();
		const bool bCanvas = Widget && Widget->Slot && Widget->Slot->IsA<UCanvasPanelSlot>();
		const bool bBoxOrOverlay = Widget && Widget->Slot &&
			(Widget->Slot->IsA<UVerticalBoxSlot>() || Widget->Slot->IsA<UHorizontalBoxSlot>() || Widget->Slot->IsA<UOverlaySlot>());

		auto Eq = [&Name](const TCHAR* S) { return Name.Equals(S, ESearchCase::IgnoreCase); };

		if (Eq(TEXT("Anchors")) || Eq(TEXT("Anchor")))
		{
			return bCanvas || !Widget || !Widget->Slot
				? TEXT("'Anchor Min X', 'Anchor Min Y', 'Anchor Max X', 'Anchor Max Y' (Canvas slot scalars)")
				: TEXT("anchors apply only to Canvas Panel children");
		}
		if (Eq(TEXT("Alignment")))
		{
			if (bCanvas) return TEXT("'Alignment X' and 'Alignment Y' (Canvas pivot)");
			if (bBoxOrOverlay) return TEXT("'Horizontal Alignment' / 'Vertical Alignment' (accepts Fill/Left/Center/Right/Top/Bottom)");
			return TEXT("'Alignment X/Y' (Canvas) or 'Horizontal/Vertical Alignment' (Box/Overlay)");
		}
		if (Eq(TEXT("Position")) || Eq(TEXT("Offset")) || Eq(TEXT("Offsets")))
		{
			return TEXT("'Position X', 'Position Y', 'Size X', 'Size Y' (Canvas slot)");
		}
		if (Eq(TEXT("Size")))
		{
			if (bBoxOrOverlay) return TEXT("'Size Rule' and 'Size Value' (Box slot)");
			return TEXT("'Size X' and 'Size Y' (Canvas slot)");
		}
		if (Eq(TEXT("Margin")))
		{
			return TEXT("'Padding' (or 'Padding Left/Top/Right/Bottom')");
		}
		if (Eq(TEXT("Color")) || Eq(TEXT("Colour")) || Eq(TEXT("Tint")))
		{
			return TEXT("text: 'ColorAndOpacity', image: 'ColorAndOpacity', button: 'Background Color' — value as UE struct text '(R=..,G=..,B=..,A=..)'");
		}
		return FString();
	}

	// Compose the "not found" warning with an optional did-you-mean hint and a generic pointer.
	FString BuildPropertyNotFoundMessage(const TCHAR* Func, UWidget* Widget, const FString& ComponentName, const FString& PropertyName)
	{
		FString Msg = FString::Printf(TEXT("UWidgetService::%s: Property '%s' not found on widget '%s'."), Func, *PropertyName, *ComponentName);
		const FString Hint = BuildSlotPropertyHint(Widget, PropertyName);
		if (!Hint.IsEmpty())
		{
			Msg += FString::Printf(TEXT(" Did you mean %s?"), *Hint);
		}
		Msg += TEXT(" Slot layout is set via scalar aliases or an explicit 'Slot.<Prop>' path (see the umg-widgets skill); use list_properties for component property names.");
		return Msg;
	}

	// Normalize a friendly slot-alias value into the form ImportText expects.
	// Alignment aliases accept "Fill"/"Left"/"Center"/"Right"/"Top"/"Bottom" (or the full enum name);
	// size-rule accepts "Fill"/"Automatic". Returns the value unchanged for non-alias properties.
	FString NormalizeSlotAliasValue(const FString& PropertyName, const FString& Value)
	{
		const bool bIsHAlign = PropertyName.Equals(TEXT("Horizontal Alignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("HorizontalAlignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("H Align"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("HAlign"), ESearchCase::IgnoreCase);
		const bool bIsVAlign = PropertyName.Equals(TEXT("Vertical Alignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("VerticalAlignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("V Align"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("VAlign"), ESearchCase::IgnoreCase);

		FString V = Value;
		V.TrimStartAndEndInline();

		// Full "Padding" with a single number -> apply to all four sides (FMargin's ImportText needs
		// the struct form, so expand "8" to "(Left=8,Top=8,Right=8,Bottom=8)").
		if (PropertyName.Equals(TEXT("Padding"), ESearchCase::IgnoreCase) &&
			!V.Contains(TEXT("=")) && !V.Contains(TEXT(",")))
		{
			float Pad = 0.0f;
			if (LexTryParseString(Pad, *V))
			{
				return FString::Printf(TEXT("(Left=%f,Top=%f,Right=%f,Bottom=%f)"), Pad, Pad, Pad, Pad);
			}
		}

		if (bIsHAlign)
		{
			if (V.StartsWith(TEXT("HAlign_"), ESearchCase::IgnoreCase)) return V;
			if (V.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Fill");
			if (V.Equals(TEXT("Left"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Left");
			if (V.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Centre"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Center");
			if (V.Equals(TEXT("Right"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Right");
		}
		else if (bIsVAlign)
		{
			if (V.StartsWith(TEXT("VAlign_"), ESearchCase::IgnoreCase)) return V;
			if (V.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Fill");
			if (V.Equals(TEXT("Top"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Top");
			if (V.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Centre"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Center");
			if (V.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Bottom");
		}

		return Value;
	}

	bool ResolveWidgetProperty(UWidget* Widget, const FString& PropertyName, FResolvedWidgetProperty& OutResolved)
	{
		OutResolved = {};
		if (!Widget)
		{
			return false;
		}
		OutResolved.AliasName = PropertyName;

		// Explicit slot targeting: "Slot.<Prop>" always addresses the widget's slot (issue #553).
		if (PropertyName.StartsWith(TEXT("Slot."), ESearchCase::IgnoreCase))
		{
			if (!Widget->Slot)
			{
				return false;
			}
			OutResolved.AliasName = PropertyName.RightChop(5);
			OutResolved.TargetObject = Widget->Slot;
			OutResolved.ResolvedPath = OutResolved.AliasName;
			// Keep the friendly aliases working behind the prefix ("Slot.Padding Left" etc.).
			UObject* AliasTarget = Widget->Slot;
			FString AliasPath = OutResolved.AliasName;
			if (ResolveSpecialWidgetProperty(Widget, OutResolved.AliasName, AliasTarget, AliasPath) && AliasTarget == Widget->Slot)
			{
				OutResolved.ResolvedPath = AliasPath;
			}
			return ResolvePropertyPath(OutResolved.TargetObject->GetClass(), OutResolved.TargetObject, OutResolved.ResolvedPath, OutResolved.Property, OutResolved.ValuePtr);
		}

		// The widget's own property wins over a same-named slot property — the old slot-first order
		// silently wrote the SLOT and left the widget's property untouched (issue #553).
		if (ResolvePropertyPath(Widget->GetClass(), Widget, PropertyName, OutResolved.Property, OutResolved.ValuePtr))
		{
			OutResolved.TargetObject = Widget;
			OutResolved.ResolvedPath = PropertyName;
			UObject* AliasTarget = Widget;
			FString AliasPath = PropertyName;
			if (ResolveSpecialWidgetProperty(Widget, PropertyName, AliasTarget, AliasPath) && AliasTarget != Widget)
			{
				OutResolved.bAmbiguous = true;
			}
			return true;
		}

		// Fall back to the slot aliases (Canvas scalars, box alignment/padding, ZOrder, ...).
		OutResolved.TargetObject = Widget;
		OutResolved.ResolvedPath = PropertyName;
		if (ResolveSpecialWidgetProperty(Widget, PropertyName, OutResolved.TargetObject, OutResolved.ResolvedPath))
		{
			if (ResolvePropertyPath(OutResolved.TargetObject->GetClass(), OutResolved.TargetObject, OutResolved.ResolvedPath, OutResolved.Property, OutResolved.ValuePtr))
			{
				return true;
			}
		}

		return false;
	}

	void MarkWidgetBlueprintModified(UWidgetBlueprint* WidgetBP, bool bStructural = false)
	{
		if (!WidgetBP)
		{
			return;
		}

		WidgetBP->Modify();
		if (bStructural)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
		}
	}

	// Shared by the Widget Blueprint path and the WI-1564 t200 class-path capture: picks the PIE world
	// (else the editor world) and creates "<NameStem>_Instance" of a UUserWidget class.
	UUserWidget* CreateWidgetInstanceForClass(UClass* WidgetClass, const FString& NameStem)
	{
		if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return nullptr;
		}

		UWorld* World = nullptr;
		if (GEditor)
		{
			if (GEditor->PlayWorld)
			{
				World = GEditor->PlayWorld.Get();
			}
			else
			{
				World = GEditor->GetEditorWorldContext().World();
			}
		}

		if (!World)
		{
			return nullptr;
		}

		const FName WidgetName = MakeUniqueObjectName(World, WidgetClass, FName(*FString::Printf(TEXT("%s_Instance"), *NameStem)));
		return CreateWidget<UUserWidget>(World, WidgetClass, WidgetName);
	}

	UUserWidget* CreateWidgetInstanceForBlueprint(UWidgetBlueprint* WidgetBP)
	{
		if (!WidgetBP)
		{
			return nullptr;
		}

		if (!WidgetBP->GeneratedClass || WidgetBP->Status == BS_Dirty)
		{
			FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		}

		if (!WidgetBP->GeneratedClass || !WidgetBP->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return nullptr;
		}

		return CreateWidgetInstanceForClass(WidgetBP->GeneratedClass, WidgetBP->GetName());
	}

	UWidget* FindRuntimeWidgetByName(UUserWidget* WidgetInstance, const FString& ComponentName)
	{
		if (!WidgetInstance || !WidgetInstance->WidgetTree)
		{
			return nullptr;
		}

		if (ComponentName.IsEmpty())
		{
			return WidgetInstance->WidgetTree->RootWidget;
		}

		TArray<UWidget*> AllWidgets;
		WidgetInstance->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return Widget;
			}
		}

		return nullptr;
	}

	template<typename StructType>
	bool ImportStructFromText(const FString& Text, StructType& OutValue)
	{
		const UScriptStruct* ScriptStruct = TBaseStructure<StructType>::Get();
		return ScriptStruct && ScriptStruct->ImportText(*Text, &OutValue, nullptr, PPF_None, GLog, ScriptStruct->GetName()) != nullptr;
	}

	bool ParseLinearColor(const FString& Text, FLinearColor& OutColor)
	{
		return ImportStructFromText(Text, OutColor);
	}

	bool ParseVector2D(const FString& Text, FVector2D& OutVector)
	{
		return ImportStructFromText(Text, OutVector);
	}

	bool ParseMargin(const FString& Text, FMargin& OutMargin)
	{
		return ImportStructFromText(Text, OutMargin);
	}

	bool ParseCornerRadius(const FString& Text, FVector4& OutCornerRadius)
	{
		OutCornerRadius = FVector4::Zero();

		FString Trimmed = Text;
		Trimmed.TrimStartAndEndInline();
		Trimmed.RemoveFromStart(TEXT("("));
		Trimmed.RemoveFromEnd(TEXT(")"));

		TArray<FString> Parts;
		Trimmed.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}

			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			float ParsedValue = 0.0f;
			if (!LexTryParseString(ParsedValue, *Value))
			{
				continue;
			}

			if (Key.Equals(TEXT("TopLeft"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.X = ParsedValue;
			}
			else if (Key.Equals(TEXT("TopRight"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.Y = ParsedValue;
			}
			else if (Key.Equals(TEXT("BottomRight"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.Z = ParsedValue;
			}
			else if (Key.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.W = ParsedValue;
			}
		}

		return true;
	}

	ESlateBrushDrawType::Type StringToBrushDrawType(const FString& DrawAs)
	{
		if (DrawAs.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::Box;
		}
		if (DrawAs.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::Border;
		}
		if (DrawAs.Equals(TEXT("RoundedBox"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::RoundedBox;
		}
		if (DrawAs.Equals(TEXT("NoDrawType"), ESearchCase::IgnoreCase) || DrawAs.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::NoDrawType;
		}
		return ESlateBrushDrawType::Image;
	}

	FString BrushDrawTypeToString(ESlateBrushDrawType::Type DrawType)
	{
		switch (DrawType)
		{
		case ESlateBrushDrawType::Box:
			return TEXT("Box");
		case ESlateBrushDrawType::Border:
			return TEXT("Border");
		case ESlateBrushDrawType::RoundedBox:
			return TEXT("RoundedBox");
		case ESlateBrushDrawType::NoDrawType:
			return TEXT("NoDrawType");
		default:
			return TEXT("Image");
		}
	}

	EMovieSceneKeyInterpolation StringToKeyInterpolation(const FString& Interpolation)
	{
		if (Interpolation.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
		{
			return EMovieSceneKeyInterpolation::Constant;
		}
		if (Interpolation.Equals(TEXT("Cubic"), ESearchCase::IgnoreCase))
		{
			return EMovieSceneKeyInterpolation::Auto;
		}
		return EMovieSceneKeyInterpolation::Linear;
	}

	bool TryGetPropertyText(UObject* Object, const FString& PropertyPath, FString& OutText)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		if (!ResolvePropertyPath(Object->GetClass(), Object, PropertyPath, Property, ValuePtr))
		{
			return false;
		}

		// ExportTextItem_Direct APPENDS to OutText; reset first so callers passing a
		// struct field that has a default-value initializer (e.g. FWidgetFontInfo.ShadowOffset)
		// get the value alone, not "<default><exported>" concatenated. (issue #470)
		OutText.Reset();
		Property->ExportTextItem_Direct(OutText, ValuePtr, nullptr, Object, PPF_None);
		return true;
	}

	bool TrySetPropertyText(UObject* Object, const FString& PropertyPath, const FString& PropertyValue)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		if (!ResolvePropertyPath(Object->GetClass(), Object, PropertyPath, Property, ValuePtr))
		{
			return false;
		}

		return Property->ImportText_Direct(*PropertyValue, ValuePtr, Object, PPF_None) != nullptr;
	}

	// A text widget's ColorAndOpacity is an FSlateColor: a bare "(R=..,G=..,B=..,A=..)" fed to
	// ImportText_Direct matches no member and is a lenient no-op that still reports success — so the
	// colour silently never lands. Parse the linear tuple and assign FSlateColor(Linear) reflectively
	// (verified by readback); if the text is instead a fully-qualified FSlateColor tuple
	// ("(SpecifiedColor=(...),ColorUseRule=UseColor_Specified)") fall back to a strict text import.
	// Returns false when a non-empty colour did not land. (VibeUE A4)
	bool TrySetSlateColorProperty(UObject* Object, const FString& PropertyName, const FString& ColorText)
	{
		if (!Object || ColorText.IsEmpty())
		{
			return false;
		}

		FStructProperty* StructProp = FindFProperty<FStructProperty>(Object->GetClass(), *PropertyName);
		if (!StructProp || StructProp->Struct != FSlateColor::StaticStruct())
		{
			return false;
		}

		FSlateColor* ColorPtr = StructProp->ContainerPtrToValuePtr<FSlateColor>(Object);
		if (!ColorPtr)
		{
			return false;
		}

		FLinearColor Linear;
		if (ParseLinearColor(ColorText, Linear))
		{
			*ColorPtr = FSlateColor(Linear);
			// Verify by readback rather than by return value.
			return ColorPtr->GetSpecifiedColor().Equals(Linear);
		}

		// Fully-qualified FSlateColor tuple: let the struct's own importer handle it.
		return StructProp->ImportText_Direct(*ColorText, ColorPtr, Object, PPF_None) != nullptr;
	}

	// UMG "optional override" values (USizeBox::WidthOverride/HeightOverride/MinDesiredWidth/…,
	// UImage aspect-ratio overrides, etc.) are inert unless their companion bOverride_<Name> bool
	// is also set — that bool is the "checkbox" the UMG designer ticks for you. Writing only the
	// value field silently no-ops: the layout engine keeps ignoring it (e.g. a SizeBox width that
	// never clamps, so the box fills its parent). Whenever we set such a value directly on an
	// object, raise the matching bOverride_ companion so the intent actually applies. (issue #508)
	void RaiseOverrideCompanionIfPresent(UObject* Object, const FProperty* ValueProperty)
	{
		if (!Object || !ValueProperty)
		{
			return;
		}

		// Only direct object members carry a bOverride_ companion; struct-nested fields
		// (e.g. WidgetStyle.Normal.TintColor) don't, and GetOwnerClass() is null for those.
		UClass* OwnerClass = ValueProperty->GetOwnerClass();
		if (!OwnerClass || !Object->GetClass()->IsChildOf(OwnerClass))
		{
			return;
		}

		const FString CompanionName = FString(TEXT("bOverride_")) + ValueProperty->GetName();
		FBoolProperty* OverrideFlag = FindFProperty<FBoolProperty>(Object->GetClass(), *CompanionName);
		if (!OverrideFlag)
		{
			return;
		}

		void* FlagPtr = OverrideFlag->ContainerPtrToValuePtr<void>(Object);
		if (!OverrideFlag->GetPropertyValue(FlagPtr))
		{
			OverrideFlag->SetPropertyValue(FlagPtr, true);
		}
	}

	FString GetLastPathSegment(const FString& PropertyPath)
	{
		FString Left;
		FString Right;
		if (PropertyPath.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			return Right;
		}
		return PropertyPath;
	}

	bool IsSlateFontProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == TBaseStructure<FSlateFontInfo>::Get();
	}

	bool IsSlateBrushProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == TBaseStructure<FSlateBrush>::Get();
	}

	bool ResolveFontProperty(UWidget* Widget, const FString& RequestedPropertyName, FResolvedWidgetProperty& OutResolved)
	{
		TArray<FString> CandidatePaths;
		if (!RequestedPropertyName.IsEmpty() && !RequestedPropertyName.Equals(TEXT("Font"), ESearchCase::IgnoreCase))
		{
			CandidatePaths.Add(RequestedPropertyName);
		}
		else
		{
			CandidatePaths = {
				TEXT("Font"),
				TEXT("WidgetStyle.Font"),
				TEXT("WidgetStyle.TextStyle.Font"),
				TEXT("TextStyle.Font")
			};
		}

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (ResolveWidgetProperty(Widget, CandidatePath, OutResolved) && IsSlateFontProperty(OutResolved.Property))
			{
				return true;
			}
		}

		return false;
	}

	bool ResolveBrushProperty(UWidget* Widget, const FString& SlotName, FResolvedWidgetProperty& OutResolved)
	{
		TArray<FString> CandidatePaths;

		if (Cast<UImage>(Widget))
		{
			CandidatePaths = { TEXT("Brush") };
		}
		else if (Cast<UButton>(Widget))
		{
			if (SlotName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Normal") };
			else if (SlotName.Equals(TEXT("Hovered"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Hovered") };
			else if (SlotName.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Pressed") };
			else if (SlotName.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Disabled") };
		}
		else if (Cast<UBorder>(Widget))
		{
			CandidatePaths = { TEXT("Background") };
		}
		else if (Cast<UProgressBar>(Widget))
		{
			CandidatePaths = { SlotName };
		}
		else if (Cast<UCheckBox>(Widget))
		{
			CandidatePaths = { FString::Printf(TEXT("WidgetStyle.%s"), *SlotName) };
		}
		else if (Cast<USlider>(Widget))
		{
			if (SlotName.Equals(TEXT("BarImage"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.NormalBarImage") };
			else if (SlotName.Equals(TEXT("ThumbImage"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.NormalThumbImage") };
			else CandidatePaths = { FString::Printf(TEXT("WidgetStyle.%s"), *SlotName) };
		}

		CandidatePaths.AddUnique(SlotName);
		CandidatePaths.AddUnique(FString::Printf(TEXT("WidgetStyle.%s"), *SlotName));

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (ResolveWidgetProperty(Widget, CandidatePath, OutResolved) && IsSlateBrushProperty(OutResolved.Property))
			{
				return true;
			}
		}

		return false;
	}

	UWidgetAnimation* FindAnimationByName(UWidgetBlueprint* WidgetBP, const FString& AnimationName)
	{
		if (!WidgetBP)
		{
			return nullptr;
		}

		for (UWidgetAnimation* Animation : WidgetBP->Animations)
		{
			if (!Animation)
			{
				continue;
			}

			if (Animation->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
			{
				return Animation;
			}

#if WITH_EDITOR
			if (Animation->GetDisplayLabel().Equals(AnimationName, ESearchCase::IgnoreCase))
			{
				return Animation;
			}
#endif
		}

		return nullptr;
	}

	int32 GetAnimationTrackCount(UWidgetAnimation* Animation)
	{
		if (!Animation || !Animation->GetMovieScene())
		{
			return 0;
		}

		int32 TrackCount = 0;
		const UMovieScene* MovieScene = Animation->GetMovieScene();
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			TrackCount += Binding.GetTracks().Num();
		}

		return TrackCount;
	}

	bool ResolveAnimationPropertyTarget(UWidget* DesignWidget, UUserWidget* PreviewWidget, const FString& PropertyName, FWidgetAnimationPropertyTarget& OutTarget)
	{
		OutTarget = {};
		if (!DesignWidget || !PreviewWidget)
		{
			return false;
		}

		UWidget* RuntimeWidget = FindRuntimeWidgetByName(PreviewWidget, DesignWidget->GetName());
		if (!RuntimeWidget)
		{
			return false;
		}

		if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase))
		{
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RuntimeWidget->Slot);
			if (!CanvasSlot)
			{
				return false;
			}

			OutTarget.AnimatedObject = CanvasSlot;
			OutTarget.TrackType = EWidgetAnimationTrackType::Margin;
			OutTarget.PropertyName = TEXT("Offsets");
			OutTarget.PropertyPath = TEXT("LayoutData.Offsets");

			if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 0;
			else if (PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 1;
			else if (PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 2;
			else if (PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 3;

			return true;
		}

		FResolvedWidgetProperty Resolved;
		if (!ResolveWidgetProperty(RuntimeWidget, PropertyName, Resolved))
		{
			return false;
		}

		OutTarget.AnimatedObject = Resolved.TargetObject;
		OutTarget.PropertyName = Resolved.Property->GetFName();
		OutTarget.PropertyPath = Resolved.ResolvedPath;

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Resolved.Property))
		{
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				OutTarget.TrackType = EWidgetAnimationTrackType::Color;
				return true;
			}

			if (StructProperty->Struct && StructProperty->Struct->GetFName() == TEXT("SlateColor"))
			{
				OutTarget.TrackType = EWidgetAnimationTrackType::Color;
				OutTarget.PropertyName = TEXT("SpecifiedColor");
				OutTarget.PropertyPath = Resolved.ResolvedPath + TEXT(".SpecifiedColor");
				return true;
			}
		}

		if (CastField<FFloatProperty>(Resolved.Property) || CastField<FDoubleProperty>(Resolved.Property))
		{
			OutTarget.TrackType = EWidgetAnimationTrackType::Float;
			return true;
		}

		return false;
	}

	FGuid EnsureAnimationBinding(UWidgetAnimation* Animation, UUserWidget* PreviewWidget, UObject* AnimatedObject)
	{
		if (!Animation || !Animation->GetMovieScene() || !PreviewWidget || !AnimatedObject)
		{
			return FGuid();
		}

		const UPanelSlot* SlotObject = Cast<UPanelSlot>(AnimatedObject);
		for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
		{
			if (SlotObject)
			{
				if (Binding.WidgetName == SlotObject->Content->GetFName() && Binding.SlotWidgetName == SlotObject->GetFName())
				{
					return Binding.AnimationGuid;
				}
			}
			else if (Binding.WidgetName == AnimatedObject->GetFName() && Binding.SlotWidgetName.IsNone())
			{
				return Binding.AnimationGuid;
			}
		}

		if (!Animation->CanPossessObject(*AnimatedObject, PreviewWidget))
		{
			return FGuid();
		}

		const FGuid BindingGuid = Animation->GetMovieScene()->AddPossessable(AnimatedObject->GetName(), AnimatedObject->GetClass());
		Animation->BindPossessableObject(BindingGuid, *AnimatedObject, PreviewWidget);
		return BindingGuid;
	}

	template<typename TrackType>
	TrackType* FindOrAddPropertyTrack(UMovieScene* MovieScene, const FGuid& BindingGuid, const FName& PropertyName, const FString& PropertyPath)
	{
		if (!MovieScene)
		{
			return nullptr;
		}

		for (UMovieSceneTrack* ExistingTrack : MovieScene->FindTracks(TrackType::StaticClass(), BindingGuid))
		{
			UMovieScenePropertyTrack* ExistingPropertyTrack = Cast<UMovieScenePropertyTrack>(ExistingTrack);
			if (ExistingPropertyTrack && ExistingPropertyTrack->GetPropertyName() == PropertyName && ExistingPropertyTrack->GetPropertyPath().ToString() == PropertyPath)
			{
				return Cast<TrackType>(ExistingTrack);
			}
		}

		TrackType* NewTrack = MovieScene->AddTrack<TrackType>(BindingGuid);
		if (UMovieScenePropertyTrack* NewPropertyTrack = Cast<UMovieScenePropertyTrack>(NewTrack))
		{
			NewPropertyTrack->SetPropertyNameAndPath(PropertyName, PropertyPath);
		}
		return NewTrack;
	}

	template<typename SectionType>
	SectionType* FindOrAddSection(UMovieScenePropertyTrack* Track, const TRange<FFrameNumber>& DefaultRange)
	{
		if (!Track)
		{
			return nullptr;
		}

		for (UMovieSceneSection* ExistingSection : Track->GetAllSections())
		{
			if (SectionType* TypedSection = Cast<SectionType>(ExistingSection))
			{
				return TypedSection;
			}
		}

		SectionType* NewSection = Cast<SectionType>(Track->CreateNewSection());
		if (!NewSection)
		{
			return nullptr;
		}

		NewSection->SetRange(DefaultRange);
		Track->AddSection(*NewSection);
		return NewSection;
	}

	void EnsurePlaybackRangeIncludesFrame(UMovieScene* MovieScene, FFrameNumber Frame)
	{
		if (!MovieScene)
		{
			return;
		}

		TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
		FFrameNumber Lower = PlaybackRange.HasLowerBound() ? PlaybackRange.GetLowerBoundValue() : FFrameNumber(0);
		FFrameNumber Upper = PlaybackRange.HasUpperBound() ? PlaybackRange.GetUpperBoundValue() : FFrameNumber(1);

		if (Frame < Lower)
		{
			Lower = Frame;
		}
		if (Frame >= Upper)
		{
			Upper = Frame + 1;
		}

		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(Lower, Upper));
	}

	bool ExportFloatValue(void* ValuePtr, FProperty* Property, UObject* OwnerObject, float& OutValue)
	{
		FString ValueText;
		Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, OwnerObject, PPF_None);
		return LexTryParseString(OutValue, *ValueText);
	}

	bool ExportLinearColorValue(void* ValuePtr, FProperty* Property, UObject* OwnerObject, FLinearColor& OutValue)
	{
		FString ValueText;
		Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, OwnerObject, PPF_None);
		return ParseLinearColor(ValueText, OutValue);
	}
}

// =================================================================
// Helper Methods
// =================================================================

UWidgetBlueprint* UWidgetService::LoadWidgetBlueprint(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
	if (!WidgetBP)
	{
		// UEditorAssetLibrary::LoadAsset returns null during PIE (CheckIfInEditorAndPIE guard), so
		// every widget entry point silently reported "not found" mid-session (issue #557). Accept
		// either a package path (/Game/UI/WBP_X) or an object path (/Game/UI/WBP_X.WBP_X) and
		// resolve it directly, the same way SpawnWidgetInPIE always had to.
		FString ObjectPath = WidgetPath;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(WidgetPath);
			ObjectPath = FString::Printf(TEXT("%s.%s"), *WidgetPath, *AssetName);
		}
		WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	}
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService: Failed to load Widget Blueprint: %s"), *WidgetPath);
	}
	return WidgetBP;
}

UWidget* UWidgetService::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& ComponentName)
{
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return nullptr;
	}

	// Search all widgets in the tree
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Widget;
		}
	}
	
	return nullptr;
}

TSubclassOf<UWidget> UWidgetService::FindWidgetClass(const FString& TypeName)
{
	// Map type names to widget classes
	static TMap<FString, TSubclassOf<UWidget>> WidgetClassMap;
	
	if (WidgetClassMap.Num() == 0)
	{
		WidgetClassMap.Add(TEXT("TextBlock"), UTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("Button"), UButton::StaticClass());
		WidgetClassMap.Add(TEXT("Image"), UImage::StaticClass());
		WidgetClassMap.Add(TEXT("EditableText"), UEditableText::StaticClass());
		WidgetClassMap.Add(TEXT("EditableTextBox"), UEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("CheckBox"), UCheckBox::StaticClass());
		WidgetClassMap.Add(TEXT("Slider"), USlider::StaticClass());
		WidgetClassMap.Add(TEXT("ProgressBar"), UProgressBar::StaticClass());
		WidgetClassMap.Add(TEXT("Spacer"), USpacer::StaticClass());
		WidgetClassMap.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
		WidgetClassMap.Add(TEXT("Overlay"), UOverlay::StaticClass());
		WidgetClassMap.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
		WidgetClassMap.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
		WidgetClassMap.Add(TEXT("ScrollBox"), UScrollBox::StaticClass());
		WidgetClassMap.Add(TEXT("GridPanel"), UGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("WidgetSwitcher"), UWidgetSwitcher::StaticClass());
		WidgetClassMap.Add(TEXT("RichTextBlock"), URichTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("ComboBoxString"), UComboBoxString::StaticClass());
		WidgetClassMap.Add(TEXT("SpinBox"), USpinBox::StaticClass());
		WidgetClassMap.Add(TEXT("MultiLineEditableText"), UMultiLineEditableText::StaticClass());
		WidgetClassMap.Add(TEXT("MultiLineEditableTextBox"), UMultiLineEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("InputKeySelector"), UInputKeySelector::StaticClass());
		WidgetClassMap.Add(TEXT("Border"), UBorder::StaticClass());
		WidgetClassMap.Add(TEXT("SizeBox"), USizeBox::StaticClass());
		WidgetClassMap.Add(TEXT("ScaleBox"), UScaleBox::StaticClass());
		WidgetClassMap.Add(TEXT("BackgroundBlur"), UBackgroundBlur::StaticClass());
		WidgetClassMap.Add(TEXT("SafeZone"), USafeZone::StaticClass());
		WidgetClassMap.Add(TEXT("UniformGridPanel"), UUniformGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("WrapBox"), UWrapBox::StaticClass());
		WidgetClassMap.Add(TEXT("InvalidationBox"), UInvalidationBox::StaticClass());
		WidgetClassMap.Add(TEXT("RetainerBox"), URetainerBox::StaticClass());
		WidgetClassMap.Add(TEXT("Throbber"), UThrobber::StaticClass());
		WidgetClassMap.Add(TEXT("CircularThrobber"), UCircularThrobber::StaticClass());
		WidgetClassMap.Add(TEXT("ListView"), UListView::StaticClass());
		WidgetClassMap.Add(TEXT("TreeView"), UTreeView::StaticClass());
		WidgetClassMap.Add(TEXT("TileView"), UTileView::StaticClass());
		WidgetClassMap.Add(TEXT("ExpandableArea"), UExpandableArea::StaticClass());
		WidgetClassMap.Add(TEXT("MenuAnchor"), UMenuAnchor::StaticClass());
		WidgetClassMap.Add(TEXT("NativeWidgetHost"), UNativeWidgetHost::StaticClass());
	}
	
	if (TSubclassOf<UWidget>* Found = WidgetClassMap.Find(TypeName))
	{
		return *Found;
	}
	
	// Try case-insensitive search
	for (const auto& Pair : WidgetClassMap)
	{
		if (Pair.Key.Equals(TypeName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}

	// Fallback 1: dynamic UClass lookup — finds any native UWidget subclass by name
	// without requiring it to be in the hardcoded map (e.g. WindowTitleBarArea, etc.)
	{
		// Try the bare name first, then with the standard 'U' prefix
		const FString WithPrefix = FString(TEXT("U")) + TypeName;
		for (const FString& Candidate : { TypeName, WithPrefix })
		{
			// Search all loaded UMG-family packages for the class
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;
				if (Class->IsChildOf(UWidget::StaticClass()) &&
					!Class->HasAnyClassFlags(CLASS_Abstract) &&
					(Class->GetName().Equals(Candidate, ESearchCase::IgnoreCase) ||
					 Class->GetName().Equals(TypeName, ESearchCase::IgnoreCase)))
				{
					TSubclassOf<UWidget> Found(Class);
					WidgetClassMap.Add(TypeName, Found);
					return Found;
				}
			}
		}
	}

	// Fallback 2: search Asset Registry for a custom Widget Blueprint by name
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString().Equals(TypeName, ESearchCase::IgnoreCase))
		{
			if (UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
			{
				// Ensure the WBP is compiled before using its GeneratedClass
				if (!WBP->GeneratedClass || WBP->Status == BS_Dirty)
				{
					UE_LOG(LogTemp, Log, TEXT("UWidgetService: Compiling WBP '%s' before resolving class"), *TypeName);
					FKismetEditorUtilities::CompileBlueprint(WBP);
				}

				if (WBP->GeneratedClass && WBP->GeneratedClass->IsChildOf(UWidget::StaticClass()))
				{
					TSubclassOf<UWidget> WidgetSubclass(*WBP->GeneratedClass);
					WidgetClassMap.Add(TypeName, WidgetSubclass);
					return WidgetSubclass;
				}
			}
		}
	}

	return nullptr;
}

// =================================================================
// Discovery Methods
// =================================================================

TArray<FString> UWidgetService::ListWidgetBlueprints(const FString& PathFilter)
{
	TArray<FString> WidgetPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));

	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		WidgetPaths.Add(AssetData.GetObjectPathString());
	}

	return WidgetPaths;
}

TArray<FWidgetInfo> UWidgetService::GetHierarchy(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Hierarchy;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Hierarchy;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	if (!RootWidget)
	{
		return Hierarchy;
	}

	// GetAllWidgets uses GetObjectsWithOuter and finds every widget regardless of
	// slot initialisation state — this correctly handles UContentWidget subclasses
	// (e.g. WindowWrapper / UWindowTitleBarArea) that return GetChildrenCount()==0
	// at editor load time even though they have children in the designer.
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	AllWidgets.AddUnique(RootWidget);

	// Build a name-keyed info map from the flat list
	TMap<FString, FWidgetInfo> InfoMap;
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.WidgetClassPath = Widget->GetClass()->GetPathName();
		Info.bIsRootWidget = (Widget == RootWidget);
		Info.bIsVariable = Widget->bIsVariable;
		InfoMap.Add(Info.WidgetName, Info);
	}

	// Wire up parent/children using GetParent() — same approach as ListComponents
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		if (UPanelWidget* Parent = Widget->GetParent())
		{
			const FString ParentName = Parent->GetName();
			const FString ChildName = Widget->GetName();

			if (FWidgetInfo* ChildInfo = InfoMap.Find(ChildName))
			{
				ChildInfo->ParentWidget = ParentName;
			}
			if (FWidgetInfo* ParentInfo = InfoMap.Find(ParentName))
			{
				ParentInfo->Children.AddUnique(ChildName);
			}
		}
	}

	// Emit in depth-first order starting from root so callers get a sensible hierarchy
	TFunction<void(const FString&)> EmitDepthFirst;
	EmitDepthFirst = [&](const FString& Name)
	{
		FWidgetInfo* Info = InfoMap.Find(Name);
		if (!Info)
		{
			return;
		}
		Hierarchy.Add(*Info);

		// Copy the child names BEFORE removing the entry. InfoMap.Remove(Name) destructs
		// *Info (freeing its Children array), so iterating Info->Children afterward is a
		// use-after-free — an access violation that crashes the Python interpreter and the
		// editor along with it. Recursive calls also mutate InfoMap, which can reallocate
		// its storage and invalidate any outstanding element pointers.
		TArray<FString> ChildNames = Info->Children;
		InfoMap.Remove(Name); // guard against cycles
		for (const FString& ChildName : ChildNames)
		{
			EmitDepthFirst(ChildName);
		}
	};

	EmitDepthFirst(RootWidget->GetName());

	// Append any widgets not reachable from root (orphans should not exist, but be safe)
	for (auto& Pair : InfoMap)
	{
		Hierarchy.Add(Pair.Value);
	}

	return Hierarchy;
}

FString UWidgetService::GetRootWidget(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree || !WidgetBP->WidgetTree->RootWidget)
	{
		return FString();
	}

	return WidgetBP->WidgetTree->RootWidget->GetName();
}

TArray<FWidgetInfo> UWidgetService::ListComponents(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Components;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Components;
	}

	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);

	UWidget* RootWidgetLC = WidgetBP->WidgetTree->RootWidget;

	// First pass: build info map keyed by name
	TMap<FString, FWidgetInfo> InfoMapLC;
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.WidgetClassPath = Widget->GetClass()->GetPathName();
		Info.bIsVariable = Widget->bIsVariable;
		Info.bIsRootWidget = (Widget == RootWidgetLC);
		InfoMapLC.Add(Info.WidgetName, Info);
	}

	// Second pass: wire parent/children via GetParent() so UContentWidget subclasses
	// (e.g. WindowWrapper) have correct Children arrays, not just ParentWidget strings
	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		if (UPanelWidget* Parent = Widget->GetParent())
		{
			const FString ParentName = Parent->GetName();
			const FString ChildName = Widget->GetName();

			if (FWidgetInfo* ChildInfo = InfoMapLC.Find(ChildName))
			{
				ChildInfo->ParentWidget = ParentName;
			}
			if (FWidgetInfo* ParentInfo = InfoMapLC.Find(ParentName))
			{
				ParentInfo->Children.AddUnique(ChildName);
			}
		}
	}

	for (auto& Pair : InfoMapLC)
	{
		Components.Add(Pair.Value);
	}

	return Components;
}

TArray<FString> UWidgetService::SearchTypes(const FString& FilterText)
{
	TArray<FString> Results;
	
	// Built-in native widget types
	for (const FString& Type : GAvailableWidgetTypes)
	{
		if (FilterText.IsEmpty() || Type.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			Results.Add(Type);
		}
	}

	// Also list custom Widget Blueprints from the Asset Registry
	// (Note: these CANNOT be added via add_component - they are listed for reference only)
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();
		if (FilterText.IsEmpty() || AssetName.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			// Prefix with [WBP] to distinguish from native types
			Results.Add(FString::Printf(TEXT("[WBP] %s (%s)"), *AssetName, *AssetData.GetObjectPathString()));
		}
	}
	
	return Results;
}

TArray<FWidgetPropertyInfo> UWidgetService::GetComponentProperties(const FString& WidgetPath, const FString& ComponentName)
{
	return ListProperties(WidgetPath, ComponentName, false);
}

// =================================================================
// Component Management
// =================================================================

FWidgetAddComponentResult UWidgetService::AddComponent(
	const FString& WidgetPath,
	const FString& ComponentType,
	const FString& ComponentName,
	const FString& ParentName,
	bool bIsVariable,
	int32 ChildIndex)
{
	FWidgetAddComponentResult Result;
	
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	// Find the widget class
	TSubclassOf<UWidget> WidgetClass = FindWidgetClass(ComponentType);
	if (!WidgetClass)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown widget type '%s'. Use search_types() to get available types, or list_widget_blueprints() for custom WBPs."), *ComponentType);
		return Result;
	}

	// For UserWidget subclasses (custom WBPs), do additional safety checks
	const bool bIsUserWidget = WidgetClass->IsChildOf(UUserWidget::StaticClass());
	if (bIsUserWidget)
	{
		// Prevent circular references: a WBP cannot contain itself as a child
		if (WidgetBP->GeneratedClass && (WidgetClass == WidgetBP->GeneratedClass ||
			WidgetClass->IsChildOf(WidgetBP->GeneratedClass)))
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Cannot add '%s': circular reference detected. A Widget Blueprint cannot contain itself as a child component."),
				*ComponentType);
			return Result;
		}

		UE_LOG(LogTemp, Log, TEXT("UWidgetService: Adding custom WBP '%s' as component '%s'"), *ComponentType, *ComponentName);
	}

	// Find parent panel (or use root)
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = FindWidgetByName(WidgetBP, ParentName);
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Parent '%s' not found or is not a panel widget"), *ParentName);
			return Result;
		}
	}
	else
	{
		// Use root as parent if it's a panel
		ParentPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
	}

	// Validate ChildIndex up front rather than silently falling back to append or to setting the root: -1 means
	// append, [0, GetChildrenCount()] means insert at that position, anything else is an explicit error - this
	// must fire even when ParentPanel is null (e.g. no root widget exists yet), since a non-default ChildIndex
	// has nothing to insert into there and would otherwise silently be ignored by the set-as-root fallback below
	if (ChildIndex != -1)
	{
		if (!ParentPanel)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("ChildIndex %d was specified, but there is no panel parent to insert into (ChildIndex requires an existing panel parent)"),
				ChildIndex);
			return Result;
		}

		if (ChildIndex < 0 || ChildIndex > ParentPanel->GetChildrenCount())
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("ChildIndex %d is out of range for parent '%s' (valid range: -1 to append, or 0-%d to insert)"),
				ChildIndex, *ParentPanel->GetName(), ParentPanel->GetChildrenCount());
			return Result;
		}
	}

	// Create the new widget
	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*ComponentName));
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create widget of type '%s'"), *ComponentType);
		return Result;
	}

	NewWidget->bIsVariable = bIsVariable;

	// Register widget with GUID map (required for UMG compilation)
	const FName WidgetFName = NewWidget->GetFName();
	if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(WidgetFName))
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetFName, FGuid::NewGuid());
	}

	// Add to parent or set as root - ChildIndex was already validated above, so it's either -1 (append) or a valid index
	if (ParentPanel)
	{
		UPanelSlot* Slot = (ChildIndex >= 0) ? ParentPanel->InsertChildAt(ChildIndex, NewWidget) : ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			Result.ErrorMessage = TEXT("Failed to add widget to parent panel");
			return Result;
		}
		Result.ParentName = ParentPanel->GetName();
	}
	else if (!WidgetBP->WidgetTree->RootWidget)
	{
		// Set as root if no root exists
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		Result.ParentName = TEXT("(root)");
	}
	else
	{
		Result.ErrorMessage = TEXT("Cannot add widget: no parent specified and root already exists");
		return Result;
	}

	// Mark blueprint as modified and compile
	WidgetBP->Modify();

	if (bIsUserWidget)
	{
		// For UserWidget subclasses, do a synchronous compile immediately.
		// This ensures the parent WBP is in a valid compiled state before
		// any deferred operations (like auto-save) try to process it,
		// which would otherwise crash in Slate prepass with stack overflow.
		UE_LOG(LogTemp, Log, TEXT("UWidgetService: Compiling parent WBP after adding UserWidget child '%s'"), *ComponentName);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	}

	Result.bSuccess = true;
	Result.ComponentName = NewWidget->GetName();
	Result.ComponentType = ComponentType;
	Result.bIsVariable = bIsVariable;

	return Result;
}

bool UWidgetService::ReorderComponent(
	const FString& WidgetPath,
	const FString& ComponentName,
	int32 NewIndex)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReorderComponent: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReorderComponent: Widget '%s' not found"), *ComponentName);
		return false;
	}

	UPanelWidget* ParentPanel = Widget->GetParent();
	if (!ParentPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReorderComponent: '%s' has no panel parent (the root widget cannot be reordered)"), *ComponentName);
		return false;
	}

	if (NewIndex < 0 || NewIndex >= ParentPanel->GetChildrenCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReorderComponent: NewIndex %d is out of range for parent '%s' (valid range: 0-%d)"),
			NewIndex, *ParentPanel->GetName(), ParentPanel->GetChildrenCount() - 1);
		return false;
	}

	WidgetBP->Modify();
	// ShiftChild rewrites ParentPanel->Slots directly, so the panel is the object whose state changes.
	ParentPanel->Modify();
	ParentPanel->ShiftChild(NewIndex, Widget);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::ReorderComponent: Moved '%s' to index %d under '%s'"),
		*ComponentName, NewIndex, *ParentPanel->GetName());
	return true;
}

FWidgetRemoveComponentResult UWidgetService::RemoveComponent(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bRemoveChildren)
{
	FWidgetRemoveComponentResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	UWidget* WidgetToRemove = FindWidgetByName(WidgetBP, ComponentName);
	if (!WidgetToRemove)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName);
		return Result;
	}

	// Can't remove root
	if (WidgetToRemove == WidgetBP->WidgetTree->RootWidget)
	{
		Result.ErrorMessage = TEXT("Cannot remove root widget");
		return Result;
	}

	// Collect children if needed
	if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetToRemove))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				if (bRemoveChildren)
				{
					Result.RemovedComponents.Add(Child->GetName());
				}
				else
				{
					Result.OrphanedChildren.Add(Child->GetName());
				}
			}
		}
	}

	// Remove from parent
	if (UPanelWidget* Parent = WidgetToRemove->GetParent())
	{
		Parent->RemoveChild(WidgetToRemove);
	}

	// Remove from widget tree
	WidgetBP->WidgetTree->RemoveWidget(WidgetToRemove);
	Result.RemovedComponents.Add(ComponentName);

	// Mark blueprint as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	Result.bSuccess = true;
	return Result;
}

// =================================================================
// Validation
// =================================================================

FWidgetValidationResult UWidgetService::Validate(const FString& WidgetPath)
{
	FWidgetValidationResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.bIsValid = false;
		Result.Errors.Add(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree"));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	// Check for root widget
	if (!WidgetBP->WidgetTree->RootWidget)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no root widget"));
	}

	// Check for duplicate names
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	TSet<FString> WidgetNames;
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget)
		{
			FString Name = Widget->GetName();
			if (WidgetNames.Contains(Name))
			{
				Result.Errors.Add(FString::Printf(TEXT("Duplicate widget name: %s"), *Name));
			}
			WidgetNames.Add(Name);
		}
	}

	// Check for orphaned widgets (not attached to hierarchy)
	if (UWidget* Root = WidgetBP->WidgetTree->RootWidget)
	{
		TSet<UWidget*> ReachableWidgets;
		TFunction<void(UWidget*)> CollectReachable = [&](UWidget* Widget)
		{
			if (!Widget || ReachableWidgets.Contains(Widget))
			{
				return;
			}
			ReachableWidgets.Add(Widget);
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
				{
					CollectReachable(Panel->GetChildAt(i));
				}
			}
		};
		CollectReachable(Root);

		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && !ReachableWidgets.Contains(Widget))
			{
				Result.Errors.Add(FString::Printf(TEXT("Orphaned widget not in hierarchy: %s"), *Widget->GetName()));
			}
		}
	}

	Result.bIsValid = (Result.Errors.Num() == 0);
	Result.ValidationMessage = Result.bIsValid 
		? TEXT("Widget hierarchy is valid") 
		: Result.Errors[0];

	return Result;
}

// =================================================================
// Property Access
// =================================================================

FString UWidgetService::GetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return FString();
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return FString();
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *BuildPropertyNotFoundMessage(TEXT("GetProperty"), Widget, ComponentName, PropertyName));
		return FString();
	}

	FString ValueStr;
	Resolved.Property->ExportTextItem_Direct(ValueStr, Resolved.ValuePtr, nullptr, Resolved.TargetObject, PPF_None);

	return ValueStr;
}

bool UWidgetService::SetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *BuildPropertyNotFoundMessage(TEXT("SetProperty"), Widget, ComponentName, PropertyName));
		return false;
	}

	if (Resolved.bAmbiguous)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: '%s' exists on both widget '%s' and its slot — writing the WIDGET's property. Use 'Slot.%s' to target the slot."),
			*PropertyName, *ComponentName, *PropertyName);
	}

	// Translate friendly slot-alias values (e.g. alignment "Fill" -> "HAlign_Fill") so box-slot
	// alignment is settable without knowing the raw enum names. Key off the name with any "Slot."
	// prefix stripped, or the alias never fires for explicit slot writes.
	FString NormalizedValue = NormalizeSlotAliasValue(Resolved.AliasName, PropertyValue);

	// FSlateColor expects (SpecifiedColor=(R=..,G=..,B=..,A=..)) — callers routinely pass the
	// bare LinearColor form, which struct import treats as a lenient no-op: no member name
	// matches, nothing is written, and the call still reports success. Wrap it so the intent
	// actually applies (e.g. ColorAndOpacity on text widgets).
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Resolved.Property))
	{
		if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("SlateColor")) &&
			!NormalizedValue.Contains(TEXT("SpecifiedColor")))
		{
			const FString Trimmed = NormalizedValue.TrimStartAndEnd();
			// Reject non-tuple values (named colors, hex) — wrapping them would survive the
			// lenient struct import as another silent no-op "success".
			if (!Trimmed.StartsWith(TEXT("(")))
			{
				UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: FSlateColor value '%s' for '%s' is not a color tuple — pass \"(R=..,G=..,B=..,A=..)\" or \"(SpecifiedColor=(R=..,G=..,B=..,A=..))\"; named colors and hex are not supported"), *PropertyValue, *PropertyName);
				return false;
			}
			NormalizedValue = FString::Printf(TEXT("(SpecifiedColor=%s,ColorUseRule=UseColor_Specified)"), *Trimmed);
		}
	}

	if (Resolved.Property->ImportText_Direct(*NormalizedValue, Resolved.ValuePtr, Resolved.TargetObject, PPF_None) == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Failed to parse value '%s' for property '%s'"), *PropertyValue, *PropertyName);
		return false;
	}

	// An override value alone is ignored unless its bOverride_ companion is set — flip it so the
	// value takes effect (e.g. SizeBox WidthOverride actually clamps instead of no-op'ing).
	RaiseOverrideCompanionIfPresent(Resolved.TargetObject, Resolved.Property);

	Resolved.TargetObject->Modify();
	// If we wrote to the widget's slot (layout/alignment/z-order), the change is structural —
	// mark it so the editor refreshes the layout.
	const bool bWroteToSlot = (Resolved.TargetObject == Widget->Slot);
	MarkWidgetBlueprintModified(WidgetBP, bWroteToSlot);

	return true;
}

bool UWidgetService::ReparentWidget(
	const FString& WidgetPath,
	const FString& WidgetName,
	const FString& NewParentName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: Widget '%s' not found"), *WidgetName);
		return false;
	}

	if (Widget == WidgetBP->WidgetTree->RootWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: Cannot reparent the root widget"));
		return false;
	}

	UWidget* NewParentWidget = FindWidgetByName(WidgetBP, NewParentName);
	UPanelWidget* NewParent = Cast<UPanelWidget>(NewParentWidget);
	if (!NewParent)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: New parent '%s' not found or is not a panel widget"), *NewParentName);
		return false;
	}

	// Reject moving a panel into itself or one of its own descendants (would create a cycle).
	for (UWidget* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->GetParent())
	{
		if (Ancestor == Widget)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: '%s' cannot be moved into itself or a descendant"), *WidgetName);
			return false;
		}
	}

	if (Widget->GetParent() == NewParent)
	{
		// Already parented here — nothing to do, treat as success.
		return true;
	}

	WidgetBP->Modify();

	// Detach from the current parent (preserves the widget object), then add under the new parent.
	if (UPanelWidget* OldParent = Widget->GetParent())
	{
		OldParent->RemoveChild(Widget);
	}

	UPanelSlot* NewSlot = NewParent->AddChild(Widget);
	if (!NewSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReparentWidget: Failed to add '%s' under '%s' (panel may be full)"), *WidgetName, *NewParentName);
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	UE_LOG(LogTemp, Log, TEXT("UWidgetService::ReparentWidget: Moved '%s' under '%s'"), *WidgetName, *NewParentName);
	return true;
}

TArray<FWidgetPropertyInfo> UWidgetService::ListProperties(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bEditableOnly)
{
	TArray<FWidgetPropertyInfo> Properties;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Properties;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Properties;
	}

	// Iterate all properties
	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		// Skip non-blueprint visible properties if filtering
		bool bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
		bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);

		if (bEditableOnly && !bIsEditable)
		{
			continue;
		}

		FWidgetPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = bIsEditable;
		Info.bIsBlueprintVisible = bIsBlueprintVisible;

		// Get category
		Info.Category = Property->GetMetaData(TEXT("Category"));

		// Get current value
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
		Property->ExportTextItem_Direct(Info.CurrentValue, ValuePtr, nullptr, Widget, PPF_None);

		Properties.Add(Info);
	}

	return Properties;
}

bool UWidgetService::SetFont(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FWidgetFontInfo& FontInfo,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveFontProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Font property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return false;
	}

	FSlateFontInfo& SlateFont = *reinterpret_cast<FSlateFontInfo*>(Resolved.ValuePtr);
	if (!FontInfo.FontFamily.IsEmpty())
	{
		UObject* FontAsset = UEditorAssetLibrary::LoadAsset(FontInfo.FontFamily);
		if (!FontAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Failed to load font asset '%s'"), *FontInfo.FontFamily);
			return false;
		}

		SlateFont.FontObject = FontAsset;
	}

	SlateFont.TypefaceFontName = FontInfo.Typeface.IsEmpty() ? FName(TEXT("Regular")) : FName(*FontInfo.Typeface);
	SlateFont.Size = FontInfo.Size;
	SlateFont.LetterSpacing = FontInfo.LetterSpacing;

	FLinearColor OutlineColor = FLinearColor::Black;
	if (ParseLinearColor(FontInfo.OutlineColor, OutlineColor))
		{
		SlateFont.OutlineSettings.OutlineColor = OutlineColor;
		}
	SlateFont.OutlineSettings.OutlineSize = FontInfo.OutlineSize;

	Resolved.TargetObject->Modify();
	Widget->Modify();

	bool bColorsApplied = true;

	// ColorAndOpacity is an FSlateColor — the plain-text setter no-ops on it, so route through the
	// FSlateColor-aware setter and fail loudly if a provided colour did not land.
	if (!FontInfo.Color.IsEmpty() && !TrySetSlateColorProperty(Widget, TEXT("ColorAndOpacity"), FontInfo.Color))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Failed to apply ColorAndOpacity '%s' on widget '%s' — pass an FSlateColor tuple, e.g. \"(R=..,G=..,B=..,A=..)\" (UMG colours are LINEAR)."), *FontInfo.Color, *ComponentName);
		bColorsApplied = false;
	}

	TrySetPropertyText(Widget, TEXT("ShadowOffset"), FontInfo.ShadowOffset);

	// ShadowColorAndOpacity is an FLinearColor, so a strict text import lands directly; still verify.
	if (!FontInfo.ShadowColor.IsEmpty() && !TrySetPropertyText(Widget, TEXT("ShadowColorAndOpacity"), FontInfo.ShadowColor))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Failed to apply ShadowColorAndOpacity '%s' on widget '%s' — pass an FLinearColor tuple, e.g. \"(R=..,G=..,B=..,A=..)\"."), *FontInfo.ShadowColor, *ComponentName);
		bColorsApplied = false;
	}

	MarkWidgetBlueprintModified(WidgetBP);
	return bColorsApplied;
}

FWidgetFontInfo UWidgetService::GetFont(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName)
{
	FWidgetFontInfo Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Result;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Result;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveFontProperty(Widget, PropertyName, Resolved))
	{
		return Result;
	}

	const FSlateFontInfo& SlateFont = *reinterpret_cast<const FSlateFontInfo*>(Resolved.ValuePtr);
	Result.FontFamily = SlateFont.FontObject ? SlateFont.FontObject->GetPathName() : FString();
	Result.Typeface = SlateFont.TypefaceFontName.IsNone() ? TEXT("Regular") : SlateFont.TypefaceFontName.ToString();
	Result.Size = SlateFont.Size;
	Result.LetterSpacing = SlateFont.LetterSpacing;
	Result.OutlineSize = SlateFont.OutlineSettings.OutlineSize;
	Result.OutlineColor = SlateFont.OutlineSettings.OutlineColor.ToString();

	TryGetPropertyText(Widget, TEXT("ColorAndOpacity"), Result.Color);
	TryGetPropertyText(Widget, TEXT("ShadowOffset"), Result.ShadowOffset);
	TryGetPropertyText(Widget, TEXT("ShadowColorAndOpacity"), Result.ShadowColor);

	return Result;
}

bool UWidgetService::SetBrush(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& SlotName,
	const FWidgetBrushInfo& BrushInfo)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveBrushProperty(Widget, SlotName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetBrush: Brush slot '%s' not found on widget '%s'"), *SlotName, *ComponentName);
		return false;
	}

	FSlateBrush& SlateBrush = *reinterpret_cast<FSlateBrush*>(Resolved.ValuePtr);
	SlateBrush.SetResourceObject(nullptr);
	SlateBrush.ImageType = BrushInfo.ResourcePath.IsEmpty() ? ESlateBrushImageType::NoImage : ESlateBrushImageType::FullColor;
	SlateBrush.DrawAs = StringToBrushDrawType(BrushInfo.DrawAs);

	FLinearColor TintColor = FLinearColor::White;
	if (ParseLinearColor(BrushInfo.TintColor, TintColor))
	{
		SlateBrush.TintColor = FSlateColor(TintColor);
	}

	FVector2D ImageSize = FVector2D::ZeroVector;
	if (ParseVector2D(BrushInfo.ImageSize, ImageSize))
	{
		SlateBrush.SetImageSize(ImageSize);
	}

	FMargin Margin;
	if (ParseMargin(BrushInfo.Margin, Margin))
	{
		SlateBrush.Margin = Margin;
	}

	if (SlateBrush.DrawAs == ESlateBrushDrawType::RoundedBox)
	{
		FVector4 CornerRadius;
		if (ParseCornerRadius(BrushInfo.CornerRadius, CornerRadius))
		{
			SlateBrush.OutlineSettings.CornerRadii = CornerRadius;
			SlateBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		}
	}

	if (!BrushInfo.ResourcePath.IsEmpty())
	{
		UObject* ResourceObject = UEditorAssetLibrary::LoadAsset(BrushInfo.ResourcePath);
		if (!ResourceObject)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetBrush: Failed to load resource '%s'"), *BrushInfo.ResourcePath);
			return false;
		}

		SlateBrush.SetResourceObject(ResourceObject);
	}

	Resolved.TargetObject->Modify();
	MarkWidgetBlueprintModified(WidgetBP);
	return true;
}

FWidgetBrushInfo UWidgetService::GetBrush(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& SlotName)
{
	FWidgetBrushInfo Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Result;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Result;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveBrushProperty(Widget, SlotName, Resolved))
	{
		return Result;
	}

	const FSlateBrush& SlateBrush = *reinterpret_cast<const FSlateBrush*>(Resolved.ValuePtr);
	Result.ResourcePath = SlateBrush.GetResourceObject() ? SlateBrush.GetResourceObject()->GetPathName() : FString();
	Result.TintColor = SlateBrush.TintColor.GetSpecifiedColor().ToString();
	Result.DrawAs = BrushDrawTypeToString(SlateBrush.DrawAs);
	const FVector2D ImageSize = SlateBrush.GetImageSize();
	Result.ImageSize = FString::Printf(TEXT("(X=%0.6f,Y=%0.6f)"), ImageSize.X, ImageSize.Y);
	Result.Margin = FString::Printf(TEXT("(Left=%0.6f,Top=%0.6f,Right=%0.6f,Bottom=%0.6f)"), SlateBrush.Margin.Left, SlateBrush.Margin.Top, SlateBrush.Margin.Right, SlateBrush.Margin.Bottom);
	Result.CornerRadius = FString::Printf(TEXT("(TopLeft=%0.6f,TopRight=%0.6f,BottomRight=%0.6f,BottomLeft=%0.6f)"), SlateBrush.OutlineSettings.CornerRadii.X, SlateBrush.OutlineSettings.CornerRadii.Y, SlateBrush.OutlineSettings.CornerRadii.Z, SlateBrush.OutlineSettings.CornerRadii.W);

	return Result;
}

TArray<FWidgetAnimInfo> UWidgetService::ListAnimations(const FString& WidgetPath)
{
	TArray<FWidgetAnimInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	for (UWidgetAnimation* Animation : WidgetBP->Animations)
	{
		if (!Animation)
		{
			continue;
		}

		FWidgetAnimInfo Info;
#if WITH_EDITOR
		Info.AnimationName = Animation->GetDisplayLabel().IsEmpty() ? Animation->GetName() : Animation->GetDisplayLabel();
#else
		Info.AnimationName = Animation->GetName();
#endif
		Info.Duration = Animation->GetEndTime() - Animation->GetStartTime();
		Info.TrackCount = GetAnimationTrackCount(Animation);
		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::CreateAnimation(
	const FString& WidgetPath,
	const FString& AnimationName,
	float Duration)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	if (AnimationName.IsEmpty() || FindAnimationByName(WidgetBP, AnimationName) != nullptr)
	{
		return false;
	}

	WidgetBP->Modify();

	UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
	if (!NewAnimation)
	{
		return false;
	}

#if WITH_EDITOR
	NewAnimation->SetDisplayLabel(AnimationName);
#endif
	NewAnimation->Rename(*AnimationName, WidgetBP);
	NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, FName(*AnimationName), RF_Transactional);
	if (!NewAnimation->MovieScene)
	{
		return false;
	}

	NewAnimation->MovieScene->SetDisplayRate(FFrameRate(20, 1));
	const FFrameTime OutFrame = FMath::Max(Duration, 0.0f) * NewAnimation->MovieScene->GetTickResolution();
	NewAnimation->MovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, OutFrame.FrameNumber + 1));
	NewAnimation->MovieScene->GetEditorData().WorkStart = 0.0f;
	NewAnimation->MovieScene->GetEditorData().WorkEnd = FMath::Max(Duration, 0.0f);

	WidgetBP->Animations.Add(NewAnimation);
	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::RemoveAnimation(
	const FString& WidgetPath,
	const FString& AnimationName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	if (!Animation)
	{
		return false;
	}

	WidgetBP->Modify();
	WidgetBP->Animations.Remove(Animation);
	Animation->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::AddAnimationTrack(
	const FString& WidgetPath,
	const FString& AnimationName,
	const FString& ComponentName,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* DesignWidget = FindWidgetByName(WidgetBP, ComponentName);
	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	UUserWidget* PreviewWidget = CreateWidgetInstanceForBlueprint(WidgetBP);
	if (!DesignWidget || !Animation || !PreviewWidget || !Animation->GetMovieScene())
	{
		return false;
	}

	FWidgetAnimationPropertyTarget Target;
	if (!ResolveAnimationPropertyTarget(DesignWidget, PreviewWidget, PropertyName, Target))
	{
		return false;
	}

	const FGuid BindingGuid = EnsureAnimationBinding(Animation, PreviewWidget, Target.AnimatedObject);
	if (!BindingGuid.IsValid())
	{
		return false;
	}

	if (Target.TrackType == EWidgetAnimationTrackType::Float)
	{
		UMovieSceneFloatTrack* Track = FindOrAddPropertyTrack<UMovieSceneFloatTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneFloatSection* Section = FindOrAddSection<UMovieSceneFloatSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		float CurrentValue = 0.0f;
		if (ResolvePropertyPath(Target.AnimatedObject->GetClass(), Target.AnimatedObject, Target.PropertyPath, Property, ValuePtr) && ExportFloatValue(ValuePtr, Property, Target.AnimatedObject, CurrentValue))
		{
			Section->GetChannel().SetDefault(CurrentValue);
		}
	}
	else if (Target.TrackType == EWidgetAnimationTrackType::Color)
	{
		UMovieSceneColorTrack* Track = FindOrAddPropertyTrack<UMovieSceneColorTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneColorSection* Section = FindOrAddSection<UMovieSceneColorSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FLinearColor CurrentColor = FLinearColor::White;
		if (ResolvePropertyPath(Target.AnimatedObject->GetClass(), Target.AnimatedObject, Target.PropertyPath, Property, ValuePtr) && ExportLinearColorValue(ValuePtr, Property, Target.AnimatedObject, CurrentColor))
		{
			Section->GetRedChannel().SetDefault(CurrentColor.R);
			Section->GetGreenChannel().SetDefault(CurrentColor.G);
			Section->GetBlueChannel().SetDefault(CurrentColor.B);
			Section->GetAlphaChannel().SetDefault(CurrentColor.A);
		}
	}
	else
	{
		UMovieSceneMarginTrack* Track = FindOrAddPropertyTrack<UMovieSceneMarginTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneMarginSection* Section = FindOrAddSection<UMovieSceneMarginSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Target.AnimatedObject);
		if (!Track || !Section || !CanvasSlot)
		{
			return false;
		}

		const FMargin Offsets = CanvasSlot->GetLayout().Offsets;
		Section->LeftCurve.SetDefault(Offsets.Left);
		Section->TopCurve.SetDefault(Offsets.Top);
		Section->RightCurve.SetDefault(Offsets.Right);
		Section->BottomCurve.SetDefault(Offsets.Bottom);
	}

	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::AddKeyframe(
	const FString& WidgetPath,
	const FString& AnimationName,
	const FString& ComponentName,
	const FString& PropertyName,
	const FWidgetAnimKeyframe& Keyframe)
{
	if (!AddAnimationTrack(WidgetPath, AnimationName, ComponentName, PropertyName))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* DesignWidget = FindWidgetByName(WidgetBP, ComponentName);
	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	UUserWidget* PreviewWidget = CreateWidgetInstanceForBlueprint(WidgetBP);
	if (!DesignWidget || !Animation || !PreviewWidget || !Animation->GetMovieScene())
	{
		return false;
	}

	FWidgetAnimationPropertyTarget Target;
	if (!ResolveAnimationPropertyTarget(DesignWidget, PreviewWidget, PropertyName, Target))
	{
		return false;
	}

	const FGuid BindingGuid = EnsureAnimationBinding(Animation, PreviewWidget, Target.AnimatedObject);
	if (!BindingGuid.IsValid())
	{
		return false;
	}

	const FFrameNumber FrameNumber = (Keyframe.Time * Animation->GetMovieScene()->GetTickResolution()).FrameNumber;
	EnsurePlaybackRangeIncludesFrame(Animation->GetMovieScene(), FrameNumber);
	const EMovieSceneKeyInterpolation Interpolation = StringToKeyInterpolation(Keyframe.Interpolation);

	if (Target.TrackType == EWidgetAnimationTrackType::Float)
	{
		float ParsedValue = 0.0f;
		if (!LexTryParseString(ParsedValue, *Keyframe.Value))
		{
			return false;
		}

		UMovieSceneFloatTrack* Track = FindOrAddPropertyTrack<UMovieSceneFloatTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneFloatSection* Section = FindOrAddSection<UMovieSceneFloatSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		AddKeyToChannel(&Section->GetChannel(), FrameNumber, ParsedValue, Interpolation);
	}
	else if (Target.TrackType == EWidgetAnimationTrackType::Color)
	{
		FLinearColor ParsedValue = FLinearColor::White;
		if (!ParseLinearColor(Keyframe.Value, ParsedValue))
		{
			return false;
		}

		UMovieSceneColorTrack* Track = FindOrAddPropertyTrack<UMovieSceneColorTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneColorSection* Section = FindOrAddSection<UMovieSceneColorSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		AddKeyToChannel(&Section->GetRedChannel(), FrameNumber, ParsedValue.R, Interpolation);
		AddKeyToChannel(&Section->GetGreenChannel(), FrameNumber, ParsedValue.G, Interpolation);
		AddKeyToChannel(&Section->GetBlueChannel(), FrameNumber, ParsedValue.B, Interpolation);
		AddKeyToChannel(&Section->GetAlphaChannel(), FrameNumber, ParsedValue.A, Interpolation);
	}
	else
	{
		float ParsedValue = 0.0f;
		if (!LexTryParseString(ParsedValue, *Keyframe.Value))
		{
			return false;
		}

		UMovieSceneMarginTrack* Track = FindOrAddPropertyTrack<UMovieSceneMarginTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneMarginSection* Section = FindOrAddSection<UMovieSceneMarginSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		switch (Target.MarginChannelIndex)
		{
		case 0:
			AddKeyToChannel(&Section->LeftCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 1:
			AddKeyToChannel(&Section->TopCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 2:
			AddKeyToChannel(&Section->RightCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 3:
			AddKeyToChannel(&Section->BottomCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		default:
			return false;
		}
	}

	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

namespace WidgetPreviewStateInternal
{
	// Reflection walk over a UObject/UStruct value collecting every texture a FSlateBrush references
	// (directly as a UTexture2D resource, or through a material's used textures). Walks nested struct
	// properties (e.g. FButtonStyle -> FSlateBrush) to a small depth; skips arrays/maps/objects on purpose.
	static void CollectBrushTextures(const UStruct* Struct, const void* Container, int32 Depth, TSet<UTexture2D*>& OutTextures)
	{
		if (!Struct || !Container || Depth > 4)
		{
			return;
		}

		for (TFieldIterator<FStructProperty> It(Struct); It; ++It)
		{
			const FStructProperty* StructProp = *It;
			if (!StructProp || !StructProp->Struct)
			{
				continue;
			}

			for (int32 Index = 0; Index < StructProp->ArrayDim; ++Index)
			{
				const void* Value = StructProp->ContainerPtrToValuePtr<void>(Container, Index);
				if (StructProp->Struct == FSlateBrush::StaticStruct())
				{
					const FSlateBrush* Brush = static_cast<const FSlateBrush*>(Value);
					UObject* Resource = Brush ? Brush->GetResourceObject() : nullptr;
					if (UTexture2D* Texture = Cast<UTexture2D>(Resource))
					{
						OutTextures.Add(Texture);
					}
					else if (const UMaterialInterface* Material = Cast<UMaterialInterface>(Resource))
					{
						TArray<UTexture*> MaterialTextures;
						Material->GetUsedTextures(MaterialTextures);
						for (UTexture* MaterialTexture : MaterialTextures)
						{
							if (UTexture2D* MaterialTexture2D = Cast<UTexture2D>(MaterialTexture))
							{
								OutTextures.Add(MaterialTexture2D);
							}
						}
					}
				}
				else
				{
					CollectBrushTextures(StructProp->Struct, Value, Depth + 1, OutTextures);
				}
			}
		}
	}

	// FND-01349: a brush texture that is not yet streamed in renders blank on the first draw.
	// Make every brush texture of the instance resident (and finish any editor async build) before DrawWidget.
	static void ForceLoadBrushTextures(UUserWidget* WidgetInstance)
	{
		if (!WidgetInstance)
		{
			return;
		}

		TSet<UTexture2D*> Textures;
		CollectBrushTextures(WidgetInstance->GetClass(), WidgetInstance, 0, Textures);
		if (const UWidgetTree* Tree = WidgetInstance->WidgetTree)
		{
			Tree->ForEachWidgetAndDescendants([&Textures](UWidget* Widget)
			{
				if (Widget)
				{
					CollectBrushTextures(Widget->GetClass(), Widget, 0, Textures);
				}
			});
		}

		if (Textures.IsEmpty())
		{
			return;
		}

#if WITH_EDITOR
		TArray<UTexture*> ToFinish;
		for (UTexture2D* Texture : Textures)
		{
			ToFinish.Add(Texture);
		}
		FTextureCompilingManager::Get().FinishCompilation(ToFinish);
#endif

		for (UTexture2D* Texture : Textures)
		{
			Texture->SetForceMipLevelsToBeResident(30.0f);
			Texture->WaitForStreaming();
		}
	}

	// --- t110: state mechanics helpers (WI-1561; DOC-0058) ---

	static void AddStateLog(TArray<FString>& StateLog, const FString& Line)
	{
		UE_LOG(LogTemp, Log, TEXT("UWidgetService::CapturePreviewState: %s"), *Line);
		StateLog.Add(Line);
	}

	// Calls Target->FunctionName(Arg) when FunctionName is a UFUNCTION whose only non-return parameter is an FName.
	static bool CallSingleNameFunction(UObject* Target, FName FunctionName, FName Arg, FString& OutWhy)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			OutWhy = TEXT("not found on the widget class");
			return false;
		}

		FNameProperty* NameParam = nullptr;
		int32 NumInputParams = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			++NumInputParams;
			if (!NameParam)
			{
				NameParam = CastField<FNameProperty>(*It);
			}
		}
		if (NumInputParams != 1 || !NameParam)
		{
			OutWhy = TEXT("signature is not (FName)");
			return false;
		}

		uint8* Parms = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
		FMemory::Memzero(Parms, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parms);
		}
		NameParam->SetPropertyValue_InContainer(Parms, Arg);
		Target->ProcessEvent(Function, Parms);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parms);
		}
		return true;
	}

	// UCommonButtonBase::SetIsSelected(true, /*bGiveClickFeedback*/ false) through reflection, so VibeUE takes
	// no CommonUI module dependency. Returns false when Target has no such UFUNCTION.
	static bool CallSetIsSelected(UObject* Target)
	{
		UFunction* Function = Target ? Target->FindFunction(TEXT("SetIsSelected")) : nullptr;
		if (!Function)
		{
			return false;
		}

		uint8* Parms = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
		FMemory::Memzero(Parms, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Parms);
		}
		if (FBoolProperty* SelectedParam = CastField<FBoolProperty>(Function->FindPropertyByName(TEXT("InSelected"))))
		{
			SelectedParam->SetPropertyValue_InContainer(Parms, true);
		}
		if (FBoolProperty* FeedbackParam = CastField<FBoolProperty>(Function->FindPropertyByName(TEXT("bGiveClickFeedback"))))
		{
			FeedbackParam->SetPropertyValue_InContainer(Parms, false);
		}
		Target->ProcessEvent(Function, Parms);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Parms);
		}
		return true;
	}

	// The instance itself when it is a UCommonButtonBase, else its first UCommonButtonBase descendant (null when CommonUI is absent).
	static UWidget* FindCommonButton(UUserWidget* Instance)
	{
		const UClass* CommonButtonClass = FindObject<UClass>(nullptr, TEXT("/Script/CommonUI.CommonButtonBase"));
		if (!Instance || !CommonButtonClass)
		{
			return nullptr;
		}
		if (Instance->IsA(CommonButtonClass))
		{
			return Instance;
		}
		UWidget* Found = nullptr;
		if (Instance->WidgetTree)
		{
			Instance->WidgetTree->ForEachWidgetAndDescendants([&Found, CommonButtonClass](UWidget* Widget)
			{
				if (!Found && Widget && Widget->IsA(CommonButtonClass))
				{
					Found = Widget;
				}
			});
		}
		return Found;
	}

	static void ForEachClassAnimation(UUserWidget* Instance, TFunctionRef<void(UWidgetAnimation*)> Visitor)
	{
		for (UClass* Class = Instance ? Instance->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
		{
			if (UWidgetBlueprintGeneratedClass* Generated = Cast<UWidgetBlueprintGeneratedClass>(Class))
			{
				for (UWidgetAnimation* Animation : Generated->Animations)
				{
					if (Animation)
					{
						Visitor(Animation);
					}
				}
			}
		}
	}

	// Animations currently PLAYING (a paused, held pose does not count).
	static TArray<UWidgetAnimation*> GetPlayingAnimations(UUserWidget* Instance)
	{
		TArray<UWidgetAnimation*> Playing;
		ForEachClassAnimation(Instance, [Instance, &Playing](UWidgetAnimation* Animation)
		{
			if (Instance->IsAnimationPlaying(Animation))
			{
				Playing.AddUnique(Animation);
			}
		});
		return Playing;
	}

	static FString JoinAnimationNames(const TArray<UWidgetAnimation*>& Animations)
	{
		TArray<FString> Names;
		for (const UWidgetAnimation* Animation : Animations)
		{
			Names.Add(Animation->GetName());
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(",")) : FString(TEXT("none"));
	}

	// Resolve a UWidgetAnimation by property name (BindWidgetAnim[Optional] / BP variable) or generated-class animation name.
	static UWidgetAnimation* FindWidgetAnimation(UUserWidget* Instance, const FString& Name)
	{
		if (!Instance || Name.IsEmpty())
		{
			return nullptr;
		}
		if (FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(Instance->GetClass()->FindPropertyByName(FName(*Name))))
		{
			if (Property->PropertyClass && Property->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()))
			{
				if (UWidgetAnimation* Animation = Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(Instance)))
				{
					return Animation;
				}
			}
		}
		UWidgetAnimation* Found = nullptr;
		const FString InstName = Name + TEXT("_INST");
		ForEachClassAnimation(Instance, [&Found, &Name, &InstName](UWidgetAnimation* Animation)
		{
			if (Found)
			{
				return;
			}
			bool bMatch = Animation->GetName() == Name || Animation->GetName() == InstName;
#if WITH_EDITOR
			bMatch = bMatch || Animation->GetDisplayLabel() == Name;
#endif
			if (bMatch)
			{
				Found = Animation;
			}
		});
		return Found;
	}

	static bool IsButtonSlateType(const TSharedRef<SWidget>& Widget)
	{
		const FName Type = Widget->GetType();
		return Type == FName(TEXT("SButton")) || Type == FName(TEXT("SCommonButton")) || Widget->GetTypeAsString().EndsWith(TEXT("Button"));
	}

	static TSharedPtr<SWidget> FindFirstButtonSlateWidget(const TSharedRef<SWidget>& Root, int32 Depth = 0)
	{
		if (Depth > 64)
		{
			return nullptr;
		}
		if (IsButtonSlateType(Root))
		{
			return Root;
		}
		if (FChildren* Children = Root->GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				if (TSharedPtr<SWidget> Found = FindFirstButtonSlateWidget(Children->GetChildAt(Index), Depth + 1))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}

	// Synthetic hover (DOC-0058): FSlateApplication ignores virtual windows, so route OnMouseEnter by hand along the
	// virtual window's hittest-grid bubble path at the widget centre (WidgetInteractionComponent precedent), which
	// covers the root SObjectWidget and the SButton/SCommonButton under it. Fallback: root + first button by tree walk.
	static void SimulateHover(UUserWidget* Instance, const TSharedRef<SWindow>& Window, FHittestGrid& HitTestGrid, TArray<FString>& StateLog)
	{
		TSharedPtr<SWidget> Root = Instance->GetCachedWidget();
		if (!Root.IsValid())
		{
			AddStateLog(StateLog, TEXT("hover: no cached root SWidget; skipped"));
			return;
		}

		const FGeometry RootGeometry = Root->GetPaintSpaceGeometry();
		const FVector2f Center = RootGeometry.GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f));
		const FPointerEvent PointerEvent(
			FSlateApplicationBase::CursorPointerIndex,
			Center,
			Center,
			TSet<FKey>(),
			EKeys::Invalid,
			0.0f,
			FModifierKeysState());

		TArray<FWidgetAndPointer> BubblePath = HitTestGrid.GetBubblePath(Center, 0.0f, /*bIgnoreEnabledStatus*/ false);
		int32 Entered = 0;
		bool bRootEntered = false;
		FString ButtonType;
		for (FWidgetAndPointer& Arranged : BubblePath)
		{
			if (Arranged.Widget == Window)
			{
				continue;
			}
			Arranged.Widget->OnMouseEnter(Arranged.Geometry, PointerEvent);
			++Entered;
			bRootEntered |= (Arranged.Widget == Root.ToSharedRef());
			if (ButtonType.IsEmpty() && IsButtonSlateType(Arranged.Widget))
			{
				ButtonType = Arranged.Widget->GetTypeAsString();
			}
		}

		if (!bRootEntered)
		{
			Root->OnMouseEnter(RootGeometry, PointerEvent);
			++Entered;
		}
		if (ButtonType.IsEmpty())
		{
			if (TSharedPtr<SWidget> Button = FindFirstButtonSlateWidget(Root.ToSharedRef()))
			{
				Button->OnMouseEnter(Button->GetPaintSpaceGeometry(), PointerEvent);
				++Entered;
				ButtonType = Button->GetTypeAsString() + TEXT("(tree-walk)");
			}
		}

		AddStateLog(StateLog, FString::Printf(
			TEXT("hover: OnMouseEnter at (%.1f,%.1f): bubble path %d widgets, %d entered, root %s, button %s, root IsHovered=%d"),
			Center.X, Center.Y, BubblePath.Num(), Entered,
			bRootEntered ? TEXT("via hittest") : TEXT("direct"),
			ButtonType.IsEmpty() ? TEXT("none") : *ButtonType,
			Root->IsHovered() ? 1 : 0));
	}

	// Steps (2)-(4) of DOC-0058 on an instance that has had one prepass draw.
	static void ApplyPreviewState(
		UUserWidget* Instance,
		const TSharedRef<SWindow>& Window,
		FHittestGrid& HitTestGrid,
		const FString& StateLower,
		const FWidgetPreviewStateOptions& Options,
		TArray<FString>& StateLog)
	{
		// (2) state hook: the widget's own state semantics (AS UHF_MaterialButtonBase::HF_PreviewState).
		bool bHookRan = false;
		const FName HookName = Options.HookFunction.IsEmpty() ? NAME_None : FName(*Options.HookFunction);
		if (!HookName.IsNone())
		{
			FString Why;
			bHookRan = CallSingleNameFunction(Instance, HookName, FName(*StateLower), Why);
			AddStateLog(StateLog, bHookRan
				? FString::Printf(TEXT("hook %s(%s) called"), *Options.HookFunction, *StateLower)
				: FString::Printf(TEXT("hook %s skipped: %s"), *Options.HookFunction, *Why));
			Instance->FlushAnimations();
		}

		// (3) generic setters. With a successful hook the disabled/selected setters are skipped: their
		// OnDisabled/OnSelected callbacks start animations that clobber the hook's held pose (t200 trap).
		if (Options.ApplyGenericSetters)
		{
			const TArray<UWidgetAnimation*> PlayingBefore = GetPlayingAnimations(Instance);
			bool bSettersRan = false;

			if (StateLower == TEXT("disabled"))
			{
				if (bHookRan)
				{
					// Generic guarantee that the widget really is disabled whatever the hook did (the kit AS hook already
					// calls SetIsEnabled(false) itself, so for kit widgets this is a measured no-op: byte-identical PNG, WI-1561
					// t225). Re-apply the hook afterwards: the OnDisabled callback's animation would otherwise clobber the
					// held Anim_Disabled pose (DrawWidget never advances animations).
					Instance->SetIsEnabled(false);
					Instance->FlushAnimations();
					FString Why;
					const bool bReapplied = CallSingleNameFunction(Instance, HookName, FName(*StateLower), Why);
					Instance->FlushAnimations();
					AddStateLog(StateLog, FString::Printf(
						TEXT("disabled: SetIsEnabled(false) then hook re-applied (%s); IsEnabled=%d"),
						bReapplied ? TEXT("ok") : *Why, Instance->GetIsEnabled() ? 1 : 0));
				}
				else
				{
					Instance->SetIsEnabled(false);
					bSettersRan = true;
					AddStateLog(StateLog, TEXT("disabled: SetIsEnabled(false)"));
				}
			}
			else if (StateLower == TEXT("selected"))
			{
				if (bHookRan)
				{
					AddStateLog(StateLog, TEXT("selected: SetIsSelected skipped (hook pose owns the state)"));
				}
				else if (UWidget* CommonButton = FindCommonButton(Instance))
				{
					const bool bCalled = CallSetIsSelected(CommonButton);
					bSettersRan = bCalled;
					AddStateLog(StateLog, FString::Printf(TEXT("selected: SetIsSelected(true) on %s %s"),
						*CommonButton->GetName(), bCalled ? TEXT("called") : TEXT("not callable")));
				}
				else
				{
					AddStateLog(StateLog, TEXT("selected: no UCommonButtonBase on the instance; nothing to select"));
				}
			}
			else if (StateLower == TEXT("focused"))
			{
				Instance->SetIsFocusable(true);
				APlayerController* OwningPlayer = Instance->GetOwningPlayer();
				if (OwningPlayer && OwningPlayer->IsLocalPlayerController() && OwningPlayer->Player)
				{
					Instance->SetFocus();
					bSettersRan = true;
					AddStateLog(StateLog, TEXT("focused: SetIsFocusable(true) + SetFocus()"));
				}
				else
				{
					// UWidget::SetUserFocus only logs a PIE message-log error without a local player; skip it.
					AddStateLog(StateLog, TEXT("focused: no LocalPlayer in the editor world; SetFocus skipped, focus visual = hook pose"));
				}
			}
			else if (StateLower == TEXT("hovered"))
			{
				SimulateHover(Instance, Window, HitTestGrid, StateLog);
				bSettersRan = true;
			}
			else if (StateLower == TEXT("pressed"))
			{
				AddStateLog(StateLog, TEXT("pressed = animation pose (no synthetic press route)"));
			}

			if (bSettersRan)
			{
				Instance->FlushAnimations();
				TArray<UWidgetAnimation*> Started = GetPlayingAnimations(Instance);
				Started.RemoveAll([&PlayingBefore](UWidgetAnimation* Animation) { return PlayingBefore.Contains(Animation); });
				AddStateLog(StateLog, FString::Printf(TEXT("setter callbacks started animations: %s"), *JoinAnimationNames(Started)));
				if (Started.Num() > 0)
				{
					if (bHookRan)
					{
						// DrawWidget never advances UMG animations, so a callback-started animation would hold its
						// frame 0 over the hook's pose: re-apply the hook so its pose wins.
						FString Why;
						CallSingleNameFunction(Instance, HookName, FName(*StateLower), Why);
						Instance->FlushAnimations();
						AddStateLog(StateLog, TEXT("hook re-applied over the started animations"));
					}
					else
					{
						// No hook: settle every started transition at its end pose (what real time would reach).
						for (UWidgetAnimation* Animation : Started)
						{
							Instance->PlayAnimation(Animation, Animation->GetEndTime(), 1, EUMGSequencePlayMode::Forward, 1.0f);
							Instance->PauseAnimation(Animation);
						}
						Instance->FlushAnimations();
						AddStateLog(StateLog, TEXT("started animations settled at their end pose"));
					}
				}
			}
		}

		// (4) explicit animation scrub: PlayAnimation(StartAtTime) -> PauseAnimation -> FlushAnimations (DOC-0058).
		if (!Options.AnimationName.IsEmpty())
		{
			if (UWidgetAnimation* Animation = FindWidgetAnimation(Instance, Options.AnimationName))
			{
				const float Time = Options.AnimationTime < 0.0f ? Animation->GetEndTime() : Options.AnimationTime;
				Instance->PlayAnimation(Animation, Time, 1, EUMGSequencePlayMode::Forward, 1.0f);
				Instance->PauseAnimation(Animation);
				Instance->FlushAnimations();
				AddStateLog(StateLog, FString::Printf(TEXT("animation %s scrubbed to %.3fs and paused"), *Animation->GetName(), Time));
			}
			else
			{
				AddStateLog(StateLog, FString::Printf(TEXT("animation %s not found; skipped"), *Options.AnimationName));
			}
		}

		Instance->FlushAnimations();
	}
}

FWidgetPreviewResult UWidgetService::CapturePreview(
	const FString& WidgetPath,
	int32 Width,
	int32 Height)
{
	// Legacy capture_preview behaviour: state "normal", no hook/setters, DPI scale 1, a single
	// DrawWidget with DeltaTime 0.1f, output "<WBP>.png". These option values reproduce it exactly.
	FWidgetPreviewStateOptions LegacyOptions;
	LegacyOptions.HookFunction.Reset();
	LegacyOptions.ApplyGenericSetters = false;
	LegacyOptions.DpiScale = 1.0f;
	LegacyOptions.Frames = 1;
	LegacyOptions.DeltaTime = 0.1f;
	return CapturePreviewInternal(WidgetPath, Width, Height, LegacyOptions, /*bLegacyOutputName*/ true);
}

FWidgetPreviewResult UWidgetService::CapturePreviewState(
	const FString& WidgetPath,
	int32 Width,
	int32 Height,
	const FWidgetPreviewStateOptions& Options)
{
	return CapturePreviewInternal(WidgetPath, Width, Height, Options, /*bLegacyOutputName*/ false);
}

FWidgetPreviewResult UWidgetService::CapturePreviewInternal(
	const FString& WidgetPath,
	int32 Width,
	int32 Height,
	const FWidgetPreviewStateOptions& Options,
	bool bLegacyOutputName)
{
	FWidgetPreviewResult Result;
	Result.Width = Width;
	Result.Height = Height;

	if (Width <= 0 || Height <= 0)
	{
		Result.ErrorMessage = TEXT("Width and Height must be greater than zero.");
		return Result;
	}

	const FString StateLower = Options.State.IsEmpty() ? FString(TEXT("normal")) : Options.State.ToLower();
	if (StateLower != TEXT("normal") && StateLower != TEXT("hovered") && StateLower != TEXT("pressed")
		&& StateLower != TEXT("focused") && StateLower != TEXT("disabled") && StateLower != TEXT("selected"))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown preview state '%s' (expected normal, hovered, pressed, focused, disabled or selected)."), *Options.State);
		return Result;
	}

	UUserWidget* WidgetInstance = nullptr;
	FString WidgetStem;
	// A /Script/ path can only name a native/AngelScript class; skip the asset load (it logs an Error
	// "Can't convert the path ... because it start with /Script/") and go straight to the class branch.
	UWidgetBlueprint* WidgetBP = WidgetPath.StartsWith(TEXT("/Script/")) ? nullptr : LoadWidgetBlueprint(WidgetPath);
	if (WidgetBP)
	{
		WidgetStem = WidgetBP->GetName();
		WidgetInstance = CreateWidgetInstanceForBlueprint(WidgetBP);
		if (!WidgetInstance)
		{
			Result.ErrorMessage = TEXT("Failed to create widget preview instance.");
			return Result;
		}
	}
	else
	{
		// --- WI-1564 t200 BEGIN: UUserWidget class-path fallback (e.g. /Script/Angelscript.CruiseUiSmoke) ---
		UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *WidgetPath);
		if (!WidgetClass)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Widget Blueprint not found, and '%s' is not a loadable UUserWidget class path (expected /Game/... asset or /Script/<Module>.<ClassName>)."),
				*WidgetPath);
			return Result;
		}

		WidgetStem = WidgetClass->GetName();
		WidgetInstance = CreateWidgetInstanceForClass(WidgetClass, WidgetStem);
		if (!WidgetInstance)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Failed to create widget preview instance for class '%s'."), *WidgetPath);
			return Result;
		}
		// --- WI-1564 t200 END ---

		// --- WI-1564 t205 BEGIN: pre-build the widget tree before TakeWidget ---
		// A code-built UUserWidget (AngelScript) creates its tree in BuildUi(); UUserWidget::RebuildWidget
		// reads the root inside TakeWidget, so a tree built later (Construct) is never seen (FND-01003), and
		// OnInitialized is skipped for a context-less CreateWidget(World, ...). BuildUi() MUST be idempotent
		// (Cruise contract: bUiBuilt guard) because the widget's own OnInitialized may also call it.
		if (UFunction* BuildFn = WidgetInstance->FindFunction(TEXT("BuildUi")); BuildFn && BuildFn->NumParms == 0)
		{
			UE_LOG(LogTemp, Verbose, TEXT("CapturePreviewInternal: pre-built '%s' via BuildUi() before TakeWidget"), *WidgetPath);
			WidgetInstance->ProcessEvent(BuildFn, nullptr);
		}
		// --- WI-1564 t205 END ---
	}

	TSharedRef<SWidget> SlateWidget = WidgetInstance->TakeWidget();

	// --- t120 BEGIN: Options.DesiredSize (state path only; legacy capture_preview never enters) ---
	// Mirrors Tools/ui_kit/rig/spawn.py desired_size: a fixed LOGICAL-size (pre-DPI) part centred in
	// the render target. Zero on an axis leaves that axis unconstrained; both zero = fill the target.
	if (!bLegacyOutputName && (Options.DesiredSize.X > 0.0 || Options.DesiredSize.Y > 0.0))
	{
		TSharedRef<SBox> SizeBox = SNew(SBox)
			[
				SlateWidget
			];
		if (Options.DesiredSize.X > 0.0)
		{
			SizeBox->SetWidthOverride(static_cast<float>(Options.DesiredSize.X));
		}
		if (Options.DesiredSize.Y > 0.0)
		{
			SizeBox->SetHeightOverride(static_cast<float>(Options.DesiredSize.Y));
		}
		SlateWidget = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SizeBox
			];
	}
	// --- t120 END ---

	// Gamma must be applied exactly once. CreateTargetFor(..., /*bUseGammaCorrection*/ true) makes
	// an sRGB render target (IsSRGB() == true, so ExportRenderTarget2DAsPNG writes a true sRGB PNG);
	// pair it with a LINEAR-space renderer (FWidgetRenderer(false)) that emits linear pixels for the
	// target to sRGB-encode on write. The old code used a gamma-space renderer (FWidgetRenderer(true))
	// AND an sRGB target, encoding every colour twice. This mirrors the engine's UMG thumbnail path
	// (FWidgetBlueprintEditorUtils::DrawSWidgetInRenderTargetInternal): linear renderer + sRGB target.
	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(FVector2D(Width, Height), TF_Bilinear, true);
	if (!RenderTarget)
	{
		Result.ErrorMessage = TEXT("Failed to create preview render target.");
		return Result;
	}

	FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(false);
	// Leave the desired-size prepass enabled (the FWidgetRenderer default) so Overlay-centred and
	// otherwise-aligned content is laid out against real widget sizes rather than zero; the old
	// SetIsPrepassNeeded(false) skipped that pass and drew centred content bottom-aligned. A
	// non-zero DeltaTime lets first-frame layout settle, matching the engine thumbnail renderer.
	const float DpiScale = Options.DpiScale > 0.0f
		? Options.DpiScale
		: GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
	Result.DpiScaleUsed = DpiScale;

	// --- t110 BEGIN: state mechanics (state path only; legacy capture_preview never enters) ---
	// A persistent virtual window + hittest grid (what DrawWidget builds and discards per call) so the
	// prepass leaves real geometry and a hittest grid for the synthetic hover. Order per DOC-0058:
	// prepass draw -> hook -> generic setters / hover -> animation scrub -> FlushAnimations -> draws.
	TSharedPtr<SVirtualWindow> StateWindow;
	TUniquePtr<FHittestGrid> StateHitTestGrid;
	if (!bLegacyOutputName)
	{
		const FVector2D DrawSize(Width, Height);
		StateWindow = SNew(SVirtualWindow).Size(DrawSize);
		StateHitTestGrid = MakeUnique<FHittestGrid>();
		StateWindow->SetContent(SlateWidget);
		StateWindow->Resize(DrawSize);
		WidgetRenderer->DrawWindow(RenderTarget, *StateHitTestGrid, StateWindow.ToSharedRef(), DpiScale, DrawSize, Options.DeltaTime);

		TArray<FString> StateLog;
		WidgetPreviewStateInternal::ApplyPreviewState(WidgetInstance, StateWindow.ToSharedRef(), *StateHitTestGrid, StateLower, Options, StateLog);
		Result.StateLog = FString::Join(StateLog, TEXT(" | "));
	}
	// --- t110 END ---

	// --- t120 BEGIN: brush texture residency (FND-01349; state path only, legacy path unchanged) ---
	if (!bLegacyOutputName)
	{
		WidgetPreviewStateInternal::ForceLoadBrushTextures(WidgetInstance);
	}
	// --- t120 END ---

	// DrawWidget(..., Scale, ...) with Scale == 1.f is identical to the 4-argument overload CapturePreview used.
	const int32 FrameCount = FMath::Max(1, Options.Frames);
	for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		if (StateWindow.IsValid())
		{
			WidgetRenderer->DrawWindow(RenderTarget, *StateHitTestGrid, StateWindow.ToSharedRef(), DpiScale, FVector2D(Width, Height), Options.DeltaTime);
		}
		else
		{
			WidgetRenderer->DrawWidget(RenderTarget, SlateWidget, DpiScale, FVector2D(Width, Height), Options.DeltaTime);
		}
	}
	if (StateWindow.IsValid())
	{
		StateWindow->SetContent(SNullWidget::NullWidget);
	}
	BeginCleanup(WidgetRenderer);

	const FString OutputDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WidgetPreviews")));
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	FString OutputStem;
	if (bLegacyOutputName)
	{
		OutputStem = WidgetStem;
	}
	else if (!Options.OutputName.IsEmpty())
	{
		OutputStem = FPaths::MakeValidFileName(Options.OutputName);
	}
	else
	{
		OutputStem = FString::Printf(TEXT("%s_%s_%dx%d"), *WidgetStem, *StateLower, Width, Height);
	}
	const FString OutputPath = FPaths::Combine(OutputDir, OutputStem + TEXT(".png"));

	TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*OutputPath));
	if (!Archive)
	{
		Result.ErrorMessage = TEXT("Failed to create preview output file.");
		return Result;
	}

	if (!FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *Archive))
	{
		Result.ErrorMessage = TEXT("Failed to export preview PNG.");
		return Result;
	}

	Archive.Reset();
	Result.bSuccess = true;
	Result.OutputPath = OutputPath;
	return Result;
}

bool UWidgetService::StartPIE()
{
	if (!GEditor)
	{
		return false;
	}

	if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
	{
		return true;
	}

	FRequestPlaySessionParams SessionParams;
	GEditor->RequestPlaySession(SessionParams);
	return true;
}

bool UWidgetService::StopPIE()
{
	if (!GEditor)
	{
		return false;
	}

	if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
	{
		for (TPair<FString, TWeakObjectPtr<UUserWidget>>& Pair : GPIEWidgetInstances)
		{
			if (Pair.Value.IsValid())
			{
				Pair.Value->RemoveFromParent();
			}
		}
		GPIEWidgetInstances.Empty();
		GEditor->RequestEndPlayMap();
	}

	return true;
}

bool UWidgetService::IsPIERunning()
{
	return GEditor && (GEditor->PlayWorld != nullptr || GEditor->bIsSimulatingInEditor);
}

FPIEWidgetHandle UWidgetService::SpawnWidgetInPIE(
	const FString& WidgetPath,
	int32 ZOrder)
{
	FPIEWidgetHandle Result;

	if (!GEditor || !GEditor->PlayWorld)
	{
		Result.ErrorMessage = TEXT("PIE is not running.");
		return Result;
	}

	// NOTE: LoadWidgetBlueprint() uses UEditorAssetLibrary::LoadAsset, which returns null
	// during PIE — so resolve the object path directly with LoadObject here, which works
	// while PIE is running. Accept either a package path (/Game/UI/WBP_X) or a full object
	// path (/Game/UI/WBP_X.WBP_X).
	FString ObjectPath = WidgetPath;
	if (!ObjectPath.Contains(TEXT(".")))
	{
		FString AssetName;
		if (WidgetPath.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			ObjectPath = FString::Printf(TEXT("%s.%s"), *WidgetPath, *AssetName);
		}
	}
	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint not found.");
		return Result;
	}

	if (!WidgetBP->GeneratedClass || WidgetBP->Status == BS_Dirty)
	{
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	}

	if (!WidgetBP->GeneratedClass || !WidgetBP->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		Result.ErrorMessage = TEXT("Widget Blueprint is not a valid UserWidget class.");
		return Result;
	}

	UWorld* PlayWorld = GEditor->PlayWorld.Get();
	UClass* WidgetClass = WidgetBP->GeneratedClass;
	if (!PlayWorld || !WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		Result.ErrorMessage = TEXT("PIE world or widget class is invalid.");
		return Result;
	}

	const FName WidgetName = MakeUniqueObjectName(PlayWorld, WidgetClass, FName(*FString::Printf(TEXT("%s_PIE"), *WidgetBP->GetName())));
	UUserWidget* WidgetInstance = CreateWidget<UUserWidget>(PlayWorld, WidgetClass, WidgetName);
	if (!WidgetInstance)
	{
		Result.ErrorMessage = TEXT("Failed to create PIE widget instance.");
		return Result;
	}

	WidgetInstance->AddToViewport(ZOrder);

	Result.bValid = true;
	Result.InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	GPIEWidgetInstances.Add(Result.InstanceId, WidgetInstance);
	return Result;
}

FString UWidgetService::GetLiveProperty(
	const FPIEWidgetHandle& Handle,
	const FString& ComponentName,
	const FString& PropertyName)
{
	const TWeakObjectPtr<UUserWidget>* WidgetPtr = GPIEWidgetInstances.Find(Handle.InstanceId);
	if (!WidgetPtr || !WidgetPtr->IsValid())
	{
		return FString();
	}

	UWidget* Widget = FindRuntimeWidgetByName(WidgetPtr->Get(), ComponentName);
	if (!Widget)
	{
		return FString();
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		return FString();
	}

	FString ValueText;
	Resolved.Property->ExportTextItem_Direct(ValueText, Resolved.ValuePtr, nullptr, Resolved.TargetObject, PPF_None);
	return ValueText;
}

bool UWidgetService::RemoveWidgetFromPIE(const FPIEWidgetHandle& Handle)
{
	TWeakObjectPtr<UUserWidget>* WidgetPtr = GPIEWidgetInstances.Find(Handle.InstanceId);
	if (!WidgetPtr)
	{
		return false;
	}

	if (WidgetPtr->IsValid())
	{
		WidgetPtr->Get()->RemoveFromParent();
	}

	GPIEWidgetInstances.Remove(Handle.InstanceId);
	return true;
}

// =================================================================
// Event Handling
// =================================================================

TArray<FWidgetEventInfo> UWidgetService::GetAvailableEvents(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& WidgetType)
{
	TArray<FWidgetEventInfo> Events;

	// Get the class to inspect
	UClass* WidgetClass = nullptr;

	if (!WidgetType.IsEmpty())
	{
		TSubclassOf<UWidget> TypeClass = FindWidgetClass(WidgetType);
		if (TypeClass)
		{
			WidgetClass = *TypeClass;
		}
	}
	else if (!ComponentName.IsEmpty() && !WidgetPath.IsEmpty())
	{
		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (WidgetBP)
		{
			UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
			if (Widget)
			{
				WidgetClass = Widget->GetClass();
			}
		}
	}

	if (!WidgetClass)
	{
		// Default to base widget events
		WidgetClass = UWidget::StaticClass();
	}

	// Find multicast delegate properties (these are events)
	for (TFieldIterator<FMulticastDelegateProperty> It(WidgetClass); It; ++It)
	{
		FMulticastDelegateProperty* DelegateProp = *It;
		if (!DelegateProp)
		{
			continue;
		}

		FWidgetEventInfo Info;
		Info.EventName = DelegateProp->GetName();
		Info.EventType = TEXT("MulticastDelegate");
		
		// Get description from metadata
		Info.Description = DelegateProp->GetMetaData(TEXT("ToolTip"));
		if (Info.Description.IsEmpty())
		{
			Info.Description = FString::Printf(TEXT("Event: %s"), *Info.EventName);
		}

		Events.Add(Info);
	}

	// Add common widget events manually if not found
	if (Events.Num() == 0)
	{
		// Button-specific events
		if (WidgetType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnClicked"), TEXT("MulticastDelegate"), TEXT("Called when the button is clicked") });
			Events.Add({ TEXT("OnPressed"), TEXT("MulticastDelegate"), TEXT("Called when the button is pressed") });
			Events.Add({ TEXT("OnReleased"), TEXT("MulticastDelegate"), TEXT("Called when the button is released") });
			Events.Add({ TEXT("OnHovered"), TEXT("MulticastDelegate"), TEXT("Called when the button is hovered") });
			Events.Add({ TEXT("OnUnhovered"), TEXT("MulticastDelegate"), TEXT("Called when hover ends") });
		}
		// Slider events
		else if (WidgetType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnValueChanged"), TEXT("MulticastDelegate"), TEXT("Called when the slider value changes") });
		}
		// CheckBox events
		else if (WidgetType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnCheckStateChanged"), TEXT("MulticastDelegate"), TEXT("Called when check state changes") });
		}
		else if (WidgetType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnSelectionChanged"), TEXT("MulticastDelegate"), TEXT("Called when selection changes") });
			Events.Add({ TEXT("OnOpening"), TEXT("MulticastDelegate"), TEXT("Called when the combo box opens") });
		}
		else if (WidgetType.Equals(TEXT("SpinBox"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnValueChanged"), TEXT("MulticastDelegate"), TEXT("Called when the spin box value changes") });
			Events.Add({ TEXT("OnValueCommitted"), TEXT("MulticastDelegate"), TEXT("Called when the value is committed") });
		}
		else if (WidgetType.Equals(TEXT("ExpandableArea"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnExpansionChanged"), TEXT("MulticastDelegate"), TEXT("Called when expansion state changes") });
		}
		else if (WidgetType.Equals(TEXT("InputKeySelector"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnKeySelected"), TEXT("MulticastDelegate"), TEXT("Called when a key is selected") });
			Events.Add({ TEXT("OnIsSelectingKeyChanged"), TEXT("MulticastDelegate"), TEXT("Called when key selection mode changes") });
		}
	}

	return Events;
}

bool UWidgetService::BindEvent(
	const FString& WidgetPath,
	const FString& WidgetName,
	const FString& EventName,
	const FString& FunctionName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	// 1. Resolve the widget component so we know its class (the delegate owner).
	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Widget '%s' not found in '%s'"), *WidgetName, *WidgetPath);
		return false;
	}
	UClass* WidgetClass = Widget->GetClass();

	// 2. Verify the event (a multicast delegate) actually exists on that widget class.
	const FName EventFName(*EventName);
	if (!FindFProperty<FMulticastDelegateProperty>(WidgetClass, EventFName))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Event '%s' does not exist on widget '%s' (%s). Use get_available_events to list valid events."),
			*EventName, *WidgetName, *WidgetClass->GetName());
		return false;
	}

	// 3. The widget must be a Blueprint variable for an event to bind to it. Promote it if needed.
	const FName WidgetFName(*WidgetName);
	auto FindWidgetVarProp = [WidgetBP, WidgetFName]() -> FObjectProperty*
	{
		UClass* SkelClass = WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass : WidgetBP->GeneratedClass;
		return SkelClass ? FindFProperty<FObjectProperty>(SkelClass, WidgetFName) : nullptr;
	};
	FObjectProperty* VariableProperty = FindWidgetVarProp();
	if (!VariableProperty)
	{
		if (!Widget->bIsVariable)
		{
			Widget->bIsVariable = true;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		VariableProperty = FindWidgetVarProp();
	}
	if (!VariableProperty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Could not resolve widget variable '%s' on '%s' after promoting it to a variable"), *WidgetName, *WidgetPath);
		return false;
	}

	// 4. Create the component-bound event node (mirrors the UMG designer's "+" button), or reuse an existing one.
	const UK2Node_ComponentBoundEvent* BoundNode =
		FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName());
	if (!BoundNode)
	{
		FKismetEditorUtilities::CreateNewBoundEventForClass(WidgetClass, EventFName, WidgetBP, VariableProperty, /*bShouldJumpToNode=*/false);
		BoundNode = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName());
	}
	if (!BoundNode)
	{
		UE_LOG(LogTemp, Error, TEXT("UWidgetService::BindEvent: Failed to create bound event '%s' for widget '%s' in '%s'"), *EventName, *WidgetName, *WidgetPath);
		return false;
	}

	// 5. Optionally wire the event's exec output to a call to the named handler function.
	//    Best-effort: the bound event node is itself a valid handler, so a missing function is a warning, not a failure.
	if (!FunctionName.IsEmpty())
	{
		UEdGraph* Graph = BoundNode->GetGraph();
		UClass* SkelClass = WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass : WidgetBP->GeneratedClass;
		UFunction* HandlerFunction = SkelClass ? SkelClass->FindFunctionByName(FName(*FunctionName)) : nullptr;
		UEdGraphPin* ThenPin = BoundNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);

		if (!HandlerFunction)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Bound event '%s' created, but handler function '%s' was not found on '%s' — create the function first to auto-wire it. The event node is still available as a handler."),
				*EventName, *FunctionName, *WidgetPath);
		}
		else if (Graph && ThenPin && ThenPin->LinkedTo.Num() == 0)
		{
			// Idempotent: only wire when the event's exec output is still unconnected.
			UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
			CallNode->SetFromFunction(HandlerFunction);
			Graph->AddNode(CallNode, false, false);
			CallNode->CreateNewGuid();
			CallNode->PostPlacedNewNode();
			CallNode->AllocateDefaultPins();
			CallNode->NodePosX = BoundNode->NodePosX + 400;
			CallNode->NodePosY = BoundNode->NodePosY;

			UEdGraphPin* CallExecPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			if (CallExecPin && K2Schema && K2Schema->TryCreateConnection(ThenPin, CallExecPin))
			{
				UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: Wired %s.%s -> %s()"), *WidgetName, *EventName, *FunctionName);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Bound event '%s' created, but could not auto-wire exec to '%s' (exec pin missing or incompatible)"), *EventName, *FunctionName);
			}
		}
	}

	// 6. Persist and verify.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	const bool bBound = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName()) != nullptr;
	UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: %s — Widget: %s, Event: %s -> Function: %s"),
		bBound ? TEXT("BOUND") : TEXT("FAILED"), *WidgetName, *EventName, *FunctionName);
	return bBound;
}

bool UWidgetService::RenameWidget(
	const FString& WidgetPath,
	const FString& OldName,
	const FString& NewName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	if (NewName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: NewName must not be empty"));
		return false;
	}

	// Find the widget by old name
	UWidget* TargetWidget = nullptr;
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == OldName)
		{
			TargetWidget = Widget;
			break;
		}
	}

	if (!TargetWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: Widget '%s' not found in '%s'"), *OldName, *WidgetPath);
		return false;
	}

	// Check for name collision
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget != TargetWidget && Widget->GetName() == NewName)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: A widget named '%s' already exists in '%s'"), *NewName, *WidgetPath);
			return false;
		}
	}

	// Route through the editor's own rename so WidgetAnimation bindings, delegate bindings,
	// desired focus, navigation bindings and graph variable nodes follow the new name
	// (a raw UObject::Rename left animation tracks pointing at the old name, FND-01276).
	if (!FWidgetBlueprintOperationUtils::RenameWidget(WidgetBP, TargetWidget, NewName))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: FWidgetBlueprintOperationUtils::RenameWidget rejected '%s' -> '%s' in '%s' (invalid or non-unique name)"), *OldName, *NewName, *WidgetPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::RenameWidget: Renamed '%s' to '%s' in '%s'"), *OldName, *NewName, *WidgetPath);
	return true;
}

FWidgetReplaceClassResult UWidgetService::ReplaceWidgetClass(
	const FString& WidgetPath,
	const FString& WidgetName,
	const FString& NewClassPath,
	bool bKeepNamedSlotContent)
{
	FWidgetReplaceClassResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	UWidget* TargetWidget = FindWidgetByName(WidgetBP, WidgetName);
	if (!TargetWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget '%s' not found in '%s'"), *WidgetName, *WidgetPath);
		return Result;
	}

	// Full class path only: a bare asset name would hit the same static cache trap as AddComponent.
	if (!NewClassPath.StartsWith(TEXT("/")) || !NewClassPath.Contains(TEXT(".")))
	{
		Result.ErrorMessage = FString::Printf(TEXT("NewClassPath '%s' must be a full class path, e.g. /Game/UI/WBP_X.WBP_X_C or /Script/UMG.Border"), *NewClassPath);
		return Result;
	}

	UClass* NewClass = LoadClass<UWidget>(nullptr, *NewClassPath);
	if (!NewClass)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not load a UWidget class from '%s' (is the target WBP compiled? the path needs the _C suffix)"), *NewClassPath);
		return Result;
	}

	Result.OldClassPath = TargetWidget->GetClass()->GetPathName();
	Result.NewClassPath = NewClass->GetPathName();
	Result.SlotClassBefore = TargetWidget->Slot ? TargetWidget->Slot->GetClass()->GetName() : FString();

	auto DescribeNamedSlots = [](UWidget* Widget, TArray<FString>& Out, TArray<FName>* OutFilledSlots)
	{
		const INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Widget);
		if (!NamedSlotHost)
		{
			return;
		}
		TArray<FName> SlotNames;
		NamedSlotHost->GetSlotNames(SlotNames);
		for (const FName SlotName : SlotNames)
		{
			const UWidget* Content = NamedSlotHost->GetContentForSlot(SlotName);
			Out.Add(FString::Printf(TEXT("%s=%s"), *SlotName.ToString(), Content ? *Content->GetName() : TEXT("<empty>")));
			if (Content && OutFilledSlots)
			{
				OutFilledSlots->Add(SlotName);
			}
		}
	};

	TArray<FName> FilledSlotsBefore;
	DescribeNamedSlots(TargetWidget, Result.NamedSlotsBefore, &FilledSlotsBefore);

	// ReplaceWidgetWithTemplate deletes the old host and, with it, every widget parked in its
	// NamedSlots (measured: a duplicated page lost its WBP_HalftoneMenu tile content). Detach the
	// content first (SetContentForSlot(null) only drops the binding, it does not delete the
	// widget), then re-attach it to the replacement host by slot name. The replace recompiles the
	// blueprint, and the compiler trashes every widget not reachable from the root, so the
	// detached content is parked under the root panel for the duration of the replace.
	TArray<TPair<FName, UWidget*>> DetachedContent;
	UPanelWidget* ParkingPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
	if (bKeepNamedSlotContent && FilledSlotsBefore.Num() > 0)
	{
		INamedSlotInterface* HostBefore = Cast<INamedSlotInterface>(TargetWidget);
		if (HostBefore && !ParkingPanel)
		{
			Result.ErrorMessage = TEXT("Cannot keep NamedSlot content: the blueprint root is not a panel to park it under. Pass bKeepNamedSlotContent=false to accept losing it.");
			return Result;
		}
		if (HostBefore)
		{
			WidgetBP->WidgetTree->Modify();
			TargetWidget->Modify();
			ParkingPanel->Modify();
			for (const FName SlotName : FilledSlotsBefore)
			{
				UWidget* Content = HostBefore->GetContentForSlot(SlotName);
				DetachedContent.Emplace(SlotName, Content);
				HostBefore->SetContentForSlot(SlotName, nullptr);
				Content->Modify();
				ParkingPanel->AddChild(Content);
			}
		}
	}

	FText ErrorText;
	if (!FWidgetBlueprintOperationUtils::ReplaceWidgetWithTemplate(WidgetBP, TargetWidget, NewClass, ErrorText))
	{
		// Put the content back on the (still-present) old host.
		if (INamedSlotInterface* HostBefore = Cast<INamedSlotInterface>(TargetWidget))
		{
			for (const TPair<FName, UWidget*>& Entry : DetachedContent)
			{
				Entry.Value->RemoveFromParent();
				HostBefore->SetContentForSlot(Entry.Key, Entry.Value);
			}
		}
		Result.ErrorMessage = ErrorText.ToString();
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::ReplaceWidgetClass: '%s' in '%s' -> '%s' failed: %s"), *WidgetName, *WidgetPath, *NewClassPath, *Result.ErrorMessage);
		return Result;
	}

	// The widget object is new; re-resolve it by (preserved) name.
	UWidget* NewWidget = FindWidgetByName(WidgetBP, WidgetName);
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Replace reported success but '%s' is no longer in the tree"), *WidgetName);
		return Result;
	}

	Result.NewClassPath = NewWidget->GetClass()->GetPathName();
	Result.SlotClassAfter = NewWidget->Slot ? NewWidget->Slot->GetClass()->GetName() : FString();

	INamedSlotInterface* HostAfter = Cast<INamedSlotInterface>(NewWidget);
	if (HostAfter)
	{
		TArray<FName> NewSlotNames;
		HostAfter->GetSlotNames(NewSlotNames);
		for (const TPair<FName, UWidget*>& Entry : DetachedContent)
		{
			Entry.Value->RemoveFromParent();
			// Only slots that exist on the new class can take the content back.
			if (NewSlotNames.Contains(Entry.Key))
			{
				HostAfter->SetContentForSlot(Entry.Key, Entry.Value);
			}
		}
	}
	else
	{
		for (const TPair<FName, UWidget*>& Entry : DetachedContent)
		{
			Entry.Value->RemoveFromParent();
		}
	}
	DescribeNamedSlots(NewWidget, Result.NamedSlotsAfter, nullptr);

	bool bSurvived = true;
	for (const FName SlotName : FilledSlotsBefore)
	{
		const UWidget* After = HostAfter ? HostAfter->GetContentForSlot(SlotName) : nullptr;
		if (!After)
		{
			bSurvived = false;
		}
	}
	Result.bNamedSlotContentSurvived = bSurvived;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UWidgetService::ReplaceWidgetClass: '%s' in '%s': %s -> %s (NamedSlots survived: %s)"),
		*WidgetName, *WidgetPath, *Result.OldClassPath, *Result.NewClassPath, bSurvived ? TEXT("yes") : TEXT("no"));
	return Result;
}

// =================================================================
// ViewModel Helper Methods
// =================================================================

// Helper to find an existing MVVM extension on a WidgetBlueprint (without creating one)
static UMVVMWidgetBlueprintExtension_View* FindMVVMExtension(UWidgetBlueprint* WidgetBP)
{
	if (!WidgetBP)
	{
		return nullptr;
	}

	for (UBlueprintExtension* Extension : WidgetBP->GetExtensions())
	{
		if (UMVVMWidgetBlueprintExtension_View* ViewExt = Cast<UMVVMWidgetBlueprintExtension_View>(Extension))
		{
			return ViewExt;
		}
	}
	return nullptr;
}

UClass* UWidgetService::FindViewModelClass(const FString& ClassName)
{
	// Normalize: strip trailing _C suffix if present (Blueprint class names end with _C)
	FString NormalizedName = ClassName;
	if (NormalizedName.EndsWith(TEXT("_C")))
	{
		NormalizedName = NormalizedName.LeftChop(2);
	}

	// 1. Try direct class lookup (C++ and loaded Blueprint ViewModel classes)
	// Build candidate names to match against UClass::GetName()
	FString NameWithU = NormalizedName;
	if (!NameWithU.StartsWith(TEXT("U")))
	{
		NameWithU = TEXT("U") + NormalizedName;
	}
	// Blueprint-generated classes have _C suffix
	FString NameWithC = NormalizedName + TEXT("_C");

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		const FString& TestName = TestClass->GetName();
		if (TestName == NormalizedName || TestName == NameWithU || TestName == NameWithC)
		{
			// Accept classes that inherit from UMVVMViewModelBase
			if (TestClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
			{
				return TestClass;
			}
		}
	}

	// 2. Try Asset Registry for dedicated MVVMViewModelBlueprint assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/ModelViewViewModelBlueprint.MVVMViewModelBlueprint")));
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			if (AssetName.Equals(NormalizedName, ESearchCase::IgnoreCase))
			{
				if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
				{
					if (BP->GeneratedClass)
					{
						return BP->GeneratedClass;
					}
				}
			}
		}
	}

	// 3. Try Asset Registry for regular Blueprint assets whose parent is a ViewModel class
	// This handles Blueprints created via BlueprintService.create_blueprint with MVVMViewModelBase parent
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.Blueprint")));
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			if (AssetName.Equals(NormalizedName, ESearchCase::IgnoreCase))
			{
				if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
				{
					if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
					{
						return BP->GeneratedClass;
					}
				}
			}
		}
	}

	// 4. Try by full path (if user provides asset path like /Game/UI/Tests/VM_TestHUD)
	FString PathToLoad = NormalizedName;
	if (PathToLoad.StartsWith(TEXT("/")))
	{
		// Strip .ClassName suffix if present (e.g., /Game/UI/Tests/VM_TestHUD.VM_TestHUD)
		int32 DotIndex;
		if (PathToLoad.FindLastChar(TEXT('.'), DotIndex))
		{
			PathToLoad = PathToLoad.Left(DotIndex);
		}

		UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(PathToLoad);
		if (UBlueprint* BP = Cast<UBlueprint>(LoadedObj))
		{
			if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
			{
				return BP->GeneratedClass;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UWidgetService::FindViewModelClass: Class '%s' not found. "
		"Tried: C++ classes ('%s', '%s', '%s'), MVVMViewModelBlueprint assets, regular Blueprint assets, "
		"and full path loading. Ensure the ViewModel inherits from MVVMViewModelBase and is compiled."),
		*ClassName, *NormalizedName, *NameWithU, *NameWithC);
	return nullptr;
}

UMVVMBlueprintView* UWidgetService::GetOrCreateMVVMView(UWidgetBlueprint* WidgetBP)
{
	if (!WidgetBP)
	{
		return nullptr;
	}

	// Try to find existing MVVM extension
	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);

	// Create extension if not found
	if (!ViewExtension)
	{
		ViewExtension = NewObject<UMVVMWidgetBlueprintExtension_View>(WidgetBP);
		if (!ViewExtension)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService: Failed to create MVVM extension"));
			return nullptr;
		}
		WidgetBP->AddExtension(ViewExtension);
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		ViewExtension->CreateBlueprintViewInstance();
		View = ViewExtension->GetBlueprintView();
	}

	return View;
}

FString UWidgetService::BindingModeToString(EMVVMBindingMode Mode)
{
	switch (Mode)
	{
	case EMVVMBindingMode::OneTimeToDestination: return TEXT("OneTimeToDestination");
	case EMVVMBindingMode::OneWayToDestination: return TEXT("OneWayToDestination");
	case EMVVMBindingMode::TwoWay: return TEXT("TwoWay");
	case EMVVMBindingMode::OneTimeToSource: return TEXT("OneTimeToSource");
	case EMVVMBindingMode::OneWayToSource: return TEXT("OneWayToSource");
	default: return TEXT("Unknown");
	}
}

EMVVMBindingMode UWidgetService::StringToBindingMode(const FString& ModeString)
{
	if (ModeString.Equals(TEXT("OneTimeToDestination"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneTimeToDestination;
	if (ModeString.Equals(TEXT("OneWayToDestination"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneWayToDestination;
	if (ModeString.Equals(TEXT("TwoWay"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::TwoWay;
	if (ModeString.Equals(TEXT("OneTimeToSource"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneTimeToSource;
	if (ModeString.Equals(TEXT("OneWayToSource"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneWayToSource;

	// Default
	return EMVVMBindingMode::OneWayToDestination;
}

// =================================================================
// ViewModel Management (MVVM)
// =================================================================

TArray<FWidgetViewModelInfo> UWidgetService::ListViewModels(const FString& WidgetPath)
{
	TArray<FWidgetViewModelInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	// Get the MVVM extension (don't create if not present)
	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		return Results;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		return Results;
	}

	for (const FMVVMBlueprintViewModelContext& VMContext : View->GetViewModels())
	{
		FWidgetViewModelInfo Info;
		Info.ViewModelName = VMContext.GetViewModelName().ToString();
		Info.ViewModelId = VMContext.GetViewModelId().ToString();

		if (UClass* VMClass = VMContext.GetViewModelClass())
		{
			Info.ViewModelClassName = VMClass->GetName();
		}

		switch (VMContext.CreationType)
		{
		case EMVVMBlueprintViewModelContextCreationType::Manual:
			Info.CreationType = TEXT("Manual");
			break;
		case EMVVMBlueprintViewModelContextCreationType::CreateInstance:
			Info.CreationType = TEXT("CreateInstance");
			break;
		case EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection:
			Info.CreationType = TEXT("GlobalViewModelCollection");
			break;
		case EMVVMBlueprintViewModelContextCreationType::PropertyPath:
			Info.CreationType = TEXT("PropertyPath");
			break;
		case EMVVMBlueprintViewModelContextCreationType::Resolver:
			Info.CreationType = TEXT("Resolver");
			break;
		}

		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::AddViewModel(
	const FString& WidgetPath,
	const FString& ViewModelClassName,
	const FString& ViewModelName,
	const FString& CreationType)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	// Find the ViewModel class
	UClass* VMClass = FindViewModelClass(ViewModelClassName);
	if (!VMClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: ViewModel class '%s' not found"), *ViewModelClassName);
		return false;
	}

	// Get or create the MVVM view
	UMVVMBlueprintView* View = GetOrCreateMVVMView(WidgetBP);
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: Failed to get/create MVVM view"));
		return false;
	}

	// Check if a ViewModel with this name already exists
	if (View->FindViewModel(FName(*ViewModelName)) != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: ViewModel '%s' already exists"), *ViewModelName);
		return false;
	}

	// Create the ViewModel context
	FMVVMBlueprintViewModelContext NewContext(VMClass, FName(*ViewModelName));

	// Set creation type
	if (CreationType.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::Manual;
	}
	else if (CreationType.Equals(TEXT("CreateInstance"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
	}
	else if (CreationType.Equals(TEXT("GlobalViewModelCollection"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection;
	}
	else if (CreationType.Equals(TEXT("PropertyPath"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::PropertyPath;
	}
	else if (CreationType.Equals(TEXT("Resolver"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::Resolver;
	}
	else
	{
		// Default to CreateInstance
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
	}

	View->AddViewModel(NewContext);

	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::AddViewModel: Added ViewModel '%s' (class: %s) to '%s'"),
		*ViewModelName, *ViewModelClassName, *WidgetPath);

	return true;
}

bool UWidgetService::RemoveViewModel(
	const FString& WidgetPath,
	const FString& ViewModelName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: No MVVM extension found"));
		return false;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: No MVVM view found"));
		return false;
	}

	// Find the ViewModel by name
	const FMVVMBlueprintViewModelContext* VMContext = View->FindViewModel(FName(*ViewModelName));
	if (!VMContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: ViewModel '%s' not found"), *ViewModelName);
		return false;
	}

	FGuid ViewModelId = VMContext->GetViewModelId();
	bool bRemoved = View->RemoveViewModel(ViewModelId);

	if (bRemoved)
	{
		WidgetBP->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		UE_LOG(LogTemp, Log, TEXT("UWidgetService::RemoveViewModel: Removed ViewModel '%s' from '%s'"),
			*ViewModelName, *WidgetPath);
	}

	return bRemoved;
}

TArray<FWidgetViewModelBindingInfo> UWidgetService::ListViewModelBindings(const FString& WidgetPath)
{
	TArray<FWidgetViewModelBindingInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		return Results;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		return Results;
	}

	UClass* SelfContext = WidgetBP->GeneratedClass;

	for (int32 i = 0; i < View->GetNumBindings(); ++i)
	{
		FMVVMBlueprintViewBinding* Binding = View->GetBindingAt(i);
		if (!Binding)
		{
			continue;
		}

		FWidgetViewModelBindingInfo Info;
		Info.BindingIndex = i;
		Info.BindingMode = BindingModeToString(Binding->BindingType);
		Info.bEnabled = Binding->bEnabled;
		Info.BindingId = Binding->BindingId.ToString();

		// Get display strings for source and destination paths
		Info.SourcePath = Binding->SourcePath.GetPropertyPath(SelfContext);
		Info.DestinationPath = Binding->DestinationPath.GetPropertyPath(SelfContext);

		// If property path is empty, try display text
		if (Info.SourcePath.IsEmpty())
		{
			Info.SourcePath = Binding->SourcePath.ToText(WidgetBP, false).ToString();
		}
		if (Info.DestinationPath.IsEmpty())
		{
			Info.DestinationPath = Binding->DestinationPath.ToText(WidgetBP, false).ToString();
		}

		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::AddViewModelBinding(
	const FString& WidgetPath,
	const FString& ViewModelName,
	const FString& ViewModelProperty,
	const FString& WidgetName,
	const FString& WidgetProperty,
	const FString& BindingMode)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMBlueprintView* View = GetOrCreateMVVMView(WidgetBP);
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Failed to get/create MVVM view"));
		return false;
	}

	// Find the ViewModel context
	const FMVVMBlueprintViewModelContext* VMContext = View->FindViewModel(FName(*ViewModelName));
	if (!VMContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: ViewModel '%s' not found. Add it first with add_view_model."), *ViewModelName);
		return false;
	}

	UClass* VMClass = VMContext->GetViewModelClass();
	if (!VMClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: ViewModel class is invalid"));
		return false;
	}

	// Find the ViewModel property
	FProperty* VMProp = VMClass->FindPropertyByName(FName(*ViewModelProperty));
	if (!VMProp)
	{
		// Try case-insensitive
		for (TFieldIterator<FProperty> It(VMClass); It; ++It)
		{
			if (It->GetName().Equals(ViewModelProperty, ESearchCase::IgnoreCase))
			{
				VMProp = *It;
				break;
			}
		}
	}
	if (!VMProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Property '%s' not found on ViewModel class '%s'"),
			*ViewModelProperty, *VMClass->GetName());
		return false;
	}

	// Find the widget and its property
	UWidget* TargetWidget = FindWidgetByName(WidgetBP, WidgetName);
	if (!TargetWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Widget '%s' not found"), *WidgetName);
		return false;
	}

	FProperty* WidgetProp = TargetWidget->GetClass()->FindPropertyByName(FName(*WidgetProperty));
	if (!WidgetProp)
	{
		// Try case-insensitive
		for (TFieldIterator<FProperty> It(TargetWidget->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(WidgetProperty, ESearchCase::IgnoreCase))
			{
				WidgetProp = *It;
				break;
			}
		}
	}
	if (!WidgetProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Property '%s' not found on widget '%s'"),
			*WidgetProperty, *WidgetName);
		return false;
	}

	// Create a new binding
	FMVVMBlueprintViewBinding& NewBinding = View->AddDefaultBinding();

	// Set binding mode
	NewBinding.BindingType = StringToBindingMode(BindingMode);

	// Set up source path (ViewModel property)
	NewBinding.SourcePath.SetViewModelId(VMContext->GetViewModelId());
	UE::MVVM::FMVVMConstFieldVariant SourceField(VMProp);
	NewBinding.SourcePath.SetPropertyPath(WidgetBP, SourceField);

	// Set up destination path (widget property)
	NewBinding.DestinationPath.SetWidgetName(FName(*WidgetName));
	UE::MVVM::FMVVMConstFieldVariant DestField(WidgetProp);
	NewBinding.DestinationPath.SetPropertyPath(WidgetBP, DestField);

	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::AddViewModelBinding: Created binding %s.%s -> %s.%s (%s)"),
		*ViewModelName, *ViewModelProperty, *WidgetName, *WidgetProperty, *BindingMode);

	return true;
}

bool UWidgetService::RemoveViewModelBinding(
	const FString& WidgetPath,
	int32 BindingIndex)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: No MVVM extension found"));
		return false;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: No MVVM view found"));
		return false;
	}

	if (BindingIndex < 0 || BindingIndex >= View->GetNumBindings())
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: Index %d out of range (0-%d)"),
			BindingIndex, View->GetNumBindings() - 1);
		return false;
	}

	View->RemoveBindingAt(BindingIndex);

	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::RemoveViewModelBinding: Removed binding at index %d"), BindingIndex);

	return true;
}

// =================================================================
// Existence Checks
// =================================================================

bool UWidgetService::WidgetBlueprintExists(const FString& WidgetPath)
{
	if (WidgetPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(WidgetPath);
}

bool UWidgetService::WidgetExists(const FString& WidgetPath, const FString& ComponentName)
{
	if (WidgetPath.IsEmpty() || ComponentName.IsEmpty())
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	return FindWidgetByName(WidgetBP, ComponentName) != nullptr;
}

// =================================================================
// Snapshot API
// =================================================================

FWidgetSlotInfo UWidgetService::BuildSlotInfo(UWidget* Widget)
{
	FWidgetSlotInfo SlotInfo;

	if (!Widget || !Widget->Slot)
	{
		SlotInfo.SlotType = TEXT("None");
		return SlotInfo;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("Canvas");
		FAnchorData Layout = CanvasSlot->GetLayout();
		SlotInfo.AnchorMin = Layout.Anchors.Minimum;
		SlotInfo.AnchorMax = Layout.Anchors.Maximum;
		SlotInfo.Offsets = Layout.Offsets;
		SlotInfo.Alignment = Layout.Alignment;
		SlotInfo.ZOrder = CanvasSlot->GetZOrder();
		SlotInfo.bAutoSize = CanvasSlot->GetAutoSize();
	}
	else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("VerticalBox");
		FSlateChildSize Size = VBoxSlot->GetSize();
		SlotInfo.SizeRule = (Size.SizeRule == ESlateSizeRule::Fill) ? TEXT("Fill") : TEXT("Automatic");
		SlotInfo.SizeValue = Size.Value;
		SlotInfo.Padding = VBoxSlot->GetPadding();
		SlotInfo.HorizontalAlignment = VBoxSlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = VBoxSlot->GetVerticalAlignment();
	}
	else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("HorizontalBox");
		FSlateChildSize Size = HBoxSlot->GetSize();
		SlotInfo.SizeRule = (Size.SizeRule == ESlateSizeRule::Fill) ? TEXT("Fill") : TEXT("Automatic");
		SlotInfo.SizeValue = Size.Value;
		SlotInfo.Padding = HBoxSlot->GetPadding();
		SlotInfo.HorizontalAlignment = HBoxSlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = HBoxSlot->GetVerticalAlignment();
	}
	else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("Overlay");
		SlotInfo.Padding = OverlaySlot->GetPadding();
		SlotInfo.HorizontalAlignment = OverlaySlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = OverlaySlot->GetVerticalAlignment();
	}
	else
	{
		// Unknown slot type — store class name so caller knows what it is
		SlotInfo.SlotType = Widget->Slot->GetClass()->GetName();
	}

	return SlotInfo;
}

TArray<FWidgetPropertyInfo> UWidgetService::CollectWidgetProperties(UWidget* Widget)
{
	TArray<FWidgetPropertyInfo> Properties;

	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		FWidgetPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		Info.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
		Info.Category = Property->GetMetaData(TEXT("Category"));

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
		Property->ExportTextItem_Direct(Info.CurrentValue, ValuePtr, nullptr, Widget, PPF_None);

		Properties.Add(Info);
	}

	return Properties;
}

TArray<FWidgetComponentSnapshot> UWidgetService::GetWidgetSnapshot(const FString& WidgetPath)
{
	TArray<FWidgetComponentSnapshot> Snapshots;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Snapshots;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	if (!RootWidget)
	{
		return Snapshots;
	}

	TFunction<void(UWidget*, const FString&)> BuildSnapshot;
	BuildSnapshot = [&](UWidget* Widget, const FString& ParentName)
	{
		if (!Widget)
		{
			return;
		}

		FWidgetComponentSnapshot Snapshot;
		Snapshot.WidgetName = Widget->GetName();
		Snapshot.WidgetClass = Widget->GetClass()->GetName();
		Snapshot.WidgetClassPath = Widget->GetClass()->GetPathName();
		Snapshot.ParentWidget = ParentName;
		Snapshot.bIsRootWidget = (Widget == RootWidget);
		Snapshot.bIsVariable = Widget->bIsVariable;
		Snapshot.SlotInfo = BuildSlotInfo(Widget);
		Snapshot.Properties = CollectWidgetProperties(Widget);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = Panel->GetChildAt(i))
				{
					Snapshot.Children.Add(Child->GetName());
				}
			}
		}

		Snapshots.Add(Snapshot);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				BuildSnapshot(Panel->GetChildAt(i), Snapshot.WidgetName);
			}
		}
	};

	BuildSnapshot(RootWidget, FString());

	return Snapshots;
}

FWidgetComponentSnapshot UWidgetService::GetComponentSnapshot(const FString& WidgetPath, const FString& ComponentName)
{
	FWidgetComponentSnapshot Snapshot;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Snapshot;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Snapshot;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	Snapshot.WidgetName = Widget->GetName();
	Snapshot.WidgetClass = Widget->GetClass()->GetName();
	Snapshot.WidgetClassPath = Widget->GetClass()->GetPathName();
	Snapshot.bIsRootWidget = (Widget == RootWidget);
	Snapshot.bIsVariable = Widget->bIsVariable;
	Snapshot.SlotInfo = BuildSlotInfo(Widget);
	Snapshot.Properties = CollectWidgetProperties(Widget);

	if (Widget->Slot && Widget->Slot->Parent)
	{
		Snapshot.ParentWidget = Widget->Slot->Parent->GetName();
	}

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				Snapshot.Children.Add(Child->GetName());
			}
		}
	}

	return Snapshot;
}