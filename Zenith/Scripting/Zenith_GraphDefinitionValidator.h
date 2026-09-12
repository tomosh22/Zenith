#pragma once

#include "Core/Zenith_PropertySystem.h"	// Zenith_PropertyType - ResolvePinType's out-param
#include "Collections/Zenith_Vector.h"
#include <string>

class Zenith_GraphDefinition;
class Zenith_GraphNodeRegistry;

//------------------------------------------------------------------------------
// Zenith_GraphDefinitionValidator - static analysis of a behaviour graph
// definition against the node library's PIN DESCRIPTOR TABLES
// (Zenith_GraphPinTable.h).
//
// What it checks:
//   - STRUCTURAL: an edge whose endpoint node does not exist, and an edge whose
//     source pin is at or past the node's effective exec-output count
//     (Zenith_GraphNodeRegistry::GetExecOutputCount - the same funnel the editor
//     draws and validates with, failure pin and dynamic pin count included).
//   - DECLARE-OR-ERROR: every variable a graph READS must be declared on the
//     graph or written by an annotated writer IN THE SAME GRAPH - where "writer"
//     never means the reading node itself, so a READWRITE reference cannot
//     satisfy its own read.
//   - TYPE AGREEMENT between a reader and the declaration / every writer. ANY
//     unifies with everything; a TARGET_REF checks its accepted-type MASK.
//   - WIRES (B-3). Every DATA edge is resolved to a real pin on a real node: an
//     endpoint pin name no table declares (or a variadic ordinal past the
//     configured family) is WIRE_PIN_UNKNOWN; a name that resolves to the wrong
//     ROLE is WIRE_ROLE_MISMATCH; two RESOLVED endpoint types that differ, with
//     neither ANY, are TYPE_MISMATCH. An exec edge INTO a pure node is
//     EXEC_INTO_PURE (the runtime drops it); a data cycle through pure producers
//     is DATA_CYCLE; a pure node nothing consumes is PURE_UNCONSUMED.
//   - DOMINANCE (warning, conservative): a consumer that can run before the
//     producer feeding it.
//   - Informational: a declared-but-never-referenced variable, a LIST name, and
//     an in-place OUTPUT alias.
//
// ★ OPAQUE NODES. A node type with no pin table contributes nothing and reads
// nothing as far as this can tell. That is the deliberate migration shape: the
// node library is annotated by a later unit, and an un-annotated node must never
// produce a false finding. While ANY node in a graph is opaque the
// declared-but-unreferenced warning is SUPPRESSED for that graph - otherwise
// every variable declaration in every shipped graph would warn.
//
// ★ ...EXCEPT ON A WIRE, DELIBERATELY. An endpoint of a DATA edge whose type is
// REGISTERED but declares no pin table is an ERROR (WIRE_PIN_UNKNOWN). That does
// not weaken the doctrine above: the doctrine protects the annotation MIGRATION
// from FALSE findings, and a wire into or out of a table-less node is not a
// false finding - Zenith_BehaviourGraph::ResolveDataEdges SKIPS exactly that
// wire, so it can never carry a value. An UNREGISTERED endpoint stays silent
// (per-game node libraries: the instantiation warning covers it), and exec
// chains through opaque nodes stay unreported as before.
//
// ★ TWO TIERS, ONE VALIDATOR.
//   - FULL (Validate) - everything above. Runs where a human is authoring:
//     Zenith_GraphBuilder::Build(), and the editor on load / param edit / drop.
//     It needs the node registry.
//   - LOAD_SAFETY (ValidateLoadSafety) - the subset a LOADED asset is refused
//     for, run by Zenith_GraphDefinition::ReadFromDataStream before anything
//     instantiates it. CRASH-CLASS OR SILENTLY-AMBIGUOUS structure only:
//     two exec edges leaving one (node, pin) - ambiguous rather than fatal,
//     since FindSuccessor would silently pick the first; two data edges entering
//     one (node, pin name); a malformed data edge (a zero node id, a self-loop,
//     an empty pin name); a pure-sourced DATA CYCLE; and a STATIC-vs-STATIC type
//     mismatch across a wire. An ORPHAN edge is NOT in this tier: it is inert at
//     runtime and the FULL tier reports it.
//     ★ IT TAKES THE REGISTRY, and its answer therefore depends on the
//     REGISTERED NODE SET. The last two checks cannot be pure facts about the
//     definition - one needs m_bPureNode, the other needs a pin NAME resolved
//     through a pin table. ReadFromDataStream drains the registry first and
//     always passes it, so the answer is deterministic rather than boot-phase
//     dependent; an initialised-but-EMPTY registry (a build with no registrar)
//     resolves nothing and so skips both, which is exactly B-1's behaviour.
//     A refused load is LoadedOk() == false - "no graph" for every consumer.
//   Validate() runs the load-safety checks first and appends their findings, so
//   one FULL report covers both tiers, and the wire pass below therefore reports
//   a type mismatch ONLY when an endpoint is instance-resolved or from-variable:
//   the static-vs-static case is already counted, once, down here.
//
// ★ ERRORS ARE ERRORS. A finding's severity is the rule's severity, full stop:
// ORPHAN_EDGE, PIN_OUT_OF_RANGE, SELF_READWRITE, UNDECLARED_READ,
// TYPE_MISMATCH, DUPLICATE_EXEC_SOURCE, DUPLICATE_DATA_INPUT,
// DATA_EDGE_MALFORMED, WIRE_PIN_UNKNOWN, WIRE_ROLE_MISMATCH, EXEC_INTO_PURE and
// DATA_CYCLE are ERROR; DECLARED_UNUSED, LIST_NAME, INSTANCE_TYPE_UNRESOLVED,
// PIN_BINDING_INVALID, DOMINANCE, PURE_UNCONSUMED and IN_PLACE_ALIASING are
// WARNING. Zenith_GraphBuilder::Build() returns false on any ERROR. (A-5..A-7
// ran this report-only while the node library was annotated; A-8 latched it.)
//
// Leaf-safe: Scripting + Core + Collections only.
//------------------------------------------------------------------------------

// Never bare ERROR / WARNING: both collide with wingdi.h macros.
enum Zenith_GraphValidationSeverity : u_int8
{
	GRAPH_VALIDATION_SEVERITY_ERROR = 0,
	GRAPH_VALIDATION_SEVERITY_WARNING,
	GRAPH_VALIDATION_SEVERITY_COUNT
};

enum Zenith_GraphValidationRule : u_int8
{
	GRAPH_VALIDATION_RULE_UNDECLARED_READ = 0,		// read of a variable nothing declares or writes
	GRAPH_VALIDATION_RULE_TYPE_MISMATCH,			// reader/writer (or reader/declaration) types disagree
	GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE,			// edge from a pin the source node does not have
	GRAPH_VALIDATION_RULE_ORPHAN_EDGE,				// edge to or from a node that is not in the graph
	GRAPH_VALIDATION_RULE_DECLARED_UNUSED,			// declared variable nothing references
	GRAPH_VALIDATION_RULE_LIST_NAME,				// informational: a blackboard LIST name
	GRAPH_VALIDATION_RULE_SELF_READWRITE,			// the only writer of a read variable is the reading node
	GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED,	// m_bInstanceResolved pin whose node declined to answer
	GRAPH_VALIDATION_RULE_PIN_BINDING_INVALID,		// the pin names a property the class does not declare as a string
	GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE,	// two exec edges leave one (node, pin) - which one runs is arbitrary
	GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT,		// two data edges enter one (node, pin name) - one wire per input
	GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED,		// data edge with a zero node id, a self-loop, or an empty pin name
	GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN,			// wire names a pin the endpoint type does not declare (or an opaque endpoint)
	GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH,		// wire names a real pin of the WRONG role (source must be OUTPUT, destination INPUT)
	GRAPH_VALIDATION_RULE_EXEC_INTO_PURE,			// exec edge into a PURE node, which has no exec input
	GRAPH_VALIDATION_RULE_DATA_CYCLE,				// data edges through pure producers form a cycle
	GRAPH_VALIDATION_RULE_DOMINANCE,				// a consumer can execute before the producer feeding it
	GRAPH_VALIDATION_RULE_PURE_UNCONSUMED,			// a pure node no wire consumes - it can never run
	GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING,		// an OUTPUT whose result var is empty, so it writes back over its own source
	GRAPH_VALIDATION_RULE_COUNT
};

struct Zenith_GraphValidationFinding
{
	Zenith_GraphValidationSeverity m_eSeverity = GRAPH_VALIDATION_SEVERITY_WARNING;
	Zenith_GraphValidationRule m_eRule = GRAPH_VALIDATION_RULE_UNDECLARED_READ;
	u_int m_uNodeID = 0;			// 0 = not attributable to one node
	std::string m_strTypeName;		// COPIED, not borrowed: a type info can be torn down by a test reset
	std::string m_strPin;			// pin name, or empty
	std::string m_strVar;			// variable name, or empty
	std::string m_strWhat;			// human-readable detail
};

class Zenith_GraphDefinitionValidator
{
public:
	// FULL-tier validation. Writes every finding into axOut (CLEARED first) and
	// logs nothing - the caller decides whether this run is worth a report (the
	// editor re-validates on every parameter keystroke).
	//
	// ★ The caller must have called xRegistry.EnsureInitialized() first: that is
	// non-const, and this deliberately takes the registry by const reference so
	// validating a graph can never mutate the node library.
	//
	// szGraphName is reporting-only (a definition carries no name); null or empty
	// reports as "<unnamed>".
	static void Validate(const Zenith_GraphDefinition& xDefinition, const Zenith_GraphNodeRegistry& xRegistry,
		const char* szGraphName, Zenith_Vector<Zenith_GraphValidationFinding>& axOut);

	// One line per finding plus one summary line, all prefixed
	// "[GraphValidator] " - a prefix that appears on no other log line in the
	// repo, so the tools-boot report is one Select-String away. An ERROR finding
	// goes to Zenith_Error (it is a real defect); a WARNING to Zenith_Log.
	static void LogFindings(const char* szGraphName, u_int uNodeCount,
		const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings);

	// LOAD_SAFETY-tier validation (see the tier note above). Writes every finding
	// into axOut (CLEARED first). Every finding is ERROR severity.
	//
	// ★ The registry is REQUIRED, not optional: an optional one would make the
	// same asset refused or accepted depending on when it was read. The caller
	// must have called EnsureInitialized() (this takes it const so validating can
	// never mutate the node library); an initialised-but-empty registry is legal
	// and simply resolves nothing.
	static void ValidateLoadSafety(const Zenith_GraphDefinition& xDefinition,
		const Zenith_GraphNodeRegistry& xRegistry,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut);

	// THE pin-type resolver, exported so nothing re-derives it. Answers the pin's
	// RESOLVED type: the descriptor's static type, the param-applied instance's
	// GetPinType answer, or the DECLARED type of the variable a from-variable pin
	// points at. false = the node or the pin does not exist, or the node type is
	// not registered in this build; eOut may legitimately be eGRAPH_PIN_TYPE_ANY.
	//
	// ★ IT ALLOCATES - one temp instance per instance-resolved or from-variable
	// query - and is a QUERY, not a draw-time accessor. A caller that colours or
	// labels pins (the editor, B-4) MUST cache the per-node answer and invalidate
	// it on a definition edit, a param edit or a reload; calling this per pin per
	// frame is an allocation per pin per frame.
	//
	// Zenith_BehaviourGraph::BuildPinState stamps the runtime slot with the SAME
	// answer, and a unit asserts the two agree for all three forms.
	static bool ResolvePinType(const Zenith_GraphDefinition& xDefinition, const Zenith_GraphNodeRegistry& xRegistry,
		u_int uNodeID, u_int uPinIndex, Zenith_PropertyType& eOut);

	static const char* GetRuleName(Zenith_GraphValidationRule eRule);
	static const char* GetSeverityName(Zenith_GraphValidationSeverity eSeverity);

private:
	// The shared body of both tiers: APPENDS, never clears. Validate() clears
	// axOut exactly once (its own first statement) and then calls this - calling
	// the public clearing entry point from inside it would drop the FULL findings.
	// It OWNS the pure-sourced DATA_CYCLE check and the STATIC-vs-STATIC wire
	// TYPE_MISMATCH, so the FULL tier's wire pass must not report either again.
	static void AppendLoadSafetyFindings(const Zenith_GraphDefinition& xDefinition,
		const Zenith_GraphNodeRegistry& xRegistry,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut);
};
