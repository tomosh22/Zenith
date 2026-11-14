# UI design: localization

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies locales and negotiation, string tables and keys, message formatting (arguments, plural rules, select, rich-text tags), number formatting, switching locale at run time, font selection per locale, localization extraction with its report and exchange files, pseudo-localization and stress modes, and localization validation. The authoring documents and the cooked `.zloc` format are in `Zenith2/Docs/Design/UI/Documents.md` ("Localization project document schema", "String table document schema", "Message syntax", "Cooked localization table format"); the runtime API is `Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UILocalization header"`.

## Scope of this file

| Topic | Section |
|---|---|
| which language is shown | "Locales and negotiation", "Locale switching at run time" |
| keys and tables | "String tables and keys" |
| turning a key into text | "Message formatting", "Number formatting" |
| scripts and fonts | "Font selection per locale" |
| getting strings to translators and back | "Localization extraction" |
| proving layouts survive translation | "Pseudo-localization and stress modes", "Localization validation" |

## Locales and negotiation

<!-- elaboration: the negotiation steps and fallback chain -->
- A locale tag is a BCP 47 subset: `language` (2–3 lowercase letters) `[-Script]` (4 letters, title case) `[-REGION]` (2 uppercase letters or 3 digits), at most 31 bytes; `Zenith_ParseLocale` canonicalises case and rejects anything else (`LOCALIZATION_ERROR_BAD_LOCALE`). The pseudo-locales `qps-ploc` (accented) and `qps-plocm` (mirrored) are reserved.
- The requested list is `ui.locale` when set, otherwise the platform's preferred locale (`Zenith_Platform::GetPreferredLocale`, `Zenith2/Docs/Design/PLAT/Platform.md`), a one-entry list; `PLATFORM_EVENT_LOCALE_CHANGED` re-runs negotiation when `ui.locale` is unset.
- Negotiation over the project's `locales`, for each requested tag in order, first match wins:
  1. an exact tag match;
  2. a match after adding the likely script (CLDR likely subtags: `zh-CN` → `zh-Hans-CN`, `zh-TW` → `zh-Hant-TW`, `sr-RS` → `sr-Cyrl-RS`) or removing the region (`fr-CA` → `fr`);
  3. a supported locale with the same language and script and any region (`fr-CA` → `fr-FR`), preferring the CLDR parent region;
  4. no request matches: the source locale.
- The fallback chain of the chosen locale, used per key, is its project-declared `fallback` list when present, else: the tag without region, then the tag's language alone, then the source locale; duplicate and unsupported entries are removed. A key missing in the whole chain formats as the missing-key marker (§ "Message formatting").
- Text direction is a property of the locale (`IsRightToLeft`: Arabic, Hebrew, Persian, Urdu scripts), taken from CLDR's character order; it sets the default paragraph direction of `AUTO` paragraphs with no strong character and mirrors layouts: a screen root's `layoutDirection: auto` resolves to the locale's direction, and the shell mirrors the placement of every child of a right-to-left element inside its parent (Layout.md "Logical direction and mirroring").

## String tables and keys

- A key is `namespace.name`; the namespace is one of the project's `namespaces` or `engine`; names follow `^[a-z][A-Za-z0-9_.]{0,63}$`, so `menu.settings.audio.title` is a key in namespace `menu`.
- Source of truth: `<project directory>/<locale>/<namespace>.strings.json`, one file per (locale, namespace), where the project directory is the directory of the game's localization project `Games/<Name>/Localization/localization.json` (the engine's: `Zenith2/Engine/ZenithUI/Internal/Content/Localization/`), as `Zenith2/Docs/Design/UI/Documents.md → "Authoring document family"` states, so a game's French menu table is `Games/<Name>/Localization/fr-FR/menu.strings.json`; the source locale's files declare every key and its arguments; target-locale files carry translations of a subset.
- The cook (`ui.localization` step) compiles one `.zloc` per locale (Documents.md "Localization sections"), merging the engine namespace and every game namespace and resolving the fallback chain's absence at run time, not at cook: a target table holds only its own translations, and lookup walks the chain's loaded tables.
- At run time all tables of the active chain are resident (a few hundred kilobytes per locale for a 10,000-string game, counted in `Z2-B-UI-001`); a lookup is a binary search over 64-bit key hashes, O(log n).
- Keys are referenced by hash everywhere at run time: UI documents' TEXTS records, `UI.Loc` results (Luau string IDs interned from the key text, then hashed), caption cue keys. The key text itself is kept only in development builds for diagnostics.

## Message formatting

`Zenith_Localization::Format(keyHash, args, arena)` and `FormatTo` interpret the key's compiled message (Documents.md "Message bytecode") from the first table in the chain that holds it:

1. Missing key: the result is the missing-key marker `⟦key⟧` (U+27E6, the key text, U+27E7) in development builds and `⟦#hash⟧` (the 16 lowercase hexadecimal digits of the key hash) in Shipping, where the key text is not kept; the counter `ui.loc.missing` increments, and development builds log the key once.
2. Arguments are matched by name hash; an argument the message references but the caller did not supply formats as `{name}` and counts `ui.loc.missing_arg`; extra arguments are ignored.
3. The output is UTF-8 text with rich-text span markers when the caller is the text engine (`Format` into a span list for `Zenith_TextLayoutEngine`), plain text otherwise (tags dropped, prompts and sprites as their labels).
4. Cost: linear in the message length plus the formatted output; no allocation outside the supplied arena or span.

### Argument formatting

| Argument kind (`Zenith_MessageArg::m_uKind`) | Default format |
|---|---|
| 1 integer | the locale's integer format (grouping, native digits) |
| 2 number | the locale's decimal format with up to 3 fraction digits, trailing zeros removed |
| 3 text | inserted verbatim (player names, user content), wrapped in Unicode isolates (FSI … PDI) so a name in another script cannot reorder the surrounding sentence |
| 4 string key | the key's own formatted message (no arguments), isolated like text |
| 5 select keyword | only valid as a `select` argument |

The `number` styles: `integer` (rounds half to even), `percent` (× 100, the locale's percent pattern), `::.N` (exactly N fraction digits).

### Plural rules

- Cardinal and ordinal rules for every supported locale come from the pinned `cldr-json` data entry's `plurals.json` and `ordinals.json` (`cldr-core/supplemental/`, the third-party data entry of the entry document's "Dependencies"), compiled by the cook to plural programs (Documents.md "Plural rule programs") stored in each `.zloc`.
- Operands follow CLDR: for a formatted value, `n` is its absolute value, `i` its integer part, `v` and `f` the visible fraction digits after applying the argument's fraction style (so `{count, number, ::.1}` with 1 gives `v = 1`, category `other` in English: "1.0 items").
- Selection: exact `=N` cases first, then the category of `value − offset`; `#` prints `value − offset`.
- `Test_UILoc_PluralRules_MatchCLDRSamples` evaluates every rule against every `@integer` and `@decimal` sample CLDR lists for it, which proves the compiler and interpreter against CLDR's own data without committing expected outputs.

### Select

`{gender, select, female {…} male {…} other {…}}` picks the case whose keyword equals the argument's keyword (hash comparison), else `other`. Keywords are author-defined; validation requires the same keyword set's `other` in every translation (rule 96 through `LOCALIZATION_ERROR_MISSING_OTHER`).

### Rich-text tags in messages

| Tag | Effect in the text engine |
|---|---|
| `<b>`, `<i>`, `<u>` | bold (the family's next weight ≥ 700), italic, underline spans |
| `<c=token>` | colour from the theme palette token, resolved per palette (accessible colour modes recolour it) |
| `<prompt action="Context.Action"/>` | an inline object: the action's current binding prompt (Interaction.md "Input prompts and remapping"), one em high, baseline-aligned; plain-text output and narration use the binding's label |
| `<img sprite="name"/>` | an inline sprite, one em high; its accessible text is the sprite's `label` key when the theme declares one, else nothing |
| `<br/>` | a hard line break |

Tags never carry links, scripts or arbitrary styles, so a translation cannot alter anything but emphasis, colour within the theme and inline objects the source declares; validation (rule 96) requires a translation's prompt and sprite objects to be the same multiset as the source's.

## Number formatting

`FormatNumber` uses the `.zloc` NUMBERS section: decimal and group separators, minus sign, percent pattern, grouping sizes (primary 3; `hi-IN` primary 3 and secondary 2, giving 12,34,567), minimum grouping digits (Spanish: 2, so 1234 is ungrouped), and native digits for the locale's default numbering system (`ar` uses `arab` digits where CLDR makes them default; `ar-MA` and most locales use `latn`). Rounding is half-even to the requested fraction digits; values beyond ±2^53 format through the integer path only when integral. Compact notation, currencies, dates and units are not formatted by UI (Excluded and deferred of the requirements).

## Locale switching at run time

1. `ui.locale` changes (a settings control) or `Zenith_UISystem::SetLocale` is called: the new chain's tables are requested through the asset registry (asynchronous, `ASSET_PRIORITY_HIGH`).
2. Until every table of the chain is loaded the active locale stays; at the first prologue after they load, `CommitPendingLocale` swaps the chain atomically (the next extractor sees the new locale), font stacks are rebuilt for the new locale's selections (§ "Font selection per locale"), every context marks every root `STYLE_DIRTY | MEASURE_DIRTY | LAYOUT_DIRTY` (text leaves re-measure because the localization revision is part of the text cache key, and `layoutDirection: auto` re-resolves), and the old chain's tables are released.
3. The switch takes effect on a frame boundary, never mid-frame; narration announces nothing; the simulation is unaffected (keys, not text, are simulation data).
4. A table that fails to load (corrupt, missing) leaves the previous locale active, logs the table under `ui.loc` and counts `ui.loc.locale_load_failed`; a locale outside the project's list negotiates as in "Locales and negotiation".

## Font selection per locale

- A theme's font tokens (`body`, `title`, …) resolve per locale: the localization project's `fonts.locales` override for the active locale (by negotiation), else `fonts.default`, else the theme's `fonts`.
- Each resolved stack is extended by the project's `fonts.scripts` families for scripts the stack's faces do not cover, then by the engine's default fallback stack (Noto Sans for Latin, Greek, Cyrillic; Noto Sans Arabic, Hebrew, Devanagari, Thai; Noto Sans CJK subsets SC, TC, JP, KR, chosen for Han by the active locale's language so a Japanese locale shows Japanese glyph forms); per-cluster fallback is Text.md "Font fallback".
- The cook validates that every locale's resolved stacks cover the code points its tables use (the `ui.localization` step builds a per-locale coverage set and reports uncovered code points as rule 82 warnings with the key names that use them).
- Locale-specific line breaking (Thai, Lao, Khmer, Burmese dictionaries; CJK strictness by locale) is selected in the paragraph pipeline from the paragraph's language (Text.md "Line breaking").

## Localization extraction

### Extraction sources

The cook step `ui.extract` (global-post, after every document step) collects every key in use and its context:

| Source | What is collected |
|---|---|
| UI documents | every text value with `key`, every `accessibility.label`/`hint`, every `choice` option key, with the document path, element `id` or uid, and `maxWidthEm` when the element constrains it |
| Luau scripts | each `UI.Loc("<literal>")` call: the cook lexes the game's Luau sources and records the string literal of every call whose first argument is a literal, with file and line; a non-literal first argument is reported as unextractable (`ui.extract.dynamic_key` warning) |
| data-defined components | properties whose schema declares `"localized": true` (row UI14) in every scene and prefab value |
| caption and dialogue sources | keys other areas' cook steps contribute through `Zenith_UIExtractionSink` (AUDIO and CINE pending, row UI4) |
| the engine namespace | every engine key, always |

The sink is a tool-side interface other cook steps receive from the cook services:

```cpp
// Zenith2/Tools/ZenithCook/Internal/UI/Zenith_UIExtraction.h
struct Zenith_UIExtractedKey
{
	Zenith_StringView m_strKey;           // "menu.start"
	Zenith_StringView m_strSourcePath;    // repo-relative content path
	uint32 m_uLine = 0;                  // 0 when not line-addressable
	Zenith_StringView m_strContext;       // element id, property path or call site
	float m_fMaxWidthEm = 0.0f;           // 0 = unconstrained
};

class Zenith_UIExtractionSink
{
public:
	void AddKey(const Zenith_UIExtractedKey& xKey);          // any cook worker; locked (cook lock order, ASSET)
};

struct Zenith_UIExtractionResult
{
	Zenith_Span<const Zenith_UIExtractedKey> m_axUses;            // sorted by key, then source path, then line
	Zenith_Span<const Zenith_StringView> m_axUndefinedKeys;       // used, absent from the source locale
	Zenith_Span<const Zenith_StringView> m_axUnusedKeys;          // defined in the source locale, never used
	Zenith_Span<const Zenith_StringView> m_axDynamicCallSites;    // UI.Loc with a non-literal key
};

struct Zenith_UIXliffRejectedUnit
{
	Zenith_StringView m_strKey;
	Zenith_LocalizationError m_xError;   // from Zenith_CompileMessage against the source message's arguments
};

struct Zenith_UIXliffImportReport
{
	Zenith_StringView m_strLocale;
	uint32 m_uAccepted = 0;
	uint32 m_uUnchanged = 0;
	Zenith_Span<const Zenith_UIXliffRejectedUnit> m_axRejected;
	Zenith_Span<const Zenith_StringView> m_axUnknownKeys;   // units naming no source-locale key; never written
};
```

### Extraction report and exchange files

- `ui.extract` writes `<cook output>/Reports/Localization/extraction.json` (canonical JSON: per key its uses, its source text, comment and arguments, and per target locale whether a translation exists and whether it is stale), and a per-locale summary (translated, missing, stale counts).
- Staleness: each translation records the source text hash it was translated from (`"sourceHash"` in the string table entry, written by the XLIFF import); a source change makes the translation stale, still shown, and listed.
- Exchange (cook command-line verbs registered by UI's cook steps, row UI17): `zenith cook --export-xliff <locale>` writes XLIFF 2.0 per namespace (`Zenith_WriteXliffExport`: `<unit id="key">`, `<source>`, `<target>` when translated, `<note>` from the comment and the extracted contexts and `maxWidthEm`, rich-text tags as `<ph>` and `<pc>` inline elements so translators cannot break them); `zenith cook --import-xliff <file>` (`Zenith_ReadXliffImport`) validates each target with `Zenith_CompileMessage` against the source's arguments and writes accepted translations into the locale's string table through the document system's journaled write (a migration-safe edit; rejected units are listed with their errors and leave the table unchanged).
- Undefined keys used by a document are rule 50 errors; unused keys are reported, never deleted.

## Pseudo-localization and stress modes

Selected by `ui.loc.pseudo` (0 off, 1 accented, 2 mirrored, 3 long); the key exists in every build and defaults to 0, and the engine's settings screens never expose it, so it is reached through the command line or a development configuration layer:

| Mode | Transformation of every formatted message |
|---|---|
| accented (`qps-ploc`) | ASCII letters map to accented look-alikes (a → å, e → é, …), text is expanded by `ui.loc.pseudoLongFactor` (1.4) with padding characters "~" distributed between words, and wrapped in `[` `]` when the project's `pseudo.markers` is true, so truncation and concatenation are visible |
| mirrored (`qps-plocm`) | the accented transformation, then the paragraph direction forced RTL and Latin letters wrapped in RLO … PDF, so layouts mirror as for Arabic without needing a translation |
| long | each message replaced by its longest translation among the project's locales, then expanded by `ui.loc.pseudoLongFactor` (default 1.4) with the same "~" padding as accented mode, so real worst-case lengths are exercised |

Arguments, tags and inline objects are transformed only in their text parts, so pseudo text still formats, plural-selects and prompts correctly. Stress runs combine a mode with `ui.access.textScale` 2.0 in the layout suite.

## Localization validation

| Check | Where | Severity |
|---|---|---|
| syntax, arguments, tags, plural categories (rule 96) | `ui.localization` cook step | error |
| undefined key in a document (rule 50) | document cook | error |
| missing translation (rule 92) | `ui.localization` | warning; the extraction report lists it |
| `maxWidthEm` exceeded by a translation (rule 98) | `ui.localization`: the translation is shaped with the reference font at 1 em and its longest line measured | per project `checks.maxWidth` |
| uncovered code points per locale (rule 82) | `ui.localization` | warning |
| layout overflow under pseudo modes and long mode | `Test_UILayoutGolden_PseudoLong` (one `SUBCASE` per text case) and `Test_UILoc_StressAllEngineDocuments` (Tests.md "Layout golden suite") | test failure |
| RTL mirroring | `Test_UILayoutGolden_MirroredMatchesReflection`: under an RTL locale the rectangles of every layout case marked "Mirrored" equal the reflection `x' = W − x − w` of their LTR rectangles (Layout.md "Logical direction and mirroring"); `Test_UILoc_StressAllEngineDocuments` lays out every engine document under `qps-plocm` and asserts no overflow | test failure |
