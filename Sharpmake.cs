using Sharpmake;
using System;

[Sharpmake.Generate]
public class FluxCompilerProject : Project
{
    public FluxCompilerProject()
    {
        Name = "FluxCompiler";

        AddTargets(new Target(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
        ));

        SourceRootPath = @"[project.SharpmakeCsPath]/FluxCompiler";
    }

    [Configure]
    public void ConfigureAll(Configuration conf, Target target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";
		
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		
		String sharpmakePath = SharpmakeCsPath;
		String shaderRoot = sharpmakePath + "/Zenith/Flux/Shaders";
		shaderRoot = shaderRoot.Replace('\\', '/');
		
		conf.Defines.Add("SHADER_SOURCE_ROOT=\"" + shaderRoot + "\"");
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
    }
}

[Sharpmake.Generate]
public class ZenithToolsProject : Project
{
    public ZenithToolsProject()
    {
        Name = "ZenithTools";

        AddTargets(new Target(
                Platform.win64,
                DevEnv.vs2022,
                Optimization.Debug | Optimization.Release
        ));

        SourceRootPath = @"[project.SharpmakeCsPath]/Tools";
		
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");
		
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\test.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\tools.*");
		SourceFilesExcludeRegex.Add(@".*assimp-5.4.2\\port.*");
		SourceFilesExcludeRegex.Add(@".*zstream_test.*");
		SourceFilesExcludeRegex.Add(@".*zfstream.cpp.*");
		SourceFilesExcludeRegex.Add(@".*zip\\test.*");
		SourceFilesExcludeRegex.Add(@".*inflate86.*");
		SourceFilesExcludeRegex.Add(@".*minizip.*");
		SourceFilesExcludeRegex.Add(@".*iostream\\test.*");
		SourceFilesExcludeRegex.Add(@".*blast\\.*");
		SourceFilesExcludeRegex.Add(@".*puff\\.*");
		SourceFilesExcludeRegex.Add(@".*testzlib\\.*");
		SourceFilesExcludeRegex.Add(@".*untgz\\.*");
		
		AdditionalSourceRootPaths.Add("[project.SharpmakeCsPath]/Zenith/Flux/MeshGeometry");
    }

    [Configure]
    public void ConfigureAll(Configuration conf, Target target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";
		
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/assimp");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/code");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/code/common");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/pugixml/src");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/zlib/contrib/minizip");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/rapidjson/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/zlib");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/code/AssetLib/Step");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/openddlparser/include/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/openddlparser/include/openddlparser");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp-5.4.2/contrib/utf8cpp/source");
		
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/entt-3.13.2/single_include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Windows");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Vulkan");
		
		conf.Defines.Add("ZENITH_TOOLS");
		
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		String sharpmakePath = SharpmakeCsPath;
		String assetRoot = sharpmakePath + "/Games/Test/Assets/";
		assetRoot = assetRoot.Replace('\\', '/');
		conf.Defines.Add("ASSETS_DIR=\"" + assetRoot + "\"");
		
		conf.Defines.Add("ASSIMP_BUILD_NO_C4D_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_OPENGEX_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_OPENGEX_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_IFC_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_IFC_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_GLTF_IMPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_GLTF_EXPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_3MF_EXPORTER");
		conf.Defines.Add("ASSIMP_BUILD_NO_BLEND_IMPORTER");
		conf.Defines.Add("OPENDDLPARSER_BUILD");
    }
}

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
		
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");
		SourceFilesExcludeRegex.Add(@".*entt-3.13.2.*");
		SourceFilesExcludeRegex.Add(@".*Tools.*");
    }

    [Configure]
    public void ConfigureAll(Configuration conf, Target target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";

        conf.PrecompHeader = "Zenith.h";
        conf.PrecompSource = "Zenith.cpp";
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/entt-3.13.2/single_include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Windows");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Vulkan");
		
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Lib");
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
		
		conf.LibraryFiles.Add("glfw3_mt.lib");
		conf.LibraryFiles.Add("vulkan-1.lib");
		
		conf.Defines.Add("ZENITH_VULKAN");
		conf.Defines.Add("ZENITH_WINDOWS");
		conf.Defines.Add("NOMINMAX");
		
		String sharpmakePath = SharpmakeCsPath;
		String shaderRoot = sharpmakePath + "/Zenith/Flux/Shaders/";
		shaderRoot = shaderRoot.Replace('\\', '/');
		conf.Defines.Add("SHADER_SOURCE_ROOT=\"" + shaderRoot + "\"");
		
		//#TO entt requires cpp17
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		
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
        conf.SolutionPath = @"[solution.SharpmakeCsPath]";
        conf.AddProject<ZenithWindowsProject>(target);
        conf.AddProject<FluxCompilerProject>(target);
        conf.AddProject<ZenithToolsProject>(target);
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
