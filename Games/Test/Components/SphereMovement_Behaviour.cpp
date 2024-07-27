#include "Zenith.h"

#include "Test/Components/SphereMovement_Behaviour.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"

SphereMovement_Behaviour::SphereMovement_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void SphereMovement_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	float fCurrentTime = Zenith_Core::GetTimePassed();

	Zenith_Maths::Vector3 xPosDelta;
	xTrans.GetPosition(xPosDelta);
	xPosDelta = m_xDesiredPosition - xPosDelta;

	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	xCollider.GetRigidBody()->applyWorldForceAtCenterOfMass({ xPosDelta.x, xPosDelta.y, xPosDelta.z });
}