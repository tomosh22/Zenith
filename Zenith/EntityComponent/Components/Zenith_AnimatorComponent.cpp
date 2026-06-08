#include "Zenith.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Core/Zenith_Engine.h"   // g_xEngine.AnimationControllers() — EC->Core, not a layering edge.
// Flux animation types are resolved HERE (the forwarding-handle .cpp), not in
// the component header. These .cpp -> Flux edges are allow-listed; the header
// itself is now Flux-include-free (Wave-19 decouple).
#include "Flux/MeshAnimation/Flux_AnimationController.h"   // pulls Flux_AnimationStateMachine.h / Flux_AnimatorStateInfo / Flux_AnimationLayer / Flux_AnimationClip transitively
#include "Flux/MeshAnimation/Flux_AnimationControllerStore.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Flux_ModelInstance.h"

//=============================================================================
// Store-backed controller access
//
// Controller() resolves the store-owned Flux_AnimationController for this
// component's entity. Wave-19: the controller lives in
// Flux_AnimationControllerStore (g_xEngine.AnimationControllers()), keyed by
// the stable EntityID slot. The cached m_pxController is preferred on the hot
// path (OnUpdate) to avoid a per-frame store lookup; the cold forwarders use
// Controller() so they resolve correctly even if invoked before OnStart cached
// the pointer (the ctor primes it via GetOrCreate, so this is normally a cheap
// assert-and-return).
//=============================================================================
Flux_AnimationController& Zenith_AnimatorComponent::Controller() const
{
	return g_xEngine.AnimationControllers().Get(m_xParentEntity.GetEntityID());
}

// EC-side mirror -> Flux POD conversion. Defined here where Flux_AnimatorStateInfo
// is complete (the header only forward-declares it). Field-for-field copy.
Zenith_AnimatorStateInfo::operator Flux_AnimatorStateInfo() const
{
	Flux_AnimatorStateInfo xOut;
	xOut.m_strStateName = m_strStateName;
	xOut.m_fNormalizedTime = m_fNormalizedTime;
	xOut.m_fLength = m_fLength;
	xOut.m_fSpeed = m_fSpeed;
	xOut.m_bHasLooped = m_bHasLooped;
	xOut.m_bIsTransitioning = m_bIsTransitioning;
	xOut.m_fTransitionProgress = m_fTransitionProgress;
	return xOut;
}

void Zenith_AnimatorComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// Whole-controller override. Wave-19: the controller is no longer a member
	// field, so the offset-based PROPERTY macro can't be used. PROPERTY_CUSTOM
	// redirects the deserialise into the store-backed controller via Controller().
	// Flux_AnimationController has WriteToDataStream/ReadFromDataStream, so the
	// DataStream `>>` SFINAE dispatch hands the payload to its deserialiser —
	// byte-format-identical to the previous `xValue >> m_xController`. Variant
	// overrides typically use this to swap the entire animation graph on a
	// per-instance basis (e.g. enemy variants that share a base mesh but differ
	// only in their animator).
	ZENITH_REGISTER_COMPONENT_PROPERTY_CUSTOM(Zenith_AnimatorComponent, "Controller", axProperties,
	{
		xValue >> pxComp->Controller();
	});
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

Zenith_AnimatorComponent::Zenith_AnimatorComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
	// Eagerly create the store-owned controller and cache the (heap-stable)
	// pointer. This keeps GetController() / clip-loading valid even when game
	// setup code touches the controller before OnStart fires (e.g. Combat's
	// LoadAnimationClips runs during component wiring).
	m_pxController = &g_xEngine.AnimationControllers().GetOrCreate(m_xParentEntity.GetEntityID());
}

Zenith_AnimatorComponent::~Zenith_AnimatorComponent()
{
	// Idempotent + moved-out guarded: a moved-from component owns nothing (the
	// moved-to instance shares the same EntityID-keyed controller), so it must
	// not Destroy. For a live component this frees the store entry; if OnDestroy
	// already ran Destroy, this call is a no-op (Destroy is idempotent). Net
	// effect: EXACTLY ONE Destroy per entity.
	if (!m_bMovedOut)
	{
		g_xEngine.AnimationControllers().Destroy(m_xParentEntity.GetEntityID());
	}
}

//=============================================================================
// Move Semantics
//
// Copy the POD identity (entity handle, cached model component, retry count,
// the HEAP-STABLE controller pointer) and mark the source moved-out. The
// controller itself stays in the store keyed by the stable EntityID slot
// (unchanged across the pool relocation), so the moved-to component resolves
// the SAME controller. The source is neutralised so its dtor/OnDestroy won't
// Destroy the shared entry.
//=============================================================================

Zenith_AnimatorComponent::Zenith_AnimatorComponent(Zenith_AnimatorComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxController(xOther.m_pxController)
	, m_pxCachedModelComponent(xOther.m_pxCachedModelComponent)
	, m_uDiscoveryRetryCount(xOther.m_uDiscoveryRetryCount)
{
	xOther.m_pxCachedModelComponent = nullptr;
	xOther.m_pxController = nullptr;
	xOther.m_bMovedOut = true;
}

Zenith_AnimatorComponent& Zenith_AnimatorComponent::operator=(Zenith_AnimatorComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// If this destination already owns a (distinct) controller, release it
		// first so the assignment doesn't leak the store entry. Guard the
		// self-keyed / moved-out cases. Destroy is idempotent.
		if (!m_bMovedOut && m_xParentEntity.GetEntityID() != xOther.m_xParentEntity.GetEntityID())
		{
			g_xEngine.AnimationControllers().Destroy(m_xParentEntity.GetEntityID());
		}

		m_xParentEntity = xOther.m_xParentEntity;
		m_pxController = xOther.m_pxController;
		m_pxCachedModelComponent = xOther.m_pxCachedModelComponent;
		m_uDiscoveryRetryCount = xOther.m_uDiscoveryRetryCount;
		m_bMovedOut = false;

		xOther.m_pxCachedModelComponent = nullptr;
		xOther.m_pxController = nullptr;
		xOther.m_bMovedOut = true;
	}
	return *this;
}

//=============================================================================
// ECS Lifecycle
//=============================================================================

void Zenith_AnimatorComponent::OnStart()
{
	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] OnStart fired for entity %u",
		m_xParentEntity.GetEntityID().m_uIndex);

	// Cache the (heap-stable) store-owned controller pointer for the hot path.
	// GetOrCreate is idempotent — the ctor already created it; this re-resolves
	// the pointer (it never changes for a given entity, but OnStart is the
	// canonical Wave-19 cache point).
	m_pxController = &g_xEngine.AnimationControllers().GetOrCreate(m_xParentEntity.GetEntityID());

	// Clear any editor animation preview state so the state machine drives animation
	m_pxController->Stop();

	// Reset state machine to default state in case editor preview advanced it
	if (m_pxController->HasStateMachine())
	{
		Flux_AnimationStateMachine& xSM = m_pxController->GetStateMachine();
		if (!xSM.GetDefaultStateName().empty())
		{
			xSM.SetState(xSM.GetDefaultStateName());
		}
	}

	TryDiscoverSkeleton();
}

void Zenith_AnimatorComponent::OnUpdate(float fDt)
{
	// Hot path: dereference the CACHED controller pointer directly (O(1), no
	// store lookup, no hash). Primed by the ctor and re-primed by OnStart.
	Flux_AnimationController& xController = *m_pxController;

	// Lazy skeleton discovery: retry each frame until found
	// Handles cases where ModelComponent loads its model after OnStart has already fired
	if (!xController.IsInitialized())
	{
		TryDiscoverSkeleton();
		if (!xController.IsInitialized())
		{
			// Log once every ~60 frames to avoid spamming
			m_uDiscoveryRetryCount++;
			if (m_uDiscoveryRetryCount == 1 || (m_uDiscoveryRetryCount % 60) == 0)
			{
				Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] Still no skeleton on entity %u after %u retries",
					m_xParentEntity.GetEntityID().m_uIndex, m_uDiscoveryRetryCount);
			}
			return;
		}
	}

	// Update world matrix from TransformComponent
	UpdateWorldMatrix();

	// Evaluate animation (state machine, IK, GPU upload)
	xController.Update(fDt);

	// Also update model instance animation if present
	SyncModelInstanceAnimation();
}

void Zenith_AnimatorComponent::OnDestroy()
{
	// Destroy the store-owned controller for this entity. Idempotent + moved-out
	// guarded (see dtor). Doing it here AND in the dtor is intentional and safe:
	// whichever fires first does the work, the second is a no-op.
	if (!m_bMovedOut)
	{
		g_xEngine.AnimationControllers().Destroy(m_xParentEntity.GetEntityID());
	}
}

//=============================================================================
// State Machine Parameter Shortcuts
//=============================================================================

void Zenith_AnimatorComponent::SetFloat(const std::string& strName, float fValue)
{
	Controller().SetFloat(strName, fValue);
}

void Zenith_AnimatorComponent::SetInt(const std::string& strName, int32_t iValue)
{
	Controller().SetInt(strName, iValue);
}

void Zenith_AnimatorComponent::SetBool(const std::string& strName, bool bValue)
{
	Controller().SetBool(strName, bValue);
}

void Zenith_AnimatorComponent::SetTrigger(const std::string& strName)
{
	Controller().SetTrigger(strName);
}

float Zenith_AnimatorComponent::GetFloat(const std::string& strName) const
{
	return Controller().GetFloat(strName);
}

int32_t Zenith_AnimatorComponent::GetInt(const std::string& strName) const
{
	return Controller().GetInt(strName);
}

bool Zenith_AnimatorComponent::GetBool(const std::string& strName) const
{
	return Controller().GetBool(strName);
}

//=============================================================================
// Controller Access
//=============================================================================

Flux_AnimationController& Zenith_AnimatorComponent::GetController()
{
	return Controller();
}

const Flux_AnimationController& Zenith_AnimatorComponent::GetController() const
{
	return Controller();
}

//=============================================================================
// Convenience
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_AnimatorComponent::PlayAnimation(const std::string& strClipName, float fBlendTime)
{
	Controller().PlayClip(strClipName, fBlendTime);
}
#endif

void Zenith_AnimatorComponent::CrossFade(const std::string& strStateName, float fDuration)
{
	Controller().CrossFade(strStateName, fDuration);
}

void Zenith_AnimatorComponent::Stop()
{
	Controller().Stop();
}

void Zenith_AnimatorComponent::SetPaused(bool bPaused)
{
	Controller().SetPaused(bPaused);
}

bool Zenith_AnimatorComponent::IsPaused() const
{
	return Controller().IsPaused();
}

void Zenith_AnimatorComponent::SetPlaybackSpeed(float fSpeed)
{
	Controller().SetPlaybackSpeed(fSpeed);
}

float Zenith_AnimatorComponent::GetPlaybackSpeed() const
{
	return Controller().GetPlaybackSpeed();
}

//=============================================================================
// Clip Management
//=============================================================================

Flux_AnimationClip* Zenith_AnimatorComponent::AddClipFromFile(const std::string& strPath)
{
	return Controller().AddClipFromFile(strPath);
}

Flux_AnimationClip* Zenith_AnimatorComponent::GetClip(const std::string& strName)
{
	return Controller().GetClip(strName);
}

//=============================================================================
// State Machine
//=============================================================================

Flux_AnimationStateMachine& Zenith_AnimatorComponent::GetStateMachine()
{
	return Controller().GetStateMachine();
}

Flux_AnimationStateMachine* Zenith_AnimatorComponent::CreateStateMachine(const std::string& strName)
{
	return Controller().CreateStateMachine(strName);
}

bool Zenith_AnimatorComponent::HasStateMachine() const
{
	return Controller().HasStateMachine();
}

//=============================================================================
// State Info Query
//=============================================================================

Zenith_AnimatorStateInfo Zenith_AnimatorComponent::GetCurrentAnimatorStateInfo() const
{
	// Fill the EC-side mirror from the Flux result. Byte-identical field copy.
	Flux_AnimatorStateInfo xFlux = Controller().GetCurrentAnimatorStateInfo();
	Zenith_AnimatorStateInfo xInfo;
	xInfo.m_strStateName = xFlux.m_strStateName;
	xInfo.m_fNormalizedTime = xFlux.m_fNormalizedTime;
	xInfo.m_fLength = xFlux.m_fLength;
	xInfo.m_fSpeed = xFlux.m_fSpeed;
	xInfo.m_bHasLooped = xFlux.m_bHasLooped;
	xInfo.m_bIsTransitioning = xFlux.m_bIsTransitioning;
	xInfo.m_fTransitionProgress = xFlux.m_fTransitionProgress;
	return xInfo;
}

//=============================================================================
// IK
//=============================================================================

void Zenith_AnimatorComponent::SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPos, float fWeight)
{
	Controller().SetIKTarget(strChainName, xPos, fWeight);
}

void Zenith_AnimatorComponent::SetIKTargetModelSpace(const std::string& strChainName, const Zenith_Maths::Vector3& xModelSpacePos, float fWeight)
{
	Controller().SetIKTargetModelSpace(strChainName, xModelSpacePos, fWeight);
}

void Zenith_AnimatorComponent::ClearIKTarget(const std::string& strChainName)
{
	Controller().ClearIKTarget(strChainName);
}

//=============================================================================
// Initialization State
//=============================================================================

void Zenith_AnimatorComponent::SetUpdateMode(Flux_AnimationUpdateMode eMode)
{
	Controller().SetUpdateMode(eMode);
}

Flux_AnimationUpdateMode Zenith_AnimatorComponent::GetUpdateMode() const
{
	return Controller().GetUpdateMode();
}

bool Zenith_AnimatorComponent::IsInitialized() const
{
	return Controller().IsInitialized();
}

//=============================================================================
// Private
//=============================================================================

void Zenith_AnimatorComponent::TryDiscoverSkeleton()
{
	Flux_AnimationController& xController = Controller();
	if (xController.IsInitialized())
		return;

	Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
	if (!pxModel)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: No ModelComponent on entity %u",
			m_xParentEntity.GetEntityID().m_uIndex);
		return;
	}

	if (!pxModel->HasModel())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: ModelComponent has no model instance (entity %u, meshes=%u)",
			m_xParentEntity.GetEntityID().m_uIndex, pxModel->GetNumMeshes());
		return;
	}

	if (!pxModel->HasSkeleton())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: ModelComponent reports no skeleton (entity %u, hasModel=%s)",
			m_xParentEntity.GetEntityID().m_uIndex, pxModel->HasModel() ? "yes" : "no");
		return;
	}

	Flux_SkeletonInstance* pxSkeleton = pxModel->GetSkeletonInstance();
	if (!pxSkeleton)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: GetSkeletonInstance returned null despite HasSkeleton=true (entity %u)",
			m_xParentEntity.GetEntityID().m_uIndex);
		return;
	}

	xController.Initialize(pxSkeleton);
	m_pxCachedModelComponent = pxModel;
	m_uDiscoveryRetryCount = 0;
	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] Auto-discovered skeleton (%u bones) on entity %u",
		pxSkeleton->GetNumBones(), m_xParentEntity.GetEntityID().m_uIndex);
}

void Zenith_AnimatorComponent::UpdateWorldMatrix()
{
	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xWorldMatrix;
	xTransform.BuildModelMatrix(xWorldMatrix);
	Controller().SetWorldMatrix(xWorldMatrix);
}

void Zenith_AnimatorComponent::SyncModelInstanceAnimation()
{
	if (m_pxCachedModelComponent && m_pxCachedModelComponent->HasModel())
	{
		Flux_ModelInstance* pxModelInstance = m_pxCachedModelComponent->GetModelInstance();
		if (pxModelInstance && pxModelInstance->HasSkeleton())
		{
			pxModelInstance->UpdateAnimation();
		}
	}
}

//=============================================================================
// Serialization
//=============================================================================

void Zenith_AnimatorComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Controller().WriteToDataStream(xStream);
}

void Zenith_AnimatorComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Controller().ReadFromDataStream(xStream);
}

//=============================================================================
// Editor UI
//=============================================================================

#ifdef ZENITH_TOOLS

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Core/Zenith_EditorQuery.h"
#include "Core/Zenith_DragDropPayloads.h"
// Flux_AnimationClip is already complete here via Flux_AnimationController.h
// (included above) -> Flux_AnimationClip.h, so no direct Flux clip include is
// needed for the editor clip list below.

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI. Delegates to per-section helpers so
// each section stays focused on its own concern.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	Flux_AnimationController& xController = Controller();

	// Editor fallback: attempt skeleton discovery if OnStart/OnUpdate haven't fired yet
	// (happens when editor is in Stopped mode - SceneManager::Update doesn't run)
	if (!xController.IsInitialized())
	{
		TryDiscoverSkeleton();
	}

	// Tick animation from editor when game logic isn't running (Stopped/Paused mode)
	if (!g_xEditorQuery.m_pfnIsEditorPlaying() && xController.IsInitialized())
	{
		UpdateWorldMatrix();
		xController.Update(g_xEngine.Frame().GetDt());
		SyncModelInstanceAnimation();
	}

	RenderStatusAndStateInfoSection();

	ImGui::Separator();

	RenderAnimationClipsSection();
	RenderPlaybackControlsSection();
	RenderParametersSection();
	RenderStateMachineSection();
	RenderLayersSection();
	RenderUpdateModeSection();
}

//-----------------------------------------------------------------------------
// Status line (initialized / not) and the current-state info shown above the
// first separator. This block runs unconditionally every panel frame.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderStatusAndStateInfoSection()
{
	Flux_AnimationController& xController = Controller();

	// Status
	if (xController.IsInitialized())
	{
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Status: Initialized (%u bones)", xController.GetNumBones());
	}
	else
	{
		ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Status: No skeleton found");
	}

	// Current state info
	if (xController.HasStateMachine())
	{
		Flux_AnimatorStateInfo xInfo = xController.GetCurrentAnimatorStateInfo();
		ImGui::Text("Current State: %s", xInfo.m_strStateName.c_str());
		float fProgress = xInfo.m_fNormalizedTime - static_cast<int>(xInfo.m_fNormalizedTime);
		ImGui::ProgressBar(fProgress, ImVec2(-1, 0), nullptr);
		if (xInfo.m_bIsTransitioning)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Transitioning (%.0f%%)", xInfo.m_fTransitionProgress * 100.0f);
		}
	}
}

//-----------------------------------------------------------------------------
// "Animation Clips" tree node: drag-drop target for .zanim files, manual path
// input + Load button, and a list of every clip already loaded on the
// controller with a per-clip Play button.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderAnimationClipsSection()
{
	if (!ImGui::TreeNode("Animation Clips"))
		return;

	Flux_AnimationController& xController = Controller();

	// Drag-drop target for .zanim files
	ImVec2 xDropSize(ImGui::GetContentRegionAvail().x, 30);
	ImGui::Button("Drop .zanim file here", xDropSize);
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMATION))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Animation dropped: %s", pFilePayload->m_szFilePath);
			xController.AddClipFromFile(pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	// Manual path entry
	static char s_szAnimPath[512] = "";
	ImGui::InputText("Path##AnimPath", s_szAnimPath, sizeof(s_szAnimPath));
	ImGui::SameLine();
	if (ImGui::Button("Load##LoadAnim"))
	{
		if (strlen(s_szAnimPath) > 0)
		{
			xController.AddClipFromFile(s_szAnimPath);
			s_szAnimPath[0] = '\0';
		}
	}

	// Loaded clips list
	const auto& xClips = xController.GetClipCollection().GetClips();
	for (u_int i = 0; i < xClips.GetSize(); ++i)
	{
		Flux_AnimationClip* pxClip = xClips.Get(i);
		if (!pxClip)
			continue;

		ImGui::PushID(static_cast<int>(i));

		float fDuration = pxClip->GetDuration();
		ImGui::BulletText("%s (%.2fs)", pxClip->GetName().c_str(), fDuration);
		ImGui::SameLine();
		if (ImGui::SmallButton("Play"))
		{
			xController.PlayClip(pxClip->GetName(), 0.15f);
		}

		ImGui::PopID();
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// "Playback Controls" tree node: paused checkbox, speed slider, Stop button,
// and a row of CrossFade buttons - one per state in the state machine.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderPlaybackControlsSection()
{
	if (!ImGui::TreeNode("Playback Controls"))
		return;

	Flux_AnimationController& xController = Controller();

	bool bPaused = xController.IsPaused();
	if (ImGui::Checkbox("Paused", &bPaused))
	{
		xController.SetPaused(bPaused);
	}

	float fSpeed = xController.GetPlaybackSpeed();
	if (ImGui::SliderFloat("Speed", &fSpeed, 0.0f, 3.0f))
	{
		xController.SetPlaybackSpeed(fSpeed);
	}

	if (ImGui::Button("Stop"))
	{
		xController.Stop();
	}

	// CrossFade to state
	if (xController.HasStateMachine())
	{
		ImGui::Separator();
		ImGui::Text("CrossFade:");

		const Zenith_HashMap<std::string, Flux_AnimationState*>& xStates = xController.GetStateMachine().GetStates();
		for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(xStates); !xIt.Done(); xIt.Next())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton(xIt.GetKey().c_str()))
			{
				xController.CrossFade(xIt.GetKey(), 0.15f);
			}
		}
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// "Parameters" tree node: one widget per state machine parameter (float / int
// / bool / trigger). No-op when the controller has no state machine.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderParametersSection()
{
	Flux_AnimationController& xController = Controller();

	if (!xController.HasStateMachine())
		return;

	if (!ImGui::TreeNode("Parameters"))
		return;

	Flux_AnimationParameters& xParams = xController.GetStateMachine().GetParameters();
	const Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>& xParamMap = xParams.GetParameters();

	for (Zenith_HashMap<std::string, Flux_AnimationParameters::Parameter>::Iterator xIt(xParamMap); !xIt.Done(); xIt.Next())
	{
		const std::string& strParamName = xIt.GetKey();
		const Flux_AnimationParameters::Parameter& xParam = xIt.GetValue();
		ImGui::PushID(strParamName.c_str());

		switch (xParam.m_eType)
		{
		case Flux_AnimationParameters::ParamType::Float:
		{
			float fVal = xParams.GetFloat(strParamName);
			if (ImGui::DragFloat(strParamName.c_str(), &fVal, 0.01f))
			{
				xParams.SetFloat(strParamName, fVal);
			}
			break;
		}
		case Flux_AnimationParameters::ParamType::Int:
		{
			int32_t iVal = xParams.GetInt(strParamName);
			if (ImGui::DragInt(strParamName.c_str(), &iVal))
			{
				xParams.SetInt(strParamName, iVal);
			}
			break;
		}
		case Flux_AnimationParameters::ParamType::Bool:
		{
			bool bVal = xParams.GetBool(strParamName);
			if (ImGui::Checkbox(strParamName.c_str(), &bVal))
			{
				xParams.SetBool(strParamName, bVal);
			}
			break;
		}
		case Flux_AnimationParameters::ParamType::Trigger:
		{
			ImGui::Text("%s", strParamName.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Fire"))
			{
				xParams.SetTrigger(strParamName);
			}
			break;
		}
		}

		ImGui::PopID();
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// "State Machine" tree node: list of states with transitions + sub-SM markers,
// followed by any-state transitions. No-op when the controller has no SM.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderStateMachineSection()
{
	Flux_AnimationController& xController = Controller();

	if (!xController.HasStateMachine())
		return;

	if (!ImGui::TreeNode("State Machine"))
		return;

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();

	// States list
	ImGui::Text("States:");
	const Zenith_HashMap<std::string, Flux_AnimationState*>& xStates = xSM.GetStates();
	for (Zenith_HashMap<std::string, Flux_AnimationState*>::Iterator xIt(xStates); !xIt.Done(); xIt.Next())
	{
		const std::string& strStateName = xIt.GetKey();
		const Flux_AnimationState* pxState = xIt.GetValue();
		bool bIsCurrent = (xSM.GetCurrentStateName() == strStateName);
		bool bIsDefault = (xSM.GetDefaultStateName() == strStateName);

		if (bIsCurrent)
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  > %s%s",
				strStateName.c_str(), bIsDefault ? " [Default]" : "");
		else
			ImGui::Text("    %s%s",
				strStateName.c_str(), bIsDefault ? " [Default]" : "");

		// Show transitions for this state
		const Zenith_Vector<Flux_StateTransition>& xTransitions = pxState->GetTransitions();
		for (uint32_t t = 0; t < xTransitions.GetSize(); ++t)
		{
			const Flux_StateTransition& xTrans = xTransitions.Get(t);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "      -> %s (dur: %.2fs, pri: %d)",
				xTrans.m_strTargetStateName.c_str(), xTrans.m_fTransitionDuration, xTrans.m_iPriority);
		}

		// Show sub-state machine
		if (pxState->IsSubStateMachine())
		{
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "      [Sub-State Machine]");
		}
	}

	// Any-state transitions
	const Zenith_Vector<Flux_StateTransition>& xAnyState = xSM.GetAnyStateTransitions();
	if (xAnyState.GetSize() > 0)
	{
		ImGui::Separator();
		ImGui::Text("Any-State Transitions:");
		for (uint32_t i = 0; i < xAnyState.GetSize(); ++i)
		{
			const Flux_StateTransition& xTrans = xAnyState.Get(i);
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "  * -> %s (dur: %.2fs, pri: %d)",
				xTrans.m_strTargetStateName.c_str(), xTrans.m_fTransitionDuration, xTrans.m_iPriority);
		}
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// "Layers" tree node: per-layer weight slider, blend mode combo, avatar mask
// status. No-op when the controller has no layers.
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderLayersSection()
{
	Flux_AnimationController& xController = Controller();

	if (!xController.HasLayers())
		return;

	if (!ImGui::TreeNode("Layers"))
		return;

	for (uint32_t i = 0; i < xController.GetLayerCount(); ++i)
	{
		Flux_AnimationLayer* pxLayer = xController.GetLayer(i);
		ImGui::PushID(i);

		if (ImGui::TreeNode("##Layer", "%s (%.0f%%)", pxLayer->GetName().c_str(), pxLayer->GetWeight() * 100.0f))
		{
			float fWeight = pxLayer->GetWeight();
			if (ImGui::SliderFloat("Weight", &fWeight, 0.0f, 1.0f))
			{
				pxLayer->SetWeight(fWeight);
			}

			const char* aszBlendModes[] = { "Override", "Additive" };
			int iBlendMode = static_cast<int>(pxLayer->GetBlendMode());
			if (ImGui::Combo("Blend Mode", &iBlendMode, aszBlendModes, 2))
			{
				pxLayer->SetBlendMode(static_cast<Flux_LayerBlendMode>(iBlendMode));
			}

			ImGui::Text("Mask: %s", pxLayer->HasAvatarMask() ? "Active" : "None");

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// Bottom-of-panel Update Mode combo (Normal / Fixed / Unscaled).
//-----------------------------------------------------------------------------
void Zenith_AnimatorComponent::RenderUpdateModeSection()
{
	Flux_AnimationController& xController = Controller();

	const char* aszUpdateModes[] = { "Normal", "Fixed", "Unscaled" };
	int iMode = static_cast<int>(xController.GetUpdateMode());
	if (ImGui::Combo("Update Mode", &iMode, aszUpdateModes, 3))
	{
		xController.SetUpdateMode(static_cast<Flux_AnimationUpdateMode>(iMode));
	}
}

#endif // ZENITH_TOOLS
