#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

class HookesLaw_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(HookesLaw_Behaviour)

	HookesLaw_Behaviour() = delete;
	HookesLaw_Behaviour(Zenith_Entity& xParentEntity);
	~HookesLaw_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override {}

	void SetDesiredPosition(const Zenith_Maths::Vector3& xPos) { m_xDesiredPosition = xPos; }

private:
	Zenith_Maths::Vector3 m_xDesiredPosition = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};

class RotationBehaviour_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	// Serialization support - type name and factory registration
	ZENITH_BEHAVIOUR_TYPE_NAME(RotationBehaviour_Behaviour)

	RotationBehaviour_Behaviour() = delete;
	RotationBehaviour_Behaviour(Zenith_Entity& xParentEntity);
	~RotationBehaviour_Behaviour() = default;

	void OnUpdate(const float fDt) ZENITH_FINAL override;
	void OnCreate() ZENITH_FINAL override {}

	void SetAngularVel(const Zenith_Maths::Vector3& xVel) { m_xAngularVel = xVel; }

private:
	Zenith_Maths::Vector3 m_xAngularVel = { 0,0,0 };

	Zenith_Entity m_xParentEntity;
};
