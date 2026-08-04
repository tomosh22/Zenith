using Sharpmake;
using System;
using System.IO;

// Tools enabled fragment for editor/tools builds
[Fragment, Flags]
public enum ToolsEnabled
{
	True = 1,
	False = 2
}

// Render backend fragment. Selects which Flux backend a config compiles + links:
// Vulkan (the real renderer), D3D12 (a reserved no-op backend kept as the
// link-neutrality proof and the future home of a real D3D12 implementation), or
// Null (the GPU-less backend every headless/CI run executes on). Win64 ships all
// three; agde is Vulkan-only. Appears as a "Vulkan_" / "D3D12_" / "Null_" PREFIX
// on every config name.
[Fragment, Flags]
public enum RenderBackend
{
	Vulkan = 1,
	D3D12 = 2,
	Null = 4
}

// Android ABI axis. The agde target set spans every ABI listed in All, so a
// game's Android build produces one native lib per ABI and Gradle merges them
// into a single multi-ABI APK.
//
// Two spellings of each ABI are load-bearing and they are NOT interchangeable:
//   * ConfigToken -- the underscore form Sharpmake bakes into the config name
//     (e.g. "Vulkan_arm64_v8a_vs2022_Debug_Agde_False"), because Sharpmake
//     derives config-name fragments from the C# enum member names.
//   * DirName -- the canonical Android ABI directory name that AGDE packages
//     from and Gradle's jniLibs expects (jniLibs/<DirName>/lib*.so).
// They coincide for x86_64 and differ for arm64 (arm64_v8a vs arm64-v8a), which
// is exactly the trap that makes hardcoding either one silently wrong.
public static class ZenithAndroidAbi
{
	// Every ABI the agde configs are generated for. arm64-v8a is what real
	// devices run; x86_64 is what the Android emulator runs on an x86_64 host
	// (the QEMU2 emulator cannot host an arm64 guest at all), so it is the only
	// way to exercise the Android build on a dev box with no ARM hardware.
	// Widening the axis is a one-line edit HERE -- every agde project reads it.
	public static Android.AndroidBuildTargets All =>
		Android.AndroidBuildTargets.arm64_v8a | Android.AndroidBuildTargets.x86_64;

	// Config-name fragment token for one ABI (underscore form).
	public static string ConfigToken(Android.AndroidBuildTargets eAbi)
	{
		switch (eAbi)
		{
			case Android.AndroidBuildTargets.arm64_v8a: return "arm64_v8a";
			case Android.AndroidBuildTargets.x86_64:    return "x86_64";
			default: throw new Exception("ZenithAndroidAbi: unsupported ABI '" + eAbi + "'");
		}
	}

	// On-disk ABI directory name for one ABI (jniLibs / AGDE packaging form).
	public static string DirName(Android.AndroidBuildTargets eAbi)
	{
		switch (eAbi)
		{
			case Android.AndroidBuildTargets.arm64_v8a: return "arm64-v8a";
			case Android.AndroidBuildTargets.x86_64:    return "x86_64";
			default: throw new Exception("ZenithAndroidAbi: unsupported ABI '" + eAbi + "'");
		}
	}
}

// Custom target supporting both Windows and Android platforms
public class ZenithTarget : ITarget
{
	public Platform Platform;
	public DevEnv DevEnv;
	public Optimization Optimization;
	public ToolsEnabled ToolsEnabled;
	public RenderBackend RenderBackend;

	// Android-specific fragments (required for AGDE platform)
	public Android.AndroidBuildTargets AndroidBuildTargets;
}

// Base project class with common configuration for all Zenith projects
public abstract class ZenithBaseProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public ZenithBaseProject() : base(typeof(ZenithTarget))
	{
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h", ".inl", ".slang");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");

		// Strip generated-artifact directories from project membership.
		// `.slang` discovery used to pick up Slang's auto-extracted standard
		// modules under `output/.../slang-standard-module-*/*.slang` because
		// those files live below SourceRootPath; their presence varies per
		// configuration and disappears on a clean checkout, so they don't
		// belong in the project. Same applies to obj/, .vs/, .git/, and the
		// AGDE / Gradle build trees under Android/app/build/intermediates —
		// all pure generated state.
		SourceFilesExcludeRegex.Add(@"\\output\\");
		SourceFilesExcludeRegex.Add(@"\\obj\\");
		SourceFilesExcludeRegex.Add(@"\\\.vs\\");
		SourceFilesExcludeRegex.Add(@"\\\.git\\");
		SourceFilesExcludeRegex.Add(@"\\Android\\app\\build\\");
		SourceFilesExcludeRegex.Add(@"\\Android\\\.gradle\\");
	}

	// Configure platform-specific file excludes (called during configuration)
	// Uses conf.SourceFilesBuildExcludeRegex for configuration-level excludes
	protected void ConfigurePlatformExcludes(Configuration conf, ZenithTarget target)
	{
		if (target.Platform == Platform.win64)
		{
			// Windows build: exclude Android platform files from compilation
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Android\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*_Android.*");
		}
		else if (target.Platform == Platform.agde)
		{
			// Android build: exclude Windows platform files and editor/tools from compilation
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Windows\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*_Windows.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Editor\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Tools\\.*");
		}

		// Render-backend source partition: a config compiles exactly one backend's
		// tree. Path-segment based, so the Vulkan exclude also drops the moved
		// platform surface under Zenith/Vulkan/Platform/Windows (and Android).
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\D3D12\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Null\\.*");
		}
		else if (target.RenderBackend == RenderBackend.D3D12)
		{
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Vulkan\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Null\\.*");
			// The ImGui Vulkan backend is a filename, not a path segment, so the
			// Vulkan exclude above misses it; it #includes <vulkan/vulkan.h> which
			// the D3D12 config has no SDK path for. The null backend does no real
			// ImGui rendering, so drop it (a real D3D12 backend would swap in
			// imgui_impl_dx12).
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui_impl_vulkan.*");
		}
		else if (target.RenderBackend == RenderBackend.Null)
		{
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\Vulkan\\.*");
			conf.SourceFilesBuildExcludeRegex.Add(@".*\\D3D12\\.*");
			// Same reasoning as the D3D12 arm: imgui_impl_vulkan is a filename,
			// not a path segment, and the Null config has no Vulkan SDK path.
			conf.SourceFilesBuildExcludeRegex.Add(@".*imgui_impl_vulkan.*");
		}
	}

	protected void ConfigureCommonSettings(Configuration conf, ZenithTarget target)
	{
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);
		conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Enable);
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		conf.Defines.Add("NOMINMAX");

		// Platform defines
		if (target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_WINDOWS");
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Defines.Add("ZENITH_ANDROID");
			// vulkan.hpp: exceptions are disabled on Android, use ResultValue returns
			conf.Defines.Add("VULKAN_HPP_NO_EXCEPTIONS");
			// Set C++20 for AGDE (Options.Vc.Compiler.CppLanguageStandard.CPP20 only works for Win64)
			conf.Options.Add(Options.Agde.Compiler.CppLanguageStandard.Cpp20);
			// Vulkan 1.1 functions require API 30+ (Android 11)
			conf.Options.Add(Options.Android.General.AndroidAPILevel.Android30);
			// Cap VMA at Vulkan 1.1 - Android NDK libvulkan.so doesn't export 1.2/1.3 symbols
			conf.Defines.Add("VMA_VULKAN_VERSION=1001000");
			// Windows-host cross-compile: the AGDE platform emits the include paths in
			// lowercase (e.g. ...\zenith\core, ...\middleware) while the on-disk dirs are
			// Zenith\Core, Middleware. clang's -Wnonportable-include-path then flags every
			// "Core/..."/"Middleware/..." #include as a case mismatch, and the AGDE default
			// -Werror turns it fatal -- a false alarm on the case-INSENSITIVE Windows host
			// (the lowercase paths resolve to the real files fine). Disable ONLY that one
			// warning; every other -Werror diagnostic stays fatal.
			conf.AdditionalCompilerOptions.Add("-Wno-nonportable-include-path");
		}

		// One render-backend define per config; Flux_BackendGuard.h #errors unless
		// exactly one is set. Agde is always Vulkan (its targets carry Vulkan).
		// Explicit three-way (never a fall-through `else`): an unhandled fragment
		// value must be a build error here, not a config that silently compiles
		// the WRONG backend.
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			conf.Defines.Add("ZENITH_VULKAN");
		}
		else if (target.RenderBackend == RenderBackend.D3D12)
		{
			conf.Defines.Add("ZENITH_D3D12");
		}
		else if (target.RenderBackend == RenderBackend.Null)
		{
			// THE build-time headless signal. Null_* configs are what every
			// headless run (CI batches, unit gates) executes.
			conf.Defines.Add("ZENITH_NULL_RENDERER");
		}
		else
		{
			throw new Exception("Unhandled RenderBackend fragment value: " + target.RenderBackend);
		}

		if (target.Optimization == Optimization.Debug)
		{
			conf.Defines.Add("ZENITH_DEBUG");
		}

		// CPU profiler master switch. Enabled in every config that exists today
		// (Debug/Release x Tools/non-tools x win64/agde). A future shipping/Retail axis
		// flips this to 0 here to strip the profiler for zero overhead -- the header
		// (Zenith_Profiling.h) defaults the macro to 1 when undefined, and the hot path
		// (Scope + ZENITH_PROFILING_FUNCTION_WRAPPER) + every subsystem method then
		// compile to nothing. Must be set identically on the base/PCH lib and the
		// engine/game/tool projects (same lockstep rule as ZENITH_TOOLS). NOTE: a
		// Sharpmake regen is required for this define to materialise in the .vcxproj; the
		// header default keeps current (already-generated) builds at 1 until then.
		conf.Defines.Add("ZENITH_PROFILING_ENABLED=1");

		// Memory tracking tier: 2=FULL (Debug) / 1=LITE (Release) / 0=OFF (future
		// shipping/Final). FULL = per-alloc hashmap tracking + guard bytes + callstacks;
		// LITE = lock-free per-category atomic counters only; OFF = straight malloc/free.
		// Same lockstep + regen rule as ZENITH_PROFILING_ENABLED above: it MUST be set
		// identically on the base/PCH lib and the engine/game/tool projects (else
		// sizeof(Zenith_MemoryStats)/operator-inlining ODR mismatch). Zenith.h defaults
		// this off ZENITH_DEBUG when undefined, so already-generated builds hold
		// Debug=2/Release=1 until a regen materialises this define.
		conf.Defines.Add(target.Optimization == Optimization.Debug
			? "ZENITH_MEMORY_TRACKING_LEVEL=2"
			: "ZENITH_MEMORY_TRACKING_LEVEL=1");
	}

	protected void ConfigureCommonIncludePaths(Configuration conf, ZenithTarget target)
	{
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Middleware/stb");
		conf.IncludePaths.Add(RootPath + "/Middleware");

		if (target.Platform == Platform.win64)
		{
			// glfw + the neutral Windows window layer serve both backends.
			conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");
			conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
			if (target.RenderBackend == RenderBackend.Vulkan)
			{
				conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
				conf.IncludePaths.Add(RootPath + "/Middleware/slang/include");
			}
			else if (target.RenderBackend == RenderBackend.D3D12)
			{
				// Null D3D12 backend headers (the d3d12/dxgi system headers ship
				// with the Windows SDK on the default include path).
				conf.IncludePaths.Add(RootPath + "/Zenith/D3D12");
			}
			else if (target.RenderBackend == RenderBackend.Null)
			{
				// The GPU-less Null backend: headers only, no graphics SDK at all.
				conf.IncludePaths.Add(RootPath + "/Zenith/Null");
			}
		}
		else if (target.Platform == Platform.agde)
		{
			conf.IncludePaths.Add(RootPath + "/Zenith/Android");
			// android_native_app_glue.h lives in NativeGlue subfolder
			conf.IncludePaths.Add(RootPath + "/Zenith/Android/NativeGlue");
			// Vulkan C++ headers (vulkan.hpp) from SDK - header-only, works with NDK's Vulkan
			conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
		}
	}

	// True for projects that actually link the renderer backend (the aggregate
	// engine lib + the games). FALSE for the L0/L1 leaf static libs (ZenithBase,
	// ZenithECS) and the SentinelECS leaf-proof exe: those are renderer-agnostic
	// and must NOT link OR PROPAGATE glfw/vulkan/slang. Propagating those libs out
	// of the leaf static libs is exactly what let the SentinelECS link silently
	// resolve an accidental ECS->renderer reference, defeating the leaf proof.
	// (win64-only knob; agde links its system libs via Sharpmake_Games.cs.)
	protected virtual bool LinksRendererBackend => true;

	protected void ConfigureCommonLibraryPaths(Configuration conf, ZenithTarget target)
	{
		if (target.Platform == Platform.win64 && LinksRendererBackend)
		{
			// glfw is the windowing backend for both renderers.
			conf.LibraryPaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
			conf.LibraryFiles.Add("glfw3_mt.lib");

			if (target.RenderBackend == RenderBackend.Vulkan)
			{
				conf.LibraryPaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Lib");
				conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
				conf.LibraryFiles.Add("vulkan-1.lib");
				conf.LibraryFiles.Add("slang.lib");
			}
			else if (target.RenderBackend == RenderBackend.D3D12)
			{
				// Null D3D12 backend links the system D3D12 import libs (they ship
				// with the Windows SDK on the default library search path).
				conf.LibraryFiles.Add("d3d12.lib");
				conf.LibraryFiles.Add("dxgi.lib");
				conf.LibraryFiles.Add("dxguid.lib");
			}
			// RenderBackend.Null deliberately adds NOTHING here: no vulkan-1/slang,
			// no d3d12/dxgi/dxguid. glfw3_mt above is the only graphics-adjacent lib
			// it links (the window/input layer is shared by every backend).

			// glfw3_mt.lib is compiled with /MT (release CRT), which pulls in LIBCMT.
			// In debug builds we use /MTd (LIBCMTD), causing a linker conflict.
			if (target.Optimization == Optimization.Debug)
			{
				conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
			}
		}
		// Android links against system Vulkan loader via --sysroot (see Sharpmake_Games.cs)
	}
}
