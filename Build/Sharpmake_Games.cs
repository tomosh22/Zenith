using Sharpmake;
using System;
using System.IO;

// Game project - Executable that links Zenith engine
// This is parameterized - create instances for each game (Test, EmptyGame, etc.)
[Sharpmake.Generate]
public class GameProject : ZenithBaseProject
{
	// The name of the game - override in derived classes or set before generation
	public virtual string GameName => "Sokoban";

	public GameProject()
	{
		Name = GameName;
		SourceRootPath = RootPath + "/Games/" + GameName;

		// Add Windows and Android targets
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});

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

		// Game-specific include paths
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
		}
	}
}

// Sokoban game project (historically misnamed TestGameProject — renamed so the
// real Games/Test project below can carry the honest name)
[Sharpmake.Generate]
public class SokobanGameProject : GameProject
{
	public override string GameName => "Sokoban";

	public SokobanGameProject() : base()
	{
		// Sokoban-specific configuration if needed
	}
}

// Test game project - the engine-feature test game (Games/Test). Restored to
// the build after the misnamed class above was found generating Sokoban
// instead; its stale hand-era vcxproj predated the RenderBackend config prefix.
[Sharpmake.Generate]
public class TestGameProject : GameProject
{
	public override string GameName => "Test";

	public TestGameProject() : base()
	{
		// Test-specific configuration if needed
	}
}

// Marble game project - physics showcase
[Sharpmake.Generate]
public class MarbleGameProject : GameProject
{
	public override string GameName => "Marble";

	public MarbleGameProject() : base()
	{
		// Marble-specific configuration if needed
	}
}

// Runner game project - Animation + Terrain showcase
[Sharpmake.Generate]
public class RunnerGameProject : GameProject
{
	public override string GameName => "Runner";

	public RunnerGameProject() : base()
	{
		// Runner-specific configuration if needed
	}
}

// Combat game project - Animation + Events + IK showcase
[Sharpmake.Generate]
public class CombatGameProject : GameProject
{
	public override string GameName => "Combat";

	public CombatGameProject() : base()
	{
		// Combat-specific configuration if needed
	}
}

// Exploration game project - Terrain + Atmosphere showcase
[Sharpmake.Generate]
public class ExplorationGameProject : GameProject
{
	public override string GameName => "Exploration";

	public ExplorationGameProject() : base()
	{
		// Exploration-specific configuration if needed
	}
}

// RenderTest game project - Procedural terrain render test
[Sharpmake.Generate]
public class RenderTestGameProject : GameProject
{
	public override string GameName => "RenderTest";

	public RenderTestGameProject() : base()
	{
		// RenderTest-specific configuration if needed
	}
}

// Survival game project - Task System + Multi-Feature showcase
[Sharpmake.Generate]
public class SurvivalGameProject : GameProject
{
	public override string GameName => "Survival";

	public SurvivalGameProject() : base()
	{
		// Survival-specific configuration if needed
	}
}

// TilePuzzle game project - Sliding puzzle with colored shapes and cats
[Sharpmake.Generate]
public class TilePuzzleGameProject : GameProject
{
	public override string GameName => "TilePuzzle";

	public TilePuzzleGameProject() : base()
	{
		// TilePuzzle-specific configuration if needed
	}
}

// AIShowcase game project - AI System demonstration
[Sharpmake.Generate]
public class AIShowcaseGameProject : GameProject
{
	public override string GameName => "AIShowcase";

	public AIShowcaseGameProject() : base()
	{
		// AIShowcase-specific configuration if needed
	}
}

// DevilsPlayground game project - Click-to-possess top-down occult horror (UE5 game-jam port)
[Sharpmake.Generate]
public class DevilsPlaygroundGameProject : GameProject
{
	public override string GameName => "DevilsPlayground";

	public DevilsPlaygroundGameProject() : base()
	{
		// DevilsPlayground-specific configuration if needed
	}
}

// CityBuilder game project - SimCity/Cities-Skylines-style city builder with
// runtime terrain deformation (Flux_TerrainModification engine subsystem).
[Sharpmake.Generate]
public class CityBuilderGameProject : GameProject
{
	public override string GameName => "CityBuilder";

	public CityBuilderGameProject() : base()
	{
		// CityBuilder-specific configuration if needed
	}
}
