#pragma once

//------------------------------------------------------------------------------
// Shared harness for the PIN-TABLE TOTALITY tests of the engine node TUs AND of
// the GAME node headers (each game's registrar hook is just another
// Zenith_GraphNodeRegistrarFn).
// Included by each Zenith_GraphNode_Registration[_<TU>].Tests.inl and by the
// per-game totality test TUs; carries NO
// node classes and makes NO registry registrations of its own, so including it
// from every TU is safe (the hazard that keeps test NODE types per-TU is
// duplicate name-keyed registration, which a harness has none of).
//
// Everything here is `inline` at namespace scope and NOT in an anonymous
// namespace, deliberately - the same reasoning as
// Zenith_GraphNodeFailurePin.TestHarness.inl: an anonymous namespace in a header
// included by many translation units is many copies again, and `static` free
// functions in a header are the C4505 build break the moment one TU uses a
// subset.
//
// WHAT THE TOTALITY WALK PROVES, for one TU:
//   (i)  EVERY property whose declared name matches `m_str*Var*` - i.e. every
//        property that CARRIES A BLACKBOARD VARIABLE NAME - is covered by some
//        descriptor in its class's pin table, as the descriptor's var-name
//        property or as its fallback var-name property. A node type with such a
//        property and NO pin table fails, naming the type and the property: an
//        un-annotated node is OPAQUE to Zenith_GraphDefinitionValidator, so the
//        failure mode this catches is a whole node quietly dropping out of
//        validation while every other test stays green.
//   (ii) EVERY property name a descriptor points at (var, const, fallback)
//        really EXISTS in that class's property table - a typo'd
//        "m_strTargtVar" would otherwise bind to nothing and check nothing.
//
// ★ WHY THE REGISTRY IS SWAPPED RATHER THAN FILTERED. The set "the node types
// THIS TU registers" has exactly one honest spelling: run this TU's registrar
// and look at what came out. It is total by construction, so a node added to the
// TU tomorrow is covered with no list to maintain. Filtering the live registry
// by m_strCategory would be neither: the field is `#ifdef ZENITH_TOOLS`
// (Zenith_GraphNodeRegistry.h) while ZENITH_TESTING is unconditional - a `_False`
// compile break the Null_True gate cannot see - and categories ALIAS across TUs
// ("Blackboard" is core's and _Math's, "Flow" is core's and _Flow's, "Scene" is
// core's and _Scene's).
//
// ★ THE RESTORE IS RAII, AND IT RESTORES THE GAME'S NODES TOO.
// Zenith_GraphPinTotalityRegistryGuard snapshots the LIVE registry (whole rows +
// their addresses) in its constructor and rebuilds it in its destructor. Two
// reasons it cannot be a plain pair of statements at the end of the walk:
//   - ZENITH_ASSERT_* CONTINUES past a failure, and an early return (or a future
//     one) would leave the process registry holding one TU's types, cascading
//     into every later test in the run;
//   - the engine registrar does NOT re-derive a GAME's node types. Games install
//     theirs directly from their project hook (ZM_RegisterGraphNodes and
//     siblings), never through SetNodeRegistrar, so `ResetForTests +
//     EnsureInitialized` alone - the shape ScriptTest_Contracts.cpp uses, which
//     is safe there only because ScriptTest registers none - would DELETE them
//     for the rest of the process. The guard therefore does NOT run the engine
//     registrar at all on restore: it installs a no-op registrar and replays
//     EVERY snapshot row in snapshot order - game rows sit at 0..N-1 because a
//     game's project hook registers BEFORE the engine registrar drains.
// It then ASSERTS the restored registry matches the snapshot by name, index-wise,
// AND by row address: Zenith_Vector::Clear() keeps the buffer, so re-registering
// the same N types refills the same N addresses and a live graph's cached
// `const Zenith_GraphNodeTypeInfo*` stays valid - but that same property makes an
// address comparison blind to a REORDER, which is why both are checked.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphPinTable.h"
#include "Core/Zenith_PropertySystem.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>
#include <string>

// The engine registrar, defined at the bottom of
// Zenith_GraphNode_Registration.cpp. Declared here so the restore guard can
// reinstall it without inventing a public header for it (every TU that needs a
// sibling registrar declares it the same way).
void Zenith_RegisterEngineGraphNodes();

typedef void (*Zenith_GraphNodeRegistrarFn)();

// THE matcher. Case-sensitive, substring "Var" AFTER the "m_str" prefix, so it
// catches m_strTargetVar, m_strResultVar and m_strVariable alike, and does not
// catch m_strValue / m_strEventName / m_strCases / m_strStateNames.
inline bool Zenith_GraphPinTotality_IsVarNameProperty(const char* szName)
{
	return szName != nullptr
		&& std::strncmp(szName, "m_str", 5) == 0
		&& std::strstr(szName + 5, "Var") != nullptr;
}

inline bool Zenith_GraphPinTotality_ContainsName(const Zenith_Vector<std::string>& xNames, const std::string& strName)
{
	for (u_int u = 0; u < xNames.GetSize(); ++u)
	{
		if (xNames.Get(u) == strName)
		{
			return true;
		}
	}
	return false;
}

inline bool Zenith_GraphPinTotality_IsExempt(const char* const* aszExemptProperties, u_int uExemptCount, const char* szName)
{
	for (u_int u = 0; u < uExemptCount; ++u)
	{
		if (aszExemptProperties[u] != nullptr && szName != nullptr
			&& std::strcmp(aszExemptProperties[u], szName) == 0)
		{
			return true;
		}
	}
	return false;
}

// Tears the registry down and refills it from ONE registrar. Only ever called
// with a Zenith_GraphPinTotalityRegistryGuard alive in an enclosing scope.
inline void Zenith_GraphPinTotality_SwapToRegistrar(Zenith_GraphNodeRegistrarFn pfnRegistrar)
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.ResetForTests();
	xRegistry.SetNodeRegistrar(pfnRegistrar);
	xRegistry.EnsureInitialized();
}

// Appends the type names ONE registrar produces. Used to subtract the sub-TU
// registrars from the core registrar, which calls all of them.
inline void Zenith_GraphPinTotality_CollectNames(Zenith_GraphNodeRegistrarFn pfnRegistrar, Zenith_Vector<std::string>& xNamesOut)
{
	Zenith_GraphPinTotality_SwapToRegistrar(pfnRegistrar);
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
	{
		xNamesOut.PushBack(xRegistry.GetTypeAt(u).m_strTypeName);
	}
}

struct Zenith_GraphPinTotalityRegistryGuard
{
	Zenith_GraphPinTotalityRegistryGuard()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
		{
			m_axRows.PushBack(xRegistry.GetTypeAt(u));
			m_apxRowAddresses.PushBack(&xRegistry.GetTypeAt(u));
		}
	}

	// Registering nothing: EnsureInitialized() drains whatever registrar is
	// installed, and the restore must mark the registry initialised WITHOUT
	// running the engine registrar - the rows come back from the snapshot.
	static void NoOpRegistrar() {}

	~Zenith_GraphPinTotalityRegistryGuard()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.ResetForTests();

		// Replay the snapshot IN ITS ORIGINAL ORDER. A game registers its node
		// types from its project hook BEFORE the engine registrar drains, so an
		// engine-first rebuild would put every game row after every engine row
		// - every live graph's cached type-info pointer would then name a
		// different type (measured: RenderTest's RTTennis* rows sat at 0..9).
		xRegistry.SetNodeRegistrar(&NoOpRegistrar);
		xRegistry.EnsureInitialized();
		for (u_int u = 0; u < m_axRows.GetSize(); ++u)
		{
			xRegistry.Register(m_axRows.Get(u));
		}
		xRegistry.SetNodeRegistrar(&Zenith_RegisterEngineGraphNodes);

		ZENITH_ASSERT_EQ(xRegistry.GetTypeCount(), m_axRows.GetSize(),
			"pin-totality restore did not rebuild the node registry to its original size");

		const u_int uCommon = xRegistry.GetTypeCount() < m_axRows.GetSize()
			? xRegistry.GetTypeCount() : m_axRows.GetSize();
		for (u_int u = 0; u < uCommon; ++u)
		{
			const Zenith_GraphNodeTypeInfo& xRestored = xRegistry.GetTypeAt(u);
			ZENITH_ASSERT_TRUE(xRestored.m_strTypeName == m_axRows.Get(u).m_strTypeName,
				"pin-totality restore REORDERED row %u (was '%s', now '%s') - every live graph's cached type info now names a different node type",
				u, m_axRows.Get(u).m_strTypeName.c_str(), xRestored.m_strTypeName.c_str());
			ZENITH_ASSERT_TRUE(&xRestored == m_apxRowAddresses.Get(u),
				"pin-totality restore MOVED row %u ('%s') - a live graph's cached type-info pointer dangles",
				u, m_axRows.Get(u).m_strTypeName.c_str());
		}
	}

	Zenith_Vector<Zenith_GraphNodeTypeInfo> m_axRows;
	Zenith_Vector<const Zenith_GraphNodeTypeInfo*> m_apxRowAddresses;
};

// The whole walk for one TU - ONE implementation, TWO front doors.
//
// This is the COUNTING form: it logs every failure with Zenith_Error and
// RETURNS how many there were. The asserting form below is a thin wrapper that
// asserts the count is zero.
//
// ★ WHY BOTH EXIST. ZENITH_ASSERT_* outside a ZENITH_TEST body asserts on
// NOTHING (Zenith_TestFramework.h:275 - the runner has no live test case to
// record against), so a game whose gate runs AUTOMATED tests rather than
// ZENITH_TESTs - DevilsPlayground, whose `zenith test` passes
// --skip-unit-tests - could only call the asserting form and watch it pass
// unconditionally. A counted return is the shape an automated test can check
// (ScriptTest_Contracts.cpp:86-135). The registry RAII restore is unchanged and
// applies to both, since it is this function's own local.
//
// pfnRegistrar          - this TU's Zenith_RegisterEngineGraphNodes_<TU>, or a
//                         GAME's registrar (DP_RegisterGraphNodes and siblings).
// apfnExcludedRegistrars/uExcludedCount - registrars whose types are NOT this
//                         TU's. Only the CORE TU needs these: its registrar,
//                         Zenith_RegisterEngineGraphNodes, calls every sibling
//                         sub-registrar, so core's own set is the difference.
// szTuName              - prefixes every message.
// aszExemptProperties   - matcher hits that no pin can express (see
//                         LogicBlackboardBool.m_strVars, a comma-separated LIST
//                         of variable names). Passed per TU, never global.
inline u_int Zenith_CountPinTableTotalityFailuresEx(Zenith_GraphNodeRegistrarFn pfnRegistrar,
	Zenith_GraphNodeRegistrarFn const* apfnExcludedRegistrars, u_int uExcludedCount,
	const char* szTuName, const char* const* aszExemptProperties, u_int uExemptCount)
{
	Zenith_GraphPinTotalityRegistryGuard xGuard;
	u_int uFailures = 0;

	Zenith_Vector<std::string> xExcludedNames;
	for (u_int u = 0; u < uExcludedCount; ++u)
	{
		const u_int uBefore = xExcludedNames.GetSize();
		Zenith_GraphPinTotality_CollectNames(apfnExcludedRegistrars[u], xExcludedNames);
		// A sibling registrar that produced nothing would silently widen this
		// TU's set instead of narrowing it.
		if (xExcludedNames.GetSize() <= uBefore)
		{
			++uFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"%s: an excluded sibling registrar registered NO node types", szTuName);
		}
	}

	Zenith_GraphPinTotality_SwapToRegistrar(pfnRegistrar);
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	u_int uTypesWalked = 0;
	u_int uVarNamePropertiesSeen = 0;
	for (u_int uType = 0; uType < xRegistry.GetTypeCount(); ++uType)
	{
		const Zenith_GraphNodeTypeInfo& xInfo = xRegistry.GetTypeAt(uType);
		if (Zenith_GraphPinTotality_ContainsName(xExcludedNames, xInfo.m_strTypeName))
		{
			continue;
		}
		++uTypesWalked;

		const Zenith_PropertyTable* pxProperties = xInfo.m_pfnGetPropertyTable != nullptr
			? xInfo.m_pfnGetPropertyTable() : nullptr;
		const Zenith_GraphPinTable* pxPins = xInfo.m_pfnGetPinTable != nullptr
			? xInfo.m_pfnGetPinTable() : nullptr;

		// (i) every blackboard-variable-NAME property is covered by a descriptor.
		if (pxProperties != nullptr)
		{
			for (u_int uProp = 0; uProp < pxProperties->GetPropertyCount(); ++uProp)
			{
				const char* szProperty = pxProperties->GetPropertyAt(uProp).m_szName;
				if (!Zenith_GraphPinTotality_IsVarNameProperty(szProperty))
				{
					continue;
				}
				if (Zenith_GraphPinTotality_IsExempt(aszExemptProperties, uExemptCount, szProperty))
				{
					continue;
				}
				++uVarNamePropertiesSeen;

				bool bCovered = false;
				if (pxPins != nullptr)
				{
					for (u_int uPin = 0; uPin < pxPins->GetPinCount() && !bCovered; ++uPin)
					{
						const Zenith_GraphPinDesc& xDesc = pxPins->GetPinAt(uPin);
						bCovered = (xDesc.m_szVarNameProperty != nullptr && std::strcmp(xDesc.m_szVarNameProperty, szProperty) == 0)
							|| (xDesc.m_szFallbackVarNameProperty != nullptr && std::strcmp(xDesc.m_szFallbackVarNameProperty, szProperty) == 0);
					}
				}
				if (!bCovered)
				{
					++uFailures;
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"%s: node type '%s' carries blackboard-variable-name property '%s' with no pin descriptor%s - the node is OPAQUE to the graph validator for that name",
						szTuName, xInfo.m_strTypeName.c_str(), szProperty,
						pxPins == nullptr ? " (the class declares NO pin table at all)" : "");
				}
			}
		}

		// (ii) every property a descriptor names really exists.
		if (pxPins != nullptr)
		{
			for (u_int uPin = 0; uPin < pxPins->GetPinCount(); ++uPin)
			{
				const Zenith_GraphPinDesc& xDesc = pxPins->GetPinAt(uPin);
				const char* aszNamed[3] = { xDesc.m_szVarNameProperty, xDesc.m_szConstProperty, xDesc.m_szFallbackVarNameProperty };
				for (u_int uSlot = 0; uSlot < 3u; ++uSlot)
				{
					if (aszNamed[uSlot] == nullptr || aszNamed[uSlot][0] == '\0')
					{
						continue;	// "" = that half of the pin is absent, by design
					}
					const bool bExists = pxProperties != nullptr
						&& pxProperties->FindProperty(aszNamed[uSlot]) != nullptr;
					if (!bExists)
					{
						++uFailures;
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"%s: node type '%s' pin '%s' names property '%s', which its property table does not declare",
							szTuName, xInfo.m_strTypeName.c_str(),
							xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)", aszNamed[uSlot]);
					}
				}
			}
		}
	}

	// Positive controls. Without these a registrar that silently produced
	// nothing - or a TU whose pin-registering statics were dead-stripped away,
	// leaving empty tables AND no properties to check them against - would pass
	// this test by examining nothing at all.
	if (uTypesWalked == 0u)
	{
		++uFailures;
		Zenith_Error(LOG_CATEGORY_UNITTEST, "%s: the registrar swap yielded NO node types for this TU", szTuName);
	}
	if (uVarNamePropertiesSeen == 0u)
	{
		++uFailures;
		Zenith_Error(LOG_CATEGORY_UNITTEST,
			"%s: not one m_str*Var* property was found across this TU - the walk proved nothing", szTuName);
	}
	return uFailures;
}

// The counting form, plain: a registrar that registers only its own types.
inline u_int Zenith_CountPinTableTotalityFailures(Zenith_GraphNodeRegistrarFn pfnRegistrar, const char* szTuName,
	const char* const* aszExemptProperties, u_int uExemptCount)
{
	return Zenith_CountPinTableTotalityFailuresEx(pfnRegistrar, nullptr, 0u, szTuName, aszExemptProperties, uExemptCount);
}

// The ASSERTING front door - for a ZENITH_TEST body, where ZENITH_ASSERT_* has a
// live test case to record against. One walk: the per-failure detail is already
// on the log as Zenith_Error lines; this assertion carries the count.
inline void Zenith_CheckPinTableTotalityEx(Zenith_GraphNodeRegistrarFn pfnRegistrar,
	Zenith_GraphNodeRegistrarFn const* apfnExcludedRegistrars, u_int uExcludedCount,
	const char* szTuName, const char* const* aszExemptProperties, u_int uExemptCount)
{
	const u_int uFailures = Zenith_CountPinTableTotalityFailuresEx(pfnRegistrar, apfnExcludedRegistrars,
		uExcludedCount, szTuName, aszExemptProperties, uExemptCount);
	ZENITH_ASSERT_EQ(uFailures, 0u,
		"%s: %u pin-table totality failure(s) - each is a Zenith_Error line above naming the node type and the property",
		szTuName, uFailures);
}

// The plain form: a TU whose registrar registers only its own types.
inline void Zenith_CheckPinTableTotality(Zenith_GraphNodeRegistrarFn pfnRegistrar, const char* szTuName,
	const char* const* aszExemptProperties, u_int uExemptCount)
{
	Zenith_CheckPinTableTotalityEx(pfnRegistrar, nullptr, 0u, szTuName, aszExemptProperties, uExemptCount);
}

// Role/type spot-check support: the descriptor a node type declares under one
// pin NAME, resolved through the live registry (never an index literal, so a
// later pin insertion cannot make an assertion silently check a different pin).
inline const Zenith_GraphPinDesc* Zenith_FindGraphPin(const char* szTypeName, const char* szPinName)
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(szTypeName);
	ZENITH_ASSERT_NOT_NULL(pxInfo, "'%s' is not registered", szTypeName);
	if (pxInfo == nullptr || pxInfo->m_pfnGetPinTable == nullptr)
	{
		return nullptr;
	}
	return pxInfo->m_pfnGetPinTable()->FindPin(szPinName);
}

// One spot-check row: '<type>.<pin>' must exist with this role, this type and
// this var-name property. eExpectedType may be eGRAPH_PIN_TYPE_ANY.
inline void Zenith_CheckGraphPin(const char* szTypeName, const char* szPinName, Zenith_GraphPinRole eExpectedRole,
	Zenith_PropertyType eExpectedType, const char* szExpectedVarProperty)
{
	const Zenith_GraphPinDesc* pxDesc = Zenith_FindGraphPin(szTypeName, szPinName);
	ZENITH_ASSERT_NOT_NULL(pxDesc, "%s declares no pin named '%s'", szTypeName, szPinName);
	if (pxDesc == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxDesc->m_eRole), static_cast<int>(eExpectedRole),
		"%s.%s has the wrong ROLE - the validator would register the wrong reader/writer", szTypeName, szPinName);
	ZENITH_ASSERT_EQ(static_cast<int>(pxDesc->m_eType), static_cast<int>(eExpectedType),
		"%s.%s has the wrong TYPE", szTypeName, szPinName);
	ZENITH_ASSERT_STREQ(pxDesc->m_szVarNameProperty, szExpectedVarProperty,
		"%s.%s binds the wrong var-name property", szTypeName, szPinName);
}

#endif // ZENITH_TESTING
