#pragma once

#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Collections/Zenith_Vector.h"

//------------------------------------------------------------------------------
// Zenith_GraphBuilder - fluent programmatic authoring over the public
// Zenith_GraphDefinition authoring API. The boot-authoring path for the
// conversion program: games build their .bgraph definitions in C++ instead of
// simulated editor clicks (AddStep_Graph* stays for editor-coverage tests).
//
// Leaf-safe: names only Scripting types; usable from unit tests in all builds.
//
// Usage (single-shot; the builder edits the caller's definition in place):
//   Zenith_GraphDefinition xDef;
//   Zenith_GraphBuilder xBuilder(xDef);
//   xBuilder.Variable("score", xScoreDefault);
//   const u_int uSource = xBuilder.Node("OnUpdate");
//   const u_int uAdd = xBuilder.Node("AddBlackboardFloat");
//   xBuilder.ParamString(uAdd, "m_strVariable", "score")
//           .Chain(uSource, uAdd);
//   const bool bOK = xBuilder.Build();   // commits params + editor layout
//
// Every failure (unknown type, unknown property, rejected edge) logs, latches
// HasErrors(), and keeps going - a boot-authoring typo surfaces as one failed
// Build() with every problem reported, not a cascade of crashes.
//------------------------------------------------------------------------------
class Zenith_GraphBuilder
{
public:
	explicit Zenith_GraphBuilder(Zenith_GraphDefinition& xDefinition);
	~Zenith_GraphBuilder();

	Zenith_GraphBuilder(const Zenith_GraphBuilder&) = delete;
	Zenith_GraphBuilder& operator=(const Zenith_GraphBuilder&) = delete;

	// Declares a blackboard variable with a typed default.
	Zenith_GraphBuilder& Variable(const char* szName, const Zenith_PropertyValue& xDefault);

	// Adds a node of the registered type; returns its node ID (0 = unknown
	// type, logged + error-latched).
	u_int Node(const char* szTypeName);

	// Sets one declared ZENITH_PROPERTY field on the node (by declared field
	// name, e.g. "m_fDegreesPerSecond"). Values are committed to the node's
	// param blob by Build().
	Zenith_GraphBuilder& Param(u_int uNodeID, const char* szProperty, const Zenith_PropertyValue& xValue);
	Zenith_GraphBuilder& ParamFloat(u_int uNodeID, const char* szProperty, float fValue);
	Zenith_GraphBuilder& ParamInt(u_int uNodeID, const char* szProperty, int32_t iValue);
	Zenith_GraphBuilder& ParamBool(u_int uNodeID, const char* szProperty, bool bValue);
	Zenith_GraphBuilder& ParamString(u_int uNodeID, const char* szProperty, const char* szValue);
	Zenith_GraphBuilder& ParamVec3(u_int uNodeID, const char* szProperty, const Zenith_Maths::Vector3& xValue);

	// Typed convenience over ParamInt for an int32-backed enum property (a
	// compare/math op code, a game state, ...) - keeps the magic int out of the
	// call site. Deliberately GENERIC: the builder names no specific op enum, so
	// this header stays leaf-safe (op enums live in EntityComponent). The stored
	// value is the enum's underlying int, so ParamEnum(uNode, p, eV) serializes
	// byte-identically to ParamInt(uNode, p, (int32_t)eV).
	template<typename TEnum>
	Zenith_GraphBuilder& ParamEnum(u_int uNodeID, const char* szProperty, TEnum eValue)
	{
		static_assert(__is_enum(TEnum), "ParamEnum requires an enum type");
		return ParamInt(uNodeID, szProperty, static_cast<int32_t>(eValue));
	}

	// Connects (uSrcNodeID, uSrcPin) -> uDstNodeID (exec input). One outgoing
	// edge per (node, pin) - rejections latch the error flag.
	Zenith_GraphBuilder& Edge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID);

	// Pin-0 linear sugar: Chain(a, b) == Edge(a, 0, b).
	Zenith_GraphBuilder& Chain(u_int uFrom, u_int uTo);

	// Commits every touched node's params into its blob and lays nodes out on
	// an auto grid (column = chain depth) so the editor opens boot-authored
	// graphs legibly. Returns !HasErrors(). Single-shot.
	bool Build();

	bool HasErrors() const { return m_bErrors; }

private:
	struct PendingNode
	{
		u_int m_uNodeID = 0;
		const Zenith_GraphNodeTypeInfo* m_pxInfo = nullptr;
		Zenith_GraphNode* m_pxInstance = nullptr;	// live configured instance; committed + freed by Build()
	};

	PendingNode* FindPending(u_int uNodeID);
	void AssignEditorPositions();

	Zenith_GraphDefinition& m_xDefinition;
	Zenith_Vector<PendingNode> m_axPending;
	bool m_bErrors = false;
	bool m_bBuilt = false;
};
