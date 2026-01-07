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
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});

		AddTargets(new ZenithTarget
		{
			Platform = Platform.agde,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.False,
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
		conf.Defines.Add($"GAME_ASSETS_DIR=\"{gameAssetRoot}\"");
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");

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
		}

		// Add Zenith engine dependency (includes Tools when ToolsEnabled)
		conf.AddPublicDependency<ZenithProject>(target);
	}
}

// Test game project - the existing test game
[Sharpmake.Generate]
public class TestGameProject : GameProject
{
	public override string GameName => "Sokoban";

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
