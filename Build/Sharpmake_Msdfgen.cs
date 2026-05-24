using Sharpmake;
using System;
using System.IO;

// msdfgen static library (core + ext). Vendored as a submodule of msdf-atlas-gen at
// Tools/Middleware/msdf-atlas-gen/msdfgen. Tools-only, Win64 only.
//
// We compile both halves into one .lib:
//   core/ — pure C++ math/SDF logic. No external deps.
//   ext/  — extensions. Depends on FreeType for font import.
//   Excluded: import-svg.cpp (needs TinyXML2), save-png.cpp (needs lodepng),
//   resolve-shape-geometry.cpp (needs Skia). All optional and disabled via
//   MSDFGEN_DISABLE_SVG / MSDFGEN_DISABLE_PNG / no MSDFGEN_USE_SKIA define.
//
// The cmake-generated msdfgen-config.h is replaced by a hand-written copy at
// msdfgen/Config/msdfgen/msdfgen-config.h (see Sharpmake_FreeType.cs comment for
// include-path rationale).
[Sharpmake.Generate]
public class MsdfgenProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public MsdfgenProject() : base(typeof(ZenithTarget))
	{
		Name = "msdfgen";
		SourceRootPath = RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen";

		SourceFilesExtensions = new Strings(".cpp", ".h", ".hpp");
		SourceFilesCompileExtensions = new Strings(".cpp");

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True
		});

		// Skip stuff we don't compile at all.
		SourceFilesExcludeRegex.Add(@".*\\msdfgen\\main\.cpp$");          // standalone CLI
		SourceFilesExcludeRegex.Add(@".*\\msdfgen\\cmake\\.*");
		SourceFilesExcludeRegex.Add(@".*\\msdfgen\\full-msdfgen.*");      // CI helper sources
		SourceFilesExcludeRegex.Add(@".*\\msdfgen\\tinyxml2\\.*");        // if it ever ships, we don't use SVG
		SourceFilesExcludeRegex.Add(@".*\\msdfgen\\freetype\\.*");        // vendored copy, we use the parent FreeType project
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";
		conf.IntermediatePath = @"[project.SharpmakeCsPath]/obj/msdfgen_[target.Optimization]";
		conf.TargetPath = @"[project.SharpmakeCsPath]/output/msdfgen/[target.Optimization]";

		conf.Output = Configuration.OutputType.Lib;
		conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP20);
		conf.Options.Add(Options.Vc.General.TreatWarningsAsErrors.Disable);
		// Match Zenith's static CRT.

		// Public to consumers: see Sharpmake_Zenith.cs IncludePaths which already
		// adds these for the Zenith engine; we add them here too for our own TUs.
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen");
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/msdf-atlas-gen/msdfgen/Config");

		// Match msdfgen-config.h's MSDFGEN_DISABLE_* settings via -D so optional
		// .cpp files conditional-compile out cleanly. (The header defines them too,
		// but having both belt and braces avoids surprises if a TU sees the .cpp
		// before the header chain.)
		conf.Defines.Add("MSDFGEN_DISABLE_SVG");
		conf.Defines.Add("MSDFGEN_DISABLE_PNG");
		conf.Defines.Add("MSDFGEN_USE_CPP11");
		conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");

		// Exclude .cpp files for the disabled features. Excluded via build regex
		// because the files are still useful for navigation but must not compile.
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdfgen\\ext\\import-svg\.cpp$");
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdfgen\\ext\\save-png\.cpp$");
		conf.SourceFilesBuildExcludeRegex.Add(@"\\msdfgen\\ext\\resolve-shape-geometry\.cpp$");

		// FreeType dependency (transitively pulled in by consumers).
		conf.AddPublicDependency<FreeTypeProject>(target);
	}
}
