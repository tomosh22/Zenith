#include "Zenith.h"

#include "Flux/Flux_VertexLayoutValidation.h"
#include "Flux/Flux_Pipeline.h"        // Flux_PipelineSpecification (the complete type)
#include "Flux/Flux_BackendTypes.h"    // Flux_Shader::GetReflection

// The one place the pipeline spec is DEREFERENCED for validation. Kept out of
// line so the Null/D3D12 pipeline headers — which only forward-declare
// Flux_PipelineSpecification — can call it from their FromSpecification stubs
// without taking an include edge that would cycle through the backend aliases.
void Flux_ValidateVertexLayoutForSpec(const Flux_PipelineSpecification& xSpec)
{
	// A spec with no shader never reaches a real pipeline build; guarding here
	// keeps the validator from being the thing that crashes on a malformed spec,
	// which would hide the actual defect.
	if (!xSpec.m_pxShader)
	{
		return;
	}

	Flux_AssertVertexLayoutMatchesReflection(xSpec.m_pxShader->GetReflection(), xSpec.m_pxVertexLayout, xSpec.m_pxShader->GetProgramName());
}
