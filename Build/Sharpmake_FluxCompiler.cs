using Sharpmake;
using System;
using System.IO;

// FluxCompiler project - Shader compiler utility (Windows only)
[Sharpmake.Generate]
public class FluxCompilerProject : ZenithBaseProject
{
	public FluxCompilerProject()
	{
		Name = "FluxCompiler";
		SourceRootPath = RootPath + "/FluxCompiler";

		// FluxCompiler is Windows-only
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]/../FluxCompiler";

		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);

		// Zenith root path - absolute path computed from Sharpmake location
		string zenithRoot = Path.GetFullPath(Path.Combine(SharpmakeCsPath, "..")).Replace('\\', '/');

		// Shader source path - using forward slashes for compatibility
		string shaderSourceRoot = zenithRoot + "/Zenith/Flux/Shaders/";
		conf.Defines.Add($"SHADER_SOURCE_ROOT=\"{shaderSourceRoot}\"");
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");

		// Include paths
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");

		// Output executable
		conf.Output = Configuration.OutputType.Exe;
	}
}
