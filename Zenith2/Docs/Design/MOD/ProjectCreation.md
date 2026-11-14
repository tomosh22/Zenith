# MOD design — Project creation

This file is part of the MOD design family. The entry document is `Zenith2/Docs/Design/MOD.md`; this file specifies how a new game directory is created without C++: the `ZenithNewGame` command line, the `ZenithNewGameScaffold` library that the command line and the editor's new-project wizard share, the templates, the creation algorithm, the C++ `game.json` validator, `game.json` migration, and the boundary with the editor wizard. It elaborates Zenith2/Docs/Architecture.md → "Engine modules and game projects" ("Creating a game") and never relaxes it. Every obligation traces to a `Z2-R-MOD-NNN` in `Zenith2/Docs/Requirements/MOD.md`.

## ZenithNewGame command line

### Synopsis

```text
ZenithNewGame create <Name> [--template <Blank|3D|2D>] [--modules <A,B,…>] [--ecs <Custom|Flecs|EnTT>]
                            [--physics <Jolt|PhysX>] [--platforms <windows,android>] [--application-id <id>]
                            [--root <Zenith2 directory>] [--dry-run]
ZenithNewGame validate <Name> [--root <Zenith2 directory>]
ZenithNewGame --migrate <Name> [--root <Zenith2 directory>] [--preview]
ZenithNewGame --help
```

The first three forms and their arguments are the synopsis of Zenith2/Docs/Architecture.md → "Engine modules and game projects"; `--root`, `--dry-run`, `--preview` and `--help` are elaborations.

<!-- elaboration -->
| Option | Default | Rule |
|---|---|---|
| `<Name>` | — (required) | `^[A-Z][A-Za-z0-9]{0,31}$`; not a Windows device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`, case-insensitive); does not start with `Zenith`; is not the name of an engine library, backend, module, tool or existing game |
| `--template` | `Blank` | `Blank`, `3D` or `2D` (§ "Templates") |
| `--modules` | the template's modules | comma-separated `MODULE_NAME`s, no spaces; replaces (never extends) the template's list; each must be one of the thirteen of Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"; dependencies are not added (the validator reports a missing one, exactly as `zenith_add_game` would); the scaffold checks names against its compiled table only, so a name whose library has not landed yet (for example `Physics` before Stage 3) creates the game and its configure then fails with the unknown-module message, which is intended |
| `--ecs` | `Custom` | `Custom`, `Flecs` or `EnTT` |
| `--physics` | `Jolt` when `Physics` is in the resulting module list; otherwise refused | `Jolt` or `PhysX`; the same backend for every platform (the object form is authored by editing `game.json`) |
| `--platforms` | `windows,android` | comma-separated, unique, each `windows` or `android` |
| `--application-id` | `com.zenith2.<name lower-cased>` | the `android.applicationId` rule; refused when `android` is not a platform |
| `--root` | the nearest ancestor of the current directory (the current directory included) that contains `CMakePresets.json`, `Engine/` and `Games/`, or such a directory's `Zenith2/` child | an existing Zenith2 root with those three entries; for `validate` alone, any directory holding `Games/<Name>/` (the form the build step and the validator fixtures use) |
| `--dry-run` | off | print the plan (every file path, its byte size and its SHA-256) and write nothing |
| `--preview` | off | with `--migrate`: print the upgrade diff and write nothing |

The engine target names that `<Name>` may not equal are read from the committed tree, not from a build: every directory name under `<root>/Engine/` and `<root>/Tools/`, and every tool target name, which the scaffold lists as a constant table (`ZenithEditor`, `ZenithCook`, `ZenithShaderCompiler`, `ZenithNewGame`, `ZenithNewGameScaffold`, `ZenithPathTracer`). The `Zenith` prefix rule already covers all of them; the explicit check yields the more specific message.

### Exit codes

<!-- elaboration -->
| Code | Meaning |
|---|---|
| 0 | success (`create` wrote the game or `--dry-run` printed its plan; `validate` found no error; `--migrate` wrote, previewed, or found the file already current) |
| 1 | an I/O failure while writing (disk full, access denied); nothing is left behind (§ "Scaffold algorithm") |
| 2 | a usage or name error, or `create` refused an existing directory ([Z2-N-ARCH-273]) |
| 3 | `validate` or `--migrate` found validation errors in the game |

### Output

The command-line refusals below go to standard error prefixed `ZenithNewGame: `; `--dry-run` and `--preview` write their plan or diff to standard output. Messages for the refusals of [Z2-N-ARCH-273]:

| Refusal | Message |
|---|---|
| bad name | `ZenithNewGame: '<Name>' must match ^[A-Z][A-Za-z0-9]{0,31}$` |
| device name | `ZenithNewGame: '<Name>' is a reserved Windows device name` |
| reserved prefix | `ZenithNewGame: '<Name>' starts with the reserved prefix Zenith` |
| engine or tool name | `ZenithNewGame: '<Name>' is the name of an engine library or tool` |
| existing directory | `ZenithNewGame: Games/<Name> already exists; nothing was written` |
| interrupted creation | `ZenithNewGame: Games/<Name> holds an interrupted creation (.newgame-incomplete); delete the directory and run create again` |
| no root | `ZenithNewGame: no Zenith2 root found above <cwd>; pass --root` |
| option conflict | `ZenithNewGame: --physics needs Physics in the module list`, `ZenithNewGame: --application-id needs android in --platforms` |

`validate` has one output form, which the `Validator/` fixtures and `<Name>_Validate`'s build failure both show: each `game.json` and game-directory message, unprefixed, exactly as Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages" spells it, one per line, to standard error; then each schema validator message (Zenith2/Docs/Architecture/DataDefinedComponents.md → "Registration at game load", "Validation errors") in its own `<file>: <keyPath>: <message>` form, one per line, to standard error, the files in byte order of path and, within a file, in the order `Zenith_TypeRegistry::RegisterSchemaJson` reports them; on a clean run, the trailer `schemas: collisions with engine and module types are checked by the cook` to standard output, which no fixture compares. A `Validator/` fixture's `expect.txt` holds the exit code on its first line and the standard-error lines on the rest.

## ZenithNewGameScaffold library API

### Targets and files

| Path | Contents |
|---|---|
| `Zenith2/Tools/NewGameScaffold/CMakeLists.txt` | `zenith_add_tool(ZenithNewGameScaffold DEPENDS ZenithCore ZenithPlatform ZenithReflect)` (a static library: the directory has no `Internal/Zenith_ZenithNewGameScaffoldMain.cpp`) |
| `Zenith2/Tools/NewGameScaffold/Zenith_NewGameScaffold.h` | the public API below |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_NewGameScaffold.cpp` | planning and applying |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_NewGameTemplates.cpp` | the template texts as raw string literals (§ "Templates") |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_GameJsonValidator.cpp` | the C++ validator (§ "Validation in C++") |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_GameJsonMigration.cpp` | the migration chain (§ "game.json migration") |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_CanonicalJson.cpp` | the canonical `game.json` writer |
| `Zenith2/Tools/NewGameScaffold/Internal/Zenith_GameName.cpp` | `Zenith_IsValidGameName`, the bad-name rule shared with the editor |
| `Zenith2/Tools/NewGameScaffold/Tests/Zenith_NewGameScaffold_Tests.cpp` | § "Project creation tests" |
| `Zenith2/Tools/NewGameScaffold/Tests/Zenith_GameJsonValidator_Tests.cpp` | the validator parity test |
| `Zenith2/Tools/NewGame/CMakeLists.txt` | `zenith_add_tool(ZenithNewGame DEPENDS ZenithNewGameScaffold ZenithCore ZenithPlatform ZenithReflect)` |
| `Zenith2/Tools/NewGame/Internal/Zenith_ZenithNewGameMain.cpp` | `main`: root heap installation, argument parsing, calls into the scaffold, exit code |
| `Zenith2/Tests/NewGame/CMakeLists.txt` | `zenith_add_integration_tests(NewGame DEPENDS ZenithNewGameScaffold ZenithCore ZenithPlatform ZenithReflect SUPPORTED ZENITH_TOOLS_HOST)` |
| `Zenith2/Tests/NewGame/Zenith_NewGame_Tests.cpp` | `Test_NewGame_CreatedGameBoots`, `Test_NewGame_Cli_ExitCodes` |

<!-- elaboration -->
Zenith2/Docs/Architecture.md → "Library dependency graph" names the library `ZenithNewGameScaffold` and the tool `ZenithNewGame` with the dependencies `ZenithCore`, `ZenithPlatform` and `ZenithReflect` (§ "Validation in C++"). One directory is one target (Zenith2/Docs/Architecture/BuildSystem.md → "Tools, benchmarks and integration suites"), so the library needs its own directory, `Tools/NewGameScaffold/`, beside `Tools/NewGame/`; the editor links the library, never the executable. Both are tooling targets (Zenith2/Docs/Contracts.md → "Purpose and scope", "Target classes"), so they may use the standard library in their own sources, with exceptions disabled (the `std::error_code` overloads of `<filesystem>`), and pass standard types only between tooling targets, never across a runtime library's API ([Z2-N-CONTRACT-275], [Z2-N-CONTRACT-276]).

### Public API

```cpp
// Zenith2/Tools/NewGameScaffold/Zenith_NewGameScaffold.h
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "ZenithCore/Zenith_StringView.h"

/// @contract enum Zenith_NewGameTemplate
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: None; parsing a template name is Zenith_ParseNewGameTemplate's job.
/// Version: since Z2 Stage 2; a new template appends an enumerator.
/// @endcontract
enum Zenith_NewGameTemplate : uint8
{
	NEW_GAME_TEMPLATE_BLANK = 1,
	NEW_GAME_TEMPLATE_3D,
	NEW_GAME_TEMPLATE_2D,
};

/// @contract enum Zenith_NewGameError
/// Ownership: A plain value.
/// Thread affinity: IMMUTABLE. A value type.
/// Invalidation: None.
/// Units: None.
/// Errors: NEW_GAME_ERROR_BAD_NAME, _RESERVED_NAME, _ENGINE_NAME, _EXISTS, _INCOMPLETE, _NO_ROOT, _OPTION_CONFLICT
///   and _BAD_OPTION map to exit code 2; _IO maps to 1; _VALIDATION maps to 3.
/// Version: since Z2 Stage 2; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_NewGameError : uint8
{
	NEW_GAME_ERROR_BAD_NAME = 1,      // fails ^[A-Z][A-Za-z0-9]{0,31}$
	NEW_GAME_ERROR_RESERVED_NAME,     // Windows device name, or the Zenith prefix
	NEW_GAME_ERROR_ENGINE_NAME,       // an engine library, backend, module or tool name
	NEW_GAME_ERROR_EXISTS,            // Games/<Name> exists
	NEW_GAME_ERROR_INCOMPLETE,        // Games/<Name> holds .newgame-incomplete
	NEW_GAME_ERROR_NO_ROOT,           // no Zenith2 root found or given
	NEW_GAME_ERROR_BAD_OPTION,        // an option value outside its allowed set
	NEW_GAME_ERROR_OPTION_CONFLICT,   // --physics without Physics, --application-id without android
	NEW_GAME_ERROR_IO,                // a filesystem operation failed; the partial result was removed
	NEW_GAME_ERROR_VALIDATION,        // the game's game.json or schemas failed validation
};

constexpr const char* Zenith_ToString(Zenith_NewGameError eError);

/// @contract function Zenith_IsValidGameName
/// Ownership: Reads strName only; keeps nothing.
/// Thread affinity: ANY_THREAD. A pure function.
/// Invalidation: None.
/// Units: Bytes of UTF-8; the rule admits ASCII alone.
/// Errors: None; false for every name the rule refuses, the empty name included.
/// Version: since Z2 Stage 2.
/// @endcontract
// The bad-name rule of "ZenithNewGame command line" alone, ^[A-Z][A-Za-z0-9]{0,31}$: what Zenith_PlanNewGame reports as
// NEW_GAME_ERROR_BAD_NAME. The device-name, Zenith-prefix, engine-name and existing-game rules are Zenith_PlanNewGame's.
bool Zenith_IsValidGameName(Zenith_StringView strName);

/// @contract type Zenith_NewGameDesc
/// Ownership: Owned by the caller; the scaffold copies what it needs.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. The caller owns every instance.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Zenith_PlanNewGame; an invalid field is reported there, never asserted.
/// Version: since Z2 Stage 2; a new option appends a member with a default that reproduces the old output.
/// @endcontract
struct Zenith_NewGameDesc
{
	std::string m_strName;
	Zenith_NewGameTemplate m_eTemplate = NEW_GAME_TEMPLATE_BLANK;
	bool m_bModulesGiven = false;                     // false: the template's modules
	std::vector<std::string> m_astrModules;
	std::string m_strECS = "Custom";
	std::string m_strPhysics;                         // empty: Jolt when Physics is selected
	std::vector<std::string> m_astrPlatforms = { "windows", "android" };
	std::string m_strApplicationID;                   // empty: com.zenith2.<name lower-cased>
	std::filesystem::path m_xRoot;                    // the Zenith2 directory
};

/// @contract type Zenith_NewGamePlan
/// Ownership: Returned by value from Zenith_PlanNewGame; owned by the caller.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. The caller owns every instance.
/// Invalidation: None; a plan is a snapshot and does not observe the filesystem after it is made.
/// Units: Byte sizes in bytes.
/// Errors: None; a plan exists only when planning succeeded.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_NewGamePlan
{
	struct Zenith_NewGamePlanFile
	{
		std::string m_strRelativePath;                // relative to Games/<Name>/, '/' separators
		std::string m_strBytes;                       // the exact file content, LF line endings
	};
	std::string m_strName;
	std::filesystem::path m_xGameDirectory;           // <root>/Games/<Name>
	std::vector<Zenith_NewGamePlanFile> m_axFiles;                      // byte order of m_strRelativePath; CMakeLists.txt is written last
};

/// @contract type Zenith_NewGameReport
/// Ownership: Owned by the caller; filled by the scaffold functions.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. The caller owns every instance.
/// Invalidation: None.
/// Units: None.
/// Errors: Carries the messages behind a returned error, in the formats of "ZenithNewGame command line".
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_NewGameReport
{
	std::vector<std::string> m_astrMessages;
};

/// @contract functions NewGameScaffold
/// Ownership: Every function returns values the caller owns; none keeps state between calls.
/// Thread affinity: ANY_THREAD. The functions share no state; two calls writing the same game directory race on the filesystem.
/// Invalidation: None.
/// Units: None.
/// Errors: The Zenith_NewGameError enumeration; detail in the report.
/// Version: since Z2 Stage 2 (Zenith_MigrateGameJson's preview mode since Z2 Stage 2, the wizard's use since Z2 Stage 4).
/// @endcontract
Zenith_NewGameError Zenith_FindZenith2Root(const std::filesystem::path& xStart, std::filesystem::path& xRootOut);
Zenith_NewGameError Zenith_PlanNewGame(const Zenith_NewGameDesc& xDesc, Zenith_NewGamePlan& xPlanOut,
	Zenith_NewGameReport& xReportOut);
Zenith_NewGameError Zenith_ApplyNewGamePlan(const Zenith_NewGamePlan& xPlan, Zenith_NewGameReport& xReportOut);
Zenith_NewGameError Zenith_ValidateGame(const std::filesystem::path& xRoot, const std::string& strName,
	Zenith_NewGameReport& xReportOut);
Zenith_NewGameError Zenith_ValidateGameJsonText(const std::string& strGameName, const std::string& strJsonText,
	const std::filesystem::path& xGameDirectory, Zenith_NewGameReport& xReportOut);
Zenith_NewGameError Zenith_MigrateGameJson(const std::filesystem::path& xRoot, const std::string& strName,
	bool bPreviewOnly, Zenith_NewGameReport& xReportOut);
```

<!-- elaboration -->
`Zenith_IsValidGameName` is the one copy of the bad-name rule: `Zenith_PlanNewGame` applies it as the first name check of step 2 of § "Scaffold algorithm", and the editor validates its `--game` argument with it (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Executable and command line", row ED38), so the two never hold separate copies. It takes `Zenith_StringView` because its callers hold command-line text, not `std::string`. The functions return the error enumeration directly rather than a `Zenith_Result`, because a success carries no value beyond the out-parameters and the scaffold is tooling that never crosses a runtime API. Success is the value 0, which is not an enumerator (the rule of Zenith2/Docs/Contracts.md → "Errors and results" that 0 is never an error value); `Zenith_ToString` renders it as `ok`, and callers test `eError == Zenith_NewGameError{}`. Standard integer types appear in this tooling header in place of the Zenith aliases, as [Z2-N-CONTRACT-275] allows.

## Templates

### Files every template writes

| File | Blank | 3D | 2D |
|---|---|---|---|
| `CMakeLists.txt` | `zenith_add_game(<Name>)` and LF | same | same |
| `game.json` | § "The written game.json" | same | same |
| `Scenes/Main.scene.json` | a data-only scene with one entity `Main` and its transform | a data-only scene with an entity `Camera` (a transform at (0, 1.7, −5) facing the origin) and an entity `Sun` (a transform at 50° elevation) | a data-only scene with an entity `Camera` (a transform at the origin) |
| `Scripts/.keep` (Stage 2) | empty | same | same |
| `Scripts/Main.luau` (from Stage 3) | § "The written behaviour", attached to `Main` | same, attached to `Camera` | same, attached to `Camera` |
| `Schemas/.keep` | empty | empty | empty |
| `Assets/Config/game.config.json` | § "The written configuration layer" | same | same |
| `Tests/Boot.gametest.json` | § "The written boot test" | same | same |

| Template | Modules | Content roots |
|---|---|---|
| `Blank` | none | `Assets`, `Scenes`, `Schemas`, `Scripts` |
| `3D` | none | the same |
| `2D` | none | the same |

<!-- elaboration -->
Every template selects no module, in every stage, because no module exists before Stage 3 (the "First stage" column of Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping") and [Z2-N-ARCH-272] requires every template's game to configure, build and boot on its first run from Stage 2, which `Test_NewGame_CreatedGameBoots` verifies. The three templates therefore differ in their scene layout, and their scenes carry only components that engine libraries register (transforms, behaviours); the camera, light and 2D-camera components are module types (GAME's `ZenithGameplay`, TWOD's `Zenith2D`), which a game gains when a subsequent ticket of its own, or its author with `--modules`, selects the module. <!-- assumption: pending ARCH -->
Zenith2/Docs/Architecture.md → "Engine modules and game projects" ("Creating a game") currently describes `Games/<Name>/Scenes/Main.scene.json` as "an empty scene with a camera and a light (3D) or a 2D camera (2D)" and `Games/<Name>/Scripts/Main.luau` as "a behaviour attached to the camera, declaring Early scripts". The batched Architecture patch rewords both rows to this design's Stage 2 form (data-only scenes with the entities named above; `Scripts/.keep`) and Stage 3 form (the behaviour attached), cross-area note M28.
<!-- /assumption -->

`Tests/Boot.gametest.json` is this design's addition to Architecture's file table: it is the created game's smoke test, and it makes the created game's first build also its first boot test.

### The written game.json

The scaffold writes `game.json` through its canonical writer (§ "Canonical JSON"). For `ZenithNewGame create Harbour --template 3D`:

```json
{
	"schemaVersion": 1,
	"name": "Harbour",
	"displayName": "Harbour",
	"version": "0.1.0",
	"platforms": ["android", "windows"],
	"modules": [],
	"backends": {
		"ecs": "Custom"
	},
	"content": {
		"roots": ["Assets", "Scenes", "Schemas", "Scripts"],
		"startupScene": "Scenes/Main.scene.json"
	},
	"android": {
		"applicationId": "com.zenith2.harbour",
		"versionCode": 1,
		"orientation": "landscape"
	}
}
```

`displayName` is the name; `version` is `0.1.0`; `platforms`, `modules` and `content.roots` are written in byte order; `backends.physics` is written as a string when `Physics` is selected; the `android` block is written only when `android` is a platform, with `versionCode` 1 and `orientation` `landscape`.

### The written behaviour

<!-- elaboration -->
The scaffold writes this file and its scene attachment from Stage 3, when the Luau gameplay runtime lands (Zenith2/Docs/Roadmap.md → "Stage 3 Playable authoring slice", deliverable 7); at Stage 2 a template writes `Scripts/.keep` instead, because a scene attaching a behaviour nothing can load would stop startup at step 17 with exit code 3.

```luau
--!strict
-- Games/<Name>/Scripts/Main.luau
local Zenith = require("@zenith")

type MainState = {
	Ticks: number,
}

return Zenith.Behaviour({
	Name = "<Name>.Main",
	Phase = "Early",
	StateVersion = 1,
	State = {
		Ticks = Zenith.Field.U32(0),
	},
	OnUpdate = function(self: Zenith.Self, state: MainState, ctx: Zenith.BehaviourContext)
		state.Ticks += 1
	end,
})
```

The declaration keys and field constructors are Zenith2/Docs/Scripting.md → "Behaviours and chunk systems" ("Behaviour declaration"); the behaviour declares `Early` scripts as Architecture's file table requires.

### The written scene

<!-- assumption: pending SCENE -->
The scene document format is SCENE's (`Zenith2/Docs/Design/SCENE.md`, canonical serialization per Zenith2/Docs/Roadmap.md → "Stage 2 Foundation"). The scaffold holds each template's scene as a literal text written in that format, with `<Name>` substituted, containing, at Stage 2: Blank — one entity named `Main` with a transform; 3D — an entity `Camera` with a transform at (0, 1.7, −5) looking at the origin, and an entity `Sun` with a transform at 50° elevation; 2D — an entity `Camera` with a transform at the origin. Only SCENE's transform appears. From Stage 3, `Main` (Blank) or `Camera` (3D and 2D) also gains SCRIPT's behaviour attachment `["Scripts/Main.luau"]`. `Test_NewGame_CreatedGameBoots` loads every template's scene, so a format change that the literals miss fails that test.
<!-- /assumption -->

The camera and light components a template scene names are engine component types, C++-registered by ZenithRuntime in startup step 6, never module types ([Z2-N-ARCH-614]): the camera is `Zenith.Render.Camera` (`Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration") and the directional light `Zenith.Lighting.DirectionalLight` (`Zenith2/Docs/Design/LIGHT.md` → "Light component schemas"), so a template that selects no module can name both.

### The written configuration layer

<!-- assumption: pending PLAT -->
`Assets/Config/game.config.json` is the game configuration layer (Zenith2/Docs/Architecture/PlatformLayer.md → "Configuration layering", layer 3), written empty in the configuration file format that the PLAT design (`Zenith2/Docs/Design/PLAT.md`) specifies, so the created game sets no configuration key until its author adds one.
<!-- /assumption -->

### The written boot test

```json
{
	"schemaVersion": 1,
	"ticks": 1,
	"timeoutSeconds": 60
}
```

The smoke boots headless, runs one frame through the frame loop and exits 0. The format is `Zenith2/Docs/Design/MOD.md → "The gametest format"`; `zenith_add_game` registers it as `Game_<Name>_Boot`.

### Canonical JSON

The scaffold's writer and `--migrate` produce the same bytes for the same content:

- UTF-8 without a byte-order mark, LF line endings, one trailing LF;
- one tab of indentation per level; `": "` between key and value; `,` then LF between members;
- an array of scalars on one line with `", "` separators; an array of objects one element per line;
- object keys in the fixed order of the table "Keys" of Zenith2/Docs/Design/MOD/GameBuild.md → "game.json schema v1", then `x-` keys in the order they appeared in the input;
- integers in decimal without a sign or leading zeros; strings with only `"`, `\` and control characters escaped (`\"`, `\\`, `\u00XX`), never `\/`.

## Scaffold algorithm

`Zenith_PlanNewGame(xDesc, xPlanOut, xReportOut)`:

1. Resolve the root: `xDesc.m_xRoot` when set, else `Zenith_FindZenith2Root(current directory)`; no root is `NEW_GAME_ERROR_NO_ROOT`.
2. Check the name rules in the order of the table "Output" (bad name, device name, reserved prefix, engine or tool name); the first failure is returned. The bad-name check is `Zenith_IsValidGameName`.
3. Check `<root>/Games/<Name>`: an existing entry is `NEW_GAME_ERROR_EXISTS`, or `NEW_GAME_ERROR_INCOMPLETE` when it contains `.newgame-incomplete`.
4. Resolve options: the module list (template or `--modules`), the ECS and physics backends, platforms, the application ID; an option outside its set is `NEW_GAME_ERROR_BAD_OPTION`; the two conflicts are `NEW_GAME_ERROR_OPTION_CONFLICT`.
5. Render every template file with `<Name>` (and `<name lower-cased>` in the application ID) substituted.
6. Validate the rendered `game.json` with `Zenith_ValidateGameJsonText` against the planned directory contents (the validator's existence checks read the plan, not the disk); an error here means the options produced an invalid game (for example `--modules AI` without `Physics`), reported with the validator's messages as `NEW_GAME_ERROR_VALIDATION`.
7. Sort the files by byte order of path, move `CMakeLists.txt` to the end, and return the plan.

`Zenith_ApplyNewGamePlan(xPlan, xReportOut)`:

1. `std::filesystem::create_directory(<root>/Games/<Name>)` with the `std::error_code` overload; a directory that already exists (a race with another creator) is `NEW_GAME_ERROR_EXISTS` and nothing is written.
2. Write `.newgame-incomplete` (empty) into it.
3. For each file in plan order: create its parent directories, write the bytes to `<file>.tmp`, flush and close, rename to `<file>`.
4. Remove `.newgame-incomplete`.
5. On any failure in steps 2–4: remove the whole `Games/<Name>` directory (`std::filesystem::remove_all` with the `std::error_code` overload), report the failing path and the system message, and return `NEW_GAME_ERROR_IO`. When the removal itself fails, the message names the directory to delete by hand, and the marker (written first) keeps `create` refusing it.

<!-- informative -->
`CMakeLists.txt` is written last because `zenith_add_subdirectories(Games)` globs `Games/*/CMakeLists.txt`: until that file exists, a configure that runs while creation is in progress (Ninja's automatic re-run in another shell, or a lane build) does not see the new game at all, and a creation that dies half-way leaves a directory no build picks up. The marker is what makes such a leftover visible and refusable; Architecture's rule that creation writes nothing outside `Games/<Name>/` ([Z2-N-ARCH-272]) is why the staging happens inside that directory rather than under `out/`.
<!-- /informative -->

## Validation in C++

`Zenith_ValidateGameJsonText` implements Zenith2/Docs/Design/MOD/GameBuild.md → "game.json schema v1" in C++, producing the same message lines, in the same order, as `zenith_validate_game_json` plus module resolution, for every case that does not depend on a preset:

| Check | C++ validator | CMake validator |
|---|---|---|
| every key rule of the table "Keys" | yes | yes |
| every message of the table "Validation messages" that names no preset | yes, byte-identical | yes |
| module dependency closure | yes (from the thirteen-row table compiled into the scaffold as constant data) | yes (from the declarations) |
| module or backend unsupported in a preset | no (no preset exists outside a configure) | yes |
| game directory entries (including `.migration` and the interrupted-migration refusal) | yes | yes |
| duplicate keys, the 65,536-byte limit, a byte-order mark | yes (Zenith2/Docs/Design/MOD/GameBuild.md → "Checks only the C++ validator makes") | no; the build runs the C++ validator through `<Name>_Validate` |

<!-- assumption: pending CORE -->
The C++ validator parses with ZenithCore's JSON reader (the backing chosen by D-SEL-JSON (open, Stage 1); the reader and its parser limits are the CORE design's, `Zenith2/Docs/Design/CORE.md`) configured to report duplicate keys, and writes with the canonical writer of § "Canonical JSON".
<!-- /assumption -->

`Test_GameJsonValidator_MatchesCMakeFixtures` runs the C++ validator over every script-mode `Zenith2/CMake/Tests/GameJson/<Case>/` fixture whose `expect.txt` contains no preset-dependent message (the `Validator/` cases are the C++ validator's own, run through the executable) and asserts that its message lines equal `expect.txt` exactly; the CMake validator is held to the same files by `CMake_GameJson_<Case>`. Both validators are therefore pinned to one set of expected outputs.

`validate <Name>` runs `Zenith_ValidateGame`: the C++ `game.json` validator and the game-directory entry check, then every `Games/<Name>/Schemas/*.schema.json` through the schema validator of Zenith2/Docs/Architecture/DataDefinedComponents.md → "Registration at game load" (`Zenith_TypeRegistry::RegisterSchemaJson`) on a scratch registry that holds no engine or module type. That catches every schema error except a stable-name or type-ID collision with an engine or module type, which only the cook can see because it links every module; `validate` prints `schemas: collisions with engine and module types are checked by the cook` after a clean run.

<!-- elaboration -->
Schema validation needs `ZenithReflect`, so `ZenithNewGameScaffold` declares `DEPENDS ZenithCore ZenithPlatform ZenithReflect`, which Zenith2/Docs/Architecture.md → "Library dependency graph" now lists ("ZenithNewGame (and its library ZenithNewGameScaffold) | ZenithCore, ZenithPlatform, ZenithReflect").

## game.json migration

`game.json` is an authoring document (project settings, [Z2-N-CONTRACT-552]) under Zenith2/Docs/Contracts.md → "Compatibility policy per kind" (Z2-D-048). `ZenithNewGame --migrate` runs the authoring-document migration framework of that section; it re-specifies none of its mechanisms:

| Element | For game.json | Governing text |
|---|---|---|
| schema version | `schemaVersion`, a `uint32`; v1 is current | [Z2-N-CONTRACT-530] |
| migration chain | one step `Zenith_PfnMigrationStep` per version increment, registered with `Zenith_RegisterMigrationStep` for the document type `Zenith.GameJson` (a `TYPE_KIND_STRUCT` registered by `Internal/Zenith_GameJsonMigration.cpp`'s registration function into the scratch `Zenith_TypeRegistry` the tool constructs, whose `Zenith_TypeID` the step registration takes) into the one `Zenith_MigrationRegistry` the tool constructs per run; v1 has no step yet; a step is a pure function over the document tree | [Z2-N-CONTRACT-538], [Z2-N-CONTRACT-539] |
| upgrade preview | `--migrate <Name> --preview` prints the framework's dry-run diff and writes nothing | [Z2-N-CONTRACT-531] |
| backup | the framework copies `game.json` to `Games/<Name>/.migration/backups/<UTC timestamp>/game.json` before any write; `.migration/` is gitignored and is an allowed game-directory entry (Zenith2/Docs/Design/MOD/GameBuild.md → "Game directory entries") | [Z2-N-CONTRACT-532], [Z2-N-CONTRACT-541] |
| journaled atomic write | the five steps of Contracts' table "Journaled atomic write": `<batch>.z2journal` beside `game.json`, `game.json.z2tmp`, the atomic rename, the journal deleted last; an interrupted run is recovered by the framework's start-up recovery when `--migrate` next runs, and until then `zenith_add_game` refuses the configure with its interrupted-migration message | [Z2-N-CONTRACT-533], [Z2-N-CONTRACT-540] |
| unknown fields | every member the reader does not recognise (the `x-` keys, and any other key) is kept with its path and re-emitted in position, with one `MIGRATION_WARNING_UNKNOWN_FIELD` per field path; a non-`x-` unknown key then fails `validate` and the configure as it did before the migration | [Z2-N-CONTRACT-543] to [Z2-N-CONTRACT-545] |
| fixtures | `Tools/NewGameScaffold/Tests/Fixtures/Migration/GameJson/v<N>.json` and `v<N>.expected.json` for every shipped version, v1 being the identity pair | [Z2-N-CONTRACT-548], [Z2-N-CONTRACT-549] |
| newer version | a `schemaVersion` above the tool's current version is opened read-only and refused with `ZenithNewGame: Games/<Name>/game.json schemaVersion <n> is newer than this tool's <current>; update the checkout`, exit code 3 | [Z2-N-CONTRACT-537] |

<!-- assumption: pending SCENE -->
The migration framework (`Zenith_MigrationRegistry`, the document tree, the journaled writer and its recovery) is delivered in Stage 2 by SCENE and ASSET (Zenith2/Docs/Contracts.md → "Compatibility policy per kind", "Delivery stages of the migration framework"); `ZenithNewGameScaffold` adds the library that hosts it to its `DEPENDS` when that library is named by the SCENE and ASSET designs.
<!-- /assumption -->

<!-- elaboration -->
The Stage 2 framework ships with v1 alone. The identity pair exercises the preview, the backup path and the unknown-field path through `game.json`'s own tests, and the framework's `Test_JournaledWrite_InterruptedAtEachStep_Recovers` ([Z2-N-CONTRACT-542]) covers interruption at every step for every document type, so the first real schema change adds one step function and one fixture pair.

## Editor wizard boundary

The editor's new-project wizard (inventory item Z2-I-MOD-09) is split between two areas:

| Concern | Owner | Where |
|---|---|---|
| the dialog: fields, template picker, module checklist with dependencies explained, platform and backend pickers, the file preview, error display, the confirm and cancel controls | EDIT | `Zenith2/Docs/Design/EDIT.md`; code under `Zenith2/Tools/ZenithEditor/Internal/` |
| what a creation means: name rules, defaults, the plan, validation, the files and their bytes, atomic application, the exit semantics | MOD | this file; `ZenithNewGameScaffold` |
| making the new game buildable in the running editor session | MOD states the rule, EDIT implements the prompt | below |

<!-- assumption: pending EDIT -->
The wizard fills a `Zenith_NewGameDesc`, calls `Zenith_PlanNewGame` on every change to show the plan's file list and any validation message live, calls `Zenith_ApplyNewGamePlan` on confirm, and shows the report's messages verbatim. It adds no default, rule or file of its own, so a game created by the wizard is byte-identical to one created by `ZenithNewGame create` with the same options. After a successful creation it tells the author that the next build configures the new game (Ninja's automatic CMake re-run sees the new `Games/<Name>/CMakeLists.txt` through `CONFIGURE_DEPENDS`), and offers to open it once built. EDIT's design owns the dialog's layout and its usability walkthrough; this assumption is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.
<!-- /assumption -->

## Project creation tests

| Test | File | Asserts |
|---|---|---|
| `Test_NewGameScaffold_Plan_BlankTemplate_Files` | `Tools/NewGameScaffold/Tests/Zenith_NewGameScaffold_Tests.cpp` | the Blank plan holds exactly the seven Stage 2 files of "Files every template writes" (from Stage 3, `Scripts/Main.luau` replaces `Scripts/.keep`), in byte order with `CMakeLists.txt` last, and `CMakeLists.txt` is exactly `zenith_add_game(<Name>)` plus LF |
| `Test_NewGameScaffold_Plan_3DTemplate_Files` | same | the 3D plan's `game.json` selects no module and its data-only scene holds the entities `Camera` and `Sun` (from Stage 3, `Camera` carries the `Main` behaviour) |
| `Test_NewGameScaffold_Plan_2DTemplate_Files` | same | the 2D plan's `game.json` selects no module and its data-only scene holds the entity `Camera` (from Stage 3, with the `Main` behaviour) |
| `Test_NewGameScaffold_Plan_ContainsNoCompiledSource` | same | no planned path ends `.cpp`, `.c`, `.h`, `.hpp` or `.inl`, for every template and option combination of a pairwise-covering set |
| `Test_NewGameScaffold_Plan_IsByteStable` | same | two plans from the same descriptor are byte-identical, and every planned file of each template equals its committed golden file byte for byte, with the planned path set equal to the golden set (§ "Golden template files") |
| `Test_NewGameScaffold_Name_RejectsInvalid` | same | `harbour`, `1Harbour`, `Har-bour`, a 33-character name and the empty name each return `NEW_GAME_ERROR_BAD_NAME` |
| `Test_NewGameScaffold_IsValidGameName` | same | `Harbour`, `H`, `Harbour2` and a 32-character name are accepted; `harbour`, `1Harbour`, `Har-bour`, `Harbour_`, a name holding a non-ASCII byte, a 33-character name and the empty view are refused; a view over a longer buffer is judged on its own length alone |
| `Test_NewGameScaffold_Name_RejectsReserved` | same | `Con`, `LPT9` and `ZenithGame` return `NEW_GAME_ERROR_RESERVED_NAME` |
| `Test_NewGameScaffold_Name_RejectsEngineName` | same | a name equal to a directory under a scratch root's `Engine/` returns `NEW_GAME_ERROR_ENGINE_NAME` |
| `Test_NewGameScaffold_Options_Defaults` | same | an otherwise empty descriptor yields Blank, no modules, Custom, both platforms and `com.zenith2.<lower-cased name>` |
| `Test_NewGameScaffold_Options_Conflicts` | same | `--physics` without Physics and `--application-id` without android return `NEW_GAME_ERROR_OPTION_CONFLICT` |
| `Test_NewGameScaffold_Plan_InvalidModules_Validation` | same | `--modules AI` returns `NEW_GAME_ERROR_VALIDATION` with the module-dependency message |
| `Test_NewGameScaffold_Apply_ExistingDirectory_Refused` | same | an existing `Games/<Name>` returns `NEW_GAME_ERROR_EXISTS` and its contents are unchanged |
| `Test_NewGameScaffold_Apply_IncompleteDirectory_Refused` | same | a directory holding `.newgame-incomplete` returns `NEW_GAME_ERROR_INCOMPLETE` |
| `Test_NewGameScaffold_Apply_FailureMidWrite_LeavesNothing` | same | with the fourth file's parent made unwritable, apply returns `NEW_GAME_ERROR_IO` and `Games/<Name>` no longer exists |
| `Test_NewGameScaffold_Apply_WritesCMakeListsLast` | same | a filesystem observer records `CMakeLists.txt` as the last file created and the marker removed after it |
| `Test_NewGameScaffold_Apply_WritesOnlyInsideGameDirectory` | same | a before-and-after listing of the scratch root differs only under `Games/<Name>/` |
| `Test_GameJsonValidator_MatchesCMakeFixtures` | `Tools/NewGameScaffold/Tests/Zenith_GameJsonValidator_Tests.cpp` | § "Validation in C++" |
| `Test_NewGameScaffold_Migrate_CurrentIsNoOp` | `Tools/NewGameScaffold/Tests/Zenith_NewGameScaffold_Tests.cpp` | a v1 file is reported current and not rewritten (its modification time is unchanged) |
| `Test_NewGameScaffold_Migrate_PreviewWritesNothing` | same | with the identity step forced to rewrite, `--preview` prints a diff and leaves the directory unchanged |
| `Test_NewGameScaffold_Migrate_BacksUpBeforeWrite` | same | with the identity step forced to rewrite, the backup `Games/<Name>/.migration/backups/<UTC timestamp>/game.json` exists with the original bytes before `game.json` changes |
| `Test_NewGameScaffold_Migrate_RecoversPendingJournal` | same | a `WRITTEN`-state `<batch>.z2journal` with a matching `game.json.z2tmp` beside `game.json` is rolled forward by the next `--migrate` through the framework's recovery, after which the configure accepts the directory |
| `Test_NewGameScaffold_Migrate_IdentityFixture` | same | `Fixtures/Migration/GameJson/v1.json` migrates to exactly `v1.expected.json` |
| `Test_NewGameScaffold_Migrate_PreservesXKeys` | same | `x-` keys at three levels and one non-`x-` unknown key survive byte for byte in position, with one `MIGRATION_WARNING_UNKNOWN_FIELD` each |
| `Test_NewGameScaffold_Migrate_NewerVersion_Refused` | same | `schemaVersion` 2 is refused with the newer-version message and code 3 |
| `Test_NewGame_CreatedGameBoots` | `Tests/NewGame/Zenith_NewGame_Tests.cpp` | create one game per template (`BootBlank`, `Boot3D`, `Boot2D`) in one scratch tree, configure the `win` preset once, build the three game targets and their content, run each `Game_<Name>_Boot`, and assert exit code 0 for each, reported as doctest subcases `[Blank]`, `[3D]`, `[2D]` ([Z2-N-ARCH-272]) |
| `Test_NewGame_Cli_ExitCodes` | same | the executable returns 0, 1, 2 and 3 for a success, a forced I/O failure, a bad name and an invalid game respectively, and prints the exact messages of "Output" |
| `Test_NewProjectWizard_MatchesCli` | `Tools/ZenithEditor/Tests/Zenith_NewProjectWizard_Tests.cpp` (EDIT's file, Stage 4) | a scripted wizard session with the options of `ZenithNewGame create Harbour --template 3D --platforms windows` produces a directory byte-identical to the command line's |

<!-- elaboration -->
`Test_NewGame_CreatedGameBoots` configures and builds real games, so it is registered with the `z2-stress` suite label (the 600-second CTest timeout of Zenith2/Docs/Contracts.md → "Tests and code hygiene gates") and in the `win` preset alone. Its scratch tree is `${ZENITH_OUT}/tests/NewGame/Zenith2/`: the tracked root files (`CMakeLists.txt`, `CMakePresets.json`, `.clang-format`) are copied, and `Engine/`, `Player/`, `Tools/`, `Benchmarks/`, `Tests/`, `CMake/`, `ThirdParty/` and `Shaders/` are NTFS directory junctions to the checkout's directories (created with `cmd.exe /c mklink /J`, which needs neither Developer Mode nor `SeCreateSymbolicLinkPrivilege`), while `Games/` is a real directory holding only the three created games, so the test never writes into the tracked `Games/`. One configure serves all three games, and every engine compile is an sccache hit against the lane's own `win` tree, which the Test step has just built, so the test's work is the scratch configure, the link of each library and game, the three cooks and the three boots. The Stage 2 acceptance run records the test's measured duration; a run above the 600-second `z2-stress` timeout fails the test, and the breach is filed against the Test step's timeout row Z2-B-PROC-011 (pending) as Zenith2/Docs/Budgets.md → "Purpose versioning and change rule" prescribes.

### Golden template files

The byte-stability oracle is the committed bytes, never a committed digest. A digest cannot be written by the change that alters a template: its author would need to run the test to learn the new value, which a loop worker cannot do. The bytes can be written, because every planned file is fully determined by the template literals, the `<Name>` substitution and § "Canonical JSON".

- `Zenith2/Tools/NewGameScaffold/Tests/Fixtures/TemplatePlans/<Template>/` (`Blank`, `3D`, `2D`) holds one file per planned path of the descriptor `create Harbour --template <Template>` with every other option at its default, at the planned relative path with the suffix `.golden` appended (`CMakeLists.txt.golden`, `game.json.golden`, `Scenes/Main.scene.json.golden`, `Scripts/.keep.golden` at Stage 2, `Scripts/Main.luau.golden` from Stage 3, …). The suffix keeps every golden file out of the `SourceLayout`, Luau and CMake scans: a golden is data of the scaffold's tests, not a game file.
- `Test_NewGameScaffold_Plan_IsByteStable` plans each template twice, asserts the two plans equal, then compares the plan with the golden directory through `ZENITH_TEST_FIXTURE_ROOT`: the sorted planned paths equal the sorted golden paths with the suffix removed, and each planned file equals its golden byte for byte. On a mismatch it prints the first differing path, its byte offset, and the planned and golden SHA-256 of that file as a diagnostic only.
- A change that alters a template's output (a new file, a scene component, a behaviour attachment) edits the matching golden files in the same diff, by hand, from the literal it changes. The test then fails only for an unintended change.
- This is the pattern for every ticket that changes a template's bytes, MOD's and other areas' alike (for example a light component added to the 3D template's `Sun`): edit the golden file the change determines; never record a digest from a test run. Only output a worker cannot derive from a specification (a numerical kernel's results, a device capture) is recorded by a `needs-human` recording ticket that runs the test and commits the file.
