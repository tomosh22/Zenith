//------------------------------------------------------------------------------
// Zenith_PropertySystem unit tests.
// Included at the bottom of Zenith_PropertySystem.cpp (the module-owns-its-tests
// pattern - see Zenith_Physics.cpp / Zenith_Blackboard.cpp).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	//--------------------------------------------------------------------------
	// Test fixtures. Namespace-scope (not function-local) because the macros
	// declare static inline data members, which local classes cannot have.
	//--------------------------------------------------------------------------

	struct PropTestAllTypes
	{
		ZENITH_PROPERTIES_BEGIN(PropTestAllTypes)
	public:
		ZENITH_PROPERTY(float, m_fSpeed, 2.0f)
		ZENITH_PROPERTY(int32_t, m_iHealth, 100)
		ZENITH_PROPERTY(u_int, m_uTeam, 3u)
		ZENITH_PROPERTY(bool, m_bEnabled, true)
		ZENITH_PROPERTY(Zenith_Maths::Vector2, m_xUV, Zenith_Maths::Vector2(0.25f, 0.75f))
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOffset, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f))
		ZENITH_PROPERTY_COLOUR(Zenith_Maths::Vector4, m_xTint, Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strLabel, "hello")
		ZENITH_PROPERTY(Zenith_GUID, m_xGuid, Zenith_GUID(0x1234ull))
	};

	struct PropTestRanged
	{
		ZENITH_PROPERTIES_BEGIN(PropTestRanged)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fAngle, 90.0f, 0.0f, 180.0f)
		ZENITH_PROPERTY_RANGED(int32_t, m_iLives, 3, 0, 10)
		ZENITH_PROPERTY_RANGED(u_int, m_uSlots, 4u, 1u, 8u)
	};

	// "Old build" fixture: has a field the new fixture dropped.
	struct PropTestMigrationOld
	{
		ZENITH_PROPERTIES_BEGIN(PropTestMigrationOld)
	public:
		ZENITH_PROPERTY(float, m_fShared, 1.0f)
		ZENITH_PROPERTY(float, m_fLegacyOnly, 5.0f)
	};

	// "New build" fixture: shared field survives, legacy is gone, new field untouched.
	struct PropTestMigrationNew
	{
		ZENITH_PROPERTIES_BEGIN(PropTestMigrationNew)
	public:
		ZENITH_PROPERTY(float, m_fShared, 0.0f)
		ZENITH_PROPERTY(std::string, m_strNewField, "default")
	};

	// Same property NAME as PropTestMigrationOld::m_fShared but a different TYPE -
	// the stored float must be dropped, never reinterpreted.
	struct PropTestTypeChanged
	{
		ZENITH_PROPERTIES_BEGIN(PropTestTypeChanged)
	public:
		ZENITH_PROPERTY(int32_t, m_fShared, 42)
	};

	//--------------------------------------------------------------------------
	// Change-hook capture
	//--------------------------------------------------------------------------
	struct PropChangeCapture
	{
		u_int m_uCount = 0;
		std::string m_strLastName;
	};

	void OnPropChangedCapture(void* pxUserData, const char* szPropertyName)
	{
		PropChangeCapture* pxCapture = static_cast<PropChangeCapture*>(pxUserData);
		pxCapture->m_uCount++;
		pxCapture->m_strLastName = szPropertyName ? szPropertyName : "";
	}

	constexpr u_int uPROP_TEST_SENTINEL = 0xDEADBEEFu;
}

ZENITH_TEST(PropertySystem, MacroRegistration)
{
	const Zenith_PropertyTable& xTable = PropTestAllTypes::GetPropertyTableStatic();
	ZENITH_ASSERT_EQ(xTable.GetPropertyCount(), 9u);

	// Declaration order preserved.
	ZENITH_ASSERT_STREQ(xTable.GetPropertyAt(0).m_szName, "m_fSpeed");
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(0).m_eType, PROPERTY_TYPE_FLOAT);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(1).m_eType, PROPERTY_TYPE_INT32);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(2).m_eType, PROPERTY_TYPE_UINT32);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(3).m_eType, PROPERTY_TYPE_BOOL);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(4).m_eType, PROPERTY_TYPE_VECTOR2);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(5).m_eType, PROPERTY_TYPE_VECTOR3);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(6).m_eType, PROPERTY_TYPE_VECTOR4);
	ZENITH_ASSERT_TRUE((xTable.GetPropertyAt(6).m_uFlags & PROPERTY_FLAG_COLOUR) != 0);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(7).m_eType, PROPERTY_TYPE_STRING);
	ZENITH_ASSERT_EQ(xTable.GetPropertyAt(8).m_eType, PROPERTY_TYPE_GUID);

	// FindProperty by name.
	ZENITH_ASSERT_NOT_NULL(xTable.FindProperty("m_strLabel"));
	ZENITH_ASSERT_NULL(xTable.FindProperty("m_fDoesNotExist"));

	// Defaults land on instances.
	PropTestAllTypes xInstance;
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_fSpeed, 2.0f, 0.0001f);
	ZENITH_ASSERT_EQ(xInstance.m_iHealth, 100);
	ZENITH_ASSERT_STREQ(xInstance.m_strLabel.c_str(), "hello");

	// Instances are independent through the descriptor thunks.
	PropTestAllTypes xOther;
	const Zenith_ReflectedProperty* pxSpeed = xTable.FindProperty("m_fSpeed");
	Zenith_PropertyValue xValue;
	xValue.SetFloat(99.0f);
	ZENITH_ASSERT_TRUE(Zenith_PropertySystem::SetPropertyValue(&xInstance, *pxSpeed, xValue));
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_fSpeed, 99.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xOther.m_fSpeed, 2.0f, 0.0001f);
}

ZENITH_TEST(PropertySystem, RoundTrip)
{
	PropTestAllTypes xSource;
	xSource.m_fSpeed = 12.5f;
	xSource.m_iHealth = -40;
	xSource.m_uTeam = 7u;
	xSource.m_bEnabled = false;
	xSource.m_xUV = Zenith_Maths::Vector2(0.1f, 0.9f);
	xSource.m_xOffset = Zenith_Maths::Vector3(-1.0f, 0.0f, 8.0f);
	xSource.m_xTint = Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.5f);
	xSource.m_strLabel = "round trip";
	xSource.m_xGuid = Zenith_GUID(0xABCDEF01ull);

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xSource, PropTestAllTypes::GetPropertyTableStatic(), xStream);
	xStream << uPROP_TEST_SENTINEL;

	PropTestAllTypes xDest;	// defaults differ from xSource everywhere
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xDest, PropTestAllTypes::GetPropertyTableStatic(), xStream);

	ZENITH_ASSERT_EQ_FLOAT(xDest.m_fSpeed, 12.5f, 0.0001f);
	ZENITH_ASSERT_EQ(xDest.m_iHealth, -40);
	ZENITH_ASSERT_EQ(xDest.m_uTeam, 7u);
	ZENITH_ASSERT_EQ(xDest.m_bEnabled, false);
	ZENITH_ASSERT_EQ_FLOAT(xDest.m_xUV.x, 0.1f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.m_xUV.y, 0.9f, 0.0001f);
	ZENITH_ASSERT_NEAR_VEC3(xDest.m_xOffset, Zenith_Maths::Vector3(-1.0f, 0.0f, 8.0f), 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.m_xTint.y, 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xDest.m_xTint.w, 0.5f, 0.0001f);
	ZENITH_ASSERT_STREQ(xDest.m_strLabel.c_str(), "round trip");
	ZENITH_ASSERT_TRUE(xDest.m_xGuid == Zenith_GUID(0xABCDEF01ull));

	// The read landed exactly on the blob end - the sentinel is intact.
	u_int uSentinel = 0;
	xStream >> uSentinel;
	ZENITH_ASSERT_EQ(uSentinel, uPROP_TEST_SENTINEL);
}

ZENITH_TEST(PropertySystem, NameMatchedMigration)
{
	PropTestMigrationOld xOld;
	xOld.m_fShared = 77.0f;
	xOld.m_fLegacyOnly = 123.0f;

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xOld, PropTestMigrationOld::GetPropertyTableStatic(), xStream);
	xStream << uPROP_TEST_SENTINEL;

	PropTestMigrationNew xNew;
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xNew, PropTestMigrationNew::GetPropertyTableStatic(), xStream);

	// Shared field migrated by name; dropped field skipped; new field untouched.
	ZENITH_ASSERT_EQ_FLOAT(xNew.m_fShared, 77.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xNew.m_strNewField.c_str(), "default");

	// Skipping the legacy entry didn't misalign the stream.
	u_int uSentinel = 0;
	xStream >> uSentinel;
	ZENITH_ASSERT_EQ(uSentinel, uPROP_TEST_SENTINEL);
}

ZENITH_TEST(PropertySystem, TypeChangeDropped)
{
	PropTestMigrationOld xOld;
	xOld.m_fShared = 3.5f;	// float in the old fixture

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xOld, PropTestMigrationOld::GetPropertyTableStatic(), xStream);
	xStream << uPROP_TEST_SENTINEL;

	// Same name, now declared int32 - the stored float must be DROPPED, not
	// bit-reinterpreted into the int.
	PropTestTypeChanged xChanged;
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xChanged, PropTestTypeChanged::GetPropertyTableStatic(), xStream);
	ZENITH_ASSERT_EQ(xChanged.m_fShared, 42);

	u_int uSentinel = 0;
	xStream >> uSentinel;
	ZENITH_ASSERT_EQ(uSentinel, uPROP_TEST_SENTINEL);
}

ZENITH_TEST(PropertySystem, RangeClamping)
{
	PropTestRanged xInstance;
	const Zenith_PropertyTable& xTable = PropTestRanged::GetPropertyTableStatic();

	Zenith_PropertyValue xValue;
	xValue.SetFloat(999.0f);
	Zenith_PropertySystem::SetPropertyValue(&xInstance, *xTable.FindProperty("m_fAngle"), xValue);
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_fAngle, 180.0f, 0.0001f);

	xValue.SetFloat(-50.0f);
	Zenith_PropertySystem::SetPropertyValue(&xInstance, *xTable.FindProperty("m_fAngle"), xValue);
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_fAngle, 0.0f, 0.0001f);

	xValue.SetInt32(-5);
	Zenith_PropertySystem::SetPropertyValue(&xInstance, *xTable.FindProperty("m_iLives"), xValue);
	ZENITH_ASSERT_EQ(xInstance.m_iLives, 0);

	xValue.SetUInt32(100u);
	Zenith_PropertySystem::SetPropertyValue(&xInstance, *xTable.FindProperty("m_uSlots"), xValue);
	ZENITH_ASSERT_EQ(xInstance.m_uSlots, 8u);

	// Clamping also applies on the deserialization path: hand-craft a source
	// instance with out-of-range values written through raw member access.
	PropTestRanged xRaw;
	xRaw.m_fAngle = 500.0f;	// bypasses the descriptor (direct member write)
	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xRaw, xTable, xStream);

	PropTestRanged xLoaded;
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xLoaded, xTable, xStream);
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.m_fAngle, 180.0f, 0.0001f);
}

ZENITH_TEST(PropertySystem, VersionSkewSkips)
{
	PropTestAllTypes xSource;
	xSource.m_fSpeed = 55.0f;

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xSource, PropTestAllTypes::GetPropertyTableStatic(), xStream);
	xStream << uPROP_TEST_SENTINEL;
	const uint64_t ulEndCursor = xStream.GetCursor();

	// Tamper the version field (first u_int of the blob).
	xStream.SetCursor(0);
	const u_int uBogusVersion = 999u;
	xStream << uBogusVersion;
	xStream.SetCursor(0);

	PropTestAllTypes xDest;
	Zenith_PropertySystem::ReadProperties(&xDest, PropTestAllTypes::GetPropertyTableStatic(), xStream);

	// Nothing applied - defaults intact.
	ZENITH_ASSERT_EQ_FLOAT(xDest.m_fSpeed, 2.0f, 0.0001f);

	// The whole blob was skipped cleanly - cursor sits exactly at the sentinel.
	u_int uSentinel = 0;
	xStream >> uSentinel;
	ZENITH_ASSERT_EQ(uSentinel, uPROP_TEST_SENTINEL);
	ZENITH_ASSERT_EQ(xStream.GetCursor(), ulEndCursor);
}

ZENITH_TEST(PropertySystem, ChangedHookFires)
{
	PropTestAllTypes xSource;	// all defaults

	Zenith_DataStream xStream;
	Zenith_PropertySystem::WriteProperties(&xSource, PropTestAllTypes::GetPropertyTableStatic(), xStream);

	// Reading identical values onto an identical instance fires NO hooks.
	PropTestAllTypes xSame;
	PropChangeCapture xCapture;
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xSame, PropTestAllTypes::GetPropertyTableStatic(), xStream,
		&OnPropChangedCapture, &xCapture);
	ZENITH_ASSERT_EQ(xCapture.m_uCount, 0u);

	// One differing value fires exactly one hook with the right name.
	PropTestAllTypes xDiffers;
	xDiffers.m_fSpeed = 1234.0f;
	xStream.SetCursor(0);
	Zenith_PropertySystem::ReadProperties(&xDiffers, PropTestAllTypes::GetPropertyTableStatic(), xStream,
		&OnPropChangedCapture, &xCapture);
	ZENITH_ASSERT_EQ(xCapture.m_uCount, 1u);
	ZENITH_ASSERT_STREQ(xCapture.m_strLastName.c_str(), "m_fSpeed");
	ZENITH_ASSERT_EQ_FLOAT(xDiffers.m_fSpeed, 2.0f, 0.0001f);	// value applied from stream
}

ZENITH_TEST(PropertySystem, PropertyValueFullSerialization)
{
	// The full (type tag + payload) form used by blackboards.
	Zenith_DataStream xStream;
	Zenith_PropertyValue xString;
	xString.SetString("blackboard value");
	xStream << xString;	// non-trivially-copyable -> WriteToDataStream
	Zenith_PropertyValue xVec;
	xVec.SetVector3(Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f));
	xStream << xVec;

	xStream.SetCursor(0);
	Zenith_PropertyValue xReadString;
	xStream >> xReadString;
	Zenith_PropertyValue xReadVec;
	xStream >> xReadVec;

	ZENITH_ASSERT_EQ(xReadString.GetType(), PROPERTY_TYPE_STRING);
	ZENITH_ASSERT_STREQ(xReadString.GetString().c_str(), "blackboard value");
	ZENITH_ASSERT_TRUE(xReadVec.Equals(xVec));
	ZENITH_ASSERT_FALSE(xReadVec.Equals(xReadString));
}

ZENITH_TEST(PropertySystem, DisplayName)
{
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("m_fOpenSpeed"), "OpenSpeed");
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("m_xColour"), "Colour");
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("m_axColourAttachments"), "ColourAttachments");
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("m_strLabel"), "Label");
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("speed"), "speed");	// no-pattern fallback
	ZENITH_ASSERT_STREQ(Zenith_PropertySystem::GetDisplayName("m_uTeam"), "Team");
}

#endif // ZENITH_TESTING
