#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class PlayerController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	PlayerController_Behaviour() = delete;
	PlayerController_Behaviour(Zenith_Entity& xParentEntity);
	~PlayerController_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override {}

	const Zenith_Maths::UVector2& GetPosition() const { return m_xPosition; }

private:
	Zenith_Maths::UVector2 m_xPosition;

	Zenith_Entity m_xParentEntity;
};