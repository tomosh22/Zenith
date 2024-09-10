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
		SourceFilesExcludeRegex.Add(@".*opencv.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\examples.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_sdl.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_opengl.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_dx.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_android.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_glut.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_wgpu.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\backends\\imgui_impl_allegro.*");
		SourceFilesExcludeRegex.Add(@".*imgui-1.91.0\\misc.*");
		
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
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/stb");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp/include");
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
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/imgui-1.91.0");
		
		
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Core");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/glm-master");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/entt-3.13.2/single_include");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Windows");
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Zenith/Vulkan");
		
		conf.Defines.Add("ZENITH_WINDOWS");
		conf.Defines.Add("ZENITH_TOOLS");
		
		conf.Defines.Add("GLM_ENABLE_EXPERIMENTAL");
		conf.Defines.Add("NOMINMAX");
		
		String sharpmakePath = SharpmakeCsPath;
		String gameAssetRoot = sharpmakePath + "/Games/Test/Assets/";
		gameAssetRoot = gameAssetRoot.Replace('\\', '/');
		conf.Defines.Add("GAME_ASSETS_DIR=\"" + gameAssetRoot + "\"");
		String engineAssetRoot = sharpmakePath + "/Zenith/Assets/";
		engineAssetRoot = engineAssetRoot.Replace('\\', '/');
		conf.Defines.Add("ENGINE_ASSETS_DIR=\"" + engineAssetRoot + "\"");
		
		
		conf.Defines.Add("OPENDDLPARSER_BUILD");
		conf.Defines.Add("__OPENCV_BUILD");
		
		conf.Output = Configuration.OutputType.Lib;
		
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/opencv/build/x64/vc16/lib");
		conf.LibraryPaths.Add("[project.SharpmakeCsPath]/Tools/Middleware/assimp/lib");
		
		if(target.Optimization == Optimization.Debug)
		{
			conf.LibraryFiles.Add("opencv_world4100d.lib");
			conf.LibraryFiles.Add("assimp-vc143-mtd.lib");
		}
		else{
			conf.LibraryFiles.Add("opencv_world4100.lib");
			conf.LibraryFiles.Add("assimp-vc143-mt.lib");
		}
		
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
		
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h", ".vert", ".frag", ".comp", ".tese", ".tesc", ".geom", ".fxh");
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
		conf.IncludePaths.Add("[project.SharpmakeCsPath]/Middleware/vma");
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
			conf.AddPublicDependency<ZenithToolsProject>(target);
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
		conf.AddProject<ZenithToolsProject>(target);
        conf.AddProject<ZenithWindowsProject>(target);
        conf.AddProject<FluxCompilerProject>(target);
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
