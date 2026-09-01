//------------------------------------------------------------------------------
// Zenith_LightComponent property-reflection integration tests (Phase 0 of the
// Behaviour Graphs program): prove ZENITH_PROPERTY works on real engine
// components, not just test fixtures. Included at the bottom of
// Zenith_LightComponent.cpp.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "UnitTests/Zenith_TempScene.h"

ZENITH_TEST(PropertySystem, LightComponentTableShape)
{
	const Zenith_PropertyTable& xTable = Zenith_LightComponent::GetPropertyTableStatic();
	ZENITH_ASSERT_EQ(xTable.GetPropertyCount(), 6u);

	const Zenith_ReflectedProperty* pxColor = xTable.FindProperty("m_xColor");
	ZENITH_ASSERT_NOT_NULL(pxColor);
	ZENITH_ASSERT_EQ(pxColor->m_eType, PROPERTY_TYPE_VECTOR3);
	ZENITH_ASSERT_TRUE((pxColor->m_uFlags & PROPERTY_FLAG_COLOUR) != 0);

	const Zenith_ReflectedProperty* pxIntensity = xTable.FindProperty("m_fIntensity");
	ZENITH_ASSERT_NOT_NULL(pxIntensity);
	ZENITH_ASSERT_TRUE(pxIntensity->m_bHasRange);
	ZENITH_ASSERT_EQ_FLOAT(pxIntensity->m_fMax, 10000000.0f, 0.5f);

	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_fRange"));
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_bCastShadows"));
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_xLocalPositionOffset"));
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_xDirectionOffset"));
}

ZENITH_TEST(PropertySystem, LightComponentLiveRoundTrip)
{
	Zenith_TempScene xTempScene("TestLightPropertyScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	Zenith_Entity xSourceEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "PropSourceLight");
	Zenith_LightComponent& xSource = xSourceEntity.AddComponent<Zenith_LightComponent>();
	xSource.SetColor(Zenith_Maths::Vector3(0.25f, 0.5f, 0.75f));
	xSource.SetIntensity(5000.0f);
	xSource.SetRange(42.0f);

	Zenith_Entity xDestEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "PropDestLight");
	Zenith_LightComponent& xDest = xDestEntity.AddComponent<Zenith_LightComponent>();

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xSource, Zenith_LightComponent::GetPropertyTableStatic(), xStream);
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xDest, Zenith_LightComponent::GetPropertyTableStatic(), xStream);

	ZENITH_ASSERT_NEAR_VEC3(xDest.GetColor(), Zenith_Maths::Vector3(0.25f, 0.5f, 0.75f), 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetIntensity(), 5000.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetRange(), 42.0f, 0.0001f);

	// Descriptor-level clamping mirrors the hand-written setter clamps.
	Zenith_PropertyValue xValue;
	xValue.SetFloat(20000000.0f);
	Zenith_PropertySystem::SetPropertyValue(&xDest, *Zenith_LightComponent::GetPropertyTableStatic().FindProperty("m_fIntensity"), xValue);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetIntensity(), 10000000.0f, 0.5f);
}

ZENITH_TEST(PropertySystem, CameraComponentProperties)
{
	const Zenith_PropertyTable& xTable = Zenith_CameraComponent::GetPropertyTableStatic();
	ZENITH_ASSERT_EQ(xTable.GetPropertyCount(), 3u);
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_fNear"));
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_fFar"));
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_fFOV"));

	// Camera is default-constructible - round-trip across two plain instances.
	Zenith_CameraComponent xSource;
	xSource.SetFOV(75.0f);
	xSource.SetNearPlane(0.5f);
	xSource.SetFarPlane(2500.0f);

	Zenith_CameraComponent xDest;
	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xSource, xTable, xStream);
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xDest, xTable, xStream);

	ZENITH_ASSERT_EQ_FLOAT(xDest.GetFOV(), 75.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetNearPlane(), 0.5f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.GetFarPlane(), 2500.0f, 0.0001f);
}


//------------------------------------------------------------------------------
// The LOCAL position offset. These pin the property the lamp post needs and the
// world-space behaviour it replaced would have failed.
//------------------------------------------------------------------------------

// With no offset in use, the light is exactly the entity's position -- so the
// clauses below are measuring the OFFSET and not some pre-existing displacement.
ZENITH_TEST(LightComponent, WorldPositionIsTheTransformWhenNoOffsetIsUsed)
{
	Zenith_TempScene xTempScene("TestLightNoOffset");
	Zenith_Entity xEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightNoOffset");
	Zenith_LightComponent& xLight = xEntity.AddComponent<Zenith_LightComponent>();

	Zenith_TransformComponent& xTransform =
		xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(7.0f));
	xLight.SetLocalPositionOffset(Zenith_Maths::Vector3(100.0f, 100.0f, 100.0f));

	const Zenith_Maths::Vector3 xWorld = xLight.GetWorldPosition();
	ZENITH_ASSERT_NEAR_VEC3(xWorld, Zenith_Maths::Vector3(3.0f, 4.0f, 5.0f), 1.0e-4f,
		"an unused offset must not move the light, whatever it holds");
}

// ★★ THE SCALE CLAUSE, and it is the one a world-space offset fails. A prop is
// scaled by ZM_ComputePropFit to reach its roster size -- 3.006 for Zenithmon's
// lamp post -- so an offset measured on the MESH has to be scaled with it or the
// bulb leaves the lantern the moment the asset is re-exported at another size.
ZENITH_TEST(LightComponent, LocalOffsetIsScaledByTheParentTransform)
{
	Zenith_TempScene xTempScene("TestLightOffsetScale");
	Zenith_Entity xEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightOffsetScale");
	Zenith_LightComponent& xLight = xEntity.AddComponent<Zenith_LightComponent>();

	Zenith_TransformComponent& xTransform =
		xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(3.0f));

	xLight.SetUsePositionOffset(true);
	xLight.SetLocalPositionOffset(Zenith_Maths::Vector3(0.0f, 0.4f, 0.0f));

	// 0.4 model units up a model scaled 3x is 1.2 world metres up.
	ZENITH_ASSERT_NEAR_VEC3(xLight.GetWorldPosition(),
		Zenith_Maths::Vector3(10.0f, 21.2f, 30.0f), 1.0e-4f,
		"the offset must be scaled by the parent -- a world-space add would put "
		"the light at y = 20.4 and the bulb below its own lantern");
}

// ★★ AND THE ROTATION CLAUSE. Every interior prop carries an authored yaw and the
// outdoor table can too; a world-space offset leaves the bulb behind when the
// post turns.
ZENITH_TEST(LightComponent, LocalOffsetIsRotatedByTheParentTransform)
{
	Zenith_TempScene xTempScene("TestLightOffsetRotation");
	Zenith_Entity xEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightOffsetRotation");
	Zenith_LightComponent& xLight = xEntity.AddComponent<Zenith_LightComponent>();

	Zenith_TransformComponent& xTransform =
		xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f));
	// A quarter turn about +Y, as a frozen literal (ZM-D-183: angleAxis differs by
	// 1-2 ULP between Debug and Release codegen).
	xTransform.SetRotation(Zenith_Maths::Quat(0.70710678f, 0.0f, 0.70710678f, 0.0f));

	xLight.SetUsePositionOffset(true);
	// +X in the model. A quarter turn about +Y maps +X to -Z.
	xLight.SetLocalPositionOffset(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));

	ZENITH_ASSERT_NEAR_VEC3(xLight.GetWorldPosition(),
		Zenith_Maths::Vector3(0.0f, 0.0f, -2.0f), 1.0e-4f,
		"the offset must be rotated by the parent -- a world-space add would leave "
		"the light at +X while the model faces -Z");
}

// Both at once, in the order the transform itself composes them: scale, rotate,
// translate. Getting the order wrong is invisible whenever either is the
// identity, which is every case above.
ZENITH_TEST(LightComponent, LocalOffsetComposesScaleThenRotationThenTranslation)
{
	Zenith_TempScene xTempScene("TestLightOffsetCompose");
	Zenith_Entity xEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightOffsetCompose");
	Zenith_LightComponent& xLight = xEntity.AddComponent<Zenith_LightComponent>();

	Zenith_TransformComponent& xTransform =
		xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));
	xTransform.SetRotation(Zenith_Maths::Quat(0.70710678f, 0.0f, 0.70710678f, 0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(4.0f));

	xLight.SetUsePositionOffset(true);
	xLight.SetLocalPositionOffset(Zenith_Maths::Vector3(0.5f, 0.25f, 0.0f));

	// scale: (2.0, 1.0, 0.0); rotate a quarter turn about +Y: (0.0, 1.0, -2.0);
	// translate: (100, 201, 298).
	ZENITH_ASSERT_NEAR_VEC3(xLight.GetWorldPosition(),
		Zenith_Maths::Vector3(100.0f, 201.0f, 298.0f), 1.0e-4f,
		"scale must be applied before rotation, and both before the translation");
}

// The offset survives a save/load unchanged -- it reaches committed scene bytes,
// so a round trip that dropped it would put every authored bulb back at its
// entity origin.
ZENITH_TEST(LightComponent, LocalOffsetRoundTripsThroughTheStream)
{
	Zenith_TempScene xTempScene("TestLightOffsetStream");
	Zenith_Entity xSourceEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightOffsetStreamSrc");
	Zenith_LightComponent& xSource =
		xSourceEntity.AddComponent<Zenith_LightComponent>();
	xSource.SetUsePositionOffset(true);
	xSource.SetLocalPositionOffset(Zenith_Maths::Vector3(0.125f, 2.5f, -0.75f));

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	Zenith_Entity xDestEntity =
		g_xEngine.Scenes().CreateEntity(xTempScene.Data(), "LightOffsetStreamDst");
	Zenith_LightComponent& xDest = xDestEntity.AddComponent<Zenith_LightComponent>();
	xDest.ReadFromDataStream(xStream);

	ZENITH_ASSERT_TRUE(xDest.GetUsePositionOffset(),
		"the use-offset flag must survive the stream");
	ZENITH_ASSERT_NEAR_VEC3(xDest.GetLocalPositionOffset(),
		Zenith_Maths::Vector3(0.125f, 2.5f, -0.75f), 1.0e-6f,
		"the offset itself must survive the stream");
}

#endif // ZENITH_TESTING
