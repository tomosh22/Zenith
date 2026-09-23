using Sharpmake;
using System;

// LibC -- shares the Zenith/Shared source root with LibB (scenario 9).
[Sharpmake.Generate]
public class LibCLibProject : ZenithBaseProject
{
	public LibCLibProject()
	{
		Name = "LibC";
		SourceRootPath = RootPath + "/Zenith/Shared";

		// LibB's own four files -- excluded from LibC one file at a time.
		SourceFilesExcludeRegex.Add(@"Shared\\LibB_Api\.h$");
		SourceFilesExcludeRegex.Add(@"Shared\\detail\\LibB_Inner\.h$");
		SourceFilesExcludeRegex.Add(@"Shared\\LibB_Sibling\.h$");
		SourceFilesExcludeRegex.Add(@"Shared\\LibB_Impl\.cpp$");
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.PrecompHeader = "FxPch.h";
		conf.Output = Configuration.OutputType.Lib;

		// Scenario 12: declared-unused. LibC declares MidLib, but nothing in
		// LibC includes a MidLib header except through the PCH.
		conf.AddPublicDependency<MidLibLibProject>(target);

		// Scenario 14: LibC declares LibB (LibC_Api.h includes LibB_Api.h),
		// so LibC -> LibB is declared+used.
		conf.AddPublicDependency<LibBLibProject>(target);
	}
}
