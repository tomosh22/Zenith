#pragma once

#include "Flux/Shaders/Generated/FluxShaderProgram.h"

struct Flux_SlangProgramDesc;

// Static description of a shader program in the Slang registry. The registry
// is the single source of truth that FluxCompiler walks to compile every
// program and that runtime code looks up via FluxShaderProgram IDs. New
// subsystems add entries when they migrate from GLSL to Slang.
struct Flux_ShaderRegistryEntry
{
	FluxShaderProgram m_eId               = FluxShaderProgram::COUNT;
	const char*       m_szName            = nullptr;  // friendly debug name, e.g. "TexturedQuad"
	const char*       m_szModuleName      = nullptr;  // path passed to ISession::loadModule, no extension
	const char*       m_szVertexEntry     = nullptr;  // null if program is not graphics or has no vs
	const char*       m_szFragmentEntry   = nullptr;
	const char*       m_szComputeEntry    = nullptr;  // null unless this is a compute program
	const char*       m_szTargetProfile   = "spirv_1_3";
	const char*       m_szSubsystem       = nullptr;  // generated-header grouping, e.g. "Quads"
};

namespace Flux_ShaderRegistry
{
	u_int GetProgramCount();
	const Flux_ShaderRegistryEntry& GetProgramByIndex(u_int uIndex);
	const Flux_ShaderRegistryEntry& GetProgram(FluxShaderProgram eId);

	// Convenience: fill a Flux_SlangProgramDesc from a registry entry so
	// callers don't repeat the field copy at every site.
	void DescribeProgram(FluxShaderProgram eId, Flux_SlangProgramDesc& xDescOut);

	// Build the on-disk filename stem used for emitted artifacts. Returns
	// `<m_szModuleName>.<entry>` for graphics stages and just `<m_szModuleName>`
	// for compute. The caller appends `.spv` or `.spv.refl`.
	std::string GetVertexArtifactStem(FluxShaderProgram eId);
	std::string GetFragmentArtifactStem(FluxShaderProgram eId);
	std::string GetComputeArtifactStem(FluxShaderProgram eId);
}
