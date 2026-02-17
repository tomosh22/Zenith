#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"

ZENITH_REGISTER_COMPONENT(Zenith_TweenComponent, "Tween")

//=============================================================================
// Construction / Move
//=============================================================================

Zenith_TweenComponent::Zenith_TweenComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_TweenComponent::Zenith_TweenComponent(Zenith_TweenComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_xActiveTweens(std::move(xOther.m_xActiveTweens))
{
}

Zenith_TweenComponent& Zenith_TweenComponent::operator=(Zenith_TweenComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		m_xParentEntity = xOther.m_xParentEntity;
		m_xActiveTweens = std::move(xOther.m_xActiveTweens);
	}
	return *this;
}

//=============================================================================
// OnUpdate - Advance and apply all active tweens
//=============================================================================

void Zenith_TweenComponent::OnUpdate(float fDt)
{
	if (m_xActiveTweens.GetSize() == 0)
		return;

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	for (uint32_t i = 0; i < m_xActiveTweens.GetSize(); )
	{
		Zenith_TweenInstance& xTween = m_xActiveTweens.Get(i);

		xTween.m_fElapsed += fDt;

		// Still in delay period
		if (xTween.m_fElapsed < xTween.m_fDelay)
		{
			++i;
			continue;
		}

		float fActiveTime = xTween.m_fElapsed - xTween.m_fDelay;
		float fRawT = (xTween.m_fDuration > 0.0f)
			? glm::clamp(fActiveTime / xTween.m_fDuration, 0.0f, 1.0f)
			: 1.0f;

		// Apply easing first, then reverse for ping-pong
		// (reversing before easing produces wrong curves for asymmetric easing functions)
		float fEasedT = Zenith_ApplyEasing(xTween.m_eEasing, fRawT);
		float fDirectionalT = xTween.m_bReversing ? (1.0f - fEasedT) : fEasedT;

		// Interpolate and apply to transform
		switch (xTween.m_eProperty)
		{
		case TWEEN_PROPERTY_POSITION:
			xTransform.SetPosition(glm::mix(xTween.m_xFrom, xTween.m_xTo, fDirectionalT));
			break;
		case TWEEN_PROPERTY_SCALE:
			xTransform.SetScale(glm::mix(xTween.m_xFrom, xTween.m_xTo, fDirectionalT));
			break;
		case TWEEN_PROPERTY_ROTATION:
		{
			// Use slerp to avoid gimbal lock and ensure shortest-path rotation
			Zenith_Maths::Quat xRot = glm::slerp(xTween.m_xFromQuat, xTween.m_xToQuat, fDirectionalT);
			xTransform.SetRotation(xRot);
			break;
		}
		}

		// Check completion
		if (fRawT >= 1.0f)
		{
			if (xTween.m_bLoop)
			{
				if (xTween.m_bPingPong)
				{
					xTween.m_bReversing = !xTween.m_bReversing;
				}
				// Reset active time only (preserve delay so it's not re-applied on loop)
				xTween.m_fElapsed = xTween.m_fDelay;
				++i;
				continue;
			}

			// Copy callback info before removal in case callback modifies the tween list
			Zenith_TweenCallback pfnCallback = xTween.m_pfnOnComplete;
			void* pCallbackData = xTween.m_pCallbackUserData;

			m_xActiveTweens.RemoveSwap(i);

			if (pfnCallback)
			{
				pfnCallback(pCallbackData);
			}
			continue;
		}

		++i;
	}
}

//=============================================================================
// Tween Creation (from current transform value)
//=============================================================================

void Zenith_TweenComponent::CancelByProperty(Zenith_TweenProperty eProperty)
{
	for (uint32_t i = 0; i < m_xActiveTweens.GetSize(); )
	{
		if (m_xActiveTweens.Get(i).m_eProperty == eProperty)
			m_xActiveTweens.RemoveSwap(i);
		else
			++i;
	}
}

void Zenith_TweenComponent::TweenPosition(const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing)
{
	CancelByProperty(TWEEN_PROPERTY_POSITION);

	Zenith_Maths::Vector3 xFrom;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xFrom);

	Zenith_TweenInstance xTween;
	xTween.m_eProperty = TWEEN_PROPERTY_POSITION;
	xTween.m_eEasing = eEasing;
	xTween.m_xFrom = xFrom;
	xTween.m_xTo = xTo;
	xTween.m_fDuration = fDuration;
	m_xActiveTweens.PushBack(xTween);
}

void Zenith_TweenComponent::TweenScale(const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing)
{
	CancelByProperty(TWEEN_PROPERTY_SCALE);

	Zenith_Maths::Vector3 xFrom;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetScale(xFrom);

	Zenith_TweenInstance xTween;
	xTween.m_eProperty = TWEEN_PROPERTY_SCALE;
	xTween.m_eEasing = eEasing;
	xTween.m_xFrom = xFrom;
	xTween.m_xTo = xTo;
	xTween.m_fDuration = fDuration;
	m_xActiveTweens.PushBack(xTween);
}

void Zenith_TweenComponent::TweenRotation(const Zenith_Maths::Vector3& xToEulerDegrees, float fDuration, Zenith_EasingType eEasing)
{
	CancelByProperty(TWEEN_PROPERTY_ROTATION);

	Zenith_Maths::Quat xCurrentRot;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xCurrentRot);

	Zenith_TweenInstance xTween;
	xTween.m_eProperty = TWEEN_PROPERTY_ROTATION;
	xTween.m_eEasing = eEasing;
	xTween.m_xFromQuat = xCurrentRot;
	xTween.m_xToQuat = Zenith_Maths::Quat(glm::radians(xToEulerDegrees));
	xTween.m_fDuration = fDuration;
	m_xActiveTweens.PushBack(xTween);
}

//=============================================================================
// Tween Creation (explicit from/to)
//=============================================================================

void Zenith_TweenComponent::TweenPositionFromTo(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing)
{
	CancelByProperty(TWEEN_PROPERTY_POSITION);

	Zenith_TweenInstance xTween;
	xTween.m_eProperty = TWEEN_PROPERTY_POSITION;
	xTween.m_eEasing = eEasing;
	xTween.m_xFrom = xFrom;
	xTween.m_xTo = xTo;
	xTween.m_fDuration = fDuration;
	m_xActiveTweens.PushBack(xTween);
}

void Zenith_TweenComponent::TweenScaleFromTo(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing)
{
	CancelByProperty(TWEEN_PROPERTY_SCALE);

	Zenith_TweenInstance xTween;
	xTween.m_eProperty = TWEEN_PROPERTY_SCALE;
	xTween.m_eEasing = eEasing;
	xTween.m_xFrom = xFrom;
	xTween.m_xTo = xTo;
	xTween.m_fDuration = fDuration;
	m_xActiveTweens.PushBack(xTween);
}

//=============================================================================
// Configure most recently added tween
//=============================================================================

void Zenith_TweenComponent::SetOnComplete(Zenith_TweenCallback pfnCallback, void* pUserData)
{
	Zenith_Assert(m_xActiveTweens.GetSize() > 0, "SetOnComplete called with no active tweens - call TweenPosition/Scale/Rotation first");
	Zenith_TweenInstance& xLast = m_xActiveTweens.Get(m_xActiveTweens.GetSize() - 1);
	xLast.m_pfnOnComplete = pfnCallback;
	xLast.m_pCallbackUserData = pUserData;
}

void Zenith_TweenComponent::SetDelay(float fDelay)
{
	Zenith_Assert(m_xActiveTweens.GetSize() > 0, "SetDelay called with no active tweens - call TweenPosition/Scale/Rotation first");
	Zenith_TweenInstance& xLast = m_xActiveTweens.Get(m_xActiveTweens.GetSize() - 1);
	xLast.m_fDelay = fDelay;
}

void Zenith_TweenComponent::SetLoop(bool bLoop, bool bPingPong)
{
	Zenith_Assert(m_xActiveTweens.GetSize() > 0, "SetLoop called with no active tweens - call TweenPosition/Scale/Rotation first");
	Zenith_TweenInstance& xLast = m_xActiveTweens.Get(m_xActiveTweens.GetSize() - 1);
	xLast.m_bLoop = bLoop;
	xLast.m_bPingPong = bPingPong;
}

//=============================================================================
// Query / Cancel
//=============================================================================

void Zenith_TweenComponent::CancelAll()
{
	m_xActiveTweens.Clear();
}

bool Zenith_TweenComponent::HasActiveTweens() const
{
	return m_xActiveTweens.GetSize() > 0;
}

uint32_t Zenith_TweenComponent::GetActiveTweenCount() const
{
	return m_xActiveTweens.GetSize();
}

//=============================================================================
// Serialization (tweens are runtime-only, just write a version marker)
//=============================================================================

void Zenith_TweenComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const uint8_t uVersion = 1;
	xStream.Write(&uVersion, sizeof(uVersion));
}

void Zenith_TweenComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint8_t uVersion = 0;
	xStream.Read(&uVersion, sizeof(uVersion));
}

//=============================================================================
// Static Helper
//=============================================================================

void Zenith_TweenComponent::ScaleTo(Zenith_Entity& xEntity, const Zenith_Maths::Vector3& xTo, float fDuration, Zenith_EasingType eEasing)
{
	if (!xEntity.HasComponent<Zenith_TweenComponent>())
	{
		xEntity.AddComponent<Zenith_TweenComponent>();
	}

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(xTo, fDuration, eEasing);
}

//=============================================================================
// Editor Panel
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_TweenComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Tween", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Active Tweens: %u", m_xActiveTweens.GetSize());

		// Display active tweens with progress bars
		for (uint32_t i = 0; i < m_xActiveTweens.GetSize(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			const Zenith_TweenInstance& xTween = m_xActiveTweens.Get(i);

			const char* szProperty = "Unknown";
			switch (xTween.m_eProperty)
			{
			case TWEEN_PROPERTY_POSITION: szProperty = "Position"; break;
			case TWEEN_PROPERTY_SCALE:    szProperty = "Scale"; break;
			case TWEEN_PROPERTY_ROTATION: szProperty = "Rotation"; break;
			}

			float fProgress = xTween.GetNormalizedTime();
			ImGui::Text("%s (%s)", szProperty, Zenith_GetEasingTypeName(xTween.m_eEasing));
			ImGui::SameLine();
			ImGui::ProgressBar(fProgress, ImVec2(-1.0f, 0.0f));

			ImGui::PopID();
		}

		if (m_xActiveTweens.GetSize() > 0)
		{
			if (ImGui::Button("Cancel All"))
			{
				CancelAll();
			}
		}

		ImGui::Separator();

		// Add Tween section
		if (ImGui::TreeNode("Add Tween"))
		{
			static int ls_iProperty = 0;
			static int ls_iEasing = 0;
			static float ls_afFrom[3] = { 0.0f, 0.0f, 0.0f };
			static float ls_afTo[3] = { 1.0f, 1.0f, 1.0f };
			static float ls_fDuration = 1.0f;
			static float ls_fDelay = 0.0f;
			static bool ls_bLoop = false;
			static bool ls_bPingPong = false;

			const char* aszProperties[] = { "Position", "Scale", "Rotation" };
			ImGui::Combo("Property", &ls_iProperty, aszProperties, 3);

			ImGui::DragFloat3("From", ls_afFrom, 0.1f);
			ImGui::DragFloat3("To", ls_afTo, 0.1f);
			ImGui::DragFloat("Duration", &ls_fDuration, 0.05f, 0.01f, 60.0f, "%.2fs");

			// Easing type combo
			if (ImGui::BeginCombo("Easing", Zenith_GetEasingTypeName(static_cast<Zenith_EasingType>(ls_iEasing))))
			{
				for (int e = 0; e < EASING_COUNT; ++e)
				{
					bool bSelected = (ls_iEasing == e);
					if (ImGui::Selectable(Zenith_GetEasingTypeName(static_cast<Zenith_EasingType>(e)), bSelected))
					{
						ls_iEasing = e;
					}
					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::DragFloat("Delay", &ls_fDelay, 0.05f, 0.0f, 60.0f, "%.2fs");
			ImGui::Checkbox("Loop", &ls_bLoop);
			if (ls_bLoop)
			{
				ImGui::SameLine();
				ImGui::Checkbox("Ping-Pong", &ls_bPingPong);
			}

			// Get current transform values button
			if (ImGui::Button("From Current"))
			{
				Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
				switch (ls_iProperty)
				{
				case 0: // Position
				{
					Zenith_Maths::Vector3 xPos;
					xTransform.GetPosition(xPos);
					ls_afFrom[0] = xPos.x; ls_afFrom[1] = xPos.y; ls_afFrom[2] = xPos.z;
					break;
				}
				case 1: // Scale
				{
					Zenith_Maths::Vector3 xScale;
					xTransform.GetScale(xScale);
					ls_afFrom[0] = xScale.x; ls_afFrom[1] = xScale.y; ls_afFrom[2] = xScale.z;
					break;
				}
				case 2: // Rotation
				{
					Zenith_Maths::Quat xRot;
					xTransform.GetRotation(xRot);
					Zenith_Maths::Vector3 xEuler = glm::degrees(glm::eulerAngles(xRot));
					ls_afFrom[0] = xEuler.x; ls_afFrom[1] = xEuler.y; ls_afFrom[2] = xEuler.z;
					break;
				}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Preview"))
			{
				Zenith_TweenInstance xTween;
				xTween.m_eProperty = static_cast<Zenith_TweenProperty>(ls_iProperty);
				xTween.m_eEasing = static_cast<Zenith_EasingType>(ls_iEasing);
				xTween.m_xFrom = Zenith_Maths::Vector3(ls_afFrom[0], ls_afFrom[1], ls_afFrom[2]);
				xTween.m_xTo = Zenith_Maths::Vector3(ls_afTo[0], ls_afTo[1], ls_afTo[2]);
				xTween.m_fDuration = ls_fDuration;
				xTween.m_fDelay = ls_fDelay;
				xTween.m_bLoop = ls_bLoop;
				xTween.m_bPingPong = ls_bPingPong;
				if (ls_iProperty == 2) // Rotation - convert Euler to quaternions
				{
					xTween.m_xFromQuat = Zenith_Maths::Quat(glm::radians(xTween.m_xFrom));
					xTween.m_xToQuat = Zenith_Maths::Quat(glm::radians(xTween.m_xTo));
				}
				m_xActiveTweens.PushBack(xTween);
			}

			ImGui::TreePop();
		}
	}
}
#endif
