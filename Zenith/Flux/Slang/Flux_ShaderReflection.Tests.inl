#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "DataStream/Zenith_DataStream.h"

// ============================================================================
// Flux_ShaderReflection sidecar (.spv.refl) round-trip — Flux Shader System
// Overhaul, Stage 3a (v5: + specialization-constant table) and the compressed-
// vertex work, T2.a (v6: + vertex-input table + per-binding strides). Headless:
// the serializer is backend-neutral (all configs read the sidecar). Covers the
// version stamp, a zero-spec round-trip (unchanged binding behaviour), a populated
// round-trip (bindings + spec constants survive byte-exact through the stream),
// GetSpecConstant lookup hit/miss, the vertex-input round-trip + tight-pack rules,
// the pure format vocabulary ([VtxFmt] parse / inference / override validation),
// and the adopt-once merge.
//
// The version-mismatch guard in ReadFromDataStream is a hard Zenith_Assert (halts
// under the test harness), so it is validated by construction here — the header
// stamp test proves the writer emits exactly the version the reader demands; a v5
// stream would carry version 5 and fail that same equality in the reader.
// ============================================================================

namespace
{
	// The sidecar's magic + current version (mirrors the file-scope constants in
	// Flux_SlangCompiler.cpp — kept in sync deliberately; the stamp test guards it).
	constexpr u_int32 kuTestReflMagic   = 0x46525846; // 'FXRF'
	constexpr u_int32 kuTestReflVersion = 6;

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

	// Offsets are deliberately NOT set here — ComputeVertexPacking owns them, and every
	// test that cares asserts on what it computed rather than on what was handed in.
	Flux_ReflectedVertexAttribute MakeVertexAttribute(const char* szName, const char* szSemantic, u_int uSemanticIndex,
													  u_int uLocation, ShaderDataType eType, u_int uBinding)
	{
		Flux_ReflectedVertexAttribute xA;
		xA.m_strName        = szName;
		xA.m_strSemantic    = szSemantic;
		xA.m_uSemanticIndex = uSemanticIndex;
		xA.m_uLocation      = uLocation;
		xA.m_eType          = eType;
		xA.m_uBinding       = uBinding;
		return xA;
	}
}

ZENITH_TEST(FluxShaderReflection, SidecarHeaderStampIsV6)
{
	Flux_ShaderReflection xRefl;
	xRefl.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));
	xRefl.BuildLookupMap();

	Zenith_DataStream xStream;
	xRefl.WriteToDataStream(xStream);
	ZENITH_ASSERT_GE(xStream.GetCursor(), 8u, "at least a magic + version header written");

	const u_int32* puHeader = static_cast<const u_int32*>(xStream.GetData());
	ZENITH_ASSERT_EQ(puHeader[0], kuTestReflMagic, "sidecar leads with the FXRF magic");
	ZENITH_ASSERT_EQ(puHeader[1], kuTestReflVersion, "sidecar version is v6 (T2.a vertex-input table)");
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

// ============================================================================
// v6 — vertex-input table (compressed-vertex work, T2.a)
// ============================================================================

ZENITH_TEST(FluxShaderReflection, VertexPackingTightPacksInDeclarationOrder)
{
	// The unified-mesh vertex as T2.b will compress it: a float3 position, a half2 UV,
	// two snorm10 basis vectors and a unorm8x4 colour. Offsets are the running byte sum
	// in DECLARATION order and the stride is the total — that is the whole rule.
	Flux_ShaderReflection xRefl;
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xPosition", "POSITION", 0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xUV",       "TEXCOORD", 0u, 1u, SHADER_DATA_TYPE_HALF2,  0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xNormal",   "NORMAL",   0u, 2u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xTangent",  "TANGENT",  0u, 3u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xColour",   "COLOR",    0u, 4u, SHADER_DATA_TYPE_UNORM8X4, 0u));
	xRefl.ComputeVertexPacking();

	const Zenith_Vector<Flux_ReflectedVertexAttribute>& axAttribs = xRefl.GetVertexAttributes();
	ZENITH_ASSERT_EQ(axAttribs.GetSize(), 5u, "every added attribute is kept");
	ZENITH_ASSERT_EQ(axAttribs.Get(0).m_uOffset, 0u,  "the first attribute starts at 0");
	ZENITH_ASSERT_EQ(axAttribs.Get(1).m_uOffset, 12u, "float3 occupies 12 bytes");
	ZENITH_ASSERT_EQ(axAttribs.Get(2).m_uOffset, 16u, "half2 occupies 4 bytes");
	ZENITH_ASSERT_EQ(axAttribs.Get(3).m_uOffset, 20u, "snorm10_10_10_2 occupies 4 bytes");
	ZENITH_ASSERT_EQ(axAttribs.Get(4).m_uOffset, 24u, "the second snorm10 occupies 4 bytes");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(0u), 28u, "stride is the packed total, with no padding");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(1u), 0u,  "an unused binding has stride 0");

	// Re-running must be idempotent — packing is derived state, never accumulated.
	xRefl.ComputeVertexPacking();
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(0u), 28u, "recomputing packing does not accumulate");
	ZENITH_ASSERT_EQ(xRefl.GetVertexAttributes().Get(4).m_uOffset, 24u, "recomputing packing reproduces the same offsets");
}

ZENITH_TEST(FluxShaderReflection, VertexPackingSplitsBindingsIndependently)
{
	// [PerInstance] moves an attribute to binding 1, whose offsets restart at 0 — the
	// two streams are separate buffers, so a shared running offset would be wrong.
	Flux_ShaderReflection xRefl;
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xPosition", "POSITION",  0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xInstRow0", "INSTANCEROW", 0u, 1u, SHADER_DATA_TYPE_FLOAT4, 1u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xUV",       "TEXCOORD",  0u, 2u, SHADER_DATA_TYPE_HALF2,  0u));
	xRefl.AddVertexAttribute(MakeVertexAttribute("a_xInstRow1", "INSTANCEROW", 1u, 3u, SHADER_DATA_TYPE_FLOAT4, 1u));
	xRefl.ComputeVertexPacking();

	const Zenith_Vector<Flux_ReflectedVertexAttribute>& axAttribs = xRefl.GetVertexAttributes();
	ZENITH_ASSERT_EQ(axAttribs.Get(0).m_uOffset, 0u,  "binding 0 starts at 0");
	ZENITH_ASSERT_EQ(axAttribs.Get(1).m_uOffset, 0u,  "binding 1 starts at 0 independently");
	ZENITH_ASSERT_EQ(axAttribs.Get(2).m_uOffset, 12u, "binding 0 continues past its own first attribute only");
	ZENITH_ASSERT_EQ(axAttribs.Get(3).m_uOffset, 16u, "binding 1 continues past its own first attribute only");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(0u), 16u, "per-vertex stride = float3 + half2");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(1u), 32u, "per-instance stride = two float4s");
}

ZENITH_TEST(FluxShaderReflection, VertexPackingEmptyTableIsAllZeroStrides)
{
	// A compute-only program (and a vertex shader that reads nothing but SV_VertexID)
	// has no attributes at all. Both strides must read 0, and an out-of-range binding
	// query must be answerable rather than indexing off the end of the array.
	Flux_ShaderReflection xRefl;
	xRefl.ComputeVertexPacking();

	ZENITH_ASSERT_EQ(xRefl.GetVertexAttributes().GetSize(), 0u, "no attributes on an empty table");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(0u), 0u, "per-vertex stride is 0");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(1u), 0u, "per-instance stride is 0");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(7u), 0u, "an out-of-range binding answers 0, not garbage");
}

ZENITH_TEST(FluxShaderReflection, VertexAttributesRoundTripThroughDataStream)
{
	Flux_ShaderReflection xWrite;
	xWrite.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));
	xWrite.AddSpecConstant(MakeSpec("FLUX_SC_VIEW_SHADOWS_PERMITTED", 0u, 1u, "bool"));
	xWrite.AddVertexAttribute(MakeVertexAttribute("a_xPosition", "POSITION", 0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
	xWrite.AddVertexAttribute(MakeVertexAttribute("a_xUV1",      "TEXCOORD", 1u, 1u, SHADER_DATA_TYPE_HALF2,  0u));
	xWrite.AddVertexAttribute(MakeVertexAttribute("a_xInstRow",  "INSTANCEROW", 0u, 2u, SHADER_DATA_TYPE_UNORM8X4, 1u));
	xWrite.ComputeVertexPacking();
	xWrite.BuildLookupMap();

	Zenith_DataStream xStream;
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	// The v6 table must not disturb the tables written before it.
	ZENITH_ASSERT_EQ(xRead.GetBindings().GetSize(), 1u, "bindings survive alongside the vertex table");
	ZENITH_ASSERT_EQ(xRead.GetSpecConstants().GetSize(), 1u, "spec constants survive alongside the vertex table");

	const Zenith_Vector<Flux_ReflectedVertexAttribute>& axAttribs = xRead.GetVertexAttributes();
	ZENITH_ASSERT_EQ(axAttribs.GetSize(), 3u, "all three vertex attributes survive round-trip");
	ZENITH_ASSERT_STREQ(axAttribs.Get(0).m_strName.c_str(), "a_xPosition", "attribute name survives");
	ZENITH_ASSERT_STREQ(axAttribs.Get(1).m_strSemantic.c_str(), "TEXCOORD", "semantic name survives");
	ZENITH_ASSERT_EQ(axAttribs.Get(1).m_uSemanticIndex, 1u, "semantic index survives");
	ZENITH_ASSERT_EQ(axAttribs.Get(1).m_uLocation, 1u, "location survives");
	ZENITH_ASSERT_EQ((int)axAttribs.Get(1).m_eType, (int)SHADER_DATA_TYPE_HALF2, "storage type survives");
	ZENITH_ASSERT_EQ(axAttribs.Get(2).m_uBinding, 1u, "the per-instance binding survives");
	// Offsets/strides are read back as WRITTEN, not recomputed — the sidecar is the
	// authority on the byte layout an exporter already baked.
	ZENITH_ASSERT_EQ(axAttribs.Get(1).m_uOffset, 12u, "the computed offset survives round-trip");
	ZENITH_ASSERT_EQ(axAttribs.Get(2).m_uOffset, 0u,  "the per-instance offset survives round-trip");
	ZENITH_ASSERT_EQ(xRead.GetVertexStride(0u), 16u, "per-vertex stride survives round-trip");
	ZENITH_ASSERT_EQ(xRead.GetVertexStride(1u), 4u,  "per-instance stride survives round-trip");
}

ZENITH_TEST(FluxShaderReflection, EmptyVertexTableRoundTripsAsZeroStrides)
{
	// Every compute program's sidecar takes this shape. It is also the shape a v5
	// reader would mis-parse, which is exactly why the version stamp moved.
	Flux_ShaderReflection xWrite;
	xWrite.AddBinding(MakeBinding(0u, 0u, "g_xGlobal", FLUX_RESOURCE_KIND_CONSTANT_BUFFER));

	Zenith_DataStream xStream;
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xRead.GetBindings().GetSize(), 1u, "the binding still round-trips");
	ZENITH_ASSERT_EQ(xRead.GetVertexAttributes().GetSize(), 0u, "an empty vertex table round-trips empty");
	ZENITH_ASSERT_EQ(xRead.GetVertexStride(0u), 0u, "per-vertex stride round-trips as 0");
	ZENITH_ASSERT_EQ(xRead.GetVertexStride(1u), 0u, "per-instance stride round-trips as 0");
}

ZENITH_TEST(FluxShaderReflection, MergeVertexInputsAdoptsOnce)
{
	// The backend merge path. Both a graphics program's sidecars carry the same
	// whole-program table, and the VS is merged first — so the second merge must be
	// inert, and a fragment-only reflection must never clear what the vertex left.
	Flux_ShaderReflection xVertexStage;
	xVertexStage.AddVertexAttribute(MakeVertexAttribute("a_xPosition", "POSITION", 0u, 0u, SHADER_DATA_TYPE_FLOAT3, 0u));
	xVertexStage.AddVertexAttribute(MakeVertexAttribute("a_xUV",       "TEXCOORD", 0u, 1u, SHADER_DATA_TYPE_HALF2,  0u));
	xVertexStage.ComputeVertexPacking();

	Flux_ShaderReflection xOther;
	xOther.AddVertexAttribute(MakeVertexAttribute("a_xWrong", "COLOR", 0u, 0u, SHADER_DATA_TYPE_FLOAT4, 0u));
	xOther.ComputeVertexPacking();

	Flux_ShaderReflection xMerged;
	xMerged.MergeVertexInputs(xVertexStage);
	ZENITH_ASSERT_EQ(xMerged.GetVertexAttributes().GetSize(), 2u, "the first non-empty table is adopted whole");
	ZENITH_ASSERT_EQ(xMerged.GetVertexStride(0u), 16u, "strides are adopted with the table, not recomputed");

	xMerged.MergeVertexInputs(xOther);
	ZENITH_ASSERT_EQ(xMerged.GetVertexAttributes().GetSize(), 2u, "a second merge cannot append");
	ZENITH_ASSERT_STREQ(xMerged.GetVertexAttributes().Get(0).m_strName.c_str(), "a_xPosition",
		"a second merge cannot overwrite the vertex stage's table");
	ZENITH_ASSERT_EQ(xMerged.GetVertexStride(0u), 16u, "a second merge cannot overwrite the strides");

	// Merging an EMPTY source into an empty destination stays empty (a compute sidecar).
	Flux_ShaderReflection xFromEmpty;
	xFromEmpty.MergeVertexInputs(Flux_ShaderReflection());
	ZENITH_ASSERT_EQ(xFromEmpty.GetVertexAttributes().GetSize(), 0u, "merging an empty table adds nothing");
}

// ---- the pure format vocabulary (no Slang session needed) -------------------

ZENITH_TEST(FluxShaderReflection, VertexFormatStringMapsToStorageTag)
{
	struct Case { const char* m_szFormat; ShaderDataType m_eExpected; };
	const Case axCases[] =
	{
		{ "half2",           SHADER_DATA_TYPE_HALF2           },
		{ "half4",           SHADER_DATA_TYPE_HALF4           },
		{ "snorm16x2",       SHADER_DATA_TYPE_SNORM16X2       },
		{ "snorm16x4",       SHADER_DATA_TYPE_SNORM16X4       },
		{ "unorm16x2",       SHADER_DATA_TYPE_UNORM16X2       },
		{ "unorm16x4",       SHADER_DATA_TYPE_UNORM16X4       },
		{ "unorm8x4",        SHADER_DATA_TYPE_UNORM8X4        },
		{ "uint8x4",         SHADER_DATA_TYPE_UINT8X4         },
		{ "uint16x4",        SHADER_DATA_TYPE_UINT16X4        },
		{ "snorm10_10_10_2", SHADER_DATA_TYPE_SNORM10_10_10_2 },
	};
	for (const Case& xCase : axCases)
	{
		ShaderDataType eType = SHADER_DATA_TYPE_NONE;
		ZENITH_ASSERT_TRUE(Flux_ParseVertexFormatString(xCase.m_szFormat, eType),
			"'%s' is part of the [VtxFmt] vocabulary", xCase.m_szFormat);
		ZENITH_ASSERT_EQ((int)eType, (int)xCase.m_eExpected, "'%s' maps to its storage tag", xCase.m_szFormat);
	}

	// An unrecognised string must NOT fall back to the declared type — the extractor
	// turns this false into a hard compile error naming the field.
	ShaderDataType eUnused = SHADER_DATA_TYPE_NONE;
	ZENITH_ASSERT_FALSE(Flux_ParseVertexFormatString("half3", eUnused), "half3 has no vertex format");
	ZENITH_ASSERT_FALSE(Flux_ParseVertexFormatString("HALF4", eUnused), "the vocabulary is case-sensitive");
	ZENITH_ASSERT_FALSE(Flux_ParseVertexFormatString("", eUnused), "an empty format string is rejected");
	ZENITH_ASSERT_FALSE(Flux_ParseVertexFormatString(nullptr, eUnused), "a null format string is rejected");
}

ZENITH_TEST(FluxShaderReflection, VertexTypeInferenceFromDeclaredField)
{
	ShaderDataType eType = SHADER_DATA_TYPE_NONE;

	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 1u, eType), "float infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_FLOAT, "float32x1 -> FLOAT");
	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 3u, eType), "float3 infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_FLOAT3, "float32x3 -> FLOAT3");
	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_UINT32, 4u, eType), "uint4 infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_UINT4, "uint32x4 -> UINT4");
	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_INT32, 2u, eType), "int2 infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_INT2, "int32x2 -> INT2");

	// half is inferable only at the two widths that have a packed tag.
	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT16, 2u, eType), "half2 infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_HALF2, "float16x2 -> HALF2");
	ZENITH_ASSERT_TRUE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT16, 4u, eType), "half4 infers");
	ZENITH_ASSERT_EQ((int)eType, (int)SHADER_DATA_TYPE_HALF4, "float16x4 -> HALF4");
	ZENITH_ASSERT_FALSE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT16, 3u, eType), "half3 has no vertex format");
	ZENITH_ASSERT_FALSE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT16, 1u, eType), "half1 has no vertex format");

	// Lane counts outside 1..4 and unknown scalars (bool, float64, int8, a matrix's
	// row/column shape) have no vertex-input representation.
	ZENITH_ASSERT_FALSE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 0u, eType), "zero lanes is not a vertex input");
	ZENITH_ASSERT_FALSE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 9u, eType), "nine lanes (float3x3) is not a vertex input");
	ZENITH_ASSERT_FALSE(Flux_InferVertexAttributeType(FLUX_VERTEX_FIELD_SCALAR_UNKNOWN, 4u, eType), "an unknown scalar kind is rejected");
}

ZENITH_TEST(FluxShaderReflection, VertexFormatOverrideValidation)
{
	// Legal: a wider storage may feed a narrower field — the fetch drops the extra lane.
	ZENITH_ASSERT_TRUE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_HALF4, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 3u),
		"half4 may feed a float3 (the fourth lane is dropped)");
	ZENITH_ASSERT_TRUE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_SNORM10_10_10_2, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 4u),
		"snorm10_10_10_2 may feed a float4");
	ZENITH_ASSERT_TRUE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_UNORM8X4, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 4u),
		"unorm8x4 is a float-family fetch");
	ZENITH_ASSERT_TRUE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_HALF2, FLUX_VERTEX_FIELD_SCALAR_FLOAT16, 2u),
		"half2 may feed a half2");
	ZENITH_ASSERT_TRUE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_UINT8X4, FLUX_VERTEX_FIELD_SCALAR_UINT32, 4u),
		"uint8x4 feeds a uint4 (bone indices)");

	// Illegal: too few lanes, or a family mismatch in either direction.
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_HALF2, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 3u),
		"half2 cannot feed a float3 — the third lane would be invented");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_UINT8X4, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 4u),
		"a uint-family fetch cannot feed a float field");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_UNORM8X4, FLUX_VERTEX_FIELD_SCALAR_UINT32, 4u),
		"a float-family fetch cannot feed a uint field");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_UINT8X4, FLUX_VERTEX_FIELD_SCALAR_INT32, 4u),
		"a UINT-family fetch cannot feed a signed-int field — Vulkan needs a SINT format for signed inputs");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_MAT4, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 4u),
		"a matrix is not a vertex-input storage format");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_NONE, FLUX_VERTEX_FIELD_SCALAR_FLOAT32, 4u),
		"NONE is not a vertex-input storage format");
	ZENITH_ASSERT_FALSE(Flux_ValidateVertexFormatOverride(SHADER_DATA_TYPE_FLOAT4, FLUX_VERTEX_FIELD_SCALAR_UNKNOWN, 4u),
		"an unknown field scalar can never be validated");
}

ZENITH_TEST(FluxShaderReflection, VertexStorageFamilyAndLaneTable)
{
	// The family/lane table is what the override rules key on, so pin the three
	// families explicitly — including that the packed float formats are NOT integers.
	ZENITH_ASSERT_EQ((int)Flux_VertexStorageFamily(SHADER_DATA_TYPE_SNORM16X4), (int)FLUX_VERTEX_STORAGE_FAMILY_FLOAT,
		"snorm is a float-family fetch");
	ZENITH_ASSERT_EQ((int)Flux_VertexStorageFamily(SHADER_DATA_TYPE_UNORM16X2), (int)FLUX_VERTEX_STORAGE_FAMILY_FLOAT,
		"unorm is a float-family fetch");
	ZENITH_ASSERT_EQ((int)Flux_VertexStorageFamily(SHADER_DATA_TYPE_UINT16X4), (int)FLUX_VERTEX_STORAGE_FAMILY_UINT,
		"uint16x4 is a uint-family fetch");
	ZENITH_ASSERT_EQ((int)Flux_VertexStorageFamily(SHADER_DATA_TYPE_INT3), (int)FLUX_VERTEX_STORAGE_FAMILY_INT,
		"int3 is an int-family fetch");
	ZENITH_ASSERT_EQ((int)Flux_VertexStorageFamily(SHADER_DATA_TYPE_BOOL), (int)FLUX_VERTEX_STORAGE_FAMILY_UNKNOWN,
		"bool has no vertex-input family");

	ZENITH_ASSERT_EQ(Flux_VertexStorageLaneCount(SHADER_DATA_TYPE_SNORM10_10_10_2), 4u, "snorm10_10_10_2 carries 4 lanes");
	ZENITH_ASSERT_EQ(Flux_VertexStorageLaneCount(SHADER_DATA_TYPE_HALF2), 2u, "half2 carries 2 lanes");
	ZENITH_ASSERT_EQ(Flux_VertexStorageLaneCount(SHADER_DATA_TYPE_FLOAT), 1u, "float carries 1 lane");
	ZENITH_ASSERT_EQ(Flux_VertexStorageLaneCount(SHADER_DATA_TYPE_MAT3), 0u, "a non-vertex format reports 0 lanes");
	ZENITH_ASSERT_EQ(Flux_VertexStorageLaneCount(SHADER_DATA_TYPE_NONE), 0u, "NONE reports 0 lanes");
}
