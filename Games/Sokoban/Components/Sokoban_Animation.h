#pragma once

#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"

#include <algorithm>
#include <cstdint>

// Smooth grid-step animation for the player and a pushed box, plus the dust
// emitter that trails a sliding box. Owns all tween state; the behaviour
// starts steps, polls the visual positions, and is told when a step completes.
class Sokoban_Animation
{
public:
	static constexpr float fSTEP_DURATION = 0.1f;

	bool IsAnimating() const { return m_bAnimating; }
	bool IsBoxAnimating() const { return m_bBoxAnimating; }
	float GetPlayerVisualX() const { return m_fPlayerVisualX; }
	float GetPlayerVisualY() const { return m_fPlayerVisualY; }
	float GetBoxVisualX() const { return m_fBoxVisualX; }
	float GetBoxVisualY() const { return m_fBoxVisualY; }
	uint32_t GetBoxToX() const { return m_uBoxToX; }
	uint32_t GetBoxToY() const { return m_uBoxToY; }

	void StartPlayerMove(uint32_t uFromX, uint32_t uFromY, uint32_t uToX, uint32_t uToY)
	{
		m_bAnimating = true;
		m_fTimer = 0.f;
		m_fPlayerStartX = static_cast<float>(uFromX);
		m_fPlayerStartY = static_cast<float>(uFromY);
		m_fPlayerVisualX = m_fPlayerStartX;
		m_fPlayerVisualY = m_fPlayerStartY;
		m_uPlayerTargetX = uToX;
		m_uPlayerTargetY = uToY;
	}

	void StartBoxPush(uint32_t uFromX, uint32_t uFromY, uint32_t uToX, uint32_t uToY)
	{
		m_bBoxAnimating = true;
		m_uBoxFromX = uFromX;
		m_uBoxFromY = uFromY;
		m_uBoxToX = uToX;
		m_uBoxToY = uToY;
		m_fBoxVisualX = static_cast<float>(uFromX);
		m_fBoxVisualY = static_cast<float>(uFromY);
	}

	// Place the player visual without animating (level start / reset).
	void SnapPlayerTo(uint32_t uX, uint32_t uY)
	{
		m_fPlayerVisualX = static_cast<float>(uX);
		m_fPlayerVisualY = static_cast<float>(uY);
	}

	// Abort any in-flight step (menu return / level regen).
	void Cancel(Zenith_EntityID uDustEmitterID)
	{
		m_bAnimating = false;
		m_bBoxAnimating = false;
		StopDust(uDustEmitterID);
	}

	// Advance the tween; returns true on the frame the step completes.
	bool Update(float fDt, uint32_t uGridWidth, uint32_t uGridHeight, Zenith_EntityID uDustEmitterID)
	{
		m_fTimer += fDt;
		const float fProgress = std::min(m_fTimer / fSTEP_DURATION, 1.f);

		m_fPlayerVisualX = m_fPlayerStartX + (static_cast<float>(m_uPlayerTargetX) - m_fPlayerStartX) * fProgress;
		m_fPlayerVisualY = m_fPlayerStartY + (static_cast<float>(m_uPlayerTargetY) - m_fPlayerStartY) * fProgress;

		if (m_bBoxAnimating)
		{
			m_fBoxVisualX = static_cast<float>(m_uBoxFromX) +
				(static_cast<float>(m_uBoxToX) - static_cast<float>(m_uBoxFromX)) * fProgress;
			m_fBoxVisualY = static_cast<float>(m_uBoxFromY) +
				(static_cast<float>(m_uBoxToY) - static_cast<float>(m_uBoxFromY)) * fProgress;

			UpdateDust(uGridWidth, uGridHeight, uDustEmitterID);
		}

		if (fProgress < 1.f)
		{
			return false;
		}

		m_bAnimating = false;
		m_bBoxAnimating = false;
		m_fPlayerVisualX = static_cast<float>(m_uPlayerTargetX);
		m_fPlayerVisualY = static_cast<float>(m_uPlayerTargetY);
		StopDust(uDustEmitterID);
		return true;
	}

private:
	// The dust emitter lives in the persistent scene (DontDestroyOnLoad).
	static Zenith_ParticleEmitterComponent* ResolveDustEmitter(Zenith_EntityID uDustEmitterID)
	{
		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xPersistentScene);
		if (!pxSceneData || uDustEmitterID == INVALID_ENTITY_ID || !pxSceneData->EntityExists(uDustEmitterID))
		{
			return nullptr;
		}
		Zenith_Entity xEmitterEntity = pxSceneData->GetEntity(uDustEmitterID);
		if (!xEmitterEntity.HasComponent<Zenith_ParticleEmitterComponent>())
		{
			return nullptr;
		}
		return &xEmitterEntity.GetComponent<Zenith_ParticleEmitterComponent>();
	}

	void UpdateDust(uint32_t uGridWidth, uint32_t uGridHeight, Zenith_EntityID uDustEmitterID)
	{
		Zenith_ParticleEmitterComponent* pxEmitter = ResolveDustEmitter(uDustEmitterID);
		if (!pxEmitter)
		{
			return;
		}

		const float fOffsetX = -static_cast<float>(uGridWidth) * 0.5f + 0.5f;
		const float fOffsetZ = -static_cast<float>(uGridHeight) * 0.5f + 0.5f;
		pxEmitter->SetEmitPosition({ m_fBoxVisualX + fOffsetX, 0.1f, m_fBoxVisualY + fOffsetZ });
		pxEmitter->SetEmitDirection({ 0.0f, 1.0f, 0.0f });
		pxEmitter->SetEmitting(true);
	}

	void StopDust(Zenith_EntityID uDustEmitterID)
	{
		if (Zenith_ParticleEmitterComponent* pxEmitter = ResolveDustEmitter(uDustEmitterID))
		{
			pxEmitter->SetEmitting(false);
		}
	}

	bool m_bAnimating = false;
	float m_fTimer = 0.f;
	float m_fPlayerVisualX = 0.f;
	float m_fPlayerVisualY = 0.f;
	float m_fPlayerStartX = 0.f;
	float m_fPlayerStartY = 0.f;
	uint32_t m_uPlayerTargetX = 0;
	uint32_t m_uPlayerTargetY = 0;

	bool m_bBoxAnimating = false;
	uint32_t m_uBoxFromX = 0;
	uint32_t m_uBoxFromY = 0;
	uint32_t m_uBoxToX = 0;
	uint32_t m_uBoxToY = 0;
	float m_fBoxVisualX = 0.f;
	float m_fBoxVisualY = 0.f;
};
