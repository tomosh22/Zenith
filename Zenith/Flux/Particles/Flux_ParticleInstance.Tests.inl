#include "UnitTests/Zenith_UnitTests.h"
#include "FileAccess/Zenith_FileAccess.h"   // the source-text pin reads the compute shader

#include <cstring>   // std::memcpy — reading the packed lane back out of the raw bytes
#include <string>    // bounded search over the shader source

// ============================================================================
// Flux_ParticleInstance packed-lane contract (compressed-vertex Phase 6).
//
// This stream has TWO producers: the CPU emitter path (this struct) and the
// ParticleUpdate compute shader, which writes the identical bytes as five raw u32
// words because a Slang structured-buffer element holding a float4 would be
// std430-padded to 32 B. The C++ word count is pinned against the generated stride
// by a static_assert in Flux_ParticleData.h; the SHADER's word count has no
// reflectable form at all, so SlangWriterWordCountMatchesTheMirror below parses it
// out of the .slang source — the only cross-language tie the two spellings have.
// The remaining tests cover what those pins cannot see — that the CPU producer
// puts the right bits in the right lane, and that the world-position lane really
// did keep full float range.
// ============================================================================

namespace
{
	u_int ReadParticleInstanceWordAt(const Flux_ParticleInstance& xInstance, u_int uAttributeIndex)
	{
		const u_int uOffset = Flux_Generated_Particles::Particles::kaxVertexAttribs[uAttributeIndex].m_uOffset;
		u_int uWord = 0u;
		std::memcpy(&uWord, reinterpret_cast<const unsigned char*>(&xInstance) + uOffset, sizeof(uWord));
		return uWord;
	}
}

ZENITH_TEST(ParticleInstance, ConstructorPacksColourAtTheGeneratedOffset)
{
	// Elements 0/1 of the layout are the shared unit quad on binding 0; the two
	// instance lanes are 2 (position/size) and 3 (colour).
	const Flux_ParticleInstance xInstance(
		Zenith_Maths::Vector3(1.5f, -2.25f, 3.75f), 0.5f,
		Zenith_Maths::Vector4(1.0f, 0.85f, 0.3f, 0.9f));

	ZENITH_ASSERT_EQ(ReadParticleInstanceWordAt(xInstance, 3u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.85f, 0.3f, 0.9f)),
		"the colour lane must hold the unorm8x4 word at the generated offset");

	// The position lane is still four raw floats — read the first back through the
	// bytes to prove the colour pack did not shift it.
	float fPositionX = 0.0f;
	std::memcpy(&fPositionX,
		reinterpret_cast<const unsigned char*>(&xInstance)
			+ Flux_Generated_Particles::Particles::kaxVertexAttribs[2u].m_uOffset,
		sizeof(fPositionX));
	ZENITH_ASSERT_EQ_FLOAT(fPositionX, 1.5f, 0.0f, "the world position lane must stay full float");
}

ZENITH_TEST(ParticleInstance, WorldPositionKeepsFarFromOriginPrecision)
{
	// THE reason the position lane was not narrowed: half floats step by 2 above
	// 2048, so a particle a kilometre out would snap to a visible grid. 4096.5 and
	// 100000.25 are exact in float32 and are NOT representable in half.
	const Flux_ParticleInstance xInstance(
		Zenith_Maths::Vector3(4096.5f, -100000.25f, 65537.0f), 0.125f,
		Zenith_Maths::Vector4(1.0f));

	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_xPosition_Size.x, 4096.5f, 0.0f, "sub-unit precision past 2048");
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_xPosition_Size.y, -100000.25f, 0.0f, "sub-unit precision far from origin");
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_xPosition_Size.z, 65537.0f, 0.0f, "integer exactness past half's 65504 ceiling");
	ZENITH_ASSERT_EQ_FLOAT(xInstance.m_xPosition_Size.w, 0.125f, 0.0f, "the radius shares the lane and must be exact too");
}

ZENITH_TEST(ParticleInstance, ComputeWriterStrideMatchesTheCPUProducer)
{
	// The shared word count both producers stride by. The static_assert beside it
	// ties it to the generated stride; this states the other half of the identity —
	// that the CPU struct is exactly that many words wide, with no tail padding.
	ZENITH_ASSERT_EQ(static_cast<u_int>(sizeof(Flux_ParticleInstance)),
		uFLUX_PARTICLE_INSTANCE_WORDS * 4u,
		"the CPU instance struct must be exactly the compute writer's word stride");
	ZENITH_ASSERT_EQ(static_cast<u_int>(sizeof(Flux_ParticleInstance)), 20u,
		"Flux_ParticleInstance is the 20-byte packed instance (was 32 before the colour flip)");
}

ZENITH_TEST(ParticleInstance, SlangWriterWordCountMatchesTheMirror)
{
	// THE cross-language pin. uINSTANCE_WORDS in Flux_ParticleUpdate.slang is a plain
	// `static const uint` — it reflects into nothing (no spec-constant entry, no
	// sidecar field, no generated header), so no static_assert can reach it, and an
	// RWStructuredBuffer<uint> reflects a 4-byte element that says nothing about 5.
	// Bumping the C++ mirror without the shader (or vice versa) would stride the
	// compute writes wrong with every build and unit green. So: read the shader
	// SOURCE and parse the literal. If the source tree is absent (a packaged build),
	// pass with a log — the drift this guards happens in dev/CI trees, which always
	// have the source.
	const char* szPath = SHADER_SOURCE_ROOT "Particles/Flux_ParticleUpdate.slang";
	if (!Zenith_FileAccess::FileExists(szPath))
	{
		Zenith_Log(LOG_CATEGORY_PARTICLES, "SlangWriterWordCountMatchesTheMirror: shader source absent (%s) — packaged tree, pin skipped", szPath);
		return;
	}

	uint64_t ulSize = 0u;
	char* pcData = Zenith_FileAccess::ReadFile(szPath, ulSize);
	ZENITH_ASSERT_TRUE(pcData != nullptr, "shader source exists but failed to read");
	if (pcData == nullptr)
	{
		return;
	}
	const std::string strSource(pcData, static_cast<size_t>(ulSize));
	Zenith_FileAccess::FreeFileData(pcData);

	const size_t ulDecl = strSource.find("static const uint uINSTANCE_WORDS");
	ZENITH_ASSERT_TRUE(ulDecl != std::string::npos,
		"Flux_ParticleUpdate.slang must declare uINSTANCE_WORDS on one line (the pin parses it)");
	if (ulDecl == std::string::npos)
	{
		return;
	}
	const size_t ulEquals = strSource.find('=', ulDecl);
	ZENITH_ASSERT_TRUE(ulEquals != std::string::npos, "uINSTANCE_WORDS declaration must carry its initialiser");
	if (ulEquals == std::string::npos)
	{
		return;
	}
	const u_int uShaderWords = static_cast<u_int>(atoi(strSource.c_str() + ulEquals + 1));
	ZENITH_ASSERT_EQ(uShaderWords, uFLUX_PARTICLE_INSTANCE_WORDS,
		"Flux_ParticleUpdate.slang's uINSTANCE_WORDS has drifted from uFLUX_PARTICLE_INSTANCE_WORDS — "
		"the compute writer would stride the instance buffer differently from the vertex fetch");
}

ZENITH_TEST(ParticleInstance, AuthoredEmitterColoursRoundTripWithinTheUnormStep)
{
	// Every emitter colour in the engine is authored in [0,1] and the lane only ever
	// carries a lerp between two such endpoints, so unorm8 is exact to the authoring
	// precision. The endpoints themselves are pinned with zero tolerance.
	const float fStep = 1.0f / 255.0f;
	for (u_int u = 0u; u <= 8u; ++u)
	{
		const float fT = static_cast<float>(u) / 8.0f;
		const Zenith_Maths::Vector4 xIn(1.0f, 0.85f * fT, 0.30f * fT, 1.0f - fT);
		const Flux_ParticleInstance xInstance(Zenith_Maths::Vector3(0.0f), 1.0f, xIn);
		const Zenith_Maths::Vector4 xOut = xInstance.GetColour();

		ZENITH_ASSERT_EQ_FLOAT(xOut.x, xIn.x, 0.0f, "a saturated channel is exact at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, xIn.y, fStep, "emitter green at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, xIn.z, fStep, "emitter blue at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.w, xIn.w, fStep, "emitter alpha at step %u", u);
	}
}
