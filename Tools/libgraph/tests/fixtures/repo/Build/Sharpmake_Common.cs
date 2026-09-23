using Sharpmake;
using System;

// Fixture mirror of the real repo's Build/Sharpmake_Common.cs: the abstract
// base every concrete project class derives from, one hop away from
// Sharpmake's own Project. sharpmake.py's kind classifier recognises this
// exact shape (a non-abstract class whose immediate base is
// "ZenithBaseProject").
public abstract class ZenithBaseProject : Project
{
	protected new static string RootPath = @"[project.SharpmakeCsPath]/..";

	public ZenithBaseProject() : base(typeof(ZenithTarget))
	{
		SourceFilesExtensions = new Strings(".cpp", ".c", ".h", ".inl");
		SourceFilesCompileExtensions = new Strings(".cpp", ".c");
	}
}
