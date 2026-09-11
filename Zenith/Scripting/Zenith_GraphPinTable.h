#pragma once

#include "Core/Zenith_PropertySystem.h"
#include "Collections/Zenith_Vector.h"

//------------------------------------------------------------------------------
// Zenith_GraphPinTable - the per-node-class PIN DESCRIPTOR TABLE.
//
// Values pass between Behaviour Graph nodes by NAMED blackboard variables: a
// node declares ZENITH_PROPERTY(std::string, m_str...Var, "...") and reads or
// writes the blackboard under that name at runtime. A mistyped name silently
// yields the type's default - nothing in the engine could see the mistake.
//
// A pin descriptor is the node class saying, ONCE, what each of those name
// properties MEANS: which direction the value flows, what type it is, and which
// property carries the name. Zenith_GraphDefinitionValidator reads the table and
// checks a whole graph against it.
//
//   ZENITH_GRAPH_PINS_BEGIN(MyNode)
//   ZENITH_GRAPH_PIN_INPUT(Value, "m_strValueVar", PROPERTY_TYPE_FLOAT)
//   ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_FLOAT)
//   ZENITH_GRAPH_PINS_END
//
// A node class with NO pin table is OPAQUE to the validator: it contributes no
// writer and performs no read as far as any check can tell. That is the
// deliberate migration shape - annotating the node library is a separate unit,
// and an un-annotated node must never produce a false finding.
//
// Roles. Only INPUT and OUTPUT ever become drawn wires; every role participates
// in validation:
//   INPUT              - reads a value (from a var-name property, a const
//                        property, or both - "" means that half is absent).
//   OUTPUT             - the node's own computed result, written to a var.
//   SELECTOR_READ      - a named-variable REFERENCE that configures the node and
//   SELECTOR_WRITE       stays a validated string forever (never a wire): a
//   SELECTOR_READWRITE   configured source, destination, or in-place target.
//   TARGET_REF         - an entity/position reference resolved at runtime; the
//                        accepted types are a MASK (see the two macros below).
//   LIST               - a name in the blackboard's parallel LIST store, which
//                        is not a Zenith_PropertyValue at all.
//
// Leaf-safe: names only Core + Collections types.
//------------------------------------------------------------------------------

enum Zenith_GraphPinRole : u_int8
{
	GRAPH_PIN_ROLE_INPUT = 0,
	GRAPH_PIN_ROLE_OUTPUT,
	GRAPH_PIN_ROLE_SELECTOR_READ,
	GRAPH_PIN_ROLE_SELECTOR_WRITE,
	GRAPH_PIN_ROLE_SELECTOR_READWRITE,
	GRAPH_PIN_ROLE_TARGET_REF,
	GRAPH_PIN_ROLE_LIST,
	GRAPH_PIN_ROLE_COUNT
};

// The ANY pin type: unifies with every other type. Spelled as the property
// enum's terminator rather than as an 11th property type, so the property
// system gains no surface from the pin system existing.
constexpr Zenith_PropertyType eGRAPH_PIN_TYPE_ANY = PROPERTY_TYPE_COUNT;

// One bit per Zenith_PropertyType. 0 = no restriction (every type accepted).
constexpr u_int uGRAPH_PIN_ACCEPT_ANY = 0u;
// Zenith_GraphContext::ResolveTargetEntity accepts a packed ENTITY_ID and
// NOTHING else - a string entity name is never legal at runtime.
constexpr u_int uGRAPH_PIN_ACCEPT_TARGET_ENTITY = 1u << static_cast<u_int>(PROPERTY_TYPE_ENTITY_ID);
// A polymorphic POSITION reference resolves either an entity's position or a
// literal world position.
constexpr u_int uGRAPH_PIN_ACCEPT_TARGET_POSITION =
	(1u << static_cast<u_int>(PROPERTY_TYPE_ENTITY_ID)) | (1u << static_cast<u_int>(PROPERTY_TYPE_VECTOR3));

inline constexpr u_int Zenith_GraphPinTypeMaskBit(Zenith_PropertyType eType)
{
	return (eType < PROPERTY_TYPE_COUNT) ? (1u << static_cast<u_int>(eType)) : 0u;
}

//------------------------------------------------------------------------------
// Zenith_GraphPinDesc - one declared pin.
//------------------------------------------------------------------------------
struct Zenith_GraphPinDesc
{
	const char* m_szName = nullptr;					// designer-facing pin name
	Zenith_GraphPinRole m_eRole = GRAPH_PIN_ROLE_INPUT;
	// The pin's STATIC type, or eGRAPH_PIN_TYPE_ANY. Ignored (and expected to be
	// ANY) when m_bInstanceResolved is set.
	Zenith_PropertyType m_eType = eGRAPH_PIN_TYPE_ANY;
	// Declared field name of the std::string property carrying the blackboard
	// variable NAME this pin binds to. "" = the pin has no var-name binding (a
	// const-only INPUT), which the variable checks SKIP.
	const char* m_szVarNameProperty = "";
	// Declared field name of the property carrying an inline constant value.
	const char* m_szConstProperty = "";
	// When the bound var-name property reads EMPTY, the pin binds to THIS
	// property's value instead. Expresses the in-place form of the maths nodes
	// (write to m_strResultVar, or back into m_strVar when no result var is set).
	const char* m_szFallbackVarNameProperty = "";
	// The type is answered per INSTANCE by Zenith_GraphNode::GetPinType (e.g. a
	// maths node whose op code decides whether it writes FLOAT or VECTOR3). A
	// node that declines to answer leaves the pin ANY plus one warning - a
	// fabricated type would be worse than no type at all.
	bool m_bInstanceResolved = false;
	// TARGET_REF only: the set of types the runtime resolver accepts.
	u_int m_uAcceptedTypeMask = uGRAPH_PIN_ACCEPT_ANY;
};

//------------------------------------------------------------------------------
// Zenith_GraphPinTable - the per-class descriptor list (the Zenith_PropertyTable
// shape, deliberately: the two are declared side by side on a node class).
//------------------------------------------------------------------------------
class Zenith_GraphPinTable
{
public:
	void AddPin(const Zenith_GraphPinDesc& xPin)
	{
		Zenith_Assert(FindPin(xPin.m_szName) == nullptr,
			"Zenith_GraphPinTable: duplicate pin '%s'", xPin.m_szName ? xPin.m_szName : "(null)");
		m_axPins.PushBack(xPin);
	}

	u_int GetPinCount() const { return m_axPins.GetSize(); }

	const Zenith_GraphPinDesc& GetPinAt(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_axPins.GetSize(), "Zenith_GraphPinTable: index %u out of range", uIndex);
		return m_axPins.Get(uIndex);
	}

	const Zenith_GraphPinDesc* FindPin(const char* szName) const
	{
		if (!szName)
		{
			return nullptr;
		}
		for (u_int u = 0; u < m_axPins.GetSize(); ++u)
		{
			const Zenith_GraphPinDesc& xPin = m_axPins.Get(u);
			if (xPin.m_szName && std::strcmp(xPin.m_szName, szName) == 0)
			{
				return &xPin;
			}
		}
		return nullptr;
	}

private:
	Zenith_Vector<Zenith_GraphPinDesc> m_axPins;
};

//------------------------------------------------------------------------------
// Declaration macros.
//
// ★ ZENITH_GRAPH_PINS_BEGIN emits `public:` FIRST and ZENITH_GRAPH_PINS_END
// restores `private:`. A private GetPinTableStatic() makes the registry's
// concept silently FALSE - i.e. a node that carefully declared its pins would
// be OPAQUE, which is exactly the failure this whole mechanism exists to
// prevent, and nothing would say so. bZENITH_HAS_PIN_TABLE is the paired tag:
// Zenith_GraphNodeRegistry static_asserts that a class carrying the tag also
// exposes a detectable table, so that drift is a compile error.
//
// Tables INHERIT the way property tables do: a derived class that declares no
// block of its own resolves T::GetPinTableStatic() to the base's static member
// function and therefore shares the base's table (the collision-source family
// relies on exactly this for its property table).
//
// Registration runs at static init, construct-on-first-use, with distinct
// alias/prefix names from the property macros so the two never collide in one
// class body. The usual MSVC dead-strip caveat applies.
//------------------------------------------------------------------------------

#define ZENITH_GRAPH_PINS_BEGIN(ClassName) \
public: \
	using ZenithGraphPinOwnerType = ClassName; \
	static constexpr bool bZENITH_HAS_PIN_TABLE = true; \
	static Zenith_GraphPinTable& GetPinTableMutable() \
	{ \
		static Zenith_GraphPinTable ls_xPinTable; \
		return ls_xPinTable; \
	} \
	static const Zenith_GraphPinTable& GetPinTableStatic() { return GetPinTableMutable(); } \
	const Zenith_GraphPinTable& GetPinTable() const { return GetPinTableStatic(); }

#define ZENITH_GRAPH_PINS_END \
private:

// Internal: shared registration machinery (the ZENITH_PROPERTY_REGISTER_BODY
// shape - a static member FUNCTION does the work, a `static inline const bool`
// runs it once at static init).
#define ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, eRoleV, eTypeV, szVarPropV, szConstPropV, szFallbackPropV, bInstanceResolvedV, uMaskV) \
	static bool ZenithGraphPinRegister_##PinName() \
	{ \
		Zenith_GraphPinDesc xPin; \
		xPin.m_szName = #PinName; \
		xPin.m_eRole = eRoleV; \
		xPin.m_eType = eTypeV; \
		xPin.m_szVarNameProperty = szVarPropV; \
		xPin.m_szConstProperty = szConstPropV; \
		xPin.m_szFallbackVarNameProperty = szFallbackPropV; \
		xPin.m_bInstanceResolved = bInstanceResolvedV; \
		xPin.m_uAcceptedTypeMask = uMaskV; \
		GetPinTableMutable().AddPin(xPin); \
		return true; \
	} \
	static inline const bool s_bZenithGraphPinReg_##PinName = ZenithGraphPinRegister_##PinName();

// --- INPUT ------------------------------------------------------------------
#define ZENITH_GRAPH_PIN_INPUT(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)

// A const-only input (no var-name property at all): a valid descriptor that the
// variable checks skip entirely.
#define ZENITH_GRAPH_PIN_INPUT_CONST(PinName, ConstProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, "", ConstProperty, "", false, uGRAPH_PIN_ACCEPT_ANY)

// "Read the var if one is named, otherwise use the inline constant."
#define ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(PinName, VarProperty, ConstProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, VarProperty, ConstProperty, "", false, uGRAPH_PIN_ACCEPT_ANY)

// --- OUTPUT -----------------------------------------------------------------
#define ZENITH_GRAPH_PIN_OUTPUT(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)

#define ZENITH_GRAPH_PIN_OUTPUT_FALLBACK(PinName, VarProperty, FallbackVarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, PinType, VarProperty, "", FallbackVarProperty, false, uGRAPH_PIN_ACCEPT_ANY)

// Type answered per instance by Zenith_GraphNode::GetPinType(pinIndex, out).
#define ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", true, uGRAPH_PIN_ACCEPT_ANY)

#define ZENITH_GRAPH_PIN_OUTPUT_INSTANCE_FALLBACK(PinName, VarProperty, FallbackVarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, VarProperty, "", FallbackVarProperty, true, uGRAPH_PIN_ACCEPT_ANY)

// --- SELECTORS (named references that stay strings forever) ------------------
#define ZENITH_GRAPH_PIN_SELECTOR_READ(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_READ, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)

#define ZENITH_GRAPH_PIN_SELECTOR_WRITE(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_WRITE, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)

#define ZENITH_GRAPH_PIN_SELECTOR_READWRITE(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_READWRITE, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)

// --- TARGET references ------------------------------------------------------
// ResolveTargetEntity accepts ENTITY_ID only.
#define ZENITH_GRAPH_PIN_TARGET_ENTITY(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_TARGET_ENTITY)

// A position reference resolves ENTITY_ID or VECTOR3.
#define ZENITH_GRAPH_PIN_TARGET_POSITION(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_TARGET_POSITION)

// --- LIST -------------------------------------------------------------------
#define ZENITH_GRAPH_PIN_LIST(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY)
