using Sharpmake;
using System;

// =============================================================================
// Single source of truth for the set of engine static-lib projects.
//
// Before this file existed, the same five-project list (ZenithBaseLibProject,
// ZenithECSLibProject, ZenithPhysicsLibProject, ZenithAILibProject,
// ZenithProject) was hand-written independently in THREE places. Phase 4a
// dissolved ZenithBaseLibProject into four single-directory leaf libs
// (ZenithCoreLibProject, ZenithMathsLibProject, ZenithDataStreamLibProject,
// ZenithFileAccessLibProject — see Sharpmake_ZenithCore.cs), taking the table
// to eight rows. Phase 4b added seven more (ZenithDebugVariables, ZenithProfiling,
// ZenithTaskSystem, ZenithInput, ZenithWindows, ZenithAndroid, ZenithUnitTests —
// see each one's own Sharpmake_Zenith<Name>.cs header comment), taking it to
// fifteen. Phase 4c added five more (ZenithSaveData, ZenithTelemetry,
// ZenithPrefab, ZenithScripting, ZenithJolt — the last one a straight split of
// the vendored Jolt tree OUT of ZenithPhysicsLibProject into its own leaf, see
// Sharpmake_ZenithJolt.cs), taking it to twenty. Phase 4d added three more
// (ZenithImGui, ZenithAssetHandling, ZenithFlux — ZenithImGui a straight split
// of imgui's five core TUs out of the aggregate into its own leaf, alongside
// ZenithFlux and ZenithAssetHandling splitting out the render system and the
// asset-handling layer with a genuine header cycle between them declared
// one-way, see Sharpmake_ZenithFlux.cs), taking it to twenty-three. Phase 4e
// added the three render-backend leaves (ZenithRenderBackendVulkan,
// ZenithRenderBackendD3D12, ZenithRenderBackendNull — splitting
// Zenith/RenderBackends into one target-restricted library PER BACKEND
// instead of one library globbing all three and build-excluding two, see
// Sharpmake_ZenithRenderBackendVulkan.cs), taking it to twenty-six. Phase 4f
// added the two top-of-the-DAG leaves (ZenithUI, layer 5, and
// ZenithEntityComponent, layer 6 -- see each one's own Sharpmake_Zenith*.cs
// header comment), taking it to twenty-eight. Phase 4g added the two
// Tools-only leaves at the top of the DAG (ZenithEditor, layer 7, and
// ZenithTools, which adopts a pre-existing orphan project rather than
// creating a new one -- see Sharpmake_ZenithEditor.cs / Sharpmake_ZenithTools.cs's
// own header comments), taking it to thirty. Phase 4h.3 split
// ZenithRenderBackendVulkan's two nested Platform/ subdirectories into
// their own single-file leaves (ZenithRenderBackendVulkanPlatformWindows /
// ...PlatformAndroid -- the last call site where ConfigurePlatformExcludes
// was load-bearing anywhere in the codebase; see those two files' own
// header comments and Sharpmake_Common.cs for the method's removal), taking
// it to thirty-two. Phase 4i added ONE more -- ZenithAssetRegistry, the
// Flux-free lower half of a split Zenith/AssetHandling (identifiers, lookup,
// lifetime), which finally BREAKS the Flux <-> AssetHandling header cycle
// Phase 4d could only declare one-way: Flux's edge moves down to the registry
// half, and the GPU half (which keeps the ZenithAssetHandling name and its 9
// units) moves ABOVE Flux and depends on it. See
// Sharpmake_ZenithAssetRegistry.cs's own header comment for the measurement
// and Sharpmake_ZenithAssetHandling.cs for the other side -- taking it to
// thirty-three. Nothing else about the mechanism changed:
//   * Sharpmake_Solutions.cs  ZenithEngineSolution.ConfigureAll  (conf.AddProject<...>)
//   * Sharpmake_Games.cs      GameSolution.ConfigureAll          (conf.AddProject<...>)
//   * Sharpmake_Zenith.cs     ZenithProject.ConfigureAll         (conf.AddPublicDependency<...>,
//                             which lists every leaf EXCEPT the aggregate itself)
// A later phase grows this list to ~30 entries, several restricted to a subset
// of targets (e.g. win64-only, or Vulkan-only) -- three independently
// hand-maintained lists cannot be trusted to stay in lockstep at that size.
//
// WHY EVERY ENTRY CARRIES A PREDICATE (do not delete this as dead weight):
// `conf.AddProject<T>` / `conf.AddPublicDependency<T>` THROW at Sharpmake-run
// time if the target being configured is not one T itself declares (via
// T's own AddTargets calls). Today every one of the five libs below declares
// the full win64 (all RenderBackend/ToolsEnabled/Optimization combos) + agde
// target set, so every predicate here is "always" -- but the moment a future
// library restricts its own target set (e.g. a win64-only lib with no agde
// target), every call site that adds it unconditionally starts throwing for
// agde. The predicate is what lets a restricted-target library be added to
// this single table with a matching guard, instead of every call site growing
// its own hand-written `if` -- which is exactly the duplication this file
// removes. Add a library by appending ONE row; if it needs a target
// restriction, give it a non-default predicate instead of guarding call sites.
// =============================================================================

public static class ZenithEngineLibs
{
	public struct Entry
	{
		public Type ProjectType;
		public Func<ZenithTarget, bool> Applies;

		public Entry(Type xProjectType, Func<ZenithTarget, bool> fnApplies = null)
		{
			ProjectType = xProjectType;
			Applies = fnApplies ?? (target => true);
		}
	}

	// The engine library set, in dependency order (leaves first, aggregate
	// last). This order is significant: it is the order every call site below
	// adds the projects in, matching what the three hand-written lists did
	// before this file existed.
	public static readonly Entry[] All = new Entry[]
	{
		// The four Phase 4a leaves that replaced ZenithBaseLibProject.
		// ZenithCore first: every other lib (and every Sentinel) depends on
		// it unconditionally (Zenith_LogFileSinkWrite — see
		// Sharpmake_ZenithCore.cs), so it is the most-depended-on entry here.
		new Entry(typeof(ZenithCoreLibProject)),
		new Entry(typeof(ZenithMathsLibProject)),
		new Entry(typeof(ZenithDataStreamLibProject)),
		new Entry(typeof(ZenithFileAccessLibProject)),

		// Phase 4b's seven, in dependency order (each depends only on entries
		// above it in this list, plus the mandatory ZenithCore edge every
		// entry already carries):
		//   ZenithDebugVariables -> ZenithCore
		//   ZenithProfiling      -> ZenithCore, ZenithDebugVariables
		//   ZenithTaskSystem     -> ZenithCore, ZenithDebugVariables
		//   ZenithInput          -> ZenithCore
		//   ZenithWindows        -> ZenithCore, ZenithInput, ZenithProfiling (win64 ONLY)
		//   ZenithAndroid        -> ZenithCore, ZenithFileAccess, ZenithProfiling, ZenithInput (agde ONLY)
		//   ZenithUnitTests      -> ZenithCore
		// ZenithWindows/ZenithAndroid are the first two entries in this table
		// to restrict their own target set -- see each one's Sharpmake_Zenith*.cs
		// header comment for why a predicate is mandatory here (an
		// unconditional AddProject/AddPublicDependency call THROWS for a
		// target the callee itself never declared).
		new Entry(typeof(ZenithDebugVariablesLibProject)),
		new Entry(typeof(ZenithProfilingLibProject)),
		new Entry(typeof(ZenithTaskSystemLibProject)),
		new Entry(typeof(ZenithInputLibProject)),
		new Entry(typeof(ZenithWindowsLibProject), target => target.Platform == Platform.win64),
		new Entry(typeof(ZenithAndroidLibProject), target => target.Platform == Platform.agde),
		new Entry(typeof(ZenithUnitTestsLibProject)),

		new Entry(typeof(ZenithECSLibProject)),

		// Phase 4c's five, in dependency order (each depends only on
		// entries above it in this list, plus the mandatory ZenithCore
		// edge every entry above carries -- ZenithJolt is the sole
		// exception, see its own header comment for why it carries NO
		// dependency edge at all):
		//   ZenithSaveData  -> ZenithCore, ZenithDataStream, ZenithFileAccess
		//   ZenithTelemetry -> ZenithCore, ZenithDataStream, ZenithMaths, ZenithECS
		//   ZenithPrefab    -> ZenithCore, ZenithDataStream, ZenithMaths, ZenithECS, ZenithProfiling
		//   ZenithScripting -> ZenithCore, ZenithDataStream, ZenithMaths, ZenithECS
		//   ZenithJolt      -> (none)
		// ZenithSaveData does not itself need ZenithECS and could sit
		// anywhere after ZenithFileAccess; it is grouped here with the
		// rest of Phase 4c's additions for locality rather than hunting
		// for the tightest-possible earlier slot.
		new Entry(typeof(ZenithSaveDataLibProject)),
		new Entry(typeof(ZenithTelemetryLibProject)),
		new Entry(typeof(ZenithPrefabLibProject)),
		new Entry(typeof(ZenithScriptingLibProject)),
		new Entry(typeof(ZenithJoltLibProject)),

		// ZenithPhysics now depends on ZenithJolt (Phase 4c split the
		// vendored Jolt tree out of this project into its own leaf), so
		// ZenithJolt's entry above must precede it -- it does.
		new Entry(typeof(ZenithPhysicsLibProject)),
		new Entry(typeof(ZenithAILibProject)),

		// Phase 4d's three, in dependency order (each depends only on
		// entries above it in this list, plus the mandatory ZenithCore edge
		// every entry above carries -- ZenithImGui is a SECOND exception,
		// alongside ZenithJolt, see its own header comment for why it
		// carries no dependency edge at all). ★ PHASE 4i MADE IT FOUR, by
		// splitting Zenith/AssetHandling into two libraries:
		//   ZenithImGui          -> (none)
		//   ZenithAssetRegistry  -> ZenithCore, ZenithMaths, ZenithDataStream,
		//                           ZenithScripting.  NO ZenithFlux edge, ever.
		//   ZenithFlux           -> ZenithCore, ZenithMaths, ZenithDataStream,
		//                           ZenithFileAccess, ZenithProfiling,
		//                           ZenithDebugVariables, ZenithTaskSystem,
		//                           ZenithECS, ZenithAssetRegistry
		//   ZenithAssetHandling  -> ZenithCore, ZenithAssetRegistry, ZenithFlux,
		//                           ZenithMaths, ZenithDataStream
		//
		// ★ THE ORDER OF THESE THREE IS THE WHOLE POINT OF PHASE 4i AND IS NOT
		// COSMETIC. Phase 4d had ZenithAssetHandling BELOW ZenithFlux, because
		// Flux depended on it -- the heavier direction of a genuine header
		// cycle, declared one-way rather than broken. Phase 4i split the
		// directory along the measured Flux-reachability line: the registry
		// half (identifiers, lookup, lifetime -- no header it owns names a Flux
		// type) sits BELOW Flux and is what Flux now depends on, while the GPU
		// half (the 9 units that own Flux types by value) sits ABOVE Flux and
		// depends on it. So ZenithAssetRegistry must precede ZenithFlux, and
		// ZenithAssetHandling must FOLLOW it -- both hold below. The resulting
		// graph is acyclic; do NOT restore a ZenithFlux -> ZenithAssetHandling
		// edge, which would close the cycle Sharpmake will not tolerate. See
		// Sharpmake_ZenithAssetRegistry.cs for the full measurement.
		new Entry(typeof(ZenithImGuiLibProject)),
		new Entry(typeof(ZenithAssetRegistryLibProject)),
		new Entry(typeof(ZenithFluxLibProject)),
		new Entry(typeof(ZenithAssetHandlingLibProject)),

		// Phase 4e's three, in dependency order (each depends only on
		// ZenithCore + ZenithFlux, plus ZenithRenderBackendVulkan's two extra
		// direct edges -- ZenithDebugVariables, ZenithTaskSystem -- see
		// Sharpmake_ZenithRenderBackendVulkan.cs's own header comment).
		// ZenithFlux's entry above must precede all three -- it does. Every
		// entry here is the SECOND target-restriction shape in this table
		// (after ZenithWindows/ZenithAndroid, Phase 4b): a predicate on
		// target.RenderBackend rather than (or, for the two win64-only ones,
		// in addition to) target.Platform. ZenithRenderBackendVulkan's
		// predicate is `RenderBackend == Vulkan` alone (true for win64 AND
		// agde, since every agde target already carries RenderBackend.Vulkan);
		// the other two additionally require Platform.win64 because neither
		// D3D12 nor Null exists as an agde config at all.
		new Entry(typeof(ZenithRenderBackendVulkanLibProject), target => target.RenderBackend == RenderBackend.Vulkan),

		// Phase 4h.3's two, the split of ZenithRenderBackendVulkan's nested
		// Platform/Windows + Platform/Android subdirectories into their own
		// single-file, single-platform leaf libraries (see
		// Sharpmake_ZenithRenderBackendVulkanPlatformWindows.cs / ...Android.cs
		// for the full rationale). Both depend on ZenithRenderBackendVulkan
		// (the entry immediately above), so they must come after it here --
		// they do. ZenithRenderBackendVulkanPlatformWindows additionally
		// depends on ZenithWindows, already registered above this table's
		// Phase 4b block. Both declare ONLY RenderBackend.Vulkan targets
		// (unlike ZenithWindows/ZenithAndroid, which span every
		// RenderBackend on their platform), so
		// ZenithRenderBackendVulkanPlatformWindows' predicate must gate on
		// BOTH Platform.win64 AND RenderBackend.Vulkan -- Platform alone
		// would let a D3D12_/Null_ win64 config try to add a project that
		// never declared that RenderBackend, the exact throw Sharpmake-run
		// caught when this predicate was first written Platform-only.
		// ZenithRenderBackendVulkanPlatformAndroid needs Platform.agde alone
		// (every agde target already carries RenderBackend.Vulkan, and this
		// project only ever declares agde targets to begin with).
		new Entry(typeof(ZenithRenderBackendVulkanPlatformWindowsLibProject), target => target.Platform == Platform.win64 && target.RenderBackend == RenderBackend.Vulkan),
		new Entry(typeof(ZenithRenderBackendVulkanPlatformAndroidLibProject), target => target.Platform == Platform.agde),

		new Entry(typeof(ZenithRenderBackendD3D12LibProject), target => target.RenderBackend == RenderBackend.D3D12 && target.Platform == Platform.win64),
		new Entry(typeof(ZenithRenderBackendNullLibProject), target => target.RenderBackend == RenderBackend.Null && target.Platform == Platform.win64),

		// Phase 4f's two, in dependency order. ZenithUI (layer 5) depends on
		// ZenithCore, ZenithMaths, ZenithDataStream, ZenithAssetHandling,
		// ZenithFlux, ZenithInput, ZenithProfiling -- every one of those
		// entries is already above it. ZenithEntityComponent (layer 6, the
		// top of the measured header DAG) depends on nearly everything
		// below it, INCLUDING ZenithUI -- see Sharpmake_ZenithUI.cs /
		// Sharpmake_ZenithEntityComponent.cs's own header comments for the
		// full edge lists and for the two seams that are deliberately NOT
		// declared edges (EntityComponent<->Editor, and the AI->EntityComponent
		// edge that turned out to be .Tests.inl-only and undeclared already).
		// Neither carries a target restriction -- both declare the full
		// win64 (all RenderBackend/ToolsEnabled/Optimization combos) + agde
		// target set, same as every leaf above.
		new Entry(typeof(ZenithUILibProject)),
		new Entry(typeof(ZenithEntityComponentLibProject)),

		// Phase 4g's two, both win64 x ToolsEnabled.True ONLY -- the THIRD
		// target-restriction shape in this table (after ZenithWindows/
		// ZenithAndroid's Platform-only predicate, Phase 4b, and the three
		// render backends' RenderBackend-only predicate, Phase 4e): a
		// predicate on BOTH target.Platform AND target.ToolsEnabled at once.
		// See Sharpmake_ZenithEditor.cs / Sharpmake_ZenithTools.cs's own
		// header comments for the full edge lists. ZenithEditor (layer 7,
		// above ZenithEntityComponent in the measured header DAG) depends on
		// ZenithWindows and everything ZenithEntityComponent itself depends
		// on except ZenithEntityComponent -- deliberately NOT declared, see
		// that file's own SEAM paragraph. ZenithTools depends on
		// ZenithEntityComponent (a real edge, unlike Editor's) plus
		// ZenithFlux/ZenithAssetHandling/ZenithTaskSystem -- every one of
		// those entries is already above it, so both rows may sit anywhere
		// after ZenithEntityComponentLibProject's own entry.
		new Entry(typeof(ZenithEditorLibProject), target => target.Platform == Platform.win64 && target.ToolsEnabled == ToolsEnabled.True),
		new Entry(typeof(ZenithToolsProject), target => target.Platform == Platform.win64 && target.ToolsEnabled == ToolsEnabled.True),

		new Entry(typeof(ZenithProject)),
	};

	// Adds every applicable engine lib project to a solution (both the
	// engine-only solution and every per-game solution use this).
	public static void AddAllToSolution(Solution.Configuration conf, ZenithTarget target)
	{
		foreach (Entry xEntry in All)
		{
			if (xEntry.Applies(target))
			{
				conf.AddProject(xEntry.ProjectType, target);
			}
		}
	}

	// Adds every applicable engine lib EXCEPT the aggregate ZenithProject
	// itself as a public dependency of the project being configured (the
	// aggregate's own ConfigureAll -- it cannot depend on itself).
	public static void AddAllAsDependenciesOfAggregate(Project.Configuration conf, ZenithTarget target)
	{
		foreach (Entry xEntry in All)
		{
			if (xEntry.ProjectType == typeof(ZenithProject))
			{
				continue;
			}
			if (xEntry.Applies(target))
			{
				conf.AddPublicDependency(target, xEntry.ProjectType);
			}
		}
	}
}
