#include "Zenith.h"

#include "Test/Components/SphereMovement_Behaviour.h"

SphereMovement_Behaviour::SphereMovement_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void SphereMovement_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	float fCurrentTime = Zenith_Core::GetTimePassed();

	Zenith_Maths::Vector3 xPos = m_xInitialPosition + Zenith_Maths::Vector3(0., sin(fCurrentTime), 0.) * m_fAmplitude;
	xTrans.SetPosition(xPos);
}