#include "Zenith.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Flux_ModelInstance.h"

ZENITH_REGISTER_COMPONENT(Zenith_AnimatorComponent, "Animator")

//=============================================================================
// Constructor / Destructor
//=============================================================================

Zenith_AnimatorComponent::Zenith_AnimatorComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_AnimatorComponent::~Zenith_AnimatorComponent()
{
}

//=============================================================================
// Move Semantics
//=============================================================================

Zenith_AnimatorComponent::Zenith_AnimatorComponent(Zenith_AnimatorComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_xController(std::move(xOther.m_xController))
	, m_pxCachedModelComponent(xOther.m_pxCachedModelComponent)
	, m_uDiscoveryRetryCount(xOther.m_uDiscoveryRetryCount)
{
	xOther.m_pxCachedModelComponent = nullptr;
}

Zenith_AnimatorComponent& Zenith_AnimatorComponent::operator=(Zenith_AnimatorComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		m_xParentEntity = xOther.m_xParentEntity;
		m_xController = std::move(xOther.m_xController);
		m_pxCachedModelComponent = xOther.m_pxCachedModelComponent;
		m_uDiscoveryRetryCount = xOther.m_uDiscoveryRetryCount;
		xOther.m_pxCachedModelComponent = nullptr;
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

	// Clear any editor animation preview state so the state machine drives animation
	m_xController.Stop();

	// Reset state machine to default state in case editor preview advanced it
	if (m_xController.HasStateMachine())
	{
		Flux_AnimationStateMachine& xSM = m_xController.GetStateMachine();
		if (!xSM.GetDefaultStateName().empty())
		{
			xSM.SetState(xSM.GetDefaultStateName());
		}
	}

	TryDiscoverSkeleton();
}

void Zenith_AnimatorComponent::OnUpdate(float fDt)
{
	// Lazy skeleton discovery: retry each frame until found
	// Handles cases where ModelComponent loads its model after OnStart has already fired
	if (!m_xController.IsInitialized())
	{
		TryDiscoverSkeleton();
		if (!m_xController.IsInitialized())
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
	m_xController.Update(fDt);

	// Also update model instance animation if present
	SyncModelInstanceAnimation();
}

void Zenith_AnimatorComponent::OnDestroy()
{
	// Flux_AnimationController destructor handles cleanup
}

//=============================================================================
// State Machine Parameter Shortcuts
//=============================================================================

void Zenith_AnimatorComponent::SetFloat(const std::string& strName, float fValue)
{
	m_xController.SetFloat(strName, fValue);
}

void Zenith_AnimatorComponent::SetInt(const std::string& strName, int32_t iValue)
{
	m_xController.SetInt(strName, iValue);
}

void Zenith_AnimatorComponent::SetBool(const std::string& strName, bool bValue)
{
	m_xController.SetBool(strName, bValue);
}

void Zenith_AnimatorComponent::SetTrigger(const std::string& strName)
{
	m_xController.SetTrigger(strName);
}

float Zenith_AnimatorComponent::GetFloat(const std::string& strName) const
{
	return m_xController.GetFloat(strName);
}

int32_t Zenith_AnimatorComponent::GetInt(const std::string& strName) const
{
	return m_xController.GetInt(strName);
}

bool Zenith_AnimatorComponent::GetBool(const std::string& strName) const
{
	return m_xController.GetBool(strName);
}

//=============================================================================
// Convenience
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_AnimatorComponent::PlayAnimation(const std::string& strClipName, float fBlendTime)
{
	m_xController.PlayClip(strClipName, fBlendTime);
}
#endif

void Zenith_AnimatorComponent::CrossFade(const std::string& strStateName, float fDuration)
{
	m_xController.CrossFade(strStateName, fDuration);
}

void Zenith_AnimatorComponent::Stop()
{
	m_xController.Stop();
}

void Zenith_AnimatorComponent::SetPaused(bool bPaused)
{
	m_xController.SetPaused(bPaused);
}

bool Zenith_AnimatorComponent::IsPaused() const
{
	return m_xController.IsPaused();
}

void Zenith_AnimatorComponent::SetPlaybackSpeed(float fSpeed)
{
	m_xController.SetPlaybackSpeed(fSpeed);
}

float Zenith_AnimatorComponent::GetPlaybackSpeed() const
{
	return m_xController.GetPlaybackSpeed();
}

//=============================================================================
// Clip Management
//=============================================================================

Flux_AnimationClip* Zenith_AnimatorComponent::AddClipFromFile(const std::string& strPath)
{
	return m_xController.AddClipFromFile(strPath);
}

Flux_AnimationClip* Zenith_AnimatorComponent::GetClip(const std::string& strName)
{
	return m_xController.GetClip(strName);
}

//=============================================================================
// State Machine
//=============================================================================

Flux_AnimationStateMachine& Zenith_AnimatorComponent::GetStateMachine()
{
	return m_xController.GetStateMachine();
}

Flux_AnimationStateMachine* Zenith_AnimatorComponent::CreateStateMachine(const std::string& strName)
{
	return m_xController.CreateStateMachine(strName);
}

bool Zenith_AnimatorComponent::HasStateMachine() const
{
	return m_xController.HasStateMachine();
}

//=============================================================================
// State Info Query
//=============================================================================

Flux_AnimatorStateInfo Zenith_AnimatorComponent::GetCurrentAnimatorStateInfo() const
{
	return m_xController.GetCurrentAnimatorStateInfo();
}

//=============================================================================
// IK
//=============================================================================

void Zenith_AnimatorComponent::SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPos, float fWeight)
{
	m_xController.SetIKTarget(strChainName, xPos, fWeight);
}

void Zenith_AnimatorComponent::ClearIKTarget(const std::string& strChainName)
{
	m_xController.ClearIKTarget(strChainName);
}

//=============================================================================
// Initialization State
//=============================================================================

void Zenith_AnimatorComponent::SetUpdateMode(Flux_AnimationUpdateMode eMode)
{
	m_xController.SetUpdateMode(eMode);
}

Flux_AnimationUpdateMode Zenith_AnimatorComponent::GetUpdateMode() const
{
	return m_xController.GetUpdateMode();
}

bool Zenith_AnimatorComponent::IsInitialized() const
{
	return m_xController.IsInitialized();
}

//=============================================================================
// Private
//=============================================================================

void Zenith_AnimatorComponent::TryDiscoverSkeleton()
{
	if (m_xController.IsInitialized())
		return;

	Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
	if (!pxModel)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: No ModelComponent on entity %u",
			m_xParentEntity.GetEntityID().m_uIndex);
		return;
	}

	if (!pxModel->IsUsingModelInstance())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: ModelComponent has no model instance (entity %u, meshEntries=%u)",
			m_xParentEntity.GetEntityID().m_uIndex, pxModel->GetNumMeshes());
		return;
	}

	if (!pxModel->HasSkeleton())
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: ModelComponent reports no skeleton (entity %u, hasModelInstance=%s)",
			m_xParentEntity.GetEntityID().m_uIndex, pxModel->IsUsingModelInstance() ? "yes" : "no");
		return;
	}

	Flux_SkeletonInstance* pxSkeleton = pxModel->GetSkeletonInstance();
	if (!pxSkeleton)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorComponent] TryDiscoverSkeleton: GetSkeletonInstance returned null despite HasSkeleton=true (entity %u)",
			m_xParentEntity.GetEntityID().m_uIndex);
		return;
	}

	m_xController.Initialize(pxSkeleton);
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
	m_xController.SetWorldMatrix(xWorldMatrix);
}

void Zenith_AnimatorComponent::SyncModelInstanceAnimation()
{
	if (m_pxCachedModelComponent && m_pxCachedModelComponent->IsUsingModelInstance())
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
	m_xController.WriteToDataStream(xStream);
}

void Zenith_AnimatorComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xController.ReadFromDataStream(xStream);
}

//=============================================================================
// Editor UI
//=============================================================================

#ifdef ZENITH_TOOLS

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Editor/Zenith_Editor.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

void Zenith_AnimatorComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// Editor fallback: attempt skeleton discovery if OnStart/OnUpdate haven't fired yet
	// (happens when editor is in Stopped mode - SceneManager::Update doesn't run)
	if (!m_xController.IsInitialized())
	{
		TryDiscoverSkeleton();
	}

	// Tick animation from editor when game logic isn't running (Stopped/Paused mode)
	if (Zenith_Editor::GetEditorMode() != EditorMode::Playing && m_xController.IsInitialized())
	{
		UpdateWorldMatrix();
		m_xController.Update(Zenith_Core::GetDt());
		SyncModelInstanceAnimation();
	}

	// Status
	if (m_xController.IsInitialized())
	{
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Status: Initialized (%u bones)", m_xController.GetNumBones());
	}
	else
	{
		ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Status: No skeleton found");
	}

	// Current state info
	if (m_xController.HasStateMachine())
	{
		Flux_AnimatorStateInfo xInfo = m_xController.GetCurrentAnimatorStateInfo();
		ImGui::Text("Current State: %s", xInfo.m_strStateName.c_str());
		float fProgress = xInfo.m_fNormalizedTime - static_cast<int>(xInfo.m_fNormalizedTime);
		ImGui::ProgressBar(fProgress, ImVec2(-1, 0), nullptr);
		if (xInfo.m_bIsTransitioning)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Transitioning (%.0f%%)", xInfo.m_fTransitionProgress * 100.0f);
		}
	}

	ImGui::Separator();

	// Animation Clips section
	if (ImGui::TreeNode("Animation Clips"))
	{
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
				m_xController.AddClipFromFile(pFilePayload->m_szFilePath);
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
				m_xController.AddClipFromFile(s_szAnimPath);
				s_szAnimPath[0] = '\0';
			}
		}

		// Loaded clips list
		const auto& xClips = m_xController.GetClipCollection().GetClips();
		for (size_t i = 0; i < xClips.size(); ++i)
		{
			Flux_AnimationClip* pxClip = xClips[i];
			if (!pxClip)
				continue;

			ImGui::PushID(static_cast<int>(i));

			float fDuration = pxClip->GetDuration();
			ImGui::BulletText("%s (%.2fs)", pxClip->GetName().c_str(), fDuration);
			ImGui::SameLine();
			if (ImGui::SmallButton("Play"))
			{
				m_xController.PlayClip(pxClip->GetName(), 0.15f);
			}

			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	// Playback Controls section
	if (ImGui::TreeNode("Playback Controls"))
	{
		bool bPaused = m_xController.IsPaused();
		if (ImGui::Checkbox("Paused", &bPaused))
		{
			m_xController.SetPaused(bPaused);
		}

		float fSpeed = m_xController.GetPlaybackSpeed();
		if (ImGui::SliderFloat("Speed", &fSpeed, 0.0f, 3.0f))
		{
			m_xController.SetPlaybackSpeed(fSpeed);
		}

		if (ImGui::Button("Stop"))
		{
			m_xController.Stop();
		}

		// CrossFade to state
		if (m_xController.HasStateMachine())
		{
			ImGui::Separator();
			ImGui::Text("CrossFade:");

			const auto& xStates = m_xController.GetStateMachine().GetStates();
			for (auto it = xStates.begin(); it != xStates.end(); ++it)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton(it->first.c_str()))
				{
					m_xController.CrossFade(it->first, 0.15f);
				}
			}
		}

		ImGui::TreePop();
	}

	// Parameters section
	if (m_xController.HasStateMachine())
	{
		if (ImGui::TreeNode("Parameters"))
		{
			Flux_AnimationParameters& xParams = m_xController.GetStateMachine().GetParameters();
			const auto& xParamMap = xParams.GetParameters();

			for (auto it = xParamMap.begin(); it != xParamMap.end(); ++it)
			{
				const Flux_AnimationParameters::Parameter& xParam = it->second;
				ImGui::PushID(it->first.c_str());

				switch (xParam.m_eType)
				{
				case Flux_AnimationParameters::ParamType::Float:
				{
					float fVal = xParams.GetFloat(it->first);
					if (ImGui::DragFloat(it->first.c_str(), &fVal, 0.01f))
					{
						xParams.SetFloat(it->first, fVal);
					}
					break;
				}
				case Flux_AnimationParameters::ParamType::Int:
				{
					int32_t iVal = xParams.GetInt(it->first);
					if (ImGui::DragInt(it->first.c_str(), &iVal))
					{
						xParams.SetInt(it->first, iVal);
					}
					break;
				}
				case Flux_AnimationParameters::ParamType::Bool:
				{
					bool bVal = xParams.GetBool(it->first);
					if (ImGui::Checkbox(it->first.c_str(), &bVal))
					{
						xParams.SetBool(it->first, bVal);
					}
					break;
				}
				case Flux_AnimationParameters::ParamType::Trigger:
				{
					ImGui::Text("%s", it->first.c_str());
					ImGui::SameLine();
					if (ImGui::SmallButton("Fire"))
					{
						xParams.SetTrigger(it->first);
					}
					break;
				}
				}

				ImGui::PopID();
			}

			ImGui::TreePop();
		}
	}

	// State Machine section
	if (m_xController.HasStateMachine())
	{
		if (ImGui::TreeNode("State Machine"))
		{
			Flux_AnimationStateMachine& xSM = m_xController.GetStateMachine();

			// States list
			ImGui::Text("States:");
			const auto& xStates = xSM.GetStates();
			for (auto it = xStates.begin(); it != xStates.end(); ++it)
			{
				bool bIsCurrent = (xSM.GetCurrentStateName() == it->first);
				bool bIsDefault = (xSM.GetDefaultStateName() == it->first);

				if (bIsCurrent)
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "  > %s%s",
						it->first.c_str(), bIsDefault ? " [Default]" : "");
				else
					ImGui::Text("    %s%s",
						it->first.c_str(), bIsDefault ? " [Default]" : "");

				// Show transitions for this state
				const Zenith_Vector<Flux_StateTransition>& xTransitions = it->second->GetTransitions();
				for (uint32_t t = 0; t < xTransitions.GetSize(); ++t)
				{
					const Flux_StateTransition& xTrans = xTransitions.Get(t);
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "      -> %s (dur: %.2fs, pri: %d)",
						xTrans.m_strTargetStateName.c_str(), xTrans.m_fTransitionDuration, xTrans.m_iPriority);
				}

				// Show sub-state machine
				if (it->second->IsSubStateMachine())
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
	}

	// Layers section
	if (m_xController.HasLayers())
	{
		if (ImGui::TreeNode("Layers"))
		{
			for (uint32_t i = 0; i < m_xController.GetLayerCount(); ++i)
			{
				Flux_AnimationLayer* pxLayer = m_xController.GetLayer(i);
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
	}

	// Update Mode
	{
		const char* aszUpdateModes[] = { "Normal", "Fixed", "Unscaled" };
		int iMode = static_cast<int>(m_xController.GetUpdateMode());
		if (ImGui::Combo("Update Mode", &iMode, aszUpdateModes, 3))
		{
			m_xController.SetUpdateMode(static_cast<Flux_AnimationUpdateMode>(iMode));
		}
	}
}

#endif // ZENITH_TOOLS
