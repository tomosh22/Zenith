# EDIT design — ZenithEditor declarations

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It declares the internal types of the `ZenithEditor` executable that the other files name without declaring (the shell, panel and extension types, the picker, the overlay instances, the preview host and the cache consumer). The types of the tool library `ZenithEditorCore` are declared in `Zenith2/Docs/Design/EDIT/Declarations.md`; that file's "Declaration index" lists both files' types. The rules of `Zenith2/Docs/Design/EDIT/Declarations.md` → "Scope of this file" apply here: every fallible function returning `Zenith_Result<T, Zenith_EditorError>`, a contract block per type; these headers are `Internal/` headers of a tool, may include ImGui forward declarations, and carry contract blocks by EDIT convention.

## Scope of this file

| Topic | Heading |
|---|---|
| the internal headers of `Zenith2/Tools/ZenithEditor/` and the types they declare | "ZenithEditor internal headers" |

## ZenithEditor internal headers

| Header (`Zenith2/Tools/ZenithEditor/Internal/…`) | Declares | Behaviour specified in |
|---|---|---|
| `App/Zenith_EditorApp.h` | `Zenith_EditorAppDesc`, `Zenith_EditorApp` | Foundation.md "Startup sequence", "Editor frame"; Pipeline.md "Runtime restart" |
| `App/Zenith_EditorModuleCatalogue.h` | `Zenith_EditorModuleRow`, `Zenith_EditorECSBackendRow` | Foundation.md "Module catalogue"; § "ECS backend row" |
| `Shell/Zenith_EditorPanel.h` | `Zenith_EditorPanel`, `Zenith_EditorPanelTypeID`, `Zenith_EditorPanelInstancing`, `Zenith_EditorPanelTypeDesc`, `Zenith_PfnCreateEditorPanel`, `Zenith_EditorRect`, `Zenith_EditorPanelContext` | Panels.md "Panel interface"; § "Panel context header" |
| `Shell/Zenith_EditorNotifications.h` | `Zenith_EditorNotificationSeverity`, `Zenith_EditorNotifications` | Viewports.md "Notifications and status bar" |
| `Shell/Zenith_EditorDrawSnapshot.h` | `Zenith_EditorDrawCommand`, `Zenith_EditorDrawList`, `Zenith_EditorDrawSnapshot` | Viewports.md "ImGui renderer" |
| `Shell/Zenith_EditorTextureRef.h` | `Zenith_EditorTextureKind`, `Zenith_EditorTextureRef`, `Zenith_EditorTextureTable` | Viewports.md "Editor texture references" |
| `Shell/Zenith_EditorTextureBackend.h` | `Zenith_EditorTextureBackend`, `Zenith_EditorFluxTextureBackend` | § "Texture backend header" |
| `Pipeline/Zenith_EditorAssetReloadSink.h` | `Zenith_EditorAssetReloadSink` | § "Asset reload sink header"; Pipeline.md "Reimport flow" |
| `Extensions/Zenith_EditorExtension.h` | `Zenith_EditorExtension`, `Zenith_EditorExtensionContext`, and the registration types of § "Extension registration types" | Panels.md "Extension interface" |
| `Viewport/Zenith_EditorPicker.h` | `Zenith_EditorPickShape`, `Zenith_EditorPickFilter`, `Zenith_EditorPickHit`, `Zenith_EditorPicker` | Viewports.md "Picking" |
| `Viewport/Zenith_EditorOverlaySnapshot.h` | `Zenith_EditorOverlayVertex`, `Zenith_EditorOverlayView`, `Zenith_EditorGridSettings`, `Zenith_EditorIconInstance`, `Zenith_EditorSelectedInstance` | Viewports.md "Overlay records" |
| `Pipeline/Zenith_EditorPreviewHost.h` | `Zenith_EditorPreviewKind`, `Zenith_EditorPreviewHost` | Panels.md "Preview host" |
| `Pipeline/Zenith_EditorCacheConsumer.h` | `Zenith_EditorCacheConsumer` | Panels.md "Thumbnail cache" |

### Application header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/App/Zenith_EditorApp.h
/// @contract value Zenith_EditorAppDesc
/// Ownership: Plain value built by Zenith_EditorMain; every pointer is borrowed for the app's lifetime.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorAppDesc
{
	const Zenith_EditorOptions* m_pxOptions = nullptr;
	const Zenith_CommandLine* m_pxCommandLine = nullptr;
	const Zenith_GameComposition* m_pxComposition = nullptr;   // built at startup step 4
	const Zenith_EditorHostPaths* m_pxPaths = nullptr;
	Zenith_Heap* m_pxRootHeap = nullptr;
	Zenith_LogRouter* m_pxLogRouter = nullptr;
	Zenith_OOMPressureFlag* m_pxOOMPressureFlag = nullptr;
	Zenith_StartupReport* m_pxReport = nullptr;
};

/// @contract type Zenith_EditorApp
/// Ownership: Created by Zenith_EditorMain as a Zenith_Owned; owns the runtime, the storage roots, the session, the cook
///   client, the host watchers (Foundation.md "Host watchers"; kept across RestartRuntime), the shell, the extension table and
///   every editor thread; destroyed by the shutdown sequence.
/// Thread affinity: MAIN_THREAD. The hook members run inside Zenith_RuntimeToolAccess::RunOneFrame on the main thread.
/// Invalidation: Every reference it hands out is invalid after its destruction; tool textures and world handles also after
///   RestartRuntime.
/// Units: Frame indices.
/// Errors: Create returns the failing startup step's error with its exit code in m_uDetail; RequestRuntimeRestart returns
///   EDITOR_ERROR_PLAY_STATE while a play world exists and EDITOR_ERROR_RUNTIME_RESTART when one is already requested.
/// Reentrancy: NONE. The hooks never call RunOneFrame; the history's Poll (inside the hook OnInputFrame) only requests a
///   restart; RestartRuntime is private and runs only from Run, after RunOneFrame returned (Foundation.md "Editor run loop").
/// Version: since Z2 Stage 3; the runtime restart since Z2 Stage 4.
/// @endcontract
class Zenith_EditorApp final : public Zenith_ToolFrameHooks, public Zenith_EditorRuntimeHost
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorApp>, Zenith_EditorError> Create(const Zenith_EditorAppDesc& xDesc);   // steps 5–15
	~Zenith_EditorApp();                                         // shutdown steps 1–10
	Zenith_ExitCode Run();                         // step 16: Foundation.md "Editor run loop"
	// Zenith_EditorRuntimeHost (Declarations.md "Session description"): sets the request; Run performs it between frames.
	Zenith_Result<void, Zenith_EditorError> RequestRuntimeRestart() override;
	Zenith_EditorRuntimeRestartState GetRuntimeRestartState() const override;
	void RequestExit(bool bSaveDirty);                           // starts the exit phase (Run step 5)
	// Zenith_ToolFrameHooks (Foundation.md "Runtime tool access"):
	void OnInputFrame(const Zenith_InputFrame& xFrame) override;
	void OnPrologueEnd() override;
	void OnWorldHalted(uint16 uWorldTag, uint64 ulTick, const Zenith_ScriptError& xError) override;
	void OnScriptModuleLive(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, uint32 uGeneration) override;
	void OnFrameEnd(uint64 ulFrameIndex) override;
	void OnSchemaWindowResult(const Zenith_SchemaWindowResult& xResult) override;   // records it for the schema model
private:
	// Pipeline.md "Runtime restart"; called by Run alone, between frames; a failure ends Run with exit code 1 (step 9).
	Zenith_Result<void, Zenith_EditorError> RestartRuntime(uint32 uFaultAtStep);
};
```

### ECS backend row

```cpp
// Zenith2/Tools/ZenithEditor/Internal/App/Zenith_EditorModuleCatalogue.h (beside Foundation.md "Module catalogue")
/// @contract value Zenith_EditorECSBackendRow
/// Ownership: constexpr data in Internal/App/Zenith_EditorModuleCatalogue.cpp, one row per ECS backend the editor links.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; a row is added in the diff that adds an ECS backend.
/// @endcontract
struct Zenith_EditorECSBackendRow
{
	const char* const m_szName = nullptr;   // "Custom", "Flecs", "EnTT": the composition record's "ecs" value
	const Zenith_PfnCreateWorld m_pfnCreateWorld = nullptr; // the backend's factory (Zenith2/Docs/Architecture/CompositionRoot.md "Runtime and entry types")
};
Zenith_Span<const Zenith_EditorECSBackendRow> Zenith_GetEditorECSBackends();
```

### Panel context header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorPanel.h (Zenith_EditorPanel itself: Panels.md "Panel interface")
/// @contract value Zenith_EditorPanelTypeID
/// Ownership: Plain value: Zenith_Hash64 of the panel type's stable name ("Zenith.Editor.Hierarchy").
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None within a build; the layout file stores the stable name, never the hash.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPanelTypeID { uint64 m_ulHash = 0; };

/// @contract enum Zenith_EditorPanelInstancing
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorPanelInstancing : uint8 { EDITOR_PANEL_SINGLE = 1, EDITOR_PANEL_MULTI };

class Zenith_EditorPanelContext;
using Zenith_PfnCreateEditorPanel = Zenith_Result<Zenith_Owned<Zenith_EditorPanel>, Zenith_EditorError> (*)(
	Zenith_EditorPanelContext& xContext, uint32 uInstance, Zenith_EditorDocumentID xDocument);

/// @contract value Zenith_EditorPanelTypeDesc
/// Ownership: constexpr data beside each panel; registered through Zenith_EditorExtensionContext::RegisterPanelType.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; registration refuses a duplicate stable name (EDITOR_ERROR_EXTENSION_DUPLICATE) and more than 256
///   types (EDITOR_ERROR_EXTENSION_LIMIT).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPanelTypeDesc
{
	const char* const m_szStableName = nullptr;  // "Zenith.Editor.Hierarchy"; never renamed (the layout file names it)
	const char* const m_szTitle = nullptr;       // "Hierarchy"
	const char* const m_szMenuPath = nullptr;    // "Window/Hierarchy", or nullptr for a document panel
	const Zenith_EditorPanelInstancing m_eInstancing = {};
	const Zenith_PfnCreateEditorPanel m_pfnCreate = nullptr;
};

/// @contract value Zenith_EditorRect
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: Pixels of the shell window, origin at its top-left, y down.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorRect { float m_fX = 0.0f, m_fY = 0.0f, m_fWidth = 0.0f, m_fHeight = 0.0f; };

/// @contract type Zenith_EditorPanelContext
/// Ownership: Built by the panel host once per editor frame from references the composition root injected into the shell;
///   passed by reference to Draw and to panel factories.
/// Thread affinity: MAIN_THREAD, editor frame step 2.
/// Invalidation: Valid for the call it is passed to; a panel never stores it or any reference it returns across frames.
/// Units: Pixels of the shell window for GetRect.
/// Errors: None of its own.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorPanelContext
{
public:
	Zenith_EditorSession& GetSession();
	Zenith_EditorCommandRegistry& GetCommands();
	Zenith_EditorNotifications& GetNotifications();
	Zenith_EditorTextureTable& GetTextures();
	Zenith_EditorCookClient* GetCookClient();          // nullptr with --no-cook-server
	Zenith_EditorPreviewHost& GetPreviewHost();
	Zenith_EditorPicker& GetPicker();
	const Zenith_ToolServices& GetServices() const;    // the current runtime's, re-injected after a restart
	Zenith_EditorRect GetRect() const;                 // the panel's content rectangle
	float GetDisplayScale() const;
	uint64 GetFrameIndex() const;
	bool IsHeadless() const;
	Zenith_Arena& GetFrameArena();                      // reset at the end of step 2
};
```

### Notifications header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorNotifications.h
/// @contract enum Zenith_EditorNotificationSeverity
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorNotificationSeverity : uint8 { EDITOR_NOTIFICATION_INFO = 1, EDITOR_NOTIFICATION_WARNING, EDITOR_NOTIFICATION_ERROR };

/// @contract type Zenith_EditorNotifications
/// Ownership: Owned by the shell; owns at most 256 retained notifications (oldest dropped) in a ring.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Text views returned by List are invalid after the next Post.
/// Units: Milliseconds.
/// Errors: None; text longer than 1,024 bytes is truncated at a UTF-8 boundary.
/// Reentrancy: NONE. Post publishes EDITOR_EVENT_NOTIFICATION, dispatched at the next Dispatch.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorNotifications
{
public:
	// Durations: 0 uses the severity's default (info 4,000 ms, warning 8,000 ms, error until dismissed); returns an ID.
	uint64 Post(Zenith_EditorNotificationSeverity eSeverity, Zenith_StringView strText, uint32 uDurationMs = 0);
	void Dismiss(uint64 ulID);
	void Draw(uint64 ulNowUs);                    // at most 8 visible toasts; older fold into "n more"
	uint32 GetVisibleCount() const;
};
```

### Draw snapshot header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorDrawSnapshot.h
struct ImDrawData;    // ImGui's; the header includes no ImGui header

/// @contract value Zenith_EditorDrawCommand
/// Ownership: Plain value in a snapshot slot.
/// Thread affinity: IMMUTABLE once the slot is published; members non-const: written field by field into a reused slot before it is published.
/// Invalidation: Invalid when its slot is reused (frame k + 2).
/// Units: Pixels for the clip rectangle; indices.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorDrawCommand
{
	float m_afClipRect[4];                 // x0, y0, x1, y1 in display pixels
	Zenith_EditorTextureRef m_xTexture;
	uint32 m_uIndexOffset;
	uint32 m_uIndexCount;
	uint32 m_uVertexOffset;
};

/// @contract value Zenith_EditorDrawList
/// Ownership: Plain value: ranges into its slot's vertex, index and command arrays.
/// Thread affinity: IMMUTABLE once published; members non-const: written field by field into a reused slot before it is published.
/// Invalidation: As Zenith_EditorDrawCommand.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorDrawList { uint32 m_uFirstVertex, m_uVertexCount, m_uFirstIndex, m_uIndexCount, m_uFirstCommand, m_uCommandCount; };

/// @contract type Zenith_EditorDrawSnapshot
/// Ownership: Owned by the shell; owns two slots, each at most editor.shell.maxDrawBytes (default 32 MiB) of 20-byte vertices,
///   16-bit indices and commands.
/// Thread affinity: ANY_THREAD_EXCEPT(Capture). Capture runs on the main thread in hook OnInputFrame; GetSlot is read on the
///   Flux.BuildGraph node of frame k, which reads only the slot of k.
/// Invalidation: The spans of slot k mod 2 are invalid after Capture of frame k + 2.
/// Units: Bytes; frame indices.
/// Errors: None returned; a capture over the cap drops whole draw lists from the end and counts editor.shell.draw_lists_dropped.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorDrawSnapshot
{
public:
	void Capture(const ImDrawData& xDrawData, uint64 ulFrameIndex);
	void Discard(uint64 ulFrameIndex);                      // headless: the slot is marked empty
	Zenith_Span<const Zenith_EditorDrawList> GetLists(uint64 ulFrameIndex) const;
	Zenith_ByteSpan GetVertices(uint64 ulFrameIndex) const;
	Zenith_Span<const uint16> GetIndices(uint64 ulFrameIndex) const;
	Zenith_Span<const Zenith_EditorDrawCommand> GetCommands(uint64 ulFrameIndex) const;
	float GetDisplayScale(uint64 ulFrameIndex) const;
};
```

### Texture backend header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorTextureBackend.h
/// @contract interface Zenith_EditorTextureBackend
/// Ownership: The seam through which Zenith_EditorTextureTable (Viewports.md "Editor texture references") reaches the renderer;
///   implemented by Zenith_EditorFluxTextureBackend (this header, forwarding each member to Flux_Renderer) and, in tests, by the
///   header-only Zenith_EditorRecordingTextureBackend (Tools/ZenithEditor/Tests/Support/Zenith_EditorRecordingTextureBackend.h,
///   X184), which records every call with its frame index and hands out bindless indices from a counter it never reuses; borrowed
///   by the table for its lifetime and re-pointed after a runtime restart (Zenith_EditorTextureTable::RebindBackend).
/// Thread affinity: Create, Update, Destroy on the main thread (editor frame step 2 and step 6); GetSRV and GetAssetSRV on the
///   Flux.BuildGraph node of frame f, reading only what the main thread published before RunFrame.
/// Invalidation: A handle is invalid after Destroy; its SRV stays valid until the renderer's deferred deletion retires it
///   ([Z2-N-REND-488]).
/// Units: Pixels.
/// Errors: Create returns EDITOR_ERROR_TEXTURE_LIMIT when the renderer refuses (its error logged), EDITOR_ERROR_PREVIEW_UNAVAILABLE
///   with no renderer (headless).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorTextureBackend
{
public:
	virtual ~Zenith_EditorTextureBackend() = default;
	virtual Zenith_Result<Flux_ToolTextureHandle, Zenith_EditorError> Create(const Flux_ToolTextureDesc& xDesc) = 0;
	virtual Zenith_Result<void, Zenith_EditorError> Update(Flux_ToolTextureHandle xTexture, Zenith_ByteSpan xPixels) = 0;
	virtual void Destroy(Flux_ToolTextureHandle xTexture) = 0;                 // Flux_Renderer::DestroyToolTexture
	virtual Zenith_RHIDescriptorHandle GetSRV(Flux_ToolTextureHandle xTexture) const = 0;
	virtual Zenith_RHIDescriptorHandle GetAssetSRV(const Zenith_AssetHandle<Flux_TextureResource>& xTexture) const = 0;   // GetSRV() of the resource; the null handle when not resident
};

/// @contract type Zenith_EditorFluxTextureBackend
/// Ownership: Owned by the shell; borrows the runtime's Flux_Renderer (re-pointed by RebindRenderer after a restart).
/// Thread affinity: As Zenith_EditorTextureBackend.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: As Zenith_EditorTextureBackend.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorFluxTextureBackend final : public Zenith_EditorTextureBackend
{
public:
	explicit Zenith_EditorFluxTextureBackend(Flux_Renderer* pxRenderer);          // nullptr headless
	void RebindRenderer(Flux_Renderer* pxRenderer);
	// Each override forwards to the one Flux_Renderer member of row ED12 (a), and GetAssetSRV to Flux_TextureResource::GetSRV.
};
```

The production backend is a forwarding layer of one call per member, so a test of the table through the recording backend tests every decision the table makes (slot, generation, release frame, reclaim frame), and the render test `Test_EditorRender_StaleTextureShowsChecker` covers the forwarding on WARP and lavapipe.

### Asset reload sink header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Pipeline/Zenith_EditorAssetReloadSink.h
/// @contract interface Zenith_EditorAssetReloadSink
/// Ownership: The seam through which reimport, the published notification and the shader reload's material path reach the
///   runtime's asset registry (Pipeline.md "Reimport flow", "Asset reload"); implemented by Zenith_EditorRegistryReloadSink
///   (forwarding to Zenith_AssetRegistry::OnMountsChanged, ::GetState and ::RequestReload) and, in tests, by the header-only
///   Zenith_EditorRecordingReloadSink (Tools/ZenithEditor/Tests/Support/Zenith_EditorRecordingReloadSink.h, X184), which records
///   the calls in order and reports a scripted state per GUID; borrowed by Zenith_EditorReimport for its lifetime.
/// Thread affinity: MAIN_THREAD, editor frame step 2.
/// Invalidation: None.
/// Units: None.
/// Errors: RequestReload returns the registry's error unchanged (a failed reload keeps the old object, ASSET's rule).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorAssetReloadSink
{
public:
	virtual ~Zenith_EditorAssetReloadSink() = default;
	virtual void OnMountsChanged() = 0;
	virtual bool IsActive(const Zenith_AssetGUID& xAsset) const = 0;    // GetState == ACTIVE
	virtual void RequestReload(const Zenith_AssetGUID& xAsset) = 0;
};
```

### Extension registration types

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Extensions/Zenith_EditorExtension.h (beside Panels.md "Extension interface")
/// @contract enum Zenith_EditorPickShape
/// Ownership: Plain enumeration returned by a bounds provider.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorPickShape : uint8
{
	EDITOR_PICK_SHAPE_BOUNDS = 1,     // the AABB hit stands
	EDITOR_PICK_SHAPE_MESH,           // the precise mesh test of Viewports.md "Precise mesh picking"
	EDITOR_PICK_SHAPE_TERRAIN,        // the terrain march
	EDITOR_PICK_SHAPE_ICON,           // the icon sphere of radius 0.04 × camera distance
};

// The generic inspector is replaced, for one component type, by this draw function: it reads the component's document value
// through xContext and executes commands; it never mutates a document. Called in editor frame step 2.
using Zenith_PfnEditorInspector = void (*)(Zenith_EditorInspectorContext& xContext, const Zenith_EditorObjectRef& xComponent);
// World bounds of one entity's component for picking and framing (Viewports.md "CPU bounds picking"); false: no bounds.
using Zenith_PfnEditorBounds = bool (*)(const Zenith_WorldInspector& xWorld, Zenith_EntityHandle xEntity,
	const Zenith_Matrix4& xWorldMatrix, const Zenith_Vector3& xCameraPosition, Zenith_AABB& xBoundsOut,
	Zenith_EditorPickShape& eShapeOut);

/// @contract type Zenith_EditorInspectorContext
/// Ownership: Built by the inspector panel per component per frame; borrows the panel context and the component's document.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Valid for the call.
/// Units: None.
/// Errors: ExecuteSetProperty returns the command registry's error.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorInspectorContext
{
public:
	Zenith_EditorPanelContext& GetPanel();
	const Zenith_FieldSet& GetValue() const;          // the document value, gathered (ECS "Zenith_FieldSet header")
	bool IsReadOnly() const;
	bool IsPlayMode() const;                          // edits go to the play world
	Zenith_Result<uint64, Zenith_EditorError> ExecuteSetProperty(Zenith_StringView strPropertyPath,
		const Zenith_JsonDocument& xValue, Zenith_JsonNode xNode, uint64 ulMergeKey);
};

/// @contract value Zenith_EditorGizmoHandleDesc
/// Ownership: Plain value filled by a gizmo provider.
/// Thread affinity: IMMUTABLE; members non-const: a gizmo provider fills it in the caller's storage.
/// Invalidation: None.
/// Units: Metres in world space.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorGizmoHandleDesc
{
	Zenith_Vector3 m_xPosition;              // world position of the handle
	Zenith_Vector3 m_xAxis;                  // drag axis (unit), or zero for a screen-plane handle
	const char* m_szPropertyPath;            // the property the drag writes ("Radius", "OuterConeAngle")
	uint32 m_uColour;                       // sRGB RGBA8
};

/// @contract interface Zenith_EditorGizmoProvider
/// Ownership: A constexpr singleton per component type, registered through RegisterGizmoProvider; never freed.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Metres; radians.
/// Errors: None returned; a non-finite value from a drag is clamped and counted (editor.gizmo.clamped).
/// Reentrancy: NONE. A provider never executes a command; the gizmo system executes component.setProperty with its value.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorGizmoProvider
{
public:
	virtual ~Zenith_EditorGizmoProvider() = default;
	virtual uint32 GetHandleCount(const Zenith_FieldSet& xValue) const = 0;
	virtual void GetHandle(const Zenith_FieldSet& xValue, const Zenith_Matrix4& xWorldMatrix, uint32 uHandle,
		Zenith_EditorGizmoHandleDesc& xOut) const = 0;
	// The new property value for an axis drag of fDeltaMetres along the handle's axis, written as JSON into xOut.
	virtual void ComputeValue(const Zenith_FieldSet& xStartValue, uint32 uHandle, float fDeltaMetres,
		Zenith_JsonWriter& xOut) const = 0;
};

using Zenith_PfnEditorAssetOpen = Zenith_Result<void, Zenith_EditorError> (*)(Zenith_EditorPanelContext& xContext, const Zenith_AssetGUID& xAsset);
using Zenith_PfnEditorAssetCreate = Zenith_Result<uint64, Zenith_EditorError> (*)(Zenith_EditorPanelContext& xContext, Zenith_StringView strFolder);

/// @contract value Zenith_EditorAssetKindHandler
/// Ownership: constexpr data per asset kind, registered through RegisterAssetKindHandler.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorAssetKindHandler
{
	const Zenith_PfnEditorAssetOpen m_pfnOpen = nullptr;    // double-click in the asset browser
	const Zenith_PfnEditorAssetCreate m_pfnCreate = nullptr; // the "Create" menu; nullptr: not creatable
	const char* const m_szCreateMenuLabel = nullptr;
	const Zenith_EditorPreviewKind m_eThumbnailKind = {};   // EDITOR_PREVIEW_NONE: the kind icon
};

using Zenith_PfnEditorDocumentValidate = Zenith_Result<void, Zenith_EditorError> (*)(const Zenith_JsonDocument& xTree,
	Zenith_Array<char>& xMessageOut);

/// @contract value Zenith_EditorDocumentHandler
/// Ownership: constexpr data per document type, registered through RegisterDocumentHandler.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorDocumentHandler
{
	const char* const m_szPanelStableName = nullptr;        // the panel opening the document; nullptr: the JSON document editor
	const Zenith_PfnEditorDocumentValidate m_pfnValidate = nullptr; // run after every change; a rejection fails the command
	Zenith_EditorProjectionHandler* const m_pxProjection = nullptr; // registered with the session when non-null
};

/// @contract enum Zenith_EditorPreferenceKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorPreferenceKind : uint8 { EDITOR_PREFERENCE_BOOL = 1, EDITOR_PREFERENCE_INTEGER, EDITOR_PREFERENCE_NUMBER, EDITOR_PREFERENCE_STRING, EDITOR_PREFERENCE_ENUM };

/// @contract value Zenith_EditorPreferenceDesc
/// Ownership: constexpr data, registered through RegisterPreferences.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_szUnit's.
/// Errors: None of its own; registration refuses a key outside "editor.<extension>." (EDITOR_ERROR_EXTENSION_DUPLICATE for a
///   repeated key).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPreferenceDesc
{
	const char* const m_szKey = nullptr;     // "editor.animation.compileBudgetMs"
	const Zenith_EditorPreferenceKind m_eKind = {};
	const char* const m_szDefaultJson = nullptr;
	const double m_dMin = 0.0, m_dMax = 0.0; // numbers; ignored for other kinds
	const char* const m_szEnumValues = nullptr; // "unity|unreal" for EDITOR_PREFERENCE_ENUM
	const char* const m_szUnit = nullptr;
	const char* const m_szDescription = nullptr;
};

class Zenith_EditorAutomation;
using Zenith_PfnEditorAutomationStepStart = Zenith_Result<void, Zenith_EditorError> (*)(Zenith_EditorAutomation& xRunner,
	const Zenith_JsonDocument& xScript, Zenith_JsonNode xStep);
using Zenith_PfnEditorAutomationStepPoll = Zenith_EditorOperationStatus (*)(Zenith_EditorAutomation& xRunner);

/// @contract value Zenith_EditorAutomationStepDesc
/// Ownership: constexpr data, registered through RegisterAutomationStep.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; the runner validates a script's steps against m_axMembers before the first step
///   (EDITOR_ERROR_AUTOMATION_SCRIPT).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_EditorAutomationStepDesc
{
	const char* const m_szKind = nullptr;            // "auditionEvent"
	const Zenith_Span<const Zenith_EditorArgDesc> m_axMembers = {};
	const Zenith_PfnEditorAutomationStepStart m_pfnStart = nullptr;
	const Zenith_PfnEditorAutomationStepPoll m_pfnPoll = nullptr; // DONE, IN_FLIGHT, FAILED
};
```

### Picker header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Viewport/Zenith_EditorPicker.h
/// @contract value Zenith_EditorPickFilter
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPickFilter
{
	uint16 m_uWorldTag = 0;
	Zenith_Span<const Zenith_PersistentID> m_axExcluded;   // dragged entities and their descendants (surface snapping)
	bool m_bSkipLockedAndHidden = true;
	bool m_bPreciseMeshes = true;                          // false: the bounds phase alone
};

/// @contract value Zenith_EditorPickHit
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: m_xEntity is valid within the editor frame alone; m_xObject persists.
/// Units: Metres; t along the ray in metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPickHit
{
	Zenith_EditorObjectRef m_xObject;          // the entity (or its prefab instance root, per the click rule)
	Zenith_EntityHandle m_xEntity;
	float m_fT = 0.0f;
	Zenith_Vector3 m_xPoint;
	Zenith_Vector3 m_xNormal;                  // geometric normal; zero for bounds-only hits
	Zenith_EditorPickShape m_eShape = EDITOR_PICK_SHAPE_BOUNDS;
};

/// @contract type Zenith_EditorPicker
/// Ownership: Owned by the shell; owns the pick-mesh cache (Zenith_EditorPickMeshCache, LRU within Z2-B-EDIT-001) and the
///   bounds-provider table.
/// Thread affinity: MAIN_THREAD, between RunFrame calls (it reads world inspectors).
/// Invalidation: None.
/// Units: Metres; pixels for the marquee.
/// Errors: EDITOR_ERROR_NOT_FOUND when nothing is hit; EDITOR_ERROR_OUT_OF_MEMORY when a mesh decode cannot allocate (the
///   bounds hit stands).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorPicker
{
public:
	Zenith_Result<void, Zenith_EditorError> PickRay(const Zenith_Ray& xRay, const Zenith_EditorPickFilter& xFilter,
		Zenith_EditorPickHit& xOut);
	uint32 PickFrustum(const Zenith_Frustum& xFrustum, const Zenith_EditorPickFilter& xFilter,
		Zenith_Span<Zenith_EditorObjectRef> axOut);                 // marquee; returns the count written
	Zenith_Result<Zenith_AABB, Zenith_EditorError> GetWorldBounds(uint16 uWorldTag, const Zenith_PersistentID& xEntity,
		const Zenith_Vector3& xCameraPosition);
	void RegisterBoundsProvider(Zenith_TypeID xComponent, Zenith_PfnEditorBounds pfnBounds);
	void InvalidateMeshes(Zenith_Span<const Zenith_AssetGUID> axAssets);   // reimport
	uint64 ReleaseCaches(uint64 ulTargetBytes);            // the cache consumer; returns bytes released
};
```

### Overlay instance types

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Viewport/Zenith_EditorOverlaySnapshot.h (beside Viewports.md "Overlay records")
/// @contract value Zenith_EditorGridSettings
/// Ownership: Plain value per scene view in the overlay snapshot.
/// Thread affinity: IMMUTABLE once published; members non-const: written field by field into a reused slot before it is published.
/// Invalidation: As its snapshot slot.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorGridSettings
{
	bool m_bVisible = true;
	uint8 m_uPlane = 1;                 // 0: YZ (Left/Right views), 1: XZ (y = 0), 2: XY (Front/Back views)
	float m_fMinorSpacing = 1.0f;        // g of Viewports.md "Grid pass"
	float m_fFadeDistance = 500.0f;      // editor.grid.fadeDistance
};

/// @contract value Zenith_EditorIconInstance
/// Ownership: Plain value in the overlay snapshot.
/// Thread affinity: IMMUTABLE once published; members non-const: written field by field into a reused slot before it is published.
/// Invalidation: As its snapshot slot.
/// Units: Metres for the position; pixels for the size.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorIconInstance
{
	float m_fX, m_fY, m_fZ;              // world position
	uint16 m_uIcon;                     // index into the vector icon table of Internal/Shell/Zenith_EditorIcons.cpp
	uint16 m_uSizePixels;               // 24 × display scale
	uint32 m_uColour;                   // sRGB RGBA8 tint
};

/// @contract value Zenith_EditorSelectedInstance
/// Ownership: Plain value in the overlay snapshot; the mesh handle is a shared asset handle the snapshot pins for its slot.
/// Thread affinity: IMMUTABLE once published; members non-const: written field by field into a reused slot before it is published.
/// Invalidation: As its snapshot slot.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorSelectedInstance
{
	Zenith_AssetHandle<Flux_MeshResource> m_xMesh;
	uint8 m_uLOD = 0;
	uint16 m_uFirstSection = 0, m_uSectionCount = 0;
	uint32 m_uSkinnedPoolSlot = 0xFFFFFFFF;   // RPROD's skinned-vertex slot of the instance, or none (row ED12 (d))
	Zenith_Matrix4 m_xWorld;
	bool m_bActive = false;                    // the active entity: mask value 0.5
};
```

### Preview host header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Pipeline/Zenith_EditorPreviewHost.h
/// @contract enum Zenith_EditorPreviewKind
/// Ownership: Plain enumeration of Panels.md "Preview host".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorPreviewKind : uint8
{
	EDITOR_PREVIEW_NONE = 0, EDITOR_PREVIEW_MESH, EDITOR_PREVIEW_PREFAB, EDITOR_PREVIEW_MATERIAL, EDITOR_PREVIEW_VFX,
	EDITOR_PREVIEW_ANIMATED_CHARACTER, EDITOR_PREVIEW_TEXTURE,
};

/// @contract type Zenith_EditorPreviewHost
/// Ownership: Owned by the shell; owns the preview worlds (through the session's world set), the preview panels' viewport
///   output tool textures (registered with the texture table, which owns them) and the thumbnail job queue.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: A preview ID is invalid after ClosePreview or LRU eviction.
/// Units: Pixels; frames.
/// Errors: EDITOR_ERROR_PREVIEW_UNAVAILABLE (headless, or no renderer), EDITOR_ERROR_RUNTIME for a failed world creation,
///   EDITOR_ERROR_TEXTURE_LIMIT.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorPreviewHost
{
public:
	Zenith_Result<uint32, Zenith_EditorError> OpenPreview(Zenith_EditorPreviewKind eKind, const Zenith_AssetGUID& xAsset);
	void ClosePreview(uint32 uPreviewID);
	void SetVisible(uint32 uPreviewID, bool bVisible, uint32 uWidth, uint32 uHeight);
	Zenith_EditorTextureRef GetOutput(uint32 uPreviewID) const;
	uint16 GetWorldTag(uint32 uPreviewID) const;               // 0 for a texture preview
	void RequestThumbnail(const Zenith_AssetGUID& xAsset, Zenith_EditorPreviewKind eKind, uint64 ulCacheKey);
	// Step 6 (OnFrameEnd): pooling, thumbnails, readbacks, and the releases MarkForRelease asked for: each marked idle preview
	// world is destroyed (DestroyWorld is legal in OnFrameEnd, Foundation.md "Tool access calls per hook") and its output
	// released with Release(ref, ulFrameIndex + 1), because this frame's snapshot was captured in step 2 and may name it.
	void Update(uint64 ulFrameIndex);
	// The cache consumer, from the governor's callback in prologue step 4: marks least-recently-shown idle preview worlds and
	// decoded thumbnails until the marked bytes reach ulTargetBytes, destroys nothing, and returns the bytes marked.
	uint64 MarkForRelease(uint64 ulTargetBytes);
	void DestroyAll();                                                         // shutdown and runtime restart
};
```

### Cache consumer header

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Pipeline/Zenith_EditorCacheConsumer.h
/// @contract type Zenith_EditorCacheConsumer
/// Ownership: Owned by the shell; implements CORE's Zenith_MemoryConsumer for MEMORY_SUBSYSTEM_EDITOR_CACHES and is
///   registered with the runtime's Zenith_MemoryGovernor at startup step 12 (and again after a runtime restart), unregistered
///   before the shell is destroyed (shutdown step 8).
/// Thread affinity: ANY_THREAD for GetResidentBytes (relaxed reads); OnPressureLevelChanged on the main thread in prologue step 4
///   (the governor's rule), where no Zenith_RuntimeToolAccess call is legal (Foundation.md "Tool access calls per hook"): it
///   frees pick-mesh cache entries (CPU memory the picker touches only in editor step 2) at once, and only marks idle preview
///   worlds and decoded thumbnails through Zenith_EditorPreviewHost::MarkForRelease, which the preview host releases in the
///   same frame's step 6 (editor.caches.deferred_releases counts them); GetResidentBytes counts marked entries until released.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None; shedding never fails, it releases what it can.
/// Reentrancy: NONE. A shedding callback never calls the governor.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorCacheConsumer final : public Zenith_MemoryConsumer
{
public:
	Zenith_EditorCacheConsumer(Zenith_EditorPreviewHost& xPreviews, Zenith_EditorPicker& xPicker);
	// Zenith_MemoryConsumer (Zenith2/Docs/Architecture.md "Memory governor"):
	Zenith_MemorySubsystem GetSubsystem() const override;    // MEMORY_SUBSYSTEM_EDITOR_CACHES
	uint64 GetResidentBytes() const override;              // decoded thumbnails + idle previews + pick meshes
	// WARNING: free least-recently-used pick meshes and mark least-recently-used decoded thumbnails and idle preview worlds until
	// the resident bytes after the step-6 release are at most 50% of what they were at the transition; CRITICAL: free and mark all
	// of them and every in-memory derived-data cache.
	void OnPressureLevelChanged(Zenith_MemoryPressureLevel eOld, Zenith_MemoryPressureLevel eNew) override;
};
```

The GPU-backed entries are therefore released after the transition, in the same iteration (the callback runs in prologue step 4, the release in step 6), and `GetResidentBytes` reports them until then. The interface is Architecture's (`Zenith2/Docs/Architecture.md` → "Memory governor"); `GetDeviceLocalBytes` keeps its default 0 apart from the bytes of decoded thumbnail tool textures, which the consumer reports there. The shedding case is tested in `ZenithEditor_Tests` by `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` (ruling Q86 (b) for CB8), not in `Zenith2/Tests/Memory`.
