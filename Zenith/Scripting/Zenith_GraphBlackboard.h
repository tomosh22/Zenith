#pragma once

#include "Core/Zenith_PropertySystem.h"
#include "Collections/Zenith_HashMap.h"
#include <string>

class Zenith_DataStream;

//------------------------------------------------------------------------------
// Zenith_GraphBlackboard - per-graph-instance variable store.
//
// Variables are Zenith_PropertyValue (the Phase 0 tagged variant), declared in
// the graph asset with a name + typed default, readable/writable by nodes and
// overridable per entity (Zenith_GraphComponent serializes per-slot overrides).
//
// Migration contract (hot reload): CopyMatchingFrom copies a value only on an
// exact name+type match - a variable whose type changed is DROPPED and
// reported, never reinterpreted.
//------------------------------------------------------------------------------
class Zenith_GraphBlackboard
{
public:
	// Set stamps/overwrites; the value's type travels with it.
	void SetValue(const std::string& strName, const Zenith_PropertyValue& xValue);

	// Null when absent.
	const Zenith_PropertyValue* TryGetValue(const std::string& strName) const;

	// Typed conveniences with defaults (BT-blackboard style). A present-but-
	// differently-typed value returns the default (and is left untouched).
	float GetFloat(const std::string& strName, float fDefault = 0.0f) const;
	bool GetBool(const std::string& strName, bool bDefault = false) const;
	int32_t GetInt32(const std::string& strName, int32_t iDefault = 0) const;

	bool HasValue(const std::string& strName) const { return TryGetValue(strName) != nullptr; }
	void RemoveValue(const std::string& strName);
	void Clear();
	u_int GetCount() const { return m_xValues.GetSize(); }

	// Exact name+type-matched copy from another blackboard (hot-reload state
	// migration). Returns the number of variables in xSource that could NOT be
	// carried over (missing here or type changed); each drop is logged.
	u_int CopyMatchingFrom(const Zenith_GraphBlackboard& xSource);

	// Per-entity override application (scene/component load). Unlike the
	// hot-reload migration above, variables ABSENT here are restored verbatim
	// (ad-hoc runtime variables round-trip); only a type CONFLICT with a
	// declared variable drops the override (declared type wins, drop logged).
	// Returns the dropped count.
	u_int ApplyOverridesFrom(const Zenith_GraphBlackboard& xSource);

	// Full serialization (count + per-variable name + tagged value).
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Iteration support (editor panel, migration, debugging).
	typedef void (*VisitFn)(void* pxUserData, const std::string& strName, const Zenith_PropertyValue& xValue);
	void VisitAll(VisitFn pfnVisit, void* pxUserData) const;

private:
	Zenith_HashMap<std::string, Zenith_PropertyValue> m_xValues;
};
