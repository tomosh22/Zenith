using Sharpmake;
using System;

// LibB -- shares the Zenith/Shared source root with LibC (scenario 9, the
// split source root). The AssetHandling-style split: LibC's own files are
// excluded here one file at a time, never by a directory wildcard.
[Sharpmake.Generate]
public class LibBLibProject : ZenithBaseProject
{
	public LibBLibProject()
	{
		Name = "LibB";
		SourceRootPath = RootPath + "/Zenith/Shared";

		// LibC's own two files -- excluded from LibB one file at a time,
		// the real AssetHandling/AssetRegistry way.
		SourceFilesExcludeRegex.Add(@"Shared\\LibC_Api\.h$");
		SourceFilesExcludeRegex.Add(@"Shared\\LibC_Impl\.cpp$");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.PrecompHeader = "FxPch.h";
		conf.Output = Configuration.OutputType.Lib;

		// Scenario 14/22: LibB declares LibA -- NOT LibC (that edge is the
		// deliberately undeclared one).
		conf.AddPublicDependency<LibALibProject>(target);

		// Scenario 25: LibB also declares MidLib directly (in addition to
		// reaching it transitively through the PCH), so LibB_Impl.cpp's own
		// direct #include "MidLib/MidLib.h" makes this declared+used.
		conf.AddPublicDependency<MidLibLibProject>(target);

		// Scenario 23: a comment-only mention. This line must NOT become a
		// declared edge -- sharpmake.py strips comments before scanning.
		// conf.AddPublicDependency<LibCLibProject>(target);
	}
}
