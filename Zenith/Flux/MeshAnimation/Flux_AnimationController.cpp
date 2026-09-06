#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"   // WU-6.2: the .zanimctrl payload
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_BoneMaskAsset.h"               // WU-6.2: a layer's .zanimmask reference

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
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
	, m_xSkeletonAsset(std::move(xOther.m_xSkeletonAsset))
	, m_xClipCollection(std::move(xOther.m_xClipCollection))
	, m_xAnimationAssets(std::move(xOther.m_xAnimationAssets))
	, m_pxStateMachine(xOther.m_pxStateMachine)
	, m_pxIKSolver(xOther.m_pxIKSolver)
	, m_xParameters(std::move(xOther.m_xParameters))
	, m_bParametersPublished(xOther.m_bParametersPublished)
	, m_xOutputPose(std::move(xOther.m_xOutputPose))
	, m_bPaused(xOther.m_bPaused)
	, m_fPlaybackSpeed(xOther.m_fPlaybackSpeed)
	, m_eUpdateMode(xOther.m_eUpdateMode)
#ifdef ZENITH_TOOLS
	, m_pxDirectPlayNode(xOther.m_pxDirectPlayNode)
	, m_pxDirectTransition(xOther.m_pxDirectTransition)
#endif
	, m_xWorldMatrix(xOther.m_xWorldMatrix)
	, m_xLayers(std::move(xOther.m_xLayers))
	// The counter travels WITH the layers: it describes the ids already handed
	// out to the list we have just taken, so restarting it at 0 here would mint a
	// duplicate on the very next AddLayer.
	, m_uNextLayerId(xOther.m_uNextLayerId)
	, m_xTempBlendPose(std::move(xOther.m_xTempBlendPose))
	, m_xScaledMaskWeights(std::move(xOther.m_xScaledMaskWeights))
	, m_pfnEventCallback(xOther.m_pfnEventCallback)
	, m_pEventCallbackUserData(xOther.m_pEventCallbackUserData)
	, m_fLastEventCheckTime(xOther.m_fLastEventCheckTime)
	, m_bEmitEventsOnSeek(xOther.m_bEmitEventsOnSeek)
	, m_pDriveOwner(xOther.m_pDriveOwner)
	, m_ulDriveFrameToken(xOther.m_ulDriveFrameToken)
{
	// The moved-from controller drives nothing: leaving the claim behind would let
	// a caller that still holds the old object read itself as this frame's driver.
	xOther.m_pDriveOwner = nullptr;
	xOther.m_ulDriveFrameToken = ulNO_DRIVE_FRAME;

	// Null out moved-from object's owned pointers to prevent double-delete
	xOther.m_pxStateMachine = nullptr;
	xOther.m_pxIKSolver = nullptr;
#ifdef ZENITH_TOOLS
	xOther.m_pxDirectPlayNode = nullptr;
	xOther.m_pxDirectTransition = nullptr;
#endif
	xOther.m_pxSkeletonInstance = nullptr;
	xOther.m_pfnEventCallback = nullptr;
	xOther.m_pEventCallbackUserData = nullptr;

	// ★ EVERY STATE MACHINE WE JUST TOOK STILL POINTS AT THE SOURCE'S PARAMETER
	// SET (D42). SetSharedParameters stored &xOther.m_xParameters, which is now a
	// moved-from object about to be destroyed. Re-binding is allocation-free, so
	// it is safe inside a noexcept move.
	RebindSharedParameters();
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

		// Transfer non-owned pointers / move handles
		m_pxSkeletonInstance = xOther.m_pxSkeletonInstance;
		m_xSkeletonAsset = std::move(xOther.m_xSkeletonAsset);

		// Move value types
		m_xClipCollection = std::move(xOther.m_xClipCollection);
		m_xAnimationAssets = std::move(xOther.m_xAnimationAssets);
		m_xOutputPose = std::move(xOther.m_xOutputPose);
		m_xParameters = std::move(xOther.m_xParameters);
		m_bParametersPublished = xOther.m_bParametersPublished;
		m_bPaused = xOther.m_bPaused;
		m_fPlaybackSpeed = xOther.m_fPlaybackSpeed;
		m_eUpdateMode = xOther.m_eUpdateMode;
		m_xWorldMatrix = xOther.m_xWorldMatrix;
		m_pfnEventCallback = xOther.m_pfnEventCallback;
		m_pEventCallbackUserData = xOther.m_pEventCallbackUserData;
		m_fLastEventCheckTime = xOther.m_fLastEventCheckTime;
		m_bEmitEventsOnSeek = xOther.m_bEmitEventsOnSeek;
		m_pDriveOwner = xOther.m_pDriveOwner;
		m_ulDriveFrameToken = xOther.m_ulDriveFrameToken;

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
		// See the move ctor: the counter belongs to the layer list, not to the object.
		m_uNextLayerId = xOther.m_uNextLayerId;
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
		xOther.m_pfnEventCallback = nullptr;
		xOther.m_pEventCallbackUserData = nullptr;
		// See the move ctor: a moved-from controller drives nothing.
		xOther.m_pDriveOwner = nullptr;
		xOther.m_ulDriveFrameToken = ulNO_DRIVE_FRAME;

		// D42: the machines we just took are pointed at the SOURCE's set.
		RebindSharedParameters();
	}
	return *this;
}

void Flux_AnimationController::Initialize(Flux_SkeletonInstance* pxSkeleton)
{
	// ★ CLEARED FIRST AND UNCONDITIONALLY — see the header. Initialize(nullptr) is
	// the DETACH, and it used to drop only the instance pointer while the skeleton
	// handle kept its AddRef'd cached pointer forever. Behaviour-neutral on an
	// attach: Set() releases the old reference before taking the new one, so this
	// only changes the nullptr case.
	m_xSkeletonAsset.Clear();

	m_pxSkeletonInstance = pxSkeleton;

	if (m_pxSkeletonInstance)
	{
		// Get skeleton asset for bone hierarchy info (handle keeps it alive)
		m_xSkeletonAsset.Set(m_pxSkeletonInstance->GetSourceSkeleton());

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

void Flux_AnimationController::ReleaseAssetReferences()
{
	// The three go together — see the header. Order is deliberate: the BORROWED
	// pointers go before the handles that pin them, so there is no instant, however
	// brief, at which the collection names a clip nothing is keeping alive.
	m_xClipCollection.Clear();

	// Zenith_Vector::Clear destroys each element, and ~Zenith_AssetHandle releases —
	// but the explicit Clear() states the intent at the one place a reader looks for
	// it, and leaves nothing depending on how the container tears an element down.
	for (u_int u = 0; u < m_xAnimationAssets.GetSize(); ++u)
	{
		m_xAnimationAssets.Get(u).Clear();
	}
	m_xAnimationAssets.Clear();

	m_xSkeletonAsset.Clear();
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
	return m_xClipCollection.GetClips().GetSize() != 0 ||
		m_pxStateMachine != nullptr ||
		m_xLayers.GetSize() > 0;
}

//=============================================================================
// Parameters (D42) — see the header for why the controller owns the live set.
//=============================================================================

void Flux_AnimationController::RebindSharedParameters() noexcept
{
	// Bind only. No seeding, no allocation — this is what the noexcept move
	// operations call to repair pointers aimed at the moved-from set.
	if (m_pxStateMachine)
		m_pxStateMachine->SetSharedParameters(&m_xParameters);

	for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
	{
		Flux_AnimationLayer* pxLayer = m_xLayers.Get(i);
		// GetStateMachine() would CREATE one; a layer without a machine has
		// nothing to bind and must not gain an empty one as a side effect of a
		// bind sweep.
		if (pxLayer && pxLayer->GetStateMachinePtr())
			pxLayer->GetStateMachine().SetSharedParameters(&m_xParameters);
	}
}

void Flux_AnimationController::PublishSharedParameters()
{
	// Seed FIRST, bind second. A machine bound before its own declarations were
	// copied across would answer GetParameters() with a set that does not carry
	// them, and a condition reading one would evaluate false for a frame.
	if (m_pxStateMachine)
		m_pxStateMachine->GetDef().SeedParametersInto(m_xParameters);

	for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
	{
		Flux_AnimationLayer* pxLayer = m_xLayers.Get(i);
		if (pxLayer && pxLayer->GetStateMachinePtr())
			pxLayer->GetStateMachine().GetDef().SeedParametersInto(m_xParameters);
	}

	RebindSharedParameters();
	m_bParametersPublished = true;
}

void Flux_AnimationController::EnsureParameterDeclared(const std::string& strName)
{
	if (!m_bParametersPublished || !m_xParameters.HasParameter(strName))
		PublishSharedParameters();
}

void Flux_AnimationController::SetFloat(const std::string& strName, float fValue)
{
	EnsureParameterDeclared(strName);
	m_xParameters.SetFloat(strName, fValue);
}

void Flux_AnimationController::SetInt(const std::string& strName, int32_t iValue)
{
	EnsureParameterDeclared(strName);
	m_xParameters.SetInt(strName, iValue);
}

void Flux_AnimationController::SetBool(const std::string& strName, bool bValue)
{
	EnsureParameterDeclared(strName);
	m_xParameters.SetBool(strName, bValue);
}

void Flux_AnimationController::SetTrigger(const std::string& strName)
{
	EnsureParameterDeclared(strName);
	m_xParameters.SetTrigger(strName);
}

float Flux_AnimationController::GetFloat(const std::string& strName) const
{
	return m_xParameters.GetFloat(strName);
}

int32_t Flux_AnimationController::GetInt(const std::string& strName) const
{
	return m_xParameters.GetInt(strName);
}

bool Flux_AnimationController::GetBool(const std::string& strName) const
{
	return m_xParameters.GetBool(strName);
}

void Flux_AnimationController::Update(float fDt)
{
	if (!m_pxSkeletonInstance || !m_xSkeletonAsset.GetDirect() || m_bPaused)
		return;

	// D42: authoring finishes before the first frame, so this is where the live
	// set is built. Latched — a per-frame seed would walk every def's declaration
	// table for nothing.
	if (!m_bParametersPublished)
		PublishSharedParameters();

	// #TODO: Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
	// Currently only ANIMATION_UPDATE_NORMAL is functional
	Zenith_Assert(m_eUpdateMode == ANIMATION_UPDATE_NORMAL,
		"Flux_AnimationUpdateMode FIXED/UNSCALED not yet implemented");
	fDt *= m_fPlaybackSpeed;

	UpdateWithSkeletonInstance(fDt);

	// WU-5A (D34): event delivery, on EVERY path and in EVERY build. This used
	// to be an #ifdef ZENITH_TOOLS block gated on the direct-play node — see the
	// Events section of the header for what that meant for shipping games.
	DispatchClipEvents();
}

// Multi-layer path: tick all layers, then compose layer 1+ on top of layer 0
// using each layer's blend mode (additive / override / masked override).
void Flux_AnimationController::EvaluateAndComposeLayers(float fDt)
{
	const uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();

	for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
	{
		m_xLayers.Get(i)->Update(fDt, *m_xSkeletonAsset.GetDirect());
	}

	// Layer 0 is the base; later layers compose on top.
	m_xOutputPose.CopyFrom(m_xLayers.Get(0)->GetOutputPose());

	if (m_xTempBlendPose.GetNumBones() != uNumBones)
		m_xTempBlendPose.Initialize(uNumBones);

	for (uint32_t i = 1; i < m_xLayers.GetSize(); ++i)
	{
		Flux_AnimationLayer* pxLayer = m_xLayers.Get(i);
		const float fWeight = pxLayer->GetWeight();
		if (fWeight <= 0.0f) continue;

		const Flux_SkeletonPose& xLayerPose = pxLayer->GetOutputPose();

		if (pxLayer->GetBlendMode() == LAYER_BLEND_ADDITIVE)
		{
			Flux_SkeletonPose::AdditiveBlend(m_xTempBlendPose, m_xOutputPose, xLayerPose, fWeight);
		}
		else if (pxLayer->HasAvatarMask())
		{
			// Masked override: per-bone weights × layer weight.
			const Zenith_Vector<float>& xWeights = pxLayer->GetAvatarMask().GetWeights();
			m_xScaledMaskWeights.Clear();
			m_xScaledMaskWeights.Reserve(xWeights.GetSize());
			for (u_int j = 0; j < xWeights.GetSize(); ++j)
				m_xScaledMaskWeights.PushBack(xWeights.Get(j) * fWeight);
			Flux_SkeletonPose::MaskedBlend(m_xTempBlendPose, m_xOutputPose, xLayerPose, m_xScaledMaskWeights);
		}
		else
		{
			// Full override: single-weight blend.
			Flux_SkeletonPose::Blend(m_xTempBlendPose, m_xOutputPose, xLayerPose, fWeight);
		}
		m_xOutputPose.CopyFrom(m_xTempBlendPose);
	}
}

// PURE. See the header: wrapped when the clip loops, clamped when it does not,
// and returned UNCHANGED for a clip with no duration — the pre-WU-2.4 tick did
// exactly that, and a zero-duration clip has no range to fold into.
float Flux_AnimationController::WrapClipTime(const Flux_AnimationClip& xClip, float fTimeSeconds)
{
	const float fDuration = xClip.GetDuration();
	if (fDuration <= 0.0f)
		return fTimeSeconds;

	if (xClip.IsLooping())
	{
		float fWrapped = fmod(fTimeSeconds, fDuration);
		if (fWrapped < 0.0f) fWrapped += fDuration;
		return fWrapped;
	}
	return glm::clamp(fTimeSeconds, 0.0f, fDuration);
}

#ifdef ZENITH_TOOLS
// Seed the output pose with bind pose values (so bones without a channel in this
// clip keep bind pose rather than identity), then sample the clip on top at the
// node's CURRENT timestamp.
//
// ★ SHARED BY THE TICK AND THE SEEK. Two copies of this would let "the pose at
// time t" mean two different things depending on how you got there, which is the
// one property a scrub has to preserve.
void Flux_AnimationController::SampleDirectPlayPoseAtCurrentTime()
{
	Flux_AnimationClip* pxClip = m_pxDirectPlayNode->GetClip();
	if (!pxClip) return;

	const uint32_t uNumBones = m_pxSkeletonInstance->GetNumBones();
	for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		const Zenith_SkeletonAsset::Bone& xBone = m_xSkeletonAsset.GetDirect()->GetBone(i);
		Flux_BoneLocalPose& xPose = m_xOutputPose.GetLocalPose(i);
		xPose.m_xPosition = xBone.m_xBindPosition;
		xPose.m_xRotation = xBone.m_xBindRotation;
		xPose.m_xScale = xBone.m_xBindScale;
	}

	m_xOutputPose.SampleFromClip(*pxClip, m_pxDirectPlayNode->GetCurrentTimestamp(), *m_xSkeletonAsset.GetDirect());
}

// Editor-only direct-clip preview path: advance playback time (with looping or
// clamping), sample the pose at the new time, and apply any active crossfade
// snapshot.
void Flux_AnimationController::UpdateDirectPlayPose(float fDt)
{
	Flux_AnimationClip* pxClip = m_pxDirectPlayNode->GetClip();
	if (!pxClip) return;

	const float fCurrentTime = WrapClipTime(*pxClip,
		m_pxDirectPlayNode->GetCurrentTimestamp() + fDt * m_pxDirectPlayNode->GetPlaybackRate());
	m_pxDirectPlayNode->SetCurrentTimestamp(fCurrentTime);

	SampleDirectPlayPoseAtCurrentTime();

	// Optional crossfade between direct clips — blends from the snapshot pose.
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
}
#endif

void Flux_AnimationController::UpdateWithSkeletonInstance(float fDt)
{
	// Animation path for the new model-instance system (Flux_SkeletonInstance).

	if (m_xLayers.GetSize() > 0)
	{
		EvaluateAndComposeLayers(fDt);
		ApplyOutputPoseToSkeleton();
		return;
	}

#ifdef ZENITH_TOOLS
	// Direct clip playback (editor preview) takes priority over the state machine.
	if (m_pxDirectPlayNode)
	{
		UpdateDirectPlayPose(fDt);
		ApplyOutputPoseToSkeleton();
		return;
	}
#endif

	if (m_pxStateMachine)
	{
		m_pxStateMachine->Update(fDt, m_xOutputPose, *m_xSkeletonAsset.GetDirect());
		ApplyOutputPoseToSkeleton();
	}
	// Otherwise: no animation playing — skeleton instance stays at the bind pose
	// it was created with. No action needed.
}

void Flux_AnimationController::ApplyOutputPoseToSkeleton()
{
	// Run IK before copying the pose to the skeleton instance. Pre-solve recompute
	// gives the FABRIK chain a fresh model-space frame to read from; post-solve
	// recompute keeps m_xOutputPose's model matrices consistent for downstream
	// CPU readers (debug draw, gizmos, animation tools).
	if (m_pxIKSolver && !m_pxIKSolver->GetChains().IsEmpty() && m_xSkeletonAsset.GetDirect())
	{
		const Zenith_SkeletonAsset& xSkel = *m_xSkeletonAsset.GetDirect();
		m_xOutputPose.ComputeModelSpaceMatricesFromSkeleton(xSkel);
		m_pxIKSolver->Solve(m_xOutputPose, xSkel, m_xWorldMatrix);
		m_xOutputPose.ComputeModelSpaceMatricesFromSkeleton(xSkel);
	}

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
	// (No GPU bone-buffer upload — the unified compute-skinning path reads the CPU skinning
	// matrices directly via GetSkinningMatrices; the legacy per-frame bone CBV was retired.)
}

const Zenith_Maths::Matrix4* Flux_AnimationController::GetSkinningMatrices() const
{
	return m_xOutputPose.GetSkinningMatrices();
}

Flux_AnimationClip* Flux_AnimationController::AddClipFromFile(const std::string& strPath)
{
	// Resolve through the registry first so we have a concrete pointer.
	Zenith_AnimationAsset* pxAnimAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strPath);
	Zenith_Assert(pxAnimAsset != nullptr, "Failed to load animation asset from: %s", strPath.c_str());

	// Get the clip (asset retains ownership)
	Flux_AnimationClip* pxClip = pxAnimAsset->GetClip();
	Zenith_Assert(pxClip != nullptr, "Animation asset has no clip: %s", strPath.c_str());

	// Build a handle that actually holds a ref to the asset. A path-only handle
	// (AnimationHandle(strPath) without Resolve/Set) does NOT increment refcount
	// until the cached pointer is populated — UnloadUnused would otherwise free
	// the asset out from under m_xClipCollection. Set() wires up the AddRef now.
	AnimationHandle xHandle;
	xHandle.Set(pxAnimAsset);
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
		m_bParametersPublished = false;   // D42: a new graph, so re-seed
	}
	return *m_pxStateMachine;
}

Flux_AnimationStateMachine* Flux_AnimationController::CreateStateMachine(const std::string& strName)
{
	delete m_pxStateMachine;
	m_pxStateMachine = new Flux_AnimationStateMachine(strName);
	m_bParametersPublished = false;
	return m_pxStateMachine;
}

Flux_AnimationStateMachine* Flux_AnimationController::BuildStateMachineFromDef(const Flux_AnimationStateMachineDef& xDef)
{
	delete m_pxStateMachine;
	m_pxStateMachine = new Flux_AnimationStateMachine();
	m_pxStateMachine->BuildFromDef(xDef, &m_xClipCollection);
	m_bParametersPublished = false;
	return m_pxStateMachine;
}

//=============================================================================
// Whole-controller build / export (WU-6.2)
//=============================================================================
bool Flux_AnimationController::BuildFromControllerDef(const Flux_AnimatorControllerDef& xDef,
	const Zenith_SkeletonAsset* pxSkeletonForMasks)
{
	bool bComplete = true;

	// (1) CLIPS FIRST, AND THAT ORDER IS LOAD-BEARING. Every state machine below
	// resolves its clip references by NAME through m_xClipCollection, so a machine
	// built before its clips are in the collection resolves nothing and poses the
	// bind pose forever — silently, because an unresolved leaf resets rather than
	// asserting.
	const Zenith_Vector<std::string>& xClipPaths = xDef.GetClipPaths();
	for (u_int u = 0; u < xClipPaths.GetSize(); ++u)
	{
		const std::string& strClipPath = xClipPaths.Get(u);
		if (strClipPath.empty())
		{
			continue;
		}

		// Resolved here rather than inside AddClipFromFile so a missing clip is a
		// named error instead of that function's bare assert.
		Zenith_AnimationAsset* pxAnimAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strClipPath);
		if (pxAnimAsset == nullptr || pxAnimAsset->GetClip() == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_ANIMATION,
				"[AnimatorController] def '%s' names clip '%s', which did not load — every state referencing it will pose the bind pose",
				xDef.GetName().c_str(), strClipPath.c_str());
			bComplete = false;
			continue;
		}
		AddClipFromFile(strClipPath);
	}

	// (2) Layers are rebuilt wholesale — the def describes the WHOLE controller,
	// so anything already here is stale.
	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		delete m_xLayers.Get(u);
	}
	m_xLayers.Clear();

	// (3) Top-level state machine. A def with none means a purely layered
	// controller, and dropping the machine is part of describing that.
	if (xDef.GetStateMachineDef() != nullptr)
	{
		BuildStateMachineFromDef(*xDef.GetStateMachineDef());
	}
	else
	{
		delete m_pxStateMachine;
		m_pxStateMachine = nullptr;
	}

	// (4) Layers.
	for (u_int u = 0; u < xDef.GetLayerCount(); ++u)
	{
		const Flux_AnimatorControllerLayerDef* pxLayerDef = xDef.GetLayer(u);
		if (pxLayerDef == nullptr)
		{
			continue;
		}

		Flux_AnimationLayer* pxLayer = AddLayer(pxLayerDef->GetName());
		// ★ THE DEF'S ID WINS, AND THE COUNTER FOLLOWS IT (WU-6.3). AddLayer has
		// just minted one; adopting the def's without moving the counter past it
		// is how the NEXT AddLayer mints a number a rebuilt layer already holds,
		// at which point GetLayerById answers with whichever comes first.
		AdoptLayerId(*pxLayer, pxLayerDef->GetLayerId());
		pxLayer->SetWeight(pxLayerDef->GetWeight());
		pxLayer->SetBlendMode(pxLayerDef->GetBlendMode());
		pxLayer->SetEmitEvents(pxLayerDef->GetEmitEvents());
		pxLayer->SetBoneMaskAssetPath(pxLayerDef->GetBoneMaskAssetPath());

		const std::string& strMaskPath = pxLayerDef->GetBoneMaskAssetPath();
		if (!strMaskPath.empty())
		{
			if (pxSkeletonForMasks == nullptr)
			{
				// ★ NOT A SILENT SKIP. An unmasked override layer replaces the WHOLE
				// skeleton, so losing a mask does not make the layer do less — it makes
				// it do far more, which reads as "the legs stopped animating".
				Zenith_Error(LOG_CATEGORY_ANIMATION,
					"[AnimatorController] layer '%s' names bone mask '%s' but BuildFromControllerDef was given no skeleton to resolve it against",
					pxLayerDef->GetName().c_str(), strMaskPath.c_str());
				bComplete = false;
			}
			else
			{
				Zenith_BoneMaskAsset* pxMaskAsset = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(strMaskPath);
				if (pxMaskAsset == nullptr)
				{
					Zenith_Error(LOG_CATEGORY_ANIMATION,
						"[AnimatorController] layer '%s' names bone mask '%s', which did not load",
						pxLayerDef->GetName().c_str(), strMaskPath.c_str());
					bComplete = false;
				}
				else
				{
					Flux_BoneMask xResolved;
					// ResolveTo reports each unresolvable bone NAME itself; a partial
					// resolve still installs what it could.
					if (!pxMaskAsset->ResolveTo(*pxSkeletonForMasks, xResolved))
					{
						bComplete = false;
					}
					if (pxMaskAsset->HasAvatarMask())
					{
						pxLayer->SetAvatarMask(xResolved);
					}
				}
			}
		}

		Flux_AnimationStateMachine* pxMachine = pxLayer->CreateStateMachine(pxLayerDef->GetName());
		pxMachine->BuildFromDef(pxLayerDef->GetStateMachineDef(), &m_xClipCollection);
	}

	// D42: a whole new graph, declarations and all.
	m_bParametersPublished = false;
	PublishSharedParameters();

	return bComplete;
}

bool Flux_AnimationController::ExportControllerDef(Flux_AnimatorControllerDef& xOutDef) const
{
	xOutDef.Clear();

	bool bComplete = true;

	// ★ CLIP PATHS COME FROM THE ASSETS, NOT FROM THE HANDLES AND NOT FROM THE
	// CLIP COLLECTION. The collection holds borrowed clip POINTERS keyed by clip
	// NAME, and a clip's name is not its file path. The handles are no better:
	// AddClipFromFile populates each one with Set(pxAsset), and
	// Zenith_AssetHandle::Set deliberately CLEARS the path (it is the procedural
	// entry point), so every handle in m_xAnimationAssets reports "". The asset
	// itself is the only thing that still knows where it came from.
	for (u_int u = 0; u < m_xAnimationAssets.GetSize(); ++u)
	{
		const Zenith_AnimationAsset* pxAsset = m_xAnimationAssets.Get(u).GetDirect();
		const std::string strPath = (pxAsset != nullptr) ? pxAsset->GetPath() : std::string();
		if (strPath.empty())
		{
			// A procedural clip has no file to name, so a def cannot reproduce it.
			Zenith_Error(LOG_CATEGORY_ANIMATION,
				"[AnimatorController] ExportControllerDef: a clip in this controller has no asset path (procedural) and cannot be written to a "
				ZENITH_ANIMCTRL_EXT);
			bComplete = false;
			continue;
		}
		xOutDef.AddClipPath(strPath);
	}

	if (m_pxStateMachine != nullptr)
	{
		xOutDef.GetOrCreateStateMachineDef().CopyFrom(m_pxStateMachine->GetDef());
	}

	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		const Flux_AnimationLayer* pxLayer = m_xLayers.Get(u);
		Flux_AnimatorControllerLayerDef* pxLayerDef = xOutDef.AddLayer(pxLayer->GetName());
		// The id the RUNTIME layer carries wins over the fresh one AddLayer just
		// handed out — an export must not renumber the layers it is describing.
		// AssignLayerId, not SetLayerId, so the def's counter moves past it too.
		xOutDef.AssignLayerId(*pxLayerDef, pxLayer->GetLayerId());
		pxLayerDef->SetWeight(pxLayer->GetWeight());
		pxLayerDef->SetBlendMode(pxLayer->GetBlendMode());
		pxLayerDef->SetEmitEvents(pxLayer->GetEmitEvents());
		pxLayerDef->SetBoneMaskAssetPath(pxLayer->GetBoneMaskAssetPath());

		// ★ A MASK WITH NO PATH IS UNWRITEABLE, AND SAYING SO IS THE WHOLE VALUE OF
		// THIS BRANCH. Flux_BoneMask is resolved and index-based; there is nothing
		// in it to turn back into a .zanimmask. A controller masked by hand (rather
		// than built from a def) therefore exports a mask-less layer, and the caller
		// is told rather than discovering it when the save silently unmasks.
		if (pxLayer->HasAvatarMask() && pxLayer->GetBoneMaskAssetPath().empty())
		{
			Zenith_Error(LOG_CATEGORY_ANIMATION,
				"[AnimatorController] ExportControllerDef: layer '%s' has an avatar mask but no " ZENITH_ANIMMASK_EXT
				" path — the mask cannot be written and this layer exports UNMASKED",
				pxLayer->GetName().c_str());
			bComplete = false;
		}

		const Flux_AnimationStateMachine* pxMachine = pxLayer->GetStateMachinePtr();
		if (pxMachine != nullptr)
		{
			pxLayerDef->GetStateMachineDef().CopyFrom(pxMachine->GetDef());
		}
	}

	return bComplete;
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
	// WU-6.3 (D43): EVERY creation path mints, not just BuildFromControllerDef.
	// An imperatively-authored controller — which is every game in the tree —
	// would otherwise hand out a list of layers all carrying the same
	// uFLUX_INVALID_LAYER_ID, and GetLayerById could not tell them apart.
	pxLayer->SetLayerId(m_uNextLayerId++);
	if (m_pxSkeletonInstance)
	{
		pxLayer->InitializePose(m_pxSkeletonInstance->GetNumBones());
	}
	m_xLayers.PushBack(pxLayer);
	// D42: the game is about to create this layer's machine and declare its
	// parameters, so the live set is out of date from here.
	m_bParametersPublished = false;
	return pxLayer;
}

void Flux_AnimationController::AdoptLayerId(Flux_AnimationLayer& xLayer, u_int uLayerId)
{
	if (uLayerId == uFLUX_INVALID_LAYER_ID)
	{
		// The source carries no id (a def written before ids existed, or one
		// authored by hand). The minted one stands rather than being replaced
		// with the sentinel, because a layer a controller owns always has an id.
		return;
	}

	xLayer.SetLayerId(uLayerId);
	if (uLayerId >= m_uNextLayerId)
	{
		m_uNextLayerId = uLayerId + 1u;
	}
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

Flux_AnimationLayer* Flux_AnimationController::GetLayerById(u_int uLayerId)
{
	// Const-correct twin below; the walk is duplicated rather than const_cast'd
	// through, which is three lines either way.
	if (uLayerId == uFLUX_INVALID_LAYER_ID)
		return nullptr;

	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		Flux_AnimationLayer* pxLayer = m_xLayers.Get(u);
		if (pxLayer != nullptr && pxLayer->GetLayerId() == uLayerId)
			return pxLayer;
	}
	return nullptr;
}

const Flux_AnimationLayer* Flux_AnimationController::GetLayerById(u_int uLayerId) const
{
	if (uLayerId == uFLUX_INVALID_LAYER_ID)
		return nullptr;

	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		const Flux_AnimationLayer* pxLayer = m_xLayers.Get(u);
		if (pxLayer != nullptr && pxLayer->GetLayerId() == uLayerId)
			return pxLayer;
	}
	return nullptr;
}

Flux_AnimationLayer* Flux_AnimationController::GetLayerByName(const std::string& strName)
{
	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		Flux_AnimationLayer* pxLayer = m_xLayers.Get(u);
		if (pxLayer != nullptr && pxLayer->GetName() == strName)
			return pxLayer;
	}
	return nullptr;
}

const Flux_AnimationLayer* Flux_AnimationController::GetLayerByName(const std::string& strName) const
{
	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		const Flux_AnimationLayer* pxLayer = m_xLayers.Get(u);
		if (pxLayer != nullptr && pxLayer->GetName() == strName)
			return pxLayer;
	}
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

	// WU-5A: the new node starts at time zero, so the mark has to as well. A
	// mark left at the old clip's playhead would either swallow every event
	// before it or, on a lower value, fire the whole opening stretch at once.
	m_fLastEventCheckTime = 0.0f;
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

	m_fLastEventCheckTime = 0.0f;
	m_xOutputPose.Reset();
}

//=============================================================================
// Direct-play scrubbing (WU-2.4) — see the header for why this exists.
//=============================================================================

bool Flux_AnimationController::HasDirectPlayClip() const
{
#ifdef ZENITH_TOOLS
	return m_pxDirectPlayNode != nullptr && m_pxDirectPlayNode->GetClip() != nullptr;
#else
	return false;
#endif
}

float Flux_AnimationController::GetDirectPlayTime() const
{
#ifdef ZENITH_TOOLS
	return m_pxDirectPlayNode ? m_pxDirectPlayNode->GetCurrentTimestamp() : 0.0f;
#else
	return 0.0f;
#endif
}

bool Flux_AnimationController::SeekDirectPlay(float fTimeSeconds)
{
#ifdef ZENITH_TOOLS
	if (!m_pxDirectPlayNode || !m_pxSkeletonInstance || !m_xSkeletonAsset.GetDirect())
		return false;

	Flux_AnimationClip* pxClip = m_pxDirectPlayNode->GetClip();
	if (!pxClip)
		return false;

	m_pxDirectPlayNode->SetCurrentTimestamp(WrapClipTime(*pxClip, fTimeSeconds));
	SampleDirectPlayPoseAtCurrentTime();
	ApplyOutputPoseToSkeleton();

	// D40: the mark MOVES even when nothing is emitted. See SetEmitEventsOnSeek —
	// leaving it behind makes the next forward tick replay the whole skipped span
	// as one burst.
	const float fNormalized = m_pxDirectPlayNode->GetNormalizedTime();
	if (m_bEmitEventsOnSeek)
	{
		// A scrub has no playback direction of its own, so "forward" is simply
		// whether the playhead moved forward across the clip. A BACKWARD scrub
		// emits nothing even with the flag on — same rule as reverse playback
		// (D39), and firing a clip's events in reverse order is not something
		// any listener is written for.
		EmitDirectPlaySpan(m_fLastEventCheckTime, fNormalized, fNormalized >= m_fLastEventCheckTime);
	}
	m_fLastEventCheckTime = fNormalized;
	return true;
#else
	(void)fTimeSeconds;
	return false;
#endif
}

bool Flux_AnimationController::TryBeginFrameDrive(const void* pDriver, u_int64 ulFrameToken)
{
	// A claim already made in THIS frame refuses every later one, the same
	// driver's included — a second tick is a second tick whoever asks for it.
	if (m_pDriveOwner != nullptr && m_ulDriveFrameToken == ulFrameToken)
		return false;

	m_pDriveOwner = pDriver;
	m_ulDriveFrameToken = ulFrameToken;
	return true;
}

void Flux_AnimationController::ClearFrameDrive(const void* pDriver)
{
	// Only the current owner may release: a dispossessed caller tidying up must
	// not hand the frame back to a driver that never had it.
	if (m_pDriveOwner == pDriver)
	{
		m_pDriveOwner = nullptr;
		m_ulDriveFrameToken = ulNO_DRIVE_FRAME;
	}
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

//=============================================================================
// Event delivery (WU-5A) — see the Events section of the header for D34-D40.
//=============================================================================

bool Flux_AnimationController::SpanContainsEventTime(const Flux_ClipEventSpan& xSpan, float fEventNormalizedTime)
{
	// D39. Reverse is a no-event EARLY RETURN, not an assert: SetPlaybackSpeed
	// accepts negatives and shipping content uses them, so clamping or
	// asserting here would change behaviour a game already relies on.
	if (!xSpan.m_bForward)
		return false;

	float fEvent = fEventNormalizedTime;

	// D38: 1.0 IS 0.0 of the next loop, on a looping clip. fmod, not a subtract,
	// so a time authored past 1.0 folds too instead of landing somewhere the
	// span can never reach.
	if (xSpan.m_bLooping && fEvent >= 1.0f)
		fEvent = fmod(fEvent, 1.0f);

	const float fPrev = xSpan.m_fPrevNormalizedTime;
	const float fCurr = xSpan.m_fCurrNormalizedTime;

	if (xSpan.m_bWrapped)
		return fEvent >= fPrev || fEvent < fCurr;   // [prev, 1) U [0, curr)

	if (xSpan.m_bReachedEnd)
		return fEvent >= fPrev && fEvent <= fCurr;  // [prev, curr] — the one closed end

	return fEvent >= fPrev && fEvent < fCurr;       // [prev, curr)
}

void Flux_AnimationController::EmitSpanEvents(const Flux_ClipEventSpan& xSpan)
{
	if (!m_pfnEventCallback || !xSpan.m_pxClip)
		return;

	const Zenith_Vector<Flux_AnimationEvent>& xEvents = xSpan.m_pxClip->GetEvents();
	for (u_int u = 0; u < xEvents.GetSize(); ++u)
	{
		const Flux_AnimationEvent& xEvent = xEvents.Get(u);
		if (SpanContainsEventTime(xSpan, xEvent.m_fNormalizedTime))
			m_pfnEventCallback(m_pEventCallbackUserData, xEvent.m_strEventName, xEvent.m_xData);
	}
}

void Flux_AnimationController::DispatchCollectedSpans()
{
	// D35: ONE emitter per layer — the highest blend weight wins, and a tie goes
	// to the LOWEST leaf index, which is collection order (depth-first, children
	// in declaration order). The comparison is STRICTLY greater so the first of
	// an equal pair keeps the win, and fBest starts at zero so a zero-weight
	// leaf can never take it.
	//
	// A blend weight does not change during one evaluate, so "highest weight at
	// the crossing instant" reduces to one winner for the whole frame.
	u_int uWinner = m_xEventSpanScratch.GetSize();
	float fBestWeight = 0.0f;
	for (u_int u = 0; u < m_xEventSpanScratch.GetSize(); ++u)
	{
		const float fWeight = m_xEventSpanScratch.Get(u).m_fWeight;
		if (fWeight > fBestWeight)
		{
			fBestWeight = fWeight;
			uWinner = u;
		}
	}

	if (uWinner < m_xEventSpanScratch.GetSize())
		EmitSpanEvents(m_xEventSpanScratch.Get(uWinner));
}

#ifdef ZENITH_TOOLS
void Flux_AnimationController::EmitDirectPlaySpan(float fPrevNormalizedTime, float fCurrNormalizedTime, bool bForward)
{
	Flux_AnimationClip* pxClip = m_pxDirectPlayNode ? m_pxDirectPlayNode->GetClip() : nullptr;
	if (!pxClip || pxClip->GetDuration() <= 0.0f)
		return;

	Flux_ClipEventSpan xSpan;
	xSpan.m_pxClip = pxClip;
	xSpan.m_fPrevNormalizedTime = fPrevNormalizedTime;
	xSpan.m_fCurrNormalizedTime = fCurrNormalizedTime;
	xSpan.m_fWeight = 1.0f;      // a direct-play preview is the only thing playing
	xSpan.m_bForward = bForward;
	xSpan.m_bLooping = pxClip->IsLooping();
	// The direct-play mark is normalized and already wrapped by WrapClipTime, so
	// unlike a blend-tree leaf there is no raw advanced time to read the wrap
	// off; curr < prev on a looping clip IS the wrap here.
	xSpan.m_bWrapped = xSpan.m_bLooping && bForward && fCurrNormalizedTime < fPrevNormalizedTime;
	xSpan.m_bReachedEnd = !xSpan.m_bLooping && bForward
		&& fCurrNormalizedTime >= 1.0f && fPrevNormalizedTime < 1.0f;

	EmitSpanEvents(xSpan);
}
#endif

void Flux_AnimationController::DispatchClipEvents()
{
	// The order MIRRORS UpdateWithSkeletonInstance exactly — layers, then the
	// editor's direct-play preview, then the state machine. Anything else would
	// dispatch events from a path that did not produce this frame's pose.
	if (m_xLayers.GetSize() > 0)
	{
		// D36: each layer arbitrates and emits on its own.
		for (uint32_t i = 0; i < m_xLayers.GetSize(); ++i)
		{
			m_xEventSpanScratch.Clear();
			m_xLayers.Get(i)->CollectEventSpans(&m_xEventSpanScratch);
			DispatchCollectedSpans();
		}
		return;
	}

#ifdef ZENITH_TOOLS
	if (m_pxDirectPlayNode)
	{
		const float fPrev = m_fLastEventCheckTime;
		const float fCurr = m_pxDirectPlayNode->GetNormalizedTime();
		EmitDirectPlaySpan(fPrev, fCurr,
			(m_fPlaybackSpeed * m_pxDirectPlayNode->GetPlaybackRate()) > 0.0f);
		// D39: the mark advances whichever way the playhead went.
		m_fLastEventCheckTime = fCurr;
		return;
	}
#endif

	if (m_pxStateMachine)
	{
		// A controller with no layers is ONE layer for arbitration (D36).
		m_xEventSpanScratch.Clear();
		m_pxStateMachine->CollectEventSpans(&m_xEventSpanScratch);
		DispatchCollectedSpans();
	}
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
			g_xEngine.Primitives().AddSphere(xPos, 0.02f, Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f));
		}
	}

	// Draw IK targets
	if (bShowIKTargets && m_pxIKSolver)
	{
		const Zenith_HashMap<std::string, Flux_IKChain>& xChains = m_pxIKSolver->GetChains();
		for (Zenith_HashMap<std::string, Flux_IKChain>::Iterator xIt(xChains); !xIt.Done(); xIt.Next())
		{
			const Flux_IKTarget* pxTarget = m_pxIKSolver->GetTarget(xIt.GetKey());
			if (pxTarget && pxTarget->m_bEnabled)
			{
				// Draw target as red sphere
				g_xEngine.Primitives().AddSphere(pxTarget->m_xPosition, 0.05f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
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
		// WU-6.3: the id is NOT in the scene bytes (that layout may not move), so
		// mint one here rather than leaving every restored layer holding the
		// sentinel. The numbers differ from the ones the save was taken with —
		// which is why a game resolves its id ONCE after building or loading a
		// graph, and never persists one.
		pxLayer->SetLayerId(m_uNextLayerId++);
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

	// D42: a whole new graph arrived, declarations and all. Publish NOW rather
	// than waiting for the first Update — a caller that deserializes and then
	// immediately reads GetParameters() should see the declared set, and the
	// binding must exist before anything drives a machine by hand.
	PublishSharedParameters();
}

#ifdef ZENITH_TESTING
#include "Flux/MeshAnimation/Flux_AnimationController.Tests.inl"
#endif
