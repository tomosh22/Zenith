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

	// ConfigurePlatformExcludes -- DELETED (Phase 4h.3). It used to
	// conf.SourceFilesBuildExcludeRegex-strip \Android\/_Android files on
	// win64 and \Windows\/_Windows files on agde, called from every single
	// project's own ConfigureAll (~40 call sites). Phase 4h measured every
	// generated .vcxproj against all four patterns and found exactly ONE
	// call site where any of them ever matched a member file:
	// ZenithRenderBackendVulkan, whose SourceRootPath (RenderBackends/Vulkan)
	// held Platform/Windows/ and Platform/Android/ side by side while the
	// project itself spanned both win64 and agde. Every other call site was
	// dead on arrival -- either the project's own directory never contained
	// a Windows/Android-named or -pathed file at all (the vast majority),
	// or -- for ZenithWindows/ZenithAndroid themselves -- the project only
	// ever configured the platform whose OWN files matched, so the OPPOSITE
	// exclusion arm (the only one that could ever have found anything) never
	// ran for it. (imgui_impl_android.cpp was a partial exception: it WAS
	// live on win64 via this method's `.*_Android.*` arm, until Phase 4h.1
	// converted its ownership to a constructor MEMBERSHIP exclude on the
	// aggregate instead -- see Sharpmake_Zenith.cs.)
	//
	// The fix for the one genuine case was the same rule this whole refactor
	// has applied everywhere else: one library per directory, declaring only
	// the targets it supports. Zenith/RenderBackends/Vulkan/Platform/{Windows,
	// Android} are now their own leaf libraries
	// (Sharpmake_ZenithRenderBackendVulkanPlatformWindows.cs / ...Android.cs),
	// membership-excluded from ZenithRenderBackendVulkan's own constructor,
	// with conditionality expressed as target declaration + solution
	// membership (Build/Sharpmake_EngineLibs.cs) rather than a per-file
	// regex. With that split done, every call site in the codebase was dead,
	// and every call site was removed (see each project's own ConfigureAll
	// for a one-line comment recording what was measured). This method is
	// the last piece: it now has no caller left. See Docs/BuildSystem.md §4
	// for the invariant this completes -- every compiled source belongs to
	// exactly one project per link target, expressed as declaration and
	// membership, never as a per-configuration subtraction.

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

		// One render-backend define per config; Core/Zenith_BackendAliases.h #errors unless
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
				// Vulkan backend headers. Kept explicit (and symmetric with the D3D12/Null
				// arms below) as a safety net for any bare-basename include that resolves
				// through this directory being directly on the include path.
				conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends/Vulkan");
			}
			else if (target.RenderBackend == RenderBackend.D3D12)
			{
				// Null D3D12 backend headers (the d3d12/dxgi system headers ship
				// with the Windows SDK on the default include path).
				conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends/D3D12");
			}
			else if (target.RenderBackend == RenderBackend.Null)
			{
				// The GPU-less Null backend: headers only, no graphics SDK at all.
				conf.IncludePaths.Add(RootPath + "/Zenith/RenderBackends/Null");
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

	// ---------------------------------------------------------------------
	// Explicit, additive per-subsystem library helpers.
	//
	// This replaces the old `LinksRendererBackend` virtual bool (a single
	// switch that conflated glfw/vulkan/slang/d3d12 into one on/off knob,
	// consulted by a since-removed ConfigureCommonLibraryPaths) with one call
	// per subsystem. A project now links exactly the libraries it calls the
	// helper for -- there is no bool to remember to override, and a leaf lib
	// that must not link or propagate glfw/vulkan/slang (ZenithCore, ZenithMaths,
	// ZenithDataStream, ZenithFileAccess, ZenithECS, ZenithPhysics, ZenithAI,
	// and the Sentinel* leaf-proof exes) simply calls
	// none of these helpers. That is structural purity: a project that never
	// calls AddVulkanBackendLibs cannot acquire vulkan-1.lib by accident, the
	// same guarantee the old subtractive bool existed to protect (an
	// accidental ECS->renderer edge at SentinelECS would now fail the link
	// with an undefined symbol, not silently succeed because some override
	// was forgotten).
	//
	// Every helper is win64-only, matching the original single guard exactly
	// (Android links its system Vulkan loader via --sysroot -- see
	// Sharpmake_Games.cs -- so none of these ever apply to agde).
	//
	// FORWARD-LOOKING NOTE (do not act on this yet -- it belongs to a later
	// phase that gives Flux and Windows their own static libraries):
	//   * AddSlangLibs will eventually be owned by the Flux library, but it
	//     must stay conditional on win64 && RenderBackend.Vulkan -- the Null
	//     and D3D12 configs have no slang lib path at all, and the one TU that
	//     calls the slang API (Zenith/Flux/Slang/Flux_SlangCompiler.cpp) is
	//     already guarded by `#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)`.
	//   * AddGlfwLibs is called directly (i.e. GLFW's own API, not just this
	//     helper) from four directories today: Zenith/Windows/ (2 files),
	//     Zenith/Input/ (2 files), the Vulkan backend (3 files), and -- until
	//     Phase 2 moved it out of Core -- Zenith_Core.cpp. Owning
	//     glfw3_mt.lib on a future ZenithWindows lib will leave ZenithInput
	//     and the Vulkan backend with undeclared win64 symbols that would
	//     only resolve at the consuming exe's link -- a future phase must
	//     give those libraries their own AddGlfwLibs call too, not assume the
	//     engine-lib call alone still covers every glfw-calling TU.
	// ---------------------------------------------------------------------

	// glfw: the windowing backend shared by every render backend (Vulkan,
	// D3D12, and Null all link it -- only the graphics-API import libs differ
	// below). Also carries the LIBCMT ignore, which exists solely because
	// glfw3_mt.lib is built /MT.
	protected void AddGlfwLibs(Configuration conf, ZenithTarget target)
	{
		if (target.Platform != Platform.win64) { return; }

		conf.LibraryPaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
		conf.LibraryFiles.Add("glfw3_mt.lib");

		// glfw3_mt.lib is compiled with /MT (release CRT), which pulls in LIBCMT.
		// In debug builds we use /MTd (LIBCMTD), causing a linker conflict.
		if (target.Optimization == Optimization.Debug)
		{
			conf.Options.Add(new Options.Vc.Linker.IgnoreSpecificLibraryNames("LIBCMT"));
		}
	}

	// The Vulkan render backend's loader import lib.
	protected void AddVulkanBackendLibs(Configuration conf, ZenithTarget target)
	{
		if (target.Platform != Platform.win64) { return; }

		conf.LibraryPaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Lib");
		conf.LibraryFiles.Add("vulkan-1.lib");
	}

	// Flux's Slang shader-compiler dependency. Only the Vulkan backend calls
	// into Slang today (see the forward-looking note above), so every call
	// site pairs this with AddVulkanBackendLibs under the same Vulkan guard.
	protected void AddSlangLibs(Configuration conf, ZenithTarget target)
	{
		if (target.Platform != Platform.win64) { return; }

		conf.LibraryPaths.Add(RootPath + "/Middleware/slang/lib");
		conf.LibraryFiles.Add("slang.lib");
	}

	// The D3D12 render backend's system import libs. These ship with the
	// Windows SDK on the default library search path, so no LibraryPaths
	// entry is needed (matches the original ConfigureCommonLibraryPaths,
	// which never added a D3D12 lib path either).
	protected void AddD3D12Libs(Configuration conf, ZenithTarget target)
	{
		if (target.Platform != Platform.win64) { return; }

		conf.LibraryFiles.Add("d3d12.lib");
		conf.LibraryFiles.Add("dxgi.lib");
		conf.LibraryFiles.Add("dxguid.lib");
	}
}
