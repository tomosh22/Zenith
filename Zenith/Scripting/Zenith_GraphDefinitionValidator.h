#pragma once

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
//   - Informational: a declared-but-never-referenced variable, and a LIST name.
//
// ★ OPAQUE NODES. A node type with no pin table contributes nothing and reads
// nothing as far as this can tell. That is the deliberate migration shape: the
// node library is annotated by a later unit, and an un-annotated node must never
// produce a false finding. While ANY node in a graph is opaque the
// declared-but-unreferenced warning is SUPPRESSED for that graph - otherwise
// every variable declaration in every shipped graph would warn.
//
// ★ TWO TIERS, ONE VALIDATOR.
//   - FULL (Validate) - everything above. Runs where a human is authoring:
//     Zenith_GraphBuilder::Build(), and the editor on load / param edit / drop.
//     It needs the node registry.
//   - LOAD_SAFETY (ValidateLoadSafety) - the subset a LOADED asset is refused
//     for, run by Zenith_GraphDefinition::ReadFromDataStream before anything
//     instantiates it. CRASH-CLASS OR SILENTLY-AMBIGUOUS structure only, and
//     every check is a pure fact about the definition, so it takes NO registry:
//     two exec edges leaving one (node, pin) - ambiguous rather than fatal,
//     since FindSuccessor would silently pick the first; two data edges entering
//     one (node, pin name); a malformed data edge (a zero node id, a self-loop,
//     an empty pin name). An ORPHAN edge is NOT in this tier: it is inert at
//     runtime and the FULL tier reports it.
//     ★ Two more checks belong here and are not written yet, each because it
//     needs machinery a later unit adds: PURE-DATA CYCLES among resolved nodes
//     (B-2, which adds m_bPureNode) and STATIC-vs-STATIC type mismatch across a
//     wire (B-3, which resolves a pin NAME through the pin tables).
//   Validate() runs the load-safety checks first and appends their findings, so
//   one FULL report covers both tiers.
//
// ★ ERRORS ARE ERRORS. A finding's severity is the rule's severity, full stop:
// ORPHAN_EDGE, PIN_OUT_OF_RANGE, SELF_READWRITE, UNDECLARED_READ,
// TYPE_MISMATCH, DUPLICATE_EXEC_SOURCE, DUPLICATE_DATA_INPUT and
// DATA_EDGE_MALFORMED are ERROR; DECLARED_UNUSED, LIST_NAME,
// INSTANCE_TYPE_UNRESOLVED and PIN_BINDING_INVALID are WARNING.
// Zenith_GraphBuilder::Build() returns false on any ERROR. (A-5..A-7 ran this
// report-only while the node library was annotated; A-8 latched it.)
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
	// into axOut (CLEARED first). Takes no registry: every check is a pure fact
	// about the definition, which is what lets ReadFromDataStream run it before
	// anything is instantiated. Every finding is ERROR severity.
	static void ValidateLoadSafety(const Zenith_GraphDefinition& xDefinition,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut);

	static const char* GetRuleName(Zenith_GraphValidationRule eRule);
	static const char* GetSeverityName(Zenith_GraphValidationSeverity eSeverity);

private:
	// The shared body of both tiers: APPENDS, never clears. Validate() clears
	// axOut exactly once (its own first statement) and then calls this - calling
	// the public clearing entry point from inside it would drop the FULL findings.
	static void AppendLoadSafetyFindings(const Zenith_GraphDefinition& xDefinition,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut);
};
