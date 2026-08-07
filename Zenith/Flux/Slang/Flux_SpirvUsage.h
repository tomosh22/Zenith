#pragma once

#include <cstdint>

// =====================================================================
// Flux_SpirvUsage — pure SPIR-V introspection: "does this module actually
// USE the variables bound to descriptor set N?"
//
// WHY THIS EXISTS. Reflection carries a per-binding `m_bStaticallyUsed` bit
// baked from Slang's IMetadata::isParameterLocationUsed, which is what tells a
// member the program really samples apart from one it merely DECLARES via the
// #include'd spine (Common/Bindings.slang lists g_xGlobal / g_xView /
// g_axTextures in EVERY program's reflection). That bit is trustworthy for the
// GLOBAL/VIEW members it was built for — and it is WRONG for the BINDLESS
// table. Slang answers `false` for the unbounded `g_axTextures[]` array even in
// a program whose fragment stage samples it, which was measured directly: with
// the grass G-buffer draw's UseBindlessTextures(2) removed, Vulkan reported
// "VkPipeline ... uses set 2 but that set is not bound" (so SPIR-V says the set
// IS statically used) while the reflection bit for that same program read 0.
// A validator gated on Slang's answer for set 2 is therefore inert, which is
// worse than no validator — it reads green while the hazard is live.
//
// SPIR-V itself is unambiguous, and the backend already holds the module bytes
// on both shader-load paths (runtime Slang compile on Windows, `.spv` file on
// Android). So we ask the module: is a variable decorated `DescriptorSet == N`
// referenced from inside any function body? Vulkan's own "statically used" is
// per-entry-point-call-graph; scanning EVERY function is a superset of that,
// which errs toward demanding a bind that a draw could technically skip —
// never toward missing one it needs.
//
// The opcode allowlist below is deliberately narrow: only operands that are
// unambiguously <id>s are compared, so a literal can never be mistaken for a
// variable id and raise a false demand. A pointer reaches a sampler through
// OpAccessChain/OpLoad in every Slang-emitted module, so the narrow list costs
// nothing in practice.
//
// Pure (no Vulkan, no Slang, no device) -> unit-tested against synthetic
// modules in every build configuration.
// =====================================================================
namespace Flux_SpirvUsage
{
	inline constexpr uint32_t kuMAGIC              = 0x07230203u;   // SPIR-V module magic (little-endian)
	inline constexpr uint32_t kuHEADER_WORDS       = 5u;
	inline constexpr uint32_t kuDECORATION_DESCRIPTOR_SET = 34u;    // SpvDecorationDescriptorSet

	inline constexpr uint32_t kuOP_FUNCTION        = 54u;
	inline constexpr uint32_t kuOP_FUNCTION_END    = 56u;
	inline constexpr uint32_t kuOP_FUNCTION_CALL   = 57u;
	inline constexpr uint32_t kuOP_IMAGE_TEXEL_PTR = 60u;
	inline constexpr uint32_t kuOP_LOAD            = 61u;
	inline constexpr uint32_t kuOP_STORE           = 62u;
	inline constexpr uint32_t kuOP_ACCESS_CHAIN    = 65u;
	inline constexpr uint32_t kuOP_IN_BOUNDS_ACCESS_CHAIN = 66u;
	inline constexpr uint32_t kuOP_PTR_ACCESS_CHAIN       = 67u;
	inline constexpr uint32_t kuOP_DECORATE        = 71u;
	inline constexpr uint32_t kuOP_COPY_OBJECT     = 83u;
	inline constexpr uint32_t kuOP_SAMPLED_IMAGE   = 86u;

	// At most this many distinct set-N variables are tracked. The BINDLESS set
	// holds exactly one (the g_axTextures table), so 8 is slack, not a budget.
	inline constexpr uint32_t kuMAX_CANDIDATES     = 8u;
}

// The inclusive operand-word range of an instruction that is unambiguously a
// list of <id>s, or an empty range (uFirstOut == 0) for anything else. Offsets
// are from the opcode word. Deliberately NARROW: comparing only certain-<id>
// operands means a literal can never be mistaken for a variable id and raise a
// false demand, and a pointer reaches a sampler through OpAccessChain/OpLoad in
// every Slang-emitted module, so the omissions cost nothing in practice.
inline void Flux_SpirvIdOperandRange(uint32_t uOpcode, uint32_t uLength, uint32_t& uFirstOut, uint32_t& uLastOut)
{
	using namespace Flux_SpirvUsage;
	uFirstOut = 0u;
	uLastOut  = 0u;
	switch (uOpcode)
	{
	case kuOP_LOAD:                                                  // [3] = pointer
	case kuOP_ACCESS_CHAIN: case kuOP_IN_BOUNDS_ACCESS_CHAIN:        // [3] = base
	case kuOP_PTR_ACCESS_CHAIN:
	case kuOP_COPY_OBJECT:                                           // [3] = operand
	case kuOP_IMAGE_TEXEL_PTR:                                       // [3] = image
		uFirstOut = 3u; uLastOut = 3u; break;
	case kuOP_SAMPLED_IMAGE:                                         // [3] = image, [4] = sampler
		uFirstOut = 3u; uLastOut = 4u; break;
	case kuOP_STORE:                                                 // [1] = pointer, [2] = object
		uFirstOut = 1u; uLastOut = 2u; break;
	case kuOP_FUNCTION_CALL:                                         // [4..] = arguments ([3] is the callee)
		uFirstOut = 4u; uLastOut = (uLength > 0u) ? uLength - 1u : 0u; break;
	default:
		break;
	}
}

// Collect the ids decorated `DescriptorSet == uSet`. Returns how many landed in
// pauOut (capped at uMaxOut). A malformed instruction stops the walk rather than
// spinning on a zero length.
inline uint32_t Flux_SpirvCollectSetVariables(const uint32_t* puWords, uint32_t uWordCount, uint32_t uSet,
                                              uint32_t* pauOut, uint32_t uMaxOut)
{
	using namespace Flux_SpirvUsage;
	uint32_t uCount = 0u;
	for (uint32_t u = kuHEADER_WORDS; u < uWordCount; )
	{
		const uint32_t uOpcode = puWords[u] & 0xFFFFu;
		const uint32_t uLength = puWords[u] >> 16u;
		if (uLength == 0u || u + uLength > uWordCount) break;
		if (uOpcode == kuOP_DECORATE && uLength >= 4u &&
			puWords[u + 2] == kuDECORATION_DESCRIPTOR_SET && puWords[u + 3] == uSet &&
			uCount < uMaxOut)
		{
			pauOut[uCount++] = puWords[u + 1];
		}
		u += uLength;
	}
	return uCount;
}

// Does the SPIR-V module reference, from inside a function body, any variable
// decorated `DescriptorSet == uSet`? puWords/uWordCount are the module's
// 32-bit words (byte size / 4). A null, truncated or non-SPIR-V blob returns
// false — this is a diagnostic input, never a place to hard-fail a boot.
inline bool Flux_SpirvUsesDescriptorSet(const uint32_t* puWords, uint32_t uWordCount, uint32_t uSet)
{
	using namespace Flux_SpirvUsage;

	if (puWords == nullptr || uWordCount <= kuHEADER_WORDS || puWords[0] != kuMAGIC)
	{
		return false;
	}

	uint32_t auCandidates[kuMAX_CANDIDATES] = {};
	const uint32_t uNumCandidates = Flux_SpirvCollectSetVariables(puWords, uWordCount, uSet, auCandidates, kuMAX_CANDIDATES);
	if (uNumCandidates == 0u)
	{
		return false;
	}

	// Is any candidate referenced from inside a function BODY? A decoration, name
	// or type reference in the module's header section is a DECLARATION, and that
	// distinction is the whole point of this scan.
	bool bInFunction = false;
	for (uint32_t u = kuHEADER_WORDS; u < uWordCount; )
	{
		const uint32_t uOpcode = puWords[u] & 0xFFFFu;
		const uint32_t uLength = puWords[u] >> 16u;
		if (uLength == 0u || u + uLength > uWordCount) break;

		if (uOpcode == kuOP_FUNCTION)          bInFunction = true;
		else if (uOpcode == kuOP_FUNCTION_END) bInFunction = false;
		else if (bInFunction)
		{
			uint32_t uFirst = 0u, uLast = 0u;
			Flux_SpirvIdOperandRange(uOpcode, uLength, uFirst, uLast);
			for (uint32_t uOperand = uFirst; uOperand != 0u && uOperand <= uLast && uOperand < uLength; uOperand++)
			{
				for (uint32_t c = 0; c < uNumCandidates; c++)
				{
					if (puWords[u + uOperand] == auCandidates[c]) return true;
				}
			}
		}
		u += uLength;
	}
	return false;
}
