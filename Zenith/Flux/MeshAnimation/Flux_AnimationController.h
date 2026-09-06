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

	// Initialize with a skeleton instance
	void Initialize(Flux_SkeletonInstance* pxSkeleton);

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

	// Load state machine from file
	bool LoadStateMachineFromFile(const std::string& strPath);

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

	// State machine parameter shortcuts
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
	//=========================================================================

	// Add a new layer (returns pointer for configuration)
	Flux_AnimationLayer* AddLayer(const std::string& strName);

	// Get layer by index (0 = base layer)
	Flux_AnimationLayer* GetLayer(uint32_t uIndex);
	const Flux_AnimationLayer* GetLayer(uint32_t uIndex) const;

	// Get number of layers
	uint32_t GetLayerCount() const { return m_xLayers.GetSize(); }

	// Set layer weight
	void SetLayerWeight(uint32_t uIndex, float fWeight);

	// Check if using layers
	bool HasLayers() const { return m_xLayers.GetSize() > 0; }

	//=========================================================================
	// Events
	//=========================================================================

	// Set event callback
	void SetEventCallback(Flux_AnimationEventCallback pfnCallback, void* pUserData = nullptr);

	// Clear event callback
	void ClearEventCallback();

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
	// Process animation events for the current frame
	void ProcessEvents(float fPrevTime, float fCurrentTime);

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

	// The skeleton instance we're animating
	Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;

	// Skeleton asset handle for bone hierarchy info — keeps the asset alive while
	// this controller exists so UnloadUnused can't free the bone data mid-frame.
	SkeletonHandle m_xSkeletonAsset;

	// Animation data
	Flux_AnimationClipCollection m_xClipCollection;
	Zenith_Vector<AnimationHandle> m_xAnimationAssets;  // Keeps assets alive for borrowed clips
	Flux_AnimationStateMachine* m_pxStateMachine = nullptr;
	Flux_IKSolver* m_pxIKSolver = nullptr;

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

	// Cached temporary pose for layer blending (avoids per-frame stack allocation of ~23KB poses)
	Flux_SkeletonPose m_xTempBlendPose;
	Zenith_Vector<float> m_xScaledMaskWeights;

	// Event callback
	Flux_AnimationEventCallback m_pfnEventCallback = nullptr;
	void* m_pEventCallbackUserData = nullptr;
	float m_fLastEventCheckTime = 0.0f;

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
inline void Flux_AnimationController::SetFloat(const std::string& strName, float fValue)
{
	if (m_pxStateMachine)
		m_pxStateMachine->GetParameters().SetFloat(strName, fValue);
}

inline void Flux_AnimationController::SetInt(const std::string& strName, int32_t iValue)
{
	if (m_pxStateMachine)
		m_pxStateMachine->GetParameters().SetInt(strName, iValue);
}

inline void Flux_AnimationController::SetBool(const std::string& strName, bool bValue)
{
	if (m_pxStateMachine)
		m_pxStateMachine->GetParameters().SetBool(strName, bValue);
}

inline void Flux_AnimationController::SetTrigger(const std::string& strName)
{
	if (m_pxStateMachine)
		m_pxStateMachine->GetParameters().SetTrigger(strName);
}

inline float Flux_AnimationController::GetFloat(const std::string& strName) const
{
	return m_pxStateMachine ? m_pxStateMachine->GetParameters().GetFloat(strName) : 0.0f;
}

inline int32_t Flux_AnimationController::GetInt(const std::string& strName) const
{
	return m_pxStateMachine ? m_pxStateMachine->GetParameters().GetInt(strName) : 0;
}

inline bool Flux_AnimationController::GetBool(const std::string& strName) const
{
	return m_pxStateMachine ? m_pxStateMachine->GetParameters().GetBool(strName) : false;
}

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
