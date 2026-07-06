#pragma once

namespace ZenithHub_SelfTest
{
	// Run the hub's self-tests: name-validation vectors (shared with the PS suite),
	// descriptor parsing (fixtures), and a scan of the real Games tree. Prints
	// results and returns the number of failures (0 == all green).
	int Run(const char* szRepoRoot);
}
