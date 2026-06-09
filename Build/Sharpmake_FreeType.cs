using Sharpmake;
using System;
using System.IO;

// FreeType static library. Vendored under Tools/Middleware/freetype. Tools-only
// (used by Zenith_Tools_FontExport via msdfgen + msdf-atlas-gen). Win64 only.
//
// File list mirrors FreeType's official CMakeLists BASE_SRCS. We compile only
// the per-module "umbrella" .c files (each one #includes its module's internal
// .c sources); compiling internals separately would cause symbol duplicates.
//
// Configuration:
//   FT2_BUILD_LIBRARY                — tells FreeType headers to expose its lib API
//   FT_CONFIG_OPTION_DISABLE_HARFBUZZ — no HarfBuzz dep (we don't bake shaped text)
//   FT_CONFIG_OPTION_USE_PNG/ZLIB/... — left undefined; FreeType uses its own paths
[Sharpmake.Generate]
public class FreeTypeProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public FreeTypeProject() : base(typeof(ZenithTarget))
	{
		Name = "FreeType";
		SourceRootPath = RootPath + "/Tools/Middleware/freetype";

		SourceFilesExtensions = new Strings(".c", ".h");
		SourceFilesCompileExtensions = new Strings(".c");

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});

		// Project-level excludes: prune entire vendored subtrees we don't compile at all.
		SourceFilesExcludeRegex.Add(@".*\\freetype\\docs\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\devel\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\tests\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\objs\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\subprojects\\.*");
		// Build harnesses for OSes we don't target (we use builds/windows/ explicitly below).
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\unix\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\amiga\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\mac\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\wince\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\vms\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\builds\\os2\\.*");
		// dlg is a debug-logging dep we don't need; FreeType also bundles tools we skip.
		SourceFilesExcludeRegex.Add(@".*\\freetype\\src\\tools\\.*");
		SourceFilesExcludeRegex.Add(@".*\\freetype\\src\\dlg\\.*");
		// bzip2 module is for bzip2-compressed PCF fonts — we don't need it.
		SourceFilesExcludeRegex.Add(@".*\\freetype\\src\\bzip2\\.*");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";
		conf.IntermediatePath = @"[project.SharpmakeCsPath]/obj/freetype_[target.Optimization]";
		conf.TargetPath = @"[project.SharpmakeCsPath]/output/freetype/[target.Optimization]";

		conf.Output = Configuration.OutputType.Lib;

		// Match Zenith's static-CRT default (libcpmt / libcpmtd). Mixing static + DLL
		// CRT causes LNK2005 at the final game.exe link.
		// (No explicit RuntimeLibrary option — MSBuild defaults to /MTd for Debug,
		//  /MT for Release static libs, which is what we want.)

		conf.Defines.Add("FT2_BUILD_LIBRARY");
		conf.Defines.Add("FT_CONFIG_OPTION_DISABLE_HARFBUZZ");
		conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");
		conf.Defines.Add("_CRT_NONSTDC_NO_WARNINGS");

		// Public include path (consumers of FreeType need this to #include <freetype/...>).
		conf.IncludePaths.Add(RootPath + "/Tools/Middleware/freetype/include");
		// Internal include path for FreeType's own #include "freetype/internal/..." chains.
		conf.IncludePrivatePaths.Add(RootPath + "/Tools/Middleware/freetype/include");

		// Build-level excludes: keep ONLY the canonical BASE_SRCS umbrella .c files
		// plus the Windows-specific ftsystem.c / ftdebug.c overrides.
		// Everything else under src/* and builds/windows/* is skipped from compilation.
		// (Files stay in the project for navigation; just not built.)

		// src/base/ — keep the listed umbrella + standalone TUs only.
		// Internal #included sources (ftadvanc, ftcalc, ...) skipped; src/base/ftsystem.c
		// and src/base/ftdebug.c skipped in favour of the builds/windows/ versions.
		string[] keepBase = {
			"ftbase", "ftbbox", "ftbdf", "ftbitmap", "ftcid", "ftfstype", "ftgasp",
			"ftglyph", "ftgxval", "ftinit", "ftmm", "ftotval", "ftpatent", "ftpfr",
			"ftstroke", "ftsynth", "fttype1", "ftwinfnt",
		};
		// Negative lookahead: exclude any src/base/*.c whose stem isn't in the keep list.
		conf.SourceFilesBuildExcludeRegex.Add(
			@"\\freetype\\src\\base\\(?!(" + string.Join("|", keepBase) + @")\.c$)[^\\]+\.c$");

		// Per-module subdirs: each has one umbrella .c and N internals. Keep umbrella only.
		string[][] modules = new[] {
			new[] { "autofit",  "autofit"  },
			new[] { "bdf",      "bdf"      },
			new[] { "cache",    "ftcache"  },
			new[] { "cff",      "cff"      },
			new[] { "cid",      "type1cid" },
			new[] { "gzip",     "ftgzip"   },
			new[] { "lzw",      "ftlzw"    },
			new[] { "pcf",      "pcf"      },
			new[] { "pfr",      "pfr"      },
			new[] { "psaux",    "psaux"    },
			new[] { "pshinter", "pshinter" },
			new[] { "psnames",  "psnames"  },
			new[] { "raster",   "raster"   },
			new[] { "sdf",      "sdf"      },
			new[] { "sfnt",     "sfnt"     },
			new[] { "smooth",   "smooth"   },
			new[] { "svg",      "svg"      },
			new[] { "truetype", "truetype" },
			new[] { "type1",    "type1"    },
			new[] { "type42",   "type42"   },
			new[] { "winfonts", "winfnt"   },
			new[] { "gxvalid",  "gxvalid"  },
			new[] { "otvalid",  "otvalid"  },
		};
		foreach (var mod in modules)
		{
			conf.SourceFilesBuildExcludeRegex.Add(
				@"\\freetype\\src\\" + mod[0] + @"\\(?!" + mod[1] + @"\.c$)[^\\]+\.c$");
		}
	}
}
