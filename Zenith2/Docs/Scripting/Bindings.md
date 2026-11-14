# Zenith2 Scripting — bindings, generated types and environments

Companion to `Zenith2/Docs/Scripting.md` (its "Modules, bindings and generated types" section is the top-level contract; this file carries the detail). Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Binding registration and the thunk ABI

### Two halves

A binding has two halves on either side of the exception firewall:

| Half | Lives in | Compiled with | Touches |
|---|---|---|---|
| dispatcher | `Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptBindingDispatch.cpp` | exceptions (`/EHs`, `-fexceptions`) | the Luau stack, `Zenith_ScriptCallFrame` |
| thunk | the owning engine library, any library at or above ZenithReflect (ZenithECS, ZenithScene, ZenithPhysics, ZenithAudio, …), which includes `ZenithReflect/Zenith_ScriptBinding.h` and nothing of ZenithScript; instantiated by `Zenith_ScriptBindingTable::AddFunction` | exceptions off | `Zenith_ScriptCallFrame`, the engine function |
| registration for libraries below ZenithReflect | `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/` (the composition root) | exceptions off | as a thunk |

- [Z2-N-SCRIPT-561] A thunk never includes a Luau header, never holds a `lua_State*` and never raises; it returns `SCRIPT_THUNK_STATUS_ERROR` with `m_xError` filled, and the dispatcher turns that into a Luau error.
- [Z2-N-SCRIPT-565] `Zenith_ScriptBinding.h` is a public header of ZenithReflect and includes no ZenithScript header; a binding owner never includes a ZenithScript header, so no library that ZenithScript depends on depends back on it (libgraph checks the edge).
- [Z2-N-SCRIPT-566] A handle type owned by a library above ZenithReflect (entity handles in ZenithECS, asset handles in ZenithAssets, persistent IDs in ZenithCore) crosses the ABI as a `Zenith_ScriptOpaqueHandle`; the owning library specialises `Zenith_ScriptValueTraits<T>` next to the type, and the registration template uses that specialisation.

### Public binding types

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h
// The Luau-free binding ABI. A public header of ZenithReflect: every library that owns bindings depends on
// ZenithReflect only, never on ZenithScript (Scripting.md → "Modules, bindings and generated types").
#pragma once
#include "ZenithReflect/Zenith_ReflectIDs.h"   // Zenith_TypeID, Zenith_PropertyID (Zenith2/Docs/Contracts.md → "Reflection")
#include "ZenithCore/Zenith_Types.h"           // uint8 … uint64
#include "ZenithCore/Zenith_Result.h"          // Zenith_Result
#include "ZenithCore/Zenith_Span.h"           // Zenith_Span, Zenith_ByteSpan (Architecture.md → "Purpose and scope", table "Assumed types")
#include "ZenithCore/Zenith_StringView.h"     // Zenith_StringView (AddNameTable)
#include "ZenithCore/Zenith_Format.h"         // Zenith_FormatString, Zenith_FormatTo (Zenith_ScriptFailThunk; Design/CORE/Text.md → "Formatting")

// Stable ID of a binding function: Zenith_Hash64 of "<namespace>.<Luau name>"; part of the binding-set hash.
struct Zenith_ScriptFunctionID { uint64 m_ulValue = 0; };

enum Zenith_ScriptErrorKind : uint8
{
	// the twelve SCRIPT_ERROR_KIND_* enumerators, in the order listed in Scripting.md → "Luau integration and the exception firewall";
	// the first is SCRIPT_ERROR_KIND_RUNTIME
};

enum Zenith_ScriptValueKind : uint8
{
	SCRIPT_VALUE_KIND_NIL,
	SCRIPT_VALUE_KIND_BOOL,
	SCRIPT_VALUE_KIND_NUMBER,          // f64
	SCRIPT_VALUE_KIND_VECTOR,          // Luau vector: 3 x f32 (Vec2 uses z = 0)
	SCRIPT_VALUE_KIND_VEC4,            // Quat, Vec4, Colour: 4 x f32 value userdata
	SCRIPT_VALUE_KIND_STRING_ID,       // interned string ID
	SCRIPT_VALUE_KIND_ENUM,            // reflected enum value (type ID + value)
	SCRIPT_VALUE_KIND_ENTITY,          // world entity handle (generational, carries its world)
	SCRIPT_VALUE_KIND_ASSET,           // shared asset handle (type ID + handle)
	SCRIPT_VALUE_KIND_PERSISTENT_ID,   // 128-bit GUID
	SCRIPT_VALUE_KIND_STRUCT,          // a reflected value struct, by pointer into the frame arena
	SCRIPT_VALUE_KIND_CALLBACK,        // a Luau function held as a registry reference
	SCRIPT_VALUE_KIND_TICKET,          // an asynchronous request ticket
};

// Handle kinds are owned by libraries above ZenithReflect, so the value carries their bits opaquely; the owning
// library specialises Zenith_ScriptValueTraits<T> (ToValue / FromValue) for its handle type, next to the type.
// The payload structs are trivial (no default member initialisers) so they can be members of Zenith_ScriptValue's union.
struct Zenith_ScriptOpaqueHandle { uint64 m_aulBits[2]; Zenith_TypeID m_xType; };
struct Zenith_ScriptEnumValue { Zenith_TypeID m_xType; sint64 m_iValue; };
struct Zenith_ScriptStructValue { Zenith_TypeID m_xType; void* m_pData; };   // frame-arena storage
struct Zenith_ScriptTicket { uint64 m_ulValue; bool m_bProvisional; };     // provisional index, then completion ID

struct Zenith_ScriptCallbackRef
{
	uint32 m_uRegistryRef;            // lua_ref index in the owning VM's registry; 0 = empty
	uint16 m_uVMIndex;                // the VM that owns the reference (0 = main VM)
	uint64 m_ulOwnerInvocation;       // the Zenith_ScriptInvocationID value of the invocation that captured it
	bool m_bRetained;                  // set by Zenith_ScriptInvocationContext::RetainCallback
};

struct Zenith_ScriptValue
{
	Zenith_ScriptValueKind m_eKind = SCRIPT_VALUE_KIND_NIL;
	union
	{
		bool m_bValue = false;         // the one variant member with an initialiser, so the union is default-constructible
		double m_fValue;               // Zenith Hungarian 'f' covers f32 and f64
		float m_afVector[4];
		uint64 m_ulStringID;          // the Core interned-string ID
		Zenith_ScriptEnumValue m_xEnum;
		Zenith_ScriptOpaqueHandle m_xHandle;   // ENTITY, ASSET, PERSISTENT_ID
		Zenith_ScriptStructValue m_xStruct;
		Zenith_ScriptCallbackRef m_xCallback;
		Zenith_ScriptTicket m_xTicket;
	};
};

template<typename T> struct Zenith_ScriptValueTraits;  // specialised by the library that owns T

enum Zenith_ScriptThunkStatus : uint8
{
	SCRIPT_THUNK_STATUS_OK,
	SCRIPT_THUNK_STATUS_ERROR,
};

// The thunk error's own code enumeration (the struct form of Zenith2/Docs/Contracts.md → "Errors and results":
// Zenith_ScriptThunkError / Zenith_ScriptThunkErrorCode), one value per Zenith_ScriptErrorKind in kind order, numbered from 1.
enum Zenith_ScriptThunkErrorCode : uint8
{
	SCRIPT_THUNK_ERROR_RUNTIME = 1, SCRIPT_THUNK_ERROR_MISUSE, SCRIPT_THUNK_ERROR_BUDGET, SCRIPT_THUNK_ERROR_SNAPSHOT_CAP,
	SCRIPT_THUNK_ERROR_RESERVATION, SCRIPT_THUNK_ERROR_YIELD_IN_SYSTEM, SCRIPT_THUNK_ERROR_CHECKPOINT, SCRIPT_THUNK_ERROR_VM_MEMORY,
	SCRIPT_THUNK_ERROR_WATCHDOG, SCRIPT_THUNK_ERROR_INTERNAL, SCRIPT_THUNK_ERROR_COMPILE, SCRIPT_THUNK_ERROR_LOAD,
};

// 8 bytes and text-free; the message of a failing thunk travels in Zenith_ScriptCallFrame::m_acErrorMessage.
struct Zenith_ScriptThunkError
{
	Zenith_ScriptThunkErrorCode m_eCode = SCRIPT_THUNK_ERROR_RUNTIME;   // the kind is m_eCode - 1; an abort kind is re-raised as that kind
	uint8 m_uReserved = 0;
	uint16 m_uCode = 0;                                           // ZS catalogue code, or 0 for a module-defined failure
	uint32 m_uDiagnosticSequence = 0;                             // for errors InvokeCallback returns: the VM's error-scratch sequence
};
static_assert(sizeof(Zenith_ScriptThunkError) == 8);
constexpr Zenith_ScriptErrorKind Zenith_ScriptThunkErrorKind(const Zenith_ScriptThunkError& xError)
{
	return static_cast<Zenith_ScriptErrorKind>(static_cast<uint8>(xError.m_eCode) - 1u);
}
constexpr Zenith_ScriptThunkErrorCode Zenith_ScriptThunkErrorCodeFromKind(Zenith_ScriptErrorKind eKind)
{
	return static_cast<Zenith_ScriptThunkErrorCode>(static_cast<uint8>(eKind) + 1u);
}

// The error of Zenith_ScriptBindingTable::Seal (the enumeration form), the first failure in sealed order.
enum Zenith_ScriptBindingError : uint8
{
	SCRIPT_BINDING_ERROR_DUPLICATE_ID = 1,        // two functions with one function ID
	SCRIPT_BINDING_ERROR_DUPLICATE_NAME,          // one namespace with two equal Luau names
	SCRIPT_BINDING_ERROR_INVALID_NAME,            // a namespace or Luau name outside rule 7 of "Name mapping from Zenith Hungarian" → "Rules"
	SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR,      // zero contexts, READ_WORLD without a resource, BUFFERED without a side-effect mask
	SCRIPT_BINDING_ERROR_MISSING_CONSUMER,        // a BUFFERED function whose mask names a SCRIPT-held kind with no consumer
	SCRIPT_BINDING_ERROR_UNKNOWN_NAME_KIND,       // a parameter's m_szResolveKind names a kind no AddNameTable declared
	SCRIPT_BINDING_ERROR_DUPLICATE_NAME_TABLE,    // two AddNameTable calls for one kind, or one table holding a name twice
};
constexpr const char* Zenith_ToString(Zenith_ScriptBindingError eError);
constexpr const char* Zenith_ToString(Zenith_ScriptThunkErrorCode eCode);

struct Zenith_ScriptCallbackOutcome
{
	uint32 m_uResultCount = 0;
	uint64 m_ulSafepoints = 0;        // safepoints of the callback, already added to the enclosing count
};

struct Zenith_WorldResourceID { uint32 m_uValue = 0; };   // physics world, navmesh, audio mixer, … (frame-graph world resources)

// The source-kind capacity the binding table and ZenithScript size their per-kind arrays by. ZenithReflect and ZenithScript do
// not depend on ZenithSave, which declares Zenith_AsyncSourceKind and uASYNC_SOURCE_KIND_CAPACITY (Architecture.md →
// "Multiplayer foundations"), so this mirror carries the value and Test_Script_Async_SourceKindCapacityMatchesRegistry proves
// the two equal.
inline constexpr uint32 uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY = 32;

// The Luau API classification of a binding (Zenith2/Docs/Design/MAINT/Versioning.md → "Luau API classification").
enum Zenith_ScriptApiStability : uint8
{
	SCRIPT_API_STABILITY_STABLE = 1,
	SCRIPT_API_STABILITY_EXPERIMENTAL,
	SCRIPT_API_STABILITY_INTERNAL,             // engine-only functions; never listed as stable surface
};
inline constexpr uint8 uSCRIPT_API_STAGE_NONE = 0xFF;   // "no stage": not experimental, not deprecated

struct Zenith_ScriptDeprecation
{
	const char* m_szReplacement = nullptr;     // "Zenith.<Namespace>.<Name>" of a sealed function; null when not deprecated
	uint8 m_uDeprecatedStage = uSCRIPT_API_STAGE_NONE;   // D, the engine MAJOR when the deprecating diff landed
	uint8 m_uRemovedStage = uSCRIPT_API_STAGE_NONE;      // D + 1 (8 when D is 7)
};

// The four inputs a registrant passes as AddFunction's trailing parameter; the default is stable since Z2 Stage 0.
struct Zenith_ScriptFunctionVersion
{
	uint8 m_uSinceStage = 0;                  // the fix version that introduced the binding (0 … 7)
	Zenith_ScriptApiStability m_eStability = SCRIPT_API_STABILITY_STABLE;
	uint8 m_uExperimentalUntilStage = uSCRIPT_API_STAGE_NONE;   // G (since … 7) when experimental
	Zenith_ScriptDeprecation m_xDeprecation;
};

enum Zenith_SideEffectKind : uint8
{
	SIDE_EFFECT_KIND_SPAWN,
	SIDE_EFFECT_KIND_DESTROY,
	SIDE_EFFECT_KIND_ADD_COMPONENT,
	SIDE_EFFECT_KIND_REMOVE_COMPONENT,
	SIDE_EFFECT_KIND_REPARENT,
	SIDE_EFFECT_KIND_FIELD_COMMAND,
	SIDE_EFFECT_KIND_EVENT,
	SIDE_EFFECT_KIND_AUDIO_TRIGGER,
	SIDE_EFFECT_KIND_VFX_TRIGGER,
	SIDE_EFFECT_KIND_SAVE_REQUEST,
	SIDE_EFFECT_KIND_ASYNC_REQUEST,
	SIDE_EFFECT_KIND_CONTRIBUTION,
	SIDE_EFFECT_KIND_XR_RECENTRE,          // Stage 7: ZenithXR's recentre request, no payload; SCRIPT-held, consumed by the XR module
	SIDE_EFFECT_KIND_COUNT,
};

using Zenith_SideEffectKindMask = uint32;   // bit (1 << Zenith_SideEffectKind) per allowed kind
inline constexpr Zenith_SideEffectKindMask uSIDE_EFFECT_MASK_FIELD_COMMAND = 1u << SIDE_EFFECT_KIND_FIELD_COMMAND;

// Zenith_ScriptCapacityClass and Zenith_ScriptCapacityDemand are defined in this header; their enumerators and fields are
// listed in Scripting.md → "Merge-time capacity and asynchronous results".
struct Zenith_ScriptCapacityDemand;

enum Zenith_ScriptInvocationKind : uint8
{
	SCRIPT_INVOCATION_KIND_SYSTEM,
	SCRIPT_INVOCATION_KIND_BEHAVIOUR,
	SCRIPT_INVOCATION_KIND_LOAD,
	SCRIPT_INVOCATION_KIND_TOOLS,
};

/// @contract interface Zenith_ScriptInvocationContext
/// Ownership: Implemented and owned by ZenithScript, one per running invocation, in the transaction arena; a thunk
///   borrows it through Zenith_ScriptCallFrame::m_pxContext for the duration of one call.
/// Thread affinity: OWNER_THREAD. Valid on the thread running the invocation, for the duration of the thunk call.
/// Invalidation: Invalid when the thunk returns; a thunk never stores the pointer.
/// Units: Ticks are fixed-step simulation ticks; GetDeltaTime is in seconds; payload sizes in bytes.
/// Errors: Fallible members return Zenith_Result with a Zenith_ScriptThunkError, which the thunk returns to the
///   dispatcher; misuse (wrong context, full per-entity cap) is an error value and never an assert.
/// Reentrancy: ALLOWED(InvokeCallback). InvokeCallback re-enters script through the protected entry, at most
///   uSCRIPT_MAX_PROTECTED_DEPTH deep; every other member is called from the thunk alone.
/// Version: since Z2 Stage 3. Source API; changes follow the source-API rule of Zenith2/Docs/Contracts.md → "Compatibility policy per kind".
/// @endcontract
class Zenith_ScriptInvocationContext
{
public:
	virtual ~Zenith_ScriptInvocationContext() = default;

	virtual Zenith_ScriptInvocationKind GetKind() const = 0;
	virtual uint64 GetTick() const = 0;
	virtual double GetDeltaTime() const = 0;
	virtual uint64 GetCurrentSimulationID() const = 0;   // the entity of the running entity transaction
	virtual uint16 GetVMIndex() const = 0;
	virtual uint64 NextRandomDraw() = 0;                  // one counter-based draw of the entity transaction

	virtual Zenith_Result<void, Zenith_ScriptThunkError> AppendSideEffect(
		Zenith_SideEffectKind eKind, Zenith_ByteSpan xPayload, const Zenith_ScriptCapacityDemand& xDemand) = 0;

	virtual Zenith_Result<Zenith_ScriptCallbackOutcome, Zenith_ScriptThunkError> InvokeCallback(
		Zenith_ScriptCallbackRef xCallback,
		const Zenith_ScriptValue* axArgs, uint32 uArgCount,
		Zenith_ScriptValue* axResults, uint32 uResultCapacity) = 0;

	virtual Zenith_Result<void, Zenith_ScriptThunkError> RetainCallback(Zenith_ScriptCallbackRef& xCallback) = 0;
	virtual void ReleaseCallback(Zenith_ScriptCallbackRef& xCallback) = 0;

	// Copies the text of an error InvokeCallback returned from the VM's error scratch (same thread) into xOut and returns the
	// bytes written; 0 when a subsequent error on this VM replaced it. It never reads the diagnostic store.
	virtual uint32 GetErrorMessage(const Zenith_ScriptThunkError& xError, Zenith_MutableByteSpan xOut) const = 0;

	// The user datum (Zenith_ScriptFunctionDesc::m_pUserData) of the function being called; null when none. A
	// SCRIPT_FUNCTION_EFFECT_BUFFERED thunk reads through it the command-kind IDs its module resolved at its first AttachWorld,
	// with no global.
	virtual const void* GetFunctionUserData() const = 0;
};

struct Zenith_ScriptCallFrame
{
	const Zenith_ScriptValue* m_axArgs = nullptr;
	uint32 m_uArgCount = 0;
	Zenith_ScriptValue* m_axResults = nullptr;
	uint32 m_uResultCapacity = 0;
	uint32 m_uResultCount = 0;
	Zenith_ScriptInvocationContext* m_pxContext = nullptr;   // null for SCRIPT_FUNCTION_CONTEXT_LOAD calls
	Zenith_ScriptThunkError m_xError;
	char m_acErrorMessage[256] = {};                         // UTF-8 message of a SCRIPT_THUNK_STATUS_ERROR; the dispatcher raises it verbatim
};

using Zenith_ScriptThunk = Zenith_ScriptThunkStatus (*)(Zenith_ScriptCallFrame& xFrame);

// The helper every thunk fails through, a template so its placeholders are checked at compile time and its body needs no
// header outside Zenith2/Docs/Contracts.md → "Allowed standard headers": formats m_acErrorMessage with Zenith_FormatTo (truncated
// at a code-point boundary), then calls the non-template tail (defined in Zenith_ScriptBinding.inl).
template <typename... TArgs>
Zenith_ScriptThunkStatus Zenith_ScriptFailThunk(Zenith_ScriptCallFrame& xFrame, Zenith_ScriptErrorKind eKind, uint16 uCode,
	Zenith_FormatString<std::type_identity_t<TArgs>...> xFormat, const TArgs&... xArgs);
// The tail: sets m_xError.m_eCode = Zenith_ScriptThunkErrorCodeFromKind(eKind) and m_uCode, and returns SCRIPT_THUNK_STATUS_ERROR
// (defined in ZenithReflect/Internal/Zenith_ScriptBindingTable.cpp).
Zenith_ScriptThunkStatus Zenith_ScriptFailThunkFinish(Zenith_ScriptCallFrame& xFrame, Zenith_ScriptErrorKind eKind, uint16 uCode);

/// @contract interface Zenith_ScriptSideEffectConsumer
/// Ownership: Implemented by the library that owns a SCRIPT-held side-effect kind (audio and VFX triggers: those modules;
///   save requests: ZenithRuntime over ZenithSave; asynchronous requests: ZenithRuntime's multiplexer over the owning submitters);
///   registered with Zenith_ScriptBindingTable::SetSideEffectConsumer before Seal; outlives every world.
/// Thread affinity: ANY_THREAD. Called on the committing world's merge node, in merge order; one world's calls never overlap,
///   two worlds' calls may run concurrently, so the consumer keeps per-world state keyed by uWorldTag.
/// Invalidation: xPayload is valid for the call.
/// Units: Payload bytes as the binding serialized them.
/// Errors: Consume cannot fail: the capacity the record needed was accepted at the merge before commit.
/// Reentrancy: NONE. A consumer never calls into script.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptSideEffectConsumer
{
public:
	virtual ~Zenith_ScriptSideEffectConsumer() = default;
	virtual void Consume(uint16 uWorldTag, Zenith_SideEffectKind eKind, uint64 ulCompletionID, Zenith_ByteSpan xPayload) = 0;
		// ulCompletionID is the assigned ID for SIDE_EFFECT_KIND_ASYNC_REQUEST, 0 otherwise
};

// A source kind's ticket-result decoder: fills named fields that ticket:Result() turns into one Luau table; false means the
// payload is undecodable (Result() then halts the world as an internal inconsistency).
inline constexpr uint32 uSCRIPT_ASYNC_RESULT_MAX_FIELDS = 8;
struct Zenith_ScriptResultField
{
	const char* m_szName = nullptr;            // static storage; the table key
	Zenith_ScriptValue m_xValue;               // SCRIPT_VALUE_KIND_NIL omits the key
	const char* m_szStaticString = nullptr;    // when set, the field is this static string and m_xValue is ignored
};
struct Zenith_ScriptResultFields
{
	Zenith_ScriptResultField m_axFields[uSCRIPT_ASYNC_RESULT_MAX_FIELDS];
	uint32 m_uFieldCount = 0;
};
using Zenith_ScriptAsyncResultDecoder = bool (*)(uint8 uPublicationKind, Zenith_ByteSpan xPayload,
	Zenith_ScriptResultFields& xOut);

enum Zenith_ScriptFunctionContext : uint8     // bit flags
{
	SCRIPT_FUNCTION_CONTEXT_SYSTEM    = 1 << 0,
	SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR = 1 << 1,
	SCRIPT_FUNCTION_CONTEXT_LOAD      = 1 << 2,   // callable from module scope while loading
	SCRIPT_FUNCTION_CONTEXT_TOOLS     = 1 << 3,   // editor console and tooling invocations only
};

enum Zenith_ScriptFunctionEffect : uint8
{
	SCRIPT_FUNCTION_EFFECT_PURE,          // no world access; result depends on arguments alone
	SCRIPT_FUNCTION_EFFECT_READ_WORLD,    // reads world resources declared in m_axResources
	SCRIPT_FUNCTION_EFFECT_BUFFERED,      // appends to the invocation's side-effect sub-buffer
};

struct Zenith_ScriptParamDesc
{
	Zenith_TypeID m_xType;
	Zenith_ScriptValueKind m_eKind;
	bool m_bOptional = false;
	const char* m_szLuauName = nullptr;   // for .d.luau parameter names
	const char* m_szResolveKind = nullptr;   // non-null: a string parameter resolved at load against this kind's name table
	                                         // (AddNameTable); the thunk receives the name's index, [Z2-N-SCRIPT-922]
};

struct Zenith_ScriptFunctionDesc
{
	Zenith_ScriptFunctionID m_xID;                   // stable across builds; part of the binding-set hash
	const char* m_szNamespace = nullptr;              // Luau table: "Physics" -> Zenith.Physics
	const char* m_szLuauName = nullptr;               // derived by the name mapping or given explicitly
	Zenith_ScriptThunk m_pfnThunk = nullptr;
	const Zenith_ScriptParamDesc* m_axParams = nullptr;
	uint32 m_uParamCount = 0;
	const Zenith_ScriptParamDesc* m_axResults = nullptr;
	uint32 m_uResultCount = 0;
	uint8 m_uContexts = 0;                           // Zenith_ScriptFunctionContext flags
	Zenith_ScriptFunctionEffect m_eEffect = SCRIPT_FUNCTION_EFFECT_PURE;
	const Zenith_WorldResourceID* m_axResources = nullptr;   // READ_WORLD only
	uint32 m_uResourceCount = 0;
	Zenith_SideEffectKindMask m_uSideEffects = 0;     // BUFFERED only: which record kinds it may append
	const void* m_pUserData = nullptr;                // borrowed; outlives the sealed table; not part of the binding-set hash
	uint8 m_uSinceStage = 0;                         // this and the next three: binding-set hash inputs
	Zenith_ScriptApiStability m_eStability = SCRIPT_API_STABILITY_STABLE;
	uint8 m_uExperimentalUntilStage = uSCRIPT_API_STAGE_NONE;
	Zenith_ScriptDeprecation m_xDeprecation;
	const char* m_szLibrary = nullptr;                // the registering library's CMake target name (SetRegistrantLibrary); not hashed
};

class Zenith_ScriptBindingTable
{
public:
	template<auto pfnFunction>
	void AddFunction(const char* szNamespace, const char* szLuauName, uint8 uContexts, Zenith_ScriptFunctionEffect eEffect,
		Zenith_Span<const Zenith_WorldResourceID> axResources = {}, Zenith_SideEffectKindMask uSideEffects = 0,
		const void* pUserData = nullptr, const Zenith_ScriptFunctionVersion& xVersion = Zenith_ScriptFunctionVersion{});

	// MAIN_THREAD, before Seal: szLibrary has static storage and stamps m_szLibrary of every later AddFunction.
	void SetRegistrantLibrary(const char* szLibrary);

	template<typename TEnum>
	void AddEnum(const char* szLuauName);      // values from reflection; names by the mapping

	template<typename TValue>
	void AddValueType(const char* szLuauName); // a reflected value struct, exposed as a frozen table or value userdata

	void AddConstant(const char* szNamespace, const char* szLuauName, double fValue);

	// Before Seal, MAIN_THREAD: the names of one kind ("action", "context", …) that parameters marked with that
	// m_szResolveKind accept; the kind and every name have static storage; [Z2-N-SCRIPT-922].
	void AddNameTable(Zenith_StringView strKind, Zenith_Span<const Zenith_StringView> axNames);
	// After Seal, ANY_THREAD (read-only): the table AddNameTable declared for strKind, in its given order; an empty span when
	// no table of that kind was declared. The loader and the dispatcher resolve marked arguments through it ([Z2-N-SCRIPT-922]).
	Zenith_Span<const Zenith_StringView> FindNameTable(Zenith_StringView strKind) const;

	void SetSideEffectConsumer(Zenith_SideEffectKind eKind, Zenith_ScriptSideEffectConsumer& xConsumer);   // AUDIO_TRIGGER, VFX_TRIGGER, SAVE_REQUEST, ASYNC_REQUEST, XR_RECENTRE
	void DeclareAsyncSource(uint8 uSourceKind, uint32 uMaxResultBytes);    // sizes each world's result arena
	void SetAsyncResultDecoder(uint8 uSourceKind, Zenith_ScriptAsyncResultDecoder pfnDecode);   // before Seal; one per kind
	Zenith_ScriptAsyncResultDecoder GetAsyncResultDecoder(uint8 uSourceKind) const;    // null: no decoder
	Zenith_ScriptSideEffectConsumer* GetSideEffectConsumer(Zenith_SideEffectKind eKind) const;
	const Zenith_ScriptFunctionDesc* FindFunction(Zenith_ScriptFunctionID xID) const;   // O(1) after Seal
	bool IsSealed() const;

	// Called once by the composition root after startup step 15 (every module's bindings and consumers registered), before the
	// first world; a collision or an invalid descriptor is an error value, which the composition root maps to exit code 3.
	Zenith_Result<void, Zenith_ScriptBindingError> Seal();
	Zenith_Span<const Zenith_ScriptFunctionDesc> GetFunctions() const;
};
```

- [Z2-N-SCRIPT-922] A binding owner whose functions take names that resolve when a module loads (PLAT's `Input` bindings in `ZenithRuntime/Internal/ScriptBindings/Zenith_InputScriptBindings.cpp`: action and context names, `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API") declares each name kind once with `Zenith_ScriptBindingTable::AddNameTable` before `Seal` and marks each such parameter with that kind in `Zenith_ScriptParamDesc::m_szResolveKind`; this is the whole load-time resolution interface, so the owner needs no ZenithScript type and the loader never calls back into the owner. When a module loads, the loader resolves every string-constant argument the module passes to a marked parameter against the kind's table and records the result in the module's load context; a name the table lacks raises ZS1807 (`unknown input <Kind> '<Name>' in <Module>`, `Zenith2/Docs/Scripting/MisuseCatalogue.md`) at load, never an assert. At a call, the dispatcher passes the thunk the name's index in the table given to `AddNameTable` as a `SCRIPT_VALUE_KIND_NUMBER`; a non-constant argument is looked up in the same table at the call, and a name it lacks is the catalogued bad-argument misuse error at the call. `Seal` refuses a marked parameter whose kind has no table (`SCRIPT_BINDING_ERROR_UNKNOWN_NAME_KIND`) and a second table for one kind or a name repeated within a table (`SCRIPT_BINDING_ERROR_DUPLICATE_NAME_TABLE`), and every name table is part of the binding-set hash.

### What AddFunction generates

`AddFunction<pfnFunction>` instantiates, in the calling translation unit (the owning engine library, exceptions off):

1. `static_assert(std::is_invocable_v<decltype(pfnFunction), Args...>)` over the function's parameter types, with the message `"script bindings require engine functions (Scripting/Bindings.md)"`; no exception-specification check is made, because an engine function carries none (`Zenith2/Docs/Scripting.md` [Z2-N-SCRIPT-043]).
2. `static_assert` that every parameter and result type has a `Zenith_ScriptValueKind` mapping (table "Value marshalling"), except an optional leading `Zenith_ScriptInvocationContext&` parameter.
3. A `Zenith_ScriptParamDesc` array for parameters and results, from reflection.
4. The thunk: a function, compiled with exceptions off and carrying no exception specification, that reads each argument from `m_axArgs[i]` into the C++ parameter type (argument kinds are already checked by the dispatcher), calls `pfnFunction`, writes the results into `m_axResults`, and maps a `Zenith_Result` error return to `SCRIPT_THUNK_STATUS_ERROR` through `Zenith_ScriptFailThunk` (kind `SCRIPT_ERROR_KIND_RUNTIME`, format `"{}"` over `Zenith_ToString` of the error's code, because a Contracts-form error carries no text; the template form needs no `<cstdarg>` or `<cstdio>`, so the thunk compiles under Zenith2/Docs/Contracts.md → "Allowed standard headers").
5. A `Zenith_ScriptFunctionDesc` appended to the table, with `m_xID` = `Hash64(namespace + "." + Luau name)`, checked for collisions when the table is sealed, and `m_pUserData` = `pUserData`, which the dispatcher hands back through `Zenith_ScriptInvocationContext::GetFunctionUserData` for every call of that function, and the version members from `xVersion` and `m_szLibrary` from the registrant library.

```cpp
// Zenith2/Engine/ZenithPhysics/Internal/Zenith_PhysicsScriptBindings.cpp (a module's registration, exceptions off)
#include "ZenithPhysics/Zenith_PhysicsScriptApi.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"

void Zenith_RegisterPhysicsScriptBindings(Zenith_ScriptBindingTable& xTable)
{
	xTable.AddFunction<&Zenith_PhysicsScriptApi::RayCast>("Physics", "RayCast",
		SCRIPT_FUNCTION_CONTEXT_SYSTEM | SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR,
		SCRIPT_FUNCTION_EFFECT_READ_WORLD, { &xPHYSICS_WORLD_RESOURCE, 1 });
	xTable.AddFunction<&Zenith_PhysicsScriptApi::ApplyImpulse>("Physics", "ApplyImpulse",
		SCRIPT_FUNCTION_CONTEXT_SYSTEM | SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR,
		SCRIPT_FUNCTION_EFFECT_BUFFERED, {}, uSIDE_EFFECT_MASK_FIELD_COMMAND);
}
```

- [Z2-N-SCRIPT-562] A binding registered with `SCRIPT_FUNCTION_EFFECT_READ_WORLD` names the world resources it reads, and a script module that calls it lists those resources in its declaration's `Uses` key, so the schedule compiler sees the access; calling it without the resource in `Uses` raises the catalogued misuse error.
- [Z2-N-SCRIPT-563] A `SCRIPT_FUNCTION_EFFECT_BUFFERED` binding appends only record kinds in its `m_uSideEffects` mask; the append helper checks the mask on every append and reports a violation as `SCRIPT_THUNK_STATUS_ERROR`, never as an assert.
- [Z2-N-SCRIPT-564] `Zenith_ScriptBindingTable` is sealed by the composition root before the first world is created; `AddFunction` after sealing is a programmer error caught by an assert, because it is engine code and not script misuse.
- [Z2-N-SCRIPT-567] `Seal` sorts the functions by (namespace, Luau name) byte order, validates every descriptor and consumer and returns the first failure in sealed order as a `Zenith_ScriptBindingError` with the table left unsealed and nothing asserted; the composition root maps any `Seal` error to startup exit code 3.
- [Z2-N-SCRIPT-568] A thunk fails only through `Zenith_ScriptFailThunk`, which fills the 8-byte `Zenith_ScriptThunkError` (`m_eCode` = kind + 1, `m_uCode`) and the call frame's `m_acErrorMessage`; the dispatcher raises that message verbatim with the kind `Zenith_ScriptThunkErrorKind` returns.

### Dispatcher algorithm

Each registered function is exposed to Luau as a C closure `Zenith_ScriptDispatch` with the function descriptor as light-userdata upvalue 1. On a call:

1. Construct `Zenith_ScriptDispatchScope` (RAII): records the frame-arena mark, increments the VM's live-dispatch counter.
2. Check the calling context against `m_uContexts` (the VM's current invocation kind); on mismatch raise the context misuse error.
3. For `READ_WORLD`, check `m_axResources` against the running module's `Uses`; on mismatch raise the resource misuse error.
4. Check the argument count against the descriptor (optional trailing parameters may be absent); on mismatch raise the arity misuse error.
5. Convert each argument from the Luau stack into a `Zenith_ScriptValue` in the frame arena (table "Value marshalling"); a conversion failure raises ZS1511 with its exact catalogue text through `luaL_error` (not `luaL_typeerrorL`, whose text is Luau's), inside this exception-enabled translation unit while the scope is live.
6. Build `Zenith_ScriptCallFrame` with the invocation context and call the thunk.
7. On `SCRIPT_THUNK_STATUS_ERROR`: push `xFrame.m_acErrorMessage` as a Luau string, set `m_eRaisedKind` to `Zenith_ScriptThunkErrorKind(xFrame.m_xError)`, and raise (an abort kind is raised as the Zenith abort value, which the Zenith `pcall` re-raises).
8. On success: push each result converted to its Luau representation; return the result count.
9. `Zenith_ScriptDispatchScope` restores the frame-arena mark and decrements the counter, whether step 7 raised or step 8 returned.

### Non-raising Luau calls

The Luau API calls ZenithScript may make outside a protected region (they neither allocate nor raise):

| Call | Condition |
|---|---|
| `lua_gettop`, `lua_type`, `lua_typename`, `lua_isnumber`, `lua_isstring`, `lua_isuserdata` | always |
| `lua_toboolean`, `lua_tonumberx`, `lua_tointegerx`, `lua_tounsignedx`, `lua_tovector` | always |
| `lua_touserdata`, `lua_touserdatatagged`, `lua_userdatatag`, `lua_tolightuserdata` | always |
| `lua_settop` | when lowering the stack top only |
| `lua_rawequal`, `lua_status`, `lua_costatus`, `lua_stackdepth` | always |
| `lua_newstate`, `lua_close`, `lua_callbacks` | VM construction and destruction only |

`lua_tolstring` is not on the list, because converting a number to a string allocates.

## Value marshalling

| C++ reflected type | Value kind | Luau type in `.d.luau` | Read (C++ → Luau) | Write (Luau → C++) and its errors |
|---|---|---|---|---|
| `bool` | `BOOL` | `boolean` | boolean | boolean required; anything else raises ZS1501 |
| `uint8`, `uint16`, `uint32`, `sint8`, `sint16`, `sint32` | `NUMBER` | `number` | exact | integral and in range required; non-integral raises ZS1502, out of range ZS1503 |
| `uint64`, `sint64` | `NUMBER` | `number` | exact when \|v\| ≤ 2⁵³, else raises ZS1504 | integral, \|v\| ≤ 2⁵³ |
| `float` | `NUMBER` | `number` | widened | rounded to nearest f32; a finite value that overflows f32 raises ZS1503 |
| `double` | `NUMBER` | `number` | exact | exact |
| `Zenith_Vector2` | `VECTOR` | `vector` (z ignored, written as 0) | `vector.create(x, y, 0)` | `vector` required |
| `Zenith_Vector3` | `VECTOR` | `vector` | `vector.create(x, y, z)` | `vector` required |
| `Zenith_Vector4`, `Zenith_Quaternion`, `Zenith_Colour` | `VEC4` | `Zenith.Vec4`, `Zenith.Quat`, `Zenith.Colour` (declared classes `ZenithVec4`, `ZenithQuat`, `ZenithColour`) | immutable value userdata (tagged, 16 bytes) | the matching class required; a `Quat` is renormalised on write when its length differs from 1 by more than 10⁻⁴, and a zero-length one raises ZS1505 |
| `Zenith_StringID` (interned string) | `STRING_ID` | `string` | the interned string's text | a Luau string; interned at merge (capacity class string intern) |
| reflected enum `Zenith_Xxx` | `ENUM` | string-literal union `"ValueA" \| "ValueB"` | the mapped value name | one of the mapped names; anything else raises ZS1506 listing the valid names |
| `Zenith_EntityHandle` | `ENTITY` | `Zenith.Entity` (declared class `ZenithEntity`) | tagged userdata; `nil` for the null handle | an `Entity` of the same world or `nil`; another world's raises ZS1102 |
| `Zenith_AssetHandle<T>` | `ASSET` | `Zenith.Asset<TKind>` | tagged userdata carrying the asset type | an `Asset` of the same asset type; mismatch raises ZS1507 |
| `Zenith_PersistentID` | `PERSISTENT_ID` | `Zenith.PersistentId` (declared class `ZenithPersistentId`) | tagged userdata | a `Zenith.PersistentId` |
| reflected value struct | `STRUCT` | a generated table type | a new frozen table with the struct's fields (behaviour API) or a nested view (component views) | a table with every declared field; a missing or unknown key raises ZS1508 |
| fixed array `T[N]` in a component | — | `Zenith.FixedArray<T>` view | indexed `a[i]`, 1-based, `#a == N` | through the view, own row; out-of-range index raises ZS1509 |
| fixed array `T[N]` field of a reflected value struct returned by value | — | `{ T }` | a frozen Luau array of its N elements, 1-based; a null handle element reads as `nil` | a table of at most N elements fills the field in order, absent elements default; more than N raises ZS1509 |
| out-of-line container | — | `Zenith.Container<T>` object | behaviour API only | behaviour API only, through the copy-on-write shadow |
| Luau function argument | `CALLBACK` | a function type | — | stored as a registry reference, released at the end of the invocation unless the thunk retains it with `Zenith_ScriptInvocationContext::RetainCallback`; a retained reference lives until `ReleaseCallback`, quarantine of the owning instance, or world teardown, and a view it captured raises ZS1001 when used in a subsequent invocation |

- [Z2-N-SCRIPT-581] Every marshalling failure in the table "Value marshalling" raises its catalogued misuse error; no conversion silently truncates, wraps or clamps.

## Component views through reflection

### Row proxies and component views

- A chunk-system `Each` receives `e`, the VM's single row-proxy userdata for the invocation, re-pointed at each row by the trampoline; `e.Entity` is the row's `Entity`, `e.<Component>` is a component view.
- A behaviour obtains a component view with `self:Get(T.<Component>)`, which is non-optional for a component named in its `Reads` or `Writes` and raises ZS1212 when the entity lacks it; `self:TryGet(T.<Component>)` returns `nil` instead; views of other entities, which it reads without writing, come from `ctx:View(entity, T.<Component>)`, which returns `nil` when the entity lacks the component.
- A component view's `__index` and `__newindex` map a field name to the field's reflection descriptor through a per-module perfect-hash table built at load from the declaration's field sets, so undeclared fields are unreachable by construction.
- Nested structs are views of their parent's row at the nested field's offset; fixed arrays are indexable views.

### Field access algorithm

Read of `e.<Component>.<Field>`:

1. Validate the view (Scripting.md → "Views epochs and misuse errors").
2. Look up the field in the module's perfect-hash table; absent raises ZS1202.
3. For another entity's row, apply the cross-entity read rule; a violation raises ZS1204.
4. Read the field's bytes through the property's `Zenith_RawColumn` (`xChunk.GetColumn(xType, xProperty)`; row `r` at `m_pData + (m_puRowIndex ? m_puRowIndex[r] : r) * m_uStride`) and push them converted by "Value marshalling"; another entity's row in a different chunk is read through `Zenith_World::Resolve` and that chunk's view (`Zenith2/Docs/Architecture.md` → "ECS model").

Writes follow Scripting.md → "Write path algorithm".

### Behaviour container API

| Call | Effect |
|---|---|
| `self:GetContainer(T.Comp.Field)` | returns the `Container<T>` object for the own entity's field (creates the shadow on first access in the invocation) |
| `c:Count()` | element count of the shadow |
| `c:Get(i)` | element `i` (1-based); out of range raises ZS1509 |
| `c:Set(i, v)` | writes element `i` in the shadow |
| `c:Insert(v)` / `c:InsertAt(i, v)` | appends or inserts; growth reserves shadow capacity first |
| `c:Remove(i)` / `c:Clear()` | removes |
| `for i, v in c:Items() do` | iterates the shadow in index order |

- [Z2-N-SCRIPT-601] Container objects are views: they are valid only in the invocation and segment that created them and raise ZS1001 or ZS1002 afterwards.

## Luau type generation

### Outputs

ZenithCook's `luau-types` step, and the editor on every registry change, call `Zenith_ScriptEmitDefinitions` and write:

| File | Contents |
|---|---|
| `zenith.d.luau` | global declarations of the engine's value classes, each with the `Zenith` prefix (`ZenithEntity`, `ZenithQuat`, `ZenithVec4`, `ZenithColour`, `ZenithPersistentId`, `ZenithAsyncTicket`, `ZenithRandom`), and of the two native tables the generated modules read (`__zenith_native`, `__zenith_types_native`), under a header comment `-- generated; engine <szZENITH_ENGINE_VERSION>; binding-set hash 0x<16 hex digits>` |
| `components.d.luau` | one view class per reflected component (C++-registered and data-defined), named `<Component>View`, and the `ZenithRow` class with one non-optional property per component (an access to a component the system does not declare raises ZS1211 at runtime) |
| `Zenith.luau` | the `@zenith` prelude module: `export type` of every `Zenith.*` type (table "Prelude type exports"), the typed `Zenith` API (the hand-written `@zenith/Core` functions `Behaviour`, `ChunkSystem`, `Field`, `Wait*`, every registered binding namespace, enum and constant) and `return` of the engine's frozen API table cast to that type; a `--- deprecated Z2 Stage <D>, use <R>, removed Z2 Stage <D+1>` or `--- experimental until Z2 Stage <G>` line above each such function's signature |
| `Types.luau` | the `@types` module: `export type <Event>Event` for every event payload (for example `InteractEvent`, so an annotation reads `event: T.InteractEvent`), and `return` of the engine's descriptor table cast to a type in which `T.<Component>` is `Zenith.ComponentDescriptor<<Component>View>` intersected with one `Zenith.FieldDescriptor<…>` per field and `T.Events.<Event>` is `Zenith.EventDescriptor<<Event>Event>` |
| `bindings.json` | the binding listing `zenith2.script-binding-listing` 1 of `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Binding listing" |
| `.luaurc` | `{"languageMode": "strict", "aliases": {"zenith": …, "types": …, "game": "<game>/Scripts", …}}` |

- [Z2-N-SCRIPT-621] Generated definition files and modules are byte-stable: the component, struct and event sections come from ZenithReflect's `Zenith_EmitComponentLuauTypes`, which orders types by stable name and fields by field ID (`Zenith2/Docs/Architecture.md` → "Data-defined components", [Z2-N-ARCH-611]), binding functions follow the (namespace, Luau name) byte order `Seal` produces, every other declaration is sorted by Luau name, the header comment carries the engine version and the binding-set hash (the value `Zenith_ScriptBindingSetHash` computes, defined in Scripting.md → "Shipping bytecode compatibility") and no timestamp, and two generations from the same registry produce identical bytes.
- [Z2-N-SCRIPT-622] The generated `@zenith` prelude module `export type`s every type of the table "Prelude type exports", so every `Zenith.X` written in type position by a simulation module resolves under strict mode; engine value classes are declared globally with the `Zenith` prefix so the prelude's aliases are not self-referential.
- [Z2-N-SCRIPT-623] `Zenith.luau` and `Types.luau` are generated per game by the `luau-types` step, type-checked and cooked like any module; at runtime they read the engine's frozen native tables, so the analysed types and the runtime values come from the same registry.

### Declaration shapes

```luau
-- zenith.d.luau (excerpt) — generated; binding-set hash 0x5E1F0A93C2D47B10
declare class ZenithEntity
	function Id(self): number                 -- stable simulation ID
	function IsValid(self): boolean
	function PersistentId(self): ZenithPersistentId?
end

declare class ZenithQuat
	X: number
	Y: number
	Z: number
	W: number
	function Rotate(self, v: vector): vector
end

declare class ZenithRandom
	function NextU32(self): number
	function NextFloat(self): number
	function NextFloat32(self): number
	function Range(self, a: number, b: number): number
	function RangeFloat(self, a: number, b: number): number
	function UnitVector(self): vector
	function Chance(self, p: number): boolean
end

declare __zenith_native: any          -- the engine's frozen API table; read by Zenith.luau alone
declare __zenith_types_native: any    -- the engine's frozen descriptor table; read by Types.luau alone
```

```luau
-- components.d.luau (excerpt)
declare class TransformView
	LocalPosition: vector
	LocalRotation: ZenithQuat
	LocalScale: vector
end

declare class StatsView                       -- data-defined: Games/OpenWorldSlice/Schemas/Stats.schema.json
	Stamina: number
	MaxStamina: number
	RegenPerSecond: number
end

declare class ZenithRow
	Entity: ZenithEntity
	Transform: TransformView
	Stats: StatsView
end
```

```luau
-- Zenith.luau, the generated @zenith prelude (excerpt)
--!strict
local Core = require("@zenith/Core")         -- hand-written Behaviour, ChunkSystem, Field, Wait*

export type Entity = ZenithEntity
export type Quat = ZenithQuat
export type Random = ZenithRandom
export type Row = ZenithRow
export type ComponentDescriptor<TView> = { StableName: string, _View: TView }
export type FieldDescriptor<TValue> = { StableName: string, Field: string, _Value: TValue }
export type EventDescriptor<TPayload> = { StableName: string, _Payload: TPayload }
export type Container<T> = {
	Count: (self: Container<T>) -> number,
	Get: (self: Container<T>, i: number) -> T,
	Set: (self: Container<T>, i: number, v: T) -> (),
	Insert: (self: Container<T>, v: T) -> (),
	InsertAt: (self: Container<T>, i: number, v: T) -> (),
	Remove: (self: Container<T>, i: number) -> (),
	Clear: (self: Container<T>) -> (),
	Items: (self: Container<T>) -> () -> (number, T),
}
export type FixedArray<T> = {             -- a fixed-size array field of a component view (own row writable)
	Count: (self: FixedArray<T>) -> number,
	Get: (self: FixedArray<T>, i: number) -> T,
	Set: (self: FixedArray<T>, i: number, v: T) -> (),
}
export type Asset<TKind> = {               -- a shared asset handle (C++ owner Zenith_AssetHandle<T>, ZenithAssets; its API is pending ASSET)
	Id: (self: Asset<TKind>) -> ZenithPersistentId,
	IsLoaded: (self: Asset<TKind>) -> boolean,
	_Kind: TKind,
}
export type Self = {
	Entity: ZenithEntity,
	Get: <TView>(self: Self, desc: ComponentDescriptor<TView>) -> TView,
	TryGet: <TView>(self: Self, desc: ComponentDescriptor<TView>) -> TView?,
	GetContainer: <T>(self: Self, desc: FieldDescriptor<Container<T>>) -> Container<T>,
}
export type SystemContext = {
	Tick: number,
	Time: number,
	DeltaTime: number,
	Random: ZenithRandom,
	Emit: <TPayload>(self: SystemContext, desc: EventDescriptor<TPayload>, payload: TPayload) -> (),
	Contribute: (self: SystemContext, name: string, value: number) -> (),
	Read: <TValue>(self: SystemContext, entity: ZenithEntity, desc: FieldDescriptor<TValue>) -> TValue,
}
export type BehaviourContext = SystemContext & {
	View: <TView>(self: BehaviourContext, entity: ZenithEntity, desc: ComponentDescriptor<TView>) -> TView?,
}

type ZenithApi = typeof(Core) & {
	Physics: {
		RayCast: (origin: vector, direction: vector, maxDistance: number, layers: CollisionLayer?) -> RayHit?,
		ApplyImpulse: (entity: ZenithEntity, impulse: vector) -> (),
	},
}
export type CollisionLayer = "Default" | "Static" | "Character" | "Projectile" | "Trigger"

return (__zenith_native :: any) :: ZenithApi
```

```luau
-- Types.luau, the generated @types module (excerpt)
--!strict
local Zenith = require("@zenith")

export type InteractEvent = { Instigator: Zenith.Entity, Target: Zenith.Entity }
export type DoorOpenedEvent = { Door: Zenith.Entity, Opener: Zenith.Entity? }

type Types = {
	DoorLeaf: Zenith.ComponentDescriptor<DoorLeafView> & { Angle: Zenith.FieldDescriptor<number> },
	Stats: Zenith.ComponentDescriptor<StatsView> & {
		Stamina: Zenith.FieldDescriptor<number>,
		MaxStamina: Zenith.FieldDescriptor<number>,
		RegenPerSecond: Zenith.FieldDescriptor<number>,
	},
	Events: {
		Interact: Zenith.EventDescriptor<InteractEvent>,
		DoorOpened: Zenith.EventDescriptor<DoorOpenedEvent>,
	},
}

return (__zenith_types_native :: any) :: Types
```

| Prelude type export | Meaning |
|---|---|
| `Zenith.Entity`, `Zenith.Quat`, `Zenith.Vec4`, `Zenith.Colour`, `Zenith.PersistentId`, `Zenith.AsyncTicket`, `Zenith.Random` | the engine value classes, aliases of the `Zenith`-prefixed declared classes |
| `Zenith.Row` | the chunk-system row (`ZenithRow`) |
| `Zenith.Self` | the behaviour instance handle, with generic `Get`, `TryGet`, `GetContainer` |
| `Zenith.SystemContext`, `Zenith.BehaviourContext` | the invocation contexts |
| `Zenith.ComponentDescriptor<TView>`, `Zenith.FieldDescriptor<TValue>`, `Zenith.EventDescriptor<TPayload>` | the descriptor types of `@types` values |
| `Zenith.Container<T>`, `Zenith.FixedArray<T>`, `Zenith.Asset<TKind>` | container, fixed-array and asset handle types |

## Name mapping from Zenith Hungarian

### Rules

1. **Types.** Strip the namespace prefix (`Zenith_`, `Flux_`, or a module's registered prefix), then strip a trailing `Component` from component types. `Zenith_TransformComponent` → `Transform`; `Zenith_RayHit` → `RayHit`; `Flux_LightDesc` → `LightDesc`.
2. **Members.** Strip the scope prefix (`m_`, `s_`), then the type prefix: the longest sequence of tokens from {`str`, `pfn`, `px`, `sz`, `ul`, `a`, `b`, `e`, `f`, `i`, `p`, `u`, `x`} (longest token tried first at each position) that is immediately followed by an uppercase letter. The remainder, already PascalCase, is the Luau name.
3. **Functions.** PascalCase names are kept: `GetPosition` → `GetPosition`. Member functions become methods (`entity:Id()`), free functions fields of their namespace table (`Zenith.Physics.RayCast`).
4. **Enums.** The type name follows rule 1. Each value strips the enum's category prefix (the longest common prefix of all its values ending in `_`), then converts SCREAMING_SNAKE to PascalCase by capitalising the first letter of each `_`-separated word and lowercasing the rest, keeping digits: `TEXTURE_FORMAT_RGBA8_UNORM` → `Rgba8Unorm`. Values become a string-literal union.
5. **Constants.** Strip the type prefix (lowercase letters before the first uppercase letter) and keep SCREAMING_SNAKE: `uFLUX_MAX_TARGETS` → `FLUX_MAX_TARGETS`, exposed as a frozen table field of its namespace.
6. **Overrides.** A registration may pass an explicit Luau name; it replaces the derived one and is still checked by rule 7.
7. **Validation.** A derived or explicit name that is empty, starts with a digit, is a Luau keyword (`and`, `break`, `do`, `else`, `elseif`, `end`, `false`, `for`, `function`, `if`, `in`, `local`, `nil`, `not`, `or`, `repeat`, `return`, `then`, `true`, `until`, `while`, `continue`, `export`, `type`, `typeof`) or collides with another name in the same scope is a generation error naming both C++ sources.

### Mapping cases

| C++ name | Kind | Luau name | Rule |
|---|---|---|---|
| `Zenith_TransformComponent` | component type | `Transform` | 1 |
| `Zenith_CharacterControllerComponent` | component type | `CharacterController` | 1 |
| `Zenith_RayHit` | value struct | `RayHit` | 1 |
| `m_xPosition` | member | `Position` | 2 |
| `m_fMaxSpeed` | member | `MaxSpeed` | 2 |
| `m_uEntityID` | member | `EntityID` | 2 |
| `m_bIsOpen` | member | `IsOpen` | 2 |
| `m_strName` | member | `Name` | 2 (`str` matched) |
| `m_pxParent` | member | `Parent` | 2 (`px` preferred over `p`) |
| `m_szLabel` | member | `Label` | 2 (`sz` matched) |
| `m_axWaypoints` | member | `Waypoints` | 2 (`a` then `x`) |
| `m_ulFrameCount` | member | `FrameCount` | 2 |
| `m_eState` | member | `State` | 2 |
| `m_pfnCallback` | member | `Callback` | 2 |
| `m_iOffset` | member | `Offset` | 2 |
| `m_uUVSet` | member | `UVSet` | 2 |
| `m_fX` | member | `X` | 2 |
| `m_bb` | member | generation error (no uppercase letter after the prefix) | 7 |
| `m_xType` | member | `Type` (a legal Luau property name; `type` lowercase would be reserved) | 7 |
| `GetWorldPosition` | function | `GetWorldPosition` | 3 |
| `COLLISION_LAYER_CHARACTER` in `Zenith_CollisionLayer` | enum value | `"Character"` | 4 |
| `TEXTURE_FORMAT_RGBA8_UNORM` | enum value | `"Rgba8Unorm"` | 4 |
| `SCRIPT_CALL_STATUS_RETURNED` | enum value | `"Returned"` | 4 |
| `uFLUX_MAX_TARGETS` | constant | `FLUX_MAX_TARGETS` | 5 |
| `fCHAR_ASPECT_RATIO` | constant | `CHAR_ASPECT_RATIO` | 5 |
| `m_xPosition` and `m_fPosition` in one type | members | generation error: both map to `Position` | 7 |

- [Z2-N-SCRIPT-641] The mapping cases table is the golden input of `Test_Script_Types_NameMapping`, and a change to any rule updates the table and the test in the same diff.

## Data-defined component types

Data-defined components (Z2-D-020) are declared in `Zenith2/Games/<Name>/Schemas/*.schema.json` (the schema format is `Zenith2/Docs/Architecture.md` → "Data-defined components") and registered with ZenithReflect at game load; the `.d.luau` generator sees them exactly as it sees C++-registered types.

| Schema field type | Luau type | View behaviour |
|---|---|---|
| `bool` | `boolean` | read/write |
| `i8`…`i32`, `u8`…`u32` | `number` | integral, range-checked writes |
| `i64`, `u64` | `number` | limited to ±2⁵³ |
| `f32`, `f64` | `number` | f32 rounds on write |
| `vec2`, `vec3` | `vector` | read/write |
| `vec4`, `quat`, `colour` | `Zenith.Vec4`, `Zenith.Quat`, `Zenith.Colour` | read/write |
| `string` (interned ID) | `string` | read-only in chunk views; behaviour writes through the shadow |
| `persistentId` | `Zenith.PersistentId` | read-only in chunk views |
| `entity` | `Zenith.Entity?` | read-only in chunk views; changed through commands |
| `asset<T>` | `Zenith.Asset<T>?` | read-only in chunk views; changed through commands |
| `array<T, N>` (fixed) | `Zenith.FixedArray<T>` | element writes on own row |
| `struct` (nested) | the nested view class | as its fields |
| `container<T>` (out of line) | `Zenith.Container<T>` | behaviour API only |

- [Z2-N-SCRIPT-661] A schema's type name and field names are used verbatim as Luau names (no Hungarian mapping applies), are validated as PascalCase Luau identifiers by rule 7 of "Name mapping from Zenith Hungarian", and a schema that fails validation is a load error naming the schema file.
- [Z2-N-SCRIPT-662] A data-defined component's view is indistinguishable from a C++-registered component's view to scripts: same view classes, same validation, same transaction handling.

Example: `Games/OpenWorldSlice/Schemas/Stats.schema.json` declares `Stats` with fields `Stamina: f32`, `MaxStamina: f32`, `RegenPerSecond: f32`; the generator emits `declare class StatsView` with three `number` properties and adds `Stats: StatsView` to `Row` (see "Declaration shapes").

## Environments and library subsets

### Chunk-system environment

| Name | Present | Notes |
|---|---|---|
| `assert`, `error`, `ipairs`, `next`, `pairs`, `rawequal`, `rawget`, `rawlen`, `select`, `tonumber`, `type`, `typeof`, `unpack` | yes | Luau built-ins |
| `pcall`, `xpcall` | Zenith replacement | re-raise abort-class and fatal errors |
| `tostring` | Zenith replacement | no addresses |
| `print` | Zenith replacement | development builds: engine log, rate-limited to 64 lines per module per tick; shipping: no-op |
| `setmetatable`, `getmetatable` | Zenith replacements | `setmetatable` refuses `__mode`, `__gc`, `__metatable` with ZS1601; `getmetatable` returns `nil` for engine userdata and proxies |
| `require` | Zenith loader | module scope only |
| `rawset` | stub raising ZS1602 | bypasses state-proxy and view validation |
| `getfenv`, `setfenv` | stub raising ZS1603 | environment smuggling |
| `collectgarbage`, `gcinfo` | stub raising ZS1604 | GC observation |
| `newproxy` | stub raising ZS1605 | mutable userdata |
| `loadstring`, `load`, `dofile`, `loadfile` | absent (no global) | no code loading exists |
| `math.*` except `random`, `randomseed` | yes | `math.noise` is deterministic and present |
| `math.random`, `math.randomseed` | stub raising ZS1606 | use `ctx.Random` |
| `os` | table of stubs raising ZS1607 | wall-clock |
| `coroutine.*` | every function a stub raising ZS1301 | systems never yield |
| `table.*` | yes | |
| `string.*` | yes | |
| `utf8.*`, `bit32.*`, `vector.*`, `buffer.*` | yes | a module-scope buffer is rejected by D-LUAU-STATELESS (open, Stage 1) |
| `debug.traceback`, `debug.info` | yes | |
| `io`, `package`, `debug.*` others | absent | |
| `Zenith` | yes | the `@zenith` prelude and system-context bindings |

- [Z2-N-SCRIPT-682] Every name this table marks as a Zenith replacement or stub is compiled as a mutable global, so optimised bytecode reaches the replacement (Scripting.md → "Modules, bindings and generated types").

### Behaviour environment

Identical to the chunk-system environment except:

| Name | Behaviour environment |
|---|---|
| `coroutine.yield` | callable only from inside the instance's `Run` coroutine (directly, equivalent to `Zenith.WaitTicks(ctx, 1)`, or through `Zenith.Wait*`); elsewhere raises ZS1302 |
| `coroutine.isyieldable`, `coroutine.running`, `coroutine.status` | present |
| `coroutine.create`, `coroutine.wrap`, `coroutine.resume`, `coroutine.close` | stubs raising ZS1303 (the engine owns the one coroutine per instance) |
| behaviour bindings (`self:Get`, `self:GetContainer`, `ctx:View`, …) | present |

- [Z2-N-SCRIPT-681] The two tables of this section are the complete environments, and `Test_Script_Sandbox_EnvironmentSubset` asserts every row of both.

## Module loader

### Cook time

1. ZenithCook walks `Scripts/**.luau` of the game and each selected module's `Scripts/`, assigns module IDs in sorted path order, and resolves every `require` literal against the aliases, producing the module manifest (path → ID, dependencies).
2. It rejects a non-literal `require`, an unresolvable path, or a cycle.
3. It type-checks with `luau-analyze` against the generated definitions, runs the address-keyed-iteration lint, runs `Zenith_ScriptCheckStateless` per module with its kind, and compiles each module with `Zenith_ScriptCompileModule`.
4. It writes one `.zlbc` per module and the manifest.

### Load time per VM

1. `require(literal)` looks up the literal in the manifest (O(1) hash), raising ZS1801 for an unknown module.
2. If the module is cached for the current generation, return the cached table.
3. If the module is on the loader's in-progress stack, raise ZS1802 with the cycle.
4. Verify the `.zlbc` header (Scripting.md → "Load-time verification") and `luau_load` it through `Zenith_ScriptProtectedCall`, under the module-load safepoint row.
5. Run the module's top-level chunk (module scope) in the role's environment; this is the one window in which `require` is callable.
6. Validate the returned value: a `Zenith.ChunkSystem` or `Zenith.Behaviour` declaration is validated (Scripting.md → "Behaviours and chunk systems"), and its state schema registered.
7. Deep-freeze every table reachable from the returned value; cache it; return it.

- [Z2-N-SCRIPT-701] Module load order within a VM is the order in which `require` calls execute, which is deterministic for a given manifest; module load never changes simulation state.
