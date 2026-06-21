#pragma once

#include "Flux/Slang/Flux_ShaderCatalog.h"

#include <string>

class Flux_ShaderReflection;

// Generates the C++ headers under Zenith/Flux/Shaders/Generated/ from the
// shader catalog plus per-program reflection data captured during compile.
// One artifact family:
//   - <Subsystem>.h             — per-subsystem CB structs, parameter-block paths,
//                                 resource path constants, descriptor-count constants
// (The former program-ID enum header is gone — shader handles are decls now.)
//
// Determinism: emission order is fixed by catalog iteration order, field
// emission order is fixed by reflection iteration order. Re-running the
// generator with no input changes must produce byte-identical output, which
// CI checks by diffing the working tree against the regenerated files.
namespace Flux_CodeGenerator
{
	struct ProgramReflection
	{
		const Flux_ShaderDecl*       m_pxDecl       = nullptr; // the program's catalog decl (subsystem grouping + name)
		const Flux_ShaderReflection* m_pxReflection = nullptr;
	};

	// Emit Generated/<Subsystem>.h aggregating every program in that
	// subsystem. Emits parameter-block path constants and CB struct stubs with
	// static_asserts on size + field-level offset asserts.
	bool WriteSubsystemHeader(const char* szOutputDir,
							   const char* szSubsystem,
							   const ProgramReflection* axPrograms,
							   u_int uProgramCount);

	// Convenience wrapper: emit one per-subsystem header for every subsystem
	// that appears in the catalog. axAll is the per-program reflection captured
	// during compile.
	bool WriteAllHeaders(const char* szOutputDir,
						  const ProgramReflection* axAll,
						  u_int uProgramCount);

	// Pure-string variant used by unit tests to verify generator output without
	// touching the file system. WriteSubsystemHeader is a thin wrapper that
	// builds the string then forwards to WriteIfChanged.
	std::string BuildSubsystemHeaderContent(const char* szSubsystem,
											 const ProgramReflection* axPrograms,
											 u_int uProgramCount);
}
