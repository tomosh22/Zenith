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
#include "UnitTests/Zenith_UnitTests.h"

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
		// FindEntityByName: FAILURE at the !xFound.IsValid() guard AFTER both
		// lookup passes (NO ENTITY OF THAT NAME). The name is non-empty, so the
		// m_strName.empty() misconfiguration guard is never entered, and self's
		// scene IS the active scene, so both lookup passes really run.
		{
			"FindEntityByName",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strName", "__no_entity_has_this_name__");
			}
		},
		// FindNearestEntity: FAILURE at the !bFound guard after the query walk
		// (NOTHING MATCHED WITHIN THE RADIUS). A declared VECTOR3 centre keeps the
		// unresolvable-centre guard out of the way and an empty m_strComponentType
		// keeps the unknown-filter guard out of the way; the scene's entities sit at
		// the origin, a million metres outside the half-metre radius.
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

//==============================================================================
// PIN RUNTIME for this TU (B-6.2) - the pins are LIVE.
//
// Every node above reads its INPUT descriptors through Zenith_GraphNode::GetInput
// and writes its OUTPUT descriptors through SetOutput. The three tests ABOVE are
// part of the proof that an UNCONNECTED node is unchanged; the far stronger
// cross-checks are GraphComponent.EntityNodeFamilyRemainderExecution (nine of the
// OUTPUT writes, through real graph instances, INCLUDING a ReadCameraBasis whose Up
// and Position names are empty) and GraphComponent.EntityTargetingActsOnOtherEntity
// - both stay green UNCHANGED. The rows below are the proof that a WIRE now carries
// a value, and that the nine formerly-guarded writes behave exactly as they did on
// the blackboard while latching their slots.
//
// ★ EVERY Wired_* ROW SETS THREE LEGS and asserts the third: the const property
// (9), the var-name property pointing at a blackboard variable holding a second
// value (7), and SetInputForTest (5). Only a live wire can produce 5, and a wired
// pin must log NO census FALLBACK line. RotateTowardDirection.Direction has no const
// property at all, so it gets two legs - which its own row says out loud.
//
// ★ NODES ARE CONSTRUCTED DIRECTLY ON THE STACK with m_strTargetVar = "" and
// xContext.m_xSelf pointing at a Zenith_TempScene entity. That is legal - and
// exercises this TU's real classes - because the accessors SELF-BIND (B-6.1).
//
// ★ ORDERING RULE: pin state is built ONCE, on the first accessor call, from the
// properties as they read THEN. Assign every property before the first Execute.
//
// ★ A TYPED OUTPUT SLOT STARTS *SET*, at its stamped zero, so "the slot is latched"
// is unfalsifiable on its own. Every Output_* row therefore asserts the slot's
// VALUE against a fixture value that is distinguishable from zero - a non-origin
// position, a 2.0 nearest distance, a (0,1,0) camera up, a non-origin camera
// position, a 45-degree euler.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================


// One row of the index contract: the constant addresses the pin it is named for,
// with the role the migration assumed.
inline void EntityPin_Check(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
	Zenith_GraphPinRole eRole, const char* szClass)
{
	ZENITH_ASSERT_LT(uIndex, xPins.GetPinCount(), "%s: pin index %u is past the end of the table", szClass, uIndex);
	if (uIndex >= xPins.GetPinCount())
	{
		return;
	}
	const Zenith_GraphPinDesc& xDesc = xPins.GetPinAt(uIndex);
	ZENITH_ASSERT_STREQ(xDesc.m_szName, szName,
		"%s: pin %u is '%s', not '%s' - the table was REORDERED, so every uPIN_ constant now addresses the wrong pin",
		szClass, uIndex, xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)", szName);
	ZENITH_ASSERT_EQ(static_cast<int>(xDesc.m_eRole), static_cast<int>(eRole),
		"%s: pin '%s' changed ROLE - its runtime accessor would bad-access", szClass, szName);
}

inline void EntityPin_SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	xBB.SetValue(szName, xValue);
}

inline void EntityPin_SeedVec3(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	xBB.SetValue(szName, xValue);
}

inline Zenith_PropertyValue EntityPin_WireFloat(float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return xValue;
}

inline Zenith_PropertyValue EntityPin_WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

// Slot readers. The tagged getters ASSERT, so the tag is checked here first - an
// assertion failure must read as a test failure and not as a DebugBreak.
inline Zenith_Maths::Vector3 EntityPin_SlotVec3(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_VECTOR3),
		"%s: the slot holds type %u, not VECTOR3", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_VECTOR3 ? pxSlot->GetVector3() : Zenith_Maths::Vector3(0.0f);
}

inline float EntityPin_SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0.0f;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_FLOAT),
		"%s: the slot holds type %u, not FLOAT", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_FLOAT ? pxSlot->GetFloat() : 0.0f;
}

inline int32_t EntityPin_SlotInt(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_INT32),
		"%s: the slot holds type %u, not INT32", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_INT32 ? pxSlot->GetInt32() : 0;
}

inline u_int64 EntityPin_SlotEntity(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_ENTITY_ID),
		"%s: the slot holds type %u, not ENTITY_ID", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_ENTITY_ID ? pxSlot->GetPackedEntityID() : 0;
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so a
// reorder - or an inserted pin - silently re-points every uPIN_ constant in this TU
// at a different descriptor. The TARGET_ENTITY / TARGET_POSITION / LIST pins carry no
// constant of their own (they stay direct), but they DO occupy table indices, so the
// per-class pin COUNT is asserted for every one of the 15 nodes: an appended or
// inserted pin fails here rather than quietly shifting an INPUT or OUTPUT.
// A static_assert is impossible - the tables are filled at static init, so this is
// the earliest a check can run.
ZENITH_TEST(GraphPinTable, EntityPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xReadPos = Zenith_GraphNode_ReadEntityPosition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xReadPos.GetPinCount(), 2u, "ReadEntityPosition gained or lost a pin");
	EntityPin_Check(xReadPos, Zenith_GraphNode_ReadEntityPosition::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadEntityPosition");

	const Zenith_GraphPinTable& xScale = Zenith_GraphNode_SetEntityScale::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xScale.GetPinCount(), 2u, "SetEntityScale gained or lost a pin");
	EntityPin_Check(xScale, Zenith_GraphNode_SetEntityScale::uPIN_Scale, "Scale", GRAPH_PIN_ROLE_INPUT,
		"SetEntityScale");

	const Zenith_GraphPinTable& xValid = Zenith_GraphNode_QueryEntityValid::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xValid.GetPinCount(), 2u, "QueryEntityValid gained or lost a pin");
	EntityPin_Check(xValid, Zenith_GraphNode_QueryEntityValid::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"QueryEntityValid");

	const Zenith_GraphPinTable& xDist = Zenith_GraphNode_ComputeDistance::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xDist.GetPinCount(), 3u, "ComputeDistance gained or lost a pin");
	EntityPin_Check(xDist, Zenith_GraphNode_ComputeDistance::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ComputeDistance");

	const Zenith_GraphPinTable& xRadius = Zenith_GraphNode_FindEntitiesInRadius::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRadius.GetPinCount(), 3u, "FindEntitiesInRadius gained or lost a pin");
	EntityPin_Check(xRadius, Zenith_GraphNode_FindEntitiesInRadius::uPIN_Count, "Count", GRAPH_PIN_ROLE_OUTPUT,
		"FindEntitiesInRadius");

	const Zenith_GraphPinTable& xDir = Zenith_GraphNode_ComputeDirection::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xDir.GetPinCount(), 3u, "ComputeDirection gained or lost a pin");
	EntityPin_Check(xDir, Zenith_GraphNode_ComputeDirection::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ComputeDirection");

	const Zenith_GraphPinTable& xSpawn = Zenith_GraphNode_SpawnPrefab::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSpawn.GetPinCount(), 2u, "SpawnPrefab gained or lost a pin");
	EntityPin_Check(xSpawn, Zenith_GraphNode_SpawnPrefab::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"SpawnPrefab");

	const Zenith_GraphPinTable& xByName = Zenith_GraphNode_FindEntityByName::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xByName.GetPinCount(), 1u, "FindEntityByName gained or lost a pin");
	EntityPin_Check(xByName, Zenith_GraphNode_FindEntityByName::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"FindEntityByName");

	const Zenith_GraphPinTable& xNearest = Zenith_GraphNode_FindNearestEntity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xNearest.GetPinCount(), 3u, "FindNearestEntity gained or lost a pin");
	EntityPin_Check(xNearest, Zenith_GraphNode_FindNearestEntity::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"FindNearestEntity");
	EntityPin_Check(xNearest, Zenith_GraphNode_FindNearestEntity::uPIN_Distance, "Distance", GRAPH_PIN_ROLE_OUTPUT,
		"FindNearestEntity");

	// Two TARGET_REF pins and nothing else: no uPIN_ constant exists because
	// nothing in its Execute addresses a pin. The count is the whole contract.
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AttachToBone::GetPinTableStatic().GetPinCount(), 2u,
		"AttachToBone gained or lost a pin; both of its pins are TARGET_REF and stay direct");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_DetachFromBone::GetPinTableStatic().GetPinCount(), 1u,
		"DetachFromBone gained a pin; nothing in its Execute addresses one");

	const Zenith_GraphPinTable& xBasis = Zenith_GraphNode_ReadCameraBasis::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xBasis.GetPinCount(), 4u, "ReadCameraBasis gained or lost a pin");
	EntityPin_Check(xBasis, Zenith_GraphNode_ReadCameraBasis::uPIN_Forward, "Forward", GRAPH_PIN_ROLE_OUTPUT,
		"ReadCameraBasis");
	EntityPin_Check(xBasis, Zenith_GraphNode_ReadCameraBasis::uPIN_Right, "Right", GRAPH_PIN_ROLE_OUTPUT,
		"ReadCameraBasis");
	EntityPin_Check(xBasis, Zenith_GraphNode_ReadCameraBasis::uPIN_Up, "Up", GRAPH_PIN_ROLE_OUTPUT,
		"ReadCameraBasis");
	EntityPin_Check(xBasis, Zenith_GraphNode_ReadCameraBasis::uPIN_Position, "Position", GRAPH_PIN_ROLE_OUTPUT,
		"ReadCameraBasis");

	const Zenith_GraphPinTable& xPitchYaw = Zenith_GraphNode_SetCameraPitchYaw::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPitchYaw.GetPinCount(), 2u, "SetCameraPitchYaw gained or lost a pin");
	EntityPin_Check(xPitchYaw, Zenith_GraphNode_SetCameraPitchYaw::uPIN_Pitch, "Pitch", GRAPH_PIN_ROLE_INPUT,
		"SetCameraPitchYaw");
	EntityPin_Check(xPitchYaw, Zenith_GraphNode_SetCameraPitchYaw::uPIN_Yaw, "Yaw", GRAPH_PIN_ROLE_INPUT,
		"SetCameraPitchYaw");

	const Zenith_GraphPinTable& xRotate = Zenith_GraphNode_RotateTowardDirection::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRotate.GetPinCount(), 2u, "RotateTowardDirection gained or lost a pin");
	EntityPin_Check(xRotate, Zenith_GraphNode_RotateTowardDirection::uPIN_Direction, "Direction",
		GRAPH_PIN_ROLE_INPUT, "RotateTowardDirection");

	const Zenith_GraphPinTable& xReadRot = Zenith_GraphNode_ReadEntityRotation::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xReadRot.GetPinCount(), 3u, "ReadEntityRotation gained or lost a pin");
	EntityPin_Check(xReadRot, Zenith_GraphNode_ReadEntityRotation::uPIN_Forward, "Forward", GRAPH_PIN_ROLE_OUTPUT,
		"ReadEntityRotation");
	EntityPin_Check(xReadRot, Zenith_GraphNode_ReadEntityRotation::uPIN_Euler, "Euler", GRAPH_PIN_ROLE_OUTPUT,
		"ReadEntityRotation");
}

//------------------------------------------------------------------------------
// WIRED INPUTS
//------------------------------------------------------------------------------

ZENITH_TEST(EntityPinRuntime, Wired_SetEntityScaleFromWire)
{
	Zenith_TempScene xTempScene("EntityPinScaleScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinScaleSelf");
	ZENITH_ASSERT_TRUE(xSelf.IsValid());

	Zenith_GraphBlackboard xBB;
	EntityPin_SeedVec3(xBB, "sv", Zenith_Maths::Vector3(7.0f, 7.0f, 7.0f));	// leg 2

	Zenith_GraphNode_SetEntityScale xNode;
	xNode.m_xScale = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);				// leg 1
	xNode.m_strScaleVar = "sv";
	xNode.m_strTargetVar = "";												// "" = self
	// leg 3: THREE DISTINCT COMPONENTS, so a transposed or partially-read vector
	// fails rather than agreeing by symmetry.
	xNode.SetInputForTest(Zenith_GraphNode_SetEntityScale::uPIN_Scale,
		EntityPin_WireVec3(Zenith_Maths::Vector3(5.0f, 6.0f, 7.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	Zenith_Maths::Vector3 xScale;
	xSelf.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_NEAR_VEC3(xScale, Zenith_Maths::Vector3(5.0f, 6.0f, 7.0f), 0.0001f);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetEntityScale::uPIN_Scale), 0u,
		"a WIRED pin must never reach the transitional var-name fallback");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(EntityPinRuntime, Wired_SetCameraPitchYaw)
{
	// The camera fixture: a Zenith_CameraComponent in THIS test's own temp scene,
	// promoted to main camera through the friend-gated test seam. Without a camera
	// the node FAILS before either pin is read, so a zero fallback count would be
	// vacuous - which is why this row asserts the camera really did resolve.
	Zenith_TempScene xTempScene("EntityPinCameraScene");
	Zenith_Entity xCameraEntity = xTempScene.CreateEntity("EntityPinCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_UnitTests::SetMainCameraForTest(xTempScene.Data(), xCameraEntity.GetEntityID());
	// Another suite may leave a camera resolvable, so assert it is THIS one.
	ZENITH_ASSERT_TRUE(Zenith_GetMainCameraAcrossScenes() == &xCamera,
		"the main camera this test drives is not the one it created");

	xCamera.SetPitch(0.0);
	xCamera.SetYaw(0.0);

	Zenith_GraphBlackboard xBB;
	EntityPin_SeedFloat(xBB, "pv", 7.0f);	// leg 2
	EntityPin_SeedFloat(xBB, "yv", 7.0f);

	Zenith_GraphNode_SetCameraPitchYaw xNode;
	xNode.m_fPitchDegrees = 9.0f;			// leg 1
	xNode.m_strPitchVar = "pv";
	xNode.m_fYawDegrees = 9.0f;
	xNode.m_strYawVar = "yv";
	xNode.m_bAdditive = false;
	xNode.m_bClampPitch = true;				// 5 and 12 are both well inside +/-89
	// leg 3: DIFFERENT values per pin, so a swapped Pitch/Yaw index fails.
	xNode.SetInputForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Pitch, EntityPin_WireFloat(5.0f));
	xNode.SetInputForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Yaw, EntityPin_WireFloat(12.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCamera.GetPitch()), glm::radians(5.0f), 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCamera.GetYaw()), glm::radians(12.0f), 0.0001f);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Pitch), 0u);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Yaw), 0u);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(EntityPinRuntime, Wired_RotateTowardDirectionPlainInput)
{
	// ★ TWO LEGS, NOT THREE: Direction is a PLAIN INPUT with no const twin, so the
	// legs are the blackboard variable and the wire. The node flattens y (m_bYawOnly
	// defaults true) and normalises, so magnitude and y differences are ERASED - the
	// two legs must be distinct DIRECTIONS.
	Zenith_TempScene xTempScene("EntityPinRotateScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinRotateSelf");
	ZENITH_ASSERT_TRUE(xSelf.IsValid());

	Zenith_GraphBlackboard xBB;
	EntityPin_SeedVec3(xBB, "dir", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));	// leg 1: +Z

	{
		Zenith_GraphNode_RotateTowardDirection xNode;
		xNode.m_strDirectionVar = "dir";
		xNode.m_fDegreesPerSecond = 0.0f;	// snap
		xNode.m_bYawOnly = true;
		xNode.m_strTargetVar = "";
		xNode.SetInputForTest(Zenith_GraphNode_RotateTowardDirection::uPIN_Direction,
			EntityPin_WireVec3(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));	// leg 2: +X

		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

		Zenith_Maths::Quat xRotation;
		xSelf.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
		const Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		ZENITH_ASSERT_NEAR_VEC3(xForward, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.01f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_RotateTowardDirection::uPIN_Direction), 0u);
	}

	// UNWIRED with an EMPTY var name: the pin has no const property, so its default
	// IS the VECTOR3 zero - which is exactly what the old GetVector3("") produced,
	// and a zero direction is FAILURE. Byte-for-byte today's behaviour.
	{
		Zenith_GraphNode_RotateTowardDirection xNode;
		xNode.m_strDirectionVar = "";
		xNode.m_strTargetVar = "";

		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE),
			"an unnamed, unwired Direction must read the type zero and FAIL on it");
	}

	// ...and a var name that names NOTHING takes the same route (the typed getter it
	// replaced defaulted on a missing name identically).
	{
		Zenith_GraphNode_RotateTowardDirection xNode;
		xNode.m_strDirectionVar = "__no_such_direction__";
		xNode.m_strTargetVar = "";

		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	}
}

//------------------------------------------------------------------------------
// OUTPUTS. Six writers were always unconditional and take B-6.1's `""`
// divergence; nine were guarded by `!m_strXVar.empty()` and are PARITY rows -
// the blackboard sees exactly what it saw, and the slot is latched as well.
//------------------------------------------------------------------------------

ZENITH_TEST(EntityPinRuntime, Output_ReadEntityPositionCarriesValueAndDualWrites)
{
	Zenith_TempScene xTempScene("EntityPinReadPosScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinReadPosSelf");
	xSelf.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_ReadEntityPosition xNode;
	xNode.m_strTargetVar = "";
	xNode.m_strResultVar = "pos";

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// The SLOT carries the position (a NON-ORIGIN fixture, so this is not the
	// stamped zero a typed slot starts at)...
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadEntityPosition::uPIN_Result), "ReadEntityPosition.Result"),
		Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f), 0.0001f);
	// ...and the TRANSITIONAL dual-write put the same value where the old
	// SetValue(m_strResultVar) put it.
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("pos"), Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f), 0.0001f);
}

// ★ THE `""` DIVERGENCE, on an UNGUARDED writer. This write was never inside an
// `!empty()` guard, so today it created a blackboard variable literally named "".
// It no longer does. Nothing can have depended on it - a graph cannot declare, wire
// or read an empty name - but it is a behaviour change and belongs in a test.
ZENITH_TEST(EntityPinRuntime, Output_ReadEntityPositionEmptyResultVarCreatesNoBlackboardEntry)
{
	Zenith_TempScene xTempScene("EntityPinReadPosEmptyScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinReadPosEmptySelf");
	xSelf.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_ReadEntityPosition xNode;
	xNode.m_strTargetVar = "";
	xNode.m_strResultVar = "";

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// The value is NOT lost - it simply has no dual-write.
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadEntityPosition::uPIN_Result), "ReadEntityPosition.Result"),
		Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f), 0.0001f);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "an unnamed OUTPUT created a blackboard variable");
}

// ★ A PARITY ROW, not a divergence row. All four ReadCameraBasis writes used to be
// guarded by their own `!empty()`; Up and Position default to NO name. The
// blackboard therefore sees exactly what it saw (Forward and Right only, count 2),
// while Up and Position now LATCH - which is what lets a wire come off them.
ZENITH_TEST(EntityPinRuntime, Output_ReadCameraBasisEmptyNamesCarryValuesWithoutWriting)
{
	Zenith_TempScene xTempScene("EntityPinBasisScene");
	Zenith_Entity xCameraEntity = xTempScene.CreateEntity("EntityPinBasisCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_UnitTests::SetMainCameraForTest(xTempScene.Data(), xCameraEntity.GetEntityID());
	ZENITH_ASSERT_TRUE(Zenith_GetMainCameraAcrossScenes() == &xCamera,
		"the main camera this test reads is not the one it created");

	xCamera.SetPitch(0.0);
	xCamera.SetYaw(0.0);
	xCamera.SetPosition(Zenith_Maths::Vector3(11.0f, 12.0f, 13.0f));	// NON-ORIGIN on purpose

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_ReadCameraBasis xNode;
	xNode.m_bFlattenXZ = false;
	xNode.m_strForwardVar = "camForward";
	xNode.m_strRightVar = "camRight";
	xNode.m_strUpVar = "";			// the default - no blackboard write, ever
	xNode.m_strPositionVar = "";	// likewise

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// yaw 0 faces +Z, and right = normalize(cross(+Y, +Z)) = +X.
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadCameraBasis::uPIN_Forward), "ReadCameraBasis.Forward"),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 0.01f);
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadCameraBasis::uPIN_Right), "ReadCameraBasis.Right"),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.01f);
	// The two formerly-guarded ones. (0,1,0) and (11,12,13) are both distinguishable
	// from the stamped VECTOR3 zero a typed slot starts at.
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadCameraBasis::uPIN_Up), "ReadCameraBasis.Up"),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.01f);
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadCameraBasis::uPIN_Position), "ReadCameraBasis.Position"),
		Zenith_Maths::Vector3(11.0f, 12.0f, 13.0f), 0.0001f);

	// PARITY on the blackboard: the two named pins wrote, the two unnamed did not.
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("camForward"), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 0.01f);
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("camRight"), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.01f);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 2u,
		"an empty Up/Position name must write NO blackboard variable - that guard's observable is the contract");
}

// The guard most likely to be kept by mistake, because m_strEulerVar's default IS
// empty: the Euler value must reach its slot anyway.
ZENITH_TEST(EntityPinRuntime, Output_ReadEntityRotationEulerCarriesValueWithEmptyName)
{
	Zenith_TempScene xTempScene("EntityPinReadRotScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinReadRotSelf");
	// 45 degrees about +Y. Deliberately NOT 90: glm::eulerAngles extracts its middle
	// angle through asin, so +/-90 sits exactly on that branch boundary.
	xSelf.GetComponent<Zenith_TransformComponent>().SetRotation(
		glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_ReadEntityRotation xNode;
	xNode.m_strForwardVar = "forward";
	xNode.m_strEulerVar = "";		// the DEFAULT - no blackboard write
	xNode.m_strTargetVar = "";

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const float fRoot2 = 0.70710678f;
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadEntityRotation::uPIN_Forward), "ReadEntityRotation.Forward"),
		Zenith_Maths::Vector3(fRoot2, 0.0f, fRoot2), 0.01f);
	// The formerly-guarded one: 45 degrees of yaw, nothing else.
	ZENITH_ASSERT_NEAR_VEC3(EntityPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadEntityRotation::uPIN_Euler), "ReadEntityRotation.Euler"),
		Zenith_Maths::Vector3(0.0f, 45.0f, 0.0f), 0.1f);

	// PARITY: Forward wrote, Euler did not.
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("forward"), Zenith_Maths::Vector3(fRoot2, 0.0f, fRoot2), 0.01f);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 1u, "an empty m_strEulerVar must write no blackboard variable");
}

ZENITH_TEST(EntityPinRuntime, Output_FindNearestEntityDistanceCarriesValueWithEmptyName)
{
	// Far from the origin, with a small radius: FindNearestEntity walks EVERY loaded
	// scene, so origin-dwelling fixtures from other suites (and the persistent
	// scene) must be outside it or this row would be measuring them.
	Zenith_TempScene xTempScene("EntityPinNearestScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinNearestSelf");
	xSelf.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(500.0f, 0.0f, 0.0f));
	Zenith_Entity xNeighbour = xTempScene.CreateEntity("EntityPinNearestNeighbour");
	xNeighbour.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(502.0f, 0.0f, 0.0f));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_FindNearestEntity xNode;
	xNode.m_strCenterVar = "";			// "" = self
	xNode.m_fRadius = 5.0f;
	xNode.m_strComponentType = "";
	xNode.m_strResultVar = "nearest";
	xNode.m_strDistanceVar = "";		// the DEFAULT - no blackboard write

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ(EntityPin_SlotEntity(
		xNode.GetOutputForTest(Zenith_GraphNode_FindNearestEntity::uPIN_Result), "FindNearestEntity.Result"),
		xNeighbour.GetEntityID().GetPacked());
	// 2.0, not zero: the formerly-guarded Distance write reaches its slot.
	ZENITH_ASSERT_EQ_FLOAT(EntityPin_SlotFloat(
		xNode.GetOutputForTest(Zenith_GraphNode_FindNearestEntity::uPIN_Distance), "FindNearestEntity.Distance"),
		2.0f, 0.001f);

	ZENITH_ASSERT_EQ(xBB.GetPackedEntityID("nearest"), xNeighbour.GetEntityID().GetPacked());
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 1u, "an empty m_strDistanceVar must write no blackboard variable");
}

ZENITH_TEST(EntityPinRuntime, Output_FindEntitiesInRadiusCountWithEmptyCountVar)
{
	Zenith_TempScene xTempScene("EntityPinRadiusScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinRadiusSelf");
	xSelf.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(600.0f, 0.0f, 0.0f));
	Zenith_Entity xNeighbour = xTempScene.CreateEntity("EntityPinRadiusNeighbour");
	xNeighbour.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(602.0f, 0.0f, 0.0f));

	Zenith_GraphBlackboard xBB;
	Zenith_GraphNode_FindEntitiesInRadius xNode;
	xNode.m_strCenterVar = "";
	xNode.m_fRadius = 5.0f;
	xNode.m_strComponentType = "";
	xNode.m_strListVar = "found";
	xNode.m_strCountVar = "";			// the write that used to be skipped entirely

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// Self is excluded, so exactly the neighbour is in range - and the Count SLOT
	// agrees with the LIST, which is the whole point of latching it.
	ZENITH_ASSERT_NOT_NULL(xBB.TryGetList("found"));
	if (xBB.TryGetList("found") != nullptr)
	{
		ZENITH_ASSERT_EQ(xBB.TryGetList("found")->GetSize(), 1u);
	}
	ZENITH_ASSERT_EQ(EntityPin_SlotInt(
		xNode.GetOutputForTest(Zenith_GraphNode_FindEntitiesInRadius::uPIN_Count), "FindEntitiesInRadius.Count"), 1);
	// The LIST store is a separate map, so the VALUE store is still empty.
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "an empty m_strCountVar must write no blackboard variable");
}

//------------------------------------------------------------------------------
// THE CENSUS OBSERVABLE, and the guard-ordering contract.
//------------------------------------------------------------------------------

// Only a MIGRATED node can reach the transitional var-name fallback, and it logs ONE
// line per (instance, pin) however hot the chain is - which is what makes "zero
// FALLBACK lines in a SUITE boot log" C-1's precondition rather than a guess.
ZENITH_TEST(EntityPinRuntime, Fallback_CountsOncePerPin)
{
	Zenith_TempScene xTempScene("EntityPinFallbackScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("EntityPinFallbackSelf");

	Zenith_GraphBlackboard xBB;
	EntityPin_SeedVec3(xBB, "sv", Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f));

	Zenith_GraphNode_SetEntityScale xNode;
	xNode.m_xScale = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xNode.m_strScaleVar = "sv";		// var-BOUND and unwired: the fallback path
	xNode.m_strTargetVar = "";

	Zenith_GraphContext xCtx;
	xCtx.m_xSelf = xSelf;
	xCtx.m_pxBlackboard = &xBB;

	const u_int uScale = Zenith_GraphNode_SetEntityScale::uPIN_Scale;
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uScale), 0u);

	for (u_int u = 0; u < 3u; ++u)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		Zenith_Maths::Vector3 xScale;
		xSelf.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
		// The var wins over the const on EVERY fire, exactly as today's ternary did.
		ZENITH_ASSERT_NEAR_VEC3(xScale, Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f), 0.0001f);
	}
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uScale), 1u, "three reads must log ONE census line");

	// A var-BOUND name that names nothing takes the CONST, not zero - and warns about
	// neither, which is what the typed blackboard getter it replaced also did.
	Zenith_GraphNode_SetEntityScale xMissing;
	xMissing.m_xScale = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xMissing.m_strScaleVar = "__absent__";
	xMissing.m_strTargetVar = "";
	ZENITH_ASSERT_EQ(static_cast<int>(xMissing.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	Zenith_Maths::Vector3 xConstScale;
	xSelf.GetComponent<Zenith_TransformComponent>().GetScale(xConstScale);
	ZENITH_ASSERT_NEAR_VEC3(xConstScale, Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f), 0.0001f);
	ZENITH_ASSERT_EQ(xMissing.GetMismatchWarningCountForTest(uScale), 0u);
}

// ★ EVERY GetInput SITS AFTER EXACTLY THE EARLY-RETURN GUARDS ITS OLD BLACKBOARD
// READ SAT BEHIND. A node that FAILS on a guard must not read its inputs at all: no
// census line, and (in a graph) no pull of a producer that has no business running.
// Each pin below is var-BOUND, so a stray read would log exactly one line.
ZENITH_TEST(EntityPinRuntime, Fallback_GuardedFailureDoesNotReadInputs)
{
	Zenith_TempScene xTempScene("EntityPinGuardScene");

	Zenith_GraphBlackboard xBB;
	EntityPin_SeedVec3(xBB, "sv", Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f));
	EntityPin_SeedVec3(xBB, "dir", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	EntityPin_SeedFloat(xBB, "pv", 7.0f);
	EntityPin_SeedFloat(xBB, "yv", 7.0f);

	// An invalid target: m_strTargetVar names a variable that does not exist, so
	// ResolveTargetEntity comes back invalid and Execute returns before the read.
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_SetEntityScale xScaleNode;
	xScaleNode.m_xScale = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xScaleNode.m_strScaleVar = "sv";
	xScaleNode.m_strTargetVar = "__nobody__";
	ZENITH_ASSERT_EQ(static_cast<int>(xScaleNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xScaleNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetEntityScale::uPIN_Scale), 0u,
		"SetEntityScale read its Scale pin before the target guard");

	Zenith_GraphNode_RotateTowardDirection xRotateNode;
	xRotateNode.m_strDirectionVar = "dir";
	xRotateNode.m_strTargetVar = "__nobody__";
	ZENITH_ASSERT_EQ(static_cast<int>(xRotateNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xRotateNode.GetFallbackUseCountForTest(Zenith_GraphNode_RotateTowardDirection::uPIN_Direction), 0u,
		"RotateTowardDirection read its Direction pin before the target guard");

	// The camera leg, and it can only be asserted when no OTHER loaded scene leaves a
	// camera resolvable (the same conditional GraphComponent.EntityNodeFamilyRemainder-
	// Execution uses for its own no-camera leg). This test creates none.
	if (Zenith_GetMainCameraAcrossScenes() == nullptr)
	{
		Zenith_GraphNode_SetCameraPitchYaw xCameraNode;
		xCameraNode.m_fPitchDegrees = 9.0f;
		xCameraNode.m_strPitchVar = "pv";
		xCameraNode.m_fYawDegrees = 9.0f;
		xCameraNode.m_strYawVar = "yv";
		ZENITH_ASSERT_EQ(static_cast<int>(xCameraNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xCameraNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Pitch), 0u,
			"SetCameraPitchYaw read its Pitch pin before the no-camera guard");
		ZENITH_ASSERT_EQ(xCameraNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetCameraPitchYaw::uPIN_Yaw), 0u);
	}
	else
	{
		// Visible, never silent: a green run with this leg skipped is not evidence.
		Zenith_Log(LOG_CATEGORY_CORE, "[UnitTest] Fallback_GuardedFailureDoesNotReadInputs: camera leg SKIPPED - another loaded scene leaves a main camera resolvable");
	}
}

#endif // ZENITH_TESTING
