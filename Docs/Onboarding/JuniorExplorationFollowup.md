# Junior Exploration Follow-up

In 2026-05, five junior programmers explored the Zenith codebase and reported areas they found difficult. This document records the triage decisions and the resulting work — useful for future newcomers who want to know what was actually hard versus what was already documented.

## Triage table

| Concern | Where | Decision | Pointer |
|---------|-------|----------|---------|
| `Flux/CLAUDE.md` claimed a `RenderOrder` enum that does not exist | `Flux/CLAUDE.md:9,42` | **fixed** — rewrote to describe topo-sort model | `Flux/CLAUDE.md` (Pass Execution Order section) |
| Six Flux subsystem docs referenced `RENDER_ORDER_*` constants as if real | `Flux/SSR/`, `Flux/SSGI/`, `Flux/HiZ/`, `Flux/HDR/`, `Flux/Vegetation/`, `Flux/Fog/` CLAUDE.md | **fixed** — rewrote each as Read/Write dependency tables | each subsystem CLAUDE.md |
| Root `CLAUDE.md` cited `RENDER_ORDER_SKYBOX` as a non-existent enum example | `CLAUDE.md:96`, `AGENTS.md:94` | **fixed** — replaced with `RESOURCE_ACCESS_READ_SRV` | both root files |
| No render-graph-specific documentation | `Flux/RenderGraph/` | **fixed** — added `RenderGraph/CLAUDE.md` covering Setup/Compile/Execute, fluent builder, barrier synthesis, Print Pass Order button | [Flux/RenderGraph/CLAUDE.md](../../Zenith/Flux/RenderGraph/CLAUDE.md) |
| No render pipeline flow diagram | `Flux/CLAUDE.md` | **fixed** — added "Render Pipeline at a Glance" ASCII diagram | top of `Flux/CLAUDE.md` |
| Async asset loading documentation implies it is usable | `AssetHandling/CLAUDE.md`, async loader headers | **fixed** — explicit "NOT IMPLEMENTED" notice added | `AssetHandling/CLAUDE.md` |
| No enumerated mapping from `SceneManager` facade methods to Internal subsystems | `EntityComponent/CLAUDE.md` | **fixed** — added facade -> subsystem mapping table | `EntityComponent/CLAUDE.md` |
| `NewcomerMap.md` lacked a "where the real implementation lives" cheat-sheet | `Docs/Onboarding/NewcomerMap.md` | **fixed** — added ECS + Flux cheat-sheet, cross-links to this doc | `Docs/Onboarding/NewcomerMap.md` |
| `Zenith_EditorState.h` declared but unused (planned refactor scaffolding) | `Editor/Zenith_EditorState.h` | **fixed** — wired in incrementally (deferred ops first, then selection, then console/content) | `Editor/Zenith_Editor.cpp` and panel files |
| `Zenith_AssetHandle::Get()` privacy forces awkward two-step access | `AssetHandling/Zenith_AssetHandle.h` | **fixed** — added public `Resolve()` accessor; existing two-step still works | `AssetHandling/Zenith_AssetHandle.h` |
| `BuildProfileHierarchy` has dense nested control flow; pause flags unclear | `Profiling/Zenith_Profiling.cpp:417-487` | **fixed** — extracted helper functions; renamed/commented `dbg_bPaused`/`g_bIsPaused` | `Profiling/Zenith_Profiling.cpp` |
| `Zenith_SceneManager.h` is 1291 lines, overwhelming despite navigation guide | `EntityComponent/Zenith_SceneManager.h` | **fixed** — split into public API (~850 lines), `Zenith_SceneManagerGuards.h`, `Zenith_SceneManagerInternal.h` | `EntityComponent/Zenith_SceneManager*.h` |
| `Zenith_Editor.cpp` claimed to have ~100 statics | `Editor/Zenith_Editor.cpp` | **false-alarm** — actual count is 6 in the .cpp; state lives in panel files. The `EditorState` wire-in addresses the spirit of the concern. | n/a |
| `IsAnimatedSkinnedModel` flagged as duplicated | `Flux/StaticMeshes/Flux_StaticMeshes.cpp:121,214,239` | **false-alarm** — one definition + two call sites = correct DRY | n/a |
| `Flux_StaticMeshes.cpp` debug logging "noisy" | `Flux/StaticMeshes/Flux_StaticMeshes.cpp:154-174` | **deferred** — one-shot guarded log, intentional. Not worth touching unless extracting for unrelated reasons. | n/a |
| `AssetHandle::Get()` privacy claimed undocumented | `AssetHandling/Zenith_AssetHandle.h:38-42` | **false-alarm** — already documented at function declaration (now superseded by `Resolve()` API addition above). | `Zenith_AssetHandle.h:38-42` |
| Memory allocation rule claimed only in CLAUDE.md | `Core/Memory/Zenith_MemoryManagement.h` | **false-alarm** — already documented in-header at lines 53-71 with examples | `Zenith_MemoryManagement.h:53-71` |
| `s_axEntitySlots` global-not-per-scene called surprising | `EntityComponent/Zenith_SceneData.h:446-470` | **false-alarm** — already documented at the declaration and in `EntityComponent/CLAUDE.md` | `Zenith_SceneData.h:446-470`, `EntityComponent/CLAUDE.md` |
| `Zenith_ComponentMeta.h` dense template metaprogramming | `EntityComponent/Zenith_ComponentMeta.h` | **deferred** — inherent complexity of a reflection-via-concepts system, well-commented; refactoring would not be a net win | n/a |
| `Flux_CommandList` 16-class command pattern verbose | (removed) | **resolved** — the deferred command-list DSL was removed entirely; render systems now record the backend command buffer directly (the render graph owns ordering + barriers, so the DSL was pure indirection). | `Flux/CLAUDE.md` |
| `SceneManager.h` <-> `SceneData.h` include cycle | `EntityComponent/Zenith_SceneManager.h`, `Zenith_SceneData.h` | **deferred** — load-bearing for template instantiation; the documented `T2.4` task to extract `Zenith_ComponentPool*` types is a separate larger refactor | `Zenith_SceneData.h:842-855` |

## Notes for future newcomers

- The codebase has CLAUDE.md files in most subsystems (`Flux/CLAUDE.md`, `EntityComponent/CLAUDE.md`, `Vulkan/CLAUDE.md`, etc.). Read those first before searching for documentation elsewhere.
- `Internal/ARCHITECTURE.md` files exist where the public surface forwards to internal subsystems (notably `EntityComponent/Internal/ARCHITECTURE.md`). They are easy to miss because they sit one level deeper than the subsystem CLAUDE.md.
- The render graph topologically sorts on declared Read/Write — there is no ordering enum or magic constant. Click `Render/RenderGraph/Print Pass Order` in debug variables to see the live sort.
- "Sharp edges" (memory allocation rules, physics reset order, entity ID lifetime, etc.) are catalogued in `Docs/Onboarding/NewcomerMap.md`. Read that section before changing engine code.
