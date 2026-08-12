using Sharpmake;
using System;
using System.IO;

// =============================================================================
// Game build-system base classes.
//
// The concrete per-game project + solution classes are NO LONGER hand-written
// here. They are generated into Build/Sharpmake_GameInstances.generated.cs from
// Games/<Name>/<Name>.zproj descriptors by Build/zenith_buildsystem.psm1
// (Invoke-ZenithCodegen), which regen.ps1 runs before Sharpmake. Adding or
// removing a game touches only its descriptor -- never this file.
//
// This file holds the two ABSTRACT bases the generated shells derive from:
//   * GameProject  -- the game .exe/.so (was the old [Generate] GameProject with
//                     a hardcoded default; now abstract, name/android supplied by
//                     the concrete subclass).
//   * GameSolution -- the per-game .sln (engine + this one game + its extras).
// =============================================================================

// Game project - the executable that links the Zenith engine. Abstract: the
// concrete GameName / HasAndroid / ExtraDefines come from the generated subclass.
public abstract class GameProject : ZenithBaseProject
{
	// The game's name -- supplied by the generated subclass. Drives the source
	// root, project name, output paths, and all asset-path defines.
	public abstract string GameName { get; }

	// True iff this game ships an Android (AGDE) build. Games without a Gradle
	// tree (Games/<Name>/Android) leave this false so no agde target is emitted.
	public virtual bool HasAndroid => false;

	// Descriptor escape hatch: extra preprocessor defines for this game. Empty
	// for every game today; kept so the first non-uniform game needs no C# edit.
	public virtual string[] ExtraDefines => new string[0];

	public GameProject()
	{
		Name = GameName;
		SourceRootPath = RootPath + "/Games/" + GameName;

		// Windows target: both backends, both tools variants, Debug + Release.
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		// Android target only for games that actually have a Gradle project.
		if (HasAndroid)
		{
			AddTargets(new ZenithTarget
			{
				Platform = Platform.agde,
				DevEnv = DevEnv.vs2022,
				Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.False,
				RenderBackend = RenderBackend.Vulkan,
				AndroidBuildTargets = ZenithAndroidAbi.All
			});
		}
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		// Configure platform-specific excludes
		ConfigurePlatformExcludes(conf, target);

		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = RootPath + "/Games/" + GameName + "/Build";

		ConfigureCommonSettings(conf, target);
		ConfigureCommonIncludePaths(conf, target);
		ConfigureCommonLibraryPaths(conf, target);

		// Game include paths -- BOTH are load-bearing:
		//   * Games/<GameName> : this game's own root.
		//   * Games/           : games #include their own headers via the
		//     "<GameName>/Sub/Header.h" form (e.g. Sokoban.cpp does
		//     #include "Sokoban/Components/Sokoban_GameComponent.h"), which resolves
		//     ONLY through this parent dir. Dropping it breaks 11/12 games with
		//     C1083 (verified 2026-07-06). The theoretical cross-game-coupling
		//     surface is tolerated: rewriting every game's include style to shed it
		//     is a large, out-of-scope change.
		conf.IncludePaths.Add(RootPath + "/Games/" + GameName);
		conf.IncludePaths.Add(RootPath + "/Games");

		// Zenith root path - absolute path computed from Sharpmake location
		// This is the only absolute path define; all asset paths are constructed from it
		string zenithRoot = new DirectoryInfo(SharpmakeCsPath).Parent.FullName.Replace('\\', '/');
		conf.Defines.Add($"ZENITH_ROOT=\"{zenithRoot}/\"");

		// Asset paths constructed from ZENITH_ROOT
		string gameAssetRoot = zenithRoot + "/Games/" + GameName + "/Assets/";
		string engineAssetRoot = zenithRoot + "/Zenith/Assets/";

		if (target.Platform == Platform.agde)
		{
			// On Android, assets are bundled into the APK by Gradle.
			// AAssetManager expects relative paths within the APK's assets directory.
			// Empty prefix: "" "Levels/level_0001.tlvl" = "Levels/level_0001.tlvl"
			conf.Defines.Add("GAME_ASSETS_DIR=\"\"");
			conf.Defines.Add("ENGINE_ASSETS_DIR=\"\"");
		}
		else
		{
			conf.Defines.Add($"GAME_ASSETS_DIR=\"{gameAssetRoot}\"");
			conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");
		}

		// Shader source path for runtime shader compilation (Windows only)
		if (target.Platform == Platform.win64)
		{
			string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
			conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		}

		// Enable tools for game projects when ToolsEnabled is True
		if (target.ToolsEnabled == ToolsEnabled.True && target.Platform == Platform.win64)
		{
			conf.Defines.Add("ZENITH_TOOLS");
		}

		// Descriptor-supplied extra defines (escape hatch). Empty for all games
		// today, so this loop is a no-op and leaves the generated vcxproj unchanged.
		foreach (string strExtraDefine in ExtraDefines)
		{
			conf.Defines.Add(strExtraDefine);
		}

		// Output executable
		if (target.Platform == Platform.win64)
		{
			conf.Output = Configuration.OutputType.Exe;
		}
		else if (target.Platform == Platform.agde)
		{
			conf.Output = Configuration.OutputType.Dll; // Shared library for Android

			// Android system libraries (shared, link with -l flags)
			conf.AdditionalLinkerOptions.Add("-landroid");
			conf.AdditionalLinkerOptions.Add("-llog");
			conf.AdditionalLinkerOptions.Add("-lvulkan");

			// Force-include ANativeActivity_onCreate from libzenith.a
			// (linker would otherwise discard it since game code doesn't reference it directly)
			conf.AdditionalLinkerOptions.Add("-u ANativeActivity_onCreate");

			// 16 KB page-size alignment (Google Play requirement, AGDE1112 without
			// it). lld defaults to a 4 KB ELF segment alignment; ask for 16 KB
			// explicitly so libzenithmon.so itself is compatible with 16 KB-page
			// devices, not just correctly built. Only matters for the final linked
			// .so (this project), not the engine's static .a libs.
			conf.AdditionalLinkerOptions.Add("-Wl,-z,max-page-size=16384");

			// APK packaging - point to Gradle project in Games/<GameName>/Android/
			conf.CustomProperties.Add("AndroidEnablePackaging", "true");
			conf.CustomProperties.Add("AndroidGradleBuildDir", zenithRoot + "/Games/" + GameName + "/Android");
			conf.CustomProperties.Add("AndroidApplicationModule", "app");

			// Static STL, not the NDK's shared one. Each game APK links exactly ONE
			// native shared library (this one) -- Google's own guidance is static STL
			// for that case (shared STL only earns its keep when multiple independent
			// .so's in the same process need to share one copy of the runtime, which
			// we never do). This also removes the vendored NDK's prebuilt
			// libc++_shared.so from the APK entirely, which otherwise triggers Google
			// Play's 16 KB page-size alignment warning (AGDE1112) -- that .so is
			// prebuilt by the NDK, not something we compile, so we can't re-align it
			// ourselves; not packaging it at all sidesteps the warning instead.
			// Overrides the Sharpmake/AGDE default of cpp_shared, same mechanism as
			// the OutDir override below (last PropertyGroup wins).
			//
			// ★ THE TOKEN IS "cpp_static", NOT "c++_static". This MSBuild property
			// (Google's AGDE toolset) uses its own vocabulary, matching Sharpmake's
			// own emitted default ("cpp_shared", not the NDK-CMake-style
			// "c++_shared") -- and silently IGNORES an unrecognized value rather
			// than erroring, falling back to shared STL. A first attempt at
			// "c++_static" built clean, packaged clean, and even LOOKED right under
			// `llvm-readelf -l` (16 KB segment alignment comes from the separate
			// linker flag above, so that check passed regardless) -- but the .so's
			// own `NEEDED` list still named libc++_shared.so, which the APK no
			// longer shipped, so it crashed on launch with UnsatisfiedLinkError.
			// Verify any future change here with `llvm-readelf -d <so> | grep
			// NEEDED`, not just a successful build.
			conf.CustomProperties.Add("UseOfStl", "cpp_static");

			// AGDE packaging requires OutDir to end with the ABI directory
			// (.../<abi>\), because AGDE hands Gradle the PARENT of OutDir as the
			// jniLibs source root and Gradle then expects <root>/<abi>/lib*.so.
			//
			// The leaf is the canonical ABI dir name (arm64-v8a / x86_64) while the
			// config folder above it uses the config-name token (arm64_v8a /
			// x86_64) -- see ZenithAndroidAbi for why those two differ.
			//
			// Note this override deliberately omits the "vulkan_" render-backend
			// prefix that Sharpmake's default output naming would add (agde is
			// Vulkan-only, so the prefix carries no information here). The
			// UN-overridden IntDir does carry it, which is why the two trees are
			// named differently on disk -- Gradle must read this one.
			string configSuffix = target.Optimization == Optimization.Debug ? "debug" : "release";
			string agdeOutDir = "$(ProjectDir)output\\agde\\"
				+ ZenithAndroidAbi.ConfigToken(target.AndroidBuildTargets) + "_vs2022_" + configSuffix + "_agde_false\\"
				+ ZenithAndroidAbi.DirName(target.AndroidBuildTargets) + "\\";
			conf.CustomProperties.Add("OutDir", agdeOutDir);

			// The OutDir override above intentionally diverges from the linker's own
			// OutputFile (set unconditionally a few PropertyGroups earlier, for the
			// plain native link step -- same vulkan_<abi>... path, no ABI leaf).
			// MSBuild's own TargetPath heuristic ($(OutDir)$(TargetName)$(TargetExt))
			// is computed from the OVERRIDDEN OutDir, so it disagrees with that
			// OutputFile and raises MSB8012 on every AGDE build -- harmless (the real
			// linked artifact and the packaged APK are both correct; this is two
			// different, both-valid views of "where did the .so go", not a build
			// defect), but noisy. Suppress just this one coded warning (never
			// warnings generally) rather than unify the two paths and risk breaking
			// the packaging OutDir shape Gradle actually depends on.
			conf.CustomProperties.Add("MSBuildWarningsAsMessages", "MSB8012");

			// Keep production `release` free for a real release/upload key. AGDE
			// still needs a signed APK it can install locally, so native Release
			// configurations package the dedicated debug-signed `agdeRelease`
			// Gradle variant while retaining `release` in the native OutDir token.
			string gradleBuildType = target.Optimization == Optimization.Debug ? "debug" : "agdeRelease";
			conf.CustomProperties.Add("AndroidGradleBuildType", gradleBuildType);

			// Match the APK filename that Gradle actually produces
			conf.CustomProperties.Add("AndroidGradlePackageOutputName", "app-" + gradleBuildType + ".apk");
		}

		// Add Zenith engine dependency (includes Tools when ToolsEnabled)
		conf.AddPublicDependency<ZenithProject>(target);

		// Copy ALL Slang runtime DLLs to output directory (Windows only).
		// MVP-0.0.5 changed this from `slang.dll` to `*.dll` because slang.dll
		// has its own dependency tree (slang-rt, slang-glslang, slang-glsl-
		// module, slang-llvm, slang-compiler, gfx) that the OS loader looks up
		// from the exe's directory at startup. Copying only slang.dll left the
		// exe failing with STATUS_DLL_NOT_FOUND on machines that hadn't
		// manually populated the output dir from another game's build. The
		// wildcard is a no-op on CI runners that only have a placeholder
		// slang.dll in Middleware/slang/bin.
		if (target.Platform == Platform.win64)
		{
			// Use zenithRoot which is the actual project root (one level up from Build/)
			string slangBinPath = zenithRoot + "/Middleware/slang/bin";
			conf.EventPostBuild.Add($"xcopy /Y /D \"{slangBinPath}\\*.dll\" \"$(OutDir)\"");

			// The material-preview controller (Flux/RenderViews) references
			// Zenith_MeshGeometryAsset's procedural generators, which links the
			// engine->assimp import chain into every game exe — the assimp runtime
			// DLLs (config-specific names — assimp-vc143-mt[d].dll + poly2tri/
			// minizip/zlib/pugixml) must sit beside the exe or STATUS_DLL_NOT_FOUND
			// fires at launch, same as slang above. On-disk assimp DLL layout is
			// asymmetric: debug DLLs live in assimp/debug/bin, release DLLs in the
			// BARE assimp/bin (there is no assimp/release/ directory). Mirrors the
			// identical post-build in Sharpmake_FluxCompiler.cs.
			string assimpBinPath = (target.Optimization == Optimization.Debug)
				? zenithRoot + "/Tools/Middleware/assimp/debug/bin"
				: zenithRoot + "/Tools/Middleware/assimp/bin";
			conf.EventPostBuild.Add($"xcopy /Y /D \"{assimpBinPath}\\*.dll\" \"$(OutDir)\"");
		}
	}
}

// Per-game solution - the engine libraries + aggregate + this ONE game + the
// game's extra Sharpmake projects (e.g. TilePuzzle's offline tools). Abstract:
// the concrete GameName / HasAndroid / GameProjectType / ExtraProjectTypeNames
// come from the generated subclass. The .sln lands at the game root
// (Games/<Name>/<Name>_<platform>.sln); the game vcxproj stays under
// Games/<Name>/Build so output paths are unchanged.
public abstract class GameSolution : Solution
{
	// Supplied by the generated subclass.
	public abstract string GameName { get; }
	public virtual bool HasAndroid => false;
	public abstract Type GameProjectType { get; }

	// Extra Sharpmake project TYPE names to include in this game's solution
	// (resolved via Type.GetType against the compiled Sharpmake assembly). Used
	// by TilePuzzle to carry its offline LevelGen / RegistryViewer tools so they
	// live in the game's own sln and stay out of the game-free engine sln.
	public virtual string[] ExtraProjectTypeNames => new string[0];

	public GameSolution() : base(typeof(ZenithTarget))
	{
		Name = GameName;

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12 | RenderBackend.Null
		});

		if (HasAndroid)
		{
			AddTargets(new ZenithTarget
			{
				Platform = Platform.agde,
				DevEnv = DevEnv.vs2022,
				Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.False,
				RenderBackend = RenderBackend.Vulkan,
				AndroidBuildTargets = ZenithAndroidAbi.All
			});
		}
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.SolutionFileName = "[solution.Name]_[target.Platform]";
		// Solution lives at the game root. The generated subclass is defined in
		// Build/, so [solution.SharpmakeCsPath] resolves to Build/ and ../Games/<N>
		// lands at the game folder.
		conf.SolutionPath = @"[solution.SharpmakeCsPath]/../Games/" + GameName;

		// Engine leaf libs (visible/buildable on their own) + the aggregate engine.
		conf.AddProject<ZenithBaseLibProject>(target);
		conf.AddProject<ZenithECSLibProject>(target);
		conf.AddProject<ZenithPhysicsLibProject>(target);
		conf.AddProject<ZenithAILibProject>(target);
		conf.AddProject<ZenithProject>(target);

		// The game itself. Non-generic AddProject: the concrete project type comes
		// from the descriptor-generated subclass, so this base needs no per-game code.
		conf.AddProject(GameProjectType, target);

		// Windows-only tools.
		if (target.Platform == Platform.win64)
		{
			// FluxCompiler (Slang -> SPIR-V) + any per-game extra projects are
			// inherently Vulkan-side; add them only on Vulkan configs (their
			// projects declare Vulkan-only targets to match).
			if (target.RenderBackend == RenderBackend.Vulkan)
			{
				conf.AddProject<FluxCompilerProject>(target);

				foreach (string strTypeName in ExtraProjectTypeNames)
				{
					Type xExtraType = Type.GetType(strTypeName);
					if (xExtraType == null)
					{
						throw new Exception("GameSolution '" + GameName +
							"': extraSharpmakeProjects references unknown type '" + strTypeName + "'");
					}
					conf.AddProject(xExtraType, target);
				}
			}

			// MSDF font deps — only present in tools-enabled builds.
			if (target.ToolsEnabled == ToolsEnabled.True)
			{
				conf.AddProject<FreeTypeProject>(target);
				conf.AddProject<MsdfgenProject>(target);
				conf.AddProject<MsdfAtlasGenProject>(target);
			}
		}
	}
}
