#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Test/Components/Test_RotationComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"

Test_RotationComponent::Test_RotationComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Test_RotationComponent::OnUpdate(const float)
{
	Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), m_xAngularVel);
	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0.0, 0.0, 0.0));
}
