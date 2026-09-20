using Sharpmake;
using System;
using System.IO;

// SentinelUI — leaf-proof executable for ZenithUI (Phase 5.2).
//
// Proves ZenithUI is a self-contained leaf over its declared dependency set
// — ZenithCore + ZenithMaths + ZenithDataStream + ZenithAssetHandling +
// ZenithFlux + ZenithInput + ZenithProfiling (see Sharpmake_ZenithUI.cs's own
// DEPENDENCY paragraph) — with NO undefined ENGINE externals and, critically,
// no EntityComponent edge: ZenithUI sits directly below ZenithEntityComponent
// in the measured header DAG (Zenith_UIComponent.h #includes UI/Zenith_UI.h,
// never the reverse), so this sentinel is also the mechanical check that the
// "UI never reaches EntityComponent" allowlist entry in
// Tools/architecture_allowlist.txt stays true at LINK time, not just at the
// architecture gate's static-analysis level. It links EXACTLY zenithui.lib +
// the seven leaves it directly declares, PLUS whatever ZenithFlux itself
// transitively re-exports (ZenithECS, ZenithTaskSystem, ZenithDebugVariables
// — Flux's own declared set, unavoidable and correct: UI depends on Flux,
// Flux genuinely depends on those) — and NOTHING else. The leaves that matter
// for THIS proof are the ones that stay absent regardless: no
// ZenithScripting, no ZenithEntityComponent, no ZenithEditor, no
// ZenithProject, no game. Its
// single TU (Tests/SentinelUI/main.cpp) drives ZenithUI with a real,
// out-of-line class (Zenith_UIRect, constructed with no parent canvas) plus
// a Flux leaf touch (Zenith_RenderBus::RecordDrawCall, the same GPU-free real
// call Tests/SentinelFlux/main.cpp uses) without ever naming g_xEngine, a
// concrete component, or a live GPU/window. If ZenithUI secretly reached
// EntityComponent (or anything else outside its declared set), THIS link
// would fail with an unresolved external — so a green SentinelUI build IS
// the leaf proof.
//
// Modelled on Sharpmake_SentinelECS.cs (same Exe / win64-False-only shape,
// same PCH-as-plain-include single-TU handling). Unlike SentinelFlux, this
// sentinel does NOT add a render-backend dependency: Sharpmake_ZenithUI.cs's
// own declared dependency list carries no backend edge either (ZenithUI
// reaches Flux only through Flux's own backend-agnostic surface — see that
// file's header comment), so adding one here would overstate what ZenithUI
// itself declares.
[Sharpmake.Generate]
public class SentinelUIProject : ZenithBaseProject
{
	public SentinelUIProject()
	{
		Name = "SentinelUI";
		SourceRootPath = RootPath + "/Tests/SentinelUI";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.IntermediatePath = @"[conf.ProjectPath]\obj\SentinelUI\[target.Platform]\[conf.Name]";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		// No AddGlfwLibs/AddVulkanBackendLibs/AddSlangLibs/AddD3D12Libs call:
		// this leaf-proof exe must not link or propagate the renderer backend
		// — matches ZenithUI's own declared dependency set, which names no
		// backend either.

		// PCH header transitively pulls imgui + Flux/Vulkan/vma/Jolt headers,
		// and ZenithUI's own headers (transitively reached through
		// UI/Zenith_UI.h) reference Flux/imgui types directly — include paths
		// only, no libs linked beyond the declared dependencies below.
		conf.IncludePaths.Add(RootPath + "/Middleware/vma");
		conf.IncludePaths.Add(RootPath + "/Middleware/JoltPhysics-5.4.0");
		conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");

		// SHADER_SOURCE_ROOT — needed for the same reason
		// Sharpmake_ZenithUI.cs documents: the shared Zenith.h PCH chain
		// transitively reaches the active backend's Pipeline header, which
		// references it unguarded.
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');
		string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");

		conf.Output = Configuration.OutputType.Exe;

		// THE WHOLE POINT: link EXACTLY ZenithUI + its own seven declared
		// leaves, nothing more — no ZenithECS, no ZenithScripting, no
		// ZenithEntityComponent, no ZenithEditor.
		conf.AddPublicDependency<ZenithUILibProject>(target);
		conf.AddPublicDependency<ZenithMathsLibProject>(target);
		conf.AddPublicDependency<ZenithDataStreamLibProject>(target);
		conf.AddPublicDependency<ZenithAssetHandlingLibProject>(target);
		conf.AddPublicDependency<ZenithFluxLibProject>(target);
		conf.AddPublicDependency<ZenithInputLibProject>(target);
		conf.AddPublicDependency<ZenithProfilingLibProject>(target);
		conf.AddPublicDependency<ZenithCoreLibProject>(target);

		// No ZENITH_TOOLS branch: this project is ToolsEnabled.False only.
	}
}
