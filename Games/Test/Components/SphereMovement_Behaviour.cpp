#include "Zenith.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"

HookesLaw_Behaviour::HookesLaw_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void HookesLaw_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	float fCurrentTime = Zenith_Core::GetTimePassed();

	Zenith_Maths::Vector3 xPosDelta;
	xTrans.GetPosition(xPosDelta);
	xPosDelta = m_xDesiredPosition - xPosDelta;

	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::AddForce(xCollider.GetRigidBody(), xPosDelta);
}

RotationBehaviour_Behaviour::RotationBehaviour_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void RotationBehaviour_Behaviour::OnUpdate(const float fDt)
{
	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetAngularVelocity(xCollider.GetRigidBody(), m_xAngularVel);
	Zenith_Physics::SetLinearVelocity(xCollider.GetRigidBody(), Zenith_Maths::Vector3(0.0, 0.0, 0.0));
}