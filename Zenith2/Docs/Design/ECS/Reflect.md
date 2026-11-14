# ECS design — ZenithReflect

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It specifies `Zenith2/Engine/ZenithReflect/`: the one reflection system of Zenith2 (`Zenith2/Docs/Contracts.md` → "Reflection"), with explicit registration, stable type and property IDs, schema versions, runtime type registration, the hot-reload window, computed layouts, lifecycle hooks, field sets and the reflection walk every consumer uses. The specification it elaborates is `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" (the registry, builder and error declarations there are kept member for member) and `Zenith2/Docs/Architecture/DataDefinedComponents.md`. The schema front end (`RegisterSchemaJson`, `RegisterSchemaBlob`, validation, `schemas.zreg`, schema migration, Luau type emission) is `Zenith2/Docs/Design/ECS/Schemas.md`.

Additions to the specification are marked `elaboration` in code comments and listed in `Zenith2/Docs/Design/ECS.md` → "Reconciliation items".

## Header list

| Header (`Zenith2/Engine/ZenithReflect/`) | Declares | Specified in |
|---|---|---|
| `Zenith_ReflectIDs.h` | `Zenith_TypeID`, `Zenith_PropertyID`, `Zenith_MakeTypeID`, the `Zenith_HashTraits` specialisations | § "Zenith_ReflectIDs header" |
| `Zenith_Phase.h` | `Zenith_Phase`, `uPHASE_COUNT`, `Zenith_GetPhaseName`, `Zenith_ParsePhaseName` | `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases" (verbatim) |
| `Zenith_NetworkAuthority.h` | `Zenith_NetworkAuthority` | `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" (verbatim) |
| `Zenith_Arbitration.h` | `Zenith_WriterSource`, `Zenith_ArbitrationRule`, `aeDEFAULT_ARBITRATION_PRECEDENCE`, `Zenith_ArbitrationSourceValue`, `Zenith_ArbitrationInputs`, `Zenith_PfnArbitrationMerge`, `Zenith_ArbitrationDecl` | `Zenith2/Docs/Architecture/FramePhases.md` → "Arbitration rules" (verbatim) plus § "Zenith_Arbitration header" |
| `Zenith_ReflectErrors.h` | `Zenith_ReflectErrorCode`, `Zenith_ReflectError`, `Zenith_ToString(Zenith_ReflectErrorCode)` | § "Zenith_ReflectErrors header" |
| `Zenith_TypeInfo.h` | `Zenith_TypeKind`, `Zenith_TypeOrigin`, `Zenith_PropertyValueType`, `Zenith_PropertyFlags`, `Zenith_PropertyEditorInfo`, `Zenith_PropertyInfo`, `Zenith_TypeLifecycleHooks`, `Zenith_TypeInfo` | § "Zenith_TypeInfo header" |
| `Zenith_ReflectValueTraits.h` | `Zenith_ReflectValueTraits<T>`, `Zenith_IsTriviallyRelocatable<T>`, `Zenith_MakeLifecycleHooks<T>` | § "Zenith_ReflectValueTraits header" |
| `Zenith_TypeLayout.h` | `Zenith_TypeLayoutField`, `Zenith_TypeLayout`, `Zenith_ComputeTypeLayout` | § "Zenith_TypeLayout header" |
| `Zenith_TypeRegistry.h` | `Zenith_ValueKindHooks`, `Zenith_TypeRegistry`, `Zenith_TypeBuilder`, `Zenith_PropertyBuilder`, `Zenith_RuntimeFieldDesc`, `Zenith_RuntimeTypeBuilder` | § "Zenith_TypeRegistry header" |
| `Zenith_FieldSet.h` | `Zenith_SerializedField`, `Zenith_SerializedFieldSet`, `Zenith_FieldSetError`, capture, apply and the two codecs | § "Zenith_FieldSet header" |
| `Zenith_ReflectionWalk.h` | `Zenith_PropertyPath`, `Zenith_PropertyVisit`, `Zenith_PfnPropertyVisitor`, `Zenith_WalkProperties`, `Zenith_ReadPropertyAt`, `Zenith_WritePropertyAt` | § "Zenith_ReflectionWalk header" |
| `Zenith_Migration.h` | `Zenith_MigrationContext` (Contracts' declaration), `Zenith_MigrationDiagnostics`, `Zenith_MigrationErrorCode`, `Zenith_MigrationError`, `Zenith_MigrationStep`, `Zenith_MigrationChain` | § "Zenith_Migration header" |
| `Zenith_Schema.h` | `Zenith_SchemaErrorCode`, `Zenith_SchemaDiagnostic`, `Zenith_SchemaError` | `Zenith2/Docs/Design/ECS/Schemas.md` → "Zenith_Schema header" |
| `Zenith_SchemaMigrator.h` | `Zenith_MigrateFieldSet` | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration" |
| `Zenith_SchemaBlob.h` | `Zenith_WriteSchemaBlob` | `Zenith2/Docs/Design/ECS/Schemas.md` → "The schema registry blob format" |
| `Zenith_LuauTypes.h` | `Zenith_EmitComponentLuauTypes` | `Zenith2/Docs/Design/ECS/Schemas.md` → "Luau type generation from schemas" |
| `Zenith_ScriptBinding.h` | the Luau-free binding ABI | owned by SCRIPT: `Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI"; it lives in this library and ECS tickets never edit it |

ZenithReflect is layer 1 (`DEPENDS ZenithCore`): it names no ECS, Jobs, Assets or Script type. Types owned by higher libraries (asset handles, entity handles, container references) reach it only through value-type enumerators, `Zenith_ReflectValueTraits` specialisations their owners write, and the value-kind hooks the composition root installs (§ "Lifecycle hooks").

## Zenith_ReflectIDs header

`Zenith_TypeID` and `Zenith_PropertyID` are exactly `Zenith2/Docs/Contracts.md` → "Reflection" (structs, no implicit conversion).

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ReflectIDs.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"
#include <compare>

/// @contract value Zenith_TypeID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None; stable for the life of the stable name it hashes.
/// Units: None.
/// Errors: None; zero is "no type".
/// Version: since Z2 Stage 2. The derivation is Zenith_Hash64 (D-SEL-HASH, open, Stage 1) of the stable name's UTF-8
///   bytes, seed 0; changing the hash is a re-cook of every schema blob and a save and replay version bump.
/// @endcontract
struct Zenith_TypeID
{
	uint64 m_ulHash = 0;
	friend constexpr bool operator==(Zenith_TypeID, Zenith_TypeID) = default;
	friend constexpr std::strong_ordering operator<=>(Zenith_TypeID, Zenith_TypeID) = default;
};

/// @contract value Zenith_PropertyID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None; never reused within its type ([Z2-N-CONTRACT-374]).
/// Units: None.
/// Errors: None; zero is "no property".
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_PropertyID
{
	uint32 m_uID = 0;
	friend constexpr bool operator==(Zenith_PropertyID, Zenith_PropertyID) = default;
	friend constexpr std::strong_ordering operator<=>(Zenith_PropertyID, Zenith_PropertyID) = default;
};

/// @contract functions Zenith_ReflectIDFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. elaboration
/// @endcontract
Zenith_TypeID Zenith_MakeTypeID(Zenith_StringView strStableName);   // { Zenith_Hash64(strStableName, 0) }
```

`Zenith_HashTraits<Zenith_TypeID>` and `Zenith_HashTraits<Zenith_PropertyID>` are specialised in this header (hash = `m_ulHash` itself for type IDs, `Zenith_Hash64` of the four little-endian bytes for property IDs) so `Zenith_HashMap` keys them by value (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"). Type IDs are used as map keys and sorted as integers; a stable-name byte order is used wherever an order is observable (§ "Lookup structures").

## Zenith_ReflectErrors header

`Zenith_ReflectErrorCode` and `Zenith_ReflectError` are `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" (table "Reflection error codes"); four codes are appended.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ReflectErrors.h
#pragma once
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract enum Zenith_ReflectErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: REFLECT_ERROR_TYPE_ID_COLLISION: two stable names hash to one ID. REFLECT_ERROR_DUPLICATE_STABLE_NAME.
///   REFLECT_ERROR_DUPLICATE_PROPERTY_ID. REFLECT_ERROR_MISSING_AUTHORITY: a component property with no authority.
///   REFLECT_ERROR_BAD_AUTHORITY: authority 11, 12 or PHASE_NONE on a property, or a resource whose owning phase is not
///   1 … 11 (phase 11 is admitted for resources alone: a phase-11-owned resource is a snapshot buffer).
///   REFLECT_ERROR_ARBITRATION_TYPE: WeightedBlend on a non-blendable type, a custom rule on a schema type, or an
///   arbitration naming an unknown property. REFLECT_ERROR_MIGRATION_CHAIN: a version with no step from its predecessor.
///   REFLECT_ERROR_MISSING_HOOKS: a type with lifetime-carrying properties and no hooks, or a value kind whose hooks
///   were never installed. REFLECT_ERROR_FROZEN: a registration after Freeze outside a window, or a second Freeze.
///   REFLECT_ERROR_HOT_RELOAD_KIND: a window change to a type kind it does not admit. REFLECT_ERROR_UNKNOWN_ELEMENT_TYPE:
///   a struct, container element or asset kind that is not registered. REFLECT_ERROR_LAYOUT: a component above 1,024
///   bytes or 16-byte alignment, or a C++ type whose registered properties overlap. REFLECT_ERROR_BAD_NAME: a stable or
///   script name that fails its grammar. REFLECT_ERROR_OUT_OF_MEMORY. REFLECT_ERROR_BAD_DEFAULT: a default that is not a
///   valid value of its property (wrong encoded size, an invalid value, encoded bytes on a STRING_ID property, a non-null
///   default on a C++ type's ASSET property), or an asset default the bound resolver cannot map when a hot-reload window
///   ends.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ReflectErrorCode : uint16
{
	REFLECT_ERROR_TYPE_ID_COLLISION = 1,
	REFLECT_ERROR_DUPLICATE_STABLE_NAME,
	REFLECT_ERROR_DUPLICATE_PROPERTY_ID,
	REFLECT_ERROR_MISSING_AUTHORITY,
	REFLECT_ERROR_BAD_AUTHORITY,
	REFLECT_ERROR_ARBITRATION_TYPE,
	REFLECT_ERROR_MIGRATION_CHAIN,
	REFLECT_ERROR_MISSING_HOOKS,
	REFLECT_ERROR_FROZEN,
	REFLECT_ERROR_HOT_RELOAD_KIND,
	REFLECT_ERROR_UNKNOWN_ELEMENT_TYPE,
	// elaboration: appended, values 12 to 15
	REFLECT_ERROR_LAYOUT,
	REFLECT_ERROR_BAD_NAME,
	REFLECT_ERROR_OUT_OF_MEMORY,
	REFLECT_ERROR_BAD_DEFAULT,
};

/// @contract value Zenith_ReflectError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: None.
/// Errors: The text naming both parties of a collision, or the missing piece, is logged at the point of failure
///   ([Z2-N-CONTRACT-464]) under the category ecs.reflect.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReflectError
{
	Zenith_ReflectErrorCode m_eCode;
	Zenith_PropertyID m_xProperty;
	Zenith_TypeID m_xType;
};
static_assert(sizeof(Zenith_ReflectError) <= 32);

constexpr const char* Zenith_ToString(Zenith_ReflectErrorCode eCode);
```

## Zenith_TypeInfo header

`Zenith_TypeKind`, `Zenith_TypeOrigin`, `Zenith_PropertyValueType`, `Zenith_PropertyInfo`, `Zenith_TypeLifecycleHooks` and `Zenith_TypeInfo` are the declarations of `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" (table "Registry types"), member for member; the additions are marked.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_TypeInfo.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithReflect/Zenith_Phase.h"
#include "ZenithReflect/Zenith_NetworkAuthority.h"

struct Zenith_ArbitrationDecl;
struct Zenith_MigrationChain;
struct Zenith_TypeLayout;

/// @contract enum Zenith_TypeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_TypeKind : uint8
{
	TYPE_KIND_COMPONENT, TYPE_KIND_STRUCT, TYPE_KIND_EVENT, TYPE_KIND_RESOURCE,
	TYPE_KIND_BEHAVIOUR_STATE, TYPE_KIND_CONFIG, TYPE_KIND_ASSET,
};

/// @contract enum Zenith_TypeOrigin
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; no consumer branches on it ([Z2-N-ARCH-643]); diagnostics and the inspector's provenance line read it.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_TypeOrigin : uint8 { TYPE_ORIGIN_CPP, TYPE_ORIGIN_SCHEMA };

/// @contract enum Zenith_PropertyValueType
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: The value encoding of each enumerator is the table "Value encodings".
/// Errors: None.
/// Version: since Z2 Stage 2. Values are part of the schemas.zreg format and never renumbered.
/// @endcontract
enum Zenith_PropertyValueType : uint8
{
	PROPERTY_TYPE_BOOL, PROPERTY_TYPE_I8, PROPERTY_TYPE_I16, PROPERTY_TYPE_I32, PROPERTY_TYPE_I64,
	PROPERTY_TYPE_U8, PROPERTY_TYPE_U16, PROPERTY_TYPE_U32, PROPERTY_TYPE_U64, PROPERTY_TYPE_F32, PROPERTY_TYPE_F64,
	PROPERTY_TYPE_VEC2, PROPERTY_TYPE_VEC3, PROPERTY_TYPE_VEC4, PROPERTY_TYPE_QUAT, PROPERTY_TYPE_COLOUR,
	PROPERTY_TYPE_STRING_ID, PROPERTY_TYPE_PERSISTENT_ID, PROPERTY_TYPE_ENTITY, PROPERTY_TYPE_ASSET,
	PROPERTY_TYPE_FIXED_ARRAY, PROPERTY_TYPE_STRUCT, PROPERTY_TYPE_CONTAINER,
	// elaboration: appended, value 23: a reflected C++ enumeration stored as its underlying integer (C++ types only)
	PROPERTY_TYPE_ENUM,
};

/// @contract enum Zenith_PropertyFlags
/// Ownership: Plain bit flags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bits.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_PropertyFlags : uint32
{
	PROPERTY_FLAG_NONE = 0,
	PROPERTY_FLAG_SERIALIZED = 1u << 0,             // default on; off = transient (saves, cooked scenes and replays skip it)
	PROPERTY_FLAG_REPLICATED = 1u << 1,
	PROPERTY_FLAG_RESTORE_AFTER_CINEMATIC = 1u << 2,
	PROPERTY_FLAG_EDITOR_HIDDEN = 1u << 3,
	// elaboration
	PROPERTY_FLAG_HAS_RANGE = 1u << 4,              // m_fMin and m_fMax apply
	PROPERTY_FLAG_CARRIES_LIFETIME = 1u << 5,       // set by the registry: ASSET, CONTAINER, or a struct or array containing one
	PROPERTY_FLAG_ARBITRATED = 1u << 6,             // set by the registry from the type's arbitration declaration
};

/// @contract value Zenith_PropertyEditorInfo
/// Ownership: Plain value; strings owned by the registry.
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: With its registry revision.
/// Units: The display range and step in the property's value units.
/// Errors: None.
/// Version: since Z2 Stage 2. elaboration: the schema's "editor" object and the C++ builder's editor calls.
/// @endcontract
struct Zenith_PropertyEditorInfo
{
	const char* m_szTooltip = nullptr;
	const char* m_szCategory = nullptr;
	double m_dDisplayMin = 0.0, m_dDisplayMax = 0.0, m_dStep = 0.0;   // all 0 = none declared
};

/// @contract value Zenith_PropertyInfo
/// Ownership: Owned by the registry; immutable after the revision that published it.
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: Retired two frames after a newer revision replaces its type (§ "Revisions and the hot-reload window").
/// Units: m_uOffset, m_uSize, m_uAlignment in bytes; m_uArrayCount and m_uMaxCount in elements.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_PropertyInfo
{
	Zenith_PropertyID m_xID;
	const char* m_szName;
	Zenith_PropertyValueType m_eType;
	Zenith_TypeID m_xElementType;          // struct, container element, fixed-array element struct, asset kind; zero otherwise
	uint32 m_uOffset;
	uint32 m_uSize;
	uint32 m_uAlignment;
	uint32 m_uArrayCount;                 // fixed arrays; 1 otherwise
	uint32 m_uMaxCount;                   // containers
	Zenith_Phase m_eAuthority;
	uint32 m_uFlags;
	Zenith_ByteSpan m_xDefault;            // in the value encoding
	// elaboration
	Zenith_PropertyValueType m_eElementValueType; // fixed arrays and containers: the element's value type
	uint32 m_uElementSize;                // fixed arrays and containers: bytes per element
	float m_fMin, m_fMax;                  // PROPERTY_FLAG_HAS_RANGE
	Zenith_PropertyEditorInfo m_xEditor;
	const char* m_szCppMemberName;         // null for schema types; diagnostics only
};

/// @contract value Zenith_TypeLifecycleHooks
/// Ownership: Plain value of function pointers into the registering library (C++ types) or into ZenithReflect (schema types);
///   m_pUserData is borrowed: null for C++ types, the type's registry-owned record for schema types ([Z2-N-ARCH-985]).
/// Thread affinity: IMMUTABLE; the hooks themselves are ANY_THREAD and follow the access rules of the storage they
///   touch. Members non-const: assembled field by field by the type builders and copied into the registry's tables.
/// Invalidation: None.
/// Units: uCount objects of the type's size, contiguous.
/// Errors: Hooks cannot fail; a hook that needs memory never allocates (containers are the ECS's, § "Lifecycle hooks").
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TypeLifecycleHooks
{
	void (*m_pfnConstruct)(void* pDestination, uint32 uCount, void* pUserData);
	void (*m_pfnDestroy)(void* pObjects, uint32 uCount, void* pUserData);
	void (*m_pfnCopy)(void* pDestination, const void* pSource, uint32 uCount, void* pUserData);
	void (*m_pfnMove)(void* pDestination, void* pSource, uint32 uCount, void* pUserData);
	void* m_pUserData;                     // passed back as every hook's pUserData; the registry stores it verbatim and never dereferences it
};

/// @contract value Zenith_TypeInfo
/// Ownership: Owned by the registry; IMMUTABLE after Zenith_TypeRegistry::Freeze or after the window that published it.
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: A pointer stays valid for two frames after a newer revision replaces the type, then is retired
///   ([Z2-N-ARCH-651]).
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TypeInfo
{
	Zenith_TypeID m_xID;
	const char* m_szStableName;
	const char* m_szScriptName;
	uint32 m_uVersion;
	Zenith_TypeKind m_eKind;
	Zenith_TypeOrigin m_eOrigin;
	uint32 m_uSize;
	uint32 m_uAlignment;
	Zenith_Span<const Zenith_PropertyInfo> m_axProperties;   // ascending property ID
	Zenith_TypeLifecycleHooks m_xHooks;                      // trivial hooks are null
	Zenith_Phase m_eResourceOwningPhase;
	Zenith_NetworkAuthority m_eNetworkAuthority;
	const Zenith_ArbitrationDecl* m_pxArbitration;
	const Zenith_MigrationChain* m_pxMigrations;
	// elaboration
	const Zenith_TypeLayout* m_pxLayout;                    // computed for schema types; recorded offsets for C++ types
	Zenith_Span<const Zenith_PropertyID> m_axRetiredIDs;    // never reusable
	uint64 m_ulRevision;                                   // the registry revision that published this TypeInfo
	uint64 m_ulShapeHash;                                  // Zenith_Hash64 over the canonical shape (§ "Shape hash")
	const char* m_szSourcePath;                             // schema file or "cpp:<registering library>"
	// elaboration: the construction default object (§ "C++ hooks"; a trivially copyable C++ type starts from its T{} bytes):
	// m_uSize bytes holding every property's declared default
	// at its offset in its value encoding, STRUCT fields from the element type's default object and FIXED_ARRAY elements
	// element by element, padding zero, ASSET elements null until ResolveReferenceDefaults writes the resolved handles;
	// built at End, owned by the registry record, valid as long as this TypeInfo
	Zenith_ByteSpan m_xDefaultObject;
	Zenith_ByteSpan GetDefaultObject() const { return m_xDefaultObject; }
};
```

Value encodings — the bytes of a `Zenith_PropertyValue`, a default, a command payload and a field-set entry, independent of every storage layout ([Z2-N-ARCH-717]):

| Value type | Bytes | Encoding |
|---|---|---|
| `BOOL` | 1 | 0 or 1; any other byte is rejected (ECS_ERROR_VALUE_SIZE is not used: the reader returns `FIELD_SET_ERROR_BAD_VALUE`) |
| `I8`…`I64`, `U8`…`U64` | 1, 2, 4, 8 | little-endian two's complement or unsigned |
| `F32`, `F64` | 4, 8 | IEEE 754 binary32 or binary64, little-endian |
| `VEC2`, `VEC3`, `VEC4` | 8, 12, 16 | consecutive `F32` components x, y, z, w |
| `QUAT` | 16 | `F32` x, y, z, w |
| `COLOUR` | 16 | `F32` linear r, g, b, a |
| `STRING_ID` | 4 | the process-local `Zenith_StringID` value in memory values; field sets and saves carry the text instead (§ "Field-set codecs") |
| `PERSISTENT_ID` | 16 | high word then low word, each little-endian ([Z2-N-CONTRACT-067]) |
| `ENTITY` | 8 | the `Zenith_EntityHandle` bits in memory values; field sets carry the referenced entity's persistent ID (16 bytes) |
| `ASSET` | 16 | the `Zenith_AssetHandle<K>` bytes in memory values; field sets carry the asset's persistent ID (16 bytes) |
| `ENUM` | 1, 2 or 4 | the underlying integer, little-endian |
| `FIXED_ARRAY` | N × element bytes | the elements' encodings, index order |
| `STRUCT` | the struct's layout size | the struct's fields at their layout offsets, each in its own encoding, padding bytes zero |
| `CONTAINER` | 8 in memory (`Zenith_ContainerRef`) | field sets carry a `uint32` count then the elements' encodings |

Every target of Zenith2 is little-endian (`Zenith2/Docs/Design/CORE/Maths.md` → "Vector types"), so the in-memory encoding of scalars and vectors is their storage bytes; the table fixes the order explicitly so field sets stay layout- and platform-independent.

## Zenith_ReflectValueTraits header

<!-- elaboration -->
How a C++ member type maps to a value type, and how C++ types get their hooks. The primary template covers the ZenithCore types (`bool`, the sized integers, `float`, `double`, `Zenith_Vector2`/`3`/`4`, `Zenith_Quaternion`, `Zenith_Colour`, `Zenith_StringID`, `Zenith_PersistentID`, fixed arrays `T[N]`, reflected enums); higher libraries specialise it next to their types (ZenithECS for `Zenith_EntityHandle` and `Zenith_ContainerRef`, ZenithAssets for `Zenith_AssetHandle<K>`), exactly as the binding ABI's `Zenith_ScriptValueTraits` is specialised.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ReflectValueTraits.h
#pragma once
#include <type_traits>
#include "ZenithReflect/Zenith_TypeInfo.h"

/// @contract value Zenith_ReflectValueTraits
/// Ownership: A compile-time traits template; owns nothing.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: An unspecialised, unsupported member type fails a static_assert in Zenith_TypeBuilder::Property naming it.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename T>
struct Zenith_ReflectValueTraits
{
	static constexpr bool bSUPPORTED = false;
	// A specialisation declares:
	//   static constexpr Zenith_PropertyValueType eTYPE;
	//   static constexpr bool bCARRIES_LIFETIME;        // ASSET and CONTAINER specialisations: true
	//   static Zenith_TypeID ElementType();      // asset kind, struct or element type; zero otherwise
};

/// @contract value Zenith_IsTriviallyRelocatable
/// Ownership: A compile-time trait.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: BeginComponent<T> static_asserts it (§ "Lifecycle hooks": backends relocate component rows bytewise).
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename T>
struct Zenith_IsTriviallyRelocatable
{
	static constexpr bool bVALUE = std::is_trivially_copyable_v<T> || T::bZENITH_TRIVIALLY_RELOCATABLE;
};

/// @contract functions Zenith_MakeLifecycleHooks
/// Ownership: Returns function pointers instantiated in the including (registering) library.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename T>
constexpr Zenith_TypeLifecycleHooks Zenith_MakeLifecycleHooks();   // null members for trivially copyable T
```

A C++ component that holds a `Zenith_AssetHandle` or a `Zenith_ContainerRef` is not trivially copyable; it declares `static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;`, which asserts that moving its bytes and forgetting the source is a valid move (no member points into the object). `Zenith_MakeLifecycleHooks<T>` instantiates construct (value-initialise), destroy (`~T`), copy (copy-construct) and move (bytewise relocation) for non-trivially-copyable `T`.

## Zenith_TypeLayout header

`Zenith_TypeLayout` is named by `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") as ECS's; the algorithm is § "Layout algorithm".

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_TypeLayout.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_TypeInfo.h"

/// @contract value Zenith_TypeLayoutField
/// Ownership: Plain value owned by its layout.
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: With its layout.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TypeLayoutField
{
	Zenith_PropertyID m_xProperty;
	uint32 m_uOffset;
	uint32 m_uSize;
	uint32 m_uAlignment;
};

/// @contract value Zenith_TypeLayout
/// Ownership: Owned by the registry (one per type revision); never written into any file ([Z2-N-ARCH-597]).
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: Retired with its TypeInfo.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TypeLayout
{
	uint32 m_uSize;
	uint32 m_uAlignment;
	Zenith_Span<const Zenith_TypeLayoutField> m_axFields;   // in placement order
	bool m_bTriviallyCopyable;                              // no field carries lifetime
};

/// @contract functions Zenith_ComputeTypeLayoutFunction
/// Ownership: Writes into caller-provided storage.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Returns false when the size would exceed uMaxBytes; the caller reports REFLECT_ERROR_LAYOUT or SCHEMA_ERROR_LIMIT.
/// Version: since Z2 Stage 2.
/// @endcontract
bool Zenith_ComputeTypeLayout(Zenith_Span<const Zenith_PropertyInfo> axProperties, uint32 uMaxBytes,
	Zenith_Span<Zenith_TypeLayoutField> axOut, uint32& uSizeOut, uint32& uAlignmentOut);
```

## Zenith_TypeRegistry header

`Zenith_TypeRegistry`, `Zenith_TypeBuilder` and `Zenith_PropertyBuilder` are `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" (table "Registration API"), member for member; the additions are marked.

### Zenith_TypeRegistry

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_TypeRegistry.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithReflect/Zenith_TypeInfo.h"
#include "ZenithReflect/Zenith_ReflectErrors.h"
#include "ZenithReflect/Zenith_Schema.h"
#include "ZenithReflect/Zenith_Arbitration.h"
#include "ZenithReflect/Zenith_Migration.h"
#include "ZenithReflect/Zenith_ReflectValueTraits.h"

class Zenith_Heap;
class Zenith_StringTable;
class Zenith_ConfigSchema;           // ZenithCore/Zenith_ConfigStore.h
class Zenith_FaultInjector;
class Zenith_TypeBuilder;
class Zenith_RuntimeTypeBuilder;

// elaboration: builds an element (an asset handle) from 16 persistent-ID bytes of an asset of kind xKind; returns false
// and leaves the element null when the ID names no such asset. pUserData is the resolver's owner (the asset registry).
using Zenith_PfnConstructFromPersistentID = bool (*)(void* pElement, const uint8* pPersistent, Zenith_TypeID xKind,
	void* pUserData);

/// @contract value Zenith_ValueKindHooks
/// Ownership: Plain value of function pointers into the library that owns the value kind (ZenithAssets for ASSET);
///   m_pUserData is borrowed and outlives the registry.
/// Thread affinity: IMMUTABLE; the lifecycle hooks are ANY_THREAD; m_pfnConstructFromPersistentID is called only by
///   ResolveReferenceDefaults and EndHotReload, on the main thread. Members non-const: InstallValueKindHooks copies it
///   over an entry of the registry's per-value-kind table.
/// Invalidation: None.
/// Units: One element per call; uElementBytes bytes each.
/// Errors: The lifecycle hooks cannot fail; m_pfnConstructFromPersistentID returns false for an ID it cannot map.
/// Version: since Z2 Stage 2. elaboration
/// @endcontract
struct Zenith_ValueKindHooks
{
	void (*m_pfnConstructNull)(void* pElement);
	void (*m_pfnDestroy)(void* pElement);
	void (*m_pfnCopy)(void* pDestination, const void* pSource);
	uint32 m_uElementBytes;
	// Reference defaults (Schemas.md, "Default values"): null in the hooks installed at startup step 10; bound by
	// ResolveReferenceDefaults once the resolver's owner exists.
	Zenith_PfnConstructFromPersistentID m_pfnConstructFromPersistentID = nullptr;
	void* m_pUserData = nullptr;
};

/// @contract type Zenith_TypeRegistry
/// Ownership: Created by Create as a Zenith_Owned<Zenith_TypeRegistry>; owned by the composition root (startup step 10)
///   and borrowed by every consumer; storage from xHeap (type records, properties, names, layouts, defaults, the ID and
///   name indexes), retired revisions released two frames after replacement.
/// Thread affinity: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards, except inside a hot-reload window,
///   which ZenithRuntime alone opens on the main thread in frame-prologue step 6 while no job runs.
///   Declarations are accepted from Create onward, before a string table is bound (the declaration phase of startup
///   step 6, Zenith2/Docs/Design/MOD.md → "Configuration-key registration"); GetConfigSchema is valid while the attached
///   configuration schema is not frozen. ResolveReferenceDefaults is the one write to a FROZEN registry outside a window:
///   once per value kind, on the main thread, with no window open (startup step 11, or the cook after its Freeze); it
///   writes only construction default objects no reader can yet reach, because Zenith_WorldDesc validation refuses every
///   world while a non-null reference default is unresolved.
/// Invalidation: TypeInfo pointers of a replaced revision stay valid for two further frames ([Z2-N-ARCH-651]).
/// Units: Revisions count from 1 (the first Freeze).
/// Errors: Zenith_Result<…, Zenith_ReflectError> or Zenith_SchemaError; every error is logged at the point of failure;
///   Freeze and EndHotReload report every error of the pass. A C++ registration (BeginComponent, BeginType) after Freeze
///   outside a window is ZENITH_CHECK ([Z2-N-CONTRACT-378]); a runtime registration then returns REFLECT_ERROR_FROZEN
///   from End, and a schema registration returns SCHEMA_ERROR_REGISTRY_FROZEN (Schemas.md, "Zenith_Schema header").
///   BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …) before BindStringTable (the step-6 declaration phase) is ZENITH_CHECK:
///   behaviour-state types register at startup step 10 alone (Zenith2/Docs/Design/ECS.md → "Behaviour-state type
///   registration"). ResolveReferenceDefaults before Freeze, inside a window, or a second time for one value kind is
///   ZENITH_CHECK.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_TypeRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_TypeRegistry>, Zenith_ReflectError> Create(Zenith_Heap& xHeap);

	template<typename T>
	Zenith_TypeBuilder& BeginComponent(const char* szStableName, uint32 uVersion);
	template<typename T>
	Zenith_TypeBuilder& BeginType(Zenith_TypeKind eKind, const char* szStableName, uint32 uVersion);

	Zenith_Result<void, Zenith_SchemaError> RegisterSchemaJson(Zenith_ByteSpan xJsonBytes, Zenith_StringView strSourcePath);
	Zenith_Result<void, Zenith_SchemaError> RegisterSchemaBlob(Zenith_ByteSpan xBlobBytes);
	Zenith_Span<const Zenith_SchemaDiagnostic> GetSchemaDiagnostics() const;

	Zenith_Result<void, Zenith_ReflectError> Freeze();
	bool IsFrozen() const;

	Zenith_Result<void, Zenith_ReflectError> BeginHotReload();
	Zenith_Result<uint64, Zenith_ReflectError> EndHotReload();
	void AbandonHotReload();
	uint64 GetRevision() const;

	const Zenith_TypeInfo* FindType(Zenith_TypeID xID) const;
	const Zenith_TypeInfo* FindTypeByStableName(Zenith_StringView strName) const;
	Zenith_Span<const Zenith_TypeInfo* const> GetTypes() const;

	// elaboration
	void BindStringTable(Zenith_StringTable& xStrings);          // startup step 10: interns every string default declared so far
	void AttachConfigSchema(Zenith_ConfigSchema& xSchema);       // startup step 6, before engine and module declarations
	Zenith_ConfigSchema& GetConfigSchema();        // the attached schema; ZENITH_CHECK when none or frozen
	bool IsStringTableBound() const;
	void InstallValueKindHooks(Zenith_PropertyValueType eType, const Zenith_ValueKindHooks& xHooks);   // before the first schema
	// Binds the value kind's resolver and resolves every non-null reference default of every registered type into its
	// construction default object; every unresolvable default is a SCHEMA_ERROR_BAD_DEFAULT diagnostic (startup step 11,
	// the cook after Freeze). Main thread, FROZEN with no window open, before any world exists; a call in any other state, or
	// a second call for one value kind whatever the first returned, is ZENITH_CHECK.
	Zenith_Result<void, Zenith_SchemaError> ResolveReferenceDefaults(Zenith_PropertyValueType eType,
		Zenith_PfnConstructFromPersistentID pfnConstruct, void* pUserData);
	bool AreReferenceDefaultsResolved() const;   // true when no non-null reference default is unresolved
	Zenith_RuntimeTypeBuilder& BeginRuntimeType(Zenith_TypeKind eKind, Zenith_StringView strStableName,
		Zenith_StringView strScriptName, uint32 uVersion);       // behaviour state types and the schema path
	const Zenith_PropertyInfo* FindProperty(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	const Zenith_TypeInfo* FindTypeByScriptName(Zenith_StringView strScriptName) const;
	bool IsInHotReloadWindow() const;
	Zenith_ReflectError GetLastReflectError() const;
	Zenith_Span<const Zenith_TypeInfo* const> GetTypesOfKind(Zenith_TypeKind eKind) const;   // byte order of stable name
	void RetireFrame();                                           // frame prologue: advances the two-frame retirement
	void SetFaultInjector(Zenith_FaultInjector* pxFaults);        // tests
};

inline constexpr uint32 uFAULT_POINT_TYPE_REGISTRY_INDEX = 0x52460001u;    // the ID index at Freeze
inline constexpr uint32 uFAULT_POINT_TYPE_REGISTRY_REVISION = 0x52460002u; // publication in EndHotReload
```

### Zenith_TypeBuilder

```cpp
/// @contract type Zenith_TypeBuilder
/// Ownership: Owned by the registry; returned by reference from BeginComponent and BeginType.
/// Thread affinity: OWNER_THREAD; valid until End().
/// Invalidation: Invalid after End() and after the next Begin call on the same registry.
/// Units: None.
/// Errors: Declarations are recorded; errors are reported by Freeze (or EndHotReload) with the codes of
///   Zenith_ReflectErrorCode, never by the call. A property member whose type has no Zenith_ReflectValueTraits fails a
///   static_assert.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_TypeBuilder
{
public:
	template<typename TOwner, typename TField>
	Zenith_PropertyBuilder& Property(Zenith_PropertyID xID, const char* szName, TField TOwner::* pMember);
	Zenith_TypeBuilder& DefaultAuthority(Zenith_Phase ePhase);
	Zenith_TypeBuilder& NetworkAuthority(Zenith_NetworkAuthority eAuthority);
	Zenith_TypeBuilder& Arbitration(Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_ArbitrationRule eRule,
		Zenith_Span<const Zenith_WriterSource> axOrder);
	Zenith_TypeBuilder& ArbitrationCustom(Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_PfnArbitrationMerge pfnMerge);
	Zenith_TypeBuilder& Migration(uint32 uFromVersion, Zenith_PfnMigrationStep pfnStep);
	Zenith_TypeBuilder& ScriptName(const char* szName);
	void End();
	// elaboration
	Zenith_TypeBuilder& RetiredProperty(Zenith_PropertyID xID);
	Zenith_TypeBuilder& ResourceOwningPhase(Zenith_Phase ePhase);       // TYPE_KIND_RESOURCE
};
```

`BeginComponent<T>` records `sizeof(T)`, `alignof(T)`, `Zenith_MakeLifecycleHooks<T>()` and `static_assert`s `sizeof(T) <= uECS_MAX_COMPONENT_BYTES`-equivalent 1,024, `alignof(T) <= 16` and `Zenith_IsTriviallyRelocatable<T>::bVALUE`. `Property` records `offsetof`-equivalent offsets computed from `pMember` against a value-initialised `T` in a stack buffer (no `offsetof` on non-standard-layout types), the member's size and alignment and its value type from the traits. When `T` has no script name, `ScriptName` defaults to the Hungarian mapping of `Zenith2/Docs/Scripting/Bindings.md` → "Name mapping from Zenith Hungarian" applied to the stable name's last segment. A type a module registers from C++ never relies on this default: it sets an explicit script name, unique programme-wide and derived from its C++ type name by rule 1 of that mapping (`Zenith2/Docs/Architecture.md` → "Registration API", [Z2-N-ARCH-652]; ruling X353-SCRIPTNAMES), so two modules' types cannot clash with `REFLECT_ERROR_BAD_NAME` at `Freeze`.

### Zenith_PropertyBuilder

```cpp
/// @contract type Zenith_PropertyBuilder
/// Ownership: Owned by the registry.
/// Thread affinity: OWNER_THREAD; valid until its type builder's End().
/// Invalidation: As Zenith_TypeBuilder.
/// Units: Range in the property's value units.
/// Errors: As Zenith_TypeBuilder.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_PropertyBuilder
{
public:
	Zenith_PropertyBuilder& Authority(Zenith_Phase ePhase);
	Zenith_PropertyBuilder& Flags(uint32 uPropertyFlags);
	Zenith_PropertyBuilder& Default(Zenith_ByteSpan xValue);
	Zenith_PropertyBuilder& Range(float fMin, float fMax);
	Zenith_PropertyBuilder& MaxCount(uint32 uMaxCount);
	Zenith_PropertyBuilder& ScriptName(const char* szName);
	Zenith_PropertyBuilder& RestoreAfterCinematic();
	Zenith_TypeBuilder& EndProperty();
	// elaboration
	Zenith_PropertyBuilder& Editor(const Zenith_PropertyEditorInfo& xInfo);
	Zenith_PropertyBuilder& DefaultText(Zenith_StringView strText);   // STRING_ID properties: interned at BindStringTable
};
```

### Zenith_RuntimeTypeBuilder

<!-- elaboration -->
The descriptor-based registration path. The schema front end builds every data-defined type through it, and each `Behaviour.<Name>.State` type is registered through it ([Z2-N-SCRIPT-382]) at startup step 10 from the cooked script manifest, or after `Freeze` only inside the hot-reload window (§ "Registration algorithm" item 5); both therefore produce `Zenith_TypeInfo` exactly as C++ registration does, differing only in `m_eOrigin` and in the layout being computed rather than recorded.

```cpp
/// @contract value Zenith_RuntimeFieldDesc
/// Ownership: Plain value; strings and spans copied by AddField.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after the call.
/// Units: Bytes and elements.
/// Errors: Validated at End (and again at Freeze).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RuntimeFieldDesc
{
	const Zenith_PropertyID m_xID = {};
	const Zenith_StringView m_strName = {};
	const Zenith_PropertyValueType m_eType = PROPERTY_TYPE_F32;
	const Zenith_PropertyValueType m_eElementValueType = PROPERTY_TYPE_F32; // FIXED_ARRAY and CONTAINER
	const Zenith_TypeID m_xElementType = {};                            // struct, asset kind
	const uint32 m_uArrayCount = 1;
	const uint32 m_uMaxCount = 0;
	const Zenith_Phase m_eAuthority = PHASE_NONE;                       // PHASE_NONE = the type default
	const uint32 m_uFlags = PROPERTY_FLAG_SERIALIZED;
	const Zenith_ByteSpan m_xDefault = {};                              // empty = the type's zero value
	const Zenith_StringView m_strDefaultText = {};                      // STRING_ID defaults: interned at End
	const float m_fMin = 0.0f, m_fMax = 0.0f;
	const Zenith_PropertyEditorInfo m_xEditor = {};
};

/// @contract type Zenith_RuntimeTypeBuilder
/// Ownership: Owned by the registry; returned by reference from BeginRuntimeType.
/// Thread affinity: OWNER_THREAD; valid until End() or Abandon().
/// Invalidation: Invalid after End() or Abandon().
/// Units: None.
/// Errors: End returns the first error of the type (REFLECT_ERROR_* codes) and registers nothing for it; the text is logged.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_RuntimeTypeBuilder
{
public:
	Zenith_RuntimeTypeBuilder& AddField(const Zenith_RuntimeFieldDesc& xField);
	Zenith_RuntimeTypeBuilder& DefaultAuthority(Zenith_Phase ePhase);
	Zenith_RuntimeTypeBuilder& NetworkAuthority(Zenith_NetworkAuthority eAuthority);
	Zenith_RuntimeTypeBuilder& Arbitration(Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_ArbitrationRule eRule,
		Zenith_Span<const Zenith_WriterSource> axOrder);           // Precedence or WeightedBlend only
	Zenith_RuntimeTypeBuilder& RetiredFields(Zenith_Span<const Zenith_PropertyID> axIDs);
	Zenith_RuntimeTypeBuilder& MigrationChain(const Zenith_MigrationChain& xChain);   // copied
	Zenith_RuntimeTypeBuilder& SourcePath(Zenith_StringView strPath);
	Zenith_RuntimeTypeBuilder& Origin(Zenith_TypeOrigin eOrigin);   // TYPE_ORIGIN_SCHEMA for schemas and behaviour state
	Zenith_Result<Zenith_TypeID, Zenith_ReflectError> End();
	void Abandon();
};
```

## Zenith_Arbitration header

The declarations of `Zenith2/Docs/Architecture/FramePhases.md` → "Arbitration rules" (`Zenith_WriterSource`, `Zenith_ArbitrationRule`, `aeDEFAULT_ARBITRATION_PRECEDENCE`, `Zenith_ArbitrationSourceValue`, `Zenith_ArbitrationInputs`, `Zenith_PfnArbitrationMerge`, `Zenith_ArbitrationDecl`) are this header verbatim, each carrying a contract block (enums `IMMUTABLE`; `Zenith_ArbitrationInputs` `IMMUTABLE for the duration of one merge call; sources in the declared order`; `Zenith_ArbitrationDecl` owned by the registry, referenced from `Zenith_TypeInfo::m_pxArbitration`). One helper is added:

```cpp
/// @contract functions Zenith_ArbitrationHelpers
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. elaboration
/// @endcontract
bool Zenith_IsBlendableValueType(Zenith_PropertyValueType eType);   // F32, F64, VEC2, VEC3, VEC4, QUAT, COLOUR
const char* Zenith_GetWriterSourceName(Zenith_WriterSource eSource);  // "Script", "Animation", "Physics", "Ragdoll", "Cinematic"
Zenith_WriterSource Zenith_ParseWriterSourceName(Zenith_StringView strName);   // WRITER_SOURCE_NONE if unknown
```

The merge itself (per row, collected sources, precedence, weighted blend with quaternion sign alignment) runs in ZenithECS: `Zenith2/Docs/Design/ECS/Scheduling.md` → "Arbitration merge".

## Zenith_FieldSet header

`Zenith_SerializedFieldSet` is named by `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") as ECS's: a property-ID-keyed value set, the neutral form every serializer, saves, replication payloads and schema migration use, so no persistent form depends on a layout ([Z2-N-ARCH-644], Z2-D-038).

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_FieldSet.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_TypeInfo.h"

class Zenith_Arena;
class Zenith_StringTable;
class Zenith_JsonWriter;
class Zenith_JsonDocument;
struct Zenith_JsonNode;

/// @contract enum Zenith_FieldSetError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FIELD_SET_ERROR_TRUNCATED, FIELD_SET_ERROR_BAD_VALUE (a value outside its encoding), FIELD_SET_ERROR_TOO_MANY
///   (above 128 entries per level), FIELD_SET_ERROR_DEPTH (struct nesting above 4), FIELD_SET_ERROR_OUT_OF_MEMORY,
///   FIELD_SET_ERROR_UNRESOLVED (an entity or asset reference the resolver could not map; the field keeps its default).
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_FieldSetError : uint8
{
	FIELD_SET_ERROR_TRUNCATED = 1,
	FIELD_SET_ERROR_BAD_VALUE,
	FIELD_SET_ERROR_TOO_MANY,
	FIELD_SET_ERROR_DEPTH,
	FIELD_SET_ERROR_OUT_OF_MEMORY,
	FIELD_SET_ERROR_UNRESOLVED,
};

/// @contract value Zenith_SerializedField
/// Ownership: Plain value; m_xValue and m_axChildren live in the field set's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its field set.
/// Units: Bytes of the field-set encoding (§ "Field-set codecs").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SerializedField
{
	const Zenith_PropertyID m_xProperty = {};
	const Zenith_PropertyValueType m_eType = {};
	const Zenith_ByteSpan m_xValue = {};                        // scalar, vector, string text, persistent IDs, arrays of those
	const Zenith_Span<const struct Zenith_SerializedFieldSet> m_axChildren = {}; // STRUCT: one; FIXED_ARRAY or CONTAINER of structs: one per element
};

/// @contract type Zenith_SerializedFieldSet
/// Ownership: Borrowed spans into an arena the caller owns; one level of a (possibly nested) value.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when its arena is reset.
/// Units: Entries in ascending property ID.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SerializedFieldSet
{
	Zenith_TypeID m_xType;
	uint32 m_uVersion = 0;                                    // the schema version the set was written at
	Zenith_Span<const Zenith_SerializedField> m_axFields;       // ascending property ID; unknown IDs kept (unknown-field preservation)
};

/// @contract interface Zenith_ReferenceResolver
/// Ownership: Implemented by the caller (SCENE, SAVE, the world inspector's users); borrowed for one call.
/// Thread affinity: The calling thread.
/// Invalidation: None.
/// Units: None.
/// Errors: A reference it cannot map returns false; the field is set to the null value and FIELD_SET_ERROR_UNRESOLVED is
///   reported after the rest of the set is applied.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ReferenceResolver
{
public:
	virtual ~Zenith_ReferenceResolver() = default;
	virtual bool EntityToPersistent(const void* pEntityBits, uint8* pPersistentOut) = 0;
	virtual bool PersistentToEntity(const uint8* pPersistent, void* pEntityBitsOut) = 0;
	virtual bool AssetToPersistent(const void* pAssetBytes, uint8* pPersistentOut) = 0;
	virtual bool PersistentToAsset(const uint8* pPersistent, Zenith_TypeID xAssetKind, void* pAssetBytesOut) = 0;
	virtual Zenith_ByteSpan ContainerElements(const void* pContainerRef, uint32& uCountOut) = 0;
	virtual bool StoreContainer(Zenith_ByteSpan xElements, uint32 uCount, void* pContainerRefOut) = 0;
};

/// @contract functions Zenith_FieldSetFunctions
/// Ownership: Output field sets are allocated in xArena; objects are the caller's.
/// Thread affinity: The calling thread; the type registry is read-only.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_Result<…, Zenith_FieldSetError>; untrusted input never asserts ([Z2-N-CONTRACT-472]).
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<Zenith_SerializedFieldSet, Zenith_FieldSetError> Zenith_CaptureFieldSet(const Zenith_TypeInfo& xType,
	const void* pObject, const Zenith_StringTable& xStrings, Zenith_ReferenceResolver& xResolver, bool bSerializedOnly,
	Zenith_Arena& xArena);
Zenith_Result<void, Zenith_FieldSetError> Zenith_ApplyFieldSet(const Zenith_TypeInfo& xType,
	const Zenith_SerializedFieldSet& xSet, Zenith_StringTable& xStrings, Zenith_ReferenceResolver& xResolver, void* pObject);
Zenith_Result<uint32, Zenith_FieldSetError> Zenith_EncodeFieldSet(const Zenith_SerializedFieldSet& xSet,
	Zenith_MutableByteSpan xOut);                        // returns bytes written; FIELD_SET_ERROR_TRUNCATED when xOut is short
uint32 Zenith_GetEncodedFieldSetBytes(const Zenith_SerializedFieldSet& xSet);
Zenith_Result<Zenith_SerializedFieldSet, Zenith_FieldSetError> Zenith_DecodeFieldSet(Zenith_ByteSpan xIn,
	Zenith_Arena& xArena, uint32& uBytesReadOut);
void Zenith_WriteFieldSetJson(const Zenith_SerializedFieldSet& xSet, const Zenith_TypeRegistry& xTypes, Zenith_JsonWriter& xWriter);
Zenith_Result<Zenith_SerializedFieldSet, Zenith_FieldSetError> Zenith_ReadFieldSetJson(const Zenith_JsonDocument& xDocument,
	Zenith_JsonNode xObject, const Zenith_TypeInfo& xType, Zenith_Arena& xArena);
```

`Zenith_ApplyFieldSet` sets every property the set names; a property the set lacks keeps the object's current value (the caller constructs defaults first); an entry whose ID the type does not know is skipped and counted for the `MIGRATION_WARNING_UNKNOWN_FIELD` warning its caller reports ([Z2-N-CONTRACT-544]); a set whose `m_uVersion` differs from the type's version is migrated first by the caller through `Zenith_MigrateFieldSet` (`Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration").

## Zenith_ReflectionWalk header

The walk the inspector (EDIT), serializers (SCENE, SAVE), replication (NET) and generators use: every consumer reaches properties through it or through `Zenith_PropertyInfo` directly, never through a C++ type ([Z2-N-ARCH-643]).

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ReflectionWalk.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_TypeInfo.h"

class Zenith_TypeRegistry;

/// @contract value Zenith_PropertyPath
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a walk extends and truncates the path in place as it descends.
/// Invalidation: None.
/// Units: Up to 5 property IDs (depth ≤ 4 structs below the component) and one element index per level.
/// Errors: None.
/// Version: since Z2 Stage 2. The path an override is keyed by ([Z2-N-CONTRACT-546]).
/// @endcontract
struct Zenith_PropertyPath
{
	Zenith_PropertyID m_axIDs[5] = {};
	uint32 m_auElementIndex[5] = {};     // 0xFFFFFFFF = not an array or container element
	uint8 m_uDepth = 0;
};

/// @contract value Zenith_PropertyVisit
/// Ownership: Plain value valid for one visitor call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_pValue is invalid after the call returns.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_PropertyVisit
{
	const Zenith_TypeInfo* const m_pxOwner = nullptr; // the struct or component declaring the property
	const Zenith_PropertyInfo* const m_pxProperty = nullptr;
	const Zenith_PropertyPath m_xPath = {};
	const void* const m_pValue = nullptr;      // the property's bytes in the walked object (null for a container's elements)
	const uint32 m_uElementCount = 0;         // FIXED_ARRAY: N; CONTAINER: the current count (from the resolver); 1 otherwise
};

using Zenith_PfnPropertyVisitor = bool (*)(const Zenith_PropertyVisit& xVisit, void* pUserData);   // false stops

/// @contract functions Zenith_ReflectionWalkFunctions
/// Ownership: Borrows the object and the registry.
/// Thread affinity: The calling thread.
/// Invalidation: None.
/// Units: None.
/// Errors: A path naming an unknown property returns false from the read and write forms; the walk itself cannot fail.
/// Version: since Z2 Stage 2.
/// @endcontract
void Zenith_WalkProperties(const Zenith_TypeRegistry& xTypes, const Zenith_TypeInfo& xType, const void* pObject,
	Zenith_PfnPropertyVisitor pfnVisitor, void* pUserData, bool bIncludeHidden);
bool Zenith_ReadPropertyAt(const Zenith_TypeRegistry& xTypes, const Zenith_TypeInfo& xType, const void* pObject,
	const Zenith_PropertyPath& xPath, Zenith_MutableByteSpan xOut);    // the value encoding
bool Zenith_WritePropertyAt(const Zenith_TypeRegistry& xTypes, const Zenith_TypeInfo& xType, void* pObject,
	const Zenith_PropertyPath& xPath, Zenith_ByteSpan xValue);         // clamps to [m_fMin, m_fMax] when a range is declared
```

The walk visits properties in ascending property ID, depth first; a struct property is visited, then its fields; a fixed array of structs visits each element's fields with the element index in the path. Edits made through the inspector are never written with `Zenith_WritePropertyAt` into live ECS storage: EDIT writes into a gathered copy and records `SetProperty` commands from the diff (`Zenith2/Docs/Design/ECS.md` → "Editor surfaces").

## Zenith_Migration header

`Zenith_MigrationContext` and `Zenith_PfnMigrationStep` are declared by `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" (subsection "Migration step interface") and are placed in this header verbatim; `Zenith_MigrationChain`, `Zenith_MigrationContext` and `Zenith_MigrationError` are named by `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") as ECS's. `Zenith_DocumentTree` is CORE's (`Zenith2/Docs/Design/CORE/Text.md` → "JSON reader": the document tree wraps `Zenith_JsonDocument`), and `Zenith_MigrationRegistry` with the document framework is SCENE's and ASSET's ([Z2-N-CONTRACT-550]).

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_Migration.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

class Zenith_DocumentTree;           // CORE (a Zenith_JsonDocument subtree)
class Zenith_MigrationDiagnostics;

/// @contract enum Zenith_MigrationErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: MIGRATION_ERROR_CHAIN_GAP: no step for a version in range. MIGRATION_ERROR_NEWER_VERSION: data written by a newer
///   schema version ([Z2-N-CONTRACT-537]: opened read-only). MIGRATION_ERROR_CONVERSION: a retype whose conversion cannot
///   produce a valid value. MIGRATION_ERROR_SCRIPT: a script op raised or exceeded its budget. MIGRATION_ERROR_BAD_INPUT:
///   malformed input. MIGRATION_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_MigrationErrorCode : uint8
{
	MIGRATION_ERROR_CHAIN_GAP = 1,
	MIGRATION_ERROR_NEWER_VERSION,
	MIGRATION_ERROR_CONVERSION,
	MIGRATION_ERROR_SCRIPT,
	MIGRATION_ERROR_BAD_INPUT,
	MIGRATION_ERROR_OUT_OF_MEMORY,
};

/// @contract value Zenith_MigrationError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: Versions.
/// Errors: Names the component (m_xType), the field (m_xProperty) and the versions; the entity or document and the text are
///   logged at the point of failure ([Z2-N-ARCH-607]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_MigrationError
{
	Zenith_MigrationErrorCode m_eCode;
	Zenith_PropertyID m_xProperty;
	Zenith_TypeID m_xType;
	uint32 m_uFromVersion;
	uint32 m_uToVersion;
};
static_assert(sizeof(Zenith_MigrationError) <= 32);

/// @contract value Zenith_MigrationContext
/// Ownership: Plain value (Contracts' declaration); m_pxDiagnostics borrowed for the step.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Versions.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_MigrationContext
{
	const Zenith_PersistentID m_xDocumentID = {};
	const Zenith_TypeID m_xDocumentType = {};
	const uint32 m_uFromVersion = 0;
	Zenith_MigrationDiagnostics* const m_pxDiagnostics = nullptr;
};

using Zenith_PfnMigrationStep = Zenith_Result<void, Zenith_MigrationError> (*)(
	Zenith_DocumentTree& xDocument, const Zenith_MigrationContext& xContext);

/// @contract value Zenith_MigrationStep
/// Ownership: Plain value owned by its chain.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its chain.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. elaboration: a step is either a C++ document step or a schema op list (Schemas.md).
/// @endcontract
struct Zenith_MigrationStep
{
	const uint32 m_uFromVersion = 0;
	const Zenith_PfnMigrationStep m_pfnStep = nullptr; // C++ types; null for schema types
	const Zenith_ByteSpan m_xSchemaOps = {};    // schema types: the encoded op list of the "migrations" entry
};

/// @contract value Zenith_MigrationChain
/// Ownership: Owned by the registry, referenced by Zenith_TypeInfo::m_pxMigrations.
/// Thread affinity: IMMUTABLE. Members non-const: assembled field by field by the type builders and copied into the
///   registry's tables.
/// Invalidation: With its TypeInfo.
/// Units: Versions.
/// Errors: Completeness is checked by Freeze (REFLECT_ERROR_MIGRATION_CHAIN): one step for each version 1 … m_uVersion − 1.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_MigrationChain
{
	Zenith_Span<const Zenith_MigrationStep> m_axSteps;   // ascending m_uFromVersion
};
```

A C++ component step migrates the component's canonical JSON field-set object (§ "JSON field-set form") inside a `Zenith_DocumentTree`, so one step serves authoring documents (SCENE applies it to the component object in place) and saves (`Zenith_MigrateFieldSet` renders the field set as that object, runs the step and reads it back).

## Registry internals

### Registry storage

| Structure | Container | Contents |
|---|---|---|
| type records | `Zenith_Array<Zenith_TypeRecord>` (general-heap tier) | one per registered type revision: the `Zenith_TypeInfo`, its property array, layout, defaults, names, arbitration, chain, retired IDs |
| ID index | `Zenith_HashMap<Zenith_TypeID, uint32>` | type ID → current record |
| stable-name index | sorted `Zenith_Array<uint32>` by stable-name bytes | binary search; also the order of `GetTypes` |
| script-name index | `Zenith_HashMap<uint64, uint32>` keyed by `Zenith_Hash64` of the script name | collision checks and `FindTypeByScriptName` |
| text arena | `Zenith_Arena` blocks of 64 KiB | names, default bytes, editor strings |
| pending set | `Zenith_Array<uint32>` | records added or replaced since the last Freeze or window start |
| retirement queue | `Zenith_Array<{record, frame}>` | replaced records, released when two `RetireFrame` calls have passed |
| schema diagnostics | `Zenith_Array<Zenith_SchemaDiagnostic>` with text in the arena | the latest schema registration's diagnostics |

### Registration algorithm

0. **Declaration phase.** From `Create` until `BindStringTable`, the registry is in its declaration phase (startup step 6, before the configuration store and the process `Zenith_StringTable` exist; `Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"): every C++ and runtime declaration is accepted and recorded; names, stable names and script names are copied into the registry's text arena at the call (they need no string table); a `STRING_ID` default is kept as its UTF-8 text (`Zenith_PropertyBuilder::DefaultText`, `Zenith_RuntimeFieldDesc::m_strDefaultText`) in the text arena. `BindStringTable` (startup step 10) interns every recorded default text in registration order and writes the resulting `Zenith_StringID` bytes into the default; from then on a default text is interned at its declaration. `AttachConfigSchema` gives the registry the `Zenith_ConfigSchema` MOD creates at step 6; `GetConfigSchema` returns it to a module's `Zenith_Module_<M>_RegisterTypes`, which registers its configuration keys there beside its types (there is no third module entry point, Z2-D-024), until MOD freezes the schema at the end of step 6's declarations. `Freeze` before `BindStringTable` is `ZENITH_CHECK` (a composition-root ordering error); a schema registration before `BindStringTable` is also `ZENITH_CHECK`, because schema defaults intern at once and schemas register at step 10.
1. **C++ types** (`BeginComponent<T>`, `BeginType<T>` … `End`): record the builder's declarations into a new pending record; no validation beyond the static asserts runs at `End`, so a module's registration function never fails midway.
2. **Runtime types** (`BeginRuntimeType` … `End`): compute the layout (§ "Layout algorithm"), handle each `STRING_ID` field's default text (`m_strDefaultText`) as item 0 says — kept as text in the declaration phase and interned at `BindStringTable`, interned at `End` once the table is bound — derive the type ID, run the per-type rules of § "Freeze validation" at once (so the schema front end reports errors per file), then append the pending record. A `STRING_ID` default is only ever text: a non-empty `m_xDefault` (or `Zenith_PropertyBuilder::Default` bytes) on a `STRING_ID` property is `REFLECT_ERROR_BAD_DEFAULT` by rule 12 of § "Freeze validation" in every state, because a string ID is meaningful only for the process string table that issued it. A failure registers nothing of that type.
3. **Collisions** are checked at `End` for runtime types and at `Freeze` for all: the new ID against the ID index and the pending set (`REFLECT_ERROR_TYPE_ID_COLLISION`, both stable names logged), the stable name against the name index (`REFLECT_ERROR_DUPLICATE_STABLE_NAME`), the script name (`REFLECT_ERROR_BAD_NAME` with both sources logged).
4. The registration order (engine, modules in canonical order, schemas — [Z2-N-ARCH-642], [Z2-N-ARCH-847] — then behaviour-state types, item 5) is the caller's; the registry records it only for diagnostics and never lets it influence IDs, layouts or iteration order.
5. **Behaviour-state types** register at startup step 10, after `RegisterSchemaBlob` and before `Freeze`, from the game's cooked script manifest, through `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, "Behaviour.<Name>.State", …)` with `Origin(TYPE_ORIGIN_SCHEMA)`, one type per behaviour the cook found, in module-ID order (the manifest's order); this is the single admitted path before `Freeze` (`Zenith2/Docs/Design/ECS.md` → "Behaviour-state type registration"), and the registry enforces it: `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …)` in the DECLARING state (startup step 6, before `BindStringTable`) is `ZENITH_CHECK` (§ "Revisions and the hot-reload window"). A registration in OPEN before `RegisterSchemaBlob` is not detectable as such, but a state field naming a schema struct or asset kind registered by the blob then fails at `End` with `REFLECT_ERROR_UNKNOWN_ELEMENT_TYPE`, so the composition root's order after the blob is what makes every state type registrable. A behaviour-state type added or changed after `Freeze` (script hot reload, the editor) registers only inside the hot-reload window (§ "Revisions and the hot-reload window").

### Freeze validation

`Freeze` validates every pending record in one pass and reports every error ([Z2-N-ARCH-646]); nothing is published when any error exists, and the error result carries the first error in (stable name byte order, property ID) order.

| # | Rule | Error |
|---|---|---|
| 1 | type ID unique; stable name unique; stable name matches `^[A-Z][A-Za-z0-9]*(\.[A-Z][A-Za-z0-9]*)+$`; script name matches `^[A-Z][A-Za-z0-9]{0,63}$` and is unique among component, struct and event types | `TYPE_ID_COLLISION`, `DUPLICATE_STABLE_NAME`, `BAD_NAME` |
| 2 | property IDs unique within the type, non-zero, below 2^31, and not in the retired list | `DUPLICATE_PROPERTY_ID` |
| 3 | every component property has an authority, directly or by the type default ([Z2-N-ARCH-482]) | `MISSING_AUTHORITY` |
| 4 | no property authority is 11, 12 or `PHASE_NONE` after defaulting ([Z2-N-ARCH-476]); a resource has an owning phase 1 … 11, where 11 (`PHASE_SAVE_SNAPSHOT`) makes it a snapshot buffer, the one write phase 11 admits ([Z2-N-ARCH-503]), and no resource owns 12 | `BAD_AUTHORITY` |
| 5 | arbitration names known properties at most once each; `WeightedBlend` only on blendable types ([Z2-N-ARCH-496]); an order names each source at most once ([Z2-N-ARCH-493]); `ArbitrationCustom` only for `TYPE_ORIGIN_CPP` | `ARBITRATION_TYPE` |
| 6 | one migration step per version 1 … version − 1 | `MIGRATION_CHAIN` |
| 7 | a type with a lifetime-carrying property has non-null construct, destroy and copy hooks; an `ASSET` property's value-kind hooks are installed | `MISSING_HOOKS` |
| 8 | every element, struct and asset-kind type ID is registered (pending records included) with the kind it needs | `UNKNOWN_ELEMENT_TYPE` |
| 9 | a component's size ≤ 1,024 and alignment ≤ 16; a C++ type's property byte ranges do not overlap and lie within `sizeof(T)` | `LAYOUT` |
| 10 | struct nesting depth ≤ 4 and no container nested in a container ([Z2-N-ARCH-595]) | `LAYOUT` |
| 11 | an event type has no authority, arbitration or container property ([Z2-N-ARCH-610]) | `BAD_AUTHORITY` / `ARBITRATION_TYPE` / `LAYOUT` |
| 12 | every default is exactly the property's encoded size and a valid value of its type; an `ASSET` default is 16 persistent-ID bytes, and a C++ type's `ASSET` default is null (its constructor, not the registry, builds its handles); a `STRING_ID` default is text only (`DefaultText`, `m_strDefaultText`), never encoded bytes; a trivially copyable C++ type's `Default()` equals the bytes of its value-initialised `T{}` at that property (§ "C++ hooks") | `BAD_DEFAULT` |

After a successful pass `Freeze` builds the indexes, sets the revision to 1 and marks the registry frozen. A second `Freeze` returns `REFLECT_ERROR_FROZEN`.

### Revisions and the hot-reload window

| State | Event | Next state and action |
|---|---|---|
| DECLARING (after Create) | a C++ or runtime declaration other than a behaviour-state type; `AttachConfigSchema`; `GetConfigSchema` (while the schema is not frozen) | DECLARING; pending records appended with default texts kept |
| DECLARING | `BeginRuntimeType(TYPE_KIND_BEHAVIOUR_STATE, …)` | `ZENITH_CHECK`: behaviour-state types register at startup step 10 alone (§ "Registration algorithm" item 5); a step-6 registration is a composition-root ordering error |
| DECLARING | `BindStringTable` | OPEN; every pending default text interned |
| DECLARING | `Freeze`, `RegisterSchemaJson`, `RegisterSchemaBlob`, `ResolveReferenceDefaults` | `ZENITH_CHECK` |
| OPEN (before Freeze) | any registration, including the behaviour-state types of the cooked script manifest at startup step 10 | OPEN; pending records appended |
| OPEN | `Freeze` success | FROZEN; revision 1 published |
| OPEN | `Freeze` failure | OPEN; nothing published; diagnostics logged |
| OPEN | `ResolveReferenceDefaults` | `ZENITH_CHECK` (defaults resolve after `Freeze`) |
| FROZEN | `BeginHotReload` (main thread, prologue step 6) | WINDOW; a staging copy of the ID and name indexes is made |
| FROZEN | a registration | FROZEN; C++ builders `ZENITH_CHECK`; runtime and schema registrations `REFLECT_ERROR_FROZEN` |
| FROZEN | `ResolveReferenceDefaults(eType, …)`, first call for `eType` (startup step 11; the cook after its `Freeze`) | FROZEN; the resolver bound for `eType` and every non-null reference default of that kind resolved into its type's construction default object; the revision is unchanged. No world can read a default object yet, because `Zenith_WorldDesc` validation refuses a world while `AreReferenceDefaultsResolved` is false, and a registry with no non-null default of that kind writes none; an unresolvable default is `SCHEMA_ERROR_BAD_DEFAULT` (exit code 3 at startup) |
| FROZEN | `ResolveReferenceDefaults` a second time for one value kind, whatever the first returned | `ZENITH_CHECK` |
| WINDOW | `BeginRuntimeType` of `TYPE_KIND_BEHAVIOUR_STATE`; `RegisterSchemaJson` of a schema type (editor builds, between play sessions) | WINDOW; a replacement record staged (same type ID; version may rise) |
| WINDOW | a registration of any other kind, or a C++ registration | WINDOW; `REFLECT_ERROR_HOT_RELOAD_KIND` recorded and returned at `EndHotReload` |
| WINDOW | `ResolveReferenceDefaults` | `ZENITH_CHECK`; the window resolves its own staged records' asset defaults in `EndHotReload` through the resolver the step-11 call bound |
| WINDOW | `EndHotReload` | validation of the staged records with § "Freeze validation" plus: a replaced schema type keeps its stable name, its version does not decrease, and a version increase carries its migration steps, and every staged non-null asset default resolves through the bound resolver (`REFLECT_ERROR_BAD_DEFAULT` otherwise, with the schema diagnostic logged); on success FROZEN with revision + 1 published by one index swap, the replaced records queued for retirement; on failure FROZEN with the previous revision in effect ([Z2-N-ARCH-651]) |
| WINDOW | `AbandonHotReload` | FROZEN; every staged record discarded |

The publication is atomic for readers because readers only exist outside the window: the window opens in the frame prologue while no job runs ([Z2-N-ARCH-437]), and the index pointer swap happens before the prologue ends. A `Zenith_TypeInfo*` of a retired revision stays readable for two further `RetireFrame` calls (the frame loop calls it once per prologue), after which its storage is freed. Consumers that cache `Zenith_TypeInfo*` (worlds' column tables, script views, the inspector) re-resolve by type ID when `GetRevision()` changes; a play world never observes a schema-type change, because the editor opens schema windows only between play sessions ([Z2-N-ARCH-648]).

### Lookup structures

`FindType` is a hash lookup (O(1) expected); `FindTypeByStableName` a binary search over the name index (O(log n)); `GetTypes` and `GetTypesOfKind` return spans sorted by stable-name byte order, the order `.d.luau` generation, the inspector's type picker and every deterministic walk use. Nothing observable iterates the hash map ([Z2-N-ARCH-884]). All lookups are data lookup, never service lookup ([Z2-N-ARCH-647]).

### Shape hash

`m_ulShapeHash` is `Zenith_Hash64Stream` over: kind, version, size, alignment, then per property in ascending ID the ID, value type, element type, array count, max count, offset, size, authority, flags and default bytes. The type registry's own hash of all shapes (`GetRevision` excluded) feeds the script binding-set hash and the schedule-cache key, so any registry change that alters a consumer's view invalidates it.

## Layout algorithm

For schema types, structs and behaviour-state types (`Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file", subsection "Inline layout"):

1. For each property compute (size, alignment) from the "Inline size" table of that section: fixed arrays N × element size with the element's alignment; structs their own computed layout (computed first, recursively, depth ≤ 4); containers 8 bytes aligned 4.
2. Sort properties by (alignment descending, property ID ascending).
3. Place each at the lowest offset ≥ the previous end that is a multiple of its alignment.
4. Size = end rounded up to the largest alignment (at least 1); a type with no properties has size 1 and alignment 1 (an empty tag component).
5. Fail when size > 1,024 (components) or > 65,535 (structs used as container elements) — `REFLECT_ERROR_LAYOUT`, or `SCHEMA_ERROR_LIMIT` through the schema front end.

Complexity O(n log n) in the property count. The layout is recomputed at every registration and never stored in a file ([Z2-N-ARCH-597]); for C++ types the layout is the recorded C++ offsets, validated by Freeze rule 9. `Test_Schema_LayoutIndependentSerialization` proves adding a field moves offsets without changing any field set.

## Lifecycle hooks

### C++ hooks

`Zenith_MakeLifecycleHooks<T>` (§ "Zenith_ReflectValueTraits header"). Trivially copyable types have null hooks and are constructed from their registered defaults by the storage: a byte copy of the construction default object, which the registry builds from the property defaults at `End` and publishes as `Zenith_TypeInfo::GetDefaultObject()` (§ "Zenith_TypeInfo header"). <!-- elaboration --> For a trivially copyable C++ type the default object starts as the bytes of a value-initialised `T{}`, which `BeginComponent<T>` and `BeginType<T>` capture on the stack and pass to the registry, so member initialisers survive; bytes outside every property's range are then zeroed, and only properties that register a `Default()` are written over it. A struct or fixed-array field of a C++ type keeps the `T{}` bytes (the C++ initialiser defines it, not the element type's default object). A C++ `Default()` whose encoded bytes differ from the `T{}` bytes at that property is rejected by rule 12 of § "Freeze validation" (`REFLECT_ERROR_BAD_DEFAULT`), so a C++ component constructs as `T{}` on every backend. <!-- elaboration --> The rule holds for every type whose `m_pfnConstruct` is null, on every backend and in every store that constructs objects (rows, event payloads, staging copies): the object's bytes are the default object's, nested struct and fixed-array defaults included, never zero-filled bytes; a non-null `m_pfnConstruct` is called instead and is never combined with the copy. A schema type's construct hook copies the same bytes itself (§ "Schema hooks").

### Schema hooks

A schema type's hooks are four generic functions in `ZenithReflect/Internal/Zenith_SchemaHooks.cpp` parameterised by the type record (every schema type's hooks hold the same four function pointers, and `m_pUserData` names the type's registry-owned record, which each hook reads from its `pUserData` parameter; no per-type thunk or slot table exists, so the hooks add no writable data, [Z2-N-ARCH-985]). Behaviour:

| Hook | Action per object |
|---|---|
| construct | copy the construction default object bytewise, then for each `ASSET` field (including inside structs and fixed arrays): a null declared default is constructed null through `m_pfnConstructNull`; a resolved non-null default is copied from the construction default object through the value-kind copy hook, adding one reference |
| destroy | for each `ASSET` field (including inside structs and fixed arrays) call the value-kind destroy hook; `CONTAINER` fields are not touched (the ECS releases their blocks, below) |
| copy | byte copy, then for each `ASSET` field the value-kind copy hook |
| move | bytewise relocation (component and struct types are trivially relocatable by construction) |

Containers carry lifetime but their storage is a world's container heap, which ZenithReflect cannot reach; so every store that holds a `container<T>` field (ECS columns, shadows, snapshots) releases and copies container blocks itself: backends free a destroyed row's blocks through `Zenith_ContainerHeap::FreeNow` and never duplicate a row that holds a container without allocating new blocks (`Zenith2/Docs/Design/ECS/Backends.md` → "Container heap"). The `PROPERTY_FLAG_CARRIES_LIFETIME` flag and `Zenith_TypeLayout::m_bTriviallyCopyable` tell every store whether a bytewise copy is allowed ([Z2-N-ARCH-609]).

### Value-kind hooks

`InstallValueKindHooks(PROPERTY_TYPE_ASSET, …)` is called by the composition root in startup step 10, before the first schema registers, with ZenithAssets' hooks (null construction, reference release, reference add); the hooks read the 16-byte handle layout, which only ZenithAssets knows. A schema with an `asset<K>` field registered before the hooks are installed fails `REFLECT_ERROR_MISSING_HOOKS`. Entity defaults are null; a non-null asset default is resolved once per process by `ResolveReferenceDefaults` (startup step 11, after the asset registry exists) into the type's construction default object through `m_pfnConstructFromPersistentID` (`Zenith2/Docs/Design/ECS/Schemas.md` → "Default values"); a hot-reload window's staged records resolve their asset defaults in `EndHotReload` through the bound resolver, an unresolvable one failing the window with `REFLECT_ERROR_BAD_DEFAULT` and the schema diagnostic logged.

## Field-set codecs

### Binary field-set record

Little-endian, bounded by its own lengths, used by saves, replay checkpoints, cooked scenes and replication payloads (the containers that embed it are SCENE's, SAVE's and NET's):

| Offset | Type | Field |
|---|---|---|
| 0 | `uint64` | type ID |
| 8 | `uint32` | schema version |
| 12 | `uint16` | field count n (≤ 128) |
| 14 | `uint16` | reserved, 0 |
| 16 | n × field | fields in ascending property ID |

Each field: `uint32` property ID, `uint8` value type, `uint8` flags (bit 0: the value is a nested record list), `uint16` reserved 0, `uint32` value byte length L, then L bytes: the value encoding of § "Zenith_TypeInfo header" with these substitutions — `STRING_ID` is the UTF-8 text (≤ 65,535 bytes, the intern limit), `ENTITY` and `ASSET` are 16-byte persistent IDs (all zero for null), `CONTAINER` is a `uint32` count followed by the elements, and a `STRUCT` or struct element is a nested field-set record (flags bit 0). Every length is checked against the remaining input with overflow-checked arithmetic before use, depth ≤ 5 records ([Z2-N-ARCH-951]); `Zenith_Fuzz_FieldSetDecode` fuzzes the decoder.

### JSON field-set form

A JSON object whose members are keyed by the decimal property ID (`"1": 100.0`), written in ascending ID, values in the "Default JSON form" of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" (strings as text, persistent IDs and references in the 36-character form or `null`, containers as arrays, structs as nested objects in the same form), unknown members preserved in position ([Z2-N-CONTRACT-543]). Data is keyed by ID so a `rename` migration op changes no data ([Z2-N-ARCH-604]); editors show names from the registry. SCENE's scene documents embed components in this form; the document's canonical key order around it is SCENE's.

## Reflection walk algorithm

`Zenith_WalkProperties` iterates `m_axProperties` (ascending ID). For each property: skip it when `PROPERTY_FLAG_EDITOR_HIDDEN` is set and hidden properties are excluded; build the visit (owner, property, path, value pointer `pObject + m_uOffset`, element count); call the visitor; for a `STRUCT` recurse into the element type with the path extended; for a `FIXED_ARRAY` of structs recurse once per element with the element index; for a `CONTAINER` visit once with `m_pValue` pointing at the `Zenith_ContainerRef` and let the caller read elements through its resolver. The walk is iterative with an explicit stack of depth ≤ 5, allocates nothing and returns early when the visitor returns false. `Zenith_ReadPropertyAt` and `Zenith_WritePropertyAt` follow a path the same way; the write form clamps numeric values to a declared range and rejects a value of the wrong size.

## Reflect internal files

| File | Contents |
|---|---|
| `Internal/Zenith_TypeRegistry.cpp` | the registry, indexes, Freeze, windows, retirement |
| `Internal/Zenith_TypeRegistryValidate.cpp` | § "Freeze validation" rules |
| `Internal/Zenith_TypeBuilders.cpp` | C++ and runtime builders |
| `Internal/Zenith_TypeLayout.cpp` | § "Layout algorithm" |
| `Internal/Zenith_SchemaHooks.cpp` | § "Schema hooks": the four generic hooks over the record named by `pUserData` |
| `Internal/Zenith_FieldSet.cpp`, `Internal/Zenith_FieldSetJson.cpp` | capture, apply, both codecs |
| `Internal/Zenith_ReflectionWalk.cpp` | the walk and path access |
| `Internal/Zenith_SchemaJson.cpp`, `Internal/Zenith_SchemaRules.cpp`, `Internal/Zenith_SchemaBlob.cpp`, `Internal/Zenith_SchemaMigrator.cpp`, `Internal/Zenith_LuauTypes.cpp` | the schema front end (`Zenith2/Docs/Design/ECS/Schemas.md`) |
| `Internal/Zenith_ReflectNames.cpp` | `Zenith_ToString`, phase and writer-source names, the log category `ecs.reflect` |

No ZenithReflect file includes a third-party header other than through ZenithCore's public API, and no file includes a platform header; ZenithReflect has no `Internal/Windows`, `Internal/Android` or `Internal/Linux` file.
