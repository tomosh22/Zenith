# CORE design: strings, Unicode, formatting and JSON

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). It specifies the text types of ZenithCore — `Zenith_StringView`, `Zenith_String`, interned `Zenith_StringID` and its `Zenith_StringTable` — the Unicode policy and functions, type-safe formatting, number conversion, the JSON writer and reader with both D-SEL-JSON (open, Stage 1) backend paths, and the document-tree cursor `Zenith_DocumentTree` that migration steps resolve JSON Pointers through.

## Scope of this file

| Topic | Fixed elsewhere | Specified here |
|---|---|---|
| names `Zenith_StringView`, `Zenith_JsonWriter`, `Zenith_StringID` | `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") | full APIs, algorithms, contracts |
| JSON parser limits | `Zenith2/Docs/Architecture.md` → "Security architecture" (table "Parser limits") | how the reader enforces them |
| unknown-field preservation and canonical order | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" | the reader and writer mechanisms that make it possible; canonical key order itself is SCENE's serializer's (`Zenith2/Docs/Roadmap.md` → "Stage 2 Foundation", deliverable 4) |
| the interned string field of data-defined components | `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" | the table, its thread safety and the determinism rule |
| the migration step signature that takes a `Zenith_DocumentTree&`; the document-pointer rules D1 to D4 | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" (subsection "Migration step interface"); `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Document pointers" | the cursor, its pointer grammar G1 to G4 (the same rules) and its resolution steps |
| text shaping, bidirectional text, font fallback | UI, per D-SEL-TEXT (open, Stage 1/2) | nothing |

## Encoding policy

<!-- elaboration -->
| Rule | Detail |
|---|---|
| storage encoding | every runtime string is UTF-8; `Zenith_String` holds valid UTF-8 by invariant; `Zenith_StringView` may view any bytes, and every consumer of external bytes validates them before treating them as text |
| normalisation | Zenith2 performs no Unicode normalisation at run time or cook time; text is stored and compared byte for byte as authored; equivalence of canonically equal but differently encoded text is the text-shaping layer's concern (UI, D-SEL-TEXT (open, Stage 1/2)) |
| identifiers | stable type names, property names, configuration keys, log category names, queue and arena names and counter names are ASCII (`[A-Za-z0-9_.]`, starting with a letter), validated by `Zenith_IsAsciiIdentifier`; their case matters |
| case operations | case folding exists for ASCII alone (`Zenith_ToAsciiLower`, `Zenith_EqualsAsciiCaseInsensitive`); no locale is ever consulted |
| platform boundary | Windows APIs take UTF-16 through `Zenith_Utf8ToUtf16` in `Internal/Windows/`; Android and Linux APIs take UTF-8 |
| NUL | text passed between engine libraries never contains U+0000; the JSON reader rejects `\u0000`, and `Zenith_String` rejects embedded NUL |
| byte order mark | a leading UTF-8 BOM (`EF BB BF`) in an authoring or configuration file is rejected by the JSON reader with `JSON_READ_ERROR_SYNTAX` at offset 0; the writer never emits one |

## Zenith_StringView

```cpp
// Zenith2/Engine/ZenithCore/Zenith_StringView.h
constexpr uint64 ulSTRING_NPOS = ~0ull;

/// @contract value Zenith_StringView
/// Ownership: Non-owning (pointer, byte length) view; the viewed bytes' owner keeps them alive. Not
///   NUL-terminated in general. Never allocates.
/// Thread affinity: IMMUTABLE. Members non-const: a view is default-constructed and
///   reassigned (strRest = strRest.Substr(…), view members of descs, records and containers).
/// Invalidation: Invalid when the viewed storage is freed, reset or modified.
/// Units: Lengths and offsets in bytes (not code points).
/// Errors: Substr and operator[] out of range are ZENITH_ASSERT; Find returns ulSTRING_NPOS when absent.
/// Version: since Z2 Stage 1. Source API.
/// @endcontract
class Zenith_StringView
{
public:
	constexpr Zenith_StringView() = default;
	constexpr Zenith_StringView(const char* pData, uint64 ulLength);   // ZENITH_ASSERT pData || ulLength == 0
	template<uint64 ulN> constexpr Zenith_StringView(const char (&acLiteral)[ulN]);   // length ulN − 1
	static Zenith_StringView FromCString(const char* szText);             // nullptr gives empty
	constexpr const char* GetData() const;
	constexpr uint64 GetLength() const;
	constexpr bool IsEmpty() const;
	constexpr char operator[](uint64 ulIndex) const;
	constexpr Zenith_StringView Substr(uint64 ulOffset, uint64 ulLength = ulSTRING_NPOS) const;   // length clamped to the end
	constexpr bool StartsWith(Zenith_StringView strPrefix) const;
	constexpr bool EndsWith(Zenith_StringView strSuffix) const;
	constexpr uint64 Find(char cValue, uint64 ulFrom = 0) const;
	constexpr uint64 Find(Zenith_StringView strNeedle, uint64 ulFrom = 0) const;
	constexpr uint64 FindLast(char cValue) const;
	constexpr Zenith_StringView TrimAsciiWhitespace() const;               // space, \t, \n, \r
	Zenith_ByteSpan AsBytes() const;
	friend constexpr bool operator==(Zenith_StringView, Zenith_StringView);          // byte equality
	friend constexpr std::strong_ordering operator<=>(Zenith_StringView, Zenith_StringView);   // unsigned byte-wise, then length
private:
	const char* m_pData = nullptr;
	uint64 m_ulLength = 0;
};
static_assert(sizeof(Zenith_StringView) == 16);

constexpr bool Zenith_IsAsciiIdentifier(Zenith_StringView strText, bool bAllowDots);
constexpr bool Zenith_EqualsAsciiCaseInsensitive(Zenith_StringView strA, Zenith_StringView strB);
constexpr char Zenith_ToAsciiLower(char cValue);
```

- `Substr(ulOffset, …)` with `ulOffset > GetLength()` is `ZENITH_ASSERT`; `ulOffset == GetLength()` gives an empty view.
- Ordering is unsigned byte-wise comparison of the common prefix, then the shorter first; it equals code-point order for valid UTF-8, and it is the ordering every deterministic sort of text uses.
- `Find(Zenith_StringView)` is a naive O(n × m) search: needles in the engine are short; nothing hashes or searches untrusted text with it inside a loop over untrusted input.

## Zenith_String

```cpp
// Zenith2/Engine/ZenithCore/Zenith_String.h
/// @contract container Zenith_String
/// Ownership: Owns a NUL-terminated UTF-8 byte buffer from the root heap or from the Zenith_Arena passed to the
///   arena constructor. Move-only; copies are explicit through TryAssign.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Every mutating call can reallocate and then invalidates every Zenith_StringView, pointer and
///   GetCString result obtained earlier; Clear invalidates them all.
/// Units: Lengths in bytes; the terminating NUL is not counted.
/// Errors: Try forms return Zenith_Result<void, Zenith_StringError>: STRING_ERROR_OUT_OF_MEMORY,
///   STRING_ERROR_INVALID_UTF8 (invalid input, which leaves the string unchanged), STRING_ERROR_EMBEDDED_NUL,
///   STRING_ERROR_TOO_LONG (beyond ulSTRING_MAX_BYTES). Non-Try forms route allocation failure through the OOM
///   handler and invalid input through ZENITH_CHECK.
/// Version: since Z2 Stage 1. Source API.
/// Alignment: byte buffer, alignment 1.
/// Overflow: at most ulSTRING_MAX_BYTES = 2^31 − 1 bytes; length arithmetic overflow-checked.
/// Allocation failure: Try forms leave the string unchanged.
/// Complexity: append amortised O(appended bytes + validation); AsView and GetCString O(1).
/// @endcontract
constexpr uint64 ulSTRING_MAX_BYTES = (1ull << 31) - 1;

class Zenith_String
{
public:
	constexpr Zenith_String() = default;
	explicit Zenith_String(Zenith_Arena& xArena);
	Zenith_String(Zenith_String&& xOther);
	Zenith_String& operator=(Zenith_String&& xOther);
	Zenith_String(const Zenith_String&) = delete;
	Zenith_String& operator=(const Zenith_String&) = delete;
	~Zenith_String();
	Zenith_Result<void, Zenith_StringError> TryAssign(Zenith_StringView strText);
	Zenith_Result<void, Zenith_StringError> TryAppend(Zenith_StringView strText);
	Zenith_Result<void, Zenith_StringError> TryAppendCodePoint(uint32 uCodePoint);
	template<typename... TArgs>
	Zenith_Result<void, Zenith_StringError> TryAppendFormat(Zenith_FormatString<TArgs...> xFormat, const TArgs&... xArgs);
	void Assign(Zenith_StringView strText);
	void Append(Zenith_StringView strText);
	Zenith_Result<void, Zenith_StringError> TryReserve(uint64 ulBytes);
	void Clear();
	Zenith_StringView AsView() const;
	const char* GetCString() const;     // "" when empty; never nullptr
	uint64 GetLength() const;
	bool IsEmpty() const;
	friend bool operator==(const Zenith_String& xA, Zenith_StringView strB);
private:
	Zenith_Array<char> m_acBytes;     // holds length + 1 bytes when non-empty, the last one NUL
};
```

- `TryAppend` validates the appended bytes (`Zenith_ValidateUtf8`) and rejects U+0000 before growing, so a failure changes nothing.
- `TryAppendFormat` formats into the string's spare capacity; when the result does not fit it grows to the exact length reported by the first formatting pass and formats again.

## Unicode functions

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Unicode.h
/// @contract enum Zenith_UnicodeError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: UNICODE_ERROR_INVALID_LEAD_BYTE (0x80–0xC1 or 0xF5–0xFF as a lead), UNICODE_ERROR_INVALID_CONTINUATION,
///   UNICODE_ERROR_TRUNCATED_SEQUENCE, UNICODE_ERROR_OVERLONG_ENCODING, UNICODE_ERROR_SURROGATE_CODE_POINT
///   (U+D800–U+DFFF encoded in UTF-8), UNICODE_ERROR_CODE_POINT_OUT_OF_RANGE (above U+10FFFF),
///   UNICODE_ERROR_UNPAIRED_SURROGATE (UTF-16 input), UNICODE_ERROR_DESTINATION_TOO_SMALL.
/// Version: since Z2 Stage 1.
/// @endcontract
enum Zenith_UnicodeError : uint8
{
	UNICODE_ERROR_INVALID_LEAD_BYTE = 1,
	UNICODE_ERROR_INVALID_CONTINUATION,
	UNICODE_ERROR_TRUNCATED_SEQUENCE,
	UNICODE_ERROR_OVERLONG_ENCODING,
	UNICODE_ERROR_SURROGATE_CODE_POINT,
	UNICODE_ERROR_CODE_POINT_OUT_OF_RANGE,
	UNICODE_ERROR_UNPAIRED_SURROGATE,
	UNICODE_ERROR_DESTINATION_TOO_SMALL,
};

struct Zenith_DecodedCodePoint
{
	uint32 m_uCodePoint = 0;
	uint32 m_uByteCount = 0;      // 1 … 4
};

/// @contract functions Zenith_Unicode
/// Ownership: Operate on caller buffers; allocate nothing.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Offsets and lengths in bytes (UTF-8) or 16-bit code units (UTF-16).
/// Errors: Zenith_Result<…, Zenith_UnicodeError>; malformed input never asserts.
/// Version: since Z2 Stage 1. Validation follows the Unicode Standard's well-formed UTF-8 table (Table 3-7).
/// @endcontract
Zenith_Result<void, Zenith_UnicodeError> Zenith_ValidateUtf8(Zenith_StringView strBytes);
uint64 Zenith_FindFirstInvalidUtf8(Zenith_StringView strBytes);          // GetLength() when valid
Zenith_Result<Zenith_DecodedCodePoint, Zenith_UnicodeError> Zenith_DecodeUtf8(Zenith_StringView strBytes, uint64 ulOffset);
uint32 Zenith_EncodeUtf8(uint32 uCodePoint, Zenith_Span<char> xOut4);  // bytes written; 0 for a surrogate or > U+10FFFF
Zenith_Result<uint64, Zenith_UnicodeError> Zenith_CountCodePoints(Zenith_StringView strBytes);
Zenith_Result<uint64, Zenith_UnicodeError> Zenith_GetUtf16Length(Zenith_StringView strBytes);
Zenith_Result<uint64, Zenith_UnicodeError> Zenith_Utf8ToUtf16(Zenith_StringView strBytes, Zenith_Span<char16_t> xOut);   // units written, no terminator
Zenith_Result<uint64, Zenith_UnicodeError> Zenith_Utf16ToUtf8(Zenith_Span<const char16_t> xUnits, Zenith_Span<char> xOut);  // bytes written
```

Validation algorithm (`Zenith_ValidateUtf8`): a scalar state machine over the bytes with an ASCII fast path that tests 8 bytes at a time for the high bit; lead byte ranges `C2–DF` (2 bytes), `E0` (next `A0–BF`), `E1–EC` and `EE–EF` (next `80–BF`), `ED` (next `80–9F`), `F0` (next `90–BF`), `F1–F3` (next `80–BF`), `F4` (next `80–8F`), every further continuation `80–BF`; the first byte that breaks the table determines the error code.

## String interning

```cpp
// Zenith2/Engine/ZenithCore/Zenith_StringID.h
/// @contract value Zenith_StringID
/// Ownership: Plain 4-byte value; the text lives in the process-lifetime Zenith_StringTable that issued it.
/// Thread affinity: IMMUTABLE. Members non-const: IDs are stored in containers and
///   components that reassign them.
/// Invalidation: None within the process; an ID is meaningless in another process and is never serialized.
/// Units: None.
/// Errors: None; resolution errors are the table's.
/// Version: since Z2 Stage 2. The numeric value is not stable across processes or runs: equality comparison
///   is its only meaning.
/// @endcontract
struct Zenith_StringID
{
	uint32 m_uValue = 0;         // 0 is the empty string, in every table
	constexpr bool IsEmpty() const { return m_uValue == 0; }
	friend constexpr bool operator==(Zenith_StringID, Zenith_StringID) = default;
};
static_assert(sizeof(Zenith_StringID) == 4);

/// @contract enum Zenith_StringError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: STRING_ERROR_OUT_OF_MEMORY, STRING_ERROR_INVALID_UTF8, STRING_ERROR_EMBEDDED_NUL,
///   STRING_ERROR_TOO_LONG (Zenith_String beyond ulSTRING_MAX_BYTES; interned text beyond
///   ulSTRING_INTERN_MAX_BYTES), STRING_ERROR_TABLE_FULL (the intern table's string or byte cap),
///   STRING_ERROR_NOT_FOUND (Find of text never interned), STRING_ERROR_INVALID_DESC (a Zenith_StringTableDesc
///   that fails Create's validation).
/// Version: since Z2 Stage 1.
/// @endcontract
enum Zenith_StringError : uint8
{
	STRING_ERROR_OUT_OF_MEMORY = 1,
	STRING_ERROR_INVALID_UTF8,
	STRING_ERROR_EMBEDDED_NUL,
	STRING_ERROR_TOO_LONG,
	STRING_ERROR_TABLE_FULL,
	STRING_ERROR_NOT_FOUND,
	STRING_ERROR_INVALID_DESC,
};

constexpr uint64 ulSTRING_INTERN_MAX_BYTES = 65535;

struct Zenith_StringTableDesc
{
	uint32 m_uMaxStrings = 0;           // the row "String intern table capacity" (pending, see CORE.md "Budget rows")
	uint64 m_ulMaxTextBytes = 0;        // the same row's byte value
	uint32 m_uEntriesPerPage = 4096;
	uint64 m_ulTextChunkBytes = 256ull * 1024;
};

struct Zenith_StringTableStats
{
	uint32 m_uStrings = 0;
	uint64 m_ulTextBytes = 0;
	uint64 m_ulReservedBytes = 0;
	uint64 m_ulInternCalls = 0;
	uint64 m_ulInternHits = 0;
};

/// @contract type Zenith_StringTable
/// Ownership: Created by Create as a Zenith_Owned<Zenith_StringTable>; the composition root owns one per process
///   and passes it by reference (never global). Storage from the root heap; nothing is freed before the table
///   is destroyed.
/// Thread affinity: ANY_THREAD. Intern and Find serialise on uLOCK_LEVEL_STRING_TABLE; GetText is lock-free.
/// Invalidation: None: a Zenith_StringView from GetText stays valid until the table is destroyed.
/// Units: Bytes; counts of strings.
/// Errors: Zenith_Result<…, Zenith_StringError>; Create returns STRING_ERROR_INVALID_DESC for a desc that fails
///   validation; GetText of an ID the table never issued is ZENITH_CHECK.
/// Version: since Z2 Stage 2. Source API.
/// Locks: uLOCK_LEVEL_STRING_TABLE (850, short-hold).
/// @endcontract
class Zenith_StringTable
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_StringTable>, Zenith_StringError> Create(const Zenith_StringTableDesc& xDesc);
	Zenith_Result<Zenith_StringID, Zenith_StringError> Intern(Zenith_StringView strText);
	Zenith_Result<Zenith_StringID, Zenith_StringError> Find(Zenith_StringView strText) const;
	Zenith_StringView GetText(Zenith_StringID xID) const;
	std::strong_ordering CompareText(Zenith_StringID xA, Zenith_StringID xB) const;
	Zenith_StringTableStats GetStats() const;
};
```

`Create` validates the desc before allocating anything and returns `STRING_ERROR_INVALID_DESC` unless: `m_uMaxStrings` is at least 1 and below 2^32 − 1; `m_ulMaxTextBytes` is at least 1; `m_uEntriesPerPage` is a power of two from 64 to 65,536; `m_ulTextChunkBytes` is at least `ulSTRING_INTERN_MAX_BYTES + 1` (one maximal string and its NUL fit in a chunk). The page-pointer array has `ceil((m_uMaxStrings + 1) / m_uEntriesPerPage)` entries, because slot 0 of page 0 is reserved (below). A zero cap is therefore a rejected desc, never a table that can hold nothing.

Algorithm:

1. The empty string is ID 0 without an entry; `Intern("")` and `Find("")` return it. IDs are 1-based: the first interned string is ID 1, stored at index 1, so slot 0 of page 0 is never written and index `i` is page `i / m_uEntriesPerPage`, slot `i % m_uEntriesPerPage`; `m_uCount` is the highest issued ID.
2. `Intern`: validate (UTF-8, no NUL, length ≤ `ulSTRING_INTERN_MAX_BYTES`) before the lock; take the lock; look the text up in a `Zenith_HashMap<Zenith_StringView, uint32>` whose keys view the table's own text storage; on a hit return it; otherwise check both caps (`STRING_ERROR_TABLE_FULL`), copy the text plus a NUL into the current text chunk (allocating a new chunk from the root heap when it does not fit; a chunk is never moved or freed), write the entry `{ const char* m_pText; uint32 m_uLength; }` into the page of the next index (allocating the page when needed), insert the map entry, publish the new count with `store(release)`, release the lock and return the index as the ID.
3. `GetText(xID)`: `ZENITH_CHECK(xID.m_uValue ≤ m_uCount.load(acquire))`, (ID 0 returns the empty view without a lookup), then read the entry from its page through the fixed page-pointer array sized at `Create` as above (so page pointers never move and the read needs no lock).
4. Every allocation happens with the lock held and comes from the root heap; the lock level is below `uLOCK_LEVEL_HEAP_OOM` so the OOM handler may run under it ([Z2-N-CONTRACT-215]).

- The determinism rule: IDs are assigned in interning order, and interning order depends on thread timing wherever strings are interned from parallel loads, so a `Zenith_StringID`'s numeric value never affects a simulation result. Simulation code compares IDs for equality only; it orders strings by `CompareText`, hashes them by their text (`Zenith_Hash64` of the bytes), and serializes them as text ([Z2-N-ARCH-884]). The world-state hash of an interned-string field hashes its text (`Zenith2/Docs/Scripting.md` → "Determinism rules and randomness").
- Text is never freed while the process runs; a hot-reload that interns new names grows the table, and the growth is visible in `core.strings.text_bytes` and bounded by the pending row.

## Formatting

<!-- elaboration -->
ZenithCore formats text without `<cstdio>` (outside the allowed headers) through a type-checked `{}` placeholder syntax. The format string is checked at compile time against the argument types.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Format.h
enum Zenith_FormatArgKind : uint8
{
	FORMAT_ARG_KIND_BOOL = 1, FORMAT_ARG_KIND_SIGNED, FORMAT_ARG_KIND_UNSIGNED, FORMAT_ARG_KIND_FLOAT,
	FORMAT_ARG_KIND_DOUBLE, FORMAT_ARG_KIND_CHAR, FORMAT_ARG_KIND_TEXT, FORMAT_ARG_KIND_POINTER,
};

/// @contract value Zenith_FormatString
/// Ownership: Wraps a string literal whose placeholders were checked at compile time.
/// Thread affinity: IMMUTABLE. m_strFormat is const: the consteval constructor sets it in its member-initializer
///   list, and a format string is passed by value, never assigned.
/// Invalidation: None.
/// Units: None.
/// Errors: A placeholder count different from the argument count, an unknown specifier, or a specifier that
///   does not fit the argument's kind is a compile error (the consteval constructor).
/// Version: since Z2 Stage 1.
/// @endcontract
template<typename... TArgs>
struct Zenith_FormatString
{
	template<uint64 ulN> consteval Zenith_FormatString(const char (&acFormat)[ulN]);   // validates
	const Zenith_StringView m_strFormat = Zenith_StringView();
};

/// @contract functions Zenith_Format
/// Ownership: Write into caller buffers; allocate nothing.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Byte counts.
/// Errors: None: truncation is reported by the return value, never as an error.
/// Version: since Z2 Stage 1. Output bytes are part of the contract for log parsing and golden tests.
/// @endcontract
template<typename... TArgs>
uint64 Zenith_FormatTo(Zenith_Span<char> xBuffer, Zenith_FormatString<std::type_identity_t<TArgs>...> xFormat, const TArgs&... xArgs);
```

| Syntax | Meaning | Argument kinds |
|---|---|---|
| `{}` | default form | every kind |
| `{:x}` | lowercase hexadecimal, no prefix | signed (two's complement of the value's width), unsigned, pointer |
| `{:.N}` with N in 0 … 9 | fixed notation with N fractional digits, round half to even | float, double |
| `{:e}` | scientific notation, shortest round-trip digits | float, double |
| `{{`, `}}` | a literal brace | — |

| Kind | Default form |
|---|---|
| bool | `true` / `false` |
| signed, unsigned | decimal, `-` for negatives |
| float, double | the shortest text that round-trips the value (`std::to_chars` with no precision, inside `Internal/Text/`), `nan`, `inf`, `-inf` for non-finite values |
| char | the byte |
| text | `Zenith_StringView`, `const char*` (nullptr prints `(null)`), `Zenith_String` |
| pointer | `0x` followed by 16 lowercase hex digits |
| enum | when a `Zenith_ToString(E)` overload is visible, its text; otherwise the underlying value in decimal |

- `Zenith_FormatTo` writes at most `xBuffer.GetCount() − 1` bytes followed by a NUL (nothing when the buffer is empty) and returns the full length the result needs, excluding the NUL; a return value ≥ the buffer size means the output was truncated at a byte boundary that never splits a UTF-8 sequence of a text argument.
- A text argument containing invalid UTF-8 is formatted with each invalid byte replaced by U+FFFD, so formatted output is always valid UTF-8.

## Number conversion

```cpp
// Zenith2/Engine/ZenithCore/Zenith_NumberText.h
/// @contract enum Zenith_ParseError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: PARSE_ERROR_EMPTY, PARSE_ERROR_INVALID_CHARACTER, PARSE_ERROR_OUT_OF_RANGE, PARSE_ERROR_TRAILING_CHARACTERS.
/// Version: since Z2 Stage 1.
/// @endcontract
enum Zenith_ParseError : uint8
{
	PARSE_ERROR_EMPTY = 1,
	PARSE_ERROR_INVALID_CHARACTER,
	PARSE_ERROR_OUT_OF_RANGE,
	PARSE_ERROR_TRAILING_CHARACTERS,
};

Zenith_Result<sint64, Zenith_ParseError> Zenith_ParseInt64(Zenith_StringView strText);
Zenith_Result<uint64, Zenith_ParseError> Zenith_ParseUInt64(Zenith_StringView strText);
Zenith_Result<double, Zenith_ParseError> Zenith_ParseDouble(Zenith_StringView strText);
Zenith_Result<bool, Zenith_ParseError> Zenith_ParseBool(Zenith_StringView strText);   // exactly "true" or "false"
uint32 Zenith_FormatDouble(double dValue, Zenith_Span<char> xOut32);               // shortest round-trip; 0 if non-finite
uint32 Zenith_FormatFloat(float fValue, Zenith_Span<char> xOut32);
```

- Accepted integer grammar: an optional `-` (signed only), then one or more ASCII digits, no leading `+`, no whitespace, no base prefix; leading zeros are accepted. The implementation is Zenith code (multiply-and-add with overflow checks).
- Accepted floating grammar: the JSON number grammar (RFC 8259 §6), plus nothing else, in at most 64 bytes on both backend paths (a longer text fails with `PARSE_ERROR_OUT_OF_RANGE` before the backend is called, so the two paths agree); `Zenith_ParseDouble` delegates to the D-SEL-JSON (open, Stage 1) backend's number parser, which is correctly rounded in both candidates (yyjson's reader; simdjson's parser) — with yyjson, the text is read with `yyjson_read_number`; with simdjson, it is parsed as a one-element document `[<text>]` in a stack buffer of at most 64 bytes, longer numbers failing with `PARSE_ERROR_OUT_OF_RANGE`. A value whose magnitude exceeds `double`'s range fails with `PARSE_ERROR_OUT_OF_RANGE`.
- Formatting uses `std::to_chars` in `Zenith2/Engine/ZenithCore/Internal/Text/Zenith_NumberText.cpp` alone, under the JR-CORE-CHARCONV record (`Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"); `<charconv>` never reaches a public header.

<!-- informative -->
`std::from_chars` for floating-point types is not provided by the libc++ of NDK r27c, which is why parsing goes through the JSON backend, which both candidates provide with correct rounding, while formatting (available in every pinned standard library) uses the standard facility.
<!-- /informative -->

## JSON writer

`Zenith_JsonWriter` is the byte-stable streaming writer every library uses (`WriteJson(Zenith_JsonWriter&)` in `Zenith2/Docs/Architecture/ZenithGraph.md` → "JSON dump", `WriteDump` in `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"). It is Zenith code on both D-SEL-JSON paths, so its output never depends on the ruling.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_JsonWriter.h
enum Zenith_JsonWriteStyle : uint8 { JSON_WRITE_STYLE_PRETTY = 1, JSON_WRITE_STYLE_COMPACT };
enum Zenith_JsonArrayLayout : uint8 { JSON_ARRAY_LAYOUT_MULTILINE = 1, JSON_ARRAY_LAYOUT_INLINE };

/// @contract enum Zenith_JsonWriteError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: JSON_WRITE_ERROR_OUT_OF_MEMORY (the output array could not grow), JSON_WRITE_ERROR_NON_FINITE_NUMBER,
///   JSON_WRITE_ERROR_INVALID_UTF8 (a key or string argument), JSON_WRITE_ERROR_KEY_OUTSIDE_OBJECT,
///   JSON_WRITE_ERROR_VALUE_WITHOUT_KEY, JSON_WRITE_ERROR_UNBALANCED_END, JSON_WRITE_ERROR_DEPTH_EXCEEDED
///   (beyond 64), JSON_WRITE_ERROR_MULTIPLE_ROOTS, JSON_WRITE_ERROR_INCOMPLETE (Finish with open containers),
///   JSON_WRITE_ERROR_INVALID_RAW_NUMBER.
/// Version: since Z2 Stage 1.
/// @endcontract
enum Zenith_JsonWriteError : uint8
{
	JSON_WRITE_ERROR_OUT_OF_MEMORY = 1,
	JSON_WRITE_ERROR_NON_FINITE_NUMBER,
	JSON_WRITE_ERROR_INVALID_UTF8,
	JSON_WRITE_ERROR_KEY_OUTSIDE_OBJECT,
	JSON_WRITE_ERROR_VALUE_WITHOUT_KEY,
	JSON_WRITE_ERROR_UNBALANCED_END,
	JSON_WRITE_ERROR_DEPTH_EXCEEDED,
	JSON_WRITE_ERROR_MULTIPLE_ROOTS,
	JSON_WRITE_ERROR_INCOMPLETE,
	JSON_WRITE_ERROR_INVALID_RAW_NUMBER,
};

/// @contract type Zenith_JsonWriter
/// Ownership: A stack object bound to a caller-owned Zenith_Array<char> that receives the output.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: The output array's views are invalidated by every writing call (it may grow).
/// Units: None.
/// Errors: The first failure is sticky: every subsequent call is a no-op and GetStatus() and Finish() return it.
///   The writing members return void by design and are fallible through that status, which is .
/// Version: since Z2 Stage 1. The output format of "Byte format" is part of the contract; a change to it is
///   a canonical-serialization change that migrates every authoring document (Zenith2/Docs/Contracts.md
///   "Compatibility policy per kind").
/// @endcontract
class Zenith_JsonWriter
{
public:
	Zenith_JsonWriter(Zenith_Array<char>& acOutput, Zenith_JsonWriteStyle eStyle);
	void BeginObject();
	void EndObject();
	void BeginArray(Zenith_JsonArrayLayout eLayout = JSON_ARRAY_LAYOUT_MULTILINE);
	void EndArray();
	void Key(Zenith_StringView strKey);
	void String(Zenith_StringView strValue);
	void Bool(bool bValue);
	void Null();
	void Int(sint64 iValue);
	void UInt(uint64 ulValue);
	void Double(double dValue);
	void Float(float fValue);
	void RawNumber(Zenith_StringView strNumberText);   // text validated against the JSON number grammar
	Zenith_Result<void, Zenith_JsonWriteError> GetStatus() const;
	Zenith_Result<void, Zenith_JsonWriteError> Finish();   // ends the document
	uint32 GetDepth() const;
};
```

### Byte format

| Element | `JSON_WRITE_STYLE_PRETTY` | `JSON_WRITE_STYLE_COMPACT` |
|---|---|---|
| encoding | UTF-8, no BOM, LF line endings | UTF-8, no BOM |
| object | `{`, LF, each member on its own line indented by one tab per depth as `"key": value`, `,` after every member but the last, LF, `}` at the parent's indentation; an empty object is `{}` | `{"key":value,…}` |
| array, multiline | `[`, LF, each element on its own line indented one tab deeper, `,` separators, LF, `]`; an empty array is `[]` | `[v,v]` |
| array, inline | `[v, v, v]` on one line (containers inside an inline array are written compact) | `[v,v]` |
| string | `"`; `"` → `\"`, `\` → `\\`, U+0008 → `\b`, U+000C → `\f`, U+000A → `\n`, U+000D → `\r`, U+0009 → `\t`, other code points below U+0020 and U+007F → `\u00XX` with lowercase hex; every other code point verbatim | same |
| integer | decimal, `-` for negatives | same |
| double, float | the shortest round-trip text of `Zenith_FormatDouble` / `Zenith_FormatFloat`; negative zero is `-0`; non-finite fails | same |
| raw number | the given text verbatim, after grammar validation | same |
| document end | `Finish` appends one LF after the root | nothing |

- Key order is the caller's; the canonical order of an authoring document is imposed by its serializer (SCENE), never by the writer.
- Duplicate keys are the caller's defect; the writer does not track keys.

## JSON reader

The reader parses untrusted bytes into a mutable document tree that preserves member order, raw number text and source positions, which unknown-field preservation and configuration provenance need. The parsing backend is D-SEL-JSON (open, Stage 1); the tree, its API and every limit are backend-neutral.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_JsonDocument.h
enum Zenith_JsonValueKind : uint8
{
	JSON_VALUE_KIND_NULL = 1, JSON_VALUE_KIND_BOOL, JSON_VALUE_KIND_NUMBER,
	JSON_VALUE_KIND_STRING, JSON_VALUE_KIND_ARRAY, JSON_VALUE_KIND_OBJECT,
};

/// @contract value Zenith_JsonLimits
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and override fields, as
///   ForSchemas does.
/// Invalidation: None.
/// Units: Bytes and counts.
/// Errors: None.
/// Version: since Z2 Stage 2. The defaults are the JSON row of Zenith2/Docs/Architecture.md
///   "Security architecture" (table "Parser limits") and change only with that table.
/// @endcontract
struct Zenith_JsonLimits
{
	uint64 m_ulMaxInputBytes = 64ull * 1024 * 1024;
	uint32 m_uMaxDepth = 64;
	uint64 m_ulMaxStringBytes = 1024ull * 1024;
	uint32 m_uMaxArrayElements = 1048576;
	uint32 m_uMaxObjectMembers = 65536;
	static constexpr Zenith_JsonLimits ForSchemas();   // m_ulMaxInputBytes = 1 MiB, the rest as above
};

enum Zenith_JsonReadErrorCode : uint8
{
	JSON_READ_ERROR_SYNTAX = 1,
	JSON_READ_ERROR_INPUT_TOO_LARGE,
	JSON_READ_ERROR_DEPTH_EXCEEDED,
	JSON_READ_ERROR_STRING_TOO_LONG,
	JSON_READ_ERROR_ARRAY_TOO_LONG,
	JSON_READ_ERROR_OBJECT_TOO_LARGE,
	JSON_READ_ERROR_NUMBER_OUT_OF_RANGE,
	JSON_READ_ERROR_INVALID_UTF8,
	JSON_READ_ERROR_NUL_CHARACTER,
	JSON_READ_ERROR_DUPLICATE_KEY,
	JSON_READ_ERROR_EMPTY_INPUT,
	JSON_READ_ERROR_OUT_OF_MEMORY,
};

/// @contract value Zenith_JsonReadError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_Result value-initialises and move-assigns its error,
///   and the reader fills the position after construction.
/// Invalidation: None.
/// Units: Line and column are 1-based (column in bytes); offset is a 0-based byte offset.
/// Errors: m_eCode names the failure; the position is that of the first offending byte (for
///   JSON_READ_ERROR_DUPLICATE_KEY, the second occurrence).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_JsonReadError
{
	Zenith_JsonReadErrorCode m_eCode{};   // 0 (no failure) until the reader sets a code; a success Zenith_Result holds this
	uint32 m_uLine = 0;
	uint32 m_uColumn = 0;
	uint64 m_ulOffset = 0;
};

enum Zenith_JsonAccessError : uint8
{
	JSON_ACCESS_ERROR_WRONG_KIND = 1,
	JSON_ACCESS_ERROR_NOT_AN_INTEGER,
	JSON_ACCESS_ERROR_OUT_OF_RANGE,
	JSON_ACCESS_ERROR_MISSING_MEMBER,
	JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE,
	JSON_ACCESS_ERROR_OUT_OF_MEMORY,
	JSON_ACCESS_ERROR_INVALID_POINTER,     // Zenith_DocumentTree::FindPath only (§ "JSON document tree")
};

struct Zenith_JsonNode
{
	uint32 m_uIndex = 0;       // 0 is the null node reference
	constexpr bool IsNull() const { return m_uIndex == 0; }
};

/// @contract type Zenith_JsonDocument
/// Ownership: Created by Parse or CreateEmpty as a Zenith_Owned<Zenith_JsonDocument>; owns every node, key and
///   string (root heap for nodes, an internal Zenith_Arena for text).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Node references stay valid until the node is removed or the document destroyed; views
///   returned by GetString, GetMemberKey and GetNumberText stay valid until that node is changed or removed.
/// Units: Positions as in Zenith_JsonReadError.
/// Errors: Parse returns Zenith_Result<…, Zenith_JsonReadError>, never asserting on any input; accessors
///   return Zenith_Result<…, Zenith_JsonAccessError> because the document's shape is untrusted; a null or
///   foreign node reference is ZENITH_ASSERT (a programmer error).
/// Version: since Z2 Stage 2. Source API. The tree is identical for both D-SEL-JSON backends.
/// @endcontract
class Zenith_JsonDocument
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_JsonDocument>, Zenith_JsonReadError> Parse(
		Zenith_ByteSpan xInput, const Zenith_JsonLimits& xLimits);
	static Zenith_Result<Zenith_Owned<Zenith_JsonDocument>, Zenith_JsonAccessError> CreateEmpty();

	Zenith_JsonNode GetRoot() const;
	Zenith_JsonValueKind GetKind(Zenith_JsonNode xNode) const;
	uint32 GetLine(Zenith_JsonNode xNode) const;       // 0 for nodes created after parsing
	uint32 GetColumn(Zenith_JsonNode xNode) const;
	Zenith_Result<bool, Zenith_JsonAccessError> GetBool(Zenith_JsonNode xNode) const;
	Zenith_Result<sint64, Zenith_JsonAccessError> GetInt64(Zenith_JsonNode xNode) const;
	Zenith_Result<uint64, Zenith_JsonAccessError> GetUInt64(Zenith_JsonNode xNode) const;
	Zenith_Result<double, Zenith_JsonAccessError> GetDouble(Zenith_JsonNode xNode) const;
	Zenith_Result<Zenith_StringView, Zenith_JsonAccessError> GetNumberText(Zenith_JsonNode xNode) const;
	Zenith_Result<Zenith_StringView, Zenith_JsonAccessError> GetString(Zenith_JsonNode xNode) const;
	Zenith_Result<uint32, Zenith_JsonAccessError> GetCount(Zenith_JsonNode xArrayOrObject) const;
	Zenith_Result<Zenith_JsonNode, Zenith_JsonAccessError> GetElement(Zenith_JsonNode xArray, uint32 uIndex) const;
	Zenith_Result<Zenith_StringView, Zenith_JsonAccessError> GetMemberKey(Zenith_JsonNode xObject, uint32 uIndex) const;
	Zenith_Result<Zenith_JsonNode, Zenith_JsonAccessError> GetMemberValue(Zenith_JsonNode xObject, uint32 uIndex) const;
	Zenith_Result<Zenith_JsonNode, Zenith_JsonAccessError> FindMember(Zenith_JsonNode xObject, Zenith_StringView strKey) const;

	Zenith_Result<Zenith_JsonNode, Zenith_JsonAccessError> CreateValue(Zenith_JsonValueKind eKind);
	Zenith_Result<void, Zenith_JsonAccessError> SetBool(Zenith_JsonNode xNode, bool bValue);
	Zenith_Result<void, Zenith_JsonAccessError> SetInt64(Zenith_JsonNode xNode, sint64 iValue);
	Zenith_Result<void, Zenith_JsonAccessError> SetDouble(Zenith_JsonNode xNode, double dValue);
	Zenith_Result<void, Zenith_JsonAccessError> SetString(Zenith_JsonNode xNode, Zenith_StringView strValue);
	Zenith_Result<void, Zenith_JsonAccessError> InsertMember(Zenith_JsonNode xObject, uint32 uPosition, Zenith_StringView strKey, Zenith_JsonNode xValue);
	Zenith_Result<void, Zenith_JsonAccessError> RemoveMember(Zenith_JsonNode xObject, uint32 uIndex);
	Zenith_Result<void, Zenith_JsonAccessError> InsertElement(Zenith_JsonNode xArray, uint32 uPosition, Zenith_JsonNode xValue);
	Zenith_Result<void, Zenith_JsonAccessError> RemoveElement(Zenith_JsonNode xArray, uint32 uIndex);
	void Write(Zenith_JsonWriter& xWriter, Zenith_JsonNode xNode) const;   // numbers written from their raw text
};
```

Parse algorithm:

1. Reject an empty input (`JSON_READ_ERROR_EMPTY_INPUT`), an input above `m_ulMaxInputBytes` (`JSON_READ_ERROR_INPUT_TOO_LARGE`) and a leading BOM (`JSON_READ_ERROR_SYNTAX`, offset 0), before calling the backend.
2. Run the backend over the input (table below). A backend error is mapped to `JSON_READ_ERROR_SYNTAX` or `JSON_READ_ERROR_INVALID_UTF8` with the backend's error byte offset.
3. Convert the backend's tree into Zenith nodes in document order, enforcing depth, string length, array length and object size limits during the walk (each violation stops the walk with its code and the offending value's position); strings are copied into the document's text arena; numbers are stored as their raw text plus two flags (`is integer text`, `fits int64`/`fits uint64`) computed with `Zenith_ParseInt64`/`Zenith_ParseUInt64`, and a double value parsed through the backend; a number whose magnitude exceeds `double` fails with `JSON_READ_ERROR_NUMBER_OUT_OF_RANGE`; any string containing U+0000 fails with `JSON_READ_ERROR_NUL_CHARACTER`.
4. Duplicate keys: for each object, sort a scratch list of `(key, member index)` with `Zenith_Sort` (byte order) and compare neighbours; a duplicate fails with `JSON_READ_ERROR_DUPLICATE_KEY` at the second of the two members. Hashing is not used because the keys are untrusted.
5. Positions: a Zenith position pass walks the already-validated input once, recording the start offset of every value in document order (it skips whitespace, strings with escapes, numbers and literals, and structural characters; it cannot fail, because the backend has validated the input), assigns offsets to nodes in the same order, and converts each to line and column through a table of line-start offsets built in the same pass (binary search).
6. The backend's own tree is freed before `Parse` returns.

| Step | yyjson path | simdjson path |
|---|---|---|
| read | `yyjson_read_opts` with `YYJSON_READ_NUMBER_AS_RAW` (raw number text) and no `YYJSON_READ_ALLOW_*` flags, with an allocator routing to the root heap (`Zenith2/Docs/Contracts/Memory.md` → "Bare allocation boundary", [Z2-N-CONTRACT-240]) | `simdjson::ondemand::parser` over a padded copy of the input (`SIMDJSON_PADDING` bytes, from the root heap), iterating values in document order and reading numbers with `raw_json_token()` |
| error offset | `yyjson_read_err::pos` | `document::current_location()` at the failure, as an offset from the input start |
| number value | parsed by `yyjson_read_number` on the raw text | `get_double()` on the value |
| UTF-8 | validated by yyjson (no invalid-Unicode flag) | validated by simdjson (`simdjson::validate_utf8` over the input before iteration) |

<!-- informative -->
The two candidates differ in speed (simdjson's SIMD stage 1 against yyjson's scalar reader) and in API shape (yyjson builds a DOM, simdjson's on-demand API streams). Converting either into one Zenith tree makes the ruling an `Internal/` change: every consumer, every limit and every test sees the same tree, and the D-SEL-JSON harness measures each backend through the same public `Parse`.
<!-- /informative -->

- Files: `Internal/Json/Zenith_JsonDocument.cpp` (tree, limits, positions, duplicates), `Internal/Json/Zenith_JsonBackend.h` (the private backend interface `Zenith_JsonBackendParse`), and exactly one backend source, `Internal/Json/Zenith_JsonBackend_Yyjson.cpp` or `Internal/Json/Zenith_JsonBackend_Simdjson.cpp`; before the ruling, the harness ticket builds both under `Zenith2/Benchmarks/JsonSelection/` against copies of the private interface, and ZenithCore carries neither; the ruling ticket adds the winner's manifest entry (owner ZenithCore) and its one source.

## JSON document tree

`Zenith_DocumentTree` is the cursor every migration step receives: `Zenith_PfnMigrationStep` takes a `Zenith_DocumentTree&` (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", subsection "Migration step interface"). It names one subtree of a `Zenith_JsonDocument` — a whole authoring document for SCENE's document steps, one component field-set object for ECS's component steps (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_Migration header"), one `settings` or `platforms.<p>` object for ASSET's importer-settings steps (`Zenith2/Docs/Design/ASSET/Migration.md`) — and resolves JSON Pointers relative to that subtree, so one step serves every document that embeds the object it migrates. It owns nothing, allocates nothing and adds no mutation API: a step edits the subtree in place through the mutators of `GetDocument()` (§ "JSON reader"), and a narrower cursor is built as `Zenith_DocumentTree(xTree.GetDocument(), xNode)` over a node `FindPath` returned. Cross-area notes C13 and S5.

### Zenith_DocumentTree declarations

```cpp
// Zenith2/Engine/ZenithCore/Zenith_DocumentTree.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_JsonDocument.h"

inline constexpr uint32 uJSON_POINTER_MAX_BYTES = 4096;

/// @contract type Zenith_DocumentTree
/// Ownership: A non-owning cursor over one Zenith_JsonDocument subtree; the document is owned by the caller and
///   outlives the cursor. Copying a cursor copies the (document, root) pair; no copy owns anything.
/// Thread affinity: OWNER_THREAD (the document's owner thread).
/// Invalidation: Invalid when the document is destroyed or the root node removed; a node FindPath returned is
///   invalidated exactly as Zenith_JsonDocument's node references are.
/// Units: Pointers are UTF-8 bytes, at most uJSON_POINTER_MAX_BYTES, in the grammar of "Pointer resolution steps".
/// Errors: FindPath returns JSON_ACCESS_ERROR_INVALID_POINTER for a pointer outside the grammar (checked before
///   any node is visited), JSON_ACCESS_ERROR_MISSING_MEMBER for an object token naming no member,
///   JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE for an array token that is not an element index below the length, and
///   JSON_ACCESS_ERROR_WRONG_KIND for a token applied to a null, bool, number or string; it never asserts on the
///   pointer's bytes. Constructing a cursor over a null root is ZENITH_ASSERT (a programmer error).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_DocumentTree
{
public:
	Zenith_DocumentTree(Zenith_JsonDocument& xDocument, Zenith_JsonNode xRoot);
	Zenith_JsonDocument& GetDocument();
	Zenith_JsonNode GetRoot() const;
	Zenith_Result<Zenith_JsonNode, Zenith_JsonAccessError> FindPath(Zenith_StringView strPointer) const;
private:
	Zenith_JsonDocument* m_pxDocument;
	Zenith_JsonNode m_xRoot;
};
static_assert(sizeof(Zenith_DocumentTree) == 16);
```

- `Zenith_JsonAccessError` gains its seventh enumerator, `JSON_ACCESS_ERROR_INVALID_POINTER`, after `JSON_ACCESS_ERROR_OUT_OF_MEMORY` (§ "JSON reader"; category `INVALID_ARGUMENT` in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy"). The existing enumerators keep their values.
- The constructor stores both arguments after `ZENITH_ASSERT(!xRoot.IsNull())`; `GetDocument` and `GetRoot` return them. The cursor is trivially copyable and has no default constructor.
- Files: the header above and `Internal/Json/Zenith_DocumentTree.cpp`.

### Pointer resolution steps

A pointer is RFC 6901 JSON Pointer text with the restrictions below. They are the same rules as D1 to D4 of `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Document pointers": SCENE's `Zenith_ValidateDocumentPointer` checks the tree-free subset (G1, G2, G4) with the same 4,096-byte limit (`uDOCUMENT_MAX_POINTER_BYTES` equals `uJSON_POINTER_MAX_BYTES`), and `FindPath` is the one tree-walking implementation of the whole grammar.

| Rule | Statement |
|---|---|
| G1 | the empty pointer names the cursor's root; every other pointer starts with `/` and is a sequence of reference tokens, each preceded by `/`; a token may be empty |
| G2 | inside a token, `~0` stands for `~` and `~1` for `/`; a `~` followed by any other byte, or ending the pointer, is invalid |
| G3 | a token applied to an array is an element index: `0`, or a digit `1`–`9` followed by digits, whose value is below the array's length; a leading zero, `-`, a sign, an empty token, any non-digit byte and a value above 0xFFFFFFFF are out of range |
| G4 | the pointer is valid UTF-8 of at most `uJSON_POINTER_MAX_BYTES` bytes; a token compares with member keys as bytes after unescaping |

`FindPath(strPointer)`:

1. If `strPointer.GetLength() > uJSON_POINTER_MAX_BYTES` or `Zenith_ValidateUtf8(strPointer)` fails, return `JSON_ACCESS_ERROR_INVALID_POINTER` (G4).
2. If `strPointer` is empty, return `m_xRoot` (G1). If its first byte is not `/`, return `JSON_ACCESS_ERROR_INVALID_POINTER`. Scan every byte once: each `~` must be followed by `0` or `1`, otherwise return `JSON_ACCESS_ERROR_INVALID_POINTER` (G2). No node is visited before this scan completes, so a malformed pointer yields `JSON_ACCESS_ERROR_INVALID_POINTER` whatever the tree holds.
3. Set `xNode = m_xRoot`. For each token in order (the bytes after one `/` up to the next `/` or the end), by `m_pxDocument->GetKind(xNode)`:
   - `JSON_VALUE_KIND_OBJECT`: when the token holds no `~`, call `FindMember(xNode, token)` on the token's own bytes; otherwise unescape it (each `~0` to `~` and each `~1` to `/`, left to right in one pass) into a stack buffer `char acToken[uJSON_POINTER_MAX_BYTES]` — unescaping only shortens a token — and call `FindMember` with that view. `FindMember` returns the first member whose key equals the token byte for byte (the reader rejects duplicate keys and `InsertMember` asserts on one); its `JSON_ACCESS_ERROR_MISSING_MEMBER` is returned unchanged.
   - `JSON_VALUE_KIND_ARRAY`: read the token as G3 states, digit by digit into a `uint64` that stops at the first value above 0xFFFFFFFF, without `Zenith_ParseUInt64` (whose grammar admits leading zeros); a token outside G3, or an index at or beyond `GetCount(xNode)`, returns `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE`; otherwise `xNode = GetElement(xNode, uIndex)`.
   - any other kind: return `JSON_ACCESS_ERROR_WRONG_KIND`.
4. Return `xNode`.

`FindPath` is O(pointer bytes + the members scanned in each object visited), iterative (no recursion), uses one 4 KiB stack buffer, takes no lock, allocates nothing and never changes the document. The pointer examples of `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Document pointers" are its conformance cases: `/entities/<id>/components/Zenith.LocalTransform/fields/1` reaches the member whose key is `Zenith.LocalTransform`, never a member `Zenith` holding `LocalTransform`.

## Text tests

| Test ID | File | Asserts |
|---|---|---|
| `Test_StringView_Compare_ByteOrder` | `Zenith_StringView_Tests.cpp` | ordering equals `std::string_view` comparison of the same bytes for 10,000 generated pairs including bytes ≥ 0x80 |
| `Test_StringView_Substr_OutOfRange_Asserts` | `Zenith_StringView_DefectTests.cpp` | `z2-death-assert` |
| `Test_StringView_AsciiHelpers` | `Zenith_StringView_Tests.cpp` | `TrimAsciiWhitespace` strips space, tab, CR and LF from both ends and nothing else (U+00A0 kept); `Zenith_EqualsAsciiCaseInsensitive` folds A–Z only (`"Ã"` never equals `"Ã¤"`); `Zenith_ToAsciiLower` maps A–Z and leaves every other byte |
| `Test_StringView_Find_Cases` | `Zenith_StringView_Tests.cpp` | needle at start, end, absent, empty needle, overlapping prefix |
| `Test_String_MatchesOracle` | `Zenith_String_Tests.cpp` | [Z2-N-CONTRACT-288] against `std::string` over append, assign, clear |
| `Test_String_Properties` | `Zenith_String_Tests.cpp` | NUL terminator and UTF-8 validity after every operation |
| `Test_String_TryFailure_LeavesUnchanged` | `Zenith_String_Tests.cpp` | allocation failure and invalid UTF-8 input each leave the bytes identical |
| `Test_String_SizeOverflow_IsRejected` | `Zenith_String_Tests.cpp` | an append that would exceed `ulSTRING_MAX_BYTES` returns `STRING_ERROR_TOO_LONG` |
| `Test_String_EmbeddedNul_Rejected` | `Zenith_String_Tests.cpp` | `STRING_ERROR_EMBEDDED_NUL` |
| `Test_Utf8_Validate_Corpus` | `Zenith_Unicode_Tests.cpp` | every case of `Tests/Fixtures/Text/utf8_cases.json` (each boundary of the well-formed table, every overlong form, surrogates, `F4 90`, truncations at every length) yields its recorded result and offset |
| `Test_Utf8_RoundTripAllScalars` | `Zenith_Unicode_Tests.cpp` | every scalar value U+0000 … U+10FFFF except surrogates encodes and decodes to itself |
| `Test_Utf16_RoundTrip_AndUnpaired` | `Zenith_Unicode_Tests.cpp` | UTF-8 → UTF-16 → UTF-8 is the identity; an unpaired surrogate gives `UNICODE_ERROR_UNPAIRED_SURROGATE` |
| `Test_Unicode_AsciiIdentifier_Rules` | `Zenith_Unicode_Tests.cpp` | `Zenith_IsAsciiIdentifier` accepts `core.memory` (dots allowed) and `jobsWorker`, and rejects a leading digit, a dot when dots are not allowed, a space and any byte ≥ 0x80 |
| `Test_Utf8_Fuzz` (`Zenith_Fuzz_Utf8`) | `Tests/Fuzz/Utf8.cpp` | libFuzzer: `Zenith_ValidateUtf8` agrees with a reference decoder and never reads out of bounds |
| `Test_StringTable_InternTwice_SameID` | `Zenith_StringTable_Tests.cpp` | same text gives the same ID; different text different IDs |
| `Test_StringTable_EmptyIsZero` | `Zenith_StringTable_Tests.cpp` | `Intern("")` returns ID 0 and adds no entry; the first non-empty `Intern` returns ID 1 and the second distinct text ID 2 |
| `Test_StringTable_Create_RejectsInvalidDesc` | `Zenith_StringTable_Tests.cpp` | `m_uMaxStrings` 0, `m_ulMaxTextBytes` 0, `m_uEntriesPerPage` 100 (not a power of two) and 32 (below 64), and `m_ulTextChunkBytes` 65,535 each return `STRING_ERROR_INVALID_DESC` with no allocation (the heap's live allocation count unchanged); `m_uMaxStrings` equal to `m_uEntriesPerPage` succeeds and interns exactly that many strings before `STRING_ERROR_TABLE_FULL` |
| `Test_StringTable_ConcurrentIntern_Consistent` | `Zenith_StringTable_Tests.cpp` | `z2-stress`, 8 threads interning overlapping sets of 100,000 strings: every text maps to one ID, `GetText` round-trips; also runs under `linux-tsan` |
| `Test_StringTable_CapReached_ReturnsTableFull` | `Zenith_StringTable_Tests.cpp` | the string cap and the byte cap each give `STRING_ERROR_TABLE_FULL` and leave the table usable |
| `Test_StringTable_TextViewsStableAcrossGrowth` | `Zenith_StringTable_Tests.cpp` | a view taken before 100,000 further interns still reads the same bytes |
| `Test_StringTable_GetTextUnknown_Checks` | `Zenith_StringTable_DefectTests.cpp` | `z2-death-check` |
| `Test_StringTable_IDsDifferAcrossInternOrder_TextHashesEqual` | `Zenith_StringTable_Tests.cpp` | two tables interning the same 1,000 strings in different orders issue different IDs while `Zenith_Hash64` of each ID's text and every `CompareText` ordering are identical |
| `Test_Format_Placeholders_AllKinds` | `Zenith_Format_Tests.cpp` | each row of the default-form table against its expected bytes |
| `Test_Format_Truncation_ReportsFullLength` | `Zenith_Format_Tests.cpp` | a 10-byte buffer holds 9 bytes and a NUL; the return equals the untruncated length; no UTF-8 sequence is split |
| `Test_Format_DoubleShortest_RoundTrips` | `Zenith_Format_Tests.cpp` | the fixture `Tests/Fixtures/Text/doubles.json` (subnormals, powers of ten, `0.1`, `1e21`, `-0`, the largest finite) formats to its recorded text and parses back to the same bits, identically on every preset |
| `Test_Format_MismatchedPlaceholders_DoNotCompile` | `Zenith_Format_CompileFail.cpp` | a CTest `WILL_FAIL` compile test: a format string with two placeholders and one argument fails to compile |
| `Test_Parse_Integers_Grammar` | `Zenith_NumberText_Tests.cpp` | `-0`, `18446744073709551615`, `18446744073709551616` (out of range), `+1`, ` 1`, `1x`, empty |
| `Test_Parse_Double_LongText_OutOfRange` | `Zenith_NumberText_Tests.cpp` | a 64-byte number text parses and a 65-byte one returns `PARSE_ERROR_OUT_OF_RANGE`, on the ruled backend and, in the D-SEL-JSON harness, on both candidates |
| `Test_Parse_Double_Grammar` | `Zenith_NumberText_Tests.cpp` | the JSON number grammar cases, `1e400` out of range, `.5` invalid |
| `Test_JsonWriter_ByteFormat_Golden` | `Zenith_JsonWriter_Tests.cpp` | a document exercising every element of "Byte format" equals `Tests/Fixtures/Json/writer_golden_pretty.json` and `_compact.json` byte for byte |
| `Test_JsonWriter_StructureErrors` | `Zenith_JsonWriter_Tests.cpp` | each structural misuse yields its error and the error is sticky |
| `Test_JsonWriter_NonFinite_Rejected` | `Zenith_JsonWriter_Tests.cpp` | NaN, `+inf`, `-inf` give `JSON_WRITE_ERROR_NON_FINITE_NUMBER` |
| `Test_JsonWriter_OutOfMemory_Sticky` | `Zenith_JsonWriter_Tests.cpp` | heap fault injection during growth gives `JSON_WRITE_ERROR_OUT_OF_MEMORY` from `Finish` |
| `Test_JsonReader_Conformance` | `Zenith_JsonDocument_Tests.cpp` | the committed JSONTestSuite corpus (`Tests/Fixtures/Json/conformance/`, `y_*` accepted, `n_*` rejected, `i_*` recorded outcome) |
| `Test_JsonReader_LimitsAtAndAboveEachLimit` | `Zenith_JsonDocument_Tests.cpp` | each limit of `Zenith_JsonLimits` accepted at the limit and rejected at limit + 1 with its code ([Z2-N-ARCH-953]) |
| `Test_JsonReader_DuplicateKey_Rejected` | `Zenith_JsonDocument_Tests.cpp` | position of the second occurrence |
| `Test_JsonReader_Positions_LineColumn` | `Zenith_JsonDocument_Tests.cpp` | every node of a fixture with tabs, CRLF and multi-byte text reports its recorded line and column |
| `Test_JsonReader_RoundTrip_PreservesOrderAndRawNumbers` | `Zenith_JsonDocument_Tests.cpp` | parse then `Write` in pretty style reproduces a canonical fixture byte for byte, including `1.10`, `1e2` and an integer above 2^53 |
| `Test_JsonDocument_Mutation_InsertRemoveRoundTrips` | `Zenith_JsonDocument_Tests.cpp` | on a parsed document: `CreateValue` and `InsertMember` at the first, a middle and the last position, `RemoveMember`, `InsertElement`, `RemoveElement`, and each `Set…` including a kind change, followed by `Write`, give the recorded fixture byte for byte; unmodified members keep their raw number text; an out-of-range position returns `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE`; a heap fault returns `JSON_ACCESS_ERROR_OUT_OF_MEMORY` with the document unchanged; a node created after parsing reports line 0 |
| `Test_JsonReader_BomAndNul_Rejected` | `Zenith_JsonDocument_Tests.cpp` | leading BOM and `\u0000` rejected |
| `Test_JsonReader_BackendAllocations_UseRootHeap` | `Zenith_JsonDocument_Tests.cpp` | a parse under a test heap raises the heap's allocation count, returns it to the starting value after the document is destroyed, and a heap fault during the backend's parse gives `JSON_READ_ERROR_OUT_OF_MEMORY` ([Z2-N-CONTRACT-240]) |
| `Test_JsonReader_Fuzz` (`Zenith_Fuzz_JsonDocument`) | `Tests/Fuzz/JsonDocument.cpp` | libFuzzer under `win-asan`: no crash, no leak, no accepted input that the writer cannot re-emit ([Z2-N-ARCH-952]) |
| `Test_DocumentTree_FindPath_MembersAndEscapes` | `Zenith_DocumentTree_Tests.cpp` | over a parsed document: `""` returns the cursor's root; `/a/b` a nested member; `/a~1b/~0c` the member `~c` of the member `a/b`; `/` the member whose key is empty; `/Zenith.LocalTransform` and `/Zenith/LocalTransform` reach different nodes in a tree holding both a member `Zenith.LocalTransform` and a member `Zenith` with a child `LocalTransform`; a cursor rooted at a subtree resolves relative to it and gets `JSON_ACCESS_ERROR_MISSING_MEMBER` for a sibling of its root |
| `Test_DocumentTree_FindPath_ArrayIndices` | `Zenith_DocumentTree_Tests.cpp` | over a three-element array: `/arr/0` and `/arr/2` resolve; `/arr/3`, `/arr/01`, `/arr/-`, `/arr/+1`, `/arr/x`, `/arr/` and `/arr/4294967296` each return `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE` |
| `Test_DocumentTree_FindPath_InvalidPointer` | `Zenith_DocumentTree_Tests.cpp` | `a`, `/a~`, `/a~2`, `/~x/b`, a pointer holding the byte `0xC3` alone and a 4,097-byte pointer each return `JSON_ACCESS_ERROR_INVALID_POINTER`; `/missing/~2` returns `JSON_ACCESS_ERROR_INVALID_POINTER`, not `MISSING_MEMBER`; a well-formed 4,096-byte pointer is accepted by the grammar and returns `JSON_ACCESS_ERROR_MISSING_MEMBER` |
| `Test_DocumentTree_FindPath_ScalarAndUnchanged` | `Zenith_DocumentTree_Tests.cpp` | a token after a number, a string, a bool and a null each returns `JSON_ACCESS_ERROR_WRONG_KIND`; across 1,000 resolutions, successful and failing, the test heap's live and total allocation counts are unchanged and `Write` of the document gives the same bytes as before |
| `Test_DocumentTree_StepEditsThroughCursor` | `Zenith_DocumentTree_Tests.cpp` | a function of the `Zenith_PfnMigrationStep` shape, given a cursor rooted at one component object, finds `/fields` with `FindPath`, renames member `1` to `5` through `GetDocument()`'s `RemoveMember` and `InsertMember`, and the document's pretty `Write` equals the expected text byte for byte, members outside the cursor's subtree untouched; a narrower cursor built from a returned node resolves relative to that node |
| `Test_DocumentTree_FindPath_MatchesReferenceResolver` | `Zenith_DocumentTree_Tests.cpp` | `Zenith_PropertyGen`, 16 seeds × 512 pointers (escaped keys, keys holding `.`, `/` and `~`, valid and invalid indices, malformed escapes, invalid UTF-8) over a generated tree of depth 6: `FindPath` returns the node or error code a reference resolver in the test (recursive, token list built by splitting first) returns; a failure prints the seed and the pointer bytes in hexadecimal |
| `Test_DocumentTree_NullRoot_Asserts` | `Zenith_DocumentTree_DefectTests.cpp` | `z2-death-assert`: constructing a cursor over `Zenith_JsonNode{}` |
