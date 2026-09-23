using Sharpmake;
using System;

// MidLib -- the fixture's Entry-table middleware lib, mirroring ZenithJolt /
// ZenithImGui: source root under Middleware/, no dependency edges at all.
[Sharpmake.Generate]
public class MidLibLibProject : ZenithBaseProject
{
	public MidLibLibProject()
	{
		Name = "MidLib";
		SourceRootPath = RootPath + "/Middleware/MidLib";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.Output = Configuration.OutputType.Lib;

		// DEPENDENCY: NONE -- see ZenithJolt/ZenithImGui's real header
		// comments for why a vendored leaf carries no ZenithCore-equivalent
		// edge.
	}
}
