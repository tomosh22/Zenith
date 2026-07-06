using Sharpmake;
using System;
using System.IO;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

// =============================================================================
// Sharpmake entry point + the engine-only solution.
//
// This file is the [Sharpmake.Main]. It:
//   1. Verifies Build/Sharpmake_GameInstances.generated.cs is in sync with the
//      on-disk .zproj descriptors (SHA256 manifest guard) -- a stale generated
//      file throws, telling the user to run Build/regen.ps1.
//   2. Generates the engine-only solution (ZenithEngineSolution) -- engine libs,
//      Sentinels, FluxCompiler, font libs, the hub. ZERO games, ever.
//   3. Reflection-enumerates every concrete GameSolution subclass emitted into
//      the generated file and generates each as its own per-game .sln.
//
// There is NO all-games solution. Per-game slns + one engine sln only.
//
// The manifest guard references GeneratedGameManifest, which lives in the
// generated file. If that file is absent, THIS file fails to compile -- a
// deliberate failsafe: Sharpmake cannot run without the generated shells.
// =============================================================================

// Engine-only solution: engine leaf libs + aggregate + Sentinels + Windows tools
// + the hub. No games. Win64 only (engine AGDE coverage comes via per-game agde
// slns). Used for engine CI gates and pure engine work.
[Sharpmake.Generate]
public class ZenithEngineSolution : Solution
{
	public ZenithEngineSolution() : base(typeof(ZenithTarget))
	{
		// "Zenith_Engine" -> zenith_engine_win64.sln (Sharpmake lowercases the file).
		Name = "Zenith_Engine";

		AddTargets(new ZenithTarget
		{
			Platform = Platform.win64,
			DevEnv = DevEnv.vs2022,
			Optimization = Optimization.Debug | Optimization.Release,
			ToolsEnabled = ToolsEnabled.True | ToolsEnabled.False,
			RenderBackend = RenderBackend.Vulkan | RenderBackend.D3D12
		});
	}

	[Configure]
	public void ConfigureAll(Configuration conf, ZenithTarget target)
	{
		conf.SolutionFileName = "[solution.Name]_[target.Platform]";
		conf.SolutionPath = @"[solution.SharpmakeCsPath]";

		// Engine leaf libs (L0/L1/Physics/AI) + the aggregate engine lib.
		conf.AddProject<ZenithBaseLibProject>(target);
		conf.AddProject<ZenithECSLibProject>(target);
		conf.AddProject<ZenithPhysicsLibProject>(target);
		conf.AddProject<ZenithAILibProject>(target);
		conf.AddProject<ZenithProject>(target);

		// Sentinel leaf-proof exes: win64 + ToolsEnabled.False only (they declare
		// only those configs; the guard must match the project's target set).
		if (target.ToolsEnabled == ToolsEnabled.False)
		{
			conf.AddProject<SentinelECSProject>(target);
			conf.AddProject<SentinelPhysicsProject>(target);
			conf.AddProject<SentinelAIProject>(target);
		}

		// FluxCompiler (Slang -> SPIR-V) + the ZenithHub launcher: Vulkan-side only
		// (both link the Vulkan engine). Engine-sln membership only.
		if (target.RenderBackend == RenderBackend.Vulkan)
		{
			conf.AddProject<FluxCompilerProject>(target);
			conf.AddProject<ZenithHubProject>(target);
		}

		// MSDF font deps: tools-enabled builds only (both backends).
		if (target.ToolsEnabled == ToolsEnabled.True)
		{
			conf.AddProject<FreeTypeProject>(target);
			conf.AddProject<MsdfgenProject>(target);
			conf.AddProject<MsdfAtlasGenProject>(target);
		}
	}
}

// -----------------------------------------------------------------------------
// Manifest guard: prove the generated file matches the on-disk descriptors.
// -----------------------------------------------------------------------------
public static class ManifestGuard
{
	private static bool s_bVerified = false;

	public static void Verify()
	{
		if (s_bVerified) { return; }
		s_bVerified = true;

		string strBuildDir = ResolveBuildDir();
		string strRepoRoot = Path.GetDirectoryName(strBuildDir.TrimEnd('\\', '/'));
		string strGamesDir = Path.Combine(strRepoRoot, "Games");

		// On-disk set: repo-relative forward-slash path -> SHA256 (uppercase hex).
		Dictionary<string, string> xOnDisk = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		foreach (string strGameDir in Directory.GetDirectories(strGamesDir))
		{
			foreach (string strZproj in Directory.GetFiles(strGameDir, "*.zproj"))
			{
				string strRel = "Games/" + Path.GetFileName(strGameDir) + "/" + Path.GetFileName(strZproj);
				xOnDisk[strRel.Replace('\\', '/')] = Sha256Hex(strZproj);
			}
		}

		// Manifest set (from the generated file).
		Dictionary<string, string> xManifest = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		foreach (GeneratedGameManifest.Entry xEntry in GeneratedGameManifest.Entries)
		{
			xManifest[xEntry.DescriptorPath] = xEntry.Sha256;
		}

		List<string> xProblems = new List<string>();
		foreach (KeyValuePair<string, string> kv in xOnDisk)
		{
			if (!xManifest.ContainsKey(kv.Key))
			{
				xProblems.Add("descriptor added since last regen: " + kv.Key);
			}
			else if (!string.Equals(xManifest[kv.Key], kv.Value, StringComparison.OrdinalIgnoreCase))
			{
				xProblems.Add("descriptor changed since last regen: " + kv.Key);
			}
		}
		foreach (KeyValuePair<string, string> kv in xManifest)
		{
			if (!xOnDisk.ContainsKey(kv.Key))
			{
				xProblems.Add("descriptor removed since last regen: " + kv.Key);
			}
		}

		if (xProblems.Count > 0)
		{
			throw new Exception(
				"Build/Sharpmake_GameInstances.generated.cs is STALE relative to the .zproj descriptors:\n  " +
				string.Join("\n  ", xProblems) +
				"\nRun Build/regen.ps1 to regenerate the game shells before running Sharpmake.");
		}
	}

	// Locate <root>/Build robustly. Sharpmake may compile sources from a temp
	// copy (so [CallerFilePath] is not guaranteed to be the on-disk path), and
	// regen runs with the working directory set to Build/. Try the caller-file
	// dir, the CWD, and CWD/Build; accept the first that has Sharpmake_Common.cs
	// AND a sibling Games/ directory.
	private static string ResolveBuildDir([CallerFilePath] string strCallerFile = null)
	{
		List<string> xCandidates = new List<string>();
		if (!string.IsNullOrEmpty(strCallerFile))
		{
			xCandidates.Add(Path.GetDirectoryName(strCallerFile));
		}
		string strCwd = Directory.GetCurrentDirectory();
		xCandidates.Add(strCwd);
		xCandidates.Add(Path.Combine(strCwd, "Build"));

		foreach (string strCandidate in xCandidates)
		{
			if (string.IsNullOrEmpty(strCandidate)) { continue; }
			if (!File.Exists(Path.Combine(strCandidate, "Sharpmake_Common.cs"))) { continue; }
			string strParent = Path.GetDirectoryName(strCandidate.TrimEnd('\\', '/'));
			if (strParent != null && Directory.Exists(Path.Combine(strParent, "Games")))
			{
				return strCandidate;
			}
		}
		throw new Exception("ManifestGuard: could not locate the Build/ directory (tried CallerFilePath + CWD). " +
			"Run Sharpmake via Build/regen.ps1.");
	}

	private static string Sha256Hex(string strPath)
	{
		using (System.Security.Cryptography.SHA256 xSha = System.Security.Cryptography.SHA256.Create())
		{
			byte[] auHash = xSha.ComputeHash(File.ReadAllBytes(strPath));
			System.Text.StringBuilder xSb = new System.Text.StringBuilder(auHash.Length * 2);
			foreach (byte uByte in auHash) { xSb.Append(uByte.ToString("X2")); }
			return xSb.ToString();
		}
	}
}

// -----------------------------------------------------------------------------
// Entry point.
// -----------------------------------------------------------------------------
public static class Main
{
	[Sharpmake.Main]
	public static void SharpmakeMain(Sharpmake.Arguments arguments)
	{
		// Fail fast if the generated shells are stale relative to the descriptors.
		ManifestGuard.Verify();

		// Engine-only solution (no games).
		arguments.Generate<ZenithEngineSolution>();

		// Every concrete GameSolution subclass (emitted into
		// Sharpmake_GameInstances.generated.cs) becomes its own per-game solution.
		// Non-generic Generate(Type) -- one CLR type per game.
		foreach (Type xType in System.Reflection.Assembly.GetExecutingAssembly().GetTypes())
		{
			if (xType.IsClass && !xType.IsAbstract && typeof(GameSolution).IsAssignableFrom(xType))
			{
				arguments.Generate(xType);
			}
		}
	}
}
