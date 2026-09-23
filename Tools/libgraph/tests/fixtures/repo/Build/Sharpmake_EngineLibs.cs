using Sharpmake;
using System;

// Single source of truth for the fixture's engine-lib set -- mirrors the
// real Build/Sharpmake_EngineLibs.cs. Comment-only mentions of class names
// elsewhere in this fixture (e.g. "ZenithBaseLibProject" in the real repo's
// equivalent file) must never produce a node or an edge; sharpmake.py
// strips comments before scanning, so this file deliberately does NOT name
// any fake class in a comment (that poison case lives inline in
// test_model.py instead, per the W1 brief).
public static class ZenithEngineLibs
{
	public struct Entry
	{
		public Type ProjectType;
		public Func<ZenithTarget, bool> Applies;

		public Entry(Type xProjectType, Func<ZenithTarget, bool> fnApplies = null)
		{
			ProjectType = xProjectType;
			Applies = fnApplies ?? (target => true);
		}
	}

	// Leaves first, aggregate last -- same order the real table uses.
	public static readonly Entry[] All = new Entry[]
	{
		new Entry(typeof(LibALibProject)),
		new Entry(typeof(LibBLibProject)),
		new Entry(typeof(LibCLibProject)),
		new Entry(typeof(MidLibLibProject)),
		new Entry(typeof(ZenithProject)),
	};

	public static void AddAllToSolution(Solution.Configuration conf, ZenithTarget target)
	{
		foreach (Entry xEntry in All)
		{
			if (xEntry.Applies(target))
			{
				conf.AddProject(xEntry.ProjectType, target);
			}
		}
	}

	public static void AddAllAsDependenciesOfAggregate(Project.Configuration conf, ZenithTarget target)
	{
		foreach (Entry xEntry in All)
		{
			if (xEntry.ProjectType == typeof(ZenithProject))
			{
				continue;
			}
			if (xEntry.Applies(target))
			{
				conf.AddPublicDependency(target, xEntry.ProjectType);
			}
		}
	}
}
