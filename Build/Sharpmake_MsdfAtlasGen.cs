using Sharpmake;
using System;
using System.IO;

// msdf-atlas-gen static library. Vendored at Tools/Middleware/msdf-atlas-gen.
// Tools-only, Win64 only. Depends on Msdfgen (and transitively FreeType).
//
// Excludes:
//   main.cpp                  — standalone CLI exe
//   artery-font-export.cpp    — needs the artery-font-format submodule headers
//                               which we don't link against (we write our own .zfont)
//   image-encode.cpp          — needs lodepng/PNG writing; we go via Zenith_TextureExport
//
// All other .cpp files in the msdf-atlas-gen/ subdir compile into the lib.
[Sharpmake.Generate]
public class MsdfAtlasGenProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public MsdfAtlasGenProject() : base(typeof(ZenithTarget))
	{
		Name = "msdf-atlas-gen";
		SourceRootPath = RootPath + "/Tools/Middleware/msdf-atlas-gen/msdf-atlas-gen";

		SourceFilesExtensions = new Strings(".cpp", ".h", ".hpp");
		SourceFilesCompileExtensions = new Strings(".cpp");

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";
		conf.IntermediatePath = @"[project.SharpmakeCsPath]/obj/msdfatlasgen_[target.Optimization]";
		conf.TargetPath = @"[project.SharpmakeCsPath]/output/msdfatlasgen/[target.Optimization]";

		conf.Output = Configuration.OutputType.Lib;
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);
		conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Disable);

		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

		conf.Defines.Add("MSDFGEN_DISABLE_SVG");
		conf.Defines.Add("MSDFGEN_DISABLE_PNG");
		conf.Defines.Add("MSDFGEN_USE_CPP11");
		conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

		// Skip files needing deps we don't link.
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdf-atlas-gen\\msdf-atlas-gen\\main\.cpp$");
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdf-atlas-gen\\msdf-atlas-gen\\artery-font-export\.cpp$");
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdf-atlas-gen\\msdf-atlas-gen\\image-encode\.cpp$");

		conf.AddPublicDependency<MsdfgenProject>(target);
	}
}
