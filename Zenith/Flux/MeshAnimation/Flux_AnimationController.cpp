#include "Zenith.h"
#include "Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
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
#ifdef ZENITH_TOOLS
	delete m_pxDirectPlayNode;
	delete m_pxDirectTransition;
#endif

	for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		delete m_xLayers.Get(i);
}

Flux_AnimationController::Flux_AnimationController(Flux_AnimationController&& xOther) noexcept
	: m_pxSkeletonInstance(xOther.m_pxSkeletonInstance)
	, m_pxSkeletonAsset(xOther.m_pxSkeletonAsset)
	, m_xClipCollection(std::move(xOther.m_xClipCollection))
	, m_xAnimationAssets(std::move(xOther.m_xAnimationAssets))
	, m_pxStateMachine(xOther.m_pxStateMachine)
	, m_pxIKSolver(xOther.m_pxIKSolver)
	, m_xOutputPose(std::move(xOther.m_xOutputPose))
	, m_bPaused(xOther.m_bPaused)
	, m_fPlaybackSpeed(xOther.m_fPlaybackSpeed)
	, m_eUpdateMode(xOther.m_eUpdateMode)
#ifdef ZENITH_TOOLS
	, m_pxDirectPlayNode(xOther.m_pxDirectPlayNode)
	, m_pxDirectTransition(xOther.m_pxDirectTransition)
#endif
	, m_xBoneBuffer(std::move(xOther.m_xBoneBuffer))
	, m_xWorldMatrix(xOther.m_xWorldMatrix)
	, m_xLayers(std::move(xOther.m_xLayers))
	, m_xTempBlendPose(std::move(xOther.m_xTempBlendPose))
	, m_xScaledMaskWeights(std::move(xOther.m_xScaledMaskWeights))
	, m_pfnEventCallback(xOther.m_pfnEventCallback)
	, m_pEventCallbackUserData(xOther.m_pEventCallbackUserData)
	, m_fLastEventCheckTime(xOther.m_fLastEventCheckTime)
{
	// Null out moved-from object's owned pointers to prevent double-delete
	xOther.m_pxStateMachine = nullptr;
	xOther.m_pxIKSolver = nullptr;
#ifdef ZENITH_TOOLS
	xOther.m_pxDirectPlayNode = nullptr;
	xOther.m_pxDirectTransition = nullptr;
#endif
	xOther.m_pxSkeletonInstance = nullptr;
	xOther.m_pxSkeletonAsset = nullptr;
	xOther.m_pfnEventCallback = nullptr;
	xOther.m_pEventCallbackUserData = nullptr;
}

Flux_AnimationController& Flux_AnimationController::operator=(Flux_AnimationController&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Delete our owned resources
		delete m_pxStateMachine;
		delete m_pxIKSolver;
#ifdef ZENITH_TOOLS
		delete m_pxDirectPlayNode;
		delete m_pxDirectTransition;
#endif

		// Transfer non-owned pointers
		m_pxSkeletonInstance = xOther.m_pxSkeletonInstance;
		m_pxSkeletonAsset = xOther.m_pxSkeletonAsset;

		// Move value types
		m_xClipCollection = std::move(xOther.m_xClipCollection);
		m_xAnimationAssets = std::move(xOther.m_xAnimationAssets);
		m_xOutputPose = std::move(xOther.m_xOutputPose);
		m_bPaused = xOther.m_bPaused;
		m_fPlaybackSpeed = xOther.m_fPlaybackSpeed;
		m_eUpdateMode = xOther.m_eUpdateMode;
		m_xBoneBuffer = std::move(xOther.m_xBoneBuffer);
		m_xWorldMatrix = xOther.m_xWorldMatrix;
		m_pfnEventCallback = xOther.m_pfnEventCallback;
		m_pEventCallbackUserData = xOther.m_pEventCallbackUserData;
		m_fLastEventCheckTime = xOther.m_fLastEventCheckTime;

		// Transfer owned pointers
		m_pxStateMachine = xOther.m_pxStateMachine;
		m_pxIKSolver = xOther.m_pxIKSolver;
#ifdef ZENITH_TOOLS
		m_pxDirectPlayNode = xOther.m_pxDirectPlayNode;
		m_pxDirectTransition = xOther.m_pxDirectTransition;
#endif

		// Transfer layers and cached blending data
		for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
			delete m_xLayers.Get(i);
		m_xLayers = std::move(xOther.m_xLayers);
		m_xTempBlendPose = std::move(xOther.m_xTempBlendPose);
		m_xScaledMaskWeights = std::move(xOther.m_xScaledMaskWeights);

		// Null out moved-from object's owned pointers
		xOther.m_pxStateMachine = nullptr;
		xOther.m_pxIKSolver = nullptr;
#ifdef ZENITH_TOOLS
		xOther.m_pxDirectPlayNode = nullptr;
		xOther.m_pxDirectTransition = nullptr;
#endif
		xOther.m_pxSkeletonInstance = nullptr;
		xOther.m_pxSkeletonAsset = nullptr;
		xOther.m_pfnEventCallback = nullptr;
		xOther.m_pEventCallbackUserData = nullptr;
	}
	return *this;
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

		// Initialize any layers that were added before Initialize() was called
		for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		{
			m_xLayers.Get(i)->InitializePose(uNumBones);
		}

		// Note: The skeleton instance owns its own bone buffer
		// We don't need to create one here - the skeleton instance will be updated
		// and use its existing buffer for rendering

		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationController] Initialized with skeleton instance (%u bones)", uNumBones);
	}
}

uint32_t Flux_AnimationController::GetNumBones() const
{
	if (m_pxSkeletonInstance)
	{
		return m_pxSkeletonInstance->GetNumBones();
	}
	return 0;
}

bool Flux_AnimationController::HasAnimationContent() const
{
	return !m_xClipCollection.GetClips().empty() ||
		m_pxStateMachine != nullptr ||
		m_xLayers.GetSize() > 0;
}

void Flux_AnimationController::Update(float fDt)
{
	if (!m_pxSkeletonInstance || !m_pxSkeletonAsset || m_bPaused)
		return;

	// #TODO: Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
	// Currently only ANIMATION_UPDATE_NORMAL is functional
	Zenith_Assert(m_eUpdateMode == ANIMATION_UPDATE_NORMAL,
		"Flux_AnimationUpdateMode FIXED/UNSCALED not yet implemented");
	fDt *= m_fPlaybackSpeed;

	UpdateWithSkeletonInstance(fDt);

#ifdef ZENITH_TOOLS
	// Process animation events (only for direct clip playback - state machine uses state callbacks)
	if (m_pxDirectPlayNode)
	{
		float fPrevTime = m_fLastEventCheckTime;
		float fCurrentTime = m_pxDirectPlayNode->GetNormalizedTime();
		ProcessEvents(fPrevTime, fCurrentTime);
		m_fLastEventCheckTime = fCurrentTime;
	}
#endif
}

void Flux_AnimationController::UpdateWithSkeletonInstance(float fDt)
{
	// This path handles animation for the new model instance system
	// using Flux_SkeletonInstance instead of Flux_MeshGeometry

	// Multi-layer path: evaluate all layers and compose
	if (m_xLayers.GetSize() > 0)
	{
		uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();

		// Update all layers
		for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		{
			m_xLayers.Get(i)->Update(fDt, *m_pxSkeletonAsset);
		}

		// Start with layer 0 as the base
		m_xOutputPose.CopyFrom(m_xLayers.Get(0)->GetOutputPose());

		// Initialize cached temp pose if needed
		if (m_xTempBlendPose.GetNumBones() != uNumBones)
			m_xTempBlendPose.Initialize(uNumBones);

		// Compose additional layers on top
		for (uint32_t i = 1; i < m_xLayers.GetSize(); ++i)
		{
			Flux_AnimationLayer* pxLayer = m_xLayers.Get(i);
			float fWeight = pxLayer->GetWeight();
			if (fWeight <= 0.0f)
				continue;

			const Flux_SkeletonPose& xLayerPose = pxLayer->GetOutputPose();

			if (pxLayer->GetBlendMode() == LAYER_BLEND_ADDITIVE)
			{
				// Additive: add layer's pose on top of current
				Flux_SkeletonPose::AdditiveBlend(m_xTempBlendPose, m_xOutputPose, xLayerPose, fWeight);
				m_xOutputPose.CopyFrom(m_xTempBlendPose);
			}
			else // LAYER_BLEND_OVERRIDE
			{
				if (pxLayer->HasAvatarMask())
				{
					// Masked override: blend based on per-bone weights
					const std::vector<float>& xWeights = pxLayer->GetAvatarMask().GetWeights();

					// Scale mask weights by layer weight using cached vector
					m_xScaledMaskWeights.resize(xWeights.size());
					for (size_t j = 0; j < xWeights.size(); ++j)
						m_xScaledMaskWeights[j] = xWeights[j] * fWeight;

					Flux_SkeletonPose::MaskedBlend(m_xTempBlendPose, m_xOutputPose, xLayerPose, m_xScaledMaskWeights);
					m_xOutputPose.CopyFrom(m_xTempBlendPose);
				}
				else
				{
					// Full override: simple blend by weight
					Flux_SkeletonPose::Blend(m_xTempBlendPose, m_xOutputPose, xLayerPose, fWeight);
					m_xOutputPose.CopyFrom(m_xTempBlendPose);
				}
			}
		}

		ApplyOutputPoseToSkeleton();
		return;
	}

#ifdef ZENITH_TOOLS
	// Handle direct clip playback (editor preview only, takes priority over state machine)
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

			// If crossfading between direct clips, blend from the snapshot pose
			if (m_pxDirectTransition)
			{
				m_pxDirectTransition->Update(fDt);
				if (m_pxDirectTransition->IsComplete())
				{
					delete m_pxDirectTransition;
					m_pxDirectTransition = nullptr;
				}
				else
				{
					m_pxDirectTransition->Blend(m_xOutputPose, m_xOutputPose);
				}
			}

			ApplyOutputPoseToSkeleton();
		}
	}
	else
#endif
	if (m_pxStateMachine)
	{
		// State machine drives animation when no direct clip is playing
		m_pxStateMachine->Update(fDt, m_xOutputPose, *m_pxSkeletonAsset);
		ApplyOutputPoseToSkeleton();
	}
	else
	{
		// No animation playing - skeleton instance stays at bind pose
		// (which was set when it was created)
	}
}

void Flux_AnimationController::ApplyOutputPoseToSkeleton()
{
	uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
	for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		const Flux_BoneLocalPose& xLocalPose = m_xOutputPose.GetLocalPose(i);
		m_pxSkeletonInstance->SetBoneLocalTransform(i,
			xLocalPose.m_xPosition,
			xLocalPose.m_xRotation,
			xLocalPose.m_xScale);
	}

	m_pxSkeletonInstance->ComputeSkinningMatrices();
	m_pxSkeletonInstance->UploadToGPU();
}

const Zenith_Maths::Matrix4* Flux_AnimationController::GetSkinningMatrices() const
{
	return m_xOutputPose.GetSkinningMatrices();
}

Flux_AnimationClip* Flux_AnimationController::AddClipFromFile(const std::string& strPath)
{
	// Store handle to keep asset alive (the clip is owned by the asset)
	AnimationHandle xHandle(strPath);
	Zenith_AnimationAsset* pxAnimAsset = xHandle.Get();
	Zenith_Assert(pxAnimAsset != nullptr, "Failed to load animation asset from: %s", strPath.c_str());

	// Get the clip (asset retains ownership)
	Flux_AnimationClip* pxClip = pxAnimAsset->GetClip();
	Zenith_Assert(pxClip != nullptr, "Animation asset has no clip: %s", strPath.c_str());

	// Store the handle to keep the asset alive
	m_xAnimationAssets.PushBack(std::move(xHandle));

	// Add as a non-owning reference (asset owns the clip)
	m_xClipCollection.AddClipReference(pxClip);

	// Resolve clip references in state machine
	if (m_pxStateMachine)
	{
		m_pxStateMachine->ResolveClipReferences(&m_xClipCollection);
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

Flux_AnimatorStateInfo Flux_AnimationController::GetCurrentAnimatorStateInfo() const
{
#ifdef ZENITH_TOOLS
	// Direct clip playback takes priority (matches Update evaluation order)
	if (m_pxDirectPlayNode)
	{
		Flux_AnimatorStateInfo xInfo;
		Flux_AnimationClip* pxClip = m_pxDirectPlayNode->GetClip();
		if (pxClip)
		{
			xInfo.m_strStateName = pxClip->GetName();
			xInfo.m_fLength = pxClip->GetDuration();
			xInfo.m_bHasLooped = m_pxDirectPlayNode->GetNormalizedTime() > 1.0f;
			xInfo.m_fSpeed = m_pxDirectPlayNode->GetPlaybackRate();
			xInfo.m_fNormalizedTime = m_pxDirectPlayNode->GetNormalizedTime();
		}
		return xInfo;
	}
#endif

	if (m_pxStateMachine)
		return m_pxStateMachine->GetCurrentStateInfo();
	return Flux_AnimatorStateInfo();
}

void Flux_AnimationController::CrossFade(const std::string& strStateName, float fDuration)
{
	if (m_pxStateMachine)
		m_pxStateMachine->CrossFade(strStateName, fDuration);
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

//=============================================================================
// Animation Layers
//=============================================================================

Flux_AnimationLayer* Flux_AnimationController::AddLayer(const std::string& strName)
{
	Flux_AnimationLayer* pxLayer = new Flux_AnimationLayer(strName);
	if (m_pxSkeletonInstance)
	{
		pxLayer->InitializePose(m_pxSkeletonInstance->GetNumBones());
	}
	m_xLayers.PushBack(pxLayer);
	return pxLayer;
}

Flux_AnimationLayer* Flux_AnimationController::GetLayer(uint32_t uIndex)
{
	if (uIndex < m_xLayers.GetSize())
		return m_xLayers.Get(uIndex);
	return nullptr;
}

const Flux_AnimationLayer* Flux_AnimationController::GetLayer(uint32_t uIndex) const
{
	if (uIndex < m_xLayers.GetSize())
		return m_xLayers.Get(uIndex);
	return nullptr;
}

void Flux_AnimationController::SetLayerWeight(uint32_t uIndex, float fWeight)
{
	if (uIndex < m_xLayers.GetSize())
		m_xLayers.Get(uIndex)->SetWeight(fWeight);
}

#ifdef ZENITH_TOOLS
void Flux_AnimationController::PlayClip(const std::string& strClipName, float fBlendTime)
{
	Flux_AnimationClip* pxClip = m_xClipCollection.GetClip(strClipName);
	if (!pxClip)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationController] Clip not found: %s", strClipName.c_str());
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
#endif

void Flux_AnimationController::Stop()
{
#ifdef ZENITH_TOOLS
	delete m_pxDirectPlayNode;
	m_pxDirectPlayNode = nullptr;

	delete m_pxDirectTransition;
	m_pxDirectTransition = nullptr;
#endif

	m_xOutputPose.Reset();
}

void Flux_AnimationController::SetEventCallback(Flux_AnimationEventCallback pfnCallback, void* pUserData)
{
	m_pfnEventCallback = pfnCallback;
	m_pEventCallbackUserData = pUserData;
}

void Flux_AnimationController::ClearEventCallback()
{
	m_pfnEventCallback = nullptr;
	m_pEventCallbackUserData = nullptr;
}

void Flux_AnimationController::ProcessEvents(float fPrevTime, float fCurrentTime)
{
	if (!m_pfnEventCallback)
		return;

	// Get current clip for event checking
	Flux_AnimationClip* pxClip = nullptr;
#ifdef ZENITH_TOOLS
	if (m_pxDirectPlayNode)
	{
		pxClip = m_pxDirectPlayNode->GetClip();
	}
#endif

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
			m_pfnEventCallback(m_pEventCallbackUserData, xEvent.m_strEventName, xEvent.m_xData);
		}
	}
}

void Flux_AnimationController::UploadToGPU()
{
	// GPU upload is now handled by Flux_SkeletonInstance::UploadToGPU()
	// which is called from UpdateWithSkeletonInstance()
}

#ifdef ZENITH_TOOLS
void Flux_AnimationController::DebugDraw(bool bShowBones, bool bShowIKTargets)
{
	if (!m_pxSkeletonInstance)
		return;

	// Draw bones
	if (bShowBones)
	{
		// Draw bone positions as spheres
		uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
		for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
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

	// Animation layers
	uint32_t uNumLayers = m_xLayers.GetSize();
	xStream << uNumLayers;
	for (uint32_t i = 0; i < uNumLayers; ++i)
	{
		m_xLayers.Get(i)->WriteToDataStream(xStream);
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

		// Note: Bone indices will be resolved when skeleton instance is set via Initialize()
	}

	// Animation layers
	uint32_t uNumLayers = 0;
	xStream >> uNumLayers;
	for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		delete m_xLayers.Get(i);
	m_xLayers.Clear();
	for (uint32_t i = 0; i < uNumLayers; ++i)
	{
		Flux_AnimationLayer* pxLayer = new Flux_AnimationLayer();
		pxLayer->ReadFromDataStream(xStream);
		m_xLayers.PushBack(pxLayer);
	}

	// Re-initialize pose if skeleton instance is set
	if (m_pxSkeletonInstance)
	{
		uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
		m_xOutputPose.Initialize(uNumBones);
		for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		{
			m_xLayers.Get(i)->InitializePose(uNumBones);
		}
	}
}
