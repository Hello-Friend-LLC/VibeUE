---
name: umg-widgets/script-base-first
description: Base-class-first UMG authoring — write the abstract AngelScript (or C++) UUserWidget base with BindWidget members and all behaviour first, then create the Widget Blueprint with that class as its parent and give it only the tree, art and animations. Covers the base-class template, BindWidget rules, creating/reparenting the WBP from Python, ListView entry rows, hot-reload traps, and the verification checklist.
---

# Base class first, Widget Blueprint second

A Widget Blueprint that carries gameplay logic in its EventGraph is an anti-pattern on this stack:
graphs are not text-diffable, not unit-testable, and `add_widget`/graph surgery through MCP is the
most crash-prone editor path. The working pattern (measured on every shipped widget in
SpaceWasteCollector, WI-1542 / WI-1533) is:

1. **Base class first.** An `UCLASS(Abstract)` AngelScript class deriving from `UUserWidget` (or
   `UCommonButtonBase`, `UCommonActivatableWidget`, ...) declares every tree element it needs as a
   `UPROPERTY(BindWidget)` member and owns ALL behaviour: data reads, refresh, state machine,
   input, logging. Pure rules live in a namespace and get unit tests.
2. **WBP second.** The Widget Blueprint is created with that class as `parent_class`. It holds only
   the widget tree (names matching the BindWidget members), slots/anchors, fonts, brushes and
   animations. No EventGraph logic, no Blueprint variables.
3. Designer-facing knobs are `UPROPERTY(EditDefaultsOnly)` on the base; animations are either bound as
   `BindWidgetAnim` members the base plays itself (preferred when the WBP animation keys a material
   scalar, section 1b / `ui-materials.md`) or triggered through `UFUNCTION(BlueprintEvent) BP_*` hooks the
   WBP may implement.

Skip the base class only for a purely decorative widget with no data and no input.

## 1. Base-class template (AngelScript, Hazelight fork)

```angelscript
// Script/UI/MyPanelWidget.as -- WBP_MyPanel only holds widgets with these names and sets this class
// as its parent; every behaviour lives here.
UCLASS(Abstract)
class UMyPanelWidget : UUserWidget
{
	// Required tree elements: the WBP compile FAILS if a name is missing or the type is wrong.
	UPROPERTY(BindWidget)
	UTextBlock TitleText;

	UPROPERTY(BindWidget)
	UProgressBar ValueBar;

	// A child WBP parented to an AS/C++ class binds through its base type (WBP_MyButton_C -> UCommonButtonBase).
	UPROPERTY(BindWidget)
	UCommonButtonBase ActionButton;

	// Optional elements: null when the WBP omits them, so guard every use with IsValid().
	UPROPERTY(BindWidgetOptional)
	UWidget FocusOutline;

	// Designer knobs live on the class, never as Blueprint variables.
	UPROPERTY(EditDefaultsOnly, Category = "My Panel")
	FString TitleString = "MY PANEL";

	UFUNCTION(BlueprintOverride)
	void PreConstruct(bool IsDesignTime)
	{
		// Runs in the designer too: keep it side-effect free (text, visibility, sizes).
		if (IsValid(TitleText))
			TitleText.SetText(FText::FromString(TitleString));
	}

	UFUNCTION(BlueprintOverride)
	void OnInitialized()
	{
		if (IsValid(ActionButton))
			ActionButton.OnButtonBaseClicked.AddUFunction(this, n"HandleActionClicked");
		// No game instance in the designer / CapturePreview: guard subsystem access.
		if (IsValid(Gameplay::GetGameInstance()))
		{
			// UGameplayMessageSubsystem::Get().RegisterListener(...)
		}
	}

	UFUNCTION()
	private void HandleActionClicked(UCommonButtonBase Button)
	{
		// ...
	}

	// Animation / FX hook: the WBP may override it to play a widget animation; the base never
	// references the animation asset.
	UFUNCTION(BlueprintEvent)
	void BP_PlayShowAnimation()
	{}
}
```

Conventions that make the WBP step mechanical:

- `UCLASS(Abstract)` so the script class itself can never be spawned; only a WBP child can.
- Every reference the class needs is a `BindWidget`/`BindWidgetOptional` member. Never fetch
  tree widgets by name at runtime.
- `BindWidget` member types may be a base type (`UWidget`, `UCommonButtonBase`, `UPanelWidget`) so
  the WBP can choose the concrete widget or a nested WBP.
- Script-built sub-trees (`NewObject` + `AddChild` into a bound panel) are the fallback when the
  WBP tree cannot be edited safely; keep them `UPROPERTY(Transient)`.
- Logging goes through the project log library (`HF_Log` on HF projects), not `Print`.
- A `UPROPERTY(BindWidget)` / `UFUNCTION` change is a **Full Reload**: stop PIE first, then save
  the `.as` file; body-only edits soft-reload live (see the project's AngelScript hot-reload rules).

### 1b. CommonButtonBase variant (hover/press/selected + animation members)

A button, tile or slot that reacts to the pointer derives `UCommonButtonBase` (CommonButton owns
hover/press/selected; no `UButton` hitbox over an `Image`). Hover/equip tweens are WBP animations that
key a material scalar; the base only plays them. Full material rules: `ui-materials.md`.

```angelscript
UCLASS(Abstract)
class UMatLab_MaterialButtonBase : UCommonButtonBase
{
	UPROPERTY(BindWidget)
	UImage Background;                       // the brush image the animation keys

	UPROPERTY(BindWidgetOptional)
	UTextBlock Label;

	// Animation members: the WBP animation's FName MUST equal the property name.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))   // optional form; NOT a plain BindWidgetAnimOptional specifier
	UWidgetAnimation Anim_Hover;
	// required form: UPROPERTY(BindWidgetAnim, Transient) UWidgetAnimation Anim_Equip;

	// Rest pose = animation frame 0, NOT the instance's parameter value. Settle it at construct.
	UFUNCTION(BlueprintOverride)
	void Construct()
	{
		if (IsValid(Anim_Hover))
			PlayAnimation(Anim_Hover, Anim_Hover.GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 100.0);
	}

	// Hook names have no BP_ prefix: OnHovered/OnUnhovered/OnSelected/OnDeselected/OnPressed/OnReleased/OnClicked.
	UFUNCTION(BlueprintOverride)
	void OnHovered()
	{
		if (IsValid(Anim_Hover))
			PlayAnimationForward(Anim_Hover);
	}

	UFUNCTION(BlueprintOverride)
	void OnUnhovered()
	{
		if (IsValid(Anim_Hover))
			PlayAnimationReverse(Anim_Hover);
	}
}
```

Rules for this variant:

- **One writer per material parameter.** The WBP animation is the only writer of a state scalar it keys;
  script plays it and never also calls `SetScalarParameterValue` with that name. Script-owned values (fill) go
  through an uncached `Image.GetDynamicMaterial()` write on every update.
- **Rest pose.** Instance parameter values are not the rest state; settle `Anim_*` at frame 0 in `Construct`
  (above). A port that deletes the pack's Construct pose chain without this loses the rest pose.
- **Style.** A `UCommonButtonBase` child with Style None draws a grey box: set a `CommonButtonStyle` with NoDrawType
  on the CDO AND on each WBP instance (the CDO Style does not reach instances).
- **Toggle/select.** `SetIsToggleable(true)` + `OnSelected`/`OnDeselected` play the equip animation forward/reverse;
  exclusivity comes from a Transient `UCommonButtonGroupBase` (`AddWidget`) owned by the grid base.
- **Hot reload.** Adding `UPROPERTY`/`BindWidgetAnim` members is a Full Reload (stop PIE); a body-only `Construct`
  edit soft-reloads.

Reusable abstract bases already shipped in `HF_Common` (`Engine/Plugins/HF_Plugins/HF_Common/Script/UI`):
`UHF_StatBarBaseWidget` / `UHF_GscStatBarBaseWidget` (GAS-attribute bar, `OverrideBoundAttribute`),
`UHF_ListRows_KeyValue` + `UHF_ListRowEntry_KeyValue`/`_Separator` (data-driven key/value rows),
`UFadeFromBlackWidgetBase` / `ULoadingScreenWidgetBase`, `UHF_VisualProxyWidgetBase` (world-space
proxy widgets), `UHF_CommonActionUserWidget` (input-key glyph + details slot). Extend these before
writing a new base.

## 2. Create the WBP with the script class as parent

Verified on UE 5.8 Hazelight (2026-10-09): script classes resolve by object path
`/Script/Angelscript.<ClassName without the U prefix>`; C++ classes by `/Script/<Module>.<Name>`.

```python
import unreal

parent = unreal.load_class(None, "/Script/Angelscript.MyPanelWidget")   # C++: "/Script/CommonUI.CommonButtonBase"
assert parent is not None, "base class not loaded: compile the .as file first"

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", parent)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "WBP_MyPanel", "/Game/UI/MyPanel", unreal.WidgetBlueprint, factory)
print(asset)
```

Equivalent through Epic's toolset (takes the class path as a string):
`call_tool("CreateWidgetBlueprint", "UMGToolSet.UMGToolSet",
{"folderPath": "/Game/UI/MyPanel", "assetName": "WBP_MyPanel", "parentClass": "/Script/Angelscript.MyPanelWidget"})`.

`load_class` returning `None` means the script class has not compiled (check the AngelScript compile
log), not that the path format is wrong.

## 3. Build the tree so every BindWidget resolves

```python
ws = unreal.WidgetService
path = "/Game/UI/MyPanel/WBP_MyPanel"
ws.add_component(path, "CanvasPanel", "RootCanvas", "", True)
ws.add_component(path, "Border", "PanelBorder", "RootCanvas", True)
ws.add_component(path, "VerticalBox", "Stack", "PanelBorder", False)
ws.add_component(path, "TextBlock", "TitleText", "Stack", True)        # BindWidget -> is_variable=True
ws.add_component(path, "ProgressBar", "ValueBar", "Stack", True)
ws.add_component(path, "WBP_MyButton", "ActionButton", "Stack", True)  # child WBP by asset name
```

Rules:

- Name == the `UPROPERTY` name, exactly (case included). Names are unique per blueprint.
- `is_variable` must be `True` for every bound widget (BindWidget binds only to variables).
- The widget class must be the member type or a subclass of it.
- `BindWidgetOptional` members may be omitted; the class must `IsValid()`-guard them.
- Per-instance values on an embedded child WBP (its label text, its icon) are set on the instance
  with `set_property(path, "ActionButton", "LabelText", "REPAIR")`; the child class CDO default
  does not reach the parent's preview.

## 4. Compile and verify (do this before claiming success)

```python
bp = unreal.EditorAssetLibrary.load_asset(path)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)            # or UMGToolSet.CompileWidgetBlueprint {"refPath": ...}

# 1. BindWidget coverage: every required member exists as a variable with a compatible class.
snap = {s.widget_name: s for s in unreal.WidgetService.get_widget_snapshot(path)}
for name in ["TitleText", "ValueBar", "ActionButton"]:
    s = snap.get(name)
    print(name, "OK" if s and s.is_variable else "MISSING/NOT VARIABLE", s.widget_class if s else None)

# 2. The generated class really derives from the base.
gen = unreal.load_class(None, path + "." + path.rsplit("/", 1)[1] + "_C")
print("parent ok:", unreal.MathLibrary.class_is_child_of(gen, parent))

unreal.EditorAssetLibrary.save_asset(path, False)
```

A compile error naming a widget ("... BindWidget ... not found" / wrong type) means step 3 is
incomplete; fix the tree, never add a Blueprint variable to satisfy it.

`get_editor_property("parent_class")` is **not** readable on a `WidgetBlueprint` from Python; use the
`class_is_child_of` check above.

## 5. Existing WBP with graph logic: reparent, then strip

For a legacy WBP whose EventGraph holds the logic, port the logic into a script class first, then:

```python
bel = unreal.BlueprintEditorLibrary
bp = unreal.EditorAssetLibrary.load_asset("/Game/UI/Legacy/WBP_Old")
print("reparent", bel.reparent_blueprint(bp, unreal.load_class(None, "/Script/Angelscript.MyPanelWidget")))
```

Then delete the graph nodes and Blueprint variables the base now owns (`BlueprintService` /
`blueprint-graphs` skill), rename tree widgets to the BindWidget names, compile, and run the
section-4 checks. Interfaces and animations survive a reparent.

Delete Blueprint variables that shadow an AS `UPROPERTY` BEFORE reparenting (same-name collision ICE).

### Remap route: port a WBP family into a new folder (pack stays untouched)

To port an existing family (page + child component WBPs) without editing the originals, copy each WBP
under a UNIQUE name (`AddComponent` resolves WBP types by bare asset name and caches it), then per WBP:
`duplicate_asset` -> `WidgetService.replace_widget_class(wbp, widget, "/Game/.../WBP_X.WBP_X_C")` for every
child UserWidget instance (the target WBP compiled first; NamedSlot content kept by default but its children
need their own replace) -> delete EventGraph nodes and shadowing BP variables -> `WidgetService.rename_widget`
to the BindWidget names (animation bindings follow) -> rename each animation object (and its `MovieScene`
subobject) to the AS `Anim_*` property name via `anim.modify(); anim.rename(...)` -> `reparent_blueprint` ->
`compile_blueprint` -> `save_asset(path, False)`. Exact commands, the remap check and the traps (never
`begin_advanced_copy_packages`/`consolidate_assets`): `ui-materials.md` section 6.

## 6. ListView entry rows (interface wall)

`UListView` entries must implement `UserObjectListEntry`. AngelScript cannot implement interfaces
and `BlueprintService.AddInterface` only accepts Blueprint Interface assets, so: duplicate an
existing WBP that already implements the interface, strip its graph and tree, `reparent_blueprint`
it to the script row class, rebuild the row tree. The parent list widget feeds rows from script via
`ListView.BP_OnEntryInitialized` / `BP_OnItemSelectionChanged` (measured: `WBP_ShipConsole_SalvageRow`
over `UShipConsoleSalvageRow`, SpaceWasteCollector WI-1542).

## 7. Review checklist for an existing script widget

| Smell | Prefer |
|---|---|
| Timer-driven polling (`SetTimer(..., 0.1, true)`) of a replicated value | Attribute/message subscription (`OnAttributeChange`, `UGameplayMessageSubsystem`); poll only for values with no change event, and at >= 0.25 s |
| `DelayedInit` retry timers waiting for the pawn | `OnPossessedPawnChanged` binding on the owning controller |
| Tree widgets fetched by name or by `ObjectIterator` | `BindWidget` members |
| Logic or variables in the WBP graph | Move to the base; keep `BP_*` events for animations only |
| Script class not `Abstract` | `UCLASS(Abstract)` unless the class is meant to be spawned bare |
| Blueprint variable shadowing a script `UPROPERTY` | Delete the BP variable; the script property is the only one |
| `UButton` hitbox over an `Image` for a styled button | `UCommonButtonBase` parent + animation members (section 1b) |
| Material state scalar written by both an animation and script | Animation is the only writer; script plays `Anim_*` |
| Material instance held in a Blueprint variable | `UPROPERTY(EditAnywhere) UMaterialInterface` on the base + `SetBrushFromMaterial` |
