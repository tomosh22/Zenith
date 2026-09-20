using Sharpmake;
using System;
using System.IO;

// ZenithAssetHandling — the UPPER (GPU-resource) half of the
// Zenith/AssetHandling split (Phase 4i). The lower half is
// Sharpmake_ZenithAssetRegistry.cs, which carries the FULL rationale for the
// split, the measured split line and the membership mechanism; read that file
// first. This comment covers only what is specific to this side.
//
// ═════════════════════════════════════════════════════════════════════════
// WHAT CHANGED (Phase 4i)
// ═════════════════════════════════════════════════════════════════════════
// Phase 4d gave this project all 21 TUs of Zenith/AssetHandling and declared
// the genuine header cycle with Flux one-way (ZenithFlux -> ZenithAssetHandling,
// the heavier direction) while this project deliberately did NOT depend back.
// The cycle was DECLARED AROUND, not broken.
//
// Phase 4i broke it, by splitting the directory rather than by moving files:
//
//   ZenithAssetRegistry  (new, below Flux)  — identifiers, lookup, lifetime.
//                                             No Flux type in any header it owns.
//   ZenithAssetHandling  (this, above Flux) — the 9 units whose .h or .cpp
//                                             reaches a Flux/** header.
//
// Declared graph: ZenithAssetRegistry <- ZenithFlux <- ZenithAssetHandling.
// Acyclic. THE EDGE BELOW IS NOW DECLARED: this project AddPublicDependency's
// ZenithFlux, which Phase 4d could not do because it would have closed the
// cycle. It is the same shape as EntityComponent -> Flux and
// backend -> Flux, both already declared one-way in this engine.
//
// Flux's own #includes of the nine headers HERE stay include-path-only (the
// plain "/Zenith" path every leaf carries via ConfigureCommonIncludePaths); any
// out-of-line symbol they call resolves at the final game exe link, where this
// library is always present (a mandatory row in Sharpmake_EngineLibs.cs). Do
// NOT "complete" the graph by adding ZenithFlux -> ZenithAssetHandling back:
// Sharpmake will not tolerate the cycle, and the exe link does not need it.
//
// ═════════════════════════════════════════════════════════════════════════
// MEMBERSHIP — 9 UNITS, SHARED SOURCE ROOT, NOTHING MOVED
// ═════════════════════════════════════════════════════════════════════════
// This project owns exactly the nine units whose .h OR .cpp reaches Flux:
//
//   HEADER reaches Flux (owns a Flux type BY VALUE — not forward-declarable):
//     Zenith_TextureAsset          .h -> Flux/Flux.h                                      (Flux_SurfaceInfo, Flux_VRAMHandle, Flux_ShaderResourceView)
//     Zenith_MeshAsset             .h -> Flux/Flux_Buffers.h                              (Flux_VertexBuffer, Flux_IndexBuffer, Flux_BufferLayout)
//     Zenith_AnimatorControllerAsset .h -> Flux/MeshAnimation/Flux_AnimatorControllerDef.h  (Flux_AnimatorControllerDef)
//     Zenith_GrassTypeTableAsset   .h -> Flux/Vegetation/Flux_GrassTypeTable.h            (Flux_GrassTypeTable)
//       ^ ★ THE ONE TO WATCH. This TU self-registers at file scope, and its
//         dead-strip anchor -- Zenith_GrassTypeTableAsset_ForceLink() -- is
//         called from Flux/Vegetation/Flux_Grass.cpp, i.e. from ZenithFlux,
//         which after Phase 4i sits BELOW the library defining it. That is
//         sound (the call IS the undefined external that pulls the object, and
//         link.exe resolves across every archive on the exe line), but this TU
//         hosts NO .Tests.inl -- so if it were ever dead-stripped the unit
//         count would not move and no gate would say a word. Zenith_Behaviour-
//         GraphAsset.cpp is the only other file-scope self-registrar here, and
//         its _ForceLink() caller (Zenith_GraphComponent) is above both halves.
//
//   .cpp ONLY reaches Flux (header is clean, but the TU creates a link edge):
//     Zenith_AnimationAsset        .cpp -> Flux/MeshAnimation/Flux_AnimationClip.h
//     Zenith_BoneMaskAsset         .cpp -> Flux/MeshAnimation/Flux_BonePose.h
//     Zenith_FontAsset             .cpp -> Flux/Text/Flux_TextImpl.h
//     Zenith_MaterialAsset         .cpp -> Flux/Flux_MaterialTable.h
//     Zenith_MeshGeometryAsset     .cpp -> Flux/MeshGeometry/Flux_MeshGeometry.h
//
//   ( + Zenith_TextureAsset.cpp -> Flux/Flux_GraphicsImpl.h and
//       Zenith_MeshAsset.cpp   -> Flux/MeshGeometry/Flux_MeshInstance.h, whose
//       headers already put them here. )
//
// NOT A SINGLE FILE MOVED and not a single #include spelling changed — 251
// files across Zenith/, Games/, Tools/ and Tests/ #include
// "AssetHandling/<something>.h". The two projects SHARE the
// Zenith/AssetHandling source root and partition it with constructor-time
// SourceFilesExcludeRegex (a MEMBERSHIP exclude — never
// conf.SourceFilesBuildExcludeRegex, the per-configuration BUILD exclude whose
// call-site count Phase 4h.3 drove to zero and which
// Tools/check_project_membership.ps1 now fails on outright).
//
// The two exclude lists are exact COMPLEMENTS: every file under
// Zenith/AssetHandling is owned by exactly one project, each .Tests.inl follows
// its own .cpp, and each project carries its own definition-free PCH create TU
// under Internal/. Adding a unit to this directory means adding it to exactly
// one of the two lists — otherwise check_project_membership.ps1's UNOWNED check
// (fatal) will say so.
//
// ★ Zenith_TextureAsset and Zenith_MeshAsset are owned WHOLESALE, by design.
// Separating their by-value Flux members from their CPU data would be a TYPE
// split, not a library split; the Definition of Done forbids a cycle only with
// the registry half, and both classes belong entirely on this side.
//
// ═════════════════════════════════════════════════════════════════════════
// DEPENDENCY (direct #include edges, the "declare what is directly used"
// convention every Phase 4b/4c/4d leaf follows)
// ═════════════════════════════════════════════════════════════════════════
//   ZenithCore         — MANDATORY for every Zenith library (Zenith_LogFileSinkWrite;
//                        see Sharpmake_ZenithCore.cs). Also direct:
//                        Zenith_FontAsset.cpp / Zenith_MeshAsset.cpp /
//                        Zenith_MeshGeometryAsset.cpp / Zenith_TextureAsset.cpp
//                        #include Core/Zenith_Engine.h.
//   ZenithAssetRegistry— every one of the nine headers here #includes
//                        AssetHandling/Zenith_Asset.h, and several also
//                        Zenith_AssetHandle.h / Zenith_AssetRegistry.h /
//                        Zenith_AssetTypeIds.h / Zenith_MaterialParamTable.h.
//   ZenithFlux         — the nine Flux/** includes listed above. THE EDGE PHASE
//                        4D COULD NOT DECLARE.
//   ZenithMaths        — Zenith_FontAsset.h, Zenith_MaterialAsset.h,
//                        Zenith_MeshAsset.h, Zenith_MeshGeometryAsset.h.
//   ZenithDataStream   — Zenith_AnimatorControllerAsset.cpp, Zenith_BoneMaskAsset.cpp,
//                        Zenith_FontAsset.cpp, Zenith_MaterialAsset.cpp,
//                        Zenith_MeshAsset.cpp, Zenith_TextureAsset.cpp.
// No ZenithScripting edge any more: the one file that needed it
// (Zenith_BehaviourGraphAsset.h) is on the registry side now, and this project
// still reaches ZenithScripting transitively through ZenithAssetRegistry's
// public edge. No ZenithECS edge: nothing under Zenith/AssetHandling
// #includes ZenithECS. Zenith_MaterialAsset.cpp #includes
// FileAccess/Zenith_FileAccess.h and four TUs #include
// Profiling/Zenith_Profiling.h; neither edge is declared, exactly as the
// undivided project did not declare them before this split — include-path-only,
// resolved at the exe link, and reachable transitively through ZenithFlux in
// any case. Left unchanged deliberately: adding edges is a separate question
// from breaking the cycle.
//
// The class is named *Lib* to avoid colliding with the abstract base
// `ZenithBaseProject` in Sharpmake_Common.cs; the OUTPUT lib is named
// "ZenithAssetHandling".
[Sharpmake.Generate]
public class ZenithAssetHandlingLibProject : ZenithBaseProject
{
	// This half DOES depend on ZenithFlux, but ZenithFlux is itself
	// renderer-agnostic — so, like ZenithFlux, this project must still not link
	// or propagate glfw/vulkan/slang directly: ConfigureAll below calls none of
	// the AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs helpers.
	public ZenithAssetHandlingLibProject()
	{
		Name = "ZenithAssetHandling";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan,
			AndroidBuildTargets = ZenithAndroidAbi.All
		});

		SourceRootPath = RootPath + "/Zenith/AssetHandling";

		// MEMBERSHIP: own the 9 GPU units + the pre-existing
		// Internal/Zenith_AssetHandlingPCH.cpp. Everything below belongs to
		// ZenithAssetRegistry — this list and that project's are exact
		// complements.
		//
		// ★ EVERY PATTERN ENDS IN `\.` ON PURPOSE. "Zenith_Asset" is a prefix of
		// Zenith_AssetHandle, Zenith_AssetRegistry, Zenith_AssetTypeIds AND
		// Zenith_AssetHandlingPCH; "Zenith_AssetRegistry" is a prefix of
		// Zenith_AssetRegistryPCH. Requiring the dot immediately after the unit
		// name keeps each pattern to its own unit's {.h, .cpp, .inl,
		// .Tests.inl} set and nothing else.
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_Asset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_AssetHandle\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_AssetRegistry\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_AssetTypeIds\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_BehaviourGraphAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_FileWatcher\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_HumanProportions\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_Image\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_MaterialParamTable\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_ModelAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_PropertyTuning\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_SkeletonAsset\.");
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Zenith_SkinDeform\.");

		// The OTHER project's PCH create TU.
		SourceFilesExcludeRegex.Add(@"AssetHandling\\Internal\\Zenith_AssetRegistryPCH\.cpp");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Phase 4h.3 -- ConfigurePlatformExcludes() call REMOVED: measured against the regenerated .vcxproj (Phase 4h.2), this project has zero members matching \Android\, _Android, \Windows\ or _Windows. See Sharpmake_Common.cs for the method's removal.

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// DISTINCT intermediate path (unchanged from Phase 4d, and now
		// load-bearing in a second way: ZenithAssetRegistry shares this source
		// root, and two projects creating Zenith.pch in one obj dir is MSB8028).
		conf.IntermediatePath = @"[conf.ProjectPath]\obj\ZenithAssetHandling\[target.Platform]\[conf.Name]";

		// Per-lib PCH: 9 compiled TUs clears the ">= 4 TUs justifies a binary
		// PCH" rule. Internal/Zenith_AssetHandlingPCH.cpp is a one-line
		// `#include "Zenith.h"` and MUST stay definition-free -- see that file's
		// own header comment for why.
		conf.PrecompHeader = "Zenith.h";
		conf.PrecompSource = "Zenith_AssetHandlingPCH.cpp";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// see the constructor comment.

		// The PCH (Zenith.h) transitively pulls imgui + Flux/Vulkan/vma/Jolt
		// headers, and SEVEN of the nine TUs here #include Flux/** headers
		// directly (see the file header comment). The other two --
		// Zenith_AnimatorControllerAsset.cpp and Zenith_GrassTypeTableAsset.cpp
		// -- reach Flux only through their OWN headers, which is why they are
		// on this side of the split at all. Include paths only, no libs linked.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT -- found via a Null_ config build (C2065
		// 'SHADER_SOURCE_ROOT': undeclared identifier), exactly the defect
		// class Sharpmake_ZenithDebugVariables.cs already documents:
		// Zenith_TextureAsset.cpp #includes Flux/Flux_GraphicsImpl.h directly,
		// which reaches the active backend's Pipeline header
		// (Zenith_Null_Pipeline.h / Zenith_D3D12_Pipeline.h), and both
		// reference SHADER_SOURCE_ROOT directly, unguarded. This define
		// previously existed only on the aggregate ZenithProject and each
		// GameProject, neither of which this standalone leaf lib is. Mirrors
		// ZenithProject's own identical win64/agde branch exactly (same value,
		// same "" on agde).
		//
		// ★ The SAME C2065 was also reported against Zenith_SkinDeform.cpp,
		// which Phase 4i put on the REGISTRY side -- so
		// Sharpmake_ZenithAssetRegistry.cs carries this identical block too.
		// Both halves need it; neither inherits it from the other.
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Defines.Add("SHADER_SOURCE_ROOT=\"\"");
		}

		conf.Output = Configuration.OutputType.Lib;

		// MANDATORY: ZenithCore -- see Sharpmake_ZenithCore.cs's header comment.
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// Direct edges -- see the file header comment's DEPENDENCY paragraph.
		conf.AddPublicDependency<ZenithAssetRegistryLibProject>(target);

		// THE EDGE PHASE 4D COULD NOT DECLARE. Safe now precisely because
		// ZenithFlux depends on ZenithAssetRegistry (the lower half) rather
		// than on this project -- see Sharpmake_ZenithFlux.cs. Never add
		// ZenithFlux -> ZenithAssetHandling back.
		conf.AddPublicDependency<ZenithFluxLibProject>(target);

		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);

		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");

			conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
			conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

			conf.PrecompSourceExcludeFolders.Add(RootPath + "/Tools");
		}
	}
}
