//------------------------------------------------------------------------------
// Routable-FAILURE coverage for the Entity node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Entity.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope.
//
// ONE table-driven test per owning TU. What each row proves, why the wired
// fail-probe is the positive control, and why the anchor/probes are engine
// nodes addressed by name all live ONCE, in the shared harness:
// Zenith_GraphNodeFailurePin.TestHarness.inl. This file carries only what is
// specific to this TU - the scene fixture, the rows, and the non-opted node.
//
// Both rows run against a REAL Zenith_TempScene holding entities, so the
// FAILURE each reaches is the not-found decision the pin exists for and not
// the invalid-target guard above it.
//
// CROSS-REFERENCE, deliberately not duplicated here: the runtime SEMANTICS of
// the pin are the FailurePin_* tests in
// Zenith/Scripting/Zenith_Scripting.Tests.inl.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"
#include "UnitTests/Zenith_TempScene.h"

ZENITH_TEST(GraphNodeFailurePin, EntityOptIns)
{
	// A real, ACTIVE scene with entities in it. Both rows below have to miss in
	// a populated world, or they would be proving the empty-world case instead.
	Zenith_TempScene xTempScene("FailurePinEntityScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("FailurePinSelf");
	Zenith_Entity xNeighbour = xTempScene.CreateEntity("FailurePinNeighbour");
	ZENITH_ASSERT_TRUE(xSelf.IsValid());
	ZENITH_ASSERT_TRUE(xNeighbour.IsValid());

	const Zenith_FailurePinCase axCases[] =
	{
		// FindEntityByName: FAILURE at Registration_Entity.cpp:361 (NO ENTITY OF
		// THAT NAME). The name is non-empty, so the :343 empty-name
		// misconfiguration guard is never entered, and self's scene IS the
		// active scene, so both lookup passes really run.
		{
			"FindEntityByName",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strName", "__no_entity_has_this_name__");
			}
		},
		// FindNearestEntity: FAILURE at Registration_Entity.cpp:436 (NOTHING
		// MATCHED WITHIN THE RADIUS). A declared VECTOR3 centre keeps the :390
		// unresolvable-centre guard out of the way and an empty filter keeps
		// :396 out of the way; the scene's entities sit at the origin, a million
		// metres outside the half-metre radius.
		{
			"FindNearestEntity",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				Zenith_PropertyValue xCentre;
				xCentre.SetVector3(Zenith_Maths::Vector3(1000000.0f, 1000000.0f, 1000000.0f));
				xBuilder.Variable("failPinFarCentre", xCentre);
				xBuilder.ParamString(uNode, "m_strCenterVar", "failPinFarCentre");
				xBuilder.ParamFloat(uNode, "m_fRadius", 0.5f);
			}
		},
	};

	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), xSelf);

	// ReadEntityPosition fails only on an author mistake (no target / no
	// transform) - exactly the shape the opt-in rule excludes.
	Zenith_CheckNodeIsNotOptedIn("ReadEntityPosition");
}

//==============================================================================
// Pin-table coverage for this TU (the A-6 annotation sweep).
//
// What the totality walk proves, why the registry is SWAPPED to this TU's
// registrar rather than filtered by category, and why the restore is RAII all
// live ONCE, in the shared harness: Zenith_GraphPinTotality.TestHarness.inl.
// Only this TU's registrar and its representative pins are here.
//==============================================================================

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, EntityTotality)
{
	// No exemptions: every m_str*Var* property in this TU names exactly one
	// blackboard variable.
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Entity, "_Entity.cpp", nullptr, 0u);
}

// One representative of each ROLE this TU declares. Roles are what the validator
// consumes, so a role typo is invisible to the totality walk - the pin would
// still count as "covered".
ZENITH_TEST(GraphPinTable, EntityRoleSpotCheck)
{
	// TARGET_ENTITY: resolved through Zenith_GraphContext::ResolveTargetEntity,
	// which accepts a packed ENTITY_ID and NOTHING else - a string entity name
	// is never legal at runtime, and the MASK is what says so.
	Zenith_CheckGraphPin("DetachFromBone", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");
	const Zenith_GraphPinDesc* pxTarget = Zenith_FindGraphPin("DetachFromBone", "Target");
	ZENITH_ASSERT_NOT_NULL(pxTarget);
	if (pxTarget != nullptr)
	{
		ZENITH_ASSERT_EQ(pxTarget->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"ResolveTargetEntity accepts a packed ENTITY_ID and nothing else");
	}
	// The same role under a property name that does not say "Target":
	// QueryEntityValid resolves m_strEntityVar the identical way.
	Zenith_CheckGraphPin("QueryEntityValid", "Entity", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strEntityVar");

	// TARGET_POSITION: the POLYMORPHIC reference - a VECTOR3 world position OR a
	// packed ENTITY_ID whose transform position is used. A wider mask than the
	// entity form, and the difference is the whole point of having two macros.
	Zenith_CheckGraphPin("ComputeDistance", "From", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strFromVar");
	const Zenith_GraphPinDesc* pxFrom = Zenith_FindGraphPin("ComputeDistance", "From");
	ZENITH_ASSERT_NOT_NULL(pxFrom);
	if (pxFrom != nullptr)
	{
		ZENITH_ASSERT_EQ(pxFrom->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_POSITION,
			"a position ref resolves ENTITY_ID or VECTOR3 - narrowing it would reject every literal-position graph");
	}

	// INPUT_VAR_OR_CONST: the ternary shape, both halves declared.
	Zenith_CheckGraphPin("SetEntityScale", "Scale", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_VECTOR3, "m_strScaleVar");
	const Zenith_GraphPinDesc* pxScale = Zenith_FindGraphPin("SetEntityScale", "Scale");
	ZENITH_ASSERT_NOT_NULL(pxScale);
	if (pxScale != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxScale->m_szConstProperty, "m_xScale",
			"the Scale pin lost its inline-constant half, so an unset scale var would look like an unsatisfied read");
	}

	// LIST + OUTPUT on one node: the found entities go into the parallel LIST
	// store, the count into an ordinary INT32 variable.
	Zenith_CheckGraphPin("FindEntitiesInRadius", "List", GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, "m_strListVar");
	Zenith_CheckGraphPin("FindEntitiesInRadius", "Count", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strCountVar");
	// OUTPUT of a packed EntityID - the node's own computed answer.
	Zenith_CheckGraphPin("FindNearestEntity", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_ENTITY_ID, "m_strResultVar");

	// ★ THE TRAP: ReadCameraBasis.m_strPositionVar is a position the node
	// WRITES, not a position reference it resolves - nothing in its Execute
	// passes it to ResolvePositionRef. OUTPUT, not TARGET_REF; annotating it the
	// other way would make the camera's own output read as an undeclared read.
	Zenith_CheckGraphPin("ReadCameraBasis", "Position", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "m_strPositionVar");
	// ...while SpawnPrefab's really is one.
	Zenith_CheckGraphPin("SpawnPrefab", "Position", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strPositionVar");
}

#endif // ZENITH_TESTING
