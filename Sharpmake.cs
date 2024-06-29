using Sharpmake;

[Sharpmake.Generate]
public class ZenithWindowsProject : Project
{
    public ZenithWindowsProject()
    {
        Name = "Zenith";

        AddTargets(new Target(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
        ));

        SourceRootPath = @"[project.SharpmakeCsPath]";
    }

    [Configure]
    public void ConfigureAll(Configuration conf, Target target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]\Build";

        conf.PrecompHeader = "Zenith.h";
        conf.PrecompSource = "Zenith.cpp";
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add("$(VULKAN_SDK)/Include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Windows");
		
		conf.LibraryPaths.Add("$(VULKAN_SDK)/Lib");
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
		
		conf.LibraryFiles.Add("glfw3_mt.lib");
		conf.LibraryFiles.Add("vulkan-1.lib");
		
		conf.Defines.Add("ZENITH_VULKAN");
		conf.Defines.Add("ZENITH_WINDOWS");
		
		if(target.Optimization == Optimization.Debug)
		{
			conf.Defines.Add("ZENITH_DEBUG");
		}
    }
}

[Sharpmake.Generate]
public class ZenithWindowsSolution : Sharpmake.Solution
{
    public ZenithWindowsSolution()
    {
        Name = "Zenith";

        AddTargets(new Target(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
        ));
    }

    [Configure()]
    public void ConfigureAll(Configuration conf, Target target)
    {
        conf.SolutionFileName = "[solution.Name]_[target.Platform]";
        conf.SolutionPath = @"[solution.SharpmakeCsPath]\Build";
        conf.AddProject<ZenithWindowsProject>(target);
    }
}

public static class Main
{
    [Sharpmake.Main]
    public static void SharpmakeMain(Sharpmake.Arguments arguments)
    {
        arguments.Generate<ZenithWindowsSolution>();
    }
}
