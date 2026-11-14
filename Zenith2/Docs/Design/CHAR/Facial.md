# CHAR design: facial animation and lip-sync workflows

Section file of `Zenith2/Docs/Design/CHAR.md`. It specifies the production side of faces: the viseme set, the face part of the graph template, automatic viseme pose books for morph-target faces, the character project settings document (the game's voice-locale list and character classes), the lip-sync document, the deterministic viseme estimator over dialogue audio, the external viseme import, manual overrides, the locale-strided lip-sync clip and its intermediate, the lip-sync importer, and the playback contract every player of a lip-sync clip follows. ANIM owns the runtime that plays the result (curves, pose libraries, slots: `Zenith2/Docs/Design/ANIM/Runtime.md → "Facial animation"`); AUDIO owns dialogue lines and their sounds (`Zenith2/Docs/Design/AUDIO/Mixer.md → "Dialogue lines"`); CINE owns cinematic dialogue sections (`Zenith2/Docs/Design/CINE/Playback.md → "Dialogue tracks"`). CHAR produces data and never runs at game time.

## Scope of this file

| Topic | Heading |
|---|---|
| the curves a face is driven by | "Viseme set" |
| the face part of every humanoid graph | "Face graph template" |
| morph-target faces | "Automatic viseme pose books" |
| game-wide character settings | "Character project settings document" |
| the per-bank lip-sync document | "Lip-sync document" |
| filling it from AUDIO's bank | "Sync from bank" |
| the viseme estimator | "Estimator input", "Estimator framing and spectrum", "Estimator features", "Estimator classification", "Estimator smoothing and normalisation", "Phrase markers" |
| other viseme sources | "External viseme import", "Manual overrides" |
| the cooked result | "Locale-strided lip-sync clip", "Window markers and events", "Clip intermediate writer", "Lip-sync importer" |
| how a lip-sync clip is played | "Lip-sync playback contract", "Voice locale as a simulation input" |
| limits | "Facial and lip-sync limits" |

## Viseme set

<!-- elaboration -->
The viseme set `zenith15` is fifteen mouth-shape classes plus one energy curve, each a `CONTROL` curve of the face skeleton (added by the facial rig template, `Zenith2/Docs/Design/CHAR/Rigging.md → "Facial rig template"`):

| Value | Enumerator | Curve | Phoneme class (examples) | Produced by the estimator |
|---|---|---|---|---|
| 0 | `CHAR_VISEME_SIL` | `char.viseme.sil` (default 1) | silence, rest | yes |
| 1 | `CHAR_VISEME_PP` | `char.viseme.pp` | p, b, m | yes (onsets) |
| 2 | `CHAR_VISEME_FF` | `char.viseme.ff` | f, v | yes |
| 3 | `CHAR_VISEME_TH` | `char.viseme.th` | θ, ð | no |
| 4 | `CHAR_VISEME_DD` | `char.viseme.dd` | t, d | no |
| 5 | `CHAR_VISEME_KK` | `char.viseme.kk` | k, g | no |
| 6 | `CHAR_VISEME_CH` | `char.viseme.ch` | tʃ, dʒ, ʃ | yes |
| 7 | `CHAR_VISEME_SS` | `char.viseme.ss` | s, z | yes |
| 8 | `CHAR_VISEME_NN` | `char.viseme.nn` | n, l | yes |
| 9 | `CHAR_VISEME_RR` | `char.viseme.rr` | r | no |
| 10 | `CHAR_VISEME_AA` | `char.viseme.aa` | ɑ, a | yes |
| 11 | `CHAR_VISEME_E` | `char.viseme.e` | e, ɛ | yes |
| 12 | `CHAR_VISEME_IH` | `char.viseme.ih` | ɪ, i | yes |
| 13 | `CHAR_VISEME_OH` | `char.viseme.oh` | o, ɔ | yes |
| 14 | `CHAR_VISEME_OU` | `char.viseme.ou` | u, ʊ, w | yes |
| — | — | `char.lipsync.energy` | jaw amplitude in [0, 1] | yes |

`uCHAR_VISEME_COUNT` = 15. The four classes the estimator never produces come from external viseme timing or manual overrides only; the estimator's output is a usable baseline whose quality is judged in CinematicSlice's sign-off ([Z2-N-BUDGET-435]), never by a test. The estimator's weights obey two invariants, both stated over the fifteen viseme curves (the energy curve is not a weight): on the binary64 frame track, before key reduction and storage, `Σ_{v ≠ sil} w_v ≤ 1 + 1e-12` (the division of step 3 can leave the sum one ulp above 1) and `w_sil = 1 − Σ_{v ≠ sil} w_v` within 1e-12 at every key; on the stored track (f32 values, after per-curve key reduction), with every curve sampled by linear interpolation of its own keys, `|Σ_v w_v(t) − 1| ≤ 15 × reduceEpsilon + 1e-6` at every key time of every curve, the 1e-6 covering fifteen f32 roundings of at most 2⁻²⁵ each ("Estimator smoothing and normalisation", step 6). Phrase markers restore rest exactly (the sum is 1 at their keys); manual overrides are not renormalised ("Manual overrides").

## Face graph template

The humanoid graph template (`Zenith2/Docs/Design/CHAR/Workflow.md → "Graph template catalogue"`) ends its base-layer path with the face part, in this node order from the body subtree to the root:

| Order | Node | Payload | Purpose |
|---|---|---|---|
| 1 | the body subtree | the template's locomotion machine and full-body slots | body |
| 2 | `SLOT` `char.lipsync`, mask `LipSyncCurves` | blend in 0.05 s, blend out 0.10 s, root motion false | gameplay lip-sync clips; the mask passes only the sixteen viseme-set curves |
| 3 | `SLOT` `cine.face`, mask `Face` | blend in 0.1 s, blend out 0.2 s, root motion false | cinematic face performances (CINE's slot name, cross-area row CN14) |
| 4 | `POSE_BLEND` over the face pose book | one entry per viseme (frame of pose `viseme.<v>`, source kind 1, curve `char.viseme.<v>`), then one per Face-group control of kind `Pose` (source: the control's curve) | turns curves into the face pose |

A slot with no clip outputs its input unchanged (`Zenith2/Docs/Design/ANIM/Graph.md → "Slots"`), so a character without dialogue keeps whatever face animation the body clips carry. The two masks are generated with the template as `.animmask.json` documents (`Zenith2/Docs/Design/ANIM/Formats.md → "Mask document schema"`): `LipSyncCurves` has `default` 0, no joint entries and one `curves` entry of weight 1 per viseme-set curve; `Face` has `default` 0, the entry `{ joint: <HEAD joint>, weight: 1, recursive: true }` followed by `{ joint: <HEAD joint>, weight: 0, recursive: false }` (the face below the head, not the head itself), and weight-1 `curves` entries for the viseme-set curves and every Face-group control curve.

<!-- assumption: pending ANIM (row CH1) -->
A slot entry of the graph document carries an optional `mask` member (the persistent ID of a mask document), compiled into the slot's mask; ANIM's text already states that masks apply to slots (`Zenith2/Docs/Design/ANIM/Graph.md → "Mask weights"`) and that a slot blends `Blend(input_j, slot_j, weight × mask_j)`, but the slot entry of "Graph document schema" lists no `mask` member; row CH1 asks ANIM to add it. Until it lands, the template places the two slots in an override layer and loses the body clips' face animation, and CHAR's Stage 4 template ticket stays `draft`.
<!-- /assumption -->

`Test_CharFaceTemplate_GraphCompilesClean` compiles the instantiated template with `Zenith_CompileAnimationGraph` and asserts no diagnostic; `Test_CharFaceTemplate_EmptySlotsKeepBodyFace` evaluates it in a `Zenith_AnimationPreview` over a body clip that animates a face curve and asserts the curve passes through unchanged when no slot plays; `Test_CharFaceTemplate_LipSyncCurvesOnly` plays a lip-sync clip in `char.lipsync` and asserts that no joint's local transform changes.

## Automatic viseme pose books

For a face driven by morph targets, `character.posebook.generateVisemes [skeleton, meshes, path]` writes a pose book (`Zenith2/Docs/Design/CHAR/Rigging.md → "Pose book document"`) whose pose `viseme.<v>` sets morph curves to 1, found by name:

1. The candidate morph names per viseme, in order: `viseme_<v>`, `v_<v>`, `<v>` (case-insensitive ASCII), and for the ARKit-style blend-shape convention the fixed combination table below, which applies to a viseme only when every curve of its row matches a target (a row with an unmatched curve leaves the viseme unmapped, step 4).
2. A mesh's targets are known by hash (`Flux_MeshResource::GetMorphTargetNameHash`, `Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"`, the accessor ANIM's row AN4 adds); a candidate matches when `Zenith_Hash64` of its UTF-8 bytes equals a target's hash. Candidates are tried in order; the first match wins.
3. The ARKit-style combination table, used when no direct name matches a viseme:

| Viseme | Curves and values |
|---|---|
| `pp` | `mouthClose` 0.6, `mouthPressLeft` 0.4, `mouthPressRight` 0.4 |
| `ff` | `mouthRollLower` 0.6, `mouthUpperUpLeft` 0.2, `mouthUpperUpRight` 0.2 |
| `ch` | `mouthFunnel` 0.5, `jawOpen` 0.15 |
| `ss` | `mouthStretchLeft` 0.3, `mouthStretchRight` 0.3, `jawOpen` 0.05 |
| `nn` | `jawOpen` 0.1, `mouthClose` 0.2 |
| `aa` | `jawOpen` 0.55 |
| `e` | `jawOpen` 0.3, `mouthStretchLeft` 0.4, `mouthStretchRight` 0.4 |
| `ih` | `jawOpen` 0.2, `mouthSmileLeft` 0.3, `mouthSmileRight` 0.3 |
| `oh` | `jawOpen` 0.35, `mouthFunnel` 0.5 |
| `ou` | `jawOpen` 0.15, `mouthPucker` 0.8 |
| `th`, `dd`, `kk`, `rr` | `jawOpen` 0.15, 0.15, 0.2, 0.15 |

4. A viseme with no match gets no pose (its POSE_BLEND entry is omitted by the template, `CHAR_DIAG_VISEME_UNMAPPED` per viseme).

`Test_CharVisemePoseBook_DirectNamesWin` and `Test_CharVisemePoseBook_ArkitCombination` cover both paths on generated target-name sets.

## Character project settings document

One per game: `Games/<Name>/Assets/Characters/<Name>.charproject.json`, document type `Zenith.CharacterProjectDocument`, schema tag `zenith2.charproject`, version 1. A second one in a game is a validation error (`CHAR_DIAG_PROJECT_DUPLICATE`, both paths named).

```json
{
	"$schema": "zenith2.charproject",
	"schemaVersion": 1,
	"id": "5d0a…",
	"voiceLocales": [ "en", "fr", "de" ],
	"visemeSet": "zenith15",
	"frontMarkerNode": "char.front",
	"maxFullRateCharactersPerScene": 8,
	"classes": {
		"hero":  { "label": "character-hero",  "heightRangeMetres": [1.4, 2.2], "massKilograms": 75, "significanceBias": 4.0, "forceFullRate": true,  "allowReducedRootMotion": false,
			"libraryRig": "7c2e…" },
		"npc":   { "label": "character-npc",   "heightRangeMetres": [1.2, 2.3], "massKilograms": 70, "significanceBias": 1.0, "forceFullRate": false, "allowReducedRootMotion": true },
		"crowd": { "label": "character-crowd", "heightRangeMetres": [1.2, 2.3], "massKilograms": 70, "significanceBias": 0.5, "forceFullRate": false, "allowReducedRootMotion": true }
	}
}
```

| Member | Type | Rule |
|---|---|---|
| `voiceLocales` | ordered array (rule C8) of BCP 47 tags | 1 … 16 entries (`uCHAR_MAX_VOICE_LOCALES`), unique, each a tag AUDIO's locale rule accepts (`Zenith2/Docs/Design/AUDIO/Mixer.md → "Locale selection"`); the order defines every lip-sync clip's window index |
| `visemeSet` | string | `zenith15` (the only value at version 1) |
| `frontMarkerNode` | string | the node name the import wizard sets as CONTENT's `validate.frontMarkerNode` ("Character import wizard" of `Zenith2/Docs/Design/CHAR/Workflow.md`) |
| `maxFullRateCharactersPerScene` | integer in [0, 1,024], default 8 | the class rate lint's threshold (rule CB2 of "Class rate lint" in `Zenith2/Docs/Design/CHAR/Debugging.md`) |
| `classes` | keyed map class ID → class | 1 … 16 classes; class IDs `^[a-z][a-z0-9]{0,15}$` |
| `classes.*.label` | string | the `.meta` label selecting the class's content rules (`Zenith2/Docs/Design/ASSET/Cook.md → "Content rules"`), `^character-[a-z0-9-]{1,22}$` |
| `classes.*.heightRangeMetres` | [min, max] | the import wizard's `validate.heightRangeMetres` and the rest-pose height check |
| `classes.*.massKilograms` | number in [1, 1,000] | the character's mass; the default ragdoll's parts carry 0.93 of it, the neck and hands having no part ("Default ragdoll generation" of `Zenith2/Docs/Design/CHAR/Rigging.md`) |
| `classes.*.significanceBias`, `forceFullRate`, `allowReducedRootMotion` | number in [0.01, 100], bools | the class's animator defaults ("Character class presets" of `Zenith2/Docs/Design/CHAR/Debugging.md`) |
| `classes.*.libraryRig` | persistent ID, optional | the `Animation.Skeleton` product of the library rig the class's characters retarget from ("Shared library rig rule" of `Zenith2/Docs/Design/CHAR/Rigging.md`); the import wizard shows it, the retarget step reads it, and the workflow panel's retarget row is computed from it ("Workflow panel" of `Zenith2/Docs/Design/CHAR/Workflow.md`); absent: the class's characters use no library |

The project document is authoring data only: nothing at run time reads it (the `char.document` importer declares no product, `Zenith2/Docs/Design/CHAR.md → "Document validation importer"`). Every run-time decision a lip-sync clip needs is carried by the clip itself ("Window markers and events"), so neither a game script nor CINE duplicates `voiceLocales`. Validation rule CV17 warns about a class whose `label` selects no `CHARACTER_CLASS_<NAME>` row of the content-rule table (`Zenith2/Docs/Design/CHAR/Debugging.md → "Validation catalogue"`).

## Lip-sync document

One per (dialogue bank, face skeleton): `*.lipsync.json`, document type `Zenith.CharacterLipSyncDocument`, schema tag `zenith2.lipsync`, version 1, canonical. It must sit in a directory that is an ancestor of every audio source it names, because the importer reads them as import inputs by relative path ("Lip-sync importer").

```json
{
	"$schema": "zenith2.lipsync",
	"schemaVersion": 1,
	"id": "a07c…",
	"bank": "c3e1…",
	"bankDigest": "9f2c4e…",
	"skeleton": "1b7e…",
	"locales": [ "en", "fr", "de" ],
	"settings": { "silenceDb": -45.0, "leadSeconds": 0.04, "attackSeconds": 0.025, "releaseSeconds": 0.06,
		"reduceEpsilon": 0.002, "sampleRateHz": 60, "padSeconds": 0.25 },
	"lines": {
		"Ranger.Greet.01": {
			"locales": {
				"en": { "audio": "en/Ranger_Greet_01.wav", "source": "Estimate", "phrases": [ 0.0, 1.42 ], "overrides": [] },
				"fr": { "audio": "fr/Ranger_Greet_01.wav", "source": "Import", "visemes": "fr/Ranger_Greet_01.visemes.json", "phrases": [], "overrides": [] },
				"de": { "source": "Silent" }
			}
		}
	}
}
```

| Member | Type | Rule |
|---|---|---|
| `bank` | persistent ID | AUDIO's bank document whose dialogue lines this document covers (`Zenith2/Docs/Design/AUDIO/Formats.md → "Bank document schema"`) |
| `bankDigest` | 64 lowercase hex | the SHA-256 of the bank document's canonical re-serialisation at the last sync (the parsed document written back with SCENE's canonical writer, LF line endings by construction, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`), so a checkout that converted line endings does not read as stale; the panel shows the document stale when the bank's digest differs |
| `skeleton` | persistent ID | the face skeleton every clip of the document binds to |
| `locales` | ordered array | equals the project's `voiceLocales` (`CHAR_DIAG_LIPSYNC_LOCALES_DIFFER` otherwise, an import error, because window indices would disagree between characters) |
| `settings` | object | the estimator and layout settings; every member optional with the defaults shown; ranges: `silenceDb` [−90, −10], `leadSeconds` [0, 0.2], `attackSeconds` and `releaseSeconds` [0.005, 0.5], `reduceEpsilon` [0, 0.05], `sampleRateHz` {30, 60, 120}, `padSeconds` [0.2, 1] (the minimum is the derivation of "Lip-sync playback contract": 0.05 + 0.1 + 1 / 30 = 0.1834, rounded up to 0.2) |
| `lines` | keyed map line ID text → line | at most 4,096 lines; the key is AUDIO's line ID text (its `Zenith_AudioDialogueLineID` is `Zenith_Hash64` of it, `Zenith2/Docs/Design/AUDIO/PublicAPI.md → "Zenith_AudioTypes header"`); 1 … 96 bytes of `[A-Za-z0-9._-]` |
| `lines.*.locales` | keyed map locale tag → entry | every tag is in `locales`; a missing tag is `source: "Silent"` |
| `…source` | `"Estimate"`, `"Import"`, `"Silent"` | the viseme source |
| `…audio` | relative path | required unless `Silent`: the locale's sound source file, relative to the document's directory, no `..` |
| `…visemes` | relative path | `Import` only: a `zenith2.visemes` file ("External viseme import") |
| `…phrases` | ordered array of seconds | phrase starts ("Phrase markers"), ascending, each in [0, duration] |
| `…overrides` | ordered array | "Manual overrides" |

## Sync from bank

`character.lipSync.syncFromBank [document]` (an editor command, one undo record) brings the document in line with its bank:

1. Open the bank document (`Zenith2/Docs/Design/EDIT/Foundation.md → "Documents"`) and read its dialogue lines in ascending line ID text: for each line and each locale entry, the sound's GUID.
2. Resolve each sound GUID to its source path through the asset database (`Zenith2/Docs/Design/ASSET/Database.md → "Zenith_AssetDatabase API"`); express it relative to the lip-sync document's directory; a path that would need `..` fails the command with `CHAR_DIAG_LIPSYNC_AUDIO_OUTSIDE` naming the line, the locale and the path.
3. For each line in the bank: add a missing line (every project locale `Estimate` with its audio, or `Silent` when the bank has no sound for that locale); update the `audio` of existing entries; keep `source`, `visemes`, `phrases` and `overrides` of entries that exist.
4. Lines no longer in the bank are removed, and their overrides with them; the command's report lists them so the author can undo.
5. `phrases`: when an entry has none, copy the times of the markers flagged as lip-sync phrase starts (flag bit 1 of AUDIO's markers record, `Zenith2/Docs/Design/AUDIO/Formats.md → "Markers and loop points section"`, which allows two sources): first the bank document's markers for that line's sound in that locale, then, when the bank document lists none, the sound's import cue points from the sound import data the editor holds; times in seconds, ascending, a duplicate time kept once.

<!-- assumption: pending AUDIO (row CH2) -->
AUDIO's bank document schema (`Zenith2/Docs/Design/AUDIO/Formats.md → "Bank document schema"`) names no marker member yet. CHAR assumes that a dialogue line's locale entry may carry `"markers": [ { "name": "phrase0", "seconds": 0.0, "flags": 2 } ]` (name 1 … 47 bytes of UTF-8, seconds ≥ 0, flags the bit mask of the markers record, bit 1 = 2 the lip-sync phrase start), which the `audio.bank` step converts to the sound's marker records; row CH2 (b) asks AUDIO to fix the member's spelling and fields. The walkthrough's bank and `Zenith_CharMakeBankDocument` write this form, and until it lands the walkthrough's phrase-marker assertion and the sync's first source stay `draft`.
<!-- /assumption -->
6. Write `locales` from the project and `bankDigest` from the bank's canonical re-serialisation (the `bankDigest` row above).
7. Report, per line, when the bank's first locale entry is not `voiceLocales[0]`: AUDIO falls back to the bank's first locale entry for a voice locale the line lacks (`Zenith2/Docs/Design/AUDIO/Mixer.md → "Locale selection"`), and the lip-sync window fallback is window 0 ("Lip-sync playback contract"), so the two agree only when that entry is the project's first voice locale (validation rule CV18, warning `CHAR_DIAG_LIPSYNC_BANK_DEFAULT_LOCALE`).

The command is pure over its inputs apart from the database lookup; running it twice with no bank change produces no edit (`Test_CharLipSyncSync_SecondRunNoEdit`). `Test_CharLipSyncSync_AddsUpdatesRemovesLines`, `Test_CharLipSyncSync_PathOutsideRefused` and `Test_CharLipSyncSync_KeepsAuthoredSources` assert steps 2 to 5; `Test_CharLipSyncSync_ReportsBankDefaultLocale` asserts step 7 on a bank whose line lists `fr` before `en` under `voiceLocales` `["en", "fr"]`; `Test_CharLipSyncSync_DigestIgnoresLineEndings` syncs, rewrites the bank file with CRLF line endings, and asserts the document is not shown stale.

## Estimator input

The estimator is `Zenith_CharEstimateVisemes`, declared in `Zenith2/Docs/Design/CHAR.md → "Lip-sync API"` as

```cpp
Zenith_Result<void, Zenith_CharError> Zenith_CharEstimateVisemes(const Zenith_DecodedAudio& xAudio,
	const Zenith_CharLipSyncSettings& xSettings, Zenith_CharVisemeTrack& xOut, Zenith_Arena& xScratch);
```

It is a pure function whose output bytes are identical on every machine that runs one build (the orchestrator's X314-FMA3 ruling: tracked and baked outputs never call a CRT transcendental):

- Every arithmetic operation is a scalar IEEE-754 binary64 operation in the order this file writes it; every sum accumulates left to right in ascending index.
- No CRT transcendental is called ([Z2-N-ARCH-888]). Sines and cosines are `Zenith_Sin(double)` and `Zenith_Cos(double)`, the exponential `Zenith_Exp(double)`, the natural logarithm `Zenith_Ln(double)` and the decimal logarithm `Zenith_Log10(double)`, CORE's deterministic binary64 functions of `Zenith2/Engine/ZenithCore/Zenith_Transcendental.h`, reached through `Zenith_Maths.h` (`Zenith2/Docs/Design/CORE/Maths.md → "Transcendental declarations"`; the natural log is `Zenith_Ln` because `Zenith_Log.h` is CORE's logging API). π is CORE's `dPI` (`Zenith2/Docs/Design/CORE/Maths.md → "Numerical conventions"`). No CHAR source includes `<cmath>` or `<math.h>`: square roots are CORE's `Zenith_Sqrt(double)`, the hardware square root with the bits IEEE-754 prescribes (`Zenith2/Docs/Design/CORE/Maths.md → "Transcendental scope and guarantee"`), so no result differs from `std::sqrt`; the estimator needs none.
- CHAR's sources compile with precise floating point and no contraction: the flags every Zenith2 target carries, `/fp:precise` without `/fp:contract` and clang's `-ffp-contract=off` ([Z2-N-ARCH-188], `Zenith2/Docs/Architecture/BuildSystem.md → "MSVC flags for the win and win-asan presets"`). CHAR adds no per-file floating-point option.
- The rule binds every CHAR source under `Zenith2/Tools/Authoring/Internal/CHAR/`, `Zenith2/Tools/ZenithCook/Internal/Character/`, `Zenith2/Tools/ZenithEditor/Internal/Character/`, the fixture generators `Zenith2/Tools/Authoring/Tests/Support/Zenith_CharTestFixtures.h` and the harness `Zenith2/Benchmarks/CharacterAuthoring/` (whose report the ruling ticket commits), not only the estimator: the default ragdoll ("Default ragdoll generation" of `Zenith2/Docs/Design/CHAR/Rigging.md`), pose capture, control evaluation, rest-pose angles and pose diffs use the same functions, so no document CHAR writes depends on the CPU that wrote it.

CORE's functions are specified to the bit and their results do not depend on the CRT's FMA3 flag (`Zenith2/Docs/Design/CORE/Maths.md → "Transcendental scope and guarantee"`), and every `main` CHAR's tests run under calls `Zenith_ApplyCrtMathMode` first ([Z2-N-ARCH-889]). Row CH21 is answered. CHAR's estimator, ragdoll, capture and pose-diff tickets are blocked by CORE's tickets Z2T-CORE-100 (`sin`, `cos`, `atan2`, `acos`) and Z2T-CORE-101 (`exp`, `ln`, `log10`).

The cross-machine guarantee is tested in four ways:

| Test | Target | Asserts |
|---|---|---|
| `ZenithAuthoring.CharNoCrtTranscendental` | CTest (`cmake -P`), the script `Zenith2/CMake/Tests/Zenith_CharNoCrtTranscendental.cmake` registered in the test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, CORE's `ZenithCore.TranscendentalNoCrtCall` form (`Zenith2/Docs/Design/CORE/Maths.md → "Maths tests"`) | fails when a file under the five paths of the bullet above includes `<cmath>` or `<math.h>` or contains a `std::` or bare call of a CRT transcendental name (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `exp`, `exp2`, `log`, `log2`, `log10`, `pow`, `cbrt`, `sqrt`, and their `f` and `l` forms; CHAR's square roots are `Zenith_Sqrt`); a name matches when it is followed, after optional spaces, by `(` and preceded by the start of a line or by a character outside `[A-Za-z0-9_]` (so `Zenith_Sin(` and `m_dSin(` never match, while `std::sin(` and ` sin(` do; CMake regular expressions have no `\b`, so the script spells the boundary as `(^|[^A-Za-z0-9_])`); comments and string literals are scanned too, so a CHAR source never spells a forbidden call even in a comment. This is the gate of Z2-R-CHAR-133 |
| `Test_CharEstimator_IndependentOfCrtFma3Path` | `ZenithAuthoring_Tests` (Windows) | `Zenith_CharEstimatorAnalyse` and `Zenith_CharEstimatorBuildFrameTrack` run over every signal of "Estimator classification" twice in one process, first after `_set_FMA3_enable(1)` and then after `_set_FMA3_enable(0)`, and the **binary64** outputs are compared byte for byte: the `Zenith_CharFrameFeatures` arrays (E, F and Z pass through `Zenith_Log10`, `Zenith_Ln` and `Zenith_Exp` directly) and the binary64 frame tracks, as well as the stored tracks. The test calls the CRT's `_set_FMA3_enable` and `_get_FMA3_enable` itself, because CORE's `Zenith_SetCrtFma3EnabledForTest` is in a CORE `Internal/` header ZenithAuthoring's tests may not include (`Zenith2/Docs/Design/CORE/Maths.md → "The CRT maths mode"`); a scope guard restores the startup state (0) on every exit path, so a failing assertion cannot leave the flag on. When `_get_FMA3_enable()` does not read 1 after the first call (a CPU without FMA3), the test prints the INFO line `CHAR FMA3 independence: unexercised on this host` and asserts only the second run, so no evidence is claimed for that host |
| `Test_CharRagdoll_IndependentOfCrtFma3Path` | `ZenithAuthoring_Tests` (Windows) | the same two runs, with the same guard and INFO rule, of the internal `Zenith_CharComputeRagdollGeometry` ("Default ragdoll generation" of `Zenith2/Docs/Design/CHAR/Rigging.md`), whose unrounded binary64 part offsets, quaternions and Euler angles are compared byte for byte, and of `Zenith_CharBuildDefaultRagdoll`'s documents, for the four named fixture styles |
| `Test_CharDeterminism_WritesDigests` | `ZenithAuthoring_Tests` | writes `${ZENITH_OUT}/test-artifacts/CHAR/determinism-digests.json` (`{ "schemaVersion": 1, "build": "<build ID>", "digests": { "<case>": "<64 lowercase hex SHA-256>" } }`, one case per signal's binary64 frame track and stored track and one per default ragdoll document) and asserts every case ran; the comparison of this file from two reference hosts of one build (different CPU vendors, one without FMA3 support) is a Stage 4 acceptance item (`needs-human`), because no digest is derivable by hand (GOLDEN-FILES) |

What these prove: the CTest scan proves that no CHAR source names a CRT transcendental; the FMA3 tests prove, on an FMA3-capable host, that no hidden CRT call (through a third-party header or a helper) reaches the binary64 intermediates, at the precision where the CRT's paths differ (their last binary64 bit); the digests compare whole outputs across hosts. Core-doc item X362 asks for one programme-wide `ForbiddenCheck` rule enforcing [Z2-N-ARCH-888] (superseding X360 (1)); the gate is a protected path, so that rule is a PROC `needs-human` change and never a worker obligation.

Input: a `Zenith_DecodedAudio` from CONTENT's decoder (`Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_MediaDecoders header"`): sample rate R in [8,000, 192,000] Hz, C in [1, 8] channels, interleaved float samples, N_m frames. Steps:

1. **Downmix.** `m[n] = (Σ_{c=0}^{C−1} x[n × C + c]) / C`, channels in order.
2. **Anti-alias and resample to 16,000 Hz.** When R = 16,000, `y = m` (the identity case: no filter). Otherwise:
   - the cut-off is `f_c = min(7,600, 0.475 × R)` Hz, which lies below the source Nyquist frequency for every R in range;
   - the 31-tap FIR is `h[k] = s(k) × (0.54 − 0.46 × Zenith_Cos(2 × dPI × k / 30))` for k = 0 … 30, with `s(k) = sinc(2 × f_c / R × (k − 15))`, `sinc(0) = 1` and `sinc(x) = Zenith_Sin(dPI × x) / (dPI × x)`; each `h[k]` is then divided by `Σ_{k=0}^{30} h[k]` (accumulated in ascending k), so the taps sum to 1;
   - `f[n] = Σ_{k=0}^{30} h[k] × m[n + k − 15]` for n = 0 … N_m − 1, with `m` zero outside 0 … N_m − 1, and `f` zero outside 0 … N_m − 1;
   - `y[i] = f[j] × (1 − u) + f[j + 1] × u` for `t = i × R / 16,000`, `j = floor(t)`, `u = t − j`, `i = 0 … floor((N_m − 1) × 16,000 / R)`; at `j = N_m − 1` the term `f[j + 1]` reads the zero extension.
3. **Lengths.** `N_y = floor((N_m − 1) × 16,000 / R) + 1` (`N_y = N_m` in the identity case); the source duration is `d = N_m / R` seconds, used for key placement and the window layout.

Complexity O(N_m × 31 + N_y).

The internal header `Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharLipSyncEstimator.h` exposes the stages to CHAR's own tests (an `Internal/` header is private to its library and its tests, `Zenith2/CLAUDE.md` "Layout"); `Zenith_CharEstimateVisemes` is exactly `Zenith_CharEstimatorAnalyse`, then `Zenith_CharEstimatorBuildFrameTrack`, then `Zenith_CharReduceTrack`:

```cpp
// Zenith2/Tools/Authoring/Internal/CHAR/Zenith_CharLipSyncEstimator.h (internal)
struct Zenith_CharFrameFeatures
{
	double m_dTimeSeconds = 0.0;              // t_i of "Estimator framing and spectrum", before key placement
	double m_dEnergyDb = 0.0;                 // E
	double m_adBandRatio[5] = {};             // r_0 … r_4
	double m_dCentroidHz = 0.0;               // Z
	double m_dFlatness = 0.0;                 // F
	double m_dZeroCrossingRate = 0.0;         // ZCR
	Zenith_CharVisemeID m_eClass = CHAR_VISEME_SIL;   // after the plosive pass
};
Zenith_Result<void, Zenith_CharError> Zenith_CharEstimatorAnalyse(const Zenith_DecodedAudio& xAudio,
	const Zenith_CharLipSyncSettings& xSettings, Zenith_Array<Zenith_CharFrameFeatures>& axFramesOut, double& dDurationOut,
	Zenith_Arena& xScratch);                       // steps of "Estimator input" … "Estimator classification"
struct Zenith_CharFrameTrack64                // the binary64 track of steps 1–4, before reduction and storage
{
	double m_dDurationSeconds = 0.0;
	Zenith_Array<double> m_adTimes;           // shared by all sixteen curves
	Zenith_Array<double> m_adValues[uCHAR_VISEME_SET_CURVES];   // one value per time, per curve
};
Zenith_Result<void, Zenith_CharError> Zenith_CharEstimatorBuildFrameTrack(Zenith_Span<const Zenith_CharFrameFeatures> axFrames,
	double dDuration, const Zenith_CharLipSyncSettings& xSettings, Zenith_CharFrameTrack64& xOut);   // steps 1–4 below
// Step 5: RDP in binary64 per curve, then each kept value rounded once to f32 into the stored track.
Zenith_Result<void, Zenith_CharError> Zenith_CharReduceTrack(const Zenith_CharFrameTrack64& xIn, double dEpsilon,
	Zenith_CharVisemeTrack& xOut);
```

## Estimator framing and spectrum

- Hop `H` = 160 samples (10 ms), window `N` = 512 (32 ms), frame count `I = ceil(N_y / H)`; frame i covers `y[i × H … i × H + N − 1]`, zero past the end; its time is `t_i = (i × H + N / 2) / 16,000 − leadSeconds`, not clamped: step 4 of "Estimator smoothing and normalisation" decides which frames become keys.
- Window `w[n] = 0.5 − 0.5 × Zenith_Cos(2 × dPI × n / N)`; windowed frame `z[n] = w[n] × y[i × H + n]`.
- Spectrum: the radix-2 decimation-in-time FFT of `z` (512 points, iterative, bit-reversal then butterflies in ascending stage and index order), with the twiddles `W_k = Zenith_Cos(2 × dPI × k / 512) − i × Zenith_Sin(2 × dPI × k / 512)` for k = 0 … 255, computed once per call; power `P[k] = Re² + Im²` for k = 0 … 256; bin k has frequency `k × 31.25` Hz. `Zenith_CharFft512(const double adIn[512], double adReOut[512], double adImOut[512])` (`Zenith2/Docs/Design/CHAR.md → "Lip-sync API"`) is in `Internal/CHAR/Zenith_CharFft.cpp`; `Test_CharFft_MatchesDirectDft` compares it with an O(N²) DFT whose twiddles are the same CORE functions, to 1e-9 relative, on five generated signals.

## Estimator features

Per frame (bin index sets are explicit, so no boundary depends on a floating-point comparison):

| Feature | Definition |
|---|---|
| energy `E` (dB) | exactly −120 when `Σ_n z[n]² = 0` (digital silence); otherwise `10 × Zenith_Log10(Σ_n z[n]² / Σ_n w[n]² + 1e-12)` |
| bands `B_0 … B_4` | `Σ P[k]` over k = 3 … 9 (93.75 … 281.25 Hz, the range [80, 300)), k = 10 … 31 ([300, 1,000)), k = 32 … 79 ([1,000, 2,500)), k = 80 … 159 ([2,500, 5,000)), k = 160 … 256 ([5,000, 8,000]) |
| ratios `r_b` | `B_b / max(Σ_b B_b, 1e-20)` |
| centroid `Z` (Hz) | `Σ_{k=3}^{256} (k × 31.25) × P[k] / max(Σ_{k=3}^{256} P[k], 1e-20)` |
| flatness `F` | `Zenith_Exp(M_log) / (M_lin + 1e-20)`, where `M_log` is the mean of `Zenith_Ln(P[k] + 1e-20)` and `M_lin` the mean of `P[k]` over the 225 bins k = 32 … 256: near 1 for noise, near 0 for a harmonic sound |
| zero-crossing rate `ZCR` | the count of n in [1, N) with `sign(y[i H + n]) ≠ sign(y[i H + n − 1])` (the sign of 0 is +; samples past the end are 0), divided by N − 1, on the unwindowed frame |

## Estimator classification

Each frame gets one class c_i by the first rule that applies, with θ = `silenceDb`:

| Rule | Condition | Class |
|---|---|---|
| C1 silence | `E < θ` | SIL |
| C2 fricative | `ZCR ≥ 0.25`, `r_3 + r_4 ≥ 0.5` and `F ≥ 0.3` | SS when `Z ≥ 4,500`; CH when `3,000 ≤ Z < 4,500`; FF otherwise |
| C3 nasal | `E < θ + 15`, `r_0 + r_1 ≥ 0.85` and `ZCR < 0.08` | NN |
| C4 vowel | otherwise | OU when `Z < 550`; OH when `550 ≤ Z < 850`; AA when `850 ≤ Z < 1,400`; E when `1,400 ≤ Z < 2,000`; IH when `Z ≥ 2,000` |

Then the plosive pass, in ascending i: when frames i − 6 … i − 1 all exist with `E < θ + 6`, frame i is not silent (`E_i ≥ θ`) and `E_i − E_{i−2} ≥ 18` dB, frames i − 2 and i − 1 become PP (the lip closure before a burst). The `E_i ≥ θ` condition keeps the pass from firing on a still-silent frame whose energy merely rose from the −120 dB floor. Classes TH, DD, KK and RR are never assigned.

`Test_CharEstimator_ClassifiesGeneratedSignals` runs `Zenith_CharEstimatorAnalyse` on the signals of `Zenith_CharMakeSignal` (`Zenith2/Docs/Design/CHAR/Tests.md → "Fixtures and generators"`, which defines each generator sample by sample), each 1.0 s at 48 kHz unless stated, θ = −45, and asserts the classes and the feature ranges below. The ranges are what this specification computes for the generators, so a failing case names the step that diverged:

| Signal | Frames and expected class | Feature ranges asserted |
|---|---|---|
| `CHAR_SIGNAL_SILENCE` | 0 … 99 SIL | E = −120 exactly (the zero-energy case of "Estimator features") |
| `CHAR_SIGNAL_VOWEL_OU` (250 and 500 Hz) | 0 … 99 OU | frames 1 … 96: Z in [370, 380] Hz, E in [−12.1, −11.9] dB, ZCR ≤ 0.07 |
| `CHAR_SIGNAL_VOWEL_AA` (700, 1,100 and 1,400 Hz) | 0 … 99 AA | frames 0 … 97: Z in [1,050, 1,080] Hz, F ≤ 0.02, `r_3 + r_4` ≤ 0.01 |
| `CHAR_SIGNAL_VOWEL_IH` (300 Hz weak, 2,500 and 3,200 Hz strong) | 0 … 99 IH, never a fricative | frames 0 … 97: ZCR in [0.37, 0.41] and `r_3 + r_4` in [0.88, 0.90] (C2's first two conditions hold), F ≤ 0.02 (C2's third fails), E in [−12.0, −11.7] dB (C3 fails), Z in [2,760, 2,780] Hz |
| `CHAR_SIGNAL_FRICATIVE` (LCG noise, one first difference) | 0 … 98 SS; frame 99 SIL (a frame two-thirds past the end: E ≈ −46.7 dB) | frames 0 … 97: Z in [5,100, 5,750] Hz, F in [0.33, 0.52], ZCR in [0.55, 0.70], `r_3 + r_4` ≥ 0.90, E in [−37.5, −34.5] dB |
| `CHAR_SIGNAL_BURST` (200 ms of zeros, then `CHAR_SIGNAL_VOWEL_AA`) | 0 … 15 SIL, 16 and 17 PP, 18 … 99 AA | E of frames 0 … 16 = −120 exactly (their samples are exactly 0 after the FIR); frame 17 E in [−52, −49] (still SIL: the burst starts at its sample 480); frame 18 E in [−18, −16]; the pass fires at i = 18 only |
| `CHAR_SIGNAL_VOWEL_AA` at 16 kHz (the identity resample) | 0 … 99 AA | frames 0 … 97: Z in [1,050, 1,080] Hz |

A twice-differenced noise does not classify as SS (its flatness is about 0.2 and its centroid about 6.2 kHz, so C2 fails and C4 gives IH), which is why the fricative generator differences once.

## Estimator smoothing and normalisation

`Zenith_CharEstimatorBuildFrameTrack` runs steps 1 to 4 into the binary64 `Zenith_CharFrameTrack64`, and `Zenith_CharReduceTrack` runs step 5 and stores the result as f32 values in the `Zenith_CharVisemeTrack`:

1. **Targets**, for every frame i = 0 … I − 1. Amplitude `a_i = clamp((E_i − θ) / 30, 0, 1)`. For a frame of class c ≠ SIL: target `x_c = a_i`, `x_sil = 1 − a_i`, every other viseme 0; for SIL: `x_sil = 1`, others 0. Energy target `x_energy = a_i`.
2. **Smoothing**, for every frame in ascending i. Per curve, `y_i = y_{i−1} + (x_i − y_{i−1}) × k_τ`, with `k_τ = 1 − Zenith_Exp(−0.01 / τ)` (computed once per call for each of the two τ), `τ = attackSeconds` when `x_i > y_{i−1}` and `releaseSeconds` otherwise; `y_{−1}` is the rest (sil 1, others 0, energy 0).
3. **Normalisation**, per frame. `s = Σ_{v ≠ sil} y_v` (ascending viseme value); when `s > 1`, every non-sil weight is divided by s; then `y_sil = max(0, 1 − Σ_{v ≠ sil} y_v)`. The energy curve is not normalised.
4. **Key placement.** Frame i becomes a key at time `t_i` on every curve when `0 < t_i < d`; a frame with `t_i ≤ 0` or `t_i ≥ d` becomes no key, although its values entered the smoothing of the frames after it. Every curve's first key is a rest key at time 0 and its last a rest key at time d (rest: sil 1, the other visemes 0, energy 0). Kept frames have strictly increasing times, because `t_i` increases with i; should two keys ever share a time, the one of the higher frame index is kept. At the default lead of 0.04 s, frames 0, 1 and 2 (t = −0.024, −0.014, −0.004) become no key; at lead 0 the last frame of a 1 s source (frame 99, t = 1.006) becomes no key.
5. **Key reduction and storage.** Each curve's binary64 key sequence, the two rest keys included, is reduced by Ramer–Douglas–Peucker with tolerance `reduceEpsilon` in value units, in binary64, and each kept value is then rounded once to f32 (times stay binary64): recursive on the index range, splitting at the interior key of largest absolute vertical distance to the chord (ties to the lower index), always keeping both end keys; a curve whose every key lies within `reduceEpsilon` of its first value keeps its first key alone. O(K log K) typical, O(K²) worst, for K keys.
6. **Invariants.** On the binary64 frame track every key time is shared by all sixteen curves, and at each the visemes satisfy `Σ_{v ≠ sil} w_v ≤ 1 + 1e-12` (after the division by s > 1 the binary64 sum may exceed 1 by an ulp) and `|w_sil + Σ_{v ≠ sil} w_v − 1| ≤ 1e-12` (the rest keys exactly). After step 5 each reduced viseme curve, before its values are rounded, differs from its unreduced polyline by at most `reduceEpsilon` at every unreduced key time (the RDP guarantee); both are linear between consecutive unreduced key times, so the difference is at most `reduceEpsilon` everywhere in [0, d]; the unreduced curves sum to 1 at every shared key time and hence everywhere. Rounding a value in [0, 1] to f32 moves it by at most 2⁻²⁵, and linear interpolation of rounded keys by no more; summing the fifteen viseme curves gives `|Σ_v w_v(t) − 1| ≤ 15 × reduceEpsilon + 15 × 2⁻²⁵ + 1e-12 < 15 × reduceEpsilon + 1e-6` for every t in [0, d], in particular at every key time of every curve, and at `reduceEpsilon` 0 too.

| Test | Asserts |
|---|---|
| `Test_CharEstimator_KeyTimeDomain` | on `CHAR_SIGNAL_VOWEL_AA` (1.0 s at 48 kHz: N_y = 16,000, I = 100, d = 1.0) through `Zenith_CharEstimatorBuildFrameTrack` (the binary64 frame track): at `leadSeconds` 0 the first frame key is frame 0 at 0.016 s and the last frame 98 at 0.996 s (frame 99 at 1.006 s is dropped); at 0.04 frames 0 … 2 are dropped, the first frame key is frame 3 at 0.006 s and the last frame 99 at 0.966 s; at 0.2 frames 0 … 18 are dropped (frame 18 at −0.004 s), the first frame key is frame 19 at 0.006 s and the last frame 99 at 0.806 s; in every case every curve's times strictly increase, its first key is a rest key at 0 and its last a rest key at 1.0 |
| `Test_CharEstimator_WeightsSumToOneBeforeReduction` | for every signal of "Estimator classification", at every key of the binary64 `Zenith_CharFrameTrack64`: `Σ_{v ≠ sil} w_v ≤ 1 + 1e-12` and `|Σ_v w_v − 1| ≤ 1e-12` |
| `Test_CharEstimator_WeightsSumWithinReductionTolerance` | for every signal, after `Zenith_CharReduceTrack` at 0.002 and at 0: at every key time of every viseme curve of the stored f32 track, all fifteen curves sampled by linear interpolation of their own keys sum to 1 within `15 × reduceEpsilon + 1e-6` |
| `Test_CharEstimator_KeyReductionWithinEpsilon` | linear interpolation of each stored curve reproduces every key of the binary64 frame track within `reduceEpsilon + 6e-8` (one f32 rounding of a value in [0, 1]) |
| `Test_CharEstimator_DeterministicOutput` | two runs, and a run on a copy of the input, give byte-identical tracks |

## Phrase markers

For each phrase start p (seconds) of the entry: every key of every curve in `[p − 0.05, p)` is replaced, and keys at `p − 0.05` and at p are inserted, with the rest values (sil 1, others and energy 0), so the mouth closes before each phrase. Applied after reduction; insertion keeps times strictly increasing (a key already at the time is replaced); a phrase start below 0.05 inserts its first key at 0 instead of `p − 0.05`. `Test_CharEstimator_PhraseStartsForceRest` asserts it.

## External viseme import

`source: "Import"` reads a viseme timing file in the format `zenith2.visemes`, version 1, produced by any external tool and committed beside the audio:

```json
{ "$schema": "zenith2.visemes", "schemaVersion": 1, "durationSeconds": 2.31,
  "cues": [ { "t": 0.00, "v": "sil" }, { "t": 0.12, "v": "pp" }, { "t": 0.20, "v": "aa", "w": 0.9 }, { "t": 0.41, "v": "th" } ] }
```

Cues are ascending in `t` (strictly; equal times are `CHAR_DIAG_VISEMES_TIME_ORDER`), `v` a viseme of "Viseme set" without the prefix, `w` in [0, 1] (default 1). The track duration `d_v` is the entry's audio duration when the entry names `audio`, else `durationSeconds`; when both exist and differ by more than 0.01 s, the warning `CHAR_DIAG_VISEMES_DURATION_MISMATCH` names the line, the locale and both durations, and cues at or after `d_v` are ignored. Conversion: sample at 100 Hz, sample j at time `j / 100` for j = 0 … `ceil(d_v × 100 − 1e-9) − 1`; at each sample the target is the last cue at or before the sample time (one-hot `w`, sil `1 − w`), energy `w`; then steps 2 to 6 of "Estimator smoothing and normalisation" with the sample times as `t_i` and `d = d_v`, and "Phrase markers". At most 65,536 cues; a malformed file fails the import naming the file and the key path. `Test_CharVisemeImport_ConvertsCues`, `Test_CharVisemeImport_RejectsUnorderedTimes` and `Test_CharVisemeImport_DurationFromAudio` (a 2.31 s file over 2.0 s of audio: `d_v` 2.0, the warning, no key past 2.0) cover it.

## Manual overrides

An entry's `overrides` array holds `{ "curve": "<viseme or energy name>", "start": s, "end": e, "keys": [[t, value], …] }` objects, applied in array order after the source's track is built: the curve's keys in [s, e] are removed and the override's keys (absolute times in [s, e], strictly increasing, values in [0, 1]) inserted. No renormalisation is applied; an override that leaves `Σ_{v ≠ sil} w_v > 1.05` at any key raises the warning `CHAR_DIAG_LIPSYNC_OVERRIDE_SUM`. The lip-sync panel's curve editor writes overrides through JSON edits (`Zenith2/Docs/Design/CHAR/Workflow.md → "Lip-sync panel"`). `Test_CharLipSyncOverrides_ReplaceRange` asserts the splice.

## Locale-strided lip-sync clip

<!-- elaboration -->
One clip per dialogue line carries every voice locale's track, laid out in fixed-length windows, so a line has one clip GUID whatever the locale (CINE's dialogue section names one face clip, `Zenith2/Docs/Design/CINE/Formats.md → "Cinematic document"`), and choosing the locale is a seek, not a different asset:

1. L = `locales.size()`; `d_l` = the duration of locale l's track: the audio duration for `Estimate`; for `Import`, `d_v` of "External viseme import"; 0 for `Silent`.
2. Stride `S = ceil((max_l d_l + padSeconds) × sampleRateHz − 1e-9) / sampleRateHz` seconds (the 1e-9 keeps a binary64 product that should be an integer from rounding up one sample); the clip duration is `D = L × S`. `D > 3,600` s (ANIM's clip duration limit, `Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits"`) fails the line with `CHAR_ERROR_LIMIT_EXCEEDED`.
3. Window l spans `[l × S, (l + 1) × S)`; locale l's track keys are shifted by `l × S`; a rest key is placed at `l × S` and at `e_l = l × S + d_l + 0.05`, and every key in between that the track does not cover is rest. Because `S ≥ max_l d_l + padSeconds` and `padSeconds ≥ 0.2`, `e_l ≤ (l + 1) × S − 0.15`.
4. Keys are concatenated over windows in ascending l; two windows never share a key time, because window l's last key `e_l` lies below `(l + 1) × S`, the first key of window l + 1.

`Test_CharLipSyncClip_StrideAndWindows` asserts S, D and every window's first and last key for three locales of 1.0, 1.6 and 0 s at 60 Hz with `padSeconds` 0.25 (S = ceil(1.85 × 60 − 1e-9) / 60 = 111 / 60 = 1.85, D = 5.55); `Test_CharLipSyncClip_DurationLimitRefused` asserts step 2's limit; `Test_CharLipSyncClip_StrideCeilTolerance` lays out one 0.4 s line with `padSeconds` 0.2 at 60 Hz, whose product `(0.4 + 0.2) × 60` is `36.00000000000001` in binary64 (37 samples without the tolerance), and asserts S = 0.6 (36 samples).

## Window markers and events

The product's clip annotation settings (`Zenith2/Docs/Design/ANIM/Formats.md → "Clip annotation document schema"`, written into the product's importer settings by the lip-sync importer) carry, per window l:

| Item | Name | Time | Detail |
|---|---|---|---|
| marker | `char.lipsync.<tag>` (tag = `locales[l]`) | `l × S` | the window start; a player finds it by name ("Lip-sync playback contract") |
| event | `char.lipsync.windowEnd` | `e_l = l × S + d_l + 0.05` | instant, fire mode weighted, `minWeight` 0, flags gameplay, payload l |

and `loop` false, `rootMotion` mode none, `streaming.allowed` true when `D > 20` s, compression `curveError` 0.002, `anim.sampleRateHz` = `sampleRateHz`. The markers make the clip self-describing: the set of voice locales, their order and the stride are all recoverable from the clip, so no run-time consumer needs the project document.

## Clip intermediate writer

CHAR writes no clip intermediate bytes itself: the layout of ANIM's `anim.clip` intermediate (`Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"`) is ANIM's, and so is its writer. CHAR's importers build a `Zenith_CharCurveClip` (`Zenith2/Docs/Design/CHAR.md → "Controls and pose book API"`), round its times with `Zenith_CharRoundClipTimes` (times written as f32 rounded once from the double values; in a channel whose rounded times are not strictly increasing, the second of each equal pair is dropped and counted in `CHAR_DIAG_CLIP_KEY_COLLAPSED`), convert it field for field to ANIM's descriptor, and call ANIM's writer.

<!-- assumption: pending ANIM (row CH15) -->
ANIM's ZenithAuthoring header `Zenith2/Tools/Authoring/Zenith_AuthoringAnimation.h` declares the curve-and-pose intermediate writer `Zenith_Result<void, Zenith_AnimationImportError> Zenith_WriteCurveClipIntermediate(const Zenith_AnimCurveClipDesc& xClip, Zenith_Array<uint8>& auOut)`, where `Zenith_AnimCurveClipDesc` holds the skeleton product GUID, the source duration (f32) and a span of channels `{ Zenith_StringView name; uint8 path (1 translation, 2 rotation, 3 scale, 4 curve); uint8 interpolation (1 step, 2 linear); Zenith_Span<const float> times; Zenith_Span<const float> values }` with the widths of the ZICL table, and the writer refuses times that are not strictly increasing. It is ANIM's code because `Zenith_WriteClipIntermediates` is ZenithCook-internal and takes glTF node tables (`Zenith2/Docs/Design/ANIM/Formats.md → "Import conversion"`), which a lip-sync or pose-book clip of another source's skeleton does not have. Until the writer lands, CHAR's pose-book importer and lip-sync importer tickets stay `draft`, blocked by ANIM's ticket; CHAR writes no fallback copy of the format.
<!-- /assumption -->

`Test_CharClipTimes_RoundAndCollapse` asserts the rounding and the drop count on a channel with two times 1e-9 s apart; `Test_CharClipIntermediate_AnimCookAccepts` (`ZenithCook_Tests`) builds a lip-sync clip, writes it through ANIM's writer, cooks it through ANIM's `anim.clip` step with the cook test access (ruling GEOM-N2), loads it and samples the curves at three times with `Zenith_SampleClip`, within the clip's `curveError` 0.002.

## Lip-sync importer

The importer `char.lipsync` (`Zenith2/Docs/Design/CHAR.md → "Importers and cook registration"`), extension `.lipsync.json`, settings none of its own, version 1 + `uCHAR_LIPSYNC_ESTIMATOR_VERSION` (1), `m_bOutOfProcess` true (it decodes third-party-parsed audio through ZenithContentCore, CONTENT's crash-isolation rule, `Zenith2/Docs/Design/CONTENT/Importers.md → "Crash isolation"`):

1. Parse the source document through SCENE's canonical reader with migration in memory; validate every member (the tables of "Lip-sync document"); a failure is `IMPORT_ERROR_SOURCE_INVALID` with the key path reported.
2. The importer does not read the project document: it lies outside the document's directory and `ReadInput` admits no `..`. The document's `locales` member is its own copy, written by "Sync from bank" from the project; the editor's validation compares the two on every save of either document and before every cook the editor requests (rule CV12 of "Validation catalogue" in `Zenith2/Docs/Design/CHAR/Debugging.md`).
3. For each line in ascending key order: for each locale l: `Estimate` → `ReadInput(audio)`, decode with `Zenith_DecodeAudioSource` (format by extension), estimate; `Import` → `ReadInput(visemes)` and, when `audio` is given, `ReadInput(audio)` for its duration; `Silent` → no track. Apply phrases and overrides.
4. Lay out the strided clip, declare the product `anim:lip.<line ID text>` (kind `Animation.Clip`), write its `anim.clip` intermediate through ANIM's writer ("Clip intermediate writer") and its annotation settings.
5. Report per line: the durations, S, the window count, the key counts per curve, and the warnings.

Every `ReadInput` records an import-input edge, so editing a WAV or a visemes file re-imports exactly the documents that read it (`Zenith2/Docs/Design/ASSET/Import.md → "Reimport triggers"`). A change of the project's voice locales reaches a lip-sync document only through "Sync from bank", which rewrites its `locales` and so its bytes, and through validation rule CV12, which reports a document not yet synced. The importer is a pure function of its inputs (`Test_CharLipSyncImporter_ReimportBitIdentical`). `Test_CharLipSyncImporter_ProductsPerLine` imports a two-line, two-locale document through ASSET's import test access and asserts two products, their keys and their intermediates; `Test_CharLipSyncImporter_MissingAudioFails` asserts that a missing audio file fails with the line, the locale and the path.

## Lip-sync playback contract

Every player of a lip-sync clip (CINE's dialogue sections, the flagship slices' dialogue samples, any game script) follows this contract, which `Test_CharIntegration_LipSyncPlaybackContract` exercises with a Luau test behaviour over a generated clip. The player holds the voice-locale **tag** v from simulation state ("Voice locale as a simulation input"); it needs neither the project's locale list nor the window count:

| Step | Rule |
|---|---|
| choose the window | the start time T is the time of the clip's marker `char.lipsync.<v>`; when the clip has none, the marker of v's language subtag (`fr-CA` → `char.lipsync.fr`); when neither exists, T = 0 (window 0, the project's first voice locale). This is AUDIO's own resolution order for the sound (`Zenith2/Docs/Design/AUDIO/Mixer.md → "Locale selection"`: exact tag, language subtag, then the bank's first locale entry), and rule CV18 keeps the bank's first entry equal to window 0's locale. Luau reads the marker with `Animation.GetMarkerTime(clip, name)` → seconds or nil; C++ (CINE, once row CH19 lands) with `Zenith_AnimationClip::GetMarkers` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationClip header"`), matching `Zenith_Hash64` of the name |
| start | in the same script invocation (or the same CINE evaluation) that posts the line: `Animation.PlaySlot(entity, "char.lipsync", clip, { startTime = T, blendIn = 0.05, blendOut = 0.1, loop = false, rootMotion = false, tag = 16384 })` (16384 is `ANIM_SLOT_TAG_FULL_RATE`, bit 14; `Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"`), and AUDIO's `Audio.PlayDialogue` with `uAUDIO_POST_FLAG_TIME_LOCKED` (`Zenith2/Docs/Design/AUDIO/Mixer.md → "Time-locked voices"`), so the audio follows the simulation clock the visemes play on |
| prepare | `Audio.PrepareDialogue` and `Animation.PrefetchClip(entity, clip, T)` before the start, when the clip is streamed |
| stop | `Animation.StopSlot(entity, "char.lipsync", 0.1)` on the first `Zenith.Animation.Notify` event `char.lipsync.windowEnd` of this slot play (the first one after T is window l's, because `e_l > l × S` and window l − 1's event lies before `l × S`), or when the dialogue is interrupted (with `Audio.StopInstance` of the line) |
| never | play without a start time, loop the clip, or let the slot's clip time reach `(l + 1) × S`, the start of the next window |

<!-- elaboration -->
**Why the stop never crosses the window.** Let Δt = 1 / `sim.tickHz`. The event fires in the evaluation of the tick n whose advance crosses `e_l`, so the clip time after tick n is below `e_l + Δt`. The script's `StopSlot` is recorded after phase 3 of tick n and reaches the evaluation of tick n + 1 ([Z2-N-ARCH-489]: commands of phases 3 to 10 reach the next tick's evaluation), where the 0.1 s blend-out starts; the slot keeps sampling its clip while its weight is above 0, which is at most `ceil(0.1 / Δt) − 1` further advances, so the last clip time read is below `e_l + Δt + (ceil(0.1 / Δt) − 1) × Δt ≤ e_l + 0.1 + Δt`. With `e_l ≤ l × S + max_l d_l + 0.05` and `(l + 1) × S ≥ l × S + max_l d_l + padSeconds`, the window holds whenever `padSeconds ≥ 0.05 + 0.1 + Δt`: 0.1834 at 30 Hz and less at every higher tick rate, hence the document minimum 0.2. The bound needs the character evaluated every tick while the slot plays: an instance at rate divisor k advances k × Δt per evaluation, so the slot play carries `ANIM_SLOT_TAG_FULL_RATE`, which forces k = 1 while the slot is active as the cinematic tag does (assumption pending ANIM, row CH16).
<!-- /elaboration -->

<!-- assumption: pending CINE (row CH19) -->
CINE's text today plays a dialogue section's face clip as an animation section with the section's start and duration (`Zenith2/Docs/Design/CINE/Playback.md → "Dialogue tracks"`). Until row CH19 lands, cinematic faces therefore play window 0: a voice locale other than `voiceLocales[0]` shows window 0's mouth under its own voice, and a section longer than S reads into window 1, so CINE sections that carry lip-sync clips must not exceed S. CHAR's CinematicSlice lip-sync sign-off item stays blocked by CH19.
<!-- /assumption -->

<!-- assumption: pending ANIM (row CH16) -->
ANIM's Luau table gains `Animation.GetMarkerTime(clip, name)` → the marker's time in seconds, or nil when the bound clip has no marker of that name (pure on the binding cache, like `GetClipLength`), and ANIM's slot tag gains bit 14, `ANIM_SLOT_TAG_FULL_RATE`, which forces rate divisor 1 while the slot is active, as bit 15 (`ANIM_SLOT_TAG_CINEMATIC`) already does in "Rate tiers and phase offsets" of `Zenith2/Docs/Design/ANIM/Runtime.md`; the Luau `PlaySlot` options take `tag`. Until both land, CHAR's playback-contract integration case and the walkthrough's dialogue behaviour stay `draft`.
<!-- /assumption -->

`Test_CharIntegration_LipSyncPlaybackContract` plays a generated three-locale clip (`padSeconds` 0.2, the minimum, at 30 Hz) at the window of tag `fr` (index 1), stops on the first `char.lipsync.windowEnd` (payload 1), asserts every tick that the slot's clip time (`GetDebugState`, slot time) stays below 2 × S, asserts the curves equal window 1's keys within 0.002 (the clip's curve error) at five times, and repeats with the tag `fr-CA` (the language-subtag fallback: window 1) and the tag `de` absent from the clip (window 0).

## Voice locale as a simulation input

Viseme curves are part of ANIM's evaluated pose, which is simulation state (saved EXACT, `Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"`), so the window a character plays must not depend on a presentation setting that a replay does not record. AUDIO's `audio.dialogue.locale` is presentation (not simulation-affecting, `Zenith2/Docs/Design/AUDIO/PublicAPI.md → "Configuration keys header"`). The orchestrator's ruling CHAR-R1 settles the model:

1. **Simulation state.** The voice-locale tag v is game simulation state: a field of a game component written only by commands (data-defined component authority `CommandsOnly`, `Zenith2/Docs/Architecture/DataDefinedComponents.md → "Top-level keys"`), owned by the game's settings script (game-side, Z2-D-077; cross-area row CH18).
2. **One entry point.** v changes only through the game's voice-locale command, which enters the simulation at a tick boundary and is recorded in the replay log with the tick's inputs; nothing else writes it, and no simulation code reads `audio.dialogue.locale`, `ui.locale` or the platform locale.
3. **Issuing it.** When the user changes the dialogue language, the settings script writes AUDIO's `audio.dialogue.locale` user value and issues the voice-locale command with the effective tag AUDIO will use (AUDIO's source order: `audio.dialogue.locale`, else `ui.locale`, else the platform's preferred locale), so the sound AUDIO selects and the window ANIM plays agree.
4. **Saves.** A save stores v, the locale in effect, as part of the component (it is simulation state like any other field).
5. **Load.** After a save restore, at the restored session's first tick, the settings script compares the restored v with the effective tag of the user's current settings; when they differ, it issues the voice-locale command with the current tag, which is recorded like any other, so lips and voice agree from the first line played after the load. A replay never consults the user's settings: it applies the commands it recorded, the post-load command included.
6. **Consumers.** Every lip-sync play and every cinematic start reads v from simulation state (rows CH18 for GAME and CH19 for CINE).

<!-- assumption: pending GAME (row CH18) -->
The voice-locale command is GAME's: a game-defined command carrying the tag, posted by the settings script, recorded in the replay log's tick block and applied at the next tick boundary, as PLAT's recorded platform events are (`Platform.LocaleChanged`, `Zenith2/Docs/Design/PLAT/SimulationInput.md → "Script-facing input API"`); row CH18 asks GAME to provide that path for game settings and to apply rules 1 to 6 in its settings script. CHAR's integration cases use the suite's test implementation of the same rules, written in `Zenith2/Tests/Character/Scripts/VoiceLocaleSettings.luau` against GAME's path.
<!-- /assumption -->

| Test | Asserts |
|---|---|
| `Test_CharIntegration_LipSyncLocaleIsSimulationInput` | a replay recorded with v = `fr` replays under `audio.dialogue.locale` = `en` with identical per-tick world-state hashes, and the face plays window 1 |
| `Test_CharIntegration_LipSyncLocaleAfterRestore` | save with v = `en` (index 0); restore with the user setting `fr`; the first restored tick issues the voice-locale command (counted once); the next line plays window 1 (slot start time S); a replay of the restored session, run with the user setting `de`, plays window 1 too and matches the live run's hashes |
| `Test_CharIntegration_LipSyncLocaleUnchangedAfterRestore` | save and restore with the same setting: no command is issued at the first tick |

## Facial and lip-sync limits

| Quantity | Limit | Error |
|---|---|---|
| voice locales | 16 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| lines per lip-sync document | 4,096 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| line ID text | 96 bytes | `CHAR_DIAG_LIPSYNC_LINE_ID` |
| overrides per entry | 256 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| keys per override | 65,536 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| cues per visemes file | 65,536 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| phrases per entry | 1,024 | `CHAR_ERROR_LIMIT_EXCEEDED` |
| audio duration per entry | 600 s | `CHAR_ERROR_LIMIT_EXCEEDED` |
| clip duration L × S | 3,600 s (ANIM's) | `CHAR_ERROR_LIMIT_EXCEEDED` |
| `padSeconds` | [0.2, 1] | `CHAR_DIAG_DOCUMENT_INVALID` (the document reader) |
| document bytes | 64 MiB | `DOCUMENT_ERROR_TOO_LARGE` |

Each is tested at the limit and at limit + 1 by `Test_CharLipSyncLimits_AtAndAboveLimit` (for `padSeconds`, 0.2 accepted and 0.19 refused); the document, visemes and pose book readers are fuzzed (`Zenith2/Docs/Design/CHAR/Tests.md → "Fuzz targets"`).
