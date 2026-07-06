using Sharpmake;
using System;
using System.IO;

// =============================================================================
// Game build-system base classes.
//
// The 12 concrete per-game project + solution classes are NO LONGER hand-written
// here. They are generated into Build/Sharpmake_GameInstances.generated.cs from
// Games/<Name>/<Name>.zproj descriptors by Build/zenith_buildsystem.psm1
// (Invoke-ZenithCodegen), which regen.ps1 runs before Sharpmake. Adding or
// removing a game touches only its descriptor -- never this file.
//
// This file holds the two ABSTRACT bases the generated shells derive from:
//   * GameProject  -- the game .exe/.so (was the old [Generate] GameProject with
//                     a "Sokoban" default; now abstract, name/android supplied by
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
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
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
				AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
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

			// APK packaging - point to Gradle project in Games/<GameName>/Android/
			conf.CustomProperties.Add("AndroidEnablePackaging", "true");
			conf.CustomProperties.Add("AndroidGradleBuildDir", zenithRoot + "/Games/" + GameName + "/Android");
			conf.CustomProperties.Add("AndroidApplicationModule", "app");

			// AGDE packaging requires OutDir to end with the ABI directory (arm64-v8a\)
			string configSuffix = target.Optimization == Optimization.Debug ? "debug" : "release";
			string agdeOutDir = "$(ProjectDir)output\\agde\\arm64_v8a_vs2022_" + configSuffix + "_agde_false\\arm64-v8a\\";
			conf.CustomProperties.Add("OutDir", agdeOutDir);

			// Map MSBuild configuration name to Gradle build type (debug/release)
			conf.CustomProperties.Add("AndroidGradleBuildType", configSuffix);

			// Match the APK filename that Gradle actually produces
			conf.CustomProperties.Add("AndroidGradlePackageOutputName", "app-" + configSuffix + ".apk");
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
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
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
				AndroidBuildTargets = Android.AndroidBuildTargets.arm64_v8a
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
