# EDIT design — shell, viewports, cameras, picking, gizmos and snapping

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies the editor shell (ImGui on PLAT's input and on Flux), the viewports and the views they request, the editor cameras, picking, the transform gizmos with their snapping mathematics, and the overlay passes. The editor renders through the same frame graph and render graph as a Player: it adds view requests through the render world and passes through `Flux_ViewExtension`, and never records a command list of its own ([Z2-N-REND-131] places the shader reload watcher in this tool; nothing else here reaches below the public Flux API).

## Scope of this file

| Topic | Heading |
|---|---|
| ImGui, input, fonts, notifications, shortcuts, the shell view | "Editor shell" |
| scene views, game views, preview views, the editor extractor | "Viewports" |
| the Unity and Unreal camera schemes, framing, bookmarks | "Editor cameras" |
| click, marquee and handle picking | "Picking" |
| translate, rotate and scale gizmos and their mathematics | "Gizmos" |
| grid, rotation, scale, surface and vertex snapping | "Snapping" |
| grid, outline, gizmo and icon passes | "Overlays" |

## Editor shell

### Shell layout and docking

The shell is one OS window (`Zenith_Window`, PLAT, `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API") holding one ImGui docking space. Panels dock inside it or float as ImGui windows inside it; the editor does not create further OS windows for panels (ImGui's multi-viewport platform windows are not enabled), so the renderer presents one swapchain. The menu bar, the toolbar (`Zenith2/Docs/Design/EDIT/Panels.md` → "Toolbar") and the status bar are fixed regions; the dock layout is saved per user (`Zenith2/Docs/Design/EDIT/Formats.md` → "Layout file") and the default layout is a constant table in `Internal/Shell/Zenith_EditorDockLayout.cpp`:

| Region | Default panels (tabs) |
|---|---|
| left, 18% width | Hierarchy, Project settings |
| centre | Scene view 1, Game view (one tab per play world) |
| right, 24% width | Inspector, Schema |
| bottom, 28% height | Asset browser, Console, Log, Diagnostics |

### ImGui platform adapter

<!-- elaboration -->
ImGui never sees an SDL or Win32 type: SDL3 stays inside `ZenithPlatform/Internal/` (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership"), so the editor uses no ImGui platform backend and feeds `ImGuiIO` from PLAT's presentation result in hook `OnInputFrame` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor frame"):

| ImGuiIO field | Source |
|---|---|
| `DisplaySize`, `DisplayFramebufferScale` | the window's pixel size and display scale (`Zenith_Window`) |
| `DeltaTime` | the editor frame's wall-clock delta from the platform's monotonic clock, clamped to [1 µs, 100 ms] |
| `AddMousePosEvent` | the frame's mouse position transitions, in window pixels |
| `AddMouseButtonEvent`, `AddMouseWheelEvent` | mouse button and wheel transitions, in timestamp order |
| `AddKeyEvent` | keyboard transitions mapped from PLAT's scancode controls to `ImGuiKey` by a constant table in `Internal/Shell/Zenith_EditorImGuiKeys.cpp`; repeats (`INPUT_TRANSITION_FLAG_REPEAT`) are delivered to ImGui, never to editor shortcuts |
| `AddInputCharactersUTF8` | the frame's committed text-input events (`Zenith2/Docs/Design/PLAT/Input.md` → "Text input API"); composition (IME pre-edit) is shown through `ImGuiPlatformImeData`, whose caret area the adapter reports to PLAT |
| `AddFocusEvent` | window focus events |
| clipboard callbacks | PLAT's clipboard (`Zenith2/Docs/Design/PLAT/Platform.md` → "Clipboard") |
| cursor shape | `ImGui::GetMouseCursor()` mapped to PLAT's system cursors each frame |

The adapter (`Internal/Shell/Zenith_EditorImGuiPlatformAdapter.h`, `Zenith_EditorImGuiPlatformAdapter`) borrows three objects of the runtime's `Zenith_Platform`: the main `Zenith_Window`, `Zenith_TextInput` (`GetTextInput()`) and `Zenith_Clipboard` (`GetClipboard()`). `ReleasePlatform()` clears all three (until the next `BindPlatform` the adapter feeds no input and its clipboard callbacks return empty text and write nothing), and `BindPlatform(Zenith_Platform&)` re-reads them; a runtime restart calls the first at its step 4 and the second at its step 7 (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"), because the platform is destroyed with the runtime. Text input is enabled on PLAT exactly while `io.WantTextInput` is true. A synthetic release on focus loss (PLAT's rule for held controls, `Zenith2/Docs/Design/PLAT/Input.md` → "Keyboard and mouse") reaches ImGui as an ordinary release.

### Editor input claimant

`Zenith_EditorInputClaimant` implements PLAT's `Zenith_InputClaimant` (`Zenith2/Docs/Design/PLAT/Input.md` → "Presentation claims"), registered after UI's claimant, as rows P15 and UI6 order them, and after DEVT's overlay adapter (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Overlay input"):

- `ClaimPointers`: claim every pointer press whose position is outside the rectangle of a game viewport that currently has input focus, using the previous frame's panel rectangles; so editor panels are never clicked through into the game.
- `ClaimActions`: claim every presentation action while an ImGui widget has keyboard focus (`io.WantCaptureKeyboard`), so typing in a text field never moves the player.
- A pointer press inside a focused game viewport is not claimed; UI's claimant, which runs first, receives it in the play world's UI surface coordinates (§ "Game viewport input").

### ImGui renderer

<!-- elaboration -->
The renderer is a `Flux_ViewExtension` (`Zenith2/Docs/Design/RPROD/Views.md` → "View extensions") at `FLUX_EXTENSION_POINT_OVERLAY` whose `GetViewKindMask` is `1 << VIEW_KIND_EDITOR`. RPROD calls `AddPasses` for every view of that kind, so the extension filters: `AddPasses` returns at once unless `xView.m_pxView->m_xViewID` equals the shell view's ID, `Flux_MakeViewID(VIEW_KIND_EDITOR, ulEDITOR_SHELL_OWNER_ID, 0)`, computed once at registration (§ "Shell view request"). The editor registers it with `Flux_Renderer::RegisterViewExtension` at its startup step 12, on the main thread with no frame running; RPROD's contract names startup step 15 (module start) as the registration point, and row ED33 asks RPROD to admit a tool composition root's registration between `Zenith_RuntimeToolAccess::Create` and the first `RunOneFrame`, which satisfies the renderer's thread-affinity rule ("at startup", main thread):

1. Snapshot. In hook `OnInputFrame` after `ImGui::Render`, `Zenith_EditorDrawSnapshot::Capture(ImDrawData*)` copies every draw list's vertices (`ImDrawVert`, 20 bytes), indices (16-bit, ImGui's default `ImDrawIdx`) and commands (clip rectangle, texture reference, index offset and count, vertex offset) into slot `k mod 2` of a two-slot ring, where k is the frame index the next `RunOneFrame` runs. Two slots suffice: the render subgraph of frame k runs inside the iteration of frame k + 1 (Z2-D-050), and the editor writes slot k + 2 only after the iteration of frame k + 1 returned. A capture larger than `editor.shell.maxDrawBytes` (default 32 MiB) drops whole draw lists from the end and counts `editor.shell.draw_lists_dropped`.
2. Passes. `AddPasses` of the shell view declares one raster pass `EditorImGui` on queue G with the view's output as its colour target (`LOAD`, `STORE`, no depth), program `Shaders/Editor/ImGui.slang`; it copies the slot's vertices and indices into the frame's upload ring and binds them through `Flux_RasterTargets` as a vertex stream and an index buffer (`Flux_VertexStreamRef`, `Flux_IndexBufferRef`, `Zenith2/Docs/Rendering.md` → "Render graph").
3. Parameters: `Flux_EditorImGuiParams { m_xProjection (float4x4, the orthographic map of display pixels to clip space), DescriptorHandle m_xTexture, uint32 m_uSrgbOutput }` per draw command; the texture handle is the command's `Zenith_EditorTextureRef` resolved at record time (§ "Editor texture references").
4. State: premultiplied-free ImGui blending (`SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA` for colour; `ONE`, `ONE_MINUS_SRC_ALPHA` for alpha), no culling, scissor per command, `Draw­Indexed` per command.
5. Colour: ImGui colours are sRGB; the shader converts them to linear before blending when the output is an sRGB target and writes them unchanged otherwise (`m_uSrgbOutput`), so the shell looks identical on both RHIs.

`Shaders/Editor/` is the editor's directory of `Zenith2/Shaders/`, compiled by ZenithShaderCompiler like every other directory, with its generated headers in `ZenithShadersGenerated`, which ZenithEditor therefore declares (cross-area row ED13).

### Editor texture references

ImGui identifies textures by an opaque 64-bit `ImTextureID`. The editor never stores a device view in it: the memory of an ImGui registration that baked a raw image view and drew whatever a recycled slot held is exactly the failure this design excludes. The texture table is therefore the **owner** of every texture it references, for both kinds: no slot is reused, and no device texture or bindless index it references is freed, until no snapshot that could resolve the old reference can still be recorded.

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorTextureRef.h
enum Zenith_EditorTextureKind : uint8
{
	EDITOR_TEXTURE_KIND_TOOL = 1,        // an editor-owned texture (font atlas, icons, thumbnails, viewport outputs), owned by the table
	EDITOR_TEXTURE_KIND_ASSET,           // a texture asset, pinned by the table's asset handle while referenced
};

/// @contract value Zenith_EditorTextureRef
/// Ownership: Plain value packed into ImTextureID: kind (8 bits), slot (24 bits), generation (32 bits).
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None: a reference whose slot was reclaimed resolves to the missing-texture checker, never to another texture.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorTextureRef { uint64 m_ulBits; };

/// @contract type Zenith_EditorTextureTable
/// Ownership: Owned by the shell; owns up to editor.shell.maxTextures (default 4,096) slots, each owning either a tool texture
///   (RegisterTool takes ownership of the Flux_ToolTextureHandle; the caller never destroys it) or an asset handle, with the
///   frame index of its release; borrows its Zenith_EditorTextureBackend (ToolDeclarations.md "Texture backend header"), the
///   one way it reaches the renderer.
/// Thread affinity: ANY_THREAD_EXCEPT(Resolve). RegisterTool, RegisterAsset, Release and CollectReleased run on the main thread
///   (editor frame steps 2 and 6); Resolve runs on the Flux.BuildGraph node of frame f, which only the iteration of frame f + 1 runs.
/// Invalidation: A released slot keeps resolving for the frames released before it (f < release frame) and is reclaimed, its
///   generation incremented, once no such frame can still be recorded.
/// Units: Frame indices.
/// Errors: EDITOR_ERROR_TEXTURE_LIMIT when every slot is in use or awaiting reclamation.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorTextureTable
{
public:
	explicit Zenith_EditorTextureTable(Zenith_EditorTextureBackend& xBackend, uint32 uMaxTextures);
	void RebindBackend(Zenith_EditorTextureBackend& xBackend);   // Pipeline.md "Runtime restart" step 7
	Zenith_Result<Zenith_EditorTextureRef, Zenith_EditorError> RegisterTool(Flux_ToolTextureHandle xTexture);
	Zenith_Result<Zenith_EditorTextureRef, Zenith_EditorError> RegisterAsset(
		Zenith_AssetHandle<Flux_TextureResource> xTexture);  // holds the handle, so the asset stays resident
	// Marks the slot; reclaims nothing yet. ulFirstFrameWithout is the first frame whose draw snapshot no longer names the
	// reference: the current frame index i when called in step 2 before the snapshot is captured, i + 1 when called after it
	// (step 6, a prologue-deferred release).
	void Release(Zenith_EditorTextureRef xRef, uint64 ulFirstFrameWithout);
	void CollectReleased(uint64 ulCurrentFrameIndex);      // step 2: reclaims slots released at least two frames ago
	// Record time: the bindless SRV of the referenced texture (GEOM's Flux_TextureResource::GetSRV() for an asset, the tool
	// texture's SRV for a tool texture), or the 8 × 8 magenta-black checker's for a reclaimed, unregistered or non-resident
	// reference (counted in editor.shell.texture_misses).
	Zenith_RHIDescriptorHandle Resolve(Zenith_EditorTextureRef xRef, uint64 ulFrameIndex) const;
	void ReleaseAll();                                      // runtime restart and shutdown: destroys every tool texture now
};
```

The frame arithmetic: the main thread writes the draw snapshot of frame i in editor frame step 2 of iteration i, and the render subgraph of frame i, which resolves its references, runs inside iteration i + 1 (Z2-D-050, § "ImGui renderer"). So:

1. `Release(ref, R)` stores release frame R: R = i when called in step 2 of iteration i before `ImGui::Render` (the reference is not in snapshot i), R = i + 1 when called after the snapshot of i was captured (step 6, the preview host's release of an evicted world's output). A reference released at R is absent from snapshot R and every one after it (the caller stopped drawing it before releasing it), but snapshot R − 1, recorded during iteration R, may still hold it: `Resolve(ref, f)` returns the slot's texture when the generation matches and the slot is live, or released with f < R, and the checker otherwise.
2. `CollectReleased(i)` in step 2 of iteration i reclaims every slot with R ≤ i − 2: by then the iteration of R − 1's render subgraph (iteration R) has returned. Reclaiming increments the slot's generation, so any reference to it resolves to the checker from then on, and then releases what the slot owns: a tool texture through the backend's `Destroy` (`Flux_Renderer::DestroyToolTexture` in production), whose deferred deletion keeps the device texture and its bindless index alive until the GPU has finished the frames that sampled it ([Z2-N-REND-488]); an asset handle by dropping it, which lets the asset system unload it under the same rule. `Resolve` therefore never needs to validate a device handle: the table is the only owner of every texture a live reference names, and nothing is destroyed while a recordable snapshot can name it.
3. An asset texture whose chain is swapped by streaming changes its `GetSRV()` at publication; `Resolve` reads `GetSRV()` at record time, and the previous descriptor stays valid until the frames that could reference it retire (`Zenith2/Docs/Design/GEOM/Textures.md` → "Flux_Texture header", the invalidation rule of `Flux_TextureResource`), so a swap never shows the checker.

Viewport outputs are recreated on every resize (§ "Viewport views"), in this order, all in step 2 of iteration i: (a) `CreateToolTexture` at the new size and `RegisterTool` → reference N; (b) the viewport stores N, so the view request staged this frame and the ImGui image drawn this frame both name N; (c) `Release(old reference, i)`. Snapshot i names only N, snapshot i − 1 names only the old reference, which stays resolvable until its reclamation at iteration i + 2.

- `Test_EditorTextureTable_StaleReferenceDrawsChecker` releases a reference, reclaims it, registers a new texture into the same slot, and asserts the old reference resolves to the checker and the new one to the new texture.
- `Test_EditorTextureTable_ResizeWhileDrawnNeverAliases` resizes a viewport every frame for 200 frames while the shell draws it, with the table constructed over the recording backend `Zenith_EditorRecordingTextureBackend` (the declared seam of ToolDeclarations.md "Texture backend header", no double of `Flux_Renderer`), which records every `Destroy` with its frame and every bindless index it hands out, and asserts that every `Resolve(ref, f)` returns either the texture that reference was registered with or the checker, never a texture registered after it into the same slot or a recycled index, and that every destroy happens at least two iterations after its release.

<!-- assumption: pending RPROD, GEOM (row ED12 (a)) -->
`Flux_ToolTextureHandle` (a generational device handle) and `Flux_Renderer::CreateToolTexture(const Flux_ToolTextureDesc&)` / `UpdateToolTexture` / `DestroyToolTexture` (a 2D texture with sampled and optionally render-target usage, created and filled from memory outside the asset system, released through the renderer's deferred deletion) are the renderer entry points the font atlas, the icons, the thumbnails and the viewport outputs need; cross-area row ED12 asks RPROD for them.
<!-- /assumption -->

### Shell view request

<!-- assumption: pending RPROD (row ED33); core wording X259 -->
Each frame the editor extractor (§ "Editor extractor") writes one `Flux_ViewRequest` for the shell: kind `VIEW_KIND_EDITOR`, owner ID the editor shell's constant `ulEDITOR_SHELL_OWNER_ID = 0x5A32454449545348` ("Z2EDITSH"), sub-index 0, `m_uEditorViewportID = uEDITOR_SHELL_VIEWPORT_ID = 0` (reserved: every other viewport ID starts at 1, § "Viewport kinds"), output `FLUX_VIEW_OUTPUT_SWAPCHAIN`, `m_uFeatureMaskClear` = every feature bit except `FLUX_VIEW_FEATURE_EDITOR_OVERLAY`. The core contract does not yet give such a view a composite-only meaning: `Zenith2/Docs/Rendering.md` → "Render views" defines `VIEW_KIND_EDITOR`'s passes as "the main-view frame plus editor overlays", and [Z2-N-REND-601] removes only the passes of cleared feature bits, so culling and the base passes would still run. Cross-doc item X259 (a) asks the core either to add the composite-only row (a request whose mask is exactly `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` runs a clear to the shell's background colour, the `FLUX_EXTENSION_POINT_OVERLAY` extensions and Present, with no culling, no scene pass and no history) or to rule another mechanism, such as an RPROD present-stage extension with no view; EDIT's shell renderer depends only on "an overlay-point extension pass whose colour target is the swapchain image", which either ruling provides. The shell's ImGui pass samples the viewport output textures of the same frame; the render graph orders their writers first because the shell pass declares them as SRV reads of the imported textures (the view-dependency array `m_axReadsViews` is for material sampling of texture views and is not used). Because `Zenith2/Docs/Rendering.md` → "Render views" orders editor views by ascending viewport ID and [Z2-N-REND-608] defers views beyond the tier's view-count cap (Z2-B-RPROD-022), the shell view takes the lowest ID, so it is first among editor views; cross-doc item X302 (with X259) asks the core that the shell view is never deferred by the cap (it counts against it and is always kept), since a dropped shell view is a blank editor window. X259 (b) aligns the core's owner-ID table ("the editor viewport's stable ID") with this request's hashed owner plus `m_uEditorViewportID`.
<!-- /assumption -->

### Fonts and DPI

- The shell uses two font families, a proportional sans and a monospace, both from the third-party `noto-fonts` data entry (row UI9; `Noto Sans` and `Noto Sans Mono`), read from the `thirdparty:` content root at startup; ImGui rasterises them into one atlas at pixel size `round(15 × displayScale)` (proportional) and `round(14 × displayScale)` (monospace), with the Latin, Latin Extended-A and General Punctuation ranges plus every code point of the game's localization string tables found by the localization project (so a localized label renders); the atlas is a tool texture.
- A display-scale change of the window (moving it to another monitor) rebuilds the atlas at the start of the next frame and scales ImGui's style (`ImGuiStyle::ScaleAllSizes`) from its unscaled copy, so sizes never compound.
- Icons are vector shapes drawn with ImGui draw-list primitives from a constant table (`Internal/Shell/Zenith_EditorIcons.cpp`: entity, prefab, prefab instance, component, scene, camera, light kinds, audio, script, material, mesh, texture, folder, warning, error, lock, cloud), so they stay sharp at every scale and need no image asset.

### Notifications and status bar

- `Zenith_EditorNotifications::Post(eSeverity, strText, ulDurationMs)` shows a toast in the lower right (info 4 s, warning 8 s, error until dismissed) and appends the text to the log panel; at most 8 are visible, older ones fold into "n more".
- The status bar shows: the play state, the cook server state (`starting`, `ready`, `busy n`, `restarting`, `content build in progress`, `unavailable`), the background operations with progress bars (`Zenith_OperationHandle` progress, Contracts' operation handles), the dirty-document count, the frame time and the memory governor level.

### Shortcuts

Every shortcut is a binding from a key chord to a command ID of the command registry (`Zenith2/Docs/Design/EDIT/Automation.md` → "Command registry"). The default map is a constant table; the user's overrides live in the preferences file (`Zenith2/Docs/Design/EDIT/Formats.md` → "Preferences file"). A chord is dispatched only when no ImGui widget wants the keyboard, and in a focused game viewport only the reserved play chords are (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Input routing during play").

| Chord | Command |
|---|---|
| `Ctrl+Z`, `Ctrl+Y`, `Ctrl+Shift+Z` | `edit.undo`, `edit.redo`, `edit.redo` |
| `Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+D`, `Delete` | `edit.copy`, `edit.cut`, `edit.paste`, `edit.duplicate`, `edit.delete` |
| `Ctrl+S`, `Ctrl+Shift+S` | `document.save`, `document.saveAll` |
| `W`, `E`, `R`, `T` (viewport focused) | `gizmo.translate`, `gizmo.rotate`, `gizmo.scale`, `gizmo.combined` |
| `X` | `gizmo.toggleSpace` (world and local) |
| `Ctrl` held during a drag | invert the snap toggle for the drag |
| `F` | `camera.frameSelection` |
| `Ctrl+P` / `F5`, `F6`, `F7`, `Shift+F5` | `play.toggle` / `play.start`, `play.pause`, `play.step`, `play.stop` |
| `Ctrl+Shift+P` | `editor.commandPalette` (every registered command by name) |
| `F3` | `devtools.toggleProfiler` (DEVT's `CycleState`, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Editor overlay") |

## Viewports

### Viewport kinds

| Kind | Shows | Camera | View kind | Count |
|---|---|---|---|---|
| scene view | an edit world (the main one, or a prefab stage) or a play world (the "eject" mode of a game viewport) | an editor camera (§ "Editor cameras") | `VIEW_KIND_EDITOR`, with the editor overlays | 1 … `editor.viewports.maxScene` (default 4) |
| game view | one play world, through its game's own active camera | the game's (RPROD's camera selection) | `VIEW_KIND_EDITOR` without the editor overlays, with `UI` | one per play world |
| preview | a preview world or a material preview scene | the preview's orbit camera | `VIEW_KIND_PREVIEW` | one per open preview panel |
| thumbnail | an asset, rendered once into the thumbnail cache | a framing camera | `VIEW_KIND_PREVIEW` | `editor.thumbnails.perFrame` (default 2) per frame |

Each viewport has a stable `uint32` viewport ID from the session's counter, which starts at 1 (0 is the shell's, § "Shell view request") and is never reused in a session, which is its view's `m_uEditorViewportID` and, with the owner ID, its `Flux_ViewID`, so each viewport keeps its own temporal histories and never disturbs another's ([Z2-N-REND-598]; `Test_View_EditorHistoryIndependentOfMain` is RPROD's).

### Viewport views

For every visible scene view, game view and preview, the editor stages one `Flux_ViewRequest` per frame (`Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"):

| Member | Scene view | Game view | Preview |
|---|---|---|---|
| `m_eKind` | `VIEW_KIND_EDITOR` | `VIEW_KIND_EDITOR` | `VIEW_KIND_PREVIEW` |
| `m_ulOwnerID` | `Zenith_Hash64` of ("EDITVIEW", viewport ID) | written by RPROD's camera extractor (below) | ("EDITPREV", viewport ID) |
| `m_xCamera`, `m_xLens` | the editor camera | the game camera | the preview camera |
| `m_xOutputs` | the viewport's tool texture (render-target usage) | the same | the same |
| `m_uOutputWidth`, `m_uOutputHeight` | the panel's content rectangle in pixels, rounded down to even numbers, at least 16 × 16 | the same | the same |
| `m_uFeatureMaskSet` / `Clear` | `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` set; the debug view of the viewport's view menu | `EDITOR_OVERLAY` cleared, `UI` set | per preview kind (the VFX preview sets the particle and fog bits, row VX11) |
| `m_xQualityOverrides` | the dynamic-resolution scale fixed at 1.0, so overlays align with the output | the game's settings | the preview tier |
| `m_bCameraCut` | set on a framing jump, a bookmark jump and a switch of the shown world | RPROD's | set on each preview reset |

- A viewport whose panel is hidden or collapsed requests nothing and keeps its output texture; its histories retire after RPROD's 60-frame rule ([Z2-N-REND-600]).
- A panel resize recreates the viewport's output texture at the new size in the same frame, in the order of § "Editor texture references" (register the new texture, swap the viewport's reference, release the old one), and sets the camera-cut bit once, so no history is reprojected across a size change.

<!-- assumption: pending RPROD (rows ED8, ED33); core wording X259 (c) -->
Game views: the editor adds to each play world, through the runtime tool access, the world resource `Zenith.Editor.GameViewTarget` { viewport ID, output texture, width, height }, whose type RPROD declares in `ZenithFlux` (it is read by RPROD's extractor, so the declaring library is RPROD's, row ED33). `Render.ExtractCameras` keys on the world purpose (X243 (b)), never on a "tool host" flag: in a world of purpose `WORLD_PURPOSE_PLAY` that holds the resource it writes the active camera's request as `VIEW_KIND_EDITOR` with that viewport ID, output and extent instead of `VIEW_KIND_MAIN`; in a world of purpose `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` it writes no camera request. So the game's camera logic, priority and cuts are RPROD's in the editor exactly as in a Player, and no editor world writes `VIEW_KIND_MAIN`. The game view's output is a tool texture, never the swapchain (the swapchain belongs to the shell view), which is why RPROD's "Present pass realisation" row that names "the editor game view" as a `FLUX_VIEW_OUTPUT_SWAPCHAIN` view is corrected by row ED33 (an RPROD design patch) and X259 (c) (the core wording).
<!-- /assumption -->

### Editor extractor

The runtime tool access provides `AddToolExtractor(Zenith_Runtime&, uint16 uWorldTag, const Zenith_ExtractorDesc&)` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access") with the semantics of `Zenith_WorldAttachContext::AddExtractor` (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The hand-off object"), so a tool can add an extractor node to a world it created.

Each edit, play and preview world gets one extractor `Editor.Extract` (EPILOGUE stage 12 of the world's group), whose body:

1. reads the editor's frame snapshot for this frame index, written on the main thread before `RunFrame` and immutable while the frame runs;
2. writes, for each viewport showing this world, its `Flux_ViewRequest` through `Flux_RenderWorldWriter::AddViewRequest`;
3. in an edit world holding an open terrain document, calls `Zenith_TerrainPreview::WriteRecords` (`Zenith2/Docs/Design/ENV/Terrain.md` → "Live preview" step 3);
4. writes nothing else: the overlays travel in the editor's own two-slot snapshot (§ "Overlay records"), not in the render world.

It declares read access to no ECS component (all its inputs are the snapshot) and write access to the world's render-world section, so it orders like any extractor before `Render.Seal`.

### Game viewport input

- The game viewport has input focus after a click inside it while its world plays; `Shift+F1`, `Escape` with the preference `editor.play.escapeReleasesFocus` (default false, because games use Escape), or a click on another panel releases it.
- While focused: `SetInputWorld(play tag)` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Input routing during play"); mouse relative mode follows the game's own requests; the cursor is confined to the viewport rectangle.

<!-- assumption: pending UI, PLAT -->
The play world's UI context is laid out at the game viewport's pixel size (UI: "the editor sets the context surface to its game view size", `Zenith2/Docs/Design/UI/Rendering.md` → "UIComposite implementation"), and the editor sets each play world's presentation rectangle (the viewport's position and size in window pixels) so that PLAT delivers pointer positions to that world's UI claimant in surface coordinates and UI claims no pointer outside the rectangle (row ED14).
<!-- /assumption -->

### Headless viewports

In a headless editor (`--headless`, no window and no renderer: `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", headless column) the shell still builds every ImGui frame, so panel logic and the automation runner run unchanged; the draw snapshot is discarded; viewports request nothing; picking uses the CPU path alone (§ "CPU bounds picking"); thumbnails and previews report "no renderer" and their requests are dropped; the scene-open, reimport, script-reload and play-start operations run and are timed identically. A headless session therefore authors and saves the same document bytes as a windowed one (`Test_EditorHeadless_SavedBytesEqualWindowed`, `Zenith2/Docs/Design/EDIT/Tests.md` → "Headless editing tests").

## Editor cameras

### Camera schemes

The editor camera is a free camera per scene view: position `p`, yaw `ψ`, pitch `θ` (the Euler convention of `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems": yaw about +Y, pitch about +X, `R = R_y(ψ) · R_x(θ)`), vertical field of view `φ` (default 60°), near `n` (default 0.05 m), far `f` (default 20,000 m), an orbit pivot `c` and an orbit distance `d`. A camera looks along its local −Z (CORE's "view forward" row), so the view direction is `v = R_y(ψ) R_x(θ) · (0, 0, −1)`: at yaw 0 and pitch 0 the camera looks along −Z. CORE's "yaw 0 faces +Z" describes a model's front (+Z, "model front"), which is the camera's back; the two rows agree because the camera's rotation is the same `R` applied to a frame whose forward is −Z. Pitch is clamped to [−89.5°, 89.5°]. The camera's pose is also written to the scene view's proxy entity each frame (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities"). The preference `editor.camera.scheme` selects the controls:

| Gesture | `unity` (default) | `unreal` |
|---|---|---|
| look | right mouse drag | right mouse drag |
| fly | right mouse held + `W A S D` forward, left, back, right; `Q`/`E` down/up | right mouse held + `W A S D Q E` |
| orbit about the pivot | `Alt` + left mouse drag | `Alt` + left mouse drag |
| pan | middle mouse drag | middle mouse drag, or left + right mouse drag |
| dolly | `Alt` + right mouse drag, or the wheel | the wheel, `Alt` + right mouse drag |
| move and turn on the ground plane | — | left mouse drag: vertical motion moves along the horizontal view direction, horizontal motion yaws |
| speed change while flying | the wheel while right mouse held | the wheel while right mouse held |

### Fly navigation

- While the look button is held, the editor enables PLAT's relative mouse mode (`Zenith_Window::SetRelativeMouseMode`), in which the mouse `delta` is raw unaccelerated device counts, not points (`Zenith2/Docs/Design/PLAT/Input.md` → "Keyboard and mouse"). Look therefore converts counts: `ψ -= Δx × k`, `θ -= Δy × k` with `k = editor.camera.lookDegreesPerCount` (default 0.15°), never the points-based position delta, whose scale differs by the device's resolution and the OS acceleration curve.
- Fly velocity `s` (m/s) starts at `editor.camera.flySpeed` (default 8), scales by 1.25 per wheel notch while flying (clamped to [0.05, 2,000]), and by `editor.camera.boostFactor` (default 4) while `Shift` is held; translation per frame is `s × Δt` along the view basis (forward `v`, right `r = normalize(cross(v, +Y))`, up +Y for `Q`/`E`), with Δt the editor frame delta clamped to 100 ms.
- The speed is remembered per scene view in the session file.

### Orbit pan and dolly

- Orbit: the pivot is the selection's bounds centre when the selection was framed last, else the point `p + d × v`; dragging changes ψ and θ by the count conversion above (in absolute mouse mode, by `Δpixels × 0.25°`), and `p = c − d × v` is recomputed each frame.
- Pan: `p += (−Δx × r + Δy × u) × w`, where `u = cross(r, v)` and `w = 2 d tan(φ / 2) / viewportHeightPixels` metres per pixel, so the point under the cursor stays under the cursor at the pivot depth.
- Dolly: the wheel moves `d ← d × 0.85^notches` (towards the pivot; `d` clamped to [0.01, 10,000] m), the drag moves `d ← d × exp(−Δy × 0.005)`.

### Framing

`camera.frameSelection` frames the bounding sphere `(c, ρ)` of the selected entities' world bounds (`Zenith2/Docs/Design/EDIT/Viewports.md` → "CPU bounds picking" gives the bounds), or of the whole active scene when nothing is selected:

- perspective: `d = ρ / sin(min(φ, φ_h) / 2) × 1.1`, where `φ_h = 2 atan(tan(φ/2) × aspect)`; the camera keeps its orientation and moves to `p = c − d × v`;
- orthographic: the orthographic height becomes `2.2 ρ`;
- a sphere of radius 0 (a single point entity) uses ρ = 1 m;
- the move is animated over `editor.camera.frameDurationMs` (default 200 ms) with a smoothstep in `d` and the pivot, and sets the camera-cut bit at its end.

### Orthographic views

The view menu offers Top (looking along −Y), Bottom, Front (along −Z), Back, Left, Right and Perspective. An orthographic view uses `FLUX_PROJECTION_ORTHOGRAPHIC` with height `h` (default 20 m; the wheel scales `h × 0.85^notches`), the grid drawn in the view's plane, and its rotation locked unless orbiting, which returns to perspective.

### Camera bookmarks

`Ctrl+1` … `Ctrl+9` store the scene view's camera (position, yaw, pitch, projection, height or field of view) as a bookmark of the active scene; `1` … `9` (viewport focused) jump to it with a camera cut. Bookmarks are per-user editor state in the session file, never in the scene document.

## Picking

### Click picking

A left click in a scene view, with no gizmo handle under the cursor and no drag beyond 4 pixels, picks the nearest object under the cursor: the CPU picker (§ "CPU bounds picking") over the entities of the shown world, then, for entities whose components register a precise pick shape, the precise test (§ "Precise mesh picking"). The hit becomes the selection per the gesture table of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection operations". A click on an entity inside a prefab instance selects the instance root first and the entity on a second click within 500 ms on the same pixel (Unity's prefab-root rule), unless `Alt` is held.

### CPU bounds picking

<!-- elaboration -->
`Zenith_EditorPicker::PickRay(const Zenith_Ray& xRay, const Zenith_EditorPickFilter& xFilter, Zenith_EditorPickHit& xOut)` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Picker header"):

1. The ray through the cursor: `o = p` (perspective) or the cursor's point on the near plane (orthographic); `dir = normalize(Zenith_TransformPointProjective(invViewProj, (x_ndc, y_ndc, 1)) − o)` with `x_ndc = 2 (u + 0.5) / W − 1`, `y_ndc = 1 − 2 (v + 0.5) / H` for pixel (u, v) of a W × H viewport.
2. Candidates: every entity of the world with a world bounds, except the editor proxy entities (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities") and the filter's exclusions; the bounds come from registered bounds providers (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface", `Zenith_PfnEditorBounds`): a mesh renderer gives the mesh asset's LOD-0 bounds (`geom.meshBounds`, row VX8 b) transformed by the world matrix (the 8 corners transformed, then their AABB); a light, camera, audio emitter, probe or any entity without a geometric component gives an icon sphere of radius `0.04 × distance to camera` around its world position; a terrain gives its tile AABBs.
3. Broad phase: a ray–AABB slab test, CORE's `Zenith_RayAABB` (`Zenith2/Docs/Design/CORE/Maths.md` → "Bounds types": `t_near = max_i(min(t1_i, t2_i))`, `t_far = min_i(max(t1_i, t2_i))` with `t1_i = (min_i − o_i) / dir_i`, `t2_i = (max_i − o_i) / dir_i`, a zero direction component handled by IEEE rules), hit when `t_far ≥ max(t_near, 0)`; candidates are sorted by `t_near`.
4. Narrow phase, front to back: the precise test for entities with a pick shape; otherwise the AABB hit stands. The first precise hit, or the nearest AABB-only hit if it is nearer than every precise hit, is the result.
5. The world's entity count is at most the W-Outdoor scale; the broad phase is O(n) per click (a click is human-rate), so no acceleration structure is kept for it.

### Precise mesh picking

- The pick-mesh cache (`Zenith_EditorPickMeshCache`) decodes a mesh asset's LOD-0 positions and indices from its cooked file in the loose tree with the vertex-format codec (`ZenithVertexFormat`, `Zenith2/Docs/Rendering.md` → "Vertex data format": SNORM16 positions dequantised by the mesh's scale and bias), builds a BVH over its triangles (binned SAH, 12 bins, leaves of at most 4 triangles), and keeps both in an LRU cache within the editor-caches budget (`MEMORY_SUBSYSTEM_EDITOR_CACHES`, Z2-B-EDIT-001), so the first click on a mesh pays the decode and every subsequent click pays a BVH traversal.
- The ray is transformed into the mesh's object space by the inverse world matrix and tested with Möller–Trumbore against the BVH, closest hit first; back faces count (a click on the inside of a mesh selects it).
- Skinned and morphed meshes are tested in bind pose; their posed bounds still bound the broad phase, so a click inside the posed silhouette but outside the bind pose selects by bounds.
- Terrain: the ray is marched in 0.5-tile-sample steps against `Zenith_TerrainDocument::SampleHeight` (`Zenith2/Docs/Design/ENV/Terrain.md` → "Zenith_TerrainDocument header") inside the terrain's AABB, refined by 8 bisection steps.

### Marquee selection

A left drag of more than 4 pixels from empty space draws a marquee. On release, the sub-frustum of the marquee rectangle is built from the four corner rays (left, right, bottom and top planes through the camera and each pair of adjacent corners; the view's near and far planes), and every entity whose world AABB intersects it (the plane test: an AABB is outside when its positive vertex is on the negative side of any plane) is selected, combined with the existing selection by the modifier (replace, `Shift` add, `Alt` remove). Locked and hidden entities (hierarchy flags of the session, never document data) are skipped.

### Gizmo handle picking

Handles are tested analytically before any object pick, so a handle in front of or behind an object always wins when under the cursor:

| Handle | Test | Hit tolerance |
|---|---|---|
| axis arrow (translate, scale) | distance between the cursor ray and the axis segment `[pivot + 0.15 L a, pivot + L a]` (closest points of two lines, § "Translate drag mathematics"), in pixels after projecting the closest point | 8 px |
| plane quad (translate) | ray–plane hit inside the quad `[0.25 L, 0.5 L]²` of the two axes | inside |
| centre (screen translate, uniform scale) | projected distance to the pivot | 10 px |
| rotation ring | ray–plane hit with the ring's plane, then `| |hit − pivot| − L | × pixelsPerMetre` | 7 px; for an axis ring only the half facing the camera |
| view ring | the screen-space circle of radius 1.15 L around the pivot | 7 px |

`L` is the handle length in metres of § "Handle sizing". The nearest handle (smallest pixel distance) wins; ties prefer axes over planes over the centre.

## Gizmos

### Gizmo modes and spaces

| Mode | Handles |
|---|---|
| translate (`W`) | three axis arrows, three plane quads, a screen-plane centre |
| rotate (`E`) | three axis rings, a view-axis ring, a free trackball inside the rings |
| scale (`R`) | three axis boxes, a uniform centre |
| combined (`T`) | translate arrows, rotate rings and scale boxes at 0.6 L |

| Space (`X` toggles world and local) | Axes |
|---|---|
| world | +X, +Y, +Z |
| local | the active entity's world rotation applied to +X, +Y, +Z |
| view (rotate ring only) | the view direction |
| parent (preference) | the active entity's parent's world rotation |

Scale is always applied in local space (a world-space non-uniform scale of a rotated entity cannot be expressed by a TRS transform).

### Pivot

| Pivot mode (toolbar) | Pivot point |
|---|---|
| active object (default) | the active entity's world position |
| selection centre | the centre of the union of the selected roots' world AABBs |
| individual origins (rotate and scale only) | each root rotates or scales about its own position; the gizmo is drawn at the active entity |

### Handle sizing

The gizmo keeps a constant screen size: `L = g × pixelsToMetres(dist)`, with `g = editor.gizmo.sizePixels × displayScale` (default 110 px), `pixelsToMetres(dist) = 2 dist tan(φ/2) / H` in perspective (`dist` the pivot's view-space depth) and `h / H` in orthographic. An axis whose direction makes |a · v| > 0.995 with the view direction is hidden (it would collapse to a point); a plane quad whose normal makes |n · v| < 0.06 is hidden (edge-on).

### Translate drag mathematics

<!-- elaboration -->
Axis drag along unit `a` through the pivot `P₀` at drag start, cursor ray `(o, d)`:

1. Write the axis as `P(s) = P₀ + s a` and the ray as `Q(t) = o + t d` (both direction vectors unit), and let `w₀ = P₀ − o`, `b = a · d`, `δ = 1 − b²`. If `δ < 1e-6` (the ray is parallel to the axis) the frame contributes no motion.
2. The closest points are at `s = (b (d · w₀) − (a · w₀)) / δ` on the axis and `t = ((d · w₀) − b (a · w₀)) / δ` on the ray (minimising `|P(s) − Q(t)|²`). A `t < 0` (the closest point behind the camera) contributes no motion.
3. At drag start `s₀` is computed with the start ray; each frame the delta along the axis is `Δ = (s − s₀)`, so the point grabbed stays under the cursor.
4. Plane drag in the plane through `P₀` with unit normal `n` (the third axis): `t = (n · (P₀ − o)) / (n · d)`; `|n · d| < 1e-6` or `t < 0` contributes no motion; the hit `h = o + t d`; the delta is `h − h₀`.
5. Screen-plane drag: the plane through `P₀` with normal `−v`.
6. A drag whose world delta would place the pivot beyond 1e6 m from the origin, or produce a non-finite value, is clamped to the last finite value and logged once (`editor.gizmo.clamped`).

### Rotate drag mathematics

1. Axis ring about unit `a` through the pivot: intersect the ray with the plane through the pivot with normal `a` (as step 4 above); when the plane is nearly edge-on (|a · d| < 0.05) use the ray's closest point to the pivot projected into the plane instead, so a ring seen edge-on stays draggable.
2. `u₀ = normalize(h₀ − pivot)`, `u = normalize(h − pivot)` (vectors shorter than 1e-6 m skip the frame).
3. The incremental angle `α = atan2((u₀ × u) · a, u₀ · u)`, accumulated per frame into the total `Θ` (so a drag beyond 180° continues instead of wrapping).
4. The rotation applied is `q = Zenith_MakeAxisAngle(a, Θ_snapped)` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion"), composed `R' = q R` in world and view space and `R' = R q` in local space.
5. Trackball (inside the rings): a frame's pixel drag `(Δx, Δy)` (x right, y down) rotates by the angle `0.4° × sqrt(Δx² + Δy²)` about the world-space axis `normalize(Δy × r + Δx × u)`, where `r` and `u` are the view's right and up vectors, so dragging right turns the near side of the object to the right and dragging down tips it towards the viewer; a drag shorter than 0.5 px contributes nothing.

### Scale drag mathematics

1. Axis box: the axis delta `Δ` of the translate mathematics with the local axis; the factor `σ = max(0.001, 1 + Δ / L)`; the new scale component `S'_i = S₀_i × σ`.
2. Uniform centre: `σ = max(0.001, 1 + (Δx − Δy) × 0.01)` from the pixel drag; every component multiplied.
3. A scale component is never set to 0 or below 0.001 by a gizmo (a zero scale is singular and makes the normal matrix fail); the inspector may type any value, including negative ones for mirroring.

### Applying a gizmo delta

- A drag is an interaction (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Interactions and merging") with merge key `Zenith_Hash64("gizmo", drag ordinal)`: every frame of the drag executes `transform.setWorld` for the selection's roots, and the whole drag is one undo record.
- For each root entity `e` with drag-start world transform `W₀(e) = T·R·S` and parent world matrix `M_parent`:
  - translate: `W(e) = translate(Δ_snapped) · W₀(e)`;
  - rotate: about the pivot `P`: position `P + q (pos₀ − P)`, rotation `q R₀` (world) or `R₀ q` (local);
  - scale: `S = S₀ ⊙ σ⃗`; with the selection-centre pivot, the position moves `P + σ⃗ ⊙ (R₀⁻¹ (pos₀ − P))` rotated back;
  - the new local transform is `M_parent⁻¹ · W(e)` decomposed into translation, rotation (`Zenith_FromMatrix3` of the orthonormalised upper 3×3) and scale (column lengths, with the sign of the determinant on X); a parent matrix that is not invertible (determinant below 1e-12) refuses the drag with the notification "Parent scale is zero".
- The edit is written as `SCENE_EDIT_SET_PROPERTY` of the `Zenith.LocalTransform` position, rotation and scale properties (SCENE's transform component, `Zenith2/Docs/Design/SCENE/Transforms.md` → "Transform components"); on an instance entity it writes an override. The projection writes the same values to the edit world out of tick, so the gizmo's feedback is the edit world's propagated transform in the following frame, and the drag reads its start values from the document, never from the world.

### Multi-object edits

The transform applies to `ListRootEntities()` alone (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection model"): a selected child of a selected parent moves with its parent and is not moved twice. Entities of documents that are read-only are skipped and named in a notification; when every root is read-only the drag does not start.

### Component gizmos

Components with spatial parameters register gizmo providers through the extension interface (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface"); the editor draws and drags them with the same handle shapes and mathematics:

| Component (owner) | Handles |
|---|---|
| light shapes: point radius, spot cone angles and range, area rectangle, directional arrow (LIGHT, `Zenith2/Docs/Design/LIGHT.md` → "Editor surfaces") | radius sphere handles; cone inner and outer angle handles on the cone rim; range handle on the axis |
| reflection probe boxes, influence and parallax extents; irradiance volume grids (LIGHT) | six face handles per box, a box per extent |
| colliders: box, sphere, capsule (PHYS, pending) | face, radius and half-height handles |
| audio emitter attenuation radii, occluder boxes (AUDIO) | radius handles; box face handles |
| camera frustum (RPROD's camera component) | field-of-view handles on the frustum rim; near and far plane handles |
| navigation links, volumes (AI, pending) | endpoint handles; box faces |
| streaming world reference bounds (STREAM) | box faces |

A component handle drag writes `SCENE_EDIT_SET_PROPERTY` of the named property, as an interaction.

## Snapping

### Snap settings

| Setting (toolbar; per user) | Default | Range |
|---|---|---|
| translate snap on/off, increment `g` | off, 0.25 m | 0.001 … 1,000 m; presets 0.01, 0.1, 0.25, 0.5, 1, 5, 10 |
| translate snap mode | absolute | absolute (to the grid), relative (the delta) |
| rotate snap on/off, increment `θ_s` | off, 15° | 0.1° … 180° |
| scale snap on/off, increment `s_s` | off, 0.1 | 0.001 … 10 |
| surface snap (`Ctrl+Shift` held, or the toggle) | off | — |
| align to surface normal | off | — |
| vertex snap (`V` held during a translate drag) | — | — |

Holding `Ctrl` during a drag inverts the translate, rotate and scale snap toggles for that drag.

### Grid snapping

- Absolute mode snaps the dragged position, per axis of the gizmo space: `x' = o + g × round((x − o) / g)` with `o` the grid origin (the world origin, or the parent's origin in local space) and `round` half away from zero; only the axes being dragged are snapped (an X-axis drag never moves Y or Z onto the grid).
- Relative mode snaps the delta: `Δ' = g × round(Δ / g)`, applied to the drag-start position, so an object off the grid keeps its offset.
- A plane drag snaps both plane axes; a screen-plane drag snaps in world axes.
- The grid pass draws the snap grid at increment `g` when translate snap is on (§ "Grid pass").

### Rotation snapping

`Θ_snapped = θ_s × round(Θ / θ_s)`, applied to the accumulated drag angle, so rotation always moves in exact multiples from the drag-start orientation; the ring shows tick marks every `θ_s` and the angle readout `Θ_snapped` in degrees to one decimal.

### Scale snapping

The factor is snapped, not the component: `σ' = max(s_s, s_s × round(σ / s_s))` for an axis or uniform drag, so a scale of 1.0 snaps through 0.9, 1.1, 1.2 …; the resulting component is `S₀_i × σ'`.

### Surface snapping

While surface snapping is active during a translate drag, the pivot follows the surface under the cursor:

1. The cursor ray is cast with the picker (§ "CPU bounds picking", precise phase), excluding the dragged entities and their descendants.
2. Where a physics module is present and the edit world's physics world holds bodies, `Zenith_Physics` ray queries are used instead for the narrow phase (the edit world's bodies exist without stepping, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module behaviour in edit and preview worlds").
3. The pivot moves to the hit point; with "align to surface normal" the rotation becomes `Zenith_MakeFromTo(+Y_local, n) · R₀`, where n is the hit's geometric normal (the triangle normal, or the terrain's `SampleNormal`).
4. Grid snapping, when on, applies after surface snapping in the surface's tangent plane (the snapped point is re-projected onto the surface along n).
5. No hit: the drag falls back to the plane drag of the current mode for that frame.

### Vertex snapping

While `V` is held, the translate drag moves the active entity so that its nearest vertex to the cursor at drag start (from the pick-mesh cache, in world space) lands on the nearest vertex under the cursor of any other visible mesh within 12 pixels; with no vertex within 12 pixels the drag behaves as an unsnapped drag. Vertices are searched in screen space over the BVH leaves the cursor's 12-pixel cone overlaps.

## Overlays

### Editor overlay passes

<!-- elaboration -->
`Zenith_EditorOverlayExtension` is the `Flux_ViewExtension` for `VIEW_KIND_EDITOR` scene views, at `FLUX_EXTENSION_POINT_OVERLAY` (output extent, after `UIComposite`; `Zenith2/Docs/Design/RPROD/Views.md` → "View extensions"), registered beside the ImGui renderer at startup step 12. RPROD calls no editor extension for a view whose `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` bit is clear; for the others `AddPasses` returns at once unless the view's `m_xViewID` is one of the scene-view IDs (`Flux_MakeViewID(VIEW_KIND_EDITOR, owner, 0)` of each viewport) listed in this frame's overlay snapshot (so neither the shell view nor a game view gets overlay passes), and otherwise declares, in order:

| Pass | Program (`Zenith2/Shaders/Editor/`) | Reads | Writes |
|---|---|---|---|
| `EditorGrid` | `Grid.slang` (fullscreen) | the view's depth (SRV) | colour, blended |
| `EditorSelectionMask` | `SelectionMask.slang` (raster) | the mesh pools of the selected instances | an `R8_UNORM` transient mask at output extent, cleared to 0 |
| `EditorSelectionOutline` | `SelectionOutline.slang` (fullscreen) | the mask | colour, blended |
| `EditorIcons` | `Icons.slang` (raster, instanced quads) | the icon atlas tool texture, the view's depth | colour, blended |
| `EditorGizmo` | `Gizmo.slang` (raster, lines and triangles) | the view's depth | colour, blended |

<!-- assumption: pending RPROD, GEOM -->
The overlay passes read the view's depth target through `Flux_ViewTargets` at `FLUX_EXTENSION_POINT_OVERLAY` (the output and render extents are equal in a scene view, whose dynamic-resolution scale is fixed at 1.0), and the selection mask draws mesh instances through GEOM's `Flux_MeshResource::GetDrawRange` and `Flux_MeshManager::GetRasterPoolStreams` (row VX8 a) with the instance's world matrix, and skinned instances through the skinned-vertex pool slot RPROD's render world gives the instance (row ED12).
<!-- /assumption -->

Every overlay format is at most 16 bits per channel (colour targets are the view's output; the mask is `R8_UNORM`), so the overlays add no target to the justified 32-bit list (`Zenith2/Docs/Rendering.md` → "GPU performance rules and lints").

### Grid pass

- An analytic infinite grid on the world plane y = 0 (or the orthographic view's plane), evaluated per pixel: the pixel's ray hits the plane at `h`; lines are drawn where `fract(h_x / g)` or `fract(h_z / g)` is within the anti-aliased line width `fwidth(h / g) × 1.5` of 0 or 1.
- Two levels: minor lines at `g` and major lines at `10 g`, where `g` is the snap increment when translate snap is on, else the power of 10 nearest to `dist / 30` (`dist` the camera height above the plane); the minor level fades out as its screen spacing drops below 8 px.
- The X axis line is red (sRGB 220, 60, 60), the Z axis line blue (60, 100, 230), other lines grey at 35% (major) and 18% (minor) alpha.
- Depth: fragments whose plane depth is behind the scene depth are drawn at 25% of their alpha, so the grid reads through geometry without hiding it; fragments farther than `editor.grid.fadeDistance` (default 500 m) fade to zero.

### Selection outline pass

- The mask pass draws every selected mesh instance (from the overlay snapshot's selected-instance list) with value 1.0, and the active entity's instances with value 0.5, without depth test, so occluded parts of a selection are outlined too.
- The outline pass samples the mask in a 5 × 5 neighbourhood: a pixel is an outline pixel when its mask is 0 and any neighbour within 2 px is non-zero; its colour is orange (sRGB 255, 140, 0) for the selection and yellow (255, 220, 60) where the neighbour value is 0.5 (the active entity), at full alpha. The mask carries no depth (a depth channel beside it would need a 32-bit target outside the justified list), so occluded and visible parts of a selection are outlined alike; the gizmo and the icons, which do test depth, show where the object is.
- The outline width scales with the display scale.

### Icons pass

Entities without visible geometry (lights, cameras, audio emitters, probes, empty entities when the preference `editor.icons.showEmpty` is on) draw a screen-aligned vector icon quad of `24 × displayScale` px at their world position, from the icon atlas the shell rasterises once from the vector icon table; icons behind the scene depth draw at 30% alpha; an icon is a pick shape (§ "CPU bounds picking", the icon sphere).

### Gizmo pass

The gizmo geometry is built on the main thread each frame into the overlay snapshot as coloured line and triangle lists in world space (arrows: a 14-segment cone and a shaft; rings: 64-segment line loops; boxes: 12 triangles), drawn with a depth comparison against the scene depth for 40%-alpha hidden parts, and never written to depth. Axis colours: X red (230, 60, 60), Y green (90, 200, 60), Z blue (60, 110, 240), view ring white, hovered handle yellow (255, 220, 60), active handle white.

### Overlay records

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Viewport/Zenith_EditorOverlaySnapshot.h
struct Zenith_EditorOverlayVertex        // 16 bytes
{
	float m_fX, m_fY, m_fZ;              // world space, metres
	uint32 m_uColour;                   // sRGB RGBA8, R in bits 0–7
};

struct Zenith_EditorOverlayView          // one per scene view with overlays
{
	uint32 m_uViewportID;
	Zenith_EditorGridSettings m_xGrid;
	Zenith_Span<const Zenith_EditorOverlayVertex> m_axGizmoLines;      // pairs
	Zenith_Span<const Zenith_EditorOverlayVertex> m_axGizmoTriangles;  // triples
	Zenith_Span<const Zenith_EditorIconInstance> m_axIcons;
	Zenith_Span<const Zenith_EditorSelectedInstance> m_axSelected;     // mesh, LOD, section range, world matrix, active flag
};
```

The overlay snapshot shares the shell snapshot's two-slot ring and frame indexing (§ "ImGui renderer" step 1), with a cap of `editor.overlay.maxVertices` (default 262,144) gizmo vertices and `editor.overlay.maxSelectedInstances` (default 65,536) outlined instances; beyond the cap the outline is drawn for the first instances in selection order and `editor.overlay.selection_truncated` counts the frame.
