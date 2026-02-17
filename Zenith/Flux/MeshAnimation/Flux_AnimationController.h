#pragma once
#include "Flux_AnimationClip.h"
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachine.h"
#include "Flux_InverseKinematics.h"
#include "Flux_AnimationLayer.h"
#include "Flux/Flux_Buffers.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Collections/Zenith_Vector.h"

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

	// Check if the controller has animation content (clips loaded or playing)
	// Used by GetBoneBuffer() to decide whether to use this controller or fall back to legacy system
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
	// GPU Buffer Access
	//=========================================================================

	// Get bone matrix buffer for rendering
	const Flux_DynamicConstantBuffer& GetBoneBuffer() const { return m_xBoneBuffer; }
	Flux_DynamicConstantBuffer& GetBoneBuffer() { return m_xBoneBuffer; }

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

	// Upload bone matrices to GPU
	void UploadToGPU();

	// Update path for skeleton instance
	void UpdateWithSkeletonInstance(float fDt);

	// Apply m_xOutputPose to skeleton instance and upload to GPU
	void ApplyOutputPoseToSkeleton();

	// The skeleton instance we're animating
	Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;

	// Skeleton asset for bone hierarchy info when using skeleton instance
	Zenith_SkeletonAsset* m_pxSkeletonAsset = nullptr;

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

	// GPU buffer for bone matrices
	Flux_DynamicConstantBuffer m_xBoneBuffer;

	// World transform (for IK)
	Zenith_Maths::Matrix4 m_xWorldMatrix = glm::mat4(1.0f);

	// Animation layers (empty = use single state machine path, non-empty = multi-layer composition)
	Zenith_Vector<Flux_AnimationLayer*> m_xLayers;

	// Cached temporary pose for layer blending (avoids per-frame stack allocation of ~23KB poses)
	Flux_SkeletonPose m_xTempBlendPose;
	std::vector<float> m_xScaledMaskWeights; // #TODO: Replace with engine type when MaskedBlend accepts non-std::vector

	// Event callback
	Flux_AnimationEventCallback m_pfnEventCallback = nullptr;
	void* m_pEventCallbackUserData = nullptr;
	float m_fLastEventCheckTime = 0.0f;
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

	GetIKSolver().SetTarget(strChainName, xTarget);
}

inline void Flux_AnimationController::ClearIKTarget(const std::string& strChainName)
{
	if (m_pxIKSolver)
		m_pxIKSolver->ClearTarget(strChainName);
}
