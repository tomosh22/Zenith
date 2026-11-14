# UI design: public API

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It holds the header inventory and the module, type, component, event, system, context, input, draw-list, document and style headers of `ZenithUI`; `Zenith2/Docs/Design/UI/ServicesAPI.md` holds the rest (text, fonts, localization, accessibility, scripting, editing, the UI-owned `Flux_UI.h` of `ZenithFlux` and the tool-side headers). Each header is given with its declarations and one contract block per type in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`. Algorithms behind these declarations are in the sibling files named in each section; this file states signatures, ownership, thread affinity, invalidation, units, errors and versions.

## Scope of this file

| Topic | Section |
|---|---|
| the header inventory and what each header may include | "Header list" |
| the module entry points | "Zenith_Module_UI header" |
| shared value types, the error records, handles | "Zenith_UITypes header" |
| the ECS component, resources and command payloads | "Zenith_UIComponents header" |
| the script-visible event types | "Zenith_UIEvents header" |
| the process-level UI system (fonts, text engine, localization, glyph cache) | "Zenith_UISystem header" |
| the UI context: screens, layout, input feed, draw lists, accessibility tree | "Zenith_UIContext header", "Zenith_UIInput header", "Zenith_UIDrawList header" |
| documents and styles | "Zenith_UIDocument header", "Zenith_UIStyle header" |
| text, fonts, baking, localization, accessibility, the Luau surface, the editor model, the GPU side and the ZenithCook files | `Zenith2/Docs/Design/UI/ServicesAPI.md` (its "Scope of this file") |

## Header list

Every header below is a public root header of `Zenith2/Engine/ZenithUI/` unless the path says otherwise. A public header includes only ZenithUI's public headers, public headers of ZenithUI's declared dependencies (`Zenith2/Docs/Architecture.md → "Library dependency graph"`, row ZenithUI) and the allowed standard headers ([Z2-N-CONTRACT-026]); no HarfBuzz, FreeType, msdfgen, SheenBidi, ICU4X or RmlUi header reaches any of them ([Z2-N-ARCH-122]).

Headers from `Zenith_UIText.h` down are specified in `Zenith2/Docs/Design/UI/ServicesAPI.md`.

| Header | Declares | Stage |
|---|---|---|
| `Zenith_Module_UI.h` | `Zenith_Module_UI_RegisterTypes`, `Zenith_Module_UI_Start` | S3 |
| `Zenith_UITypes.h` | `Zenith_UIRect`, `Zenith_UIEdges`, `Zenith_UISize`, `Zenith_UIScreenHandle`, `Zenith_UIElementRef`, `Zenith_UIValueKind`, `Zenith_UIValue`, `Zenith_UIErrorCode`, `Zenith_UIError`, `Zenith_ToString` overloads | S3 |
| `Zenith_UIComponents.h` | `Zenith_UIScreenSpace`, `Zenith_UIScreenComponent`, `Zenith_UIRequestKind`, `Zenith_UIFocusRequest`, `Zenith_UIAnnounceRequest`, `Zenith_UIScrollRequest`, `Zenith_UIAnimationRequest`, `Zenith_UIClearCaptionsRequest`, `Zenith_UICaptionKind`, `Zenith_UICaptionCue`, `Zenith_RegisterUIComponentTypes` | S3 (captions S4) |
| `Zenith_UIEvents.h` | `Zenith_UIEventKind`, `Zenith_UIActivatedEvent`, `Zenith_UIValueChangedEvent`, `Zenith_UIToggledEvent`, `Zenith_UISelectionChangedEvent`, `Zenith_UITextCommittedEvent`, `Zenith_UICancelledEvent`, `Zenith_UIPublicationRecord`, `Zenith_EncodeUIPublication`, `Zenith_DecodeUIPublication` | S3 |
| `Zenith_UISystem.h` | `Zenith_UISystemDesc`, `Zenith_UISystem`, `Zenith_UIConfig`, `Zenith_RegisterUIConfigKeys`, `Zenith_ReadUIConfig` | S3 |
| `Zenith_UIContext.h` | `Zenith_UISurfaceDesc`, `Zenith_UIContextDesc`, `Zenith_UIScreenDesc`, `Zenith_UIScreenState`, `Zenith_UIFrameParams`, `Zenith_UIElementLayout`, `Zenith_UIContextStats`, `Zenith_UIContext` | S3 |
| `Zenith_UIInput.h` | `Zenith_UIPointerPhase`, `Zenith_UIPointerInput`, `Zenith_UINavInput`, `Zenith_UINavDirection`, `Zenith_UITextInput`, `Zenith_UIHit`, `Zenith_UIEventRecord` | S3 |
| `Zenith_UIDrawList.h` | `Zenith_UIDrawQuad`, `Zenith_UIDrawBatch`, `Zenith_UIDrawList`, `Zenith_UIWorldPanel` | S3 |
| `Zenith_UIDocument.h` | `Zenith_UIDocumentAsset`, `Zenith_UIElementType`, `Zenith_UIDocumentInfo`, `Zenith_UIDocumentDiagnostic`, `Zenith_UIValidationContext`, `Zenith_UIDocumentModel`, `Zenith_CompileUIDocument` | S3 |
| `Zenith_UIStyle.h` | `Zenith_UIStyleProperty`, `Zenith_UILayoutDirection`, `Zenith_UIStateFlags`, `Zenith_UIEasing`, `Zenith_UILengthUnit`, `Zenith_UILayoutKind`, `Zenith_UIStackDirection`, `Zenith_UIJustify`, `Zenith_UIAlign`, `Zenith_UIPosition`, `Zenith_UIImageFit`, `Zenith_UIPointerEvents`, `Zenith_UILength`, `Zenith_UIStyleSpan`, `Zenith_UIImageSource`, `Zenith_UIGradient`, `Zenith_UIComputedStyle` | S3 |
| `Zenith_UIText.h` | `Zenith_TextDirection`, `Zenith_TextAlign`, `Zenith_TextWrap`, `Zenith_TextOverflow`, `Zenith_TextStyle`, `Zenith_TextSpan`, `Zenith_TextParagraphDesc`, `Zenith_TextGlyph`, `Zenith_TextLine`, `Zenith_TextLayout`, `Zenith_TextCaret`, `Zenith_TextLayoutError`, `Zenith_TextLayoutEngine`, `Zenith_TextEditBuffer` | S3 (bidi S4) |
| `Zenith_UIFont.h` | `Zenith_FontAsset`, `Zenith_FontFaceInfo`, `Zenith_FontMetrics`, `Zenith_FontStack`, `Zenith_GlyphKey`, `Zenith_GlyphPlacement`, `Zenith_GlyphCache`, `Zenith_FontError` | S3 |
| `Zenith_UIFontBake.h` | `Zenith_FontBakeDesc`, `Zenith_FontBakeFace`, `Zenith_FontBakeOutput`, `Zenith_BakeFontFamily`, `Zenith_GenerateGlyphMSDF` | S3 |
| `Zenith_UILocalization.h` | `Zenith_LocaleID`, `Zenith_LocalizationTableAsset`, `Zenith_MessageArg`, `Zenith_PluralCategory`, `Zenith_Localization`, `Zenith_LocalizationErrorCode`, `Zenith_LocalizationError`, `Zenith_CompileMessage`, `Zenith_EvaluatePluralRule`, `Zenith_FormatNumber` | S3 (format and plurals S4) |
| `Zenith_UIAccessibility.h` | `Zenith_UIColourVision`, `Zenith_UIAccessibilitySettings`, `Zenith_UIRole`, `Zenith_UIA11yStateFlags`, `Zenith_UIA11yNode`, `Zenith_UIA11yTree`, `Zenith_UINarrationPriority`, `Zenith_UINarrationRequest` | S3 (settings), S4 (tree, narration) |
| `Zenith_UIScriptApi.h` | `Zenith_UIScriptApi`, `Zenith_RegisterUIScriptBindings` | S3 |
| `Zenith_UIEditorModel.h` | `Zenith_UIEditOperationKind`, `Zenith_UIEditOperation`, `Zenith_UIEditResult`, `Zenith_UIWidgetDesc`, `Zenith_UIPropertyDesc`, `Zenith_UIEditorModel` | S4 |
| `Zenith2/Engine/ZenithFlux/Flux_UI.h` | `Flux_UIQuadMode`, `Flux_UIQuad`, `Flux_UIBatch`, `Flux_RenderUIList`, `Flux_UIAtlasPageDesc`, `Flux_UIAtlasPage`, `Flux_UIRendererDesc`, `Flux_UIRenderer`, `Flux_UIFrame`, `Flux_AddUIComposite`, `Flux_AddUIWorldSpace` | S3 |

<!-- informative -->
Zenith exposed one convenience header (`Zenith_UI.h`) that pulled in every element class, and elements were C++ classes with virtual `Update`/`Render`. Zenith2 has no element class hierarchy in the public API: elements are records of a cooked document instantiated inside a `Zenith_UIContext`, addressed by `Zenith_UIElementRef`, so the public surface stays a handful of value types and two service objects, and the runtime behind them can be the custom implementation or the RmlUi integration that D-SEL-UIRT (open, Stage 1/2) selects without an API change.
<!-- /informative -->

## Zenith_Module_UI header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_Module_UI.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithReflect/Zenith_TypeRegistry.h"
#include "ZenithRuntime/Zenith_Module.h"

/// @contract functions Zenith_ModuleUIEntryPoints
/// Ownership: Zenith_Module_UI_Start returns the module instance (Zenith_UIModule, Internal) as a
///   Zenith_Owned<Zenith_ModuleInstance> from the general heap; ZenithRuntime owns it until shutdown.
/// Thread affinity: MAIN_THREAD; RegisterTypes at startup step 6, Start at step 15 (Zenith2/Docs/Architecture/CompositionRoot.md
///   "Startup sequence").
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ModuleError: MODULE_ERROR_REGISTRATION when a type, configuration key, command kind, binding, kind loader,
///   claimant or extension registration is refused (each refusal logged under ui.module first); MODULE_ERROR_OUT_OF_MEMORY when
///   a creation step cannot allocate. A failure at any step unwinds every earlier step in reverse ([Z2-N-CONTRACT-340]).
/// Version: since Z2 Stage 3. Exactly the two entry points of [Z2-N-ARCH-168]; the plain Start form of
///   Zenith2/Docs/Design/MOD/GameBuild.md "Module to target mapping".
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_UI_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_UI_Start(
	Zenith_ModuleStartContext& xContext);
```

`RegisterTypes` performs, in order: `Zenith_RegisterUIComponentTypes(xTypes)`; the six event types of "Zenith_UIEvents header"; the two world-resource types of the entry document's "World resources"; the payload structs of the six command kinds; `Zenith_RegisterUIConfigKeys(xTypes.GetConfigSchema())` (a module registers its configuration keys from `RegisterTypes`, before any configuration layer is read). `Start` performs the eleven steps of `Zenith2/Docs/Design/UI.md → "Module start sequence"`, each with a failure-injection value.

## Zenith_UITypes header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UITypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAssets/Zenith_AssetHandle.h"

/// @contract value Zenith_UIRect
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Canvas units (u) of the owning screen unless the member using it says pixels; origin top-left, +Y down
///   (Zenith2/Docs/Design/CORE/Maths.md "Coordinate systems", framebuffer row).
/// Errors: None; a negative width or height is clamped to 0 by every consumer.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIRect
{
	float m_fX = 0.0f;
	float m_fY = 0.0f;
	float m_fWidth = 0.0f;
	float m_fHeight = 0.0f;
};

/// @contract value Zenith_UIEdges
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Canvas units, or pixels for a safe-area inset.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIEdges
{
	float m_fLeft = 0.0f;
	float m_fTop = 0.0f;
	float m_fRight = 0.0f;
	float m_fBottom = 0.0f;
};

/// @contract value Zenith_UISize
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Canvas units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UISize
{
	float m_fWidth = 0.0f;
	float m_fHeight = 0.0f;
};

/// @contract value Zenith_UIScreenHandle
/// Ownership: Plain value. A context-scoped generational handle with the rules of a world handle in
///   Zenith2/Docs/Contracts.md "Handle kinds": the issuing Zenith_UIContext owns the screen; copying never extends its life.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign. Resolution follows the
///   issuing context's thread affinity.
/// Invalidation: Stale once CloseScreen ran on it or its context was destroyed; resolution then fails with
///   UI_ERROR_STALE_HANDLE in every configuration (generation and context-tag compare).
/// Units: None.
/// Errors: None by itself; resolving members return Zenith_UIError.
/// Version: since Z2 Stage 3. Never serialized ([Z2-N-CONTRACT-064]); publications name screens by the screen entity's
///   stable simulation ID instead.
/// @endcontract
class Zenith_UIScreenHandle
{
public:
	constexpr Zenith_UIScreenHandle() = default;
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Zenith_UIScreenHandle, Zenith_UIScreenHandle) = default;
private:
	friend class Zenith_UIContext;
	uint16 m_uContextTag = 0;
	uint16 m_uIndex = 0;
	uint32 m_uGeneration = 0;
};

/// @contract value Zenith_UIElementRef
/// Ownership: Plain value naming one element of one screen instance.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: As its screen handle; m_uElement is an index into the screen instance's element table, stable for the
///   instance's lifetime; a virtualized list item's element is valid until the item is recycled
///   (Zenith2/Docs/Design/UI/Layout.md "Item recycling"), after which resolution returns UI_ERROR_STALE_HANDLE.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIElementRef
{
	Zenith_UIScreenHandle m_xScreen;
	uint32 m_uElement = 0xFFFFFFFFu;     // 0xFFFFFFFF: the screen itself
	uint32 m_uItemGeneration = 0;        // 0 for elements outside virtualized lists
};

/// @contract enum Zenith_UIValueKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; values are stable (cooked binding records store them).
/// @endcontract
enum Zenith_UIValueKind : uint8
{
	UI_VALUE_KIND_NONE = 0,
	UI_VALUE_KIND_BOOL = 1,
	UI_VALUE_KIND_INT = 2,          // sint64
	UI_VALUE_KIND_FLOAT = 3,        // double
	UI_VALUE_KIND_STRING_ID = 4,    // an interned string: a localization key or literal text by the binding's text mode
	UI_VALUE_KIND_TEXT = 5,         // a view of presentation text (context-owned storage); never from simulation
	UI_VALUE_KIND_COLOUR = 6,       // linear Zenith_Colour
	UI_VALUE_KIND_VECTOR2 = 7,
	UI_VALUE_KIND_ASSET = 8,        // a texture asset (Flux.Texture) for image bindings
	UI_VALUE_KIND_LIST = 9,         // a list source: count plus an element accessor (Layout.md "Data binding evaluation")
};

/// @contract value Zenith_UIValue
/// Ownership: Plain value; m_strText views storage owned by whoever produced the value for the duration stated by the
///   producing call; m_xAsset is a counted reference while the value lives in a binding slot.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: m_strText as its producer states; others none.
/// Units: As the bound property.
/// Errors: None; a kind the consumer cannot convert is rendered as empty and counted (ui.binding.conversion_failed).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIValue
{
	Zenith_UIValueKind m_eKind = UI_VALUE_KIND_NONE;
	bool m_bValue = false;
	sint64 m_iValue = 0;
	double m_dValue = 0.0;
	Zenith_StringID m_xString;
	Zenith_StringView m_strText;
	Zenith_Colour m_xColour;
	Zenith_Vector2 m_xVector;
	Zenith_AssetHandle<Flux_TextureResource> m_xAsset;
	uint32 m_uListCount = 0;
};

/// @contract enum Zenith_UIErrorCode
/// Ownership: Plain enumeration, the code of Zenith_UIError (the struct form of Zenith2/Docs/Contracts.md "Errors and results").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator names the failure; Zenith2/Docs/Design/UI.md "Error enumerations" lists when each is returned.
/// Version: since Z2 Stage 3; values are never renumbered ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_UIErrorCode : uint16
{
	UI_ERROR_INVALID_DESC = 1,
	UI_ERROR_OUT_OF_MEMORY,
	UI_ERROR_STALE_HANDLE,
	UI_ERROR_SCREEN_LIMIT,
	UI_ERROR_ELEMENT_LIMIT,
	UI_ERROR_DOCUMENT_NOT_READY,
	UI_ERROR_DOCUMENT_CORRUPT,
	UI_ERROR_DOCUMENT_VERSION,
	UI_ERROR_UNKNOWN_ELEMENT,
	UI_ERROR_UNKNOWN_BINDING,
	UI_ERROR_BINDING_TYPE,
	UI_ERROR_NOT_FOCUSABLE,
	UI_ERROR_FONT_MISSING,
	UI_ERROR_LOCALE_UNSUPPORTED,
	UI_ERROR_RENDERER_UNAVAILABLE,
	UI_ERROR_ATLAS_FULL,
	UI_ERROR_PUBLICATION_FULL,
	UI_ERROR_AUTHORING_INVALID,
	UI_ERROR_EDIT_REJECTED,
	UI_ERROR_RUNTIME_BACKEND,
};

/// @contract value Zenith_UIError
/// Ownership: Plain value, 16 bytes, text-free ([Z2-N-CONTRACT-480]); the failing call logs the detail under its category
///   first ([Z2-N-CONTRACT-464]).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uIndex is an element, screen, binding or byte index as the code's row states.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIError
{
	const Zenith_UIErrorCode m_eCode = UI_ERROR_INVALID_DESC;
	const uint16 m_uDetail = 0;            // the nested code: Zenith_AssetError code, Zenith_FontError, Zenith_TextLayoutError, …
	const uint32 m_uIndex = 0;
	const uint64 m_ulName = 0;             // Zenith_Hash64 of the element, binding or key name; 0 when not applicable
};
static_assert(sizeof(Zenith_UIError) == 16);

constexpr const char* Zenith_ToString(Zenith_UIErrorCode eCode);
```

`Flux_TextureResource` is GEOM's (`Zenith2/Docs/Design/GEOM/Textures.md → "Flux_Texture header"`), reached through ZenithUI's declared `ZenithFlux` edge; `Zenith_UITypes.h` forward-declares it; `Zenith_UIDrawList.h` includes `ZenithFlux/Flux_UI.h` (for `Flux_UIQuad` and `Flux_UIBatch`) and no texture header, and the texture's descriptor is read only in `Internal/` sources, which include `ZenithFlux/Flux_Texture.h`.

## Zenith_UIComponents header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIComponents.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithReflect/Zenith_TypeRegistry.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithUI/Zenith_UIDocument.h"

/// @contract enum Zenith_UIScreenSpace
/// Ownership: Plain enumeration; property 5 of Zenith.UI.Screen.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an out-of-range stored value is treated as UI_SCREEN_SPACE_SCREEN and counted.
/// Version: since Z2 Stage 3; values are stable (scene documents store them).
/// @endcontract
enum Zenith_UIScreenSpace : uint8
{
	UI_SCREEN_SPACE_SCREEN = 0,     // composited over the view output (UIComposite)
	UI_SCREEN_SPACE_WORLD = 1,      // a panel in the entity's XY plane, drawn in the transparent stage (UIWorldSpace)
};

/// @contract type Zenith_UIScreenComponent
/// Ownership: Stored by the ECS as component "Zenith.UI.Screen" (C++-registered, [Z2-N-ARCH-614]); one per screen entity.
///   m_xDocument is a counted asset reference managed by the component's lifecycle hooks ([Z2-N-ARCH-645]).
/// Thread affinity: PHASE(LATE_SCRIPTS). Every property's authority is PHASE_LATE_SCRIPTS; other phases change it through
///   commands; any phase, extraction included, reads it.
/// Invalidation: None beyond the entity's.
/// Units: m_xWorldSizeMetres in metres; m_fPixelsPerMetre in canvas units per metre; m_iLayer unitless (higher draws above).
/// Errors: None at the component; an unloaded or failed document makes the screen draw nothing and count
///   ui.screen.document_unavailable (Zenith2/Docs/Design/UI.md "Failure table").
/// Version: since Z2 Stage 3. Schema version 1; property IDs are never reused ([Z2-N-ARCH-594]).
/// @endcontract
struct Zenith_UIScreenComponent
{
	static constexpr uint32 uPROP_DOCUMENT = 1;
	static constexpr uint32 uPROP_LAYER = 2;
	static constexpr uint32 uPROP_VISIBLE = 3;
	static constexpr uint32 uPROP_INTERACTIVE = 4;
	static constexpr uint32 uPROP_SPACE = 5;
	static constexpr uint32 uPROP_WORLD_SIZE_METRES = 6;
	static constexpr uint32 uPROP_PIXELS_PER_METRE = 7;
	static constexpr uint32 uPROP_BLOCKS_INPUT = 8;
	static constexpr uint32 uPROP_INITIAL_FOCUS = 9;
	static constexpr uint32 uPROP_STATE_SERIAL = 10;

	Zenith_AssetHandle<Zenith_UIDocumentAsset> m_xDocument;   // asset<UI.Document>
	sint32 m_iLayer = 0;
	bool m_bVisible = true;
	bool m_bInteractive = true;
	Zenith_UIScreenSpace m_eSpace = UI_SCREEN_SPACE_SCREEN;
	Zenith_Vector2 m_xWorldSizeMetres{ 1.0f, 1.0f };
	float m_fPixelsPerMetre = 512.0f;
	bool m_bBlocksInput = false;                               // modal: claims every pointer and consumes navigation
	Zenith_StringID m_xInitialFocus;                           // element id focused when the screen first becomes interactive
	uint32 m_uStateSerial = 0;                                // incremented by scripts to re-apply m_xInitialFocus
};

/// @contract enum Zenith_UIRequestKind
/// Ownership: Plain enumeration; the request kinds of Zenith.UI.Requests.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (FOCUS, SCROLL, ANIMATION); ANNOUNCE and the caption kinds since Z2 Stage 4.
/// @endcontract
enum Zenith_UIRequestKind : uint8
{
	UI_REQUEST_KIND_FOCUS = 1,
	UI_REQUEST_KIND_SCROLL = 2,
	UI_REQUEST_KIND_ANIMATION = 3,
	UI_REQUEST_KIND_ANNOUNCE = 4,
	UI_REQUEST_KIND_CAPTION = 5,
	UI_REQUEST_KIND_CLEAR_CAPTIONS = 6,
};

/// @contract value Zenith_UIFocusRequest
/// Ownership: Plain value; the payload struct of command kind UI.Focus (TYPE_KIND_STRUCT "Zenith.UI.FocusRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: m_xScreen follows the entity handle's rules; a stale handle at playback drops the command and counts
///   ui.requests.stale ([Z2-N-ARCH-492]).
/// Units: None.
/// Errors: None; an unknown element name at application is counted (ui.requests.unknown_element) and ignored.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIFocusRequest
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;          // empty: clear focus on that screen
};

/// @contract value Zenith_UIScrollRequest
/// Ownership: Plain value; the payload of UI.ScrollTo ("Zenith.UI.ScrollRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: As Zenith_UIFocusRequest.
/// Units: m_iItem is a list item index; -1 scrolls the named element itself into view.
/// Errors: None; out-of-range items clamp.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIScrollRequest
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	sint32 m_iItem = -1;
};

/// @contract value Zenith_UIAnimationRequest
/// Ownership: Plain value; the payload of UI.PlayAnimation ("Zenith.UI.AnimationRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: As Zenith_UIFocusRequest.
/// Units: None.
/// Errors: None; an animation the theme does not define is counted (ui.requests.unknown_animation) and ignored.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIAnimationRequest
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	Zenith_StringID m_xAnimation;
};

/// @contract enum Zenith_UICaptionKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable.
/// @endcontract
enum Zenith_UICaptionKind : uint8
{
	UI_CAPTION_KIND_SUBTITLE = 0,        // spoken dialogue; shown when ui.captions.subtitles is on
	UI_CAPTION_KIND_SOUND = 1,           // a described sound ("[door slams]"); shown when ui.captions.sounds is on
	UI_CAPTION_KIND_COUNT = 2,
};

/// @contract value Zenith_UIAnnounceRequest
/// Ownership: Plain value; the payload of UI.Announce ("Zenith.UI.AnnounceRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown key narrates the key text in development builds and nothing in Shipping (counted).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UIAnnounceRequest
{
	Zenith_StringID m_xKey;              // a localization key
	uint8 m_uPriority = 0;              // Zenith_UINarrationPriority
};

/// @contract value Zenith_UIClearCaptionsRequest
/// Ownership: Plain value; the payload of UI.ClearCaptions ("Zenith.UI.ClearCaptionsRequest").
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a zero mask clears nothing.
/// Version: since Z2 Stage 4 (captions are Stage 4).
/// @endcontract
struct Zenith_UIClearCaptionsRequest
{
	uint8 m_uKindMask = 0xFF;           // bit (1 << Zenith_UICaptionKind) per kind to remove
};

/// @contract value Zenith_UICaptionCue
/// Ownership: Plain value; the payload of UI.ShowCaption ("Zenith.UI.CaptionCue"), and the element of the caption resource.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: m_uDurationTicks in simulation ticks of the issuing world; m_ulStartTick set at playback.
/// Errors: None; a zero duration is 1 tick; more than uUI_CAPTION_QUEUE_CAPACITY live cues evict the lowest priority, oldest
///   first (counted ui.captions.evicted).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_UICaptionCue
{
	Zenith_StringID m_xKey;              // the caption text's localization key
	Zenith_StringID m_xSpeakerKey;       // the speaker name's key; empty for none
	Zenith_UICaptionKind m_eKind = UI_CAPTION_KIND_SUBTITLE;
	uint8 m_uPriority = 128;
	uint16 m_uSpeakerColour = 0;        // index into the theme's speaker palette; 0 = default text colour
	uint32 m_uDurationTicks = 0;
	uint64 m_ulStartTick = 0;
};
inline constexpr uint32 uUI_CAPTION_QUEUE_CAPACITY = 8;

/// @contract functions Zenith_UIComponentRegistration
/// Ownership: Registers into the caller's type registry.
/// Thread affinity: MAIN_THREAD, startup step 6 (from Zenith_Module_UI_RegisterTypes).
/// Invalidation: None.
/// Units: None.
/// Errors: The first Zenith_ReflectError of the registry, mapped to MODULE_ERROR_REGISTRATION by the entry point.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterUIComponentTypes(Zenith_TypeRegistry& xTypes);
```

| Stable name | C++ type | Kind | Script name | Authority or owning phase |
|---|---|---|---|---|
| `Zenith.UI.Screen` | `Zenith_UIScreenComponent` | component | `UIScreen` | `PHASE_LATE_SCRIPTS` for every property |
| `Zenith.UI.FocusRequest` | `Zenith_UIFocusRequest` | struct (command payload) | — | — |
| `Zenith.UI.ScrollRequest` | `Zenith_UIScrollRequest` | struct | — | — |
| `Zenith.UI.AnimationRequest` | `Zenith_UIAnimationRequest` | struct | — | — |
| `Zenith.UI.AnnounceRequest` | `Zenith_UIAnnounceRequest` | struct | — | — |
| `Zenith.UI.ClearCaptionsRequest` | `Zenith_UIClearCaptionsRequest` | struct | — | — |
| `Zenith.UI.CaptionCue` | `Zenith_UICaptionCue` | struct | `UICaptionCue` | — |

The two world resources (`Zenith.UI.Requests`, `Zenith.UI.Captions`) are Internal types; their layout and owning phases are in `Zenith2/Docs/Design/UI.md → "World resources"`. Decoded UI publications are not a world resource: they are module state of each world (Interaction.md "Publication path" step 4).

## Zenith_UIEvents header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIEvents.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_StringView.h"    // Zenith_UIPublicationRecord's name and text views
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithUI/Zenith_UITypes.h"          // Zenith_UIError (the codec's error type)

/// @contract enum Zenith_UIEventKind
/// Ownership: Plain enumeration; the first field of every UI publication payload.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a decoder refuses an unknown value (UI_ERROR_DOCUMENT_CORRUPT with m_uDetail 1).
/// Version: since Z2 Stage 3; recorded in replay logs, so values are never renumbered.
/// @endcontract
enum Zenith_UIEventKind : uint16
{
	UI_EVENT_KIND_ACTIVATED = 1,
	UI_EVENT_KIND_VALUE_CHANGED = 2,
	UI_EVENT_KIND_TOGGLED = 3,
	UI_EVENT_KIND_SELECTION_CHANGED = 4,
	UI_EVENT_KIND_TEXT_COMMITTED = 5,
	UI_EVENT_KIND_CANCELLED = 6,
	UI_EVENT_KIND_WORLD_RESUMED = 7,   // the resume record of a resumeWorld action: published and recorded, never emitted as an ECS event
};

/// @contract value Zenith_UIActivatedEvent
/// Ownership: Plain value; event type "Zenith.UI.Activated" (script name UI.Activated), emitted by UI.Events@W<tag> in phase 1
///   and delivered in phase 9 of the same tick (Zenith2/Docs/Design/UI/Interaction.md "Publication path").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: m_xScreen follows entity-handle rules.
/// Units: m_iItem is a list item index, -1 outside lists.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIActivatedEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;          // the element's authored id
	Zenith_StringID m_xEvent;            // the element's authored event name ("events.activate"), or empty
	sint32 m_iItem = -1;
};

/// @contract value Zenith_UIValueChangedEvent
/// Ownership: Plain value; "Zenith.UI.ValueChanged" (UI.ValueChanged): a slider, stepper or progress-style control committed a value.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As Zenith_UIActivatedEvent.
/// Units: m_dValue in the control's own range.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIValueChangedEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	Zenith_StringID m_xEvent;
	double m_dValue = 0.0;
	sint32 m_iItem = -1;
};

/// @contract value Zenith_UIToggledEvent
/// Ownership: Plain value; "Zenith.UI.Toggled" (UI.Toggled).
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As Zenith_UIActivatedEvent.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIToggledEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	Zenith_StringID m_xEvent;
	bool m_bChecked = false;
	sint32 m_iItem = -1;
};

/// @contract value Zenith_UISelectionChangedEvent
/// Ownership: Plain value; "Zenith.UI.SelectionChanged" (UI.SelectionChanged): a list or choice control's selected index.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As Zenith_UIActivatedEvent.
/// Units: m_iIndex is 0-based, -1 for no selection.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UISelectionChangedEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	Zenith_StringID m_xEvent;
	sint32 m_iIndex = -1;
};

/// @contract value Zenith_UITextCommittedEvent
/// Ownership: Plain value; "Zenith.UI.TextCommitted" (UI.TextCommitted). m_xText is interned by UI.Events at emission.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As Zenith_UIActivatedEvent.
/// Units: None.
/// Errors: None; when the string table is full the event carries empty text and ui.events.text_intern_failed counts it.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UITextCommittedEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xElement;
	Zenith_StringID m_xEvent;
	Zenith_StringID m_xText;
};

/// @contract value Zenith_UICancelledEvent
/// Ownership: Plain value; "Zenith.UI.Cancelled" (UI.Cancelled): UI.Cancel pressed while the screen held focus and its document
///   declares "cancelEvent".
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As Zenith_UIActivatedEvent.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UICancelledEvent
{
	Zenith_EntityHandle m_xScreen;
	Zenith_StringID m_xEvent;
};

/// @contract value Zenith_UIPublicationRecord
/// Ownership: Plain value; the decoded form of one ASYNC_SOURCE_UI publication payload (Zenith2/Docs/Design/UI/Interaction.md
///   "Event kinds and payloads"); its string members view the payload bytes.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: The string views are valid while the payload bytes are.
/// Units: Byte lengths.
/// Errors: None by itself.
/// Version: since Z2 Stage 3. The byte layout is part of the replay log (ZRPL) content; a change is a ZRPL format bump.
/// @endcontract
struct Zenith_UIPublicationRecord
{
	Zenith_UIEventKind m_eKind = UI_EVENT_KIND_ACTIVATED;
	sint32 m_iItem = -1;
	uint64 m_ulScreenSimulationID = 0;
	uint64 m_ulValueBits = 0;           // VALUE_CHANGED: f64 bits; TOGGLED: 0 or 1; SELECTION_CHANGED: the sint32 index, sign-extended
	uint64 m_ulWriteBackType = 0;       // Zenith_TypeID of a self two-way binding's component, 0 for none
	uint32 m_uWriteBackProperty = 0;    // its Zenith_PropertyID
	Zenith_StringView m_strElement;      // at most 64 bytes
	Zenith_StringView m_strEvent;        // at most 64 bytes
	Zenith_StringView m_strText;         // TEXT_COMMITTED only, at most 256 bytes
};

/// @contract functions Zenith_UIPublicationCodec
/// Ownership: Encode writes into the caller's buffer; Decode views the caller's bytes.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes; uUI_PUBLICATION_MAX_BYTES (448) is the largest payload.
/// Errors: Encode returns UI_ERROR_INVALID_DESC for a name or text above its limit or invalid UTF-8; Decode returns
///   UI_ERROR_DOCUMENT_CORRUPT for a short payload, an unknown kind, a length past the payload or invalid UTF-8; replayed
///   payloads are untrusted ([Z2-N-ARCH-951]) and never assert.
/// Version: since Z2 Stage 3.
/// @endcontract
inline constexpr uint32 uUI_PUBLICATION_MAX_BYTES = 448;
Zenith_Result<uint32, Zenith_UIError> Zenith_EncodeUIPublication(const Zenith_UIPublicationRecord& xRecord,
	Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_UIPublicationRecord, Zenith_UIError> Zenith_DecodeUIPublication(Zenith_ByteSpan xPayload);
```

The six event types are registered as `TYPE_KIND_EVENT` with the property IDs in field order from 1 and `PROPERTY_FLAG_SERIALIZED` (a save writes the events a snapshot tick carries into the next tick, `Zenith2/Docs/Design/SAVE/Formats.md → "Events section"`).

## Zenith_UISystem header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UISystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithCore/Zenith_StringTable.h"
#include "ZenithCore/Zenith_MemoryGovernor.h"
#include "ZenithAssets/Zenith_AssetRegistry.h"
#include "ZenithUI/Zenith_UIText.h"
#include "ZenithUI/Zenith_UIFont.h"
#include "ZenithUI/Zenith_UILocalization.h"
#include "ZenithUI/Zenith_UIAccessibility.h"

class Flux_UIRenderer;
class Zenith_Heap;
class Zenith_FaultInjector;

/// @contract value Zenith_UIConfig
/// Ownership: Plain value filled by Zenith_ReadUIConfig from the configuration store.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None; the module re-reads it when a ui.* key changes (prologue step 3).
/// Units: As the key table of Zenith2/Docs/Design/UI.md "Configuration keys".
/// Errors: None; Zenith_ReadUIConfig validates every value.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIConfig
{
	float m_fUIScale = 1.0f;
	Zenith_StringView m_strLocale;                     // empty: the platform's preferred locale
	uint8 m_uPseudoLocalization = 0;                  // 0 off, 1 accented, 2 mirrored, 3 long
	float m_fPseudoLongFactor = 1.4f;
	Zenith_UIAccessibilitySettings m_xAccessibility;
	float m_fSDFSharpness = 1.0f;
	uint32 m_uDynamicGlyphsPerFrame = 16;
	uint32 m_uLayoutCacheEntries = 2048;
	uint32 m_uMaxDynamicPages = 8;
	uint32 m_uPageUploadsPerFrame = 2;
	uint32 m_uHeadlessCanvasWidth = 1920;
	uint32 m_uHeadlessCanvasHeight = 1080;
	uint32 m_uMaxEventsPerFrame = 64;                 // 2 … 1,024; one publication per frame is reserved for a resume record
	uint32 m_uMaxEventsInFlight = 256;                // 16 … 4,096; outstanding publications per world, one reserved likewise
	uint32 m_uMaxScreensPerWorld = 64;
	uint32 m_uMaxElementsPerContext = 65536;
	float m_fDragThresholdDp = 8.0f;
	uint32 m_uNavRepeatDelayMs = 400;
	uint32 m_uNavRepeatIntervalMs = 80;
	float m_fWheelStepDp = 48.0f;
	float m_fWorldSpaceLuminanceNits = 100.0f;
	float m_fWorldMinGlyphPixels = 6.0f;
	uint32 m_uDebugViewMask = 0;                      // bit 0 ui.debug.bounds, 1 focus, 2 batches, 3 atlas, 4 a11y, 5 pseudo
};

/// @contract functions Zenith_UIConfigFunctions
/// Ownership: Register writes key descriptors into the caller's schema; Read fills the caller's value.
/// Thread affinity: MAIN_THREAD (Register at startup step 6; Read at module start and in the prologue).
/// Invalidation: Zenith_UIConfig::m_strLocale views store-owned text, valid until the store's next change of ui.locale.
/// Units: None.
/// Errors: Register returns the schema's Zenith_ConfigError (duplicate key, bad range); Read never fails: a value outside its
///   range was refused by the store already, so Read copies validated values.
/// Version: since Z2 Stage 3; no ui.* key is simulation-affecting (Zenith2/Docs/Design/CORE/Diagnostics.md
///   "Simulation-affecting list"), so none is recorded in a replay header.
/// @endcontract
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterUIConfigKeys(Zenith_ConfigSchema& xSchema);
void Zenith_ReadUIConfig(const Zenith_ConfigStore& xStore, Zenith_UIConfig& xOut);

/// @contract value Zenith_UISystemDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the system.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Zenith_UISystem::Create (UI_ERROR_INVALID_DESC for a null heap, string table or asset registry).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UISystemDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_StringTable* m_pxStrings = nullptr;
	Zenith_AssetRegistry* m_pxAssets = nullptr;
	Flux_UIRenderer* m_pxRenderer = nullptr;           // null when headless: no atlas page is uploaded, no MSDF is generated
	Zenith_UIConfig m_xConfig;
	Zenith_StringView m_strPlatformLocale;             // Zenith_Platform::GetPreferredLocale() at start
	Zenith_FaultInjector* m_pxFaultInjector = nullptr; // tests alone
};

/// @contract type Zenith_UISystem
/// Ownership: Created by the UI module at start step 7 as a Zenith_Owned (or by a test or tool directly); owns the text layout
///   engine, the font registry, the glyph cache and its atlases, the localization state and the runtime backend selected by
///   D-SEL-UIRT (open, Stage 1/2). One per process in a Player; tools and tests may create more.
/// Thread affinity: ANY_THREAD_EXCEPT(SetLocale, ApplyConfig, BeginFrame, EndFrame, OnPressureLevelChanged).
///   The excepted members run on the main thread in the frame prologue; GetText, GetGlyphCache and
///   GetLocalization return objects whose own contracts allow use from the UI extractors of every world concurrently, because
///   each extractor touches only its own world's contexts and the shared objects are internally partitioned (Zenith2/Docs/
///   Design/UI.md "Thread affinity per type").
/// Invalidation: References returned by the getters are valid until the system is destroyed.
/// Units: None.
/// Errors: Create returns Zenith_UIError (UI_ERROR_INVALID_DESC, UI_ERROR_OUT_OF_MEMORY, UI_ERROR_RUNTIME_BACKEND) after
///   unwinding; SetLocale returns UI_ERROR_LOCALE_UNSUPPORTED for a locale the project does not list.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_UISystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UISystem>, Zenith_UIError> Create(const Zenith_UISystemDesc& xDesc);
	~Zenith_UISystem();

	Zenith_TextLayoutEngine& GetText();
	Zenith_GlyphCache& GetGlyphCache();
	Zenith_Localization& GetLocalization();
	const Zenith_UIConfig& GetConfig() const;
	bool IsHeadless() const;

	Zenith_Result<void, Zenith_UIError> SetLocale(Zenith_StringView strLocaleTag);   // prologue; async table load
	void ApplyConfig(const Zenith_UIConfig& xConfig);                                             // prologue step 3
	void BeginFrame(uint64 ulFrameIndex, uint64 ulMonotonicUs);   // Platform.Pump claimant callback, first of the frame
	void EndFrame();                                                // after every extractor of the frame: atlas uploads
	uint64 GetResidentBytes() const;                 // the memory consumer's host bytes
	uint64 GetDeviceLocalBytes() const;              // atlas pages
	void OnPressureLevelChanged(Zenith_MemoryPressureLevel eOld, Zenith_MemoryPressureLevel eNew);
private:
	struct PrivateTag;
public:
	Zenith_UISystem(PrivateTag, const Zenith_UISystemDesc& xDesc);
};
```

`EndFrame` runs at the start of the next frame's prologue (the first claimant callback calls `EndFrame` for the previous frame and then `BeginFrame`), because no frame-graph node of the module runs after every extractor (Zenith2/Docs/Design/UI/Text.md "Page upload and residency").

## Zenith_UIContext header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIContext.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_Heap.h"           // Zenith_Heap (Zenith_UIContextDesc::m_pxHeap)
#include "ZenithUI/Zenith_UITypes.h"
#include "ZenithUI/Zenith_UIComponents.h"     // Zenith_UIScreenSpace (Zenith_UIScreenState::m_eSpace)
#include "ZenithUI/Zenith_UIInput.h"
#include "ZenithUI/Zenith_UIDrawList.h"
#include "ZenithUI/Zenith_UIDocument.h"
#include "ZenithUI/Zenith_UIStyle.h"          // Zenith_UIComputedStyle (GetComputedStyle)
#include "ZenithUI/Zenith_UIText.h"           // Zenith_TextLayout (GetTextLayout)
#include "ZenithUI/Zenith_UIAccessibility.h"

class Zenith_UISystem;

/// @contract value Zenith_UISurfaceDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Pixels for the surface and the safe area; m_fDisplayScale is pixels per dp (Zenith2/Docs/Design/PLAT/Platform.md
///   "Units points pixels and dp").
/// Errors: SetSurface clamps a zero extent to 1 × 1 and a non-finite or non-positive scale to 1 (counted).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UISurfaceDesc
{
	uint32 m_uWidthPixels = 1920;
	uint32 m_uHeightPixels = 1080;
	float m_fDisplayScale = 1.0f;
	Zenith_UIEdges m_xSafeAreaPixels;
};

/// @contract value Zenith_UIContextDesc
/// Ownership: Plain value; m_pxSystem and m_pxHeap are borrowed and outlive the context.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Counts.
/// Errors: Validated by Create (UI_ERROR_INVALID_DESC: null system or heap, zero limits, m_uContextTag 0).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIContextDesc
{
	Zenith_UISystem* m_pxSystem = nullptr;
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_UISurfaceDesc m_xSurface;
	uint16 m_uContextTag = 1;             // distinct per live context in a process; world contexts use the world tag
	uint32 m_uMaxScreens = 64;
	uint32 m_uMaxElements = 65536;
	uint32 m_uMaxEventsPerFrame = 64;
};

/// @contract value Zenith_UIScreenState
/// Ownership: Plain value: the presentation-relevant fields of a Zenith.UI.Screen component, or a tool's own values.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: As Zenith_UIScreenComponent.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIScreenState
{
	sint32 m_iLayer = 0;
	bool m_bVisible = true;
	bool m_bInteractive = true;
	bool m_bBlocksInput = false;
	Zenith_UIScreenSpace m_eSpace = UI_SCREEN_SPACE_SCREEN;
	Zenith_Vector2 m_xWorldSizeMetres{ 1.0f, 1.0f };
	float m_fPixelsPerMetre = 512.0f;
	Zenith_Matrix4 m_xWorldMatrix;         // UI_SCREEN_SPACE_WORLD: the interpolated entity transform of the frame
	Zenith_StringID m_xInitialFocus;
	uint32 m_uStateSerial = 0;
};

/// @contract value Zenith_UIScreenDesc
/// Ownership: Plain value; m_xDocument is copied (one counted reference held by the screen).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: OpenScreen returns UI_ERROR_SCREEN_LIMIT at m_uMaxScreens, UI_ERROR_ELEMENT_LIMIT when instantiation would exceed
///   m_uMaxElements; a document that is not ACTIVE yet opens a pending screen that instantiates when it becomes ACTIVE.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIScreenDesc
{
	Zenith_AssetHandle<Zenith_UIDocumentAsset> m_xDocument;
	Zenith_UIScreenState m_xState;
	uint64 m_ulOwnerID = 0;               // the screen entity's stable simulation ID in world contexts; tools any value
};

/// @contract value Zenith_UIFrameParams
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Microseconds of presentation time (the monotonic clock), never simulation ticks.
/// Errors: None; a delta above 250,000 µs is clamped to it (a resume or a breakpoint).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIFrameParams
{
	uint64 m_ulFrameIndex = 0;
	uint64 m_ulPresentationTimeUs = 0;
	uint64 m_ulDeltaUs = 0;
	uint64 m_ulSimulationTick = 0;        // the world's latest tick, for caption lifetimes
	Zenith_Matrix4 m_xMainViewProjection;  // the world's main camera this frame, for world-space hit testing next frame
	bool m_bHasMainCamera = false;
};

/// @contract value Zenith_UIElementLayout
/// Ownership: Plain value returned by GetElementLayout.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Describes the layout of the last Update.
/// Units: Canvas units of the screen; m_fCanvasScale pixels per canvas unit.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIElementLayout
{
	Zenith_UIRect m_xBorderBox;            // relative to the screen canvas origin, before visual transforms
	Zenith_UIRect m_xContentBox;
	Zenith_UIRect m_xClipRect;             // the effective clip; equal to the canvas when unclipped
	Zenith_UISize m_xScrollExtent;         // scroll containers: the content size; 0 otherwise
	float m_fCanvasScale = 1.0f;
	bool m_bVisible = false;               // after visibility, opacity 0 counts as visible
	bool m_bFocused = false;
};

/// @contract value Zenith_UIContextStats
/// Ownership: Plain value snapshot.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Describes the last Update.
/// Units: Counts; microseconds.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIContextStats
{
	uint32 m_uScreens = 0;
	uint32 m_uElements = 0;
	uint32 m_uElementsLaidOut = 0;        // elements whose layout ran this Update (incremental layout)
	uint32 m_uTextLayouts = 0;            // paragraphs shaped this Update (cache misses)
	uint32 m_uTextCacheHits = 0;
	uint32 m_uQuads = 0;
	uint32 m_uBatches = 0;
	uint32 m_uVisibleGlyphs = 0;
	uint32 m_uMissingGlyphs = 0;          // glyphs drawn as nothing this frame because their page is not resident
	uint64 m_ulUpdateUs = 0;
	uint64 m_ulLayoutUs = 0;
	uint64 m_ulDrawListUs = 0;
};

/// @contract type Zenith_UIContext
/// Ownership: Created by Create as a Zenith_Owned; a world's context is owned by the UI module's per-world state, a tool's or
///   test's by its creator. Owns its screen instances, their element tables, presentation state (hover, press, focus,
///   scroll, animations, text-field buffers) and its event queue; holds counted references to documents, fonts and textures.
/// Thread affinity: OWNER_THREAD. Ownership moves only at frame-graph edges: the main thread in Platform.Pump (input feed),
///   then the world's UI extractor node (Update, BuildDrawList, BuildA11yTree), then the main thread of the next prologue.
/// Invalidation: Spans returned by DrainEvents, BuildDrawList and BuildA11yTree live in the caller's arena; screen handles
///   go stale at CloseScreen; element refs of recycled list items go stale at recycling.
/// Units: Pixels at the input feed; canvas units in layouts.
/// Errors: Zenith_UIError as each member states; input for an unknown pointer or a stale screen is ignored and counted.
/// Reentrancy: NONE. The context invokes no callback.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_UIContext
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UIContext>, Zenith_UIError> Create(const Zenith_UIContextDesc& xDesc);
	~Zenith_UIContext();

	// Screens
	Zenith_Result<Zenith_UIScreenHandle, Zenith_UIError> OpenScreen(const Zenith_UIScreenDesc& xDesc);
	// Preview screens (the editor model, tests): compiled .zui section bytes validated exactly as Zenith_UIDocumentLoader
	// validates a cooked file and copied into the context; dependencies (fonts, textures) are resolved by GUID through the
	// asset registry like a loaded document's; xDesc.m_xDocument is ignored. UI_ERROR_DOCUMENT_CORRUPT on invalid bytes.
	Zenith_Result<Zenith_UIScreenHandle, Zenith_UIError> OpenScreenFromBytes(Zenith_ByteSpan xCompiledSections,
		const Zenith_UIScreenDesc& xDesc);
	void CloseScreen(Zenith_UIScreenHandle xScreen);
	Zenith_Result<void, Zenith_UIError> SetScreenState(Zenith_UIScreenHandle xScreen, const Zenith_UIScreenState& xState);
	Zenith_Result<void, Zenith_UIError> SetBinding(Zenith_UIScreenHandle xScreen, uint32 uSlot, const Zenith_UIValue& xValue);
	Zenith_Result<uint32, Zenith_UIError> FindBindingSlot(Zenith_UIScreenHandle xScreen, Zenith_StringView strName) const;
	Zenith_Result<Zenith_UIElementRef, Zenith_UIError> FindElement(Zenith_UIScreenHandle xScreen, Zenith_StringView strID) const;
	uint32 GetScreenCount() const;

	// Surface and settings
	void SetSurface(const Zenith_UISurfaceDesc& xSurface);                 // relayouts every screen at the next Update
	void SetAccessibility(const Zenith_UIAccessibilitySettings& xSettings);

	// Input feed (main thread, inside Platform.Pump)
	Zenith_UIHit HitTest(Zenith_Vector2 xPixel) const;       // against the last Update's layout
	bool FeedPointer(const Zenith_UIPointerInput& xInput);   // true: the context consumed it
	bool FeedNavigation(const Zenith_UINavInput& xInput);    // true: a focus scope consumed it
	void FeedText(const Zenith_UITextInput& xInput);
	Zenith_Result<void, Zenith_UIError> RequestFocus(Zenith_UIElementRef xElement);
	void ClearFocus();
	Zenith_UIElementRef GetFocused() const;
	bool HasFocusScope() const;                               // an interactive screen holds focus
	bool IsTextEntryActive() const;                           // a text field is being edited
	Zenith_UIRect GetTextEntryAreaPixels() const;             // the IME candidate area

	// Frame (the world's UI extractor node)
	void Update(const Zenith_UIFrameParams& xParams);
	void BuildDrawList(Zenith_UIDrawList& xOut, Zenith_Arena& xArena) const;
	void BuildA11yTree(Zenith_UIA11yTree& xOut, Zenith_Arena& xArena) const;
	Zenith_Span<const Zenith_UIEventRecord> DrainEvents(Zenith_Arena& xArena);
	Zenith_Span<const Zenith_UINarrationRequest> DrainNarration(Zenith_Arena& xArena);   // main thread, Pump:
	                                                                       // reads the context-owned narration ring (Accessibility.md "Narration composition")
	void StageAccessibilityPublication(const Zenith_UIA11yTree& xTree);   // extractor: copy into the publish back buffer
	void SwapAccessibilityBuffers();                                       // main thread, Pump: back becomes front
	const Zenith_UIA11yTree& GetPublishedAccessibilityTree() const;   // the front buffer, until the next swap

	// Queries (tests, the editor, accessibility validation)
	Zenith_Result<Zenith_UIElementLayout, Zenith_UIError> GetElementLayout(Zenith_UIElementRef xElement) const;
	Zenith_Result<Zenith_UIComputedStyle, Zenith_UIError> GetComputedStyle(Zenith_UIElementRef xElement) const;
	Zenith_Result<const Zenith_TextLayout*, Zenith_UIError> GetTextLayout(Zenith_UIElementRef xElement) const;
	Zenith_UIContextStats GetStats() const;
private:
	struct PrivateTag;
public:
	Zenith_UIContext(PrivateTag, const Zenith_UIContextDesc& xDesc);
};

```

## Zenith_UIInput header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIInput.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithUI/Zenith_UITypes.h"
#include "ZenithUI/Zenith_UIEvents.h"         // Zenith_UIPublicationRecord (Zenith_UIEventRecord::m_xRecord)

/// @contract enum Zenith_UIPointerPhase
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_UIPointerPhase : uint8
{
	UI_POINTER_PHASE_MOVE = 1, UI_POINTER_PHASE_PRESS, UI_POINTER_PHASE_RELEASE, UI_POINTER_PHASE_CANCEL,
	UI_POINTER_PHASE_SCROLL, UI_POINTER_PHASE_LEAVE,
};

/// @contract value Zenith_UIPointerInput
/// Ownership: Plain value built by the platform bridge from Zenith_InputTransition records, or by a test.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Pixels of the context's surface; m_xScroll in wheel notches (x, y) or pixels for touch pans.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIPointerInput
{
	Zenith_UIPointerPhase m_ePhase = UI_POINTER_PHASE_MOVE;
	uint8 m_uPointer = 0;                 // 0 mouse; 1–10 touch points (PLAT touch index + 1); 11 pen
	uint8 m_uButton = 0;                  // mouse: 0 primary, 1 secondary, 2 middle
	bool m_bTouch = false;
	Zenith_Vector2 m_xPixel;
	Zenith_Vector2 m_xScroll;
	uint64 m_ulTimestampUs = 0;
};

/// @contract enum Zenith_UINavDirection
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_UINavDirection : uint8
{
	UI_NAV_UP = 1, UI_NAV_DOWN, UI_NAV_LEFT, UI_NAV_RIGHT, UI_NAV_NEXT, UI_NAV_PREVIOUS,
	UI_NAV_SUBMIT, UI_NAV_CANCEL, UI_NAV_SCROLL,
};

/// @contract value Zenith_UINavInput
/// Ownership: Plain value built from the presentation action states of Engine.UI (UI.Navigate, UI.Submit, UI.Cancel,
///   UI.TabNext, UI.TabPrevious, UI.Scroll; Zenith2/Docs/Design/PLAT/Input.md "Engine actions and contexts").
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: m_xScroll in UI.Scroll's axis units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UINavInput
{
	Zenith_UINavDirection m_eDirection = UI_NAV_UP;
	bool m_bRepeat = false;                // generated by the hold-repeat timer, not by a press
	Zenith_Vector2 m_xScroll;
};

/// @contract value Zenith_UITextInput
/// Ownership: Plain value; m_strText views the platform's frame text arena (valid until the next Pump).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: m_strText until the next Pump.
/// Units: Cursor and selection in code points from the composition start; m_uKey a Zenith_InputKey value.
/// Errors: None; m_strText is valid UTF-8 (PLAT validated it).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UITextInput
{
	uint8 m_uKind = 0;                    // 1 commit, 2 composition, 3 composition end, 4 key, 5 paste
	Zenith_StringView m_strText;
	sint32 m_iCursor = 0;
	sint32 m_iSelectionLength = 0;
	uint16 m_uKey = 0;                    // kind 4: the key (Backspace, Delete, Left, Right, Home, End, Return, A, C, X, V)
	uint8 m_uModifiers = 0;               // bit 0 shift, 1 control, 2 alt, 3 command
	bool m_bRepeat = false;
};

/// @contract value Zenith_UIHit
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Describes the layout of the last Update.
/// Units: m_xCanvas in the hit screen's canvas units.
/// Errors: None; IsHit() is false when no interactive screen lies under the pixel.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIHit
{
	Zenith_UIElementRef m_xElement;        // the deepest hit-testable element, or the screen when only its background is hit
	Zenith_Vector2 m_xCanvas;
	bool m_bHit = false;
	bool m_bBlocking = false;              // the screen blocks input below it
	bool IsHit() const { return m_bHit; }
};

/// @contract value Zenith_UIEventRecord
/// Ownership: Plain value in the drain arena; string views point into that arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the drain arena.
/// Units: As Zenith_UIPublicationRecord.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIEventRecord
{
	Zenith_UIPublicationRecord m_xRecord;  // m_ulScreenSimulationID is the screen's m_ulOwnerID
	Zenith_UIScreenHandle m_xScreen;
	uint8 m_uPresentationAction = 0;      // 0 none; 1 resumeWorld: an Activated event of a button whose TYPEDATA
	                                       //   presentationAction is resumeWorld, or a Cancelled event of a screen whose header
	                                       //   byte 23 (cancelAction) is resumeWorld; set by the context when it produces the
	                                       //   event; ClaimActions runs Interaction.md "Pause menus" for 1 (never published)
};
```

## Zenith_UIDrawList header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIDrawList.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithFlux/Flux_UI.h"

/// @contract value Zenith_UIDrawBatch
/// Ownership: Plain value in the caller's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the arena.
/// Units: Scissor in pixels of the surface (screen space) or of the panel canvas (world space).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
using Zenith_UIDrawBatch = Flux_UIBatch;

/// @contract value Zenith_UIWorldPanel
/// Ownership: Plain value in the caller's arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: With the arena.
/// Units: m_xCanvasToWorld maps canvas pixels (x right, y down) to world metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIWorldPanel
{
	Zenith_Matrix4 m_xCanvasToWorld;
	uint32 m_uFirstBatch = 0;
	uint32 m_uBatchCount = 0;
	float m_fSortDepth = 0.0f;             // view-space distance of the panel centre; filled by the GPU side per view
};

/// @contract value Zenith_UIDrawList
/// Ownership: Plain value; spans in the caller's arena (Zenith_UIContext::BuildDrawList).
/// Thread affinity: IMMUTABLE once built; members non-const: its producer default-constructs it and fills it member by
///   member.
/// Invalidation: With the arena.
/// Units: Pixels.
/// Errors: None; a list that would exceed uFLUX_UI_MAX_QUADS_PER_LIST is truncated at a batch boundary in paint order and
///   counted (ui.draw.quads_truncated).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIDrawList
{
	Zenith_Span<const Flux_UIQuad> m_axQuads;
	Zenith_Span<const Flux_UIBatch> m_axScreenBatches;   // screen-space screens, paint order
	Zenith_Span<const Flux_UIBatch> m_axWorldBatches;    // world-space panels' batches
	Zenith_Span<const Zenith_UIWorldPanel> m_axWorldPanels;
	uint32 m_uSurfaceWidth = 0;
	uint32 m_uSurfaceHeight = 0;
};
```

## Zenith_UIDocument header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIDocument.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Heap.h"          // Zenith_Heap (Zenith_UIDocumentModel::Create)
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Array.h"         // Zenith_Array<uint8> (Zenith_CompileUIDocument)
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithUI/Zenith_UITypes.h"

/// @contract enum Zenith_UIElementType
/// Ownership: Plain enumeration; the "type" member of an element and the type byte of a cooked element record.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked record with another value is UI_ERROR_DOCUMENT_CORRUPT.
/// Version: since Z2 Stage 3 (types 1–13); 14–16 since Z2 Stage 4. Values are stable (cooked documents store them).
/// @endcontract
enum Zenith_UIElementType : uint8
{
	UI_ELEMENT_PANEL = 1, UI_ELEMENT_TEXT = 2, UI_ELEMENT_IMAGE = 3, UI_ELEMENT_BUTTON = 4, UI_ELEMENT_TOGGLE = 5,
	UI_ELEMENT_SLIDER = 6, UI_ELEMENT_TEXT_FIELD = 7, UI_ELEMENT_SCROLL = 8, UI_ELEMENT_LIST = 9, UI_ELEMENT_PROGRESS = 10,
	UI_ELEMENT_CHOICE = 11, UI_ELEMENT_INPUT_PROMPT = 12, UI_ELEMENT_INSTANCE = 13, UI_ELEMENT_REBIND = 14,
	UI_ELEMENT_CAPTION_AREA = 15, UI_ELEMENT_ON_SCREEN_CONTROL = 16,
};

/// @contract value Zenith_UIDocumentInfo
/// Ownership: Plain value read from a cooked document's header section.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Canvas units for the reference size.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIDocumentInfo
{
	Zenith_PersistentID m_xID;
	uint32 m_uElementCount = 0;
	uint32 m_uBindingCount = 0;
	uint32 m_uTemplateCount = 0;
	uint8 m_uScaleMode = 0;               // 0 dp, 1 reference
	float m_fReferenceWidth = 0.0f;
	float m_fReferenceHeight = 0.0f;
	float m_fMatch = 0.5f;
};

/// @contract type Zenith_UIDocumentAsset
/// Ownership: The runtime object of asset kind ASSET_KIND_UI_DOCUMENT (Zenith2/Docs/Design/ASSET.md "Asset kinds", row 13),
///   created by UI's kind loader from a cooked .zui; reached through Zenith_AssetHandle<Zenith_UIDocumentAsset>; immutable;
///   owns its decoded sections in the general heap.
/// Thread affinity: IMMUTABLE once ACTIVE; members non-const: its loader writes them after constructing the object;
///   readable from any thread.
/// Invalidation: Pointers and spans it returns are valid while a handle to it is held.
/// Units: As the cooked format (Zenith2/Docs/Design/UI/Documents.md "Cooked UI document format").
/// Errors: Load failures are the loader's (Zenith2/Docs/Design/UI.md "Asset kind loaders"); accessors cannot fail.
/// Version: since Z2 Stage 3. Cooked format version 1.
/// @endcontract
class Zenith_UIDocumentAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_UI_DOCUMENT;
	const Zenith_UIDocumentInfo& GetInfo() const;
	Zenith_Span<const Zenith_AssetGUID> GetDependencies() const;   // fonts, textures, instances
	Zenith_StringView GetString(uint32 uStringIndex) const;       // the document's string pool
	uint32 FindElement(Zenith_StringView strID) const;            // 0xFFFFFFFF when absent
	uint32 FindBindingSlot(Zenith_StringView strName) const;      // 0xFFFFFFFF when absent
	Zenith_ByteSpan GetSection(uint16 uSectionKind) const;        // the runtime backend's view
private:
	friend class Zenith_UIDocumentLoader;
	void* m_pData = nullptr;
};

/// @contract value Zenith_UIDocumentDiagnostic
/// Ownership: Plain value; m_strMessage and m_strPointer view the model's diagnostic arena.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: Until the next Parse, Validate or Compile on the same model.
/// Units: m_uLine 1-based line of the source document; 0 when unknown.
/// Errors: None.
/// Version: since Z2 Stage 3. Message texts are part of the cook report and golden-tested.
/// @endcontract
struct Zenith_UIDocumentDiagnostic
{
	uint8 m_uSeverity = 0;                // 1 info, 2 warning, 3 error
	uint16 m_uRule = 0;                   // the rule number of Documents.md "Validation rules"
	uint32 m_uLine = 0;
	Zenith_StringView m_strPointer;        // JSON pointer of the offending member
	Zenith_StringView m_strMessage;
};

class Zenith_UIDocumentModel;   // declared below; Zenith_UIValidationContext names it before its declaration

/// @contract value Zenith_UIValidationContext
/// Ownership: Plain value; the resolver callbacks and their user data are borrowed for the Validate call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. The callbacks never call back into the model.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIValidationContext
{
	// Resolves an authoring path to an asset GUID and kind; false when unknown (validation rule 31).
	bool (*m_pfnResolvePath)(Zenith_StringView strPath, Zenith_AssetGUID& xGUIDOut, Zenith_AssetKind& eKindOut, void* pUserData) = nullptr;
	// Resolves a component stable name and property name for a binding (rule 40); false when unknown.
	bool (*m_pfnResolveProperty)(Zenith_StringView strComponent, Zenith_StringView strProperty, uint64& ulTypeIDOut,
		uint32& uPropertyIDOut, uint8& uValueTypeOut, void* pUserData) = nullptr;
	// True when the source locale table defines the key (rule 50).
	bool (*m_pfnHasLocalizationKey)(Zenith_StringView strKey, void* pUserData) = nullptr;
	void* m_pUserData = nullptr;
	const Zenith_UIDocumentModel* m_pxTheme = nullptr;   // the resolved theme document, for class and token checks
};

/// @contract type Zenith_UIDocumentModel
/// Ownership: Created by Create as a Zenith_Owned by the cook, the editor or a test; owns a parsed authoring document (UI,
///   theme), its element tree in the general heap, and its diagnostics.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Element indices returned by members are invalid after a structural edit (Zenith_UIEditorModel applies edits).
/// Units: As the authoring schema.
/// Errors: Parse returns UI_ERROR_AUTHORING_INVALID with the first error's rule in m_uDetail, every diagnostic kept;
///   UI_ERROR_DOCUMENT_VERSION for a schemaVersion newer than the build ([Z2-N-CONTRACT-537]).
/// Version: since Z2 Stage 3. Tool-facing source API; also linked in Players for development live preview of loose documents.
/// @endcontract
class Zenith_UIDocumentModel
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UIDocumentModel>, Zenith_UIError> Create(Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_UIError> Parse(const Zenith_JsonDocument& xDocument, Zenith_StringView strPath);
	Zenith_Result<void, Zenith_UIError> Validate(const Zenith_UIValidationContext& xContext);
	Zenith_Span<const Zenith_UIDocumentDiagnostic> GetDiagnostics() const;
	bool IsTheme() const;
	uint32 GetElementCount() const;
	Zenith_PersistentID GetElementUID(uint32 uElement) const;
	uint32 FindElementByUID(Zenith_PersistentID xUID) const;      // 0xFFFFFFFF when absent
	Zenith_Result<void, Zenith_UIError> WriteJson(Zenith_JsonDocument& xOut) const;   // members in canonical order
};

/// @contract functions Zenith_UIDocumentCompile
/// Ownership: Writes the cooked bytes into the caller's array.
/// Thread affinity: ANY_THREAD; the model is read-only for the call.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: UI_ERROR_AUTHORING_INVALID when the model has an error diagnostic; UI_ERROR_ELEMENT_LIMIT above the cooked limits;
///   UI_ERROR_OUT_OF_MEMORY; on failure xOut keeps its previous content.
/// Version: since Z2 Stage 3. Deterministic: the same model gives byte-identical output (Zenith2/Docs/Design/UI/Documents.md
///   "Cooked UI document format").
/// @endcontract
Zenith_Result<void, Zenith_UIError> Zenith_CompileUIDocument(const Zenith_UIDocumentModel& xModel,
	const Zenith_UIValidationContext& xContext, Zenith_Array<uint8>& xSectionsOut);
```

`Zenith_CompileUIDocument` writes the section payloads, and the cook step wraps them in ASSET's cooked file through `Zenith_CookedFileWriter` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_CookedFile header"`).

## Zenith_UIStyle header

```cpp
// Zenith2/Engine/ZenithUI/Zenith_UIStyle.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithUI/Zenith_UITypes.h"
#include "ZenithUI/Zenith_UIText.h"

/// @contract enum Zenith_UIStyleProperty
/// Ownership: Plain enumeration; the property IDs of the style catalogue (Zenith2/Docs/Design/UI/Documents.md
///   "Style property catalogue"), stored in cooked style blocks.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As the catalogue's unit column.
/// Errors: None; an unknown ID in a cooked block is UI_ERROR_DOCUMENT_CORRUPT.
/// Version: since Z2 Stage 3; IDs are never reused, a removed property's ID stays reserved.
/// @endcontract
enum Zenith_UIStyleProperty : uint16
{
	// box and sizing
	UI_STYLE_WIDTH = 1, UI_STYLE_HEIGHT = 2, UI_STYLE_MIN_WIDTH = 3, UI_STYLE_MIN_HEIGHT = 4, UI_STYLE_MAX_WIDTH = 5,
	UI_STYLE_MAX_HEIGHT = 6, UI_STYLE_MARGIN = 7, UI_STYLE_PADDING = 8, UI_STYLE_BORDER_WIDTH = 9, UI_STYLE_CORNER_RADIUS = 10,
	UI_STYLE_ASPECT_RATIO = 11,
	// stack item
	UI_STYLE_GROW = 16, UI_STYLE_SHRINK = 17, UI_STYLE_BASIS = 18, UI_STYLE_ALIGN_SELF = 19,
	// anchored positioning
	UI_STYLE_POSITION = 20, UI_STYLE_ANCHOR_MIN = 21, UI_STYLE_ANCHOR_MAX = 22, UI_STYLE_PIVOT = 23, UI_STYLE_OFFSET_MIN = 24,
	UI_STYLE_OFFSET_MAX = 25,
	// grid item
	UI_STYLE_GRID_COLUMN = 26, UI_STYLE_GRID_ROW = 27, UI_STYLE_GRID_COLUMN_SPAN = 28, UI_STYLE_GRID_ROW_SPAN = 29,
	// sibling paint and hit-test order (Rendering.md "Paint order")
	UI_STYLE_Z_INDEX = 30,
	// container layout
	UI_STYLE_LAYOUT = 40, UI_STYLE_DIRECTION = 41, UI_STYLE_JUSTIFY = 42, UI_STYLE_ALIGN_ITEMS = 43, UI_STYLE_WRAP = 44,
	UI_STYLE_GAP = 45, UI_STYLE_ROW_GAP = 46, UI_STYLE_GRID_COLUMNS = 47, UI_STYLE_GRID_ROWS = 48,
	UI_STYLE_LAYOUT_DIRECTION = 49,   // inherited; Layout.md "Logical direction and mirroring"
	// visual
	UI_STYLE_BACKGROUND_COLOUR = 64, UI_STYLE_BACKGROUND_GRADIENT = 65, UI_STYLE_BACKGROUND_IMAGE = 66,
	UI_STYLE_BACKGROUND_SLICE = 67, UI_STYLE_BORDER_COLOUR = 68, UI_STYLE_SHADOW_COLOUR = 69, UI_STYLE_SHADOW_OFFSET = 70,
	UI_STYLE_SHADOW_BLUR = 71, UI_STYLE_OPACITY = 72, UI_STYLE_TRANSLATE = 73, UI_STYLE_SCALE = 74, UI_STYLE_ROTATE = 75,
	UI_STYLE_TRANSFORM_ORIGIN = 76, UI_STYLE_CLIP = 77, UI_STYLE_TINT = 78, UI_STYLE_IMAGE_FIT = 79,
	UI_STYLE_POINTER_EVENTS = 80, UI_STYLE_FOCUS_RING_COLOUR = 81, UI_STYLE_FOCUS_RING_WIDTH = 82,
	UI_STYLE_FOCUS_RING_OFFSET = 83, UI_STYLE_MIRROR_IN_RTL = 84,
	// text
	UI_STYLE_FONT_FAMILY = 96, UI_STYLE_FONT_SIZE = 97, UI_STYLE_FONT_WEIGHT = 98, UI_STYLE_FONT_STYLE = 99,
	UI_STYLE_TEXT_COLOUR = 100, UI_STYLE_LINE_HEIGHT = 101, UI_STYLE_LETTER_SPACING = 102, UI_STYLE_TEXT_ALIGN = 103,
	UI_STYLE_TEXT_WRAP = 104, UI_STYLE_TEXT_OVERFLOW = 105, UI_STYLE_MAX_LINES = 106, UI_STYLE_TEXT_OUTLINE_COLOUR = 107,
	UI_STYLE_TEXT_OUTLINE_WIDTH = 108, UI_STYLE_TEXT_SHADOW_COLOUR = 109, UI_STYLE_TEXT_SHADOW_OFFSET = 110,
	UI_STYLE_TEXT_DIRECTION = 111, UI_STYLE_TABULAR_NUMBERS = 112, UI_STYLE_UNDERLINE = 113, UI_STYLE_TEXT_SCALE_POLICY = 114,
	// motion
	UI_STYLE_TRANSITION = 128, UI_STYLE_ANIMATION = 129, UI_STYLE_ESSENTIAL_MOTION = 130,
};

/// @contract enum Zenith_UILayoutDirection
/// Ownership: Plain enumeration; the keyword of style property 49 (layoutDirection) in a cooked style block, and the resolved
///   direction of an element after cascade step 9 (Zenith2/Docs/Design/UI/Layout.md "Cascade order"), which is LTR or RTL only.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index other than 0, 1 or 2 is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UILayoutDirection : uint8
{
	UI_LAYOUT_DIRECTION_AUTO = 0,   // authored only: resolves to the parent's direction, at a screen root to the locale's
	UI_LAYOUT_DIRECTION_LTR = 1,
	UI_LAYOUT_DIRECTION_RTL = 2,
};

/// @contract enum Zenith_UIStateFlags
/// Ownership: Plain enumeration of bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_UIStateFlags : uint16
{
	UI_STATE_HOVER = 1u << 0, UI_STATE_PRESSED = 1u << 1, UI_STATE_FOCUSED = 1u << 2, UI_STATE_FOCUS_VISIBLE = 1u << 3,
	UI_STATE_DISABLED = 1u << 4, UI_STATE_CHECKED = 1u << 5, UI_STATE_SELECTED = 1u << 6, UI_STATE_EDITING = 1u << 7,
	UI_STATE_INVALID = 1u << 8,
};

/// @contract enum Zenith_UIEasing
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; values stable.
/// @endcontract
enum Zenith_UIEasing : uint8
{
	UI_EASING_LINEAR = 0, UI_EASING_EASE_IN, UI_EASING_EASE_OUT, UI_EASING_EASE_IN_OUT, UI_EASING_STEP,
};

// The keyword enumerations below each equal the keyword's index in the property's grammar of Documents.md "Style property
// catalogue", the word a cooked style block stores ("Style block encoding", value kind 3), so decoding is a range check and
// a cast; alignItems (43) has no auto in its grammar, so its keyword index k decodes to Zenith_UIAlign(k + 1).
/// @contract enum Zenith_UILengthUnit
/// Ownership: Plain enumeration; the unit of a resolved Zenith_UILength (value kinds 1 and 2, or the keywords auto, minContent, maxContent and none of properties 1 to 6 and 18).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable.
/// @endcontract
enum Zenith_UILengthUnit : uint8
{
	UI_LENGTH_UNIT_AUTO = 0, UI_LENGTH_UNIT_LENGTH = 1, UI_LENGTH_UNIT_PERCENT = 2, UI_LENGTH_UNIT_MIN_CONTENT = 3,
	UI_LENGTH_UNIT_MAX_CONTENT = 4, UI_LENGTH_UNIT_NONE = 5,
};

/// @contract enum Zenith_UILayoutKind
/// Ownership: Plain enumeration; the keyword of property 40 (layout).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UILayoutKind : uint8 { UI_LAYOUT_KIND_STACK = 0, UI_LAYOUT_KIND_GRID = 1, UI_LAYOUT_KIND_OVERLAY = 2 };

/// @contract enum Zenith_UIStackDirection
/// Ownership: Plain enumeration; the keyword of property 41 (direction), the authored LTR value (Layout.md "Logical direction and mirroring").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIStackDirection : uint8
{
	UI_STACK_DIRECTION_ROW = 0, UI_STACK_DIRECTION_COLUMN = 1, UI_STACK_DIRECTION_ROW_REVERSE = 2,
	UI_STACK_DIRECTION_COLUMN_REVERSE = 3,
};

/// @contract enum Zenith_UIJustify
/// Ownership: Plain enumeration; the keyword of property 42 (justify).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIJustify : uint8
{
	UI_JUSTIFY_START = 0, UI_JUSTIFY_CENTRE = 1, UI_JUSTIFY_END = 2, UI_JUSTIFY_SPACE_BETWEEN = 3, UI_JUSTIFY_SPACE_AROUND = 4,
	UI_JUSTIFY_SPACE_EVENLY = 5,
};

/// @contract enum Zenith_UIAlign
/// Ownership: Plain enumeration; the keyword of property 19 (alignSelf, index = enumerator) and of property 43 (alignItems, index k = enumerator k + 1).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIAlign : uint8
{
	UI_ALIGN_AUTO = 0, UI_ALIGN_START = 1, UI_ALIGN_CENTRE = 2, UI_ALIGN_END = 3, UI_ALIGN_STRETCH = 4, UI_ALIGN_BASELINE = 5,
};

/// @contract enum Zenith_UIPosition
/// Ownership: Plain enumeration; the keyword of property 20 (position).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIPosition : uint8 { UI_POSITION_FLOW = 0, UI_POSITION_ANCHORED = 1 };

/// @contract enum Zenith_UIImageFit
/// Ownership: Plain enumeration; the keyword of property 79 (imageFit).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIImageFit : uint8 { UI_IMAGE_FIT_FILL = 0, UI_IMAGE_FIT_CONTAIN = 1, UI_IMAGE_FIT_COVER = 2, UI_IMAGE_FIT_NONE = 3 };

/// @contract enum Zenith_UIPointerEvents
/// Ownership: Plain enumeration; the keyword of property 80 (pointerEvents), read by hit testing (Interaction.md "Hit testing").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a cooked keyword index outside the enumeration is UI_ERROR_DOCUMENT_CORRUPT at load.
/// Version: since Z2 Stage 3; values stable (cooked style blocks store the keyword index, equal to the enumerator).
/// @endcontract
enum Zenith_UIPointerEvents : uint8 { UI_POINTER_EVENTS_AUTO = 0, UI_POINTER_EVENTS_NONE = 1, UI_POINTER_EVENTS_PASS_THROUGH = 2 };

/// @contract value Zenith_UILength
/// Ownership: Plain value; a resolved length of properties 1 to 6 and 18.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: m_fValue in canvas units (UI_LENGTH_UNIT_LENGTH) or percent of the containing block (UI_LENGTH_UNIT_PERCENT); unused for the other units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UILength
{
	float m_fValue = 0.0f;
	Zenith_UILengthUnit m_eUnit = UI_LENGTH_UNIT_AUTO;
};

/// @contract value Zenith_UIStyleSpan
/// Ownership: Plain value naming a run of 4-byte words inside the document's STYLES section (Documents.md "Style block encoding"): the grid track lists of properties 47 and 48 (two words per track: kind, then f32) and the property-ID list of property 128 (one word per property ID).
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Valid while the element's screen holds its document.
/// Units: Words.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIStyleSpan
{
	uint32 m_uFirstWord = 0;              // byte offset ÷ 4 of the first word inside the STYLES section
	uint16 m_uCount = 0;                  // tracks or property IDs; 0: the property's default (gridColumns ["1fr"], no
	                                       //   explicit rows, no transition)
};

/// @contract value Zenith_UIImageSource
/// Ownership: Plain value; the image reference of property 66 in the TYPEDATA image convention (Documents.md "Typed data records").
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: m_auRect in texels.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIImageSource
{
	uint32 m_uReference = 0xFFFFFFFFu;    // 0xFFFFFFFF none; bit 31 set: a STRINGS index of a sprite name in bits 0–30;
	                                       //   clear: a DEPENDENCIES index of a texture
	uint16 m_auRect[4] = {};              // texels x, y, w, h of a texture; all 0: the whole texture (sprites carry their own)
};

/// @contract value Zenith_UIGradient
/// Ownership: Plain value; property 65 (backgroundGradient).
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Linear colours.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIGradient
{
	Zenith_Colour m_xFrom;
	Zenith_Colour m_xTo;
	uint8 m_uAxis = 0;                    // 0 x, 1 y
	bool m_bSet = false;                   // false: none (the default)
};

/// @contract value Zenith_UIComputedStyle
/// Ownership: Plain value: the resolved style of one element after the cascade, tokens, palette and accessibility scaling
///   (Zenith2/Docs/Design/UI/Layout.md "Style resolution"), written by the shell's resolver for both D-SEL-UIRT candidates.
///   It has one field for every property of Zenith2/Docs/Design/UI/Documents.md "Style property catalogue" (the text
///   properties in m_xText or in the paragraph fields below), so it is the only per-element style storage.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Describes the last Update; m_xGridColumns, m_xGridRows and m_xTransition as Zenith_UIStyleSpan.
/// Units: Canvas units for lengths; linear colours; degrees for m_fRotateDegrees; seconds for m_fTransitionSeconds.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_UIComputedStyle
{
	// box and sizing (1–11)
	Zenith_UILength m_xWidth;              // 1; auto
	Zenith_UILength m_xHeight;             // 2; auto
	Zenith_UILength m_xMinWidth{ 0.0f, UI_LENGTH_UNIT_LENGTH };    // 3
	Zenith_UILength m_xMinHeight{ 0.0f, UI_LENGTH_UNIT_LENGTH };   // 4
	Zenith_UILength m_xMaxWidth{ 0.0f, UI_LENGTH_UNIT_NONE };      // 5
	Zenith_UILength m_xMaxHeight{ 0.0f, UI_LENGTH_UNIT_NONE };     // 6
	Zenith_UIEdges m_xMargin;              // 7
	Zenith_UIEdges m_xPadding;             // 8
	Zenith_UIEdges m_xBorderWidth;         // 9
	float m_afCornerRadius[4] = {};        // 10: top-left, top-right, bottom-right, bottom-left
	float m_fAspectRatio = 0.0f;           // 11; 0 none
	// stack item (16–19)
	float m_fGrow = 0.0f;                  // 16
	float m_fShrink = 1.0f;                // 17
	Zenith_UILength m_xBasis;              // 18; auto
	Zenith_UIAlign m_eAlignSelf = UI_ALIGN_AUTO;   // 19
	// anchored positioning (20–25)
	Zenith_UIPosition m_ePosition = UI_POSITION_FLOW;   // 20
	Zenith_Vector2 m_xAnchorMin;           // 21: [0, 0]
	Zenith_Vector2 m_xAnchorMax;           // 22: [0, 0]
	Zenith_Vector2 m_xPivot{ 0.5f, 0.5f }; // 23
	Zenith_Vector2 m_xOffsetMin;           // 24: [0, 0]
	Zenith_Vector2 m_xOffsetMax{ 100.0f, 100.0f };   // 25
	// grid item (26–29)
	uint16 m_uGridColumn = 0;             // 26: 0 auto, else the 1-based column
	uint16 m_uGridRow = 0;                // 27
	uint8 m_uGridColumnSpan = 1;          // 28: 1 … 64
	uint8 m_uGridRowSpan = 1;             // 29
	// paint order (30)
	sint16 m_iZIndex = 0;                 // −1,000 … 1,000: sibling paint and hit-test order
	// container layout (40–49)
	Zenith_UILayoutKind m_eLayout = UI_LAYOUT_KIND_STACK;              // 40
	Zenith_UIStackDirection m_eDirection = UI_STACK_DIRECTION_COLUMN;  // 41
	Zenith_UIJustify m_eJustify = UI_JUSTIFY_START;                    // 42
	Zenith_UIAlign m_eAlignItems = UI_ALIGN_STRETCH;                   // 43; never UI_ALIGN_AUTO
	bool m_bWrap = false;                  // 44
	float m_fGap = 0.0f;                   // 45
	float m_fRowGap = 0.0f;                // 46; "gap" resolves to m_fGap at resolution
	Zenith_UIStyleSpan m_xGridColumns;     // 47
	Zenith_UIStyleSpan m_xGridRows;        // 48
	Zenith_UILayoutDirection m_eLayoutDirection = UI_LAYOUT_DIRECTION_LTR;   // 49 resolved: LTR or RTL, never AUTO
	// visual (64–84)
	Zenith_Colour m_xBackground;           // 64
	Zenith_UIGradient m_xBackgroundGradient;   // 65
	Zenith_UIImageSource m_xBackgroundImage;   // 66
	uint16 m_auBackgroundSlice[4] = { 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu };   // 67: l, t, r, b texels; all 0xFFFF the sprite's own
	Zenith_Colour m_xBorderColour;         // 68
	Zenith_Colour m_xShadowColour;         // 69
	Zenith_Vector2 m_xShadowOffset;        // 70
	float m_fShadowBlur = 0.0f;            // 71
	float m_fOpacity = 1.0f;               // 72, multiplied down the tree
	Zenith_Vector2 m_xTranslate;           // 73
	Zenith_Vector2 m_xScale{ 1.0f, 1.0f }; // 74
	float m_fRotateDegrees = 0.0f;         // 75
	Zenith_Vector2 m_xTransformOrigin{ 0.5f, 0.5f };   // 76, fractions of the border box
	bool m_bClip = false;                  // 77
	Zenith_Colour m_xTint{ 1.0f, 1.0f, 1.0f, 1.0f };   // 78
	Zenith_UIImageFit m_eImageFit = UI_IMAGE_FIT_FILL;  // 79
	Zenith_UIPointerEvents m_ePointerEvents = UI_POINTER_EVENTS_AUTO;   // 80, inherited
	Zenith_Colour m_xFocusRingColour;      // 81, inherited; the theme's focusRing.colour by default
	float m_fFocusRingWidth = 0.0f;        // 82, inherited; the theme's by default, × ui.access.focusRingScale (cascade step 8)
	float m_fFocusRingOffset = 0.0f;       // 83, inherited
	bool m_bMirrorInRtl = false;           // 84: swap an image's u0 and u1 when m_eLayoutDirection is RTL
	// text (96–114)
	Zenith_TextStyle m_xText;              // 96–102, 107–110, 112, 113 (the font stack of the fontFamily tokens; size × text scale)
	Zenith_TextAlign m_eTextAlign = TEXT_ALIGN_START;            // 103
	Zenith_TextWrap m_eTextWrap = TEXT_WRAP_WORD;                // 104
	Zenith_TextOverflow m_eTextOverflow = TEXT_OVERFLOW_VISIBLE; // 105
	uint16 m_uMaxLines = 0;                                     // 106; 0 unbounded
	Zenith_TextDirection m_eTextDirection = TEXT_DIRECTION_AUTO; // 111
	bool m_bTextScaleFixed = false;                              // 114: false scale, true fixed
	// motion (128–130)
	Zenith_UIStyleSpan m_xTransition;      // 128: the transitioned property IDs
	float m_fTransitionSeconds = 0.0f;     // 128: duration (0 under reduced motion)
	Zenith_UIEasing m_eTransitionEasing = UI_EASING_LINEAR;   // 128
	uint32 m_uAnimation = 0;              // 129: STRINGS index of the animation name; 0 none
	bool m_bEssentialMotion = false;       // 130
	uint16 m_uState = 0;                  // Zenith_UIStateFlags in effect
};
```

