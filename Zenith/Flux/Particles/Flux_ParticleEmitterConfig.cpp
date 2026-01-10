#include "Zenith.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"

void Flux_ParticleEmitterConfig::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Spawn Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Spawn Rate", &m_fSpawnRate, 1.0f, 0.0f, 1000.0f, "%.1f particles/sec");
		int iBurstCount = static_cast<int>(m_uBurstCount);
		if (ImGui::DragInt("Burst Count", &iBurstCount, 1, 0, 1000))
		{
			m_uBurstCount = static_cast<uint32_t>(iBurstCount);
		}
		int iMaxParticles = static_cast<int>(m_uMaxParticles);
		if (ImGui::DragInt("Max Particles", &iMaxParticles, 1, 1, 4096))
		{
			m_uMaxParticles = static_cast<uint32_t>(iMaxParticles);
		}
	}

	if (ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Lifetime Min", &m_fLifetimeMin, 0.01f, 0.01f, 60.0f, "%.2f sec");
		ImGui::DragFloat("Lifetime Max", &m_fLifetimeMax, 0.01f, 0.01f, 60.0f, "%.2f sec");
		m_fLifetimeMax = (std::max)(m_fLifetimeMax, m_fLifetimeMin);
	}

	if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Emit Direction", &m_xEmitDirection.x, 0.1f);
		ImGui::DragFloat("Spread Angle", &m_fSpreadAngleDegrees, 1.0f, 0.0f, 180.0f, "%.1f deg");
		ImGui::DragFloat("Speed Min", &m_fSpeedMin, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Speed Max", &m_fSpeedMax, 0.1f, 0.0f, 100.0f);
		m_fSpeedMax = (std::max)(m_fSpeedMax, m_fSpeedMin);
	}

	if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Gravity", &m_xGravity.x, 0.1f);
		ImGui::DragFloat("Drag", &m_fDrag, 0.01f, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit4("Color Start", &m_xColorStart.x);
		ImGui::ColorEdit4("Color End", &m_xColorEnd.x);
	}

	if (ImGui::CollapsingHeader("Size", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Size Start", &m_fSizeStart, 0.01f, 0.01f, 10.0f);
		ImGui::DragFloat("Size End", &m_fSizeEnd, 0.01f, 0.01f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Rotation"))
	{
		ImGui::DragFloat("Rotation Min", &m_fRotationMin, 0.1f, -6.28f, 6.28f, "%.2f rad");
		ImGui::DragFloat("Rotation Max", &m_fRotationMax, 0.1f, -6.28f, 6.28f, "%.2f rad");
		ImGui::DragFloat("Rotation Speed Min", &m_fRotationSpeedMin, 0.1f, -10.0f, 10.0f, "%.2f rad/s");
		ImGui::DragFloat("Rotation Speed Max", &m_fRotationSpeedMax, 0.1f, -10.0f, 10.0f, "%.2f rad/s");
	}

	if (ImGui::CollapsingHeader("Visual"))
	{
		char szTexturePath[256];
		strncpy(szTexturePath, m_strTexturePath.c_str(), sizeof(szTexturePath) - 1);
		szTexturePath[sizeof(szTexturePath) - 1] = '\0';
		if (ImGui::InputText("Texture Path", szTexturePath, sizeof(szTexturePath)))
		{
			m_strTexturePath = szTexturePath;
		}
		ImGui::TextDisabled("(empty = colored quads)");
	}

	if (ImGui::CollapsingHeader("Compute Mode"))
	{
		ImGui::Checkbox("Use GPU Compute", &m_bUseGPUCompute);
		if (m_bUseGPUCompute)
		{
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "GPU: Better for large particle counts");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "CPU: Better for small bursts");
		}
	}
}

#endif // ZENITH_TOOLS
