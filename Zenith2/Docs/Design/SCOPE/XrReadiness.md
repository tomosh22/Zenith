# SCOPE design: XR readiness checks

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It lists the checks that keep multi-view and input XR-ready from Stage 2 (Z2-D-072, [Z2-N-ARCH-004]), says how each binds the Stage 2 to Stage 6 tickets of its owner area (ruling Z2-D-139), specifies the readiness and exclusion tests SCOPE owns in `Zenith_Integration_ScopeChecks`, and says how the checks retire into their Stage 7 replacements.

## Scope of this file

A readiness check is a property that a Stage 7 XR ticket relies on and that an earlier ticket of another area could break without failing any of its own tests. Each check is a row of the scope register (`Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register", array `readiness`) with an owner heading, the stage from which it holds, and the test or tests that prove it. Where the owner area already tests the property, the row names that test and SCOPE adds none; where no owner test covers it, SCOPE's suite does (§ "Readiness tests SCOPE owns").

## How a readiness check binds a ticket

1. A ticket of any area whose diff edits a readiness row's owner heading, or the files that heading specifies, keeps every test the row names: it may change a test's body to the new contract, never delete or rename it without adding the new name to the row's `tests` in the same diff (and then only when the property still holds).
2. The ticket author finds the rows by the owner heading: `Zenith2/Docs/Scope/ScopeRegister.json`, `readiness[].heading`. The A5 ticket drafts of the owner areas cite the governing ruling (Z2-D-139) in the Goal of every ticket that edits a listed heading.
3. Enforcement is mechanical in two places: the ordinary `Test` step runs the named tests, so a broken property fails the ticket; the stage-acceptance check `Pwsh_ScopeRegister_ReadinessTestsPresent` fails when a named test no longer exists (`Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks").
4. A property that changes legitimately (an owner's ruling replaces it) is changed in the register in the same diff as the owner's design, with the readiness row's `title` and `tests` updated and the change named in the diff's design-doc edit.

## Multi-view checks

| ID | Property | From | Owner heading | Test(s) | Why XR needs it |
|---|---|---|---|---|---|
| XRR-V1 | `VIEW_KIND_STEREO` is declared and rejected by `Flux_BuildViewList` with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE` and counted, every other view of the frame unaffected | S2 | `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" | `Test_View_StereoRejected` (RPROD) | the kind's value and the request type exist before the XR tickets, so enabling it is additive ([Z2-N-REND-602]) |
| XRR-V2 | each view's persistent histories are its own; a second view never perturbs the first's TAA history | S2 | `Zenith2/Docs/Design/RPROD/Views.md` → "View registry" | `Test_View_EditorHistoryIndependentOfMain` (RPROD) | the two eyes are two views with independent histories (Z2-D-059) |
| XRR-V3 | view IDs derived from (kind, owner, sub-index) are stable across frames and list order, distinct for sub-indices 0 and 1 of one owner, and distinct from the same owner's main view | S2 | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header" | `Test_View_IDStableAcrossListOrder` (RPROD), `Test_ScopeMultiView_StereoEyeIdsDistinct` (SCOPE) | each eye keys its histories on its ID; a collision would share a TAA history between eyes |
| XRR-V4 | a two-view group renders each view identically to rendering it alone, with shared culling | S5 | `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view" | `Test_View_GroupSharesCullingWithoutCrossTalk` (RPROD) | the stereo pair is a two-view group sharing phase-1 culling |
| XRR-V5 | every shading path renders a view with an off-centre (asymmetric) projection correctly: two half-views with off-centre frusta covering the left and right halves of a symmetric view equal the two halves of that view outside an 8-pixel seam band | S5 | `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction" | `Test_ScopeMultiView_SplitFrustumCropParity` (SCOPE) | OpenXR eye fields of view are asymmetric (`XrFovf`); a pass that rebuilds view rays from a vertical field of view and an aspect ratio alone would be wrong per eye |
| XRR-V6 | a texture view's output texture may be a different texture each frame (a ring of three), each frame writing the texture it names, histories keyed on the view ID alone | S5 | `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view" | `Test_ScopeMultiView_OutputTextureRotation` (SCOPE) | an OpenXR swapchain hands out one of its images per frame |
| XRR-V7 | the frame loop's run-ahead is bounded by the one pacing wait on the render-submit thread | S3 | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer" | `Test_FramePacer_LatencySlotBoundsRunAhead` (RPROD) | `xrWaitFrame` takes the pacer's place as that one wait |
| XRR-V8 | world-space UI panels map canvas points to world points and hit-test rays | S4 | `Zenith2/Docs/Design/UI/Layout.md` → "World-space panels" | `Test_UIWorldPanel_CanvasToWorld` (UI) | in-world menus and pointer rays from XR controllers use world-space panels |

<!-- assumption: pending RPROD, CORE (rows SP1, SP3) -->
XRR-V5 needs `Flux_ViewLens` to carry off-centre tangents from Stage 5 (`m_fTanLeft`, `m_fTanRight`, `m_fTanUp`, `m_fTanDown`, each the tangent of the half-angle on its side, left and down negative; all four zero means the symmetric lens of `m_fFovYRadians`), honoured by step 4 of `Flux_BuildViewList` through CORE's three builders `Zenith_MakePerspectiveOffCentreStandardZ(float fTanLeft, float fTanRight, float fTanUp, float fTanDown, float fNear, float fFar)`, `Zenith_MakePerspectiveOffCentreReversedZ(float fTanLeft, float fTanRight, float fTanUp, float fTanDown, float fNear, float fFar)` and `Zenith_MakePerspectiveOffCentreReversedZInfinite(float fTanLeft, float fTanRight, float fTanUp, float fTanDown, float fNear)`, whose rows are `(2/(r−l), 0, (r+l)/(r−l), 0)`, `(0, 2/(u−d), (u+d)/(u−d), 0)`, the depth row of the symmetric builder of the same depth mode, and `(0, 0, −1, 0)`, with l, r, u, d the four tangents.
<!-- /assumption -->

## Input checks

| ID | Property | From | Owner heading | Test(s) | Why XR needs it |
|---|---|---|---|---|---|
| XRR-I1 | the `Input` Luau namespace exposes actions and contexts only, no device read | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" | `Test_InputScriptApi_ExposesActionsOnly` (PLAT) | gameplay sees XR controllers as actions, so Stage 7 changes no script |
| XRR-I2 | `/xr/<left\|right\|head>/<control>` paths parse and are reserved: a binding naming one fails with `INPUT_ERROR_UNSUPPORTED_DEVICE_KIND` | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Control paths" | `Test_ControlPath_XrPathsReserved` (PLAT) | the XR controls' path namespace is fixed before Stage 7 |
| XRR-I3 | the `Pose` action type is declared and refused by the Stage 3 bindings loader with `INPUT_ERROR_UNSUPPORTED_ACTION_TYPE`; its command encoding is reserved, so no replay format changes when it is enabled | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design" | `Test_ScopeInput_PoseActionTypeReserved` (SCOPE) | XR grip, aim and head poses are POSE actions |
| XRR-I4 | input devices created at run time deliver transitions in call order, and destroying one synthesises releases | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API" | `Test_VirtualDevice_CreateSetDestroy` (PLAT) | the OpenXR device source creates its three controller devices as `INPUT_DEVICE_KIND_XR_CONTROLLER` virtual devices and destroys them on session loss |
| XRR-I5 | simulation haptics go through commands and effects on device IDs; a rolled-back script plays nothing | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API" | `Test_Haptics_CommandsRolledBackPlayNothing` (PLAT) | XR controller vibration is a haptic effect on an XR controller device |

## Other subsystems XR touches

| ID | Property | From | Owner heading | Test(s) | Why XR needs it |
|---|---|---|---|---|---|
| XRR-O1 | the UI composite draws into the target its view supplies, so the screen-space canvas can be composited into an offscreen image | S3 | `Zenith2/Docs/Design/UI/Rendering.md` → "UIComposite implementation" | `Test_ScopeMultiView_UiCompositeIntoTextureView` (SCOPE) | the XR quad layer shows the screen-space UI from its own image (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers") |

Audio needs no readiness check: the listener follows the camera entity, which the XR camera rig moves (GAME's director output). Physics, animation and AI need none: they never see a view.

## Readiness tests SCOPE owns

All in `Zenith_Integration_ScopeChecks` (`Zenith2/Tests/ScopeChecks/`), headless on the `win` preset's WARP and lavapipe devices (X170: not `needs-gpu`), each installing the RHI test validation sink and failing on any validation message (`Zenith2/Docs/Design/RHI.md` → "The sync-validation-clean rule"). They build a renderer through the public ZenithFlux API, a hand-built render world and RPROD's test access, never an `Internal/` header of another library.

| Test | File | Stage | Case asserted |
|---|---|---|---|
| `Test_ScopeMultiView_StereoEyeIdsDistinct` | `Zenith_ScopeMultiView_Tests.cpp` | S2 | for 1,000 owner IDs drawn from `Zenith_RandomSource` seed 7: `Flux_MakeViewID(VIEW_KIND_STEREO, owner, 0)` ≠ `…(…, 1)`, both ≠ `Flux_MakeViewID(VIEW_KIND_MAIN, owner, 0)`, and each equals itself recomputed after a different call order; failure case: a collision among the 3,000 IDs fails naming the owner |
| `Test_ScopeMultiView_SplitFrustumCropParity` | `Zenith_ScopeMultiView_Tests.cpp` | S5 | for each shading path the device supports (Forward+ on lavapipe and WARP; Forward, Deferred and Visibility buffer on WARP): the W-Indoor parity scene rendered as one main-kind texture view of 512 × 256 with a symmetric lens (vertical field of view 60°), and as two texture views of 256 × 256 whose off-centre tangents are the left and right halves of that frustum (left view: l = −t·a, r = 0; right view: l = 0, r = t·a; u = t, d = −t; t = tan 30°, a = 2); the neighbourhood-wide effects are switched off in all three requests, because their kernels cross the seam: each request's `m_uFeatureMaskClear` clears `FLUX_VIEW_FEATURE_SSAO`, `FLUX_VIEW_FEATURE_SSR`, `FLUX_VIEW_FEATURE_CONTACT_SHADOWS`, `FLUX_VIEW_FEATURE_BLOOM`, `FLUX_VIEW_FEATURE_DEPTH_OF_FIELD`, `FLUX_VIEW_FEATURE_MOTION_BLUR`, `FLUX_VIEW_FEATURE_TEMPORAL_AA` and `FLUX_VIEW_FEATURE_AUTO_EXPOSURE` (the fixed exposure of the parity scene applies), so every remaining pass (prepass, clustering, fog, shading, sky, transparency, tonemap) is per-pixel or per-cluster and depends on the projection alone; each half-view's image equals the matching half of the full image with FLIP ≤ 0.02 over pixels farther than 8 pixels from the seam column, and the depth images agree to 1 ULP of D24 outside the band; failure case: a lens with the tangents swapped makes the comparison fail |
| `Test_ScopeMultiView_OutputTextureRotation` | `Zenith_ScopeMultiView_Tests.cpp` | S5 | a texture view whose request names output texture k mod 3 of three textures in frame k, for 9 frames of a scene whose clear colour encodes the frame index: after frame k the texture k mod 3 holds frame k's colour and the other two keep their earlier frames; the view's history-reset count stays 0 after frame 0 (histories follow the view ID, not the texture); failure case: naming a texture that was released returns `FLUX_VIEW_ERROR_INVALID_REQUEST` and counts it |
| `Test_ScopeMultiView_UiCompositeIntoTextureView` | `Zenith_ScopeMultiView_Tests.cpp` | S3 | a UI document with one solid 64 × 64 rectangle at canvas (16, 16) composited into an editor-kind view's 256 × 256 texture output: the rectangle's pixels carry its colour and every pixel outside it the scene's; failure case: a view whose output extent differs from the canvas extent scales the rectangle by the canvas-to-output ratio |
| `Test_ScopeInput_PoseActionTypeReserved` | `Zenith_ScopeInput_Tests.cpp` | S3 | a bindings file declaring an action of type `Pose` fails loading with `INPUT_ERROR_UNSUPPORTED_ACTION_TYPE` naming the action; `INPUT_ACTION_TYPE_POSE` has the value PLAT's header gives it; the reserved command encoding of a POSE action decodes to a refused command, counted, never applied; failure case: a `Pose` action with a non-`/xr/` control fails with the same error (retired at Stage 7, § "Retirement at Stage 7") |

## Exclusion checks SCOPE owns

| Test | File | Stage | Case asserted |
|---|---|---|---|
| `Test_ScopeExclusion_LocalDevicesShareSlotZero` | `Zenith_ScopeExclusion_Tests.cpp` | S3 | a headless runtime with two virtual gamepads and the logical keyboard, each pressing a different binding of one action in ticks 1, 2 and 3: every input command of the 3 ticks carries player slot 0, the action is pressed in each tick, and no API of `Zenith_InputSystem` takes a player slot other than 0; failure case: a virtual device desc naming a player slot other than 0 (`m_uPlayerSlot` of the device payload) is refused with `INPUT_ERROR_BAD_VIRTUAL_DEVICE` |

The modding exclusion needs no SCOPE test: its enforcement entries are [Z2-N-ARCH-262], [Z2-N-ARCH-264] and [Z2-N-ARCH-274] (the `ForbiddenCheck` gate step) and MOD's exclusion requirement, named in the register.

<!-- assumption: pending PLAT (row SP4) -->
The failure case of `Test_ScopeExclusion_LocalDevicesShareSlotZero` assumes that a virtual device cannot be given a player slot other than 0; PLAT's desc has no slot member today, so the assertion is that `Zenith_VirtualDeviceDesc` has no slot member and every created device's `Zenith_PlatformDevicePayload::m_uPlayerSlot` is 0.
<!-- /assumption -->

## Retirement at Stage 7

| Row | Stage 7 replacement | Owner of the replacement (the ticket that defines it today) |
|---|---|---|
| XRR-V1 | `Test_View_StereoWithoutRuntimeRejected`: a stereo request while no `Flux_StereoFrameSource` is set is still rejected with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE`; with a source set but no running session the request renders as the mono main view (SCOPE's fallback), so the two behaviours differ only in whether a source exists | RPROD, Z2T-RPROD-131 |
| XRR-I2 | `Test_Bindings_InteractionProfileKey` plus `Test_InputDevices_XrControllerVirtualKind` | PLAT, Z2T-PLAT-096 |
| XRR-I3 | `Test_InputValue_PoseRoundTrip`; the ticket deletes `Test_ScopeInput_PoseActionTypeReserved` in the same diff | PLAT, Z2T-PLAT-096 |

The replacement names are defined today only in those tickets, not yet in their owners' design test tables; each ticket adds its names to its owner's test table in its own diff. Every other row keeps its tests in Stage 7. The register's `retiredAt` and `replacedBy` fields carry the table; the presence check requires the replacement names in every acceptance of the retiring stage and after it. Other Stage 7 tests SCOPE cites but does not own: `Test_Xr_LiveEqualsReplayWithPose` (PLAT, Z2T-PLAT-097) and `Test_FluxRenderer_StereoFrameOnOpenXR` (RPROD, Z2T-RPROD-132).
