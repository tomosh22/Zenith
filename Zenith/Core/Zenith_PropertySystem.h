#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include <string>
#include <cstring>

class Zenith_DataStream;

//------------------------------------------------------------------------------
// Zenith_PropertySystem - declare-once reflected properties.
//
// A class opts in by placing ZENITH_PROPERTIES_BEGIN(ClassName) in its body and
// declaring tunable fields via ZENITH_PROPERTY / ZENITH_PROPERTY_RANGED /
// ZENITH_PROPERTY_COLOUR. Each declaration registers a Zenith_ReflectedProperty
// (name + type + fn-ptr getter/setter) into the class's static property table.
// The table then drives, with zero further per-field code:
//   - name-matched, length-framed serialization (add/remove/reorder-safe),
//   - the auto ImGui properties panel (ZENITH_TOOLS),
//   - live tuning re-apply with an OnPropertyChanged hook (see
//     AssetHandling/Zenith_PropertyTuning.h for the FileWatcher binding).
//
// This is the foundation layer of the Behaviour Graphs scripting system: graph
// blackboard variables and node parameters are Zenith_PropertyValue, and game
// ECS components use the same macros for their tunables.
//
// NOTE: distinct from the prefab-variant override Zenith_PropertyDescriptor in
// ZenithECS/Zenith_ComponentMeta.h (a minimal name -> stream-setter). Unifying
// the prefab path onto this system is planned once graph components land.
//
// Engine conventions honoured: no std::function (plain function pointers), no
// exceptions, Zenith containers.
//------------------------------------------------------------------------------

enum Zenith_PropertyType : u_int8
{
	PROPERTY_TYPE_FLOAT = 0,
	PROPERTY_TYPE_INT32,
	PROPERTY_TYPE_UINT32,
	PROPERTY_TYPE_BOOL,
	PROPERTY_TYPE_VECTOR2,
	PROPERTY_TYPE_VECTOR3,
	PROPERTY_TYPE_VECTOR4,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_ENTITY_ID,	// packed Zenith_EntityID (64-bit); Core stays ECS-agnostic, ECS-side code packs/unpacks
	PROPERTY_TYPE_GUID,
	PROPERTY_TYPE_COUNT
};

// Display/behaviour hints on a reflected property.
enum
{
	PROPERTY_FLAG_NONE      = 0,
	PROPERTY_FLAG_COLOUR    = 1 << 0,	// VECTOR3/VECTOR4 rendered as a colour picker
	PROPERTY_FLAG_READ_ONLY = 1 << 1,	// shown in the panel but not editable
};

//------------------------------------------------------------------------------
// Zenith_PropertyValue - tagged variant holding one property value.
//------------------------------------------------------------------------------
struct Zenith_PropertyValue
{
	Zenith_PropertyValue()
	{
		m_afVector[0] = 0.0f;
		m_afVector[1] = 0.0f;
		m_afVector[2] = 0.0f;
		m_afVector[3] = 0.0f;
	}

	// Typed setters - each stamps the type tag.
	void SetFloat(float fValue)                              { m_eType = PROPERTY_TYPE_FLOAT;     m_fFloat = fValue; }
	void SetInt32(int32_t iValue)                            { m_eType = PROPERTY_TYPE_INT32;     m_iInt32 = iValue; }
	void SetUInt32(u_int uValue)                             { m_eType = PROPERTY_TYPE_UINT32;    m_uUInt32 = uValue; }
	void SetBool(bool bValue)                                { m_eType = PROPERTY_TYPE_BOOL;      m_bBool = bValue; }
	void SetVector2(const Zenith_Maths::Vector2& xValue)     { m_eType = PROPERTY_TYPE_VECTOR2;   m_afVector[0] = xValue.x; m_afVector[1] = xValue.y; }
	void SetVector3(const Zenith_Maths::Vector3& xValue)     { m_eType = PROPERTY_TYPE_VECTOR3;   m_afVector[0] = xValue.x; m_afVector[1] = xValue.y; m_afVector[2] = xValue.z; }
	void SetVector4(const Zenith_Maths::Vector4& xValue)     { m_eType = PROPERTY_TYPE_VECTOR4;   m_afVector[0] = xValue.x; m_afVector[1] = xValue.y; m_afVector[2] = xValue.z; m_afVector[3] = xValue.w; }
	void SetString(const std::string& strValue)              { m_eType = PROPERTY_TYPE_STRING;    m_strString = strValue; }
	void SetPackedEntityID(u_int64 ulPacked)                 { m_eType = PROPERTY_TYPE_ENTITY_ID; m_ulPacked = ulPacked; }
	void SetGUID(Zenith_GUID xGUID)                          { m_eType = PROPERTY_TYPE_GUID;      m_ulPacked = xGUID.m_uGUID; }

	// Typed getters - assert the tag matches.
	float GetFloat() const                 { Zenith_Assert(m_eType == PROPERTY_TYPE_FLOAT, "PropertyValue: type mismatch (have %u, want FLOAT)", m_eType); return m_fFloat; }
	int32_t GetInt32() const               { Zenith_Assert(m_eType == PROPERTY_TYPE_INT32, "PropertyValue: type mismatch (have %u, want INT32)", m_eType); return m_iInt32; }
	u_int GetUInt32() const                { Zenith_Assert(m_eType == PROPERTY_TYPE_UINT32, "PropertyValue: type mismatch (have %u, want UINT32)", m_eType); return m_uUInt32; }
	bool GetBool() const                   { Zenith_Assert(m_eType == PROPERTY_TYPE_BOOL, "PropertyValue: type mismatch (have %u, want BOOL)", m_eType); return m_bBool; }
	Zenith_Maths::Vector2 GetVector2() const { Zenith_Assert(m_eType == PROPERTY_TYPE_VECTOR2, "PropertyValue: type mismatch (have %u, want VECTOR2)", m_eType); return Zenith_Maths::Vector2(m_afVector[0], m_afVector[1]); }
	Zenith_Maths::Vector3 GetVector3() const { Zenith_Assert(m_eType == PROPERTY_TYPE_VECTOR3, "PropertyValue: type mismatch (have %u, want VECTOR3)", m_eType); return Zenith_Maths::Vector3(m_afVector[0], m_afVector[1], m_afVector[2]); }
	Zenith_Maths::Vector4 GetVector4() const { Zenith_Assert(m_eType == PROPERTY_TYPE_VECTOR4, "PropertyValue: type mismatch (have %u, want VECTOR4)", m_eType); return Zenith_Maths::Vector4(m_afVector[0], m_afVector[1], m_afVector[2], m_afVector[3]); }
	const std::string& GetString() const   { Zenith_Assert(m_eType == PROPERTY_TYPE_STRING, "PropertyValue: type mismatch (have %u, want STRING)", m_eType); return m_strString; }
	u_int64 GetPackedEntityID() const      { Zenith_Assert(m_eType == PROPERTY_TYPE_ENTITY_ID, "PropertyValue: type mismatch (have %u, want ENTITY_ID)", m_eType); return m_ulPacked; }
	Zenith_GUID GetGUID() const            { Zenith_Assert(m_eType == PROPERTY_TYPE_GUID, "PropertyValue: type mismatch (have %u, want GUID)", m_eType); return Zenith_GUID(m_ulPacked); }

	Zenith_PropertyType GetType() const { return m_eType; }

	// Exact type + payload comparison (used for change detection).
	bool Equals(const Zenith_PropertyValue& xOther) const;

	// Payload-only serialization - the surrounding format owns the type tag and
	// length framing (see Zenith_PropertySystem::WriteProperties). The read form
	// takes the externally-stored type tag and stamps it.
	void WritePayloadToDataStream(Zenith_DataStream& xStream) const;
	void ReadPayloadFromDataStream(Zenith_DataStream& xStream, Zenith_PropertyType eType);

	// Full serialization (type tag + payload) - for blackboards and direct use.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_PropertyType m_eType = PROPERTY_TYPE_FLOAT;
	union
	{
		float    m_fFloat;
		int32_t  m_iInt32;
		u_int    m_uUInt32;
		bool     m_bBool;
		float    m_afVector[4];
		u_int64  m_ulPacked;	// ENTITY_ID (packed) and GUID payloads
	};
	std::string m_strString;
};

//------------------------------------------------------------------------------
// Zenith_ReflectedProperty - one declared property on a class.
//------------------------------------------------------------------------------

// Engine convention: no std::function - plain function pointers over a void*
// instance. The owning class's macro expansion supplies type-safe thunks.
typedef void (*Zenith_ReflectedPropertyGetFn)(const void* pxInstance, Zenith_PropertyValue& xOut);
typedef void (*Zenith_ReflectedPropertySetFn)(void* pxInstance, const Zenith_PropertyValue& xIn);

struct Zenith_ReflectedProperty
{
	const char* m_szName = nullptr;		// the field name as declared, e.g. "m_fOpenSpeed"
	Zenith_PropertyType m_eType = PROPERTY_TYPE_FLOAT;
	Zenith_ReflectedPropertyGetFn m_pfnGet = nullptr;
	Zenith_ReflectedPropertySetFn m_pfnSet = nullptr;
	bool m_bHasRange = false;			// FLOAT/INT32/UINT32 only; clamped on every set
	float m_fMin = 0.0f;
	float m_fMax = 0.0f;
	u_int m_uFlags = PROPERTY_FLAG_NONE;
};

//------------------------------------------------------------------------------
// Zenith_PropertyTable - the per-class descriptor list.
//------------------------------------------------------------------------------
class Zenith_PropertyTable
{
public:
	void AddProperty(const Zenith_ReflectedProperty& xProperty)
	{
		Zenith_Assert(FindProperty(xProperty.m_szName) == nullptr,
			"Zenith_PropertyTable: duplicate property '%s'", xProperty.m_szName ? xProperty.m_szName : "(null)");
		m_axProperties.PushBack(xProperty);
	}

	u_int GetPropertyCount() const { return m_axProperties.GetSize(); }

	const Zenith_ReflectedProperty& GetPropertyAt(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_axProperties.GetSize(), "Zenith_PropertyTable: index %u out of range", uIndex);
		return m_axProperties.Get(uIndex);
	}

	const Zenith_ReflectedProperty* FindProperty(const char* szName) const
	{
		if (!szName)
		{
			return nullptr;
		}
		for (u_int u = 0; u < m_axProperties.GetSize(); ++u)
		{
			const Zenith_ReflectedProperty& xProperty = m_axProperties.Get(u);
			if (xProperty.m_szName && std::strcmp(xProperty.m_szName, szName) == 0)
			{
				return &xProperty;
			}
		}
		return nullptr;
	}

private:
	Zenith_Vector<Zenith_ReflectedProperty> m_axProperties;
};

//------------------------------------------------------------------------------
// Type traits - map a C++ field type onto a Zenith_PropertyType + store/load.
// Extend by specialising Zenith_PropertyTraits<T> next to the type that needs
// it (e.g. ECS-side code specialises for Zenith_EntityID, packing via
// GetPacked/FromPacked, without Core ever naming the ECS type).
//------------------------------------------------------------------------------
template<typename T>
struct Zenith_PropertyTraits;	// undefined primary: unsupported property type -> compile error

template<> struct Zenith_PropertyTraits<float>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_FLOAT;
	static void Store(Zenith_PropertyValue& xOut, const float& fValue) { xOut.SetFloat(fValue); }
	static void Load(const Zenith_PropertyValue& xIn, float& fValue) { fValue = xIn.GetFloat(); }
};

template<> struct Zenith_PropertyTraits<int32_t>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_INT32;
	static void Store(Zenith_PropertyValue& xOut, const int32_t& iValue) { xOut.SetInt32(iValue); }
	static void Load(const Zenith_PropertyValue& xIn, int32_t& iValue) { iValue = xIn.GetInt32(); }
};

template<> struct Zenith_PropertyTraits<u_int>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_UINT32;
	static void Store(Zenith_PropertyValue& xOut, const u_int& uValue) { xOut.SetUInt32(uValue); }
	static void Load(const Zenith_PropertyValue& xIn, u_int& uValue) { uValue = xIn.GetUInt32(); }
};

template<> struct Zenith_PropertyTraits<bool>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_BOOL;
	static void Store(Zenith_PropertyValue& xOut, const bool& bValue) { xOut.SetBool(bValue); }
	static void Load(const Zenith_PropertyValue& xIn, bool& bValue) { bValue = xIn.GetBool(); }
};

template<> struct Zenith_PropertyTraits<Zenith_Maths::Vector2>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_VECTOR2;
	static void Store(Zenith_PropertyValue& xOut, const Zenith_Maths::Vector2& xValue) { xOut.SetVector2(xValue); }
	static void Load(const Zenith_PropertyValue& xIn, Zenith_Maths::Vector2& xValue) { xValue = xIn.GetVector2(); }
};

template<> struct Zenith_PropertyTraits<Zenith_Maths::Vector3>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_VECTOR3;
	static void Store(Zenith_PropertyValue& xOut, const Zenith_Maths::Vector3& xValue) { xOut.SetVector3(xValue); }
	static void Load(const Zenith_PropertyValue& xIn, Zenith_Maths::Vector3& xValue) { xValue = xIn.GetVector3(); }
};

template<> struct Zenith_PropertyTraits<Zenith_Maths::Vector4>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_VECTOR4;
	static void Store(Zenith_PropertyValue& xOut, const Zenith_Maths::Vector4& xValue) { xOut.SetVector4(xValue); }
	static void Load(const Zenith_PropertyValue& xIn, Zenith_Maths::Vector4& xValue) { xValue = xIn.GetVector4(); }
};

template<> struct Zenith_PropertyTraits<std::string>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_STRING;
	static void Store(Zenith_PropertyValue& xOut, const std::string& strValue) { xOut.SetString(strValue); }
	static void Load(const Zenith_PropertyValue& xIn, std::string& strValue) { strValue = xIn.GetString(); }
};

template<> struct Zenith_PropertyTraits<Zenith_GUID>
{
	static constexpr Zenith_PropertyType eTYPE = PROPERTY_TYPE_GUID;
	static void Store(Zenith_PropertyValue& xOut, const Zenith_GUID& xValue) { xOut.SetGUID(xValue); }
	static void Load(const Zenith_PropertyValue& xIn, Zenith_GUID& xValue) { xValue = xIn.GetGUID(); }
};

//------------------------------------------------------------------------------
// System operations - serialization, change application, editor panel.
//------------------------------------------------------------------------------
namespace Zenith_PropertySystem
{
	// Fired once per property whose value actually changed during ReadProperties /
	// panel edits / tuning re-apply. pxUserData is the caller's opaque context.
	typedef void (*PropertyChangedFn)(void* pxUserData, const char* szPropertyName);

	// Set through the descriptor with range clamping + change detection.
	// Returns true if the stored value changed.
	bool SetPropertyValue(void* pxInstance, const Zenith_ReflectedProperty& xProperty, const Zenith_PropertyValue& xValue);

	// Name-matched, length-framed blob:
	//   u_int version | u_int blobBytes | u_int count |
	//   per property: string name | u_int8 type | u_int payloadBytes | payload
	// Read skips unknown names, type mismatches, and unsupported versions without
	// misaligning the surrounding stream; matched values go through
	// SetPropertyValue (clamped), firing pfnOnChanged per changed property.
	void WriteProperties(const void* pxInstance, const Zenith_PropertyTable& xTable, Zenith_DataStream& xStream);
	void ReadProperties(void* pxInstance, const Zenith_PropertyTable& xTable, Zenith_DataStream& xStream,
		PropertyChangedFn pfnOnChanged = nullptr, void* pxUserData = nullptr);

#ifdef ZENITH_TOOLS
	// Per-row screen-rect callback: fired after each property's widget with its
	// ImGui item rect. Lets editor panels (and their automated tests) locate a
	// specific property's widget on screen.
	typedef void (*PropertyRowRectFn)(void* pxUserData, const char* szPropertyName,
		float fMinX, float fMinY, float fMaxX, float fMaxY);

	// Auto ImGui panel: one widget per property (slider when ranged, colour
	// picker under PROPERTY_FLAG_COLOUR, read-only text under
	// PROPERTY_FLAG_READ_ONLY). Returns true if any property changed.
	bool RenderPropertyPanel(void* pxInstance, const Zenith_PropertyTable& xTable,
		PropertyChangedFn pfnOnChanged = nullptr, void* pxUserData = nullptr,
		PropertyRowRectFn pfnRowRect = nullptr, void* pxRowRectUserData = nullptr);
#endif

	// "m_fOpenSpeed" -> "OpenSpeed": strips the scope + type prefix for designer-
	// facing display. Returns szName unchanged when it doesn't match the pattern.
	const char* GetDisplayName(const char* szName);
}

//------------------------------------------------------------------------------
// Declaration macros.
//
// Usage (inside a class body):
//   ZENITH_PROPERTIES_BEGIN(DPDoor_Behaviour)
//   ZENITH_PROPERTY(float, m_fOpenSpeed, 2.0f)
//   ZENITH_PROPERTY_RANGED(float, m_fOpenAngle, 90.0f, 0.0f, 180.0f)
//   ZENITH_PROPERTY_COLOUR(Zenith_Maths::Vector3, m_xTint, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f))
//
// ZENITH_PROPERTIES_BEGIN leaves the class in a private: section; re-specify
// access afterwards if needed. Field defaults may contain commas
// (Vector3(0, 1, 0)) - the trailing variadic swallows them.
//
// Registration runs at static init (the same pattern as the component-meta
// macros); the per-class table is construct-on-first-use so initialization
// order is safe. The usual MSVC dead-strip caveat applies: an entirely
// unreferenced TU never registers.
//------------------------------------------------------------------------------

#define ZENITH_PROPERTIES_BEGIN(ClassName) \
public: \
	using ZenithPropertyOwnerType = ClassName; \
	static Zenith_PropertyTable& GetPropertyTableStatic() \
	{ \
		static Zenith_PropertyTable ls_xTable; \
		return ls_xTable; \
	} \
	const Zenith_PropertyTable& GetPropertyTable() const { return GetPropertyTableStatic(); } \
private:

// Internal: shared registration machinery. The registration body lives in a
// static member FUNCTION (a function body is a complete-class context, so the
// accessor lambdas may dereference members of the enclosing class - a static
// data member initializer is NOT such a context and MSVC correctly rejects
// member access there while the class is still incomplete). The static inline
// bool then just calls it, running registration once at static init.
#define ZENITH_PROPERTY_REGISTER_BODY(Type, Name, bHasRangeV, fMinV, fMaxV, uFlagsV) \
	static bool ZenithPropRegister_##Name() \
	{ \
		Zenith_ReflectedProperty xProp; \
		xProp.m_szName = #Name; \
		xProp.m_eType = Zenith_PropertyTraits<Type>::eTYPE; \
		xProp.m_pfnGet = +[](const void* pxInstance, Zenith_PropertyValue& xOut) \
		{ \
			Zenith_PropertyTraits<Type>::Store(xOut, static_cast<const ZenithPropertyOwnerType*>(pxInstance)->Name); \
		}; \
		xProp.m_pfnSet = +[](void* pxInstance, const Zenith_PropertyValue& xIn) \
		{ \
			Zenith_PropertyTraits<Type>::Load(xIn, static_cast<ZenithPropertyOwnerType*>(pxInstance)->Name); \
		}; \
		xProp.m_bHasRange = bHasRangeV; \
		xProp.m_fMin = fMinV; \
		xProp.m_fMax = fMaxV; \
		xProp.m_uFlags = uFlagsV; \
		GetPropertyTableStatic().AddProperty(xProp); \
		return true; \
	} \
	static inline const bool s_bZenithPropReg_##Name = ZenithPropRegister_##Name();

#define ZENITH_PROPERTY(Type, Name, ...) \
	Type Name = __VA_ARGS__; \
	ZENITH_PROPERTY_REGISTER_BODY(Type, Name, false, 0.0f, 0.0f, PROPERTY_FLAG_NONE)

#define ZENITH_PROPERTY_RANGED(Type, Name, Default, Min, Max) \
	Type Name = Default; \
	ZENITH_PROPERTY_REGISTER_BODY(Type, Name, true, static_cast<float>(Min), static_cast<float>(Max), PROPERTY_FLAG_NONE)

#define ZENITH_PROPERTY_COLOUR(Type, Name, ...) \
	Type Name = __VA_ARGS__; \
	ZENITH_PROPERTY_REGISTER_BODY(Type, Name, false, 0.0f, 0.0f, PROPERTY_FLAG_COLOUR)
