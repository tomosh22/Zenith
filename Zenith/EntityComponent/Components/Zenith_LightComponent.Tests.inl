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
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_xPositionOffset"));
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

#endif // ZENITH_TESTING
