#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Test_GraphNodes - the Test game's Behaviour Graph node library.
 *
 * W1 conversion note: the TestSpinPlatform / TestHookesForce physics-toy
 * mega-nodes were DELETED - the boot-authored graphs now compose engine
 * nodes instead (SetAngularVelocity + SetVelocity; ReadEntityPosition +
 * MathBlackboardVector3 + ApplyForce; see BuildGraph_* in Test.cpp). Only
 * the systems seam remains: TestSpawnProjectile executes the component's
 * Shoot() body (ring-slot pooling + prefab apply + launch impulse).
 * Registered from Project_RegisterGameComponents via Test_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Test/Components/Test_PlayerControllerComponent.h"

// Shared resolve: the controller shim, or null. bWalkOnly replicates the old
// OnUpdate early-return - slots/health/compass/shoot were dead in fly-cam
// mode, and the shim nodes preserve that gate.
inline Test_PlayerControllerComponent* Test_ResolveShim(Zenith_GraphContext& xContext, bool bWalkOnly)
{
	Test_PlayerControllerComponent* pxShim = xContext.m_xSelf.IsValid()
		? xContext.m_xSelf.TryGetComponent<Test_PlayerControllerComponent>() : nullptr;
	if (pxShim == nullptr || (bWalkOnly && pxShim->IsFlyCamEnabled()))
	{
		return nullptr;
	}
	return pxShim;
}

// The shoot action: the graph binds the E-press (and the programmatic
// "Shoot" custom event) to this action, which executes the bullet spawn
// systems (ring-slot pooling + prefab apply + launch impulse).
class TestNode_SpawnProjectile : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = Test_ResolveShim(xContext, /*bWalkOnly*/ true);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->Shoot();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestSpawnProjectile"; }
};

// C-key toggle (decision moved from OnUpdate; works in both modes, like the
// old pre-branch check did).
class TestNode_ToggleFlyCam : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = Test_ResolveShim(xContext, /*bWalkOnly*/ false);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->ToggleFlyCam();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestToggleFlyCam"; }
};

// Inventory slot selection (keys 1-6 -> slots 0-5; systems = the UI update
// inside SetSelectedSlot).
class TestNode_SelectSlot : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(TestNode_SelectSlot)
public:
	ZENITH_PROPERTY(int32_t, m_iSlot, 0)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = Test_ResolveShim(xContext, /*bWalkOnly*/ true);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->SetSelectedSlot(m_iSlot);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestSelectSlot"; }
};

// Health demo: negative delta = damage, positive = heal (clamping + HUD
// writes stay in the component's systems body).
class TestNode_ModifyHealth : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(TestNode_ModifyHealth)
public:
	ZENITH_PROPERTY(float, m_fDelta, -10.0f)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = Test_ResolveShim(xContext, /*bWalkOnly*/ true);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (m_fDelta < 0.0f)
		{
			pxShim->TakeDamage(-m_fDelta);
		}
		else
		{
			pxShim->Heal(m_fDelta);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestModifyHealth"; }
};

// Per-tick compass presentation (yaw -> cardinal text stays a systems body).
class TestNode_UpdateCompass : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Test_PlayerControllerComponent* pxShim = Test_ResolveShim(xContext, /*bWalkOnly*/ true);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->UpdateCompassUI();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "TestUpdateCompass"; }
};

inline void Test_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<TestNode_SpawnProjectile>("TestSpawnProjectile", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_ToggleFlyCam>("TestToggleFlyCam", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_SelectSlot>("TestSelectSlot", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_ModifyHealth>("TestModifyHealth", GRAPH_EVENT_NONE, 1, false, "Test");
	xRegistry.RegisterNodeType<TestNode_UpdateCompass>("TestUpdateCompass", GRAPH_EVENT_NONE, 1, false, "Test");
}
