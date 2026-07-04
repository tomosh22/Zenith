#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "DataStream/Zenith_DataStream.h"

// ============================================================================
// Flux_ShaderReflection sidecar (.spv.refl) round-trip — Flux Shader System
// Overhaul, Stage 3a (format v5: + specialization-constant table). Headless: the
// serializer is backend-neutral (all configs read the sidecar). Covers the v5
// header stamp, a zero-spec round-trip (unchanged binding behaviour), a populated
// round-trip (bindings + spec constants survive byte-exact through the stream),
// and GetSpecConstant lookup hit/miss.
//
// The version-mismatch guard in ReadFromDataStream is a hard Zenith_Assert (halts
// under the test harness), so it is validated by construction here — the header
// stamp test proves the writer emits exactly the version the reader demands; a v4
// stream would carry version 4 and fail that same equality in the reader.
// ============================================================================

namespace
{
	// The sidecar's magic + current version (mirrors the file-scope constants in
	// Flux_SlangCompiler.cpp — kept in sync deliberately; the stamp test guards it).
	constexpr u_int32 kuTestReflMagic   = 0x46525846; // 'FXRF'
	constexpr u_int32 kuTestReflVersion = 5;

	Flux_ReflectedBinding MakeBinding(u_int uSet, u_int uBinding, const char* szName, FluxResourceKind eKind)
	{
		Flux_ReflectedBinding xB;
		xB.m_uSet            = uSet;
		xB.m_uBinding        = uBinding;
		xB.m_strName         = szName;
		xB.m_uSize           = 64u;
		xB.m_eResourceKind   = eKind;
		xB.m_uDescriptorCount = 1u;
		xB.m_uStageMask      = FLUX_SHADER_STAGE_BIT_VERTEX | FLUX_SHADER_STAGE_BIT_FRAGMENT;
		xB.m_bStaticallyUsed = true;
		return xB;
	}

	Flux_ReflectedSpecConstant MakeSpec(const char* szName, u_int uId, u_int uDefault, const char* szType)
	{
		Flux_ReflectedSpecConstant xS;
		xS.m_strName        = szName;
		xS.m_uConstantId    = uId;
		xS.m_uSize          = 4u;
		xS.m_uDefaultValue  = uDefault;
		xS.m_strTypeName    = szType;
		return xS;
	}
}

ZENITH_TEST(FluxShaderReflection, SidecarHeaderStampIsV5)
{
	Flux_ShaderReflection xRefl;
	xRefl.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));
	xRefl.BuildLookupMap();

	Zenith_DataStream xStream;
	xRefl.WriteToDataStream(xStream);
	ZENITH_ASSERT_GE(xStream.GetCursor(), 8u, "at least a magic + version header written");

	const u_int32* puHeader = static_cast<const u_int32*>(xStream.GetData());
	ZENITH_ASSERT_EQ(puHeader[0], kuTestReflMagic, "sidecar leads with the FXRF magic");
	ZENITH_ASSERT_EQ(puHeader[1], kuTestReflVersion, "sidecar version is v5 (Stage 3a spec-constant table)");
}

ZENITH_TEST(FluxShaderReflection, RoundTripZeroSpecConstants)
{
	Flux_ShaderReflection xWrite;
	xWrite.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));
	xWrite.AddBinding(MakeBinding(1u, 3u, "g_xLightBuffer", FLUX_RESOURCE_KIND_STRUCTURED_BUFFER));
	xWrite.BuildLookupMap();

	Zenith_DataStream xStream;
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xRead.GetBindings().GetSize(), 2u, "both bindings survive round-trip");
	ZENITH_ASSERT_EQ(xRead.GetSpecConstants().GetSize(), 0u, "no spec constants when none were written");
	ZENITH_ASSERT_NOT_NULL(xRead.GetBinding("g_xLightBuffer"), "binding lookup rebuilt after read");
	const Flux_ReflectedBinding* pxLight = xRead.GetBinding("g_xLightBuffer");
	if (pxLight)
	{
		ZENITH_ASSERT_EQ(pxLight->m_uSet, 1u, "set survives");
		ZENITH_ASSERT_EQ(pxLight->m_uBinding, 3u, "binding survives");
	}
}

ZENITH_TEST(FluxShaderReflection, RoundTripPopulatedSpecConstants)
{
	Flux_ShaderReflection xWrite;
	xWrite.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));
	xWrite.AddSpecConstant(MakeSpec("FLUX_SC_VIEW_SHADOWS_PERMITTED", 0u, 1u, "bool"));
	xWrite.AddSpecConstant(MakeSpec("FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED", 1u, 1u, "bool"));
	xWrite.BuildLookupMap();

	Zenith_DataStream xStream;
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xRead.GetBindings().GetSize(), 1u, "binding survives alongside spec constants");
	ZENITH_ASSERT_EQ(xRead.GetSpecConstants().GetSize(), 2u, "both spec constants survive round-trip");

	const Flux_ReflectedSpecConstant& xSpec0 = xRead.GetSpecConstants().Get(0);
	const Flux_ReflectedSpecConstant& xSpec1 = xRead.GetSpecConstants().Get(1);
	ZENITH_ASSERT_STREQ(xSpec0.m_strName.c_str(), "FLUX_SC_VIEW_SHADOWS_PERMITTED", "spec 0 name survives");
	ZENITH_ASSERT_EQ(xSpec0.m_uConstantId, 0u, "spec 0 id survives");
	ZENITH_ASSERT_EQ(xSpec0.m_uDefaultValue, 1u, "spec 0 default survives");
	ZENITH_ASSERT_STREQ(xSpec0.m_strTypeName.c_str(), "bool", "spec 0 type survives");
	ZENITH_ASSERT_STREQ(xSpec1.m_strName.c_str(), "FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED", "spec 1 name survives");
	ZENITH_ASSERT_EQ(xSpec1.m_uConstantId, 1u, "spec 1 id survives");
}

ZENITH_TEST(FluxShaderReflection, GetSpecConstantHitAndMiss)
{
	Flux_ShaderReflection xRefl;
	xRefl.AddSpecConstant(MakeSpec("FLUX_SC_VIEW_SHADOWS_PERMITTED", 4u, 1u, "bool"));

	const Flux_ReflectedSpecConstant* pxHit = xRefl.GetSpecConstant("FLUX_SC_VIEW_SHADOWS_PERMITTED");
	ZENITH_ASSERT_NOT_NULL(pxHit, "present spec constant is found by name");
	if (pxHit) ZENITH_ASSERT_EQ(pxHit->m_uConstantId, 4u, "found spec constant carries its id");

	ZENITH_ASSERT_NULL(xRefl.GetSpecConstant("FLUX_SC_MISSING"), "absent name -> null");
	ZENITH_ASSERT_NULL(xRefl.GetSpecConstant(nullptr), "null name -> null");
}
