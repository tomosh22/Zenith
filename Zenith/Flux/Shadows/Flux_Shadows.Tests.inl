#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// Shadow sampling quality-flag unit tests. No GPU, no engine.
//
// Hosted at the end of Flux_Shadows.cpp (always-linked feature TU) so the
// static-init registrations survive MSVC dead-strip and the file-static
// BuildShadowQualityFlags is in scope.
//
// The flag word is packed into a FLOAT (ShadowSampling.g_xParams2.y) and read
// back with uint() in Common/ShadowSampling.slang. Two things can silently
// break that and produce no symptom other than the wrong shadow filter:
//   * a renumbered/overlapping FLUX_SHADOW_FLAG_* bit, and
//   * the float round trip, which the header asserts is exact "for small
//     integer values" — pinned here for the whole reachable range rather than
//     trusted as a comment.
// The Slang-side SHADOW_FLAG_* mirror stays comment-enforced (same as the
// ShadowSamplingLayout byte-layout mirror it sits beside); these tests pin the
// C++ half so a one-sided renumber is caught here.
// ============================================================================

// ---- Bit values are distinct, contiguous, and non-overlapping ---------------

ZENITH_TEST(FluxShadows, QualityFlagBitsArePowersOfTwoAndDistinct)
{
	const u_int auFlags[5] = {
		FLUX_SHADOW_FLAG_PCSS_CASCADE0_ONLY,
		FLUX_SHADOW_FLAG_CHEAP_FAR_CASCADES,
		FLUX_SHADOW_FLAG_ROUGHNESS_GATED_PCSS,
		FLUX_SHADOW_FLAG_CASCADE_FALLBACK_BLEND,
		FLUX_SHADOW_FLAG_CONTACT_SHADOWS,
	};

	u_int uSeen = 0u;
	for (u_int u = 0; u < 5u; ++u)
	{
		ZENITH_ASSERT_NE(auFlags[u], 0u, "flag %u must not be zero", u);
		ZENITH_ASSERT_EQ(auFlags[u] & (auFlags[u] - 1u), 0u,
			"flag %u (0x%x) must be a single bit", u, auFlags[u]);
		ZENITH_ASSERT_EQ(uSeen & auFlags[u], 0u,
			"flag %u (0x%x) overlaps an earlier flag", u, auFlags[u]);
		uSeen |= auFlags[u];
	}
	// Mirrors SHADOW_FLAG_* 1<<0 .. 1<<4 in Common/ShadowSampling.slang.
	ZENITH_ASSERT_EQ(uSeen, 0x1Fu, "the five flags must occupy exactly bits 0-4");
}

// ---- OR-fold ---------------------------------------------------------------

ZENITH_TEST(FluxShadows, BuildQualityFlagsSetsOneBitPerSwitch)
{
	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(false, false, false, false, false), 0u,
		"all switches off must produce an empty word");

	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(true, false, false, false, false),
		FLUX_SHADOW_FLAG_PCSS_CASCADE0_ONLY, "PCSS-cascade0-only maps to its own bit");
	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(false, true, false, false, false),
		FLUX_SHADOW_FLAG_CHEAP_FAR_CASCADES, "cheap-far-cascades maps to its own bit");
	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(false, false, true, false, false),
		FLUX_SHADOW_FLAG_ROUGHNESS_GATED_PCSS, "roughness-gated-PCSS maps to its own bit");
	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(false, false, false, true, false),
		FLUX_SHADOW_FLAG_CASCADE_FALLBACK_BLEND, "cascade-fallback-blend maps to its own bit");
	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(false, false, false, false, true),
		FLUX_SHADOW_FLAG_CONTACT_SHADOWS, "contact-shadows maps to its own bit");

	ZENITH_ASSERT_EQ(BuildShadowQualityFlags(true, true, true, true, true), 0x1Fu,
		"all switches on must set every bit");
}

ZENITH_TEST(FluxShadows, DefaultSamplingFlagsMatchTheShippingSwitches)
{
	// The shipping defaults: cascade-0-only PCSS, cheap far cascades, cascade
	// fallback blend and contact shadows on; the roughness gate OFF pending
	// visual sign-off. FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS seeds the GPU mirror
	// before the first UpdateShadowMatrices, so it must agree with the fold.
	ZENITH_ASSERT_EQ(FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS,
		BuildShadowQualityFlags(true, true, false, true, true),
		"the seeded default must equal the fold of the default switch positions");
	ZENITH_ASSERT_EQ(FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS & FLUX_SHADOW_FLAG_ROUGHNESS_GATED_PCSS, 0u,
		"the roughness gate must stay off by default");

	// The struct default must match the constant it is initialised from.
	const Flux_ShadowSamplingConfig xConfig;
	ZENITH_ASSERT_EQ(xConfig.m_uQualityFlags, FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS,
		"Flux_ShadowSamplingConfig must default to the same word");
}

// ---- The float channel round-trips every reachable word exactly -------------

ZENITH_TEST(FluxShadows, QualityFlagsSurviveTheFloatChannelExactly)
{
	// g_xParams2.y is a float. Every combination of the five bits (0..31) must
	// come back bit-identical through uint(), which is what the shader does.
	for (u_int uFlags = 0u; uFlags <= 0x1Fu; ++uFlags)
	{
		const float fPacked   = static_cast<float>(uFlags);
		const u_int uUnpacked = static_cast<u_int>(fPacked);
		ZENITH_ASSERT_EQ(uUnpacked, uFlags,
			"flag word 0x%x must survive the float channel", uFlags);
	}
}

ZENITH_TEST(FluxShadows, GPUMirrorSeedsFlagsAndThresholdIntoParams2)
{
	// Params2 = (pcssEnabled, qualityFlags, roughnessThreshold, spare). The
	// shader reads .y as the flag word and .z as the cheap-tier roughness
	// floor; a re-ordering here silently re-points both.
	const Flux_ShadowSamplingGPU xGPU;
	ZENITH_ASSERT_EQ_FLOAT(xGPU.m_xParams2.x, 1.0f, 1e-6f, "params2.x = pcssEnabled");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xGPU.m_xParams2.y), FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS,
		"params2.y = quality flags");
	ZENITH_ASSERT_EQ_FLOAT(xGPU.m_xParams2.z, 0.6f, 1e-6f, "params2.z = cheap-tier roughness floor");

	// Byte-for-byte mirror of ShadowSamplingLayout (6x float4).
	ZENITH_ASSERT_EQ(u_int(sizeof(Flux_ShadowSamplingGPU)), 96u,
		"the GPU mirror must stay 6x float4 to match ShadowSamplingLayout");
}
