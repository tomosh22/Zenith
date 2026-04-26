#pragma once

#include "Flux/Slang/Flux_ShaderRegistry.h"

#include <string>

class Flux_ShaderReflection;

// Generates the C++ headers under Zenith/Flux/Shaders/Generated/ from the
// shader registry plus per-program reflection data captured during compile.
// Two artifact families:
//   - FluxShaderProgram.h       — program-ID enum (one entry per registered program)
//   - <Subsystem>.h             — per-subsystem CB structs, parameter-block paths,
//                                 resource path constants, descriptor-count constants
//
// Determinism: emission order is fixed by registry iteration order, field
// emission order is fixed by reflection iteration order. Re-running the
// generator with no input changes must produce byte-identical output, which
// CI checks by diffing the working tree against the regenerated files.
namespace Flux_CodeGenerator
{
	struct ProgramReflection
	{
		FluxShaderProgram          m_eId        = FluxShaderProgram::COUNT;
		const Flux_ShaderReflection* m_pxReflection = nullptr;
	};

	// Emit Generated/FluxShaderProgram.h with one enum value per registered
	// program. Idempotent. szOutputDir must be the absolute path to the
	// Generated folder, with a trailing separator.
	bool WriteProgramEnumHeader(const char* szOutputDir);

	// Emit Generated/<Subsystem>.h aggregating every program in that
	// subsystem. Currently emits parameter-block path constants and CB
	// struct stubs with static_asserts on size; field-level offset asserts
	// will land once a CB-bearing program is ported.
	bool WriteSubsystemHeader(const char* szOutputDir,
							   const char* szSubsystem,
							   const ProgramReflection* axPrograms,
							   u_int uProgramCount);

	// Convenience wrapper: emit FluxShaderProgram.h plus one per-subsystem
	// header for every subsystem that appears in the registry. axAll is the
	// per-program reflection captured during compile, indexed by program ID.
	bool WriteAllHeaders(const char* szOutputDir,
						  const ProgramReflection* axAll,
						  u_int uProgramCount);

	// Pure-string variants used by unit tests to verify generator output
	// without touching the file system. The Write* functions above are thin
	// wrappers that build the string then forward to WriteIfChanged.
	std::string BuildProgramEnumContent();
	std::string BuildSubsystemHeaderContent(const char* szSubsystem,
											 const ProgramReflection* axPrograms,
											 u_int uProgramCount);
}
