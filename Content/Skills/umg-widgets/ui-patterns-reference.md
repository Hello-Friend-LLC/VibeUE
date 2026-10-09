---
name: umg-widgets/ui-patterns-reference
description: Project-neutral reference implementations of the UI-material AngelScript patterns (material-write helper, pure rules + unit test, animated button base with rest-pose settle, slot base, fill base, grid base with CommonButtonGroup) — copy and rename the prefix; needs no Epic UIMaterialLab pack. Read before writing any AS widget base that drives a UI material.
---

# UI pattern reference (copy, then rename `UiMat` to your project prefix)

Proven on UE 5.8 Hazelight in the SpaceWasteCollector Applications-page port (WI-1548). The source pack
(Epic UIMaterialLab) is NOT required: every pattern below stands alone. Process: `screen-process.md`; materials and
animations: `ui-materials.md`; base-class rules: `script-base-first.md`. Verify any signature against the generated
AS API dump / compiler in your project before relying on it (a dump miss proves nothing).

## Contract every base follows

1. **Animation is the only writer of a state parameter.** The WBP animation keys the material scalar
   (`Hovered`, `Is Active`); script only plays it. Script writes only script-owned values (fill amount, item
   texture), never a parameter an animation also keys.
2. **One scalar per state**; look = a UI-domain material instance, not a new master.
3. **Uncached MID writes**: re-fetch `Image.GetDynamicMaterial()` on every write; the MID goes stale after
   `SetBrushFromMaterial` or an animation restore.
4. **Pure rules in a namespace** (no UObjects), unit-tested world-free; everything else is thin glue.
5. **UCLASS(Abstract) base, WBP holds tree/art/animations only**: zero EventGraph nodes, zero Blueprint variables.
   Animations bind through `UPROPERTY(BindWidgetAnim, Transient)`; the optional form is
   `UPROPERTY(Transient, meta = (BindWidgetAnimOptional))` (the bare keyword is rejected). The animation FName must
   equal the property name or it never binds.
6. **Buttons derive `UCommonButtonBase`** (CommonUI owns hover/press/selected; no separate `UButton` hitbox). A
   `UCommonButtonBase` child with `Style` None draws a default grey box: assign a `CommonButtonStyle` with
   NoDrawType on the class default AND on every existing WBP instance (CDO value does not reach instances).
7. **Rest pose**: a hover animation's frame 0 is the rest state, not the material instance default. Settle it at
   Construct (below), or an instance whose default is the hovered value rests hovered.

## Material write helper (script-owned values only)

```angelscript
namespace UiMatMaterial
{
	void SetScalar(UImage Image, FName Param, float Value)
	{
		if (!IsValid(Image))
			return;
		UMaterialInstanceDynamic MID = Image.GetDynamicMaterial(); // never cache the MID
		if (IsValid(MID))
			MID.SetScalarParameterValue(Param, Value);
	}
	// SetVector / SetTexture are the same shape with SetVectorParameterValue / SetTextureParameterValue.
}
```

## Pure rules + unit test

```angelscript
namespace UiMatRules
{
	float ClampFill(float Fill) { return Math::Clamp(Fill, 0.0, 1.0); }

	// Lit segments of a segmented bar: round DOWN (a segment lights only once fully reached); the bias absorbs
	// float noise (0.6 * 5 = 2.9999999); NumSteps <= 0 yields 0; never exceeds NumSteps.
	int QuantizeSegments(float Fill01, int NumSteps)
	{
		if (NumSteps <= 0)
			return 0;
		const int Lit = Math::FloorToInt(ClampFill(Fill01) * NumSteps + 0.0001);
		return Math::Clamp(Lit, 0, NumSteps);
	}
}

void Test_UiMatQuantize(FUnitTest& T)
{
	T.AssertEquals(5, UiMatRules::QuantizeSegments(0.5f, 10), "half lights half");
	T.AssertEquals(3, UiMatRules::QuantizeSegments(0.6f, 5), "float noise still lands on 3");
	T.AssertEquals(0, UiMatRules::QuantizeSegments(0.5f, 0), "zero steps yields 0");
}
```

Keep tests cheap and world-free: hot reload runs every unit test of the recompiled module.

## Animated button base (hover animation + rest-pose settle)

```angelscript
UCLASS(Abstract)
class UUiMatButtonBase : UCommonButtonBase
{
	UPROPERTY(BindWidget) UImage Background;            // brush image whose material the animation keys
	UPROPERTY(BindWidgetOptional) UTextBlock Label;
	UPROPERTY(EditAnywhere) FText ButtonText;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation Anim_Hover;

	UFUNCTION(BlueprintOverride) void PreConstruct(bool IsDesignTime) { ApplyButtonText(); }

	// Rest state = Anim_Hover frame 0. Play it in reverse from its end at a high speed so it lands on
	// frame 0 in the first tick (no visible hover-out tween at spawn).
	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		if (IsValid(Anim_Hover))
			PlayAnimation(Anim_Hover, Anim_Hover.GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 100.0);
	}

	UFUNCTION(BlueprintOverride) void OnHovered()   { if (IsValid(Anim_Hover)) PlayAnimationForward(Anim_Hover); }
	UFUNCTION(BlueprintOverride) void OnUnhovered() { if (IsValid(Anim_Hover)) PlayAnimationReverse(Anim_Hover); }

	private void ApplyButtonText()
	{
		if (IsValid(Label) && !ButtonText.IsEmpty())
			Label.SetText(ButtonText);
	}
}
```

## Slot base (selected = equipped, second animation)

Derive from the button base; `OnInitialized` → `SetIsSelectable(true); SetIsToggleable(true);`.
`OnSelected` → `PlayAnimationForward(Anim_Equip)`, `OnDeselected` → `PlayAnimationReverse(Anim_Equip)`
(`Anim_Equip` is another `BindWidgetAnimOptional`). A per-slot look comes from an `EditAnywhere
UMaterialInterface SlotMaterial` applied with `Background.SetBrushFromMaterial` in `PreConstruct` (an instance,
not a new master). Item art is script-owned data: `UiMatMaterial::SetTexture(Background, n"ItemTexture", Tex)`.
The hook names are `OnHovered/OnUnhovered/OnSelected/OnDeselected` (the `BP_`-prefixed names do not exist).

## Fill widget base (one scalar, parameter chosen per instance)

```angelscript
UCLASS(Abstract)
class UUiMatFillBase : UUserWidget
{
	UPROPERTY(BindWidget) UImage FillImage;
	UPROPERTY(EditAnywhere) FName FillParamName = n"Fill amount"; // the master's fill scalar
	UPROPERTY(EditAnywhere) UMaterialInterface FillMaterial;      // per-instance brush material
	UPROPERTY(EditAnywhere) float InitialFill = 0.5;
	private float Fill = 0.0;

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		if (IsValid(FillMaterial) && IsValid(FillImage))
			FillImage.SetBrushFromMaterial(FillMaterial);
		SetFill(InitialFill);
	}

	UFUNCTION() void SetFill(float NewFill)
	{
		Fill = UiMatRules::ClampFill(NewFill);
		UiMatMaterial::SetScalar(FillImage, FillParamName, Fill);
	}
}
```

A per-instance brush needs `FillMaterial` as a property: widget tooling cannot address a nested child image per
instance, so do not wrap the fill in a one-off child WBP to vary its brush.

## Grid base with exclusive selection (`UCommonButtonGroupBase`)

```angelscript
UCLASS(Abstract)
class UUiMatGridBase : UUserWidget
{
	UPROPERTY(BindWidget) UUniformGridPanel Grid;   // rename the tree widget to this and mark IsVariable
	UPROPERTY(Transient) UCommonButtonGroupBase Group;

	UFUNCTION(BlueprintOverride)
	void OnInitialized()
	{
		Group = Cast<UCommonButtonGroupBase>(NewObject(this, UCommonButtonGroupBase));
		if (!IsValid(Group) || !IsValid(Grid))
			return;
		Group.SetSelectionRequired(false);
		for (UWidget Child : Grid.GetAllChildren())
		{
			UUiMatSlotBase SlotButton = Cast<UUiMatSlotBase>(Child);
			if (IsValid(SlotButton))
				Group.AddWidget(SlotButton);
		}
		Group.OnSelectedButtonBaseChanged.AddUFunction(this, n"HandleGroupSelectionChanged");
	}

	UFUNCTION() private void HandleGroupSelectionChanged(UCommonButtonBase Button, int ButtonIndex)
	{
		OnSlotSelected(ButtonIndex);
	}
	UFUNCTION(BlueprintEvent) void OnSlotSelected(int Index) {}
}
```

## Verify what compiling cannot

Compile and lineage checks passed while three real defects (grey button box, wrong rest pose, oversized fill)
only showed in the PIE parity ladder (`screen-process.md` step 5). Capture the pair in the same game-thread tick
(animated regions differ otherwise), and treat an exact 0.00 diff as proof only after confirming the two images
come from two different assets.
