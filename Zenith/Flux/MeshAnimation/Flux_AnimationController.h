#pragma once
#include "Flux_AnimationClip.h"
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachine.h"
#include "Flux_InverseKinematics.h"
#include "Flux/Flux_Buffers.h"
#include "DataStream/Zenith_DataStream.h"
#include <functional>

// Forward declarations
class Flux_MeshGeometry;
class Flux_SkeletonInstance;
class Zenith_SkeletonAsset;

//=============================================================================
// Flux_AnimationEventCallback
// Callback for animation events
//=============================================================================
using Flux_AnimationEventCallback = std::function<void(const std::string& strEventName,
	const Zenith_Maths::Vector4& xData)>;

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

	// Initialize with a mesh (legacy system - required for bone data)
	void Initialize(Flux_MeshGeometry* pxGeometry);

	// Initialize with a skeleton instance (new model instance system)
	void Initialize(Flux_SkeletonInstance* pxSkeleton);

	// Check if initialized
	bool IsInitialized() const { return m_pxGeometry != nullptr || m_pxSkeletonInstance != nullptr; }

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

	// Play a specific clip (bypasses state machine)
	void PlayClip(const std::string& strClipName, float fBlendTime = 0.15f);

	// Stop animation
	void Stop();

	// Pause/Resume
	void SetPaused(bool bPaused) { m_bPaused = bPaused; }
	bool IsPaused() const { return m_bPaused; }

	// Playback speed
	void SetPlaybackSpeed(float fSpeed) { m_fPlaybackSpeed = fSpeed; }
	float GetPlaybackSpeed() const { return m_fPlaybackSpeed; }

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
	// Events
	//=========================================================================

	// Set event callback
	void SetEventCallback(Flux_AnimationEventCallback fnCallback);

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

	// Debug draw bones
	void DebugDraw(bool bShowBones = true, bool bShowIKTargets = true);

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

	// Update path for skeleton instance (new model instance system)
	void UpdateWithSkeletonInstance(float fDt);

	// The mesh geometry we're animating (legacy system)
	Flux_MeshGeometry* m_pxGeometry = nullptr;

	// The skeleton instance we're animating (new model instance system)
	Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;

	// Skeleton asset for bone hierarchy info when using skeleton instance
	Zenith_SkeletonAsset* m_pxSkeletonAsset = nullptr;

	// Animation data
	Flux_AnimationClipCollection m_xClipCollection;
	Flux_AnimationStateMachine* m_pxStateMachine = nullptr;
	Flux_IKSolver* m_pxIKSolver = nullptr;

	// Current state
	Flux_SkeletonPose m_xOutputPose;
	bool m_bPaused = false;
	float m_fPlaybackSpeed = 1.0f;

	// Direct clip playback (when not using state machine)
	Flux_BlendTreeNode_Clip* m_pxDirectPlayNode = nullptr;
	Flux_CrossFadeTransition* m_pxDirectTransition = nullptr;

	// GPU buffer for bone matrices
	Flux_DynamicConstantBuffer m_xBoneBuffer;

	// World transform (for IK)
	Zenith_Maths::Matrix4 m_xWorldMatrix = glm::mat4(1.0f);

	// Event callback
	Flux_AnimationEventCallback m_fnEventCallback;
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
