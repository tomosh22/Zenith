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
	xCollider.GetRigidBody()->applyWorldForceAtCenterOfMass({ xPosDelta.x, xPosDelta.y, xPosDelta.z });
}

RotationBehaviour_Behaviour::RotationBehaviour_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void RotationBehaviour_Behaviour::OnUpdate(const float fDt)
{
	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	xCollider.GetRigidBody()->setAngularVelocity({ m_xAngularVel.x, m_xAngularVel.y, m_xAngularVel.z });
	xCollider.GetRigidBody()->setLinearVelocity({ 0.,0.,0. });
}