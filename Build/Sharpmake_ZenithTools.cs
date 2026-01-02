using Sharpmake;
using System;
using System.IO;

// Zenith Tools project - Windows only for asset import/export
[Sharpmake.Generate]
public class ZenithToolsProject : ZenithBaseProject
{
	public ZenithToolsProject()
	{
		Name = "ZenithTools";
		SourceRootPath = RootPath + "/Tools";

		// Tools are Windows-only
		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False
		});

		// Assimp excludes
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

		// Other excludes
		SourceFilesExcludeRegex.Add(@".*VulkanSDK.*");
		SourceFilesExcludeRegex.Add(@".*FluxCompiler.*");
		SourceFilesExcludeRegex.Add(@".*glm-master.*");

		// OpenCV excludes
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

		// Jolt Physics excludes
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Build.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Docs.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\UnitTests.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\Samples.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\TestFramework.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\PerformanceTest.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\JoltViewer.*");
		SourceFilesExcludeRegex.Add(@".*JoltPhysics-5.4.0\\HelloWorld.*");

		SourceFilesExcludeRegex.Add(@".*cmake.*");

		// Include mesh geometry from Zenith for tools
		AdditionalSourceRootPaths.Add(RootPath + "/Zenith/Flux/MeshGeometry");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		ConfigureCommonSettings(conf, target);

		// Tools-specific include paths
		conf.IncludePaths.Add(RootPath + "/Zenith");
		conf.IncludePaths.Add(RootPath + "/Zenith/Core");
		conf.IncludePaths.Add(RootPath + "/Middleware/glm-master");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/assimp/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/build/include/opencv2");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/modules/core/include/");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/modules/ts/include/");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/cmake/templates/");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/3rdparty/libtiff/");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/3rdparty/openjpeg/openjp2/");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/3rdparty/libjpeg");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/opencv/sources/3rdparty/libpng");
		conf.IncludePaths.Add(RootPath + "/Middleware/stb");
		conf.IncludePaths.Add(RootPath + "/Middleware");
		conf.IncludePaths.Add(RootPath + "/Middleware/imgui-docking");
		conf.IncludePaths.Add(RootPath + "/Middleware/glfw-3.4.bin.WIN64/include");
		conf.IncludePaths.Add(RootPath + "/Middleware/VulkanSDK/1.3.280.0/Include");
		conf.IncludePaths.Add(RootPath + "/Zenith/Windows");
		conf.IncludePaths.Add(RootPath + "/Zenith/Vulkan");

		conf.Defines.Add("ZENITH_TOOLS");
		conf.Defines.Add("OPENDDLPARSER_BUILD");
		conf.Defines.Add("__OPENCV_BUILD");

		// Compute actual paths using SharpmakeCsPath (Build directory)
		string zenithRoot = new DirectoryInfo(SharpmakeCsPath).Parent.FullName;

		// Asset paths for tools (these are defaults - game projects can override)
		// Note: Tools need these paths for asset import/export
		string engineAssetRoot = (zenithRoot + "/Zenith/Assets/").Replace('\\', '/');
		conf.Defines.Add($"ENGINE_ASSETS_DIR=\"{engineAssetRoot}\"");
		// GAME_ASSETS_DIR is set to a placeholder - specific game paths come from the game project
		conf.Defines.Add("GAME_ASSETS_DIR=\"./Assets/\"");

		conf.Output = Configuration.OutputType.Lib;

		// Library paths
		conf.LibraryPaths.Add(RootPath + "/Tools/Middleware/opencv/build/x64/vc16/lib");
		conf.LibraryPaths.Add(RootPath + "/Tools/Middleware/assimp/lib");

		if (target.Optimization == Optimization.Debug)
		{
			conf.LibraryFiles.Add("opencv_world4100d.lib");
			conf.LibraryFiles.Add("assimp-vc143-mtd.lib");
		}
		else
		{
			conf.LibraryFiles.Add("opencv_world4100.lib");
			conf.LibraryFiles.Add("assimp-vc143-mt.lib");
		}
	}
}
