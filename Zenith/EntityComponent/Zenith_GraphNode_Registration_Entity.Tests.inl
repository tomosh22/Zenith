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

#endif // ZENITH_TESTING
