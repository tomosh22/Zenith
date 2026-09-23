using Sharpmake;
using System;

// SentinelLibB -- the fixture's leaf-proof executable (mirrors
// Tests/Sentinel*/main.cpp + Build/Sharpmake_Sentinel*.cs). Deps declared in
// order LibB, LibA, MidLib -- so subject = LibB (the first). The
// RenderBackend.Vulkan-conditional block stays unresolved for the fixture's
// backend-prefix-less "fx_*" configs (scenario 22).
[Sharpmake.Generate]
public class SentinelLibBProject : ZenithBaseProject
{
	public SentinelLibBProject()
	{
		Name = "SentinelLibB";
		SourceRootPath = RootPath + "/Tests/SentinelLibB";
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.ProjectFileName = "[project.Name]_[target.Platform]";
		conf.ProjectPath = @"[project.SharpmakeCsPath]";

		conf.Output = Configuration.OutputType.Exe;

		conf.AddPublicDependency<LibBLibProject>(target);
		conf.AddPublicDependency<LibALibProject>(target);
		conf.AddPublicDependency<MidLibLibProject>(target);

		// Unresolved for the fixture's "fx_*" configs (no RenderBackend
		// prefix); resolved for an inline "vulkan_x" / "null_x" test config.
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			conf.AddPublicDependency<LibCLibProject>(target);
		}
		else if (target.RenderBackend == RenderBackend.D3D12)
		{
			conf.AddPublicDependency<LibALibProject>(target);
		}
		else if (target.RenderBackend == RenderBackend.Null)
		{
			conf.AddPublicDependency<LibBLibProject>(target);
		}
	}
}
