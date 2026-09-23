using Sharpmake;
using System;

// Fixture mirror of the real repo's Build/Sharpmake_Games.cs: the abstract
// GameProject/GameSolution bases. Every concrete generated game class
// derives from GameProject and writes only `GameName => "..."`
// (expression-bodied) -- Name and SourceRootPath come from this base's own
// constructor, exactly like the real repo.
public abstract class GameProject : ZenithBaseProject
{
	public abstract string GameName { get; }

	public GameProject()
	{
		Name = GameName;
		SourceRootPath = RootPath + "/Games/" + GameName;
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = RootPath + "/Games/" + GameName + "/build";

		// Every game gets this ONE inherited edge -- scenario: "the
		// inherited AddPublicDependency<ZenithProject>() applies to every
		// game" (mirrors Sharpmake_Games.cs:258 in the real repo).
		conf.AddPublicDependency<ZenithProject>(target);
	}
}

// Per-game solution -- excluded entirely from sharpmake.py's node scan (a
// Solution-family base produces a .sln, not a vcxproj).
public abstract class GameSolution : Solution
{
	public abstract string GameName { get; }

	public GameSolution() : base(typeof(ZenithTarget))
	{
		Name = GameName;
	}
}
