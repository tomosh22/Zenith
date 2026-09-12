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
// ★ A PIN'S TYPE HAS THREE SOURCES, and only one of them is the graph. The
// CLASS answers statically (m_eType); the INSTANCE answers per configured node
// (m_bInstanceResolved -> Zenith_GraphNode::GetPinType); and the GRAPH answers
// through m_szTypeFromVarNameProperty - the pin takes the DECLARED type of the
// variable a named property points at (GetVariable's output). That third form is
// a TYPE source and nothing else: see the field comment below for why it binds,
// writes and dual-writes nothing.
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

class Zenith_GraphNode;

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
	// ★ THE ONE WAY A PIN'S TYPE COMES FROM THE GRAPH rather than from the class
	// (m_eType) or from the instance (m_bInstanceResolved). Declared field name of
	// the std::string property whose VALUE names a blackboard variable; the pin's
	// resolved type is THAT VARIABLE'S DECLARED type
	// (Zenith_GraphVariableDecl::m_xDefault.GetType()). An undeclared variable
	// leaves the pin ANY and earns no finding of its own - the SELECTOR_READ that
	// names it is already reported by declare-or-error, and a second finding for
	// one mistake is noise. A writer-only variable is NOT a type source either:
	// the DECLARATION is the contract.
	//
	// ★ IT IS A TYPE SOURCE AND NOTHING ELSE - never a binding, never a writer.
	// A descriptor carrying it leaves m_szVarNameProperty, m_szConstProperty and
	// m_szFallbackVarNameProperty all "" on purpose: binding the same property as
	// a var name would make the node an annotated WRITER of the variable it
	// READS, which would satisfy every other reader's declare-or-error and
	// dual-write the variable to itself.
	const char* m_szTypeFromVarNameProperty = "";
	// The type is answered per INSTANCE by Zenith_GraphNode::GetPinType (e.g. a
	// maths node whose op code decides whether it writes FLOAT or VECTOR3). A
	// node that declines to answer leaves the pin ANY plus one warning - a
	// fabricated type would be worse than no type at all.
	bool m_bInstanceResolved = false;
	// INPUT only: the descriptor declares a FAMILY of ordinal pins rather than one
	// pin. A data edge names a member as "<family><ordinal>" ("in0", "in1", ...);
	// the count comes from the param-applied instance's
	// Zenith_GraphNode::GetDynamicDataInputCount(). A family member is addressed
	// at runtime through the ordinal overload of GetInput.
	bool m_bVariadic = false;
	// TARGET_REF only: the set of types the runtime resolver accepts.
	u_int m_uAcceptedTypeMask = uGRAPH_PIN_ACCEPT_ANY;
};

//------------------------------------------------------------------------------
// Runtime helpers shared by the pin runtime (Zenith_GraphNode accessors +
// Zenith_BehaviourGraph resolution) and the definition validator.
//------------------------------------------------------------------------------

// The ZERO value of a runtime pin type, TYPE-STAMPED. Zenith_PropertyValue
// default-constructs FLOAT-tagged, and the tagged getters ASSERT on a tag
// mismatch, so an untyped zero handed to a typed consumer would DebugBreak a
// developer. eGRAPH_PIN_TYPE_ANY has no zero - which is exactly why an ANY
// output slot starts UNSET instead of zeroed.
inline Zenith_PropertyValue Zenith_GraphPin_MakeZeroValue(Zenith_PropertyType eType)
{
	Zenith_PropertyValue xValue;
	switch (eType)
	{
	case PROPERTY_TYPE_FLOAT:		xValue.SetFloat(0.0f); break;
	case PROPERTY_TYPE_INT32:		xValue.SetInt32(0); break;
	case PROPERTY_TYPE_UINT32:		xValue.SetUInt32(0u); break;
	case PROPERTY_TYPE_BOOL:		xValue.SetBool(false); break;
	case PROPERTY_TYPE_VECTOR2:		xValue.SetVector2(Zenith_Maths::Vector2(0.0f)); break;
	case PROPERTY_TYPE_VECTOR3:		xValue.SetVector3(Zenith_Maths::Vector3(0.0f)); break;
	case PROPERTY_TYPE_VECTOR4:		xValue.SetVector4(Zenith_Maths::Vector4(0.0f)); break;
	case PROPERTY_TYPE_STRING:		xValue.SetString(std::string()); break;
	case PROPERTY_TYPE_ENTITY_ID:	xValue.SetPackedEntityID(0ull); break;
	case PROPERTY_TYPE_GUID:		xValue.SetGUID(Zenith_GUID(0ull)); break;
	default:						xValue.SetFloat(0.0f); break;	// eGRAPH_PIN_TYPE_ANY: no zero exists
	}
	return xValue;
}

enum Zenith_GraphPinReadPropertyResult : u_int8
{
	GRAPH_PIN_READ_PROPERTY_NOT_BOUND = 0,	// the descriptor names no property - legal, skipped
	GRAPH_PIN_READ_PROPERTY_OK,
	GRAPH_PIN_READ_PROPERTY_INVALID			// the named property is missing or is not a string
};

// Reads a declared std::string property off a live instance THROUGH the
// property table. The tagged getters ASSERT on a type mismatch
// (Zenith_PropertySystem.h) and an assert DebugBreaks a developer, so the tag is
// checked BEFORE GetString: a mis-declared pin table yields a result code, never
// a break.
//
// ONE home: the validator resolves its bindings through this, and so does the
// runtime at Zenith_BehaviourGraph::InitialiseFromDefinition - "the var name the
// runtime binds" and "the var name the validator checked" cannot drift.
inline Zenith_GraphPinReadPropertyResult Zenith_GraphPin_ReadStringProperty(const Zenith_PropertyTable* pxTable,
	const Zenith_GraphNode* pxNode, const char* szProperty, std::string& strOut)
{
	if (szProperty == nullptr || szProperty[0] == '\0')
	{
		return GRAPH_PIN_READ_PROPERTY_NOT_BOUND;
	}
	if (pxTable == nullptr || pxNode == nullptr)
	{
		return GRAPH_PIN_READ_PROPERTY_INVALID;
	}
	const Zenith_ReflectedProperty* pxProperty = pxTable->FindProperty(szProperty);
	if (pxProperty == nullptr || pxProperty->m_pfnGet == nullptr || pxProperty->m_eType != PROPERTY_TYPE_STRING)
	{
		return GRAPH_PIN_READ_PROPERTY_INVALID;
	}
	Zenith_PropertyValue xValue;
	pxProperty->m_pfnGet(pxNode, xValue);
	if (xValue.GetType() != PROPERTY_TYPE_STRING)
	{
		return GRAPH_PIN_READ_PROPERTY_INVALID;
	}
	strOut = xValue.GetString();
	return GRAPH_PIN_READ_PROPERTY_OK;
}

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

	// The pin's INDEX, which is what every runtime accessor addresses (a data
	// edge stores the NAME; the binding arrays are indexed by position in this
	// table). GetPinCount() = no such pin - never an assert.
	u_int FindPinIndex(const char* szName) const
	{
		if (!szName)
		{
			return m_axPins.GetSize();
		}
		for (u_int u = 0; u < m_axPins.GetSize(); ++u)
		{
			const Zenith_GraphPinDesc& xPin = m_axPins.Get(u);
			if (xPin.m_szName && std::strcmp(xPin.m_szName, szName) == 0)
			{
				return u;
			}
		}
		return m_axPins.GetSize();
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
#define ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, eRoleV, eTypeV, szVarPropV, szConstPropV, szFallbackPropV, bInstanceResolvedV, uMaskV, bVariadicV, szTypeFromVarPropV) \
	static bool ZenithGraphPinRegister_##PinName() \
	{ \
		Zenith_GraphPinDesc xPin; \
		xPin.m_szName = #PinName; \
		xPin.m_eRole = eRoleV; \
		xPin.m_eType = eTypeV; \
		xPin.m_szVarNameProperty = szVarPropV; \
		xPin.m_szConstProperty = szConstPropV; \
		xPin.m_szFallbackVarNameProperty = szFallbackPropV; \
		xPin.m_szTypeFromVarNameProperty = szTypeFromVarPropV; \
		xPin.m_bInstanceResolved = bInstanceResolvedV; \
		xPin.m_bVariadic = bVariadicV; \
		xPin.m_uAcceptedTypeMask = uMaskV; \
		GetPinTableMutable().AddPin(xPin); \
		return true; \
	} \
	static inline const bool s_bZenithGraphPinReg_##PinName = ZenithGraphPinRegister_##PinName();

// --- INPUT ------------------------------------------------------------------
#define ZENITH_GRAPH_PIN_INPUT(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

// A const-only input (no var-name property at all): a valid descriptor that the
// variable checks skip entirely.
#define ZENITH_GRAPH_PIN_INPUT_CONST(PinName, ConstProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, "", ConstProperty, "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

// "Read the var if one is named, otherwise use the inline constant."
#define ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(PinName, VarProperty, ConstProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, VarProperty, ConstProperty, "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

// A FAMILY of ordinal inputs: wires name "<PinName><ordinal>" ("in0", "in1",
// ...) and the member count comes from the param-applied instance's
// GetDynamicDataInputCount(). No var-name and no const property - a family
// member is a wire or it is the type's zero. Addressed at runtime through
// GetInput<T>(ctx, pin, ordinal).
#define ZENITH_GRAPH_PIN_INPUT_VARIADIC(PinName, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_INPUT, PinType, "", "", "", false, uGRAPH_PIN_ACCEPT_ANY, true, "")

// --- OUTPUT -----------------------------------------------------------------
#define ZENITH_GRAPH_PIN_OUTPUT(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

#define ZENITH_GRAPH_PIN_OUTPUT_FALLBACK(PinName, VarProperty, FallbackVarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, PinType, VarProperty, "", FallbackVarProperty, false, uGRAPH_PIN_ACCEPT_ANY, false, "")

// Type answered per instance by Zenith_GraphNode::GetPinType(pinIndex, out).
#define ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", true, uGRAPH_PIN_ACCEPT_ANY, false, "")

#define ZENITH_GRAPH_PIN_OUTPUT_INSTANCE_FALLBACK(PinName, VarProperty, FallbackVarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, VarProperty, "", FallbackVarProperty, true, uGRAPH_PIN_ACCEPT_ANY, false, "")

// ★ Type FROM THE GRAPH: the pin's resolved type is the DECLARED type of the
// variable named by SelectorVarProperty's value (see
// m_szTypeFromVarNameProperty above). The node's own blackboard read is declared
// separately, as a SELECTOR_READ on the same property - THIS descriptor binds
// nothing, writes nothing and dual-writes nothing, which is what keeps a node
// that READS a variable from registering as an annotated WRITER of it.
// Undeclared variable -> ANY, and no second finding.
#define ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE(PinName, SelectorVarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, "", "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, SelectorVarProperty)

// --- SELECTORS (named references that stay strings forever) ------------------
#define ZENITH_GRAPH_PIN_SELECTOR_READ(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_READ, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

#define ZENITH_GRAPH_PIN_SELECTOR_WRITE(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_WRITE, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

#define ZENITH_GRAPH_PIN_SELECTOR_READWRITE(PinName, VarProperty, PinType) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_SELECTOR_READWRITE, PinType, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")

// --- TARGET references ------------------------------------------------------
// ResolveTargetEntity accepts ENTITY_ID only.
#define ZENITH_GRAPH_PIN_TARGET_ENTITY(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_TARGET_ENTITY, false, "")

// A position reference resolves ENTITY_ID or VECTOR3.
#define ZENITH_GRAPH_PIN_TARGET_POSITION(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_TARGET_POSITION, false, "")

// --- LIST -------------------------------------------------------------------
#define ZENITH_GRAPH_PIN_LIST(PinName, VarProperty) \
	ZENITH_GRAPH_PIN_REGISTER_BODY(PinName, GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, VarProperty, "", "", false, uGRAPH_PIN_ACCEPT_ANY, false, "")
