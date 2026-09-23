using Sharpmake;
using System;

// LibA -- the fixture's leaf lib. No AddPublicDependency calls at all,
// mirroring ZenithCore's position at the bottom of the real DAG.
[Sharpmake.Generate]
public class LibALibProject : ZenithBaseProject
{
	public LibALibProject()
	{
		Name = "LibA";
		SourceRootPath = RootPath + "/Zenith/LibA";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		// LibA CREATEs the shared FxPch.h (mirrors ZenithCore creating
		// Zenith.pch from Zenith.cpp).
		conf.PrecompHeader = "FxPch.h";
		conf.PrecompSource = "LibA.cpp";

		conf.Output = Configuration.OutputType.Lib;

		// No AddPublicDependency calls -- LibA is a true leaf.
	}
}
