using Sharpmake;
using System;

[Fragment, Flags]
public enum ToolsEnabled
{
    True = 1,
	False = 2
}
public class CustomTarget : ITarget
{
	public Platform Platform;
    public DevEnv DevEnv;
    public Optimization Optimization;
    public ToolsEnabled ToolsEnabled;
}

[Sharpmake.Generate]
public class FluxCompilerProject : Project
{
    public FluxCompilerProject() : base(typeof(CustomTarget))
    {
        Name = "FluxCompiler";

        AddTargets(new CustomTarget
		{
                Platform = Platform.win64,
                DevEnv = DevEnv.vs2022,
                Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
        });

        SourceRootPath = @"[project.SharpmakeCsPath]/FluxCompiler";
    }

    [Configure]
    public void ConfigureAll(Configuration conf, CustomTarget target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";
		
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		
		String sharpmakePath = SharpmakeCsPath;
		String shaderRoot = sharpmakePath + "/Zenith/Flux/Shaders";
		shaderRoot = shaderRoot.Replace('\\', '/');
		
		conf.Defines.Add("SHADER_SOURCE_ROOT=\"" + shaderRoot + "\"");
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
    }
}

[Sharpmake.Generate]
public class ZenithToolsProject : Project
{
    public ZenithToolsProject() : base(typeof(CustomTarget))
    {
        Name = "ZenithTools";

        AddTargets(new CustomTarget
		{
                Platform = Platform.win64,
                DevEnv = DevEnv.vs2022,
                Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
        });

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
		
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");
		SourceFilesExcludeRegex.Add(@".*entt-3.13.2.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\helloworld.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\test.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\testbed.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\samples.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\cmake.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\calib3d.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\core.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\dnn.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\gapi.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\highgui.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\features2d.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\flann.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\imgproc.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\objdetect.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\ml.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\python.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\stitching.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\java.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\video.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\ts.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\js.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\photo.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\modules\\imgcodecs\\test.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\3rdparty.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\samples.*");
		SourceFilesExcludeRegex.Add(@".*opencv\\sources\\apps.*");
		
		AdditionalSourceRootPaths.Add("[project.SharpmakeCsPath]/Zenith/Flux/MeshGeometry");
    }

    [Configure]
    public void ConfigureAll(Configuration conf, CustomTarget target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";
		
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/stb");
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
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/build/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/build/include/opencv2");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/modules/core/include/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/modules/ts/include/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/cmake/templates/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/3rdparty/libtiff/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/3rdparty/openjpeg/openjp2/");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/3rdparty/libjpeg");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/sources/3rdparty/libpng");
		
		
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
		conf.Defines.Add("__OPENCV_BUILD");
		
		conf.Output = Configuration.OutputType.Lib;
    }
}

[Sharpmake.Generate]
public class ZenithWindowsProject : Project
{
    public ZenithWindowsProject() : base(typeof(CustomTarget))
    {
        Name = "Zenith";

        AddTargets(new CustomTarget
		{
                Platform = Platform.win64,
                DevEnv = DevEnv.vs2022,
                Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
        });

        SourceRootPath = @"[project.SharpmakeCsPath]";
		
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");
		
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");
		SourceFilesExcludeRegex.Add(@".*entt-3.13.2.*");
		SourceFilesExcludeRegex.Add(@".*Tools.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\helloworld.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\test.*");
		SourceFilesExcludeRegex.Add(@".*reactphysics3d-0.10.1\\testbed.*");
    }

    [Configure]
    public void ConfigureAll(Configuration conf, CustomTarget target)
    {
        conf.ProjectFileName = "[project.Name]_[target.Platform]";
        conf.ProjectPath = @"[project.SharpmakeCsPath]";

        conf.PrecompHeader = "Zenith.h";
        conf.PrecompSource = "Zenith.cpp";
		conf.PrecompSourceExcludeFolders.Add("[project.SharpmakeCsPath]/Middleware/reactphysics3d-0.10.1");
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/entt-3.13.2/single_include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/reactphysics3d-0.10.1/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Windows");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Vulkan");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Games");
		
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Lib");
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/lib-vc2022");
		
		conf.LibraryFiles.Add("glfw3_mt.lib");
		conf.LibraryFiles.Add("vulkan-1.lib");
		
		conf.Defines.Add("ZENITH_VULKAN");
		conf.Defines.Add("ZENITH_WINDOWS");
		conf.Defines.Add("NOMINMAX");
		if(target.ToolsEnabled == ToolsEnabled.True)
		{
			conf.Defines.Add("ZENITH_TOOLS");
		}
		
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
		
		conf.AddPrivateDependency<ZenithToolsProject>(target);
    }
}

[Sharpmake.Generate]
public class ZenithWindowsSolution : Sharpmake.Solution
{
    public ZenithWindowsSolution() : base(typeof(CustomTarget))
    {
        Name = "Zenith";

        AddTargets(new CustomTarget
		{
                Platform = Platform.win64,
                DevEnv = DevEnv.vs2022,
                Optimization = Optimization.Debug | Optimization.Release,
				ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
        });
    }

    [Configure()]
    public void ConfigureAll(Configuration conf, CustomTarget target)
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
