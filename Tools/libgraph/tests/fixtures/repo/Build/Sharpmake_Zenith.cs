using Sharpmake;
using System;

// The "Zenith" aggregate -- mirrors the real ZenithProject: source root
// under Zenith/EngineComposition, expands to a declared edge to every other
// Entry-table node via ZenithEngineLibs.AddAllAsDependenciesOfAggregate.
[Sharpmake.Generate]
public class ZenithProject : ZenithBaseProject
{
	public ZenithProject()
	{
		Name = "Zenith";
		SourceRootPath = RootPath + "/Zenith/EngineComposition";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.PrecompHeader = "FxPch.h";
		conf.Output = Configuration.OutputType.Lib;

		// Scenario: aggregate expansion -- a declared edge to every other
		// Entry-table node (LibA, LibB, LibC, MidLib), not to itself.
		ZenithEngineLibs.AddAllAsDependenciesOfAggregate(conf, target);
	}
}
