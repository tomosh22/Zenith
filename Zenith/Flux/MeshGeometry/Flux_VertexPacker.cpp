#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_VertexPacker.h"

#include <cmath>     // std::sqrt — the SNORM10 pre-normalisation
#include <cstring>   // std::memcpy — every element write

namespace
{
	// Positive-and-finite, decided on the BITS rather than with `f > 0.0f &&
	// std::isfinite(f)`. This TU compiles under /fp:fast, where the compiler is
	// licensed to assume no NaN ever occurs and may therefore fold away the very
	// predicate that exists to catch one. Reading the exponent and magnitude out
	// of the storage is not a floating-point operation, so no fast-math
	// assumption applies to it.
	bool IsPositiveFiniteBits(float fValue)
	{
		uint32_t uBits = 0u;
		std::memcpy(&uBits, &fValue, sizeof(uBits));
		const uint32_t uSign = uBits >> 31;
		const uint32_t uExponent = (uBits >> 23) & 0xFFu;
		const uint32_t uMagnitude = uBits & 0x7FFFFFFFu;
		return (uSign == 0u) && (uExponent != 0xFFu) && (uMagnitude != 0u);
	}

	// The source feeding (semantic, semanticIndex), or nullptr for "absent".
	// A view with no data or no lanes reads as absent too, so a caller may hold a
	// fixed-size array of views and leave the slots it has nothing for blank.
	const Flux_VertexSourceView* FindSource(
		const Flux_VertexSourceView* paxSources, u_int uSourceCount,
		FluxVertexSemantic eSemantic, u_int uSemanticIndex)
	{
		if (paxSources == nullptr)
		{
			return nullptr;
		}
		for (u_int uSource = 0; uSource < uSourceCount; uSource++)
		{
			const Flux_VertexSourceView& xView = paxSources[uSource];
			const bool bUsable = (xView.m_pfData != nullptr) && (xView.m_uLaneCount > 0u);
			if (bUsable && (xView.m_eSemantic == eSemantic) && (xView.m_uSemanticIndex == uSemanticIndex))
			{
				return &xView;
			}
		}
		return nullptr;
	}

	// One vertex's first three lanes of a (possibly absent, possibly short)
	// source, with every lane it does not supply taken from the semantic's
	// canonical default — the same rule the main pack loop applies.
	Zenith_Maths::Vector3 SampleVec3(
		const Flux_VertexSourceView* pxView, u_int uVertex, FluxVertexSemantic eSemantic)
	{
		Zenith_Maths::Vector3 xOut(Flux_CanonicalVertexDefault(eSemantic));
		if (pxView == nullptr)
		{
			return xOut;
		}
		const float* const pfVertex = pxView->m_pfData + static_cast<size_t>(uVertex) * pxView->m_uLaneCount;
		const u_int uCopy = (pxView->m_uLaneCount < 3u) ? pxView->m_uLaneCount : 3u;
		for (int iLane = 0; iLane < static_cast<int>(uCopy); iLane++)
		{
			xOut[iLane] = pfVertex[iLane];
		}
		return xOut;
	}

	// An integer-format lane, from the float source views are written in. Every
	// semantic in the closed vocabulary is a float attribute, so an integer
	// storage format (bone indices, palette ids) still arrives as a float that
	// happens to hold a whole number. Negatives are floored to 0 rather than
	// cast: converting a negative float to an unsigned integer is undefined, and
	// no attribute the integer formats carry has a meaningful negative value.
	u_int LaneToUint(float fValue)
	{
		return (fValue > 0.0f) ? static_cast<u_int>(fValue) : 0u;
	}

	// Write ONE element's already-resolved lanes at pDst in its storage format.
	// Every packed format goes through Flux_VertexCodec — the bit layouts are a
	// file format (baked chunks on disk carry those exact words), so none of the
	// quantisation is re-derived here.
	void WriteElement(
		uint8_t* pDst, ShaderDataType eType, const float* pfLanes,
		FluxVertexSemantic eSemantic, const Flux_PosQuant* pxPosQuant)
	{
		switch (eType)
		{
		case SHADER_DATA_TYPE_FLOAT:
		case SHADER_DATA_TYPE_FLOAT2:
		case SHADER_DATA_TYPE_FLOAT3:
		case SHADER_DATA_TYPE_FLOAT4:
			// The float family is the identity encoding: copy the lanes through
			// untouched, so a caller may compare them bit-exactly with its source.
			std::memcpy(pDst, pfLanes, sizeof(float) * Flux_VertexFormatLaneCount(eType));
			break;

		case SHADER_DATA_TYPE_HALF2:
		{
			const u_int uWord = Flux_PackHalf2(Zenith_Maths::Vector2(pfLanes[0], pfLanes[1]));
			std::memcpy(pDst, &uWord, sizeof(uWord));
			break;
		}

		case SHADER_DATA_TYPE_HALF4:
		{
			const u_int64 ulWord = Flux_PackHalf4(
				Zenith_Maths::Vector4(pfLanes[0], pfLanes[1], pfLanes[2], pfLanes[3]));
			std::memcpy(pDst, &ulWord, sizeof(ulWord));
			break;
		}

		case SHADER_DATA_TYPE_SNORM16X4:
		{
			// The quant box is a POSITION transform and nothing else: it maps
			// authored world units onto [-1,1] before quantisation. Applied to a
			// normal or a colour it would be nonsense, so it is scoped to the one
			// semantic it describes; without a box even a position takes the plain
			// (clamped) path.
			const bool bQuantised = (pxPosQuant != nullptr) && (eSemantic == FLUX_VERTEX_SEMANTIC_POSITION);
			const u_int64 ulWord = bQuantised
				? Flux_PackSnorm16x4(Zenith_Maths::Vector3(pfLanes[0], pfLanes[1], pfLanes[2]), pfLanes[3], *pxPosQuant)
				: Flux_PackSnorm16x4(Zenith_Maths::Vector4(pfLanes[0], pfLanes[1], pfLanes[2], pfLanes[3]));
			std::memcpy(pDst, &ulWord, sizeof(ulWord));
			break;
		}

		case SHADER_DATA_TYPE_UNORM16X2:
		{
			const u_int uWord = Flux_PackUnorm16x2(Zenith_Maths::Vector2(pfLanes[0], pfLanes[1]));
			std::memcpy(pDst, &uWord, sizeof(uWord));
			break;
		}

		case SHADER_DATA_TYPE_UNORM8X4:
		{
			const u_int uWord = Flux_PackUnorm8x4(
				Zenith_Maths::Vector4(pfLanes[0], pfLanes[1], pfLanes[2], pfLanes[3]));
			std::memcpy(pDst, &uWord, sizeof(uWord));
			break;
		}

		case SHADER_DATA_TYPE_UINT8X4:
		{
			const u_int uWord = Flux_PackUint8x4(Zenith_Maths::UVector4(
				LaneToUint(pfLanes[0]), LaneToUint(pfLanes[1]),
				LaneToUint(pfLanes[2]), LaneToUint(pfLanes[3])));
			std::memcpy(pDst, &uWord, sizeof(uWord));
			break;
		}

		case SHADER_DATA_TYPE_SNORM10_10_10_2:
		{
			// The codec CLAMPS to [-1,1] but does not normalise, and every SNORM10
			// element in the corpus is a unit direction. Clamping a length-1.2
			// normal per-axis changes its DIRECTION, not just its precision, so the
			// vector is normalised first.
			Zenith_Maths::Vector3 xDirection(pfLanes[0], pfLanes[1], pfLanes[2]);
			const float fLengthSq = Zenith_Maths::LengthSq(xDirection);
			if (IsPositiveFiniteBits(fLengthSq))
			{
				xDirection = xDirection * (1.0f / std::sqrt(fLengthSq));
			}
			else
			{
				// A zero-length (or non-finite) direction has no normalisation —
				// 0/0 is a NaN, and a NaN packs to an arbitrary word that would
				// decode as a plausible direction. Fall back to the semantic's
				// canonical default, which is exactly what a wholly ABSENT source
				// would have produced.
				xDirection = Zenith_Maths::Vector3(Flux_CanonicalVertexDefault(eSemantic));
			}
			const u_int uWord = Flux_PackSnorm10_10_10_2(
				xDirection.x, xDirection.y, xDirection.z, pfLanes[3]);
			std::memcpy(pDst, &uWord, sizeof(uWord));
			break;
		}

		default:
			// Unreachable: Flux_VertexFormatLaneCount already returned 0 for every
			// format not handled above, and the caller skipped the element.
			Zenith_Assert(false, "Flux_PackVertices: unwritable storage format %d reached the writer", static_cast<int>(eType));
			break;
		}
	}
}

void Flux_PackVertices(
	void* pDst,
	const Flux_VertexLayoutDesc& xLayout,
	const Flux_VertexSourceView* paxSources,
	u_int uSourceCount,
	u_int uVertexCount,
	const Flux_PosQuant* pxPosQuant)
{
	// Nothing to write is not an error: a zero-vertex mesh and a program with no
	// vertex inputs (the canonical empty layout) are both legitimate.
	if ((uVertexCount == 0u) || (xLayout.m_uElementCount == 0u) || (xLayout.m_paxElements == nullptr))
	{
		return;
	}

	Zenith_Assert(pDst != nullptr, "Flux_PackVertices: null destination for %u vertices", uVertexCount);
	if (pDst == nullptr)
	{
		return;
	}

	const u_int uStride = xLayout.m_auStrides[0];
	Zenith_Assert(uStride > 0u, "Flux_PackVertices: layout declares %u elements but a zero binding-0 stride", xLayout.m_uElementCount);
	if (uStride == 0u)
	{
		return;
	}

	uint8_t* const pDstBytes = static_cast<uint8_t*>(pDst);

	// Hoisted out of both loops: the two sources the 4-lane TANGENT handedness
	// rule reads. They are found by semantic like any other source, but they are
	// consumed by an ELEMENT that is neither of them.
	const Flux_VertexSourceView* const pxNormalSource =
		FindSource(paxSources, uSourceCount, FLUX_VERTEX_SEMANTIC_NORMAL, 0u);
	const Flux_VertexSourceView* const pxBinormalSource =
		FindSource(paxSources, uSourceCount, FLUX_VERTEX_SEMANTIC_BINORMAL, 0u);

	// ELEMENT-outer / VERTEX-inner. Everything that depends only on the element —
	// its source, its lane count, its defaults, whether its w has to be derived —
	// is then resolved once per element instead of once per vertex, leaving the
	// hot body a lane copy plus one codec call. No allocation happens in either
	// loop; the per-vertex working set is four floats of stack.
	for (u_int uElement = 0; uElement < xLayout.m_uElementCount; uElement++)
	{
		const Flux_VertexLayoutElement& xElement = xLayout.m_paxElements[uElement];
		if (xElement.m_uBinding != 0u)
		{
			// Binding 1 is the per-instance stream: a different element count, a
			// different stride, a different producer, and a count that is not
			// uVertexCount. Skipping is correct rather than an error — packing the
			// per-vertex stream of a two-binding layout is a legitimate call.
			continue;
		}

		const u_int uLanes = Flux_VertexFormatLaneCount(xElement.m_eType);
		if (uLanes == 0u)
		{
			Zenith_Assert(false, "Flux_PackVertices: element %u declares storage format %d, which the packer cannot write", uElement, static_cast<int>(xElement.m_eType));
			continue;
		}

		const Flux_VertexSourceView* const pxSource =
			FindSource(paxSources, uSourceCount, xElement.m_eSemantic, xElement.m_uSemanticIndex);
		const Zenith_Maths::Vector4 xDefault = Flux_CanonicalVertexDefault(xElement.m_eSemantic);
		const float* const pfSourceBase = (pxSource != nullptr) ? pxSource->m_pfData : nullptr;
		const u_int uSourceLanes = (pxSource != nullptr) ? pxSource->m_uLaneCount : 0u;
		// A source with MORE lanes than the element is truncated to the first N; a
		// source with fewer leaves the rest on the canonical default (see the
		// header). uCopyLanes is 0 exactly when the source is absent, which is what
		// keeps pfSourceBase from ever being dereferenced while null.
		const u_int uCopyLanes = (uSourceLanes < uLanes) ? uSourceLanes : uLanes;

		// A 4-lane TANGENT whose own source does not carry lane 3 takes its
		// handedness from the bitangent: w = sign(dot(cross(N,T),B)). An explicit
		// 4-lane tangent source WINS — an authored w is a decision, not something
		// to re-derive — and with no BINORMAL source there is nothing to derive
		// from, so w stays on the canonical pad (0).
		const bool bDeriveTangentW = (xElement.m_eSemantic == FLUX_VERTEX_SEMANTIC_TANGENT)
			&& (uLanes == 4u) && (uCopyLanes < 4u) && (pxBinormalSource != nullptr);

		for (u_int uVertex = 0; uVertex < uVertexCount; uVertex++)
		{
			float afLanes[4] = { xDefault.x, xDefault.y, xDefault.z, xDefault.w };
			for (u_int uLane = 0; uLane < uCopyLanes; uLane++)
			{
				afLanes[uLane] = pfSourceBase[static_cast<size_t>(uVertex) * uSourceLanes + uLane];
			}

			if (bDeriveTangentW)
			{
				const Zenith_Maths::Vector3 xNormal = SampleVec3(pxNormalSource, uVertex, FLUX_VERTEX_SEMANTIC_NORMAL);
				const Zenith_Maths::Vector3 xTangent(afLanes[0], afLanes[1], afLanes[2]);
				const Zenith_Maths::Vector3 xBinormal = SampleVec3(pxBinormalSource, uVertex, FLUX_VERTEX_SEMANTIC_BINORMAL);
				const float fHandedness = Zenith_Maths::Dot(Zenith_Maths::Cross(xNormal, xTangent), xBinormal);
				// +1 on an exact zero: a degenerate frame has no handedness, and the
				// positive branch is the one a well-formed right-handed basis takes.
				afLanes[3] = (fHandedness < 0.0f) ? -1.0f : 1.0f;
			}

			WriteElement(
				pDstBytes + static_cast<size_t>(uVertex) * uStride + xElement.m_uOffset,
				xElement.m_eType, afLanes, xElement.m_eSemantic, pxPosQuant);
		}
	}
}

// The packer's tests live beside the packer, which is only safe because this TU is
// no longer dead-strippable: T3.b gave Flux_PackVertices three production callers
// (Flux_MeshInstance's static-mesh paths, Zenith_MeshAsset::EnsureGPUBuffers and
// Flux_MeshGeometry::GenerateLayoutAndVertexData), so the linker pulls this .obj
// in and the static registrars below come with it. For one task, while the packer
// had no caller at all, they had to be hosted from the always-linked
// Flux_MaterialTable.cpp instead — MSVC dropped this whole TU and the Null_ Combat
// tally did not move off 1400, which is exactly what a dead-stripped test file
// looks like. If the last caller ever goes away, that workaround is the fix.
#include "Flux/MeshGeometry/Flux_VertexPacker.Tests.inl"
