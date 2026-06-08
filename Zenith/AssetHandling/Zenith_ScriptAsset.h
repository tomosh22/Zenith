#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include <string>

class Zenith_DataStream;
class Zenith_Entity;
class Zenith_ScriptBehaviour;

// Factory function pointer for creating a behaviour instance.
// std::function is FORBIDDEN per project conventions - use plain function pointers.
typedef Zenith_ScriptBehaviour* (*Zenith_ScriptBehaviourFactoryFn)(Zenith_Entity&);

/**
 * Zenith_ScriptAsset - Asset wrapper for a script behaviour type.
 *
 * Models Unity's "script as asset" pattern: a .zscript file lives alongside
 * other assets in the asset browser, can be drag-dropped onto entities,
 * and is referenced by scenes via its asset path.
 *
 * The .zscript file itself is a thin metadata file containing only the
 * behaviour type name. The actual behaviour code is the C++ class compiled
 * into the executable. Auto-registration via the ZENITH_BEHAVIOUR_TYPE_NAME
 * macro means new behaviours self-register at program startup.
 *
 * This class also owns the static factory map (replacing the deleted
 * Zenith_BehaviourRegistry singleton). Scripts look up their factory
 * function by behaviour type name when instantiating.
 */
class Zenith_ScriptAsset : public Zenith_Asset
{
public:
	Zenith_ScriptAsset() = default;

	// Used by static-init auto-registration when a behaviour creates its in-memory asset
	Zenith_ScriptAsset(const char* szBehaviourTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory);

	const char* GetBehaviourTypeName() const { return m_strBehaviourTypeName.c_str(); }

	// True if a factory is bound. False if the asset was loaded from disk
	// but no matching C++ behaviour is currently registered (orphan asset).
	bool IsRegistered() const { return m_pfnFactory != nullptr; }

	Zenith_ScriptBehaviour* CreateInstance(Zenith_Entity& xEntity) const;

	// Zenith_Asset overrides - uses .zdata-style serialization pipeline
	const char* GetTypeName() const override { return "ScriptAsset"; }
	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

	//--------------------------------------------------------------------
	// Static factory registry (replaces the deleted Zenith_BehaviourRegistry).
	// Game code does NOT call RegisterFactory directly - the
	// ZENITH_BEHAVIOUR_TYPE_NAME macro auto-registers via static inline init.
	//
	// "Internal" factories (test fixtures, runtime-only behaviours) register via
	// RegisterFactoryInternal / ZENITH_BEHAVIOUR_TYPE_NAME_INTERNAL. They behave
	// identically at runtime (LookupFactory finds them, scenes can attach them)
	// but are excluded from SyncRegisteredTypesToDisk so they don't pollute
	// game:Scripts/ with .zscript files for test-only behaviours.
	//--------------------------------------------------------------------
	static void RegisterFactory(const char* szTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory);
	static void RegisterFactoryInternal(const char* szTypeName, Zenith_ScriptBehaviourFactoryFn pfnFactory);
	static Zenith_ScriptBehaviourFactoryFn LookupFactory(const char* szTypeName);
	static bool HasFactory(const char* szTypeName);
	static bool IsInternalFactory(const char* szTypeName);
	static void GetAllRegisteredTypeNames(Zenith_Vector<std::string>& xOut);  // includes internals
	static void GetPublicRegisteredTypeNames(Zenith_Vector<std::string>& xOut);  // excludes internals

	// Deterministic asset path for a behaviour type name. Used by editor and EditorAutomation.
	// Returns "game:Scripts/<TypeName>.zscript" - no map lookup required.
	static std::string MakeAssetPath(const char* szTypeName);

#ifdef ZENITH_TOOLS
	// Called once at startup, AFTER static-init (so all factories are registered)
	// and BEFORE any scene load or EditorAutomation run.
	//   - mkdir game:Scripts/
	//   - For each registered factory: write game:Scripts/<TypeName>.zscript if absent
	//   - For each existing *.zscript on disk: if its behaviour name has no
	//     registered factory, rename to *.zscript.stale (do NOT delete)
	static void SyncRegisteredTypesToDisk();
#endif

private:
	std::string                     m_strBehaviourTypeName;
	Zenith_ScriptBehaviourFactoryFn m_pfnFactory = nullptr;

	// Function-local statics (construct-on-first-use) - safe under static init order.
	// The internal-name set lets SyncRegisteredTypesToDisk and editor enumeration
	// exclude test/runtime-only factories from the on-disk asset surface.
	// The internal-name "set" is a Zenith_HashMap<std::string, bool> used as a set:
	// presence (Contains) is the membership test; the bool value is an unused dummy.
	static Zenith_HashMap<std::string, Zenith_ScriptBehaviourFactoryFn>& GetFactoryMap();
	static Zenith_HashMap<std::string, bool>& GetInternalNameSet();
};
