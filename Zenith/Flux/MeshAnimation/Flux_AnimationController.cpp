#include "Zenith.h"
#include "Flux_AnimationController.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Core/Zenith_Core.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"
#endif

//=============================================================================
// Flux_AnimationController
//=============================================================================
Flux_AnimationController::Flux_AnimationController()
{
}

Flux_AnimationController::~Flux_AnimationController()
{
	delete m_pxStateMachine;
	delete m_pxIKSolver;
	delete m_pxDirectPlayNode;
	delete m_pxDirectTransition;
}

void Flux_AnimationController::Initialize(Flux_MeshGeometry* pxGeometry)
{
	m_pxGeometry = pxGeometry;

	if (m_pxGeometry)
	{
		// Initialize pose with number of bones
		m_xOutputPose.Initialize(m_pxGeometry->GetNumBones());

		// Create bone buffer if geometry has bones
		if (m_pxGeometry->GetNumBones() > 0)
		{
			Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, FLUX_MAX_BONES * sizeof(Zenith_Maths::Matrix4), m_xBoneBuffer);

			// CRITICAL: Upload identity matrices to GPU immediately
			// Without this, the bone buffer contains uninitialized data and the mesh
			// will render incorrectly (collapsed/invisible) until an animation is played
			UploadToGPU();
		}
	}
}

void Flux_AnimationController::Initialize(Flux_SkeletonInstance* pxSkeleton)
{
	m_pxSkeletonInstance = pxSkeleton;

	if (m_pxSkeletonInstance)
	{
		// Get skeleton asset for bone hierarchy info
		m_pxSkeletonAsset = m_pxSkeletonInstance->GetSourceSkeleton();

		// Initialize pose with number of bones
		uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
		m_xOutputPose.Initialize(uNumBones);

		// Note: The skeleton instance owns its own bone buffer
		// We don't need to create one here - the skeleton instance will be updated
		// and use its existing buffer for rendering

		Zenith_Log("[AnimationController] Initialized with skeleton instance (%u bones)", uNumBones);
	}
}

uint32_t Flux_AnimationController::GetNumBones() const
{
	if (m_pxGeometry)
	{
		return m_pxGeometry->GetNumBones();
	}
	if (m_pxSkeletonInstance)
	{
		return m_pxSkeletonInstance->GetNumBones();
	}
	return 0;
}

bool Flux_AnimationController::HasAnimationContent() const
{
	// Check if we have clips loaded, state machine, or active direct playback
	return !m_xClipCollection.GetClips().empty() ||
		m_pxStateMachine != nullptr ||
		m_pxDirectPlayNode != nullptr;
}

void Flux_AnimationController::Update(float fDt)
{
	// Need either geometry (legacy) or skeleton instance (new system)
	if ((!m_pxGeometry && !m_pxSkeletonInstance) || m_bPaused)
		return;

	fDt *= m_fPlaybackSpeed;

	float fPrevTime = m_fLastEventCheckTime;

	// Use skeleton instance path if available (new model instance system)
	if (m_pxSkeletonInstance && m_pxSkeletonAsset)
	{
		UpdateWithSkeletonInstance(fDt);

		// Process animation events
		float fCurrentTime = m_pxDirectPlayNode ? m_pxDirectPlayNode->GetNormalizedTime() : 0.0f;
		ProcessEvents(fPrevTime, fCurrentTime);
		m_fLastEventCheckTime = fCurrentTime;
		return;
	}

	// Legacy path using Flux_MeshGeometry
	// Update animation source (state machine or direct playback)
	if (m_pxStateMachine)
	{
		m_pxStateMachine->Update(fDt, m_xOutputPose, *m_pxGeometry);
	}
	else if (m_pxDirectPlayNode)
	{
		// Handle direct clip playback with optional transition
		if (m_pxDirectTransition && !m_pxDirectTransition->IsComplete())
		{
			m_pxDirectTransition->Update(fDt);

			Flux_SkeletonPose xTargetPose;
			m_pxDirectPlayNode->Evaluate(fDt, xTargetPose, *m_pxGeometry);
			m_pxDirectTransition->Blend(m_xOutputPose, xTargetPose);

			if (m_pxDirectTransition->IsComplete())
			{
				delete m_pxDirectTransition;
				m_pxDirectTransition = nullptr;
			}
		}
		else
		{
			m_pxDirectPlayNode->Evaluate(fDt, m_xOutputPose, *m_pxGeometry);
		}
	}
	else
	{
		// No animation source - reset to bind pose
		m_xOutputPose.Reset();
	}

	// Apply IK after animation
	if (m_pxIKSolver)
	{
		// Compute model space matrices first (required for IK)
		// Note: This requires access to the skeleton hierarchy
		// For now, we use flat computation
		m_xOutputPose.ComputeModelSpaceMatricesFlat(*m_pxGeometry);

		m_pxIKSolver->Solve(m_xOutputPose, *m_pxGeometry, m_xWorldMatrix);
	}

	// Compute final skinning matrices
	m_xOutputPose.ComputeSkinningMatrices(*m_pxGeometry);

	// Process animation events
	float fCurrentTime = m_pxDirectPlayNode ? m_pxDirectPlayNode->GetNormalizedTime() : 0.0f;
	ProcessEvents(fPrevTime, fCurrentTime);
	m_fLastEventCheckTime = fCurrentTime;

	// Upload to GPU
	UploadToGPU();
}

void Flux_AnimationController::UpdateWithSkeletonInstance(float fDt)
{
	// This path handles animation for the new model instance system
	// using Flux_SkeletonInstance instead of Flux_MeshGeometry

	// Handle direct clip playback (simplified - no state machine support yet for skeleton instance)
	if (m_pxDirectPlayNode)
	{
		Flux_AnimationClip* pxClip = m_pxDirectPlayNode->GetClip();
		if (pxClip)
		{
			// Advance playback time
			float fCurrentTime = m_pxDirectPlayNode->GetCurrentTimestamp();
			fCurrentTime += fDt * m_pxDirectPlayNode->GetPlaybackRate();

			// Handle looping
			float fDuration = pxClip->GetDuration();
			if (fDuration > 0.0f)
			{
				if (pxClip->IsLooping())
				{
					fCurrentTime = fmod(fCurrentTime, fDuration);
					if (fCurrentTime < 0.0f)
						fCurrentTime += fDuration;
				}
				else
				{
					fCurrentTime = glm::clamp(fCurrentTime, 0.0f, fDuration);
				}
			}

			m_pxDirectPlayNode->SetCurrentTimestamp(fCurrentTime);

			// Initialize output pose with bind pose values from skeleton
			// This ensures bones WITHOUT animation channels keep their bind pose
			// instead of getting identity values
			uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
			for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
			{
				const Zenith_SkeletonAsset::Bone& xBone = m_pxSkeletonAsset->GetBone(i);
				Flux_BoneLocalPose& xPose = m_xOutputPose.GetLocalPose(i);
				xPose.m_xPosition = xBone.m_xBindPosition;
				xPose.m_xRotation = xBone.m_xBindRotation;
				xPose.m_xScale = xBone.m_xBindScale;
			}

			// Sample the clip into the output pose using skeleton asset for bone mapping
			// This overwrites bind pose values for bones that have animation channels
			m_xOutputPose.SampleFromClip(*pxClip, fCurrentTime, *m_pxSkeletonAsset);

			// Debug: Log scale values once
			static bool s_bLoggedScales = false;
			if (!s_bLoggedScales)
			{
				Zenith_Log("[AnimationController] Scale values after sampling:");
				for (uint32_t i = 0; i < uNumBones && i < 5; ++i)
				{
					const Flux_BoneLocalPose& xLocalPose = m_xOutputPose.GetLocalPose(i);
					const Zenith_SkeletonAsset::Bone& xBone = m_pxSkeletonAsset->GetBone(i);
					Zenith_Log("  [%u] '%s': scale=(%.3f, %.3f, %.3f), bindScale=(%.3f, %.3f, %.3f)",
						i, xBone.m_strName.c_str(),
						xLocalPose.m_xScale.x, xLocalPose.m_xScale.y, xLocalPose.m_xScale.z,
						xBone.m_xBindScale.x, xBone.m_xBindScale.y, xBone.m_xBindScale.z);
				}
				s_bLoggedScales = true;
			}

			// Apply the sampled pose to the skeleton instance
			for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
			{
				const Flux_BoneLocalPose& xLocalPose = m_xOutputPose.GetLocalPose(i);
				m_pxSkeletonInstance->SetBoneLocalTransform(i,
					xLocalPose.m_xPosition,
					xLocalPose.m_xRotation,
					xLocalPose.m_xScale);
			}

			// Have skeleton instance compute skinning matrices and upload to GPU
			m_pxSkeletonInstance->ComputeSkinningMatrices();
			m_pxSkeletonInstance->UploadToGPU();
		}
	}
	else
	{
		// No animation playing - skeleton instance stays at bind pose
		// (which was set when it was created)
	}
}

const Zenith_Maths::Matrix4* Flux_AnimationController::GetSkinningMatrices() const
{
	return m_xOutputPose.GetSkinningMatrices();
}

Flux_AnimationClip* Flux_AnimationController::AddClipFromFile(const std::string& strPath)
{
	Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromFile(strPath);
	if (pxClip)
	{
		m_xClipCollection.AddClip(pxClip);

		// Resolve clip references in state machine
		if (m_pxStateMachine)
		{
			m_pxStateMachine->ResolveClipReferences(&m_xClipCollection);
		}
	}
	return pxClip;
}

void Flux_AnimationController::RemoveClip(const std::string& strName)
{
	m_xClipCollection.RemoveClip(strName);
}

Flux_AnimationClip* Flux_AnimationController::GetClip(const std::string& strName)
{
	return m_xClipCollection.GetClip(strName);
}

Flux_AnimationStateMachine& Flux_AnimationController::GetStateMachine()
{
	if (!m_pxStateMachine)
	{
		m_pxStateMachine = new Flux_AnimationStateMachine("Default");
	}
	return *m_pxStateMachine;
}

Flux_AnimationStateMachine* Flux_AnimationController::CreateStateMachine(const std::string& strName)
{
	delete m_pxStateMachine;
	m_pxStateMachine = new Flux_AnimationStateMachine(strName);
	return m_pxStateMachine;
}

bool Flux_AnimationController::LoadStateMachineFromFile(const std::string& strPath)
{
	Flux_AnimationStateMachine* pxNewSM = Flux_AnimationStateMachine::LoadFromFile(strPath);
	if (pxNewSM)
	{
		delete m_pxStateMachine;
		m_pxStateMachine = pxNewSM;

		// Resolve clip references
		m_pxStateMachine->ResolveClipReferences(&m_xClipCollection);
		return true;
	}
	return false;
}

Flux_IKSolver& Flux_AnimationController::GetIKSolver()
{
	if (!m_pxIKSolver)
	{
		m_pxIKSolver = new Flux_IKSolver();
	}
	return *m_pxIKSolver;
}

Flux_IKSolver* Flux_AnimationController::CreateIKSolver()
{
	delete m_pxIKSolver;
	m_pxIKSolver = new Flux_IKSolver();
	return m_pxIKSolver;
}

void Flux_AnimationController::PlayClip(const std::string& strClipName, float fBlendTime)
{
	Flux_AnimationClip* pxClip = m_xClipCollection.GetClip(strClipName);
	if (!pxClip)
	{
		Zenith_Log("[AnimationController] Clip not found: %s", strClipName.c_str());
		return;
	}

	// Create new play node
	Flux_BlendTreeNode_Clip* pxNewNode = new Flux_BlendTreeNode_Clip(pxClip);

	// Set up transition if we have a current clip
	if (m_pxDirectPlayNode && fBlendTime > 0.0f)
	{
		delete m_pxDirectTransition;
		m_pxDirectTransition = new Flux_CrossFadeTransition();
		m_pxDirectTransition->Start(m_xOutputPose, fBlendTime);
	}
	else
	{
		delete m_pxDirectTransition;
		m_pxDirectTransition = nullptr;
	}

	delete m_pxDirectPlayNode;
	m_pxDirectPlayNode = pxNewNode;
}

void Flux_AnimationController::Stop()
{
	delete m_pxDirectPlayNode;
	m_pxDirectPlayNode = nullptr;

	delete m_pxDirectTransition;
	m_pxDirectTransition = nullptr;

	m_xOutputPose.Reset();
}

void Flux_AnimationController::SetEventCallback(Flux_AnimationEventCallback fnCallback)
{
	m_fnEventCallback = fnCallback;
}

void Flux_AnimationController::ClearEventCallback()
{
	m_fnEventCallback = nullptr;
}

void Flux_AnimationController::ProcessEvents(float fPrevTime, float fCurrentTime)
{
	if (!m_fnEventCallback)
		return;

	// Get current clip for event checking
	Flux_AnimationClip* pxClip = nullptr;
	if (m_pxDirectPlayNode)
	{
		pxClip = m_pxDirectPlayNode->GetClip();
	}

	if (!pxClip)
		return;

	const auto& xEvents = pxClip->GetEvents();
	for (const auto& xEvent : xEvents)
	{
		// Check if event time is between prev and current
		bool bTriggered = false;

		if (fCurrentTime >= fPrevTime)
		{
			// Normal playback
			bTriggered = (xEvent.m_fNormalizedTime > fPrevTime && xEvent.m_fNormalizedTime <= fCurrentTime);
		}
		else
		{
			// Looped - check both ranges
			bTriggered = (xEvent.m_fNormalizedTime > fPrevTime) || (xEvent.m_fNormalizedTime <= fCurrentTime);
		}

		if (bTriggered)
		{
			m_fnEventCallback(xEvent.m_strEventName, xEvent.m_xData);
		}
	}
}

void Flux_AnimationController::UploadToGPU()
{
	if (!m_pxGeometry || m_pxGeometry->GetNumBones() == 0)
		return;

	const Zenith_Maths::Matrix4* pxMatrices = m_xOutputPose.GetSkinningMatrices();
	Flux_MemoryManager::UploadBufferData(
		m_xBoneBuffer.GetBuffer().m_xVRAMHandle,
		pxMatrices,
		FLUX_MAX_BONES * sizeof(Zenith_Maths::Matrix4)
	);
}

#ifdef ZENITH_TOOLS
void Flux_AnimationController::DebugDraw(bool bShowBones, bool bShowIKTargets)
{
	if (!m_pxGeometry)
		return;

	// Draw bones
	if (bShowBones)
	{
		// Would need access to skeleton hierarchy to draw bone connections
		// For now, draw bone positions as spheres
		for (uint32_t i = 0; i < m_pxGeometry->GetNumBones() && i < FLUX_MAX_BONES; ++i)
		{
			Zenith_Maths::Vector3 xPos = Zenith_Maths::Vector3(m_xOutputPose.GetModelSpaceMatrix(i)[3]);
			xPos = Zenith_Maths::Vector3(m_xWorldMatrix * Zenith_Maths::Vector4(xPos, 1.0f));

			// Draw small sphere at bone position
			Flux_Primitives::AddSphere(xPos, 0.02f, Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f));
		}
	}

	// Draw IK targets
	if (bShowIKTargets && m_pxIKSolver)
	{
		for (const auto& xPair : m_pxIKSolver->GetChains())
		{
			const Flux_IKTarget* pxTarget = m_pxIKSolver->GetTarget(xPair.first);
			if (pxTarget && pxTarget->m_bEnabled)
			{
				// Draw target as red sphere
				Flux_Primitives::AddSphere(pxTarget->m_xPosition, 0.05f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
			}
		}
	}
}
#endif

void Flux_AnimationController::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Playback state
	xStream << m_bPaused;
	xStream << m_fPlaybackSpeed;

	// World matrix
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			xStream << m_xWorldMatrix[i][j];

	// Clip collection
	m_xClipCollection.WriteToDataStream(xStream);

	// State machine
	bool bHasStateMachine = (m_pxStateMachine != nullptr);
	xStream << bHasStateMachine;
	if (bHasStateMachine)
	{
		m_pxStateMachine->WriteToDataStream(xStream);
	}

	// IK solver
	bool bHasIKSolver = (m_pxIKSolver != nullptr);
	xStream << bHasIKSolver;
	if (bHasIKSolver)
	{
		m_pxIKSolver->WriteToDataStream(xStream);
	}

	// Direct play clip name (if playing)
	bool bHasDirectPlay = (m_pxDirectPlayNode != nullptr && m_pxDirectPlayNode->GetClip() != nullptr);
	xStream << bHasDirectPlay;
	if (bHasDirectPlay)
	{
		xStream << m_pxDirectPlayNode->GetClipName();
		xStream << m_pxDirectPlayNode->GetPlaybackRate();
		xStream << m_pxDirectPlayNode->GetCurrentTimestamp();
	}
}

void Flux_AnimationController::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Playback state
	xStream >> m_bPaused;
	xStream >> m_fPlaybackSpeed;

	// World matrix
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			xStream >> m_xWorldMatrix[i][j];

	// Clip collection
	m_xClipCollection.ReadFromDataStream(xStream);

	// State machine
	bool bHasStateMachine = false;
	xStream >> bHasStateMachine;
	if (bHasStateMachine)
	{
		delete m_pxStateMachine;
		m_pxStateMachine = new Flux_AnimationStateMachine();
		m_pxStateMachine->ReadFromDataStream(xStream);
		m_pxStateMachine->ResolveClipReferences(&m_xClipCollection);
	}

	// IK solver
	bool bHasIKSolver = false;
	xStream >> bHasIKSolver;
	if (bHasIKSolver)
	{
		delete m_pxIKSolver;
		m_pxIKSolver = new Flux_IKSolver();
		m_pxIKSolver->ReadFromDataStream(xStream);

		// Resolve bone indices when geometry is available
		if (m_pxGeometry)
		{
			for (auto& xPair : const_cast<std::unordered_map<std::string, Flux_IKChain>&>(m_pxIKSolver->GetChains()))
			{
				xPair.second.ResolveBoneIndices(*m_pxGeometry);
			}
		}
	}

	// Direct play clip
	bool bHasDirectPlay = false;
	xStream >> bHasDirectPlay;
	if (bHasDirectPlay)
	{
		std::string strClipName;
		float fPlaybackRate = 1.0f;
		float fCurrentTime = 0.0f;

		xStream >> strClipName;
		xStream >> fPlaybackRate;
		xStream >> fCurrentTime;

		Flux_AnimationClip* pxClip = m_xClipCollection.GetClip(strClipName);
		if (pxClip)
		{
			delete m_pxDirectPlayNode;
			m_pxDirectPlayNode = new Flux_BlendTreeNode_Clip(pxClip, fPlaybackRate);
			m_pxDirectPlayNode->SetCurrentTimestamp(fCurrentTime);
		}
	}

	// Re-initialize pose if geometry is set
	if (m_pxGeometry)
	{
		m_xOutputPose.Initialize(m_pxGeometry->GetNumBones());
	}
}
