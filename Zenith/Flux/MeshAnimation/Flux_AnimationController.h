#pragma once
#include "Flux_AnimationClip.h"
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachine.h"
#include "Flux_InverseKinematics.h"
#include "Flux_AnimationLayer.h"
#include "AssetHandling/Zenith_AssetHandle.h"

//=============================================================================
// Flux_AnimationUpdateMode
// Controls how animation timing is driven
//=============================================================================
enum Flux_AnimationUpdateMode : uint8_t
{
	ANIMATION_UPDATE_NORMAL,     // Uses scaled deltaTime (affected by time scale)
	ANIMATION_UPDATE_FIXED,      // Uses fixed timestep (for physics-synced animation)
	ANIMATION_UPDATE_UNSCALED    // Uses unscaled deltaTime (UI animations, pause menus)
};

// Forward declarations
class Flux_SkeletonInstance;
class Zenith_SkeletonAsset;
// WU-6.2. Forward-declared rather than included: the def header includes this
// one's neighbours (Flux_AnimationLayer.h -> Flux_AnimationStateMachine.h), and
// only Flux_AnimationController.cpp needs the complete type.
class Flux_AnimatorControllerDef;

//=============================================================================
// Flux_AnimationEventCallback
// Callback for animation events
//=============================================================================
using Flux_AnimationEventCallback = void(*)(void* pUserData, const std::string& strEventName,
	const Zenith_Maths::Vector4& xData);

//=============================================================================
// Flux_AnimationController
// Unified controller that manages clips, state machine, and IK
// This is the main interface for animation playback
//=============================================================================
class Flux_AnimationController
{
public:
	Flux_AnimationController();
	~Flux_AnimationController();

	// Non-copyable - owns dynamically allocated state machine, IK solver, etc.
	Flux_AnimationController(const Flux_AnimationController&) = delete;
	Flux_AnimationController& operator=(const Flux_AnimationController&) = delete;

	// Moveable - transfers ownership of owned pointers
	Flux_AnimationController(Flux_AnimationController&& xOther) noexcept;
	Flux_AnimationController& operator=(Flux_AnimationController&& xOther) noexcept;

	// Initialize with a skeleton instance.
	//
	// ★ Initialize(nullptr) DETACHES — it does not merely forget the pointer. The
	// skeleton-asset handle is cleared FIRST and unconditionally, so a caller that
	// says "stop animating this instance" has also given the asset reference back.
	// It used to be Set() INSIDE the `if (pxSkeleton)` branch, which left the
	// controller holding an AddRef'd cached pointer with nothing anywhere to clear
	// it: the handle then died in ~Flux_AnimationController, at whatever point that
	// happened to be — for an object outliving Zenith_AssetRegistry::Shutdown, that
	// is a Release() into freed memory.
	void Initialize(Flux_SkeletonInstance* pxSkeleton);

	// ★ DROP EVERY ASSET REFERENCE THIS CONTROLLER HOLDS, WHILE THE REGISTRY IS
	// STILL ALIVE. The skeleton handle, every handle in m_xAnimationAssets and the
	// clip collection go together, because they are ONE invariant: a handle in that
	// vector is what keeps the asset behind a BORROWED clip pointer in the
	// collection alive, so releasing one without emptying the other leaves the
	// collection holding pointers into assets nothing pins.
	//
	// This is a TEARDOWN verb, not a reset: the controller keeps its skeleton
	// INSTANCE pointer (Update no-ops without the asset), its state machine and its
	// layers, and anything that resolved a clip reference through the collection is
	// left pointing at a clip the registry may now free. Call it when the owner is
	// closing, and call it from anything that can outlive the registry — a
	// controller torn down at atexit has no registry left to release into.
	void ReleaseAssetReferences();

	// Check if initialized
	bool IsInitialized() const { return m_pxSkeletonInstance != nullptr; }

	// Get the number of bones from either system
	uint32_t GetNumBones() const;

	// Check if the controller has animation content (clips loaded or playing).
	bool HasAnimationContent() const;

	//=========================================================================
	// Update (call each frame)
	//=========================================================================

	// Main update function - evaluates state machine, applies IK, uploads to GPU
	void Update(float fDt);

	// Get the current output pose
	const Flux_SkeletonPose& GetOutputPose() const { return m_xOutputPose; }

	// Get skinning matrices for custom rendering
	const Zenith_Maths::Matrix4* GetSkinningMatrices() const;

	//=========================================================================
	// Animation Clip Management
	//=========================================================================

	// Get clip collection
	Flux_AnimationClipCollection& GetClipCollection() { return m_xClipCollection; }
	const Flux_AnimationClipCollection& GetClipCollection() const { return m_xClipCollection; }

	// Add a clip from file
	Flux_AnimationClip* AddClipFromFile(const std::string& strPath);

	// Remove a clip by name
	void RemoveClip(const std::string& strName);

	// Get clip by name
	Flux_AnimationClip* GetClip(const std::string& strName);

	//=========================================================================
	// State Machine Access
	//=========================================================================

	// Get state machine (creates if doesn't exist)
	Flux_AnimationStateMachine& GetStateMachine();
	const Flux_AnimationStateMachine* GetStateMachinePtr() const { return m_pxStateMachine; }

	// Check if state machine exists
	bool HasStateMachine() const { return m_pxStateMachine != nullptr; }

	// State info query (Unity's GetCurrentAnimatorStateInfo)
	Flux_AnimatorStateInfo GetCurrentAnimatorStateInfo() const;

	// Force-crossfade to a named state, bypassing transition conditions (Unity's Animator.CrossFade)
	void CrossFade(const std::string& strStateName, float fDuration = 0.15f);

	// Create a new state machine (replaces existing)
	Flux_AnimationStateMachine* CreateStateMachine(const std::string& strName = "Default");

	// Build the top-level state machine from a definition (a COPY — see
	// Flux_AnimationStateMachine::BuildFromDef). Clip references resolve through
	// this controller's own collection.
	Flux_AnimationStateMachine* BuildStateMachineFromDef(const Flux_AnimationStateMachineDef& xDef);

	//=========================================================================
	// Whole-controller build / export (WU-6.2, .zanimctrl)
	//
	// ★ A STATE-MACHINE DEF ALONE CANNOT DESCRIBE A CONTROLLER. This object has
	// an OPTIONAL top-level machine and N LAYERS, each owning its own machine,
	// and every layered game reaches its graph through a layer with
	// m_pxStateMachine null. Flux_AnimatorControllerDef is the whole of it: the
	// clip paths, the optional top-level def, and per layer an id, a name, a
	// weight, a blend mode, an emit-events flag, a bone-mask ASSET PATH and an
	// embedded def.
	//=========================================================================

	// Replace EVERYTHING this controller holds that the def describes: the clip
	// collection is added to (by AddClipFromFile, so the asset handles are taken
	// here), the top-level state machine is rebuilt or dropped, and every layer is
	// destroyed and rebuilt from the def's list.
	//
	// pxSkeletonForMasks resolves each layer's .zanimmask from BONE NAMES to bone
	// indices. It may be null ONLY when no layer names a mask.
	//
	// ★ A DANGLING REFERENCE FAILS LOUDLY. A mask path that does not resolve, a
	// mask that needs a skeleton when none was passed, or a clip path that does
	// not load returns FALSE with a Zenith_Error naming the path. The build still
	// completes as far as it can — a half-built controller is easier to inspect
	// than an empty one — so the return value is the only thing that says the
	// result is incomplete. Do not ignore it.
	bool BuildFromControllerDef(const Flux_AnimatorControllerDef& xDef,
		const Zenith_SkeletonAsset* pxSkeletonForMasks);

	// The inverse, and what an editor Save calls. Writes this controller's clip
	// paths, top-level machine and layers into xOutDef (which is CLEARED first).
	//
	// ★ A LAYER'S BONE-MASK PATH CANNOT BE RECOVERED FROM THE RUNTIME LAYER, and
	// this reports that rather than papering over it: Flux_AnimationLayer holds a
	// resolved, index-based Flux_BoneMask and no path, so an export of a
	// controller built any way other than by BuildFromControllerDef writes an
	// EMPTY mask path for a masked layer and returns FALSE. Round-tripping a def
	// through Build -> Export preserves the paths, because Build keeps them.
	bool ExportControllerDef(Flux_AnimatorControllerDef& xOutDef) const;

	//=========================================================================
	// Parameters — ONE live set per controller (D42)
	//
	// ★ THIS USED TO BE PER STATE MACHINE, AND FOR A LAYERED CONTROLLER THAT
	// MEANT NOWHERE. SetFloat/SetBool/SetTrigger went through m_pxStateMachine,
	// which is NULL whenever the animator is built out of layers — so
	// Zenith_AnimatorComponent::SetFloat("Speed", …) was a silent no-op for every
	// game whose graph lives on a layer. Zenithmon's humans are exactly that
	// shape: ZM_PlayerController::DriveAnimatorSpeed sets "Speed" every frame and
	// nothing on the far end ever saw it.
	//
	// Now the controller owns the set and PUBLISHES it: the top-level state
	// machine, every layer's state machine and (through
	// Flux_AnimationStateMachine::SetState) every sub-state machine read and write
	// THIS object. Each def's DECLARATIONS are seeded into it — never overwriting
	// a name already present, because two layers commonly declare the same
	// "Speed" and the second seeding must not reset the value the first is
	// running on.
	//
	// Publication is LAZY (first Update, or the first Set* naming something the
	// live set does not carry yet) because authoring happens after the controller
	// exists: a game adds a layer, creates its machine and declares its
	// parameters, all before the first frame.
	//=========================================================================
	Flux_AnimationParameters& GetParameters() { return m_xParameters; }
	const Flux_AnimationParameters& GetParameters() const { return m_xParameters; }

	// Seed every attached machine's declarations into the live set and bind that
	// set to all of them. Idempotent; safe to call at any point after authoring.
	void PublishSharedParameters();

	//=========================================================================
	// IK Solver Access
	//=========================================================================

	// Get IK solver (creates if doesn't exist)
	Flux_IKSolver& GetIKSolver();
	const Flux_IKSolver* GetIKSolverPtr() const { return m_pxIKSolver; }

	// Check if IK solver exists
	bool HasIKSolver() const { return m_pxIKSolver != nullptr; }

	// Create a new IK solver (replaces existing)
	Flux_IKSolver* CreateIKSolver();

	//=========================================================================
	// Convenience Methods
	//=========================================================================

#ifdef ZENITH_TOOLS
	// Play a specific clip (editor preview only, bypasses state machine)
	void PlayClip(const std::string& strClipName, float fBlendTime = 0.15f);
#endif

	// Stop animation
	void Stop();

	//=========================================================================
	// Direct-play SCRUBBING (WU-2.4)
	//
	// ★ THERE WAS NO WAY TO ASK FOR THE POSE AT A TIME. Every entry point into
	// this class ADVANCED a clock: Update(dt) steps, PlayClip restarts at zero,
	// and the only time SETTER in the whole animation system is
	// Flux_BlendTreeNode_Clip::SetCurrentTimestamp — reachable only through the
	// private, tools-only m_pxDirectPlayNode, which has no accessor. An animation
	// editor's play head is not an increment, so scrubbing was unimplementable
	// from outside.
	//
	// ★ THE DECLARATIONS ARE NOT TOOLS-GATED even though direct play itself still
	// is: a caller in a non-tools build compiles and gets a documented `false`,
	// rather than needing its own #ifdef around every call. There is no direct-play
	// node to seek without ZENITH_TOOLS, so that is the whole of the behaviour.
	//
	// ★ SEEK DOES NOT GO THROUGH UpdateWithSkeletonInstance, and that is
	// deliberate. That dispatcher hands the frame to the LAYER path the moment any
	// layer exists, which disables the direct-play preview (and the top-level state
	// machine with it). A scrub names the thing being scrubbed, so it drives the
	// direct-play node directly and works on a layered controller too.
	//=========================================================================

	// True when a direct-play clip is armed (PlayClip has run, Stop has not).
	bool HasDirectPlayClip() const;

	// The direct-play node's clip time in SECONDS, or 0 when nothing is armed.
	float GetDirectPlayTime() const;

	// Evaluate the direct-play clip AT fTimeSeconds and apply the result to this
	// controller's skeleton instance, WITHOUT advancing any clock. The resulting
	// pose is identical to the one a tick that reached the same time would leave
	// (both seed the bind pose and sample the clip through the same helper); the
	// difference is that a seek ignores playback speed, the paused flag and any
	// in-flight crossfade — a scrub is not a transition.
	//
	// fTimeSeconds is wrapped or clamped by WrapClipTime, so a caller may pass a
	// raw slider value. False when nothing is armed or no skeleton is initialized.
	bool SeekDirectPlay(float fTimeSeconds);

	// ★ A SEEK EMITS NO EVENTS BY DEFAULT but still MOVES THE BOOKKEEPING MARK
	// (D40). Leaving the mark where it was would make the next forward tick
	// process the whole span from the old mark to the new time and fire a BURST of
	// events the playhead skipped over. Event DELIVERY policy is WU-5A's; this
	// flag is the hook it will honour, and nothing sets it true today.
	void SetEmitEventsOnSeek(bool bEmit) { m_bEmitEventsOnSeek = bEmit; }
	bool GetEmitEventsOnSeek() const { return m_bEmitEventsOnSeek; }
	// The NORMALIZED clip time the last event scan reached. Exposed so a scrub can
	// be shown to have moved it.
	float GetLastEventCheckTime() const { return m_fLastEventCheckTime; }

	// PURE. Fold a raw clip time into the clip's own range: wrapped when the clip
	// loops, clamped when it does not, and returned unchanged for a clip with no
	// duration (there is no range to fold into).
	static float WrapClipTime(const Flux_AnimationClip& xClip, float fTimeSeconds);

	//=========================================================================
	// Per-frame DRIVE GUARD (WU-2.4)
	//
	// ★ ONE DRIVER PER CONTROLLER PER FRAME. The animator inspector ticks the
	// entity's controller itself while the editor is Stopped (nothing else does —
	// Scene::Update is not running). A second panel that also ticked it would
	// double-tick: the clip would run at 2x with both panels open and at 1x with
	// one, which reads as "the preview speed is wrong" rather than as two drivers,
	// and no assert anywhere would fire.
	//
	// The token is the caller's frame identity — g_xEngine.Frame().GetFrameIndex()
	// for editor code. The FIRST claim in a given frame wins and every later one
	// in that same frame is refused, INCLUDING a repeat by the same driver: a
	// second tick is a second tick regardless of who asks for it.
	//=========================================================================
	static constexpr u_int64 ulNO_DRIVE_FRAME = ~0ull;

	bool TryBeginFrameDrive(const void* pDriver, u_int64 ulFrameToken);
	const void* GetFrameDriveOwner() const { return m_pDriveOwner; }
	u_int64 GetFrameDriveToken() const { return m_ulDriveFrameToken; }
	// Hand the frame back early (a panel that claimed and then decided not to
	// tick). Only the current owner may release.
	void ClearFrameDrive(const void* pDriver);

	// Pause/Resume
	void SetPaused(bool bPaused) { m_bPaused = bPaused; }
	bool IsPaused() const { return m_bPaused; }

	// Playback speed
	void SetPlaybackSpeed(float fSpeed) { m_fPlaybackSpeed = fSpeed; }
	float GetPlaybackSpeed() const { return m_fPlaybackSpeed; }

	// Update mode (timing source)
	void SetUpdateMode(Flux_AnimationUpdateMode eMode) { m_eUpdateMode = eMode; }
	Flux_AnimationUpdateMode GetUpdateMode() const { return m_eUpdateMode; }

	// Parameter shortcuts. These read and write the CONTROLLER's live set (D42),
	// so they reach a layer's state machine and a sub-state machine alike — which
	// the pre-WU-6.1 versions, routed through m_pxStateMachine, could not. A
	// getter naming something no attached machine declares returns the type's
	// default rather than publishing (it is const), which is the same answer the
	// old code gave for an undeclared name.
	void SetFloat(const std::string& strName, float fValue);
	void SetInt(const std::string& strName, int32_t iValue);
	void SetBool(const std::string& strName, bool bValue);
	void SetTrigger(const std::string& strName);

	float GetFloat(const std::string& strName) const;
	int32_t GetInt(const std::string& strName) const;
	bool GetBool(const std::string& strName) const;

	// IK target shortcuts
	void SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPosition, float fWeight = 1.0f);
	// Variant for callers that already converted the target to model (skeleton)
	// space and don't want Solve to apply the inverse world transform. See the
	// m_bIsModelSpace comment in Flux_IKTarget for why this exists.
	void SetIKTargetModelSpace(const std::string& strChainName, const Zenith_Maths::Vector3& xModelSpacePosition, float fWeight = 1.0f);
	// Variant that also drives the END-EFFECTOR (tip-bone) orientation so a
	// rigidly-attached tool (racket / weapon) can be aimed, not just reached.
	// Both position and rotation are expected already in model (skeleton) space.
	void SetIKTargetModelSpace(const std::string& strChainName, const Zenith_Maths::Vector3& xModelSpacePosition, const Zenith_Maths::Quat& xModelSpaceRotation, float fWeight = 1.0f);
	void ClearIKTarget(const std::string& strChainName);

	//=========================================================================
	// Animation Layers
	//
	// ★ HOLD A LAYER ID, NOT A Flux_AnimationLayer* (WU-6.3 / D43, and this
	// WITHDRAWS a documented guarantee — D44). The store's heap stability still
	// means the CONTROLLER pointer a component caches never moves; it never said
	// anything about the layers INSIDE the controller, and two ordinary verbs
	// destroy every one of them: BuildFromControllerDef rebuilds the layer list
	// wholesale from a def, and ReadFromDataStream deletes and re-reads it. A
	// game that cached the pointer its AddLayer returned was reading freed memory
	// from the next controller-asset load or scene deserialize onwards, and
	// nothing on either path could tell it so.
	//
	// The id is the handle: unique within this controller and monotonic for its
	// whole lifetime, so a layer destroyed and rebuilt from the same def keeps
	// its number while its INDEX moves, and an id belonging to a layer that is
	// simply gone resolves to nullptr instead of to whatever now sits at that
	// index. Resolve per use — GetLayerById is a short linear walk of a list that
	// is two or three entries long in every game in the tree.
	//=========================================================================

	// Add a new layer (returns pointer for immediate configuration — do not
	// store it). The layer is minted a fresh id from this controller's monotonic
	// counter; the counter is never rewound, so an id a destroyed layer held is
	// never handed out again.
	Flux_AnimationLayer* AddLayer(const std::string& strName);

	// Get layer by index (0 = base layer). An index is a POSITION IN THE BLEND
	// ORDER, which is what composition needs and what identity is not.
	Flux_AnimationLayer* GetLayer(uint32_t uIndex);
	const Flux_AnimationLayer* GetLayer(uint32_t uIndex) const;

	// By stable id — the addressing gameplay uses. Null when no layer carries it
	// (including for uFLUX_INVALID_LAYER_ID, which nothing is ever minted).
	Flux_AnimationLayer* GetLayerById(u_int uLayerId);
	const Flux_AnimationLayer* GetLayerById(u_int uLayerId) const;

	// By name, in blend order — the FIRST match wins. Null when nothing matches.
	// Names are not unique (nothing rejects two "Aim" layers), which is exactly
	// why the id and not the name is the identity; this is for authoring code
	// that knows what it built, and for looking an id up once after a rebuild.
	Flux_AnimationLayer* GetLayerByName(const std::string& strName);
	const Flux_AnimationLayer* GetLayerByName(const std::string& strName) const;

	// The id the next AddLayer will mint. Monotonic; never rewound by a removal,
	// a rebuild or a deserialize. NOT serialized (see Flux_AnimationLayer).
	u_int GetNextLayerId() const { return m_uNextLayerId; }

	// Get number of layers
	uint32_t GetLayerCount() const { return m_xLayers.GetSize(); }

	// Set layer weight
	void SetLayerWeight(uint32_t uIndex, float fWeight);

	// Check if using layers
	bool HasLayers() const { return m_xLayers.GetSize() > 0; }

	//=========================================================================
	// Events (WU-5A)
	//
	// ★ EVENTS USED TO FIRE ON EXACTLY ONE PATH, AND IT WAS THE EDITOR'S.
	// ProcessEvents had a single call site, inside #ifdef ZENITH_TOOLS and
	// gated on the tools-only direct-play node; inside the function the clip was
	// ALSO only ever sourced from that node. So in a shipping build the whole
	// mechanism returned immediately, and even in a tools build nothing a state
	// machine or a layer played could ever fire an event. A game that authored
	// footsteps into a .zanim and hooked SetEventCallback got silence, with no
	// diagnostic anywhere — the callback was installed, the clip carried the
	// events, and the code that would have matched them was unreachable.
	//
	// Delivery now runs on the state-machine and layer paths in EVERY build
	// (D34). Direct play is one more source into the same dispatcher rather
	// than the only one.
	//
	// WHO EMITS, when more than one clip is crossing an event at once:
	//  • Within one layer — the leaf with the HIGHEST blend weight, ties to the
	//    LOWEST leaf index; a zero-weight leaf never emits (D35).
	//  • Across layers — every layer independently, silenced per layer with
	//    Flux_AnimationLayer::SetEmitEvents (D36). A controller with no layers
	//    is one layer for this purpose.
	//  • Across a crossfade — the side at weight >= 0.5, ties to the TARGET
	//    (D37).
	//
	// WHICH events, over one step: the half-open normalized span [prev, curr),
	// with a wrap spelled [prev, 1) U [0, curr) and one closed end for a
	// non-looping clip that stops at 1.0 (D38). Reverse playback emits nothing
	// and still moves the mark (D39); so does a scrub (D40).
	//=========================================================================

	// Set event callback
	void SetEventCallback(Flux_AnimationEventCallback pfnCallback, void* pUserData = nullptr);

	// Clear event callback
	void ClearEventCallback();

	// PURE (D38). Does fEventNormalizedTime fall inside xSpan's crossing?
	//
	//  • !m_bForward            -> false, always. Reverse emits nothing (D39).
	//  • m_bWrapped             -> [prev, 1) U [0, curr). The two ranges are one
	//                              OR, so a step longer than the clip (which can
	//                              land above prev AND wrap) fires each event
	//                              ONCE, not twice.
	//  • m_bReachedEnd          -> [prev, curr], the one CLOSED top end. A
	//                              non-looping clip stops AT 1.0 and never steps
	//                              past it, so a half-open span could never
	//                              contain an event authored there.
	//  • otherwise              -> [prev, curr).
	//
	// ★ AN EVENT AT NORMALIZED 1.0 ON A LOOPING CLIP IS AN EVENT AT 0.0 OF THE
	// NEXT LOOP (D38), and is folded to 0.0 before any of the above. 1.0 and 0.0
	// are the same instant on a loop; without the fold, a clip authored with a
	// beat on the last frame fires it either never (half-open at the top) or
	// twice (once as 1.0, once as 0.0 the following frame).
	static bool SpanContainsEventTime(const Flux_ClipEventSpan& xSpan, float fEventNormalizedTime);

	//=========================================================================
	// World Transform
	//=========================================================================

	// Set world transform (for IK target transformation)
	void SetWorldMatrix(const Zenith_Maths::Matrix4& xWorldMatrix) { m_xWorldMatrix = xWorldMatrix; }
	const Zenith_Maths::Matrix4& GetWorldMatrix() const { return m_xWorldMatrix; }

	//=========================================================================
	// Debug
	//=========================================================================

#ifdef ZENITH_TOOLS
	// Debug draw bones
	void DebugDraw(bool bShowBones = true, bool bShowIKTargets = true);
#endif

	//=========================================================================
	// Serialization
	//=========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	// WU-5A (D34). Collect this frame's clip-event spans and fire the winners.
	// ★ NOT TOOLS-GATED, and that is the whole point of the unit: this runs on
	// the layer and state-machine paths in a shipping build. Called once per
	// Update, AFTER the pose has been evaluated — collecting is also what clears
	// each leaf's pending span, so it must happen every frame whether or not a
	// callback is installed.
	void DispatchClipEvents();

	// D35 arbitration over one layer's collected spans, then emission.
	void DispatchCollectedSpans();

	// Fire one span's events through m_pfnEventCallback.
	void EmitSpanEvents(const Flux_ClipEventSpan& xSpan);

#ifdef ZENITH_TOOLS
	// The direct-play preview keeps its span at CONTROLLER level rather than on
	// the node, because UpdateDirectPlayPose does not go through the node's
	// Evaluate (it drives the timestamp by hand) and because a scrub has to be
	// able to move the mark without a step having happened at all (D40).
	void EmitDirectPlaySpan(float fPrevNormalizedTime, float fCurrNormalizedTime, bool bForward);
#endif

	// Update path for skeleton instance
	void UpdateWithSkeletonInstance(float fDt);

	// UpdateWithSkeletonInstance phases — split out so the dispatcher reads as
	// "multi-layer? → direct-play preview? → state machine? → bind pose".
	// EvaluateAndComposeLayers: ticks all layers and composes them on top of
	//   layer 0 (additive vs override-with-mask vs override-without-mask).
	// UpdateDirectPlayPose: editor-only direct-clip preview (advance time,
	//   sample clip into output pose, apply optional crossfade snapshot).
	void EvaluateAndComposeLayers(float fDt);
#ifdef ZENITH_TOOLS
	void UpdateDirectPlayPose(float fDt);
	// Seed the output pose from the bind pose and sample the direct-play clip at
	// the node's CURRENT timestamp. Shared by the tick and by SeekDirectPlay so
	// the two cannot produce different poses for the same time — which is exactly
	// what a scrub-vs-play comparison would otherwise be measuring.
	void SampleDirectPlayPoseAtCurrentTime();
#endif

	// Apply m_xOutputPose to skeleton instance and upload to GPU
	void ApplyOutputPoseToSkeleton();

	// D42. Point every attached machine at m_xParameters WITHOUT seeding —
	// allocation-free, which is what lets the noexcept move operations repair the
	// shared pointers a move has just left aimed at the source's set.
	void RebindSharedParameters() noexcept;

	// D42. The live set must carry strName before a write to it can mean
	// anything; if it does not, a declaration has appeared since the last publish
	// (or none has happened yet), so publish now.
	void EnsureParameterDeclared(const std::string& strName);

	// WU-6.3. Take an id chosen elsewhere (a def's, on a BuildFromControllerDef)
	// onto a layer this controller has just minted one for, and move the counter
	// PAST it — the mirror of Flux_AnimatorControllerDef::AssignLayerId, and for
	// the same reason: a bare SetLayerId would leave the counter behind the ids
	// now in the list and the next AddLayer would mint a duplicate. An id of
	// uFLUX_INVALID_LAYER_ID means "the def has none", and the minted one stands.
	void AdoptLayerId(Flux_AnimationLayer& xLayer, u_int uLayerId);

	// The skeleton instance we're animating
	Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;

	// Skeleton asset handle for bone hierarchy info — keeps the asset alive while
	// this controller is ATTACHED so UnloadUnused can't free the bone data
	// mid-frame. Dropped by Initialize(nullptr) and by ReleaseAssetReferences().
	SkeletonHandle m_xSkeletonAsset;

	// Animation data. m_xAnimationAssets pins the assets behind the BORROWED clip
	// pointers in m_xClipCollection — the two move together, see
	// ReleaseAssetReferences().
	Flux_AnimationClipCollection m_xClipCollection;
	Zenith_Vector<AnimationHandle> m_xAnimationAssets;  // Keeps assets alive for borrowed clips
	Flux_AnimationStateMachine* m_pxStateMachine = nullptr;
	Flux_IKSolver* m_pxIKSolver = nullptr;

	// D42: the ONE live parameter set. Every state machine this controller drives
	// borrows a pointer to it (Flux_AnimationStateMachine::SetSharedParameters).
	Flux_AnimationParameters m_xParameters;
	// Cleared whenever the graph changes (a machine created, a layer added, a
	// stream read) so the next Update re-seeds rather than running on a set that
	// predates the declarations.
	bool m_bParametersPublished = false;

	// Current state
	Flux_SkeletonPose m_xOutputPose;
	bool m_bPaused = false;
	float m_fPlaybackSpeed = 1.0f;
	Flux_AnimationUpdateMode m_eUpdateMode = ANIMATION_UPDATE_NORMAL;

#ifdef ZENITH_TOOLS
	// Direct clip playback (editor preview only)
	Flux_BlendTreeNode_Clip* m_pxDirectPlayNode = nullptr;
	Flux_CrossFadeTransition* m_pxDirectTransition = nullptr;
#endif

	// World transform (for IK)
	Zenith_Maths::Matrix4 m_xWorldMatrix = glm::mat4(1.0f);

	// Animation layers (empty = use single state machine path, non-empty = multi-layer composition)
	Zenith_Vector<Flux_AnimationLayer*> m_xLayers;
	// WU-6.3. The monotonic layer-id counter. Moves forward only — a rebuild that
	// destroys every layer does NOT rewind it, which is what makes a stale id
	// resolve to nullptr instead of to a different layer that inherited its
	// number. Not serialized; ids are stable within one controller's lifetime,
	// and a .zanimctrl round trip carries the def's ids instead (adopted here).
	u_int m_uNextLayerId = 0;

	// Cached temporary pose for layer blending (avoids per-frame stack allocation of ~23KB poses)
	Flux_SkeletonPose m_xTempBlendPose;
	Zenith_Vector<float> m_xScaledMaskWeights;

	// Event callback
	Flux_AnimationEventCallback m_pfnEventCallback = nullptr;
	void* m_pEventCallbackUserData = nullptr;
	// ★ THE DIRECT-PLAY MARK ONLY. The state-machine and layer paths have no
	// single controller-level playhead to mark — a blend tree's leaves each run
	// their own clock — so their bookkeeping lives per leaf
	// (Flux_BlendTreeNode_Clip::GetPreviousTimestamp). This is what a scrub moves
	// (D40) and what the editor's preview session reads back.
	float m_fLastEventCheckTime = 0.0f;

	// Reused per-layer collection buffer — cleared, never reallocated, so the
	// per-frame walk costs no allocation.
	Zenith_Vector<Flux_ClipEventSpan> m_xEventSpanScratch;

	// WU-2.4: scrub event policy hook (D40) — see SetEmitEventsOnSeek.
	bool m_bEmitEventsOnSeek = false;

	// WU-2.4: per-frame drive guard — see TryBeginFrameDrive. Non-owning identity
	// only; never dereferenced.
	const void* m_pDriveOwner = nullptr;
	u_int64 m_ulDriveFrameToken = ulNO_DRIVE_FRAME;
};

//=============================================================================
// Inline implementations
//=============================================================================
// The parameter shortcuts are NOT inline here any more: they publish the shared
// set on demand (D42), which needs the layer and state-machine bodies. See
// Flux_AnimationController.cpp.

inline void Flux_AnimationController::SetIKTarget(const std::string& strChainName,
	const Zenith_Maths::Vector3& xPosition,
	float fWeight)
{
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = xPosition;
	xTarget.m_fWeight = fWeight;
	xTarget.m_bEnabled = true;
	xTarget.m_bIsModelSpace = false;

	GetIKSolver().SetTarget(strChainName, xTarget);
}

inline void Flux_AnimationController::SetIKTargetModelSpace(const std::string& strChainName,
	const Zenith_Maths::Vector3& xModelSpacePosition,
	float fWeight)
{
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = xModelSpacePosition;
	xTarget.m_fWeight = fWeight;
	xTarget.m_bEnabled = true;
	xTarget.m_bIsModelSpace = true;

	GetIKSolver().SetTarget(strChainName, xTarget);
}

inline void Flux_AnimationController::SetIKTargetModelSpace(const std::string& strChainName,
	const Zenith_Maths::Vector3& xModelSpacePosition,
	const Zenith_Maths::Quat& xModelSpaceRotation,
	float fWeight)
{
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = xModelSpacePosition;
	xTarget.m_xRotation = xModelSpaceRotation;
	xTarget.m_fWeight = fWeight;
	xTarget.m_bEnabled = true;
	xTarget.m_bUseRotation = true;
	xTarget.m_bIsModelSpace = true;

	GetIKSolver().SetTarget(strChainName, xTarget);
}

inline void Flux_AnimationController::ClearIKTarget(const std::string& strChainName)
{
	if (m_pxIKSolver)
		m_pxIKSolver->ClearTarget(strChainName);
}
