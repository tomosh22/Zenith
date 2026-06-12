#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Test_GraphNodes - the Test game's Behaviour Graph node library.
 *
 * Two physics toys mirroring the retired Test_RotationComponent /
 * Test_HookesLawComponent bodies exactly (constant angular velocity spin;
 * Hooke's-law spring force toward a target position). Registered from
 * Project_RegisterGameComponents via Test_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "Test/Components/Test_PlayerControllerComponent.h"

// Test_RotationComponent::OnUpdate verbatim: constant angular velocity, zeroed
// linear velocity (keeps the spinning platform anchored).
class TestNode_SpinPlatform : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(TestNode_SpinPlatform)
public:
	ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xAngularVel, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid() || !xContext.m_xSelf.HasComponent<Zenith_ColliderComponent>())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_ColliderComponent& xCollider = xContext.m_xSelf.GetComponent<Zenith_ColliderComponent>();
		g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), m_xAngularVel);
		g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestSpinPlatform"; }
};

// Test_HookesLawComponent::OnUpdate verbatim: spring force toward the desired
// position (force = displacement; Jolt integrates).
class TestNode_HookesForce : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(TestNode_HookesForce)
public:
	ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xDesiredPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()
			|| !xContext.m_xSelf.HasComponent<Zenith_TransformComponent>()
			|| !xContext.m_xSelf.HasComponent<Zenith_ColliderComponent>())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_TransformComponent& xTrans = xContext.m_xSelf.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xPosDelta;
		xTrans.GetPosition(xPosDelta);
		xPosDelta = m_xDesiredPosition - xPosDelta;
		Zenith_ColliderComponent& xCollider = xContext.m_xSelf.GetComponent<Zenith_ColliderComponent>();
		g_xEngine.Physics().AddForce(xCollider.GetBodyID(), xPosDelta);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestHookesForce"; }
};

// The shoot action (wave 2): the E-press plumbing stays on
// Test_PlayerControllerComponent (which fires the "Shoot" custom event); the
// graph binds the event to this action, which executes the bullet spawn
// systems (ring-slot pooling + prefab apply + launch impulse) back through
// the component.
class TestNode_SpawnProjectile : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Test_PlayerControllerComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Shoot();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestSpawnProjectile"; }
};

inline void Test_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<TestNode_SpinPlatform>("TestSpinPlatform", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_HookesForce>("TestHookesForce", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_SpawnProjectile>("TestSpawnProjectile", GRAPH_EVENT_NONE, 1, false, "Test");
}
