#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Test/Components/Test_HookesLawComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"

Test_HookesLawComponent::Test_HookesLawComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Test_HookesLawComponent::OnUpdate(const float)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Vector3 xPosDelta;
	xTrans.GetPosition(xPosDelta);
	xPosDelta = m_xDesiredPosition - xPosDelta;

	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().AddForce(xCollider.GetBodyID(), xPosDelta);
}
