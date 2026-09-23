# libgraph fixture repo

A miniature Zenith checkout, small enough to read in one sitting, that the
real `vcxproj.py` / `sharpmake.py` / `model.py` parsers accept unmodified.
Waves 2-4 build on this same tree (see the W1 brief and the approved plan at
`C:\Users\tomos\.claude\plans\i-want-a-way-quirky-mountain.md`). **Nothing
under `fixtures/repo/**` is owned by any wave but W1** — a later wave that
needs a change here asks the orchestrator, who routes it back to W1.

Pinned node names: libs `LibA` (leaf), `LibB` (depends on `LibA` and, per
scenario 25, `MidLib`), `LibC`
(shares `Zenith/Shared` with `LibB`, AssetHandling-style split); middleware
`MidLib` (Entry-table, source root `Middleware/MidLib`) plus two
tree-discovered synthesized third-party nodes; aggregate `Zenith` (source
root `Zenith/EngineComposition`); games `GameX` and `GameY`; proof
`SentinelLibB` (subject `LibB`).

Two configs on every vcxproj: `fx_debug` (`FunctionLevelLinking=false`) and
`fx_release` (`FunctionLevelLinking=true`). Neither carries a RenderBackend
prefix, so `SentinelLibB`'s conditional edge stays **unresolved** for both —
RenderBackend resolution itself is tested with inline `vulkan_x` / `null_x`
config strings in `test_model.py`, not against this tree.

W2b authors `fixtures/dumpbin/{LibA,LibB,LibC,MidLib,Zenith,GameX,GameY}.txt`
(not `SentinelLibB`) from the "dumpbin symbols" column below.

## Edge-class rule (for W2b/W3, stated here per the W1 brief)

An edge L→M carries exactly one BASE class plus zero or more ADDITIVE tags.
*Use* = a non-test, non-pch include (a `conditional` one still counts) OR a
link symbol. *pch reach* = a closure edge that exists only because of the
PCH.

| Declared? | Condition | Base class |
|---|---|---|
| yes | has a use | `declared+used` |
| yes | no use | `declared-unused` |
| no | has a non-test, non-pch include | `used-undeclared` |
| no | only includes come from `*.Tests.inl` | `test-only` |
| no | only reach is the PCH | `pch` |
| no | no include at all, but a link symbol | `link-only` |

Additive tags: `allowlisted` (an include line matches an allowlist key;
`allowlist_kind` is that key's kind, `null` for a plain line) and `in-cycle`
(src/dst share an SCC over `declared+used` / `used-undeclared` edges). Game
edges are not classified. `pch` edges are attributed to the node that owns
`pch_root` — here, **LibA** (exactly as ZenithCore is in the real repo).

★ **The `via_aggregate` refinement (`model.py`'s `DeclaredEdge.via_aggregate`
field).** A `via_aggregate=True` edge (the aggregate `Zenith` → every
Entry-table lib, from expanding `ZenithEngineLibs.AddAllAsDependenciesOfAggregate`)
counts as `Declared? = yes` in the table above **only when the edge ALSO
carries at least one normal (non-pch, non-test) include line**. The
aggregate "declares" every lib mechanically, so treating that alone as a
real declaration would hide its genuine link-only and undeclared structure
— a link symbol reached with no include does NOT, by itself, make a
`via_aggregate` edge count as declared (contrast a literal
`AddPublicDependency<X>` edge, `via_aggregate=False`, which counts as
declared regardless of whether anything ever uses it — see scenario 12,
LibC→MidLib `declared-unused`). And: **an aggregate edge with no include
line and no link symbol at all is not emitted as an edge — neither
`declared` nor any other base class fits it, so it is dropped entirely,**
not reported as some zero-evidence row. (In this fixture, Zenith never
`#include`s or calls into anything owned by LibB or MidLib at all — those
two aggregate-expansion facts stay in `Model.declared` as raw data, since
`model.py` records every literal dependency regardless of use, but produce
no Zenith→LibB / Zenith→MidLib edge in the metrics/report output.)

## Scenario map

1. **PCH.** `Zenith/LibA/FxPch.h`, owned by LibA, found via LibA's
   `Zenith/LibA` include dir. Every fixture `.cpp` starts
   `#include "FxPch.h"`. Includes `"MidLib/MidLib.h"`. `pch_root` =
   `Zenith/LibA/FxPch.h`. Files: `Zenith/LibA/FxPch.h`.

2. **Pure internal header.** `Zenith/LibA/Internal/LibA_Private.h`, included
   only from `Zenith/LibA/LibA.cpp`. Declared-internal (under `Internal/`).
   Files: `Zenith/LibA/Internal/LibA_Private.h`, `Zenith/LibA/LibA.cpp`.

3. **Encapsulation break (allowlisted).**
   `Zenith/LibA/Internal/LibA_Detail.h` included by
   `Zenith/EngineComposition/Composition.cpp`. Edge Zenith→LibA:
   `declared+used` + `allowlisted` (`allowlist_kind = encapsulation`), via
   `Tools/architecture_allowlist.txt`'s `encapsulation:` line. This edge IS
   `via_aggregate=True` too, but — unlike scenario 18 — it carries a normal
   include line (`#include "LibA/Internal/LibA_Detail.h"`), so the
   `via_aggregate` refinement still counts it as declared: `declared+used`
   stands. dumpbin:
   `LibA_DetailState` has no out-of-line symbols (header-only), so nothing
   to assert there; the include itself is the fixture.

4. **Unused public header.** `Zenith/LibA/LibA_Unused.h` — listed by LibA's
   vcxproj, included by nobody at all.

5. **Entry header and types.** `Zenith/LibA/LibA.h`: `class LibA_Thing`,
   `struct LibA_Data`, `enum class LibA_Mode`, `using LibA_Handle = unsigned
   int;`, `typedef int LibA_Count;`, out-of-line `LibA_Thing::Tick()`
   (defined in `LibA.cpp`) and free function `LibA_Init()`. `LibA_Mode` and
   `LibA_Count` are used by no consumer outside LibA — dumpbin: `LibA.txt`
   defines `LibA_Thing::Tick` and `LibA_Init` as External; no other fixture
   `.txt` should UNDEF either name that resolves through a `LibA_Mode`- or
   `LibA_Count`-typed symbol (neither type has an out-of-line surface of its
   own to check).

6. **Exposed-only header.** `Zenith/LibA/LibA_Math.h`, included by `LibA.h`
   only — never directly from outside LibA.

7. **Single-consumer header.** `Zenith/LibA/LibA_Solo.h`, included from
   outside LibA only by `Zenith/Shared/LibC_Impl.cpp` (scenario 11).

8. **Leaking header.** `Zenith/Shared/LibB_Api.h` (owned by LibB) includes
   `"LibA/LibA.h"` and uses LibA's types three ways: `LibA_Data m_xData;`
   (embed, by-value member), `std::vector<LibA_Data> m_axItems;` (embed,
   template argument by value), `LibA_Thing* m_pxThing;` (ref), and
   `class LibB_Widget : public LibA_Thing` (embed, base class). dumpbin:
   `LibB.txt` should show no UNDEF to `LibA_Thing::Tick` from this header
   alone (no call site here) — the leak is a type/header leak, not a link
   edge.

9. **Split source root.** `LibB` and `LibC` share `Zenith/Shared/` (see
   `Build/Sharpmake_LibB.cs` / `Sharpmake_LibC.cs`'s per-file
   `SourceFilesExcludeRegex` lines). `Build/libb_win64.vcxproj` lists
   `LibB_Api.h`, `detail/LibB_Inner.h`, `LibB_Sibling.h`, `LibB_Impl.cpp`;
   `Build/libc_win64.vcxproj` lists `LibC_Api.h`, `LibC_Impl.cpp` — an exact
   partition, verified by `model.py`'s ownership reconciliation.

10. **Parent include stack.** `Zenith/Shared/LibB_Api.h` includes
    `"detail/LibB_Inner.h"`, which includes `"LibB_Sibling.h"`.
    `LibB_Sibling.h` exists only at `Zenith/Shared/LibB_Sibling.h`, and
    `Zenith/Shared` is in no project's include dirs — resolvable only via
    the parent-include stack (LibB_Api.h's own directory, contributed to the
    stack when LibB_Inner.h is opened from it).

11. **Pure used-undeclared.** `Zenith/Shared/LibC_Impl.cpp` includes
    `"LibA/LibA.h"` and `"LibA/LibA_Solo.h"`; contains `sizeof(LibA_Data)`
    (a mention) and a by-value parameter `void LibC_Take(LibA_Data xData)`
    (a mention). LibC's `.cs` does not declare LibA. No allowlist line
    matches. Edge LibC→LibA: `used-undeclared`, no `allowlisted` tag.

12. **declared-unused.** `Build/Sharpmake_LibC.cs` declares MidLib
    (`AddPublicDependency<MidLibLibProject>`), but nothing in LibC includes
    a MidLib header except transitively through the PCH
    (`FxPch.h` → `MidLib/MidLib.h`). Edge LibC→MidLib: `declared-unused`.
    dumpbin: `LibC.txt` must contain no UNDEF resolving to a MidLib-owned
    symbol (`MidLib_Init`, `MidLib_BackendImpl`).

13. **test-only.** `Zenith/LibA/LibA.Tests.inl` (listed in LibA, included
    only from `LibA.cpp`) includes `"Shared/LibC_Api.h"` and calls
    `LibC_Api_Touch()`. Edge LibA→LibC: `test-only`. dumpbin: `LibA.txt`
    must contain no UNDEF resolving to `LibC_Api_Touch` (or any other
    LibC-owned symbol).

14. **Cycle + allowlisted + undeclared.** `Zenith/Shared/LibC_Api.h`
    includes `"Shared/LibB_Api.h"`; LibC declares LibB, so LibC→LibB is
    `declared+used` + `in-cycle`. `Zenith/Shared/LibB_Impl.cpp` includes
    `"Shared/LibC_Api.h"` and calls `LibC_Api_Touch()`; LibB does NOT
    declare LibC, so LibB→LibC is `used-undeclared` + `in-cycle` +
    `allowlisted` (`allowlist_kind = null`), via
    `Tools/layering_allowlist.txt`'s
    `Zenith/Shared/LibB_Impl.cpp => Shared/LibC_Api.h` line. dumpbin:
    `LibB.txt` UNDEFs `LibC_Api_Touch`; `LibC.txt` defines it External.

15. **Conditional include.** `Zenith/Shared/LibB_Impl.cpp` includes
    `"LibA/LibA_Extra.h"` inside `#ifdef FX_TOOLS … #endif` — a genuine
    conditional use (not the file's own include guard), tagged
    `conditional`, still counting as use.

16. **Ambiguous include.** `Zenith/FxConfig.h` and `Zenith/LibA/FxConfig.h`
    both exist and are both listed by `Build/liba_win64.vcxproj`.
    `Zenith/Shared/LibC_Impl.cpp` includes `"FxConfig.h"`: two include dirs
    (`Zenith`, `Zenith/LibA`) could satisfy it (both are LibC's own
    `AdditionalIncludeDirectories`), so it resolves to the first
    (`Zenith/FxConfig.h`) and should be reported ambiguous.

17. **Games.** `Games/GameX/Source/GameX_Component.h` includes only
    `"Shared/LibB_Api.h"`, so GameX reaches LibA only via the leaking
    header, **from a header**; holds `LibA_Handle m_hHandle` (by value) and
    `LibB_Widget* m_pxWidget` (pointer). `Games/GameX/Source/GameX.cpp`
    includes the component header. `Games/GameY/Source/GameY.cpp` includes
    `"Shared/LibB_Api.h"` directly, so GameY reaches LibA only via the
    leaking header, **from a `.cpp` only**. Both games' vcxproj use
    `PrecompiledHeader=NotUsing`, but both `.cpp` files
    `#include "FxPch.h"` literally.

18. **link-only.** `Zenith/EngineComposition/Composition.cpp` declares
    `extern void LibC_ExportAll();` and calls it without including any LibC
    header. Edge Zenith→LibC: `link-only`. ★ **This flipped twice while the
    fixture was being reviewed — the `via_aggregate` field is exactly why
    it settles here.** The aggregate declares every Entry-table lib
    (`Zenith→LibC` exists in `Model.declared` via aggregate expansion,
    `via_aggregate=True`), which made an early pass call this
    `declared+used` (declared: yes, since SOME edge exists; has a use, the
    `LibC_ExportAll` UNDEF). But metrics treats a `via_aggregate=True`
    declaration as `Declared? = yes` **only when the edge also carries a
    normal include line** (see the `via_aggregate` refinement above) — and
    this one carries none, only the link symbol. So `Declared?` reads `no`
    for classification purposes, and the edge-class table's next matching
    row is `no / no include at all, but a link symbol / link-only`. Scenario
    27 below reaches the same base class from a src node (MidLib) that
    genuinely never declares its dst at all — two different roads to
    `link-only`, one through the `via_aggregate` refinement, one without it.
    dumpbin: `Zenith.txt` (the aggregate IS a lib, so W2b authors this
    dumpbin file too) UNDEFs `LibC_ExportAll`; `LibC.txt` defines it
    External, out of line (`Zenith/Shared/LibC_Impl.cpp`).

19. **Orphan.** `Zenith/LibA/LibA_Orphan.h` exists on disk but is not listed
    in `Build/liba_win64.vcxproj` (or anywhere else) — appears in
    `Model.orphans`.

20. **Duplicate.** `Middleware/MidLib/backends/mid_impl.h` is listed by BOTH
    `Build/libb_win64.vcxproj` and `Build/libc_win64.vcxproj`. Owned by
    MidLib (its source root `Middleware/MidLib` prefix-matches the path);
    reported in `Model.duplicates`; `FileOwner.duplicate_of = ["LibB",
    "LibC"]`.

21. **Unlisted third-party.** Nobody's vcxproj lists
    `Middleware/glmx/glmx.hpp`; no Entry-table middleware node's source root
    matches `Middleware/glmx`, so it synthesizes the plain middleware node
    `glmx` (`vcxproj: None`). `Zenith/LibA/LibA.h` includes
    `"glmx/glmx.hpp"`.

22. **Sentinel.** `Tests/SentinelLibB/main.cpp` +
    `Build/Sharpmake_SentinelLibB.cs`, deps in order LibB, LibA, MidLib —
    `subject = LibB`. Also carries a RenderBackend conditional chain
    (Vulkan → LibC, D3D12 → LibA again, Null → LibB again) that stays
    unresolved for both `fx_debug`/`fx_release` (no backend prefix). Lower
    bound for LibB = {LibA, MidLib} (LibB's own declared closure): LibB→LibA
    gets `lower_bound: true`; LibB→LibC gets `false` (LibC is never in
    SentinelLibB's *unconditional* closure).

23. **Comment-only dependency.** `Build/Sharpmake_LibB.cs` has a
    `// conf.AddPublicDependency<LibCLibProject>(target);` line — must not
    become a declared edge (`sharpmake.py` strips comments first). The
    unknown-class and vcxproj-with-no-class hard-error poison cases are
    deliberately NOT in this tree (they live as inline strings /
    tempdir-built trees in `test_model.py`), so this fixture always parses
    clean.

24. **Test include from an ordinary TU (dst is the `.Tests.inl`).** Real
    repo shape: `Zenith/EntityComponent/Components/Zenith_AIAgentComponent.cpp`
    `#include`s AI-owned `AI/**/*.Tests.inl` directly. `Zenith/Shared/LibB_Api.Tests.inl`
    (owned by LibB, listed in `Build/libb_win64.vcxproj`) `#include`s
    `"LibB_Api.h"` and touches `LibB_Widget` (header-only — no out-of-line
    call). `Zenith/LibA/LibA.cpp` `#include`s
    `"Shared/LibB_Api.Tests.inl"` directly — LibA has no OTHER include of
    any LibB file. Edge LibA→LibB: `test-only` (a `test` tag applies when
    EITHER src or dst is a `*.Tests.inl` — here it's the dst side, the
    mirror image of scenario 13's src-side case). dumpbin: `LibA.txt` must
    carry **no UNDEF resolving to a LibB-owned symbol** — the test body's
    `LibB_Widget` construction is compiler-generated (no user-defined,
    out-of-line constructor), so the link side stays genuinely empty and the
    edge can't accidentally reclassify as `used-undeclared` via a link-only
    reach.

25. **Header reachable both via the PCH and directly.**
    `Zenith/Shared/LibB_Impl.cpp` `#include`s `"MidLib/MidLib.h"` directly,
    in addition to reaching it transitively through `FxPch.h` (scenario 1:
    `FxPch.h` → `"MidLib/MidLib.h"`). `Build/Sharpmake_LibB.cs` now also
    declares MidLib (`AddPublicDependency<MidLibLibProject>`), alongside its
    existing LibA dependency. Edge LibB→MidLib: `declared+used`, via the
    direct include — stays `declared+used` rather than falling back to
    `pch`-only reach precisely because the direct include exists.
    `closure_no_pch("Zenith/Shared/LibB_Impl.cpp")` must still contain
    `Middleware/MidLib/MidLib.h` even though that header is ALSO inside
    `closure(pch_root)` — i.e. subtracting the PCH-only reach must not
    remove a header that has an independent, direct reach of its own.

26. **pch-only (a real `pch` base class, on a middleware node).**
    `Middleware/MidLib/MidLib.cpp`'s ONLY include is `#include "FxPch.h"` —
    LibA's own file (`Zenith/LibA/FxPch.h`) and the model's `pch_root`.
    `Build/Sharpmake_MidLib.cs` declares no dependency on LibA (it declares
    nothing at all). MidLib's only reach to LibA is therefore the PCH header
    itself, with no other LibA include anywhere in MidLib. Edge MidLib→LibA:
    base class `pch`, attributed to LibA (the `pch_root` owner) — the first
    scenario in this fixture where a `pch`-classified edge is the whole
    point rather than a side effect. **Middleware edges are classified same
    as lib edges; only game edges are exempt** (see the edge-class rule
    above), so this is a genuine, asserted edge, not a skipped one.

27. **link-only, from a src with NO declared edge at all (not even
    `via_aggregate`).** `Middleware/MidLib/MidLib.cpp` also declares
    `extern void LibB_Widget_Poke();` and calls it, with NO `#include` of
    any LibB file anywhere in MidLib, and `Build/Sharpmake_MidLib.cs`
    declares no dependency on LibB (MidLib isn't an aggregate, so unlike
    scenario 18 there is no `via_aggregate` edge to even consider — MidLib
    has zero recorded declaration toward LibB in `Model.declared`).
    `Zenith/Shared/LibB_Impl.cpp` defines `void LibB_Widget_Poke() { }` out
    of line, with no LibB header declaring it (mirrors scenario 18's
    `LibC_ExportAll` shape at the file level — no include, an extern
    declaration, a call — but the two now reach `link-only` by different
    roads: scenario 18 via the `via_aggregate` refinement stripping its
    declaration down to "not declared for classification purposes"; this
    one because MidLib was never declared toward LibB in any form to begin
    with). Edge MidLib→LibB: base class `link-only` (no include at all, but
    a link symbol). dumpbin: `MidLib.txt` UNDEFs `LibB_Widget_Poke`;
    `LibB.txt` owns it.

## Known deviations from the brief (see the W1 implementer's final report)

None — every scenario in the original W1 brief (1–23) was realised exactly
as specified. Scenarios 24–27 were added afterward, requested by the
wave 2 critics and folded in by W1 (the fixture's sole owner) with no
deviation from how the orchestrator described them either.
