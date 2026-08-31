#include "Zenith.h"
#include "Zenith_Tools_MeshoptDecode.h"

#include <cmath>
#include <cstring>

// See the header for what this is and why the engine's own importer cannot
// stand in for it. Everything here is a straight port of a decoder that was
// first written and verified in isolation against upstream meshoptimizer's
// output on real gltfpack data; the comments call out the places where the
// obvious reading of the format is the wrong one.
namespace Zenith_Tools_MeshoptDecode
{
namespace
{
	constexpr unsigned char uVERTEX_HEADER = 0xa0;
	constexpr unsigned char uINDEX_HEADER = 0xe0;

	constexpr size_t ulBYTE_GROUP_SIZE = 16;
	constexpr size_t ulVERTEX_BLOCK_SIZE_BYTES = 8192;
	constexpr size_t ulVERTEX_BLOCK_MAX_SIZE = 256;
	constexpr size_t ulTAIL_MAX_SIZE = 32;

	// The codeaux table the encoder appends to every index stream. Its last two
	// entries are never used for encoding, which is exactly why codetri 0xfe/0xff
	// are free to mean something else.
	constexpr size_t ulCODEAUX_TABLE_SIZE = 16;

	inline unsigned char Unzigzag8(unsigned char uValue)
	{
		return static_cast<unsigned char>(-(uValue & 1) ^ (uValue >> 1));
	}

	inline size_t GetVertexBlockSize(size_t ulVertexSize)
	{
		size_t ulResult = ulVERTEX_BLOCK_SIZE_BYTES / ulVertexSize;
		ulResult &= ~(ulBYTE_GROUP_SIZE - 1);
		return ulResult < ulVERTEX_BLOCK_MAX_SIZE ? ulResult : ulVERTEX_BLOCK_MAX_SIZE;
	}

	// One 16-byte group. bitslog2 selects 0/2/4/8 bits per byte; in the 2- and
	// 4-bit forms the all-ones code is an ESCAPE whose real byte comes from a
	// second cursor running just past the packed nibbles.
	const unsigned char* DecodeBytesGroup(
		const unsigned char* pData,
		const unsigned char* pEnd,
		unsigned char* pBuffer,
		int iBitsLog2)
	{
		switch (iBitsLog2)
		{
		case 0:
			memset(pBuffer, 0, ulBYTE_GROUP_SIZE);
			return pData;

		case 1:
		{
			if (pData + 4 > pEnd)
			{
				return nullptr;
			}
			const unsigned char* pVar = pData + 4;
			for (size_t k = 0; k < ulBYTE_GROUP_SIZE; ++k)
			{
				const unsigned char uEnc =
					(pData[k >> 2] >> (6 - 2 * (k & 3))) & 3;
				if (uEnc == 3)
				{
					if (pVar >= pEnd)
					{
						return nullptr;
					}
					pBuffer[k] = *pVar++;
				}
				else
				{
					pBuffer[k] = uEnc;
				}
			}
			return pVar;
		}

		case 2:
		{
			if (pData + 8 > pEnd)
			{
				return nullptr;
			}
			const unsigned char* pVar = pData + 8;
			for (size_t k = 0; k < ulBYTE_GROUP_SIZE; ++k)
			{
				const unsigned char uEnc =
					(pData[k >> 1] >> (4 - 4 * (k & 1))) & 15;
				if (uEnc == 15)
				{
					if (pVar >= pEnd)
					{
						return nullptr;
					}
					pBuffer[k] = *pVar++;
				}
				else
				{
					pBuffer[k] = uEnc;
				}
			}
			return pVar;
		}

		default:
			if (pData + ulBYTE_GROUP_SIZE > pEnd)
			{
				return nullptr;
			}
			memcpy(pBuffer, pData, ulBYTE_GROUP_SIZE);
			return pData + ulBYTE_GROUP_SIZE;
		}
	}

	const unsigned char* DecodeBytes(
		const unsigned char* pData,
		const unsigned char* pEnd,
		unsigned char* pBuffer,
		size_t ulSize)
	{
		Zenith_Assert(ulSize % ulBYTE_GROUP_SIZE == 0, "byte-group size must divide the run");

		const size_t ulHeaderSize = (ulSize / ulBYTE_GROUP_SIZE + 3) / 4;
		if (pData + ulHeaderSize > pEnd)
		{
			return nullptr;
		}
		const unsigned char* pHeader = pData;
		pData += ulHeaderSize;

		for (size_t i = 0; i < ulSize; i += ulBYTE_GROUP_SIZE)
		{
			const size_t ulHeaderOffset = i / ulBYTE_GROUP_SIZE;
			const int iBitsLog2 =
				(pHeader[ulHeaderOffset >> 2] >> ((ulHeaderOffset & 3) * 2)) & 3;
			pData = DecodeBytesGroup(pData, pEnd, pBuffer + i, iBitsLog2);
			if (pData == nullptr)
			{
				return nullptr;
			}
		}
		return pData;
	}

	//-------------------------------------------------------------------------
	// Index-stream helpers
	//-------------------------------------------------------------------------

	// meshoptimizer's varint: a lead byte plus at most four continuation bytes.
	// The four-iteration cap is what keeps a malformed stream from running away.
	bool DecodeVByte(
		const unsigned char*& pData,
		const unsigned char* pEnd,
		unsigned int& uValueOut)
	{
		if (pData >= pEnd)
		{
			return false;
		}
		const unsigned char uLead = *pData++;
		if (uLead < 128)
		{
			uValueOut = uLead;
			return true;
		}

		unsigned int uResult = uLead & 127;
		unsigned int uShift = 7;
		for (int i = 0; i < 4; ++i)
		{
			if (pData >= pEnd)
			{
				return false;
			}
			const unsigned char uGroup = *pData++;
			uResult |= static_cast<unsigned int>(uGroup & 127) << uShift;
			uShift += 7;
			if (uGroup < 128)
			{
				break;
			}
		}
		uValueOut = uResult;
		return true;
	}

	bool DecodeIndexDelta(
		const unsigned char*& pData,
		const unsigned char* pEnd,
		unsigned int uLast,
		unsigned int& uIndexOut)
	{
		unsigned int uValue = 0;
		if (!DecodeVByte(pData, pEnd, uValue))
		{
			return false;
		}
		const unsigned int uDelta = (uValue >> 1) ^ (~(uValue & 1) + 1);
		uIndexOut = uLast + uDelta;
		return true;
	}
}

//-----------------------------------------------------------------------------
// Vertex codec
//-----------------------------------------------------------------------------
bool DecodeVertexBuffer(
	void* pDest,
	size_t ulVertexCount,
	size_t ulVertexSize,
	const unsigned char* pSrc,
	size_t ulSrcSize)
{
	if (pDest == nullptr || pSrc == nullptr)
	{
		return false;
	}
	if (ulVertexSize == 0 || ulVertexSize > 256 || (ulVertexSize % 4) != 0)
	{
		// meshoptimizer only ever encodes 4-byte-aligned vertex sizes; anything
		// else means the bufferView stride and the codec disagree.
		return false;
	}
	if (ulSrcSize < 1)
	{
		return false;
	}
	if ((pSrc[0] & 0xf0) != uVERTEX_HEADER)
	{
		return false;
	}

	const int iVersion = pSrc[0] & 0x0f;
	if (iVersion != 0)
	{
		// Codec v1 exists upstream but has never been seen from gltfpack output
		// this pipeline handles, and an untested decode is worse than a refusal.
		return false;
	}

	const size_t ulTailSize = ulVertexSize < ulTAIL_MAX_SIZE ? ulTAIL_MAX_SIZE : ulVertexSize;
	if (ulSrcSize < 1 + ulTailSize)
	{
		return false;
	}

	// ★ The decode SEEDS from the end of the stream: the encoder stores the final
	// vertex verbatim in the tail and every block delta-chains from it.
	unsigned char auLastVertex[256];
	memcpy(auLastVertex, pSrc + ulSrcSize - ulVertexSize, ulVertexSize);

	const unsigned char* pData = pSrc + 1;
	const unsigned char* pEnd = pSrc + ulSrcSize;

	unsigned char auBuffer[ulVERTEX_BLOCK_MAX_SIZE];
	unsigned char auTransposed[ulVERTEX_BLOCK_SIZE_BYTES];

	unsigned char* pOut = static_cast<unsigned char*>(pDest);
	const size_t ulBlockSize = GetVertexBlockSize(ulVertexSize);
	if (ulBlockSize == 0)
	{
		return false;
	}

	size_t ulBase = 0;
	while (ulBase < ulVertexCount)
	{
		const size_t ulRemaining = ulVertexCount - ulBase;
		const size_t ulCount = ulRemaining < ulBlockSize ? ulRemaining : ulBlockSize;
		const size_t ulCountAligned =
			(ulCount + ulBYTE_GROUP_SIZE - 1) & ~(ulBYTE_GROUP_SIZE - 1);

		// One pass per BYTE LANE, not per vertex: the encoder transposes so that
		// byte k of every vertex forms one delta-coded run.
		for (size_t k = 0; k < ulVertexSize; ++k)
		{
			pData = DecodeBytes(pData, pEnd, auBuffer, ulCountAligned);
			if (pData == nullptr)
			{
				return false;
			}

			unsigned char uPrev = auLastVertex[k];
			size_t ulOffset = k;
			for (size_t i = 0; i < ulCount; ++i)
			{
				const unsigned char uValue =
					static_cast<unsigned char>(Unzigzag8(auBuffer[i]) + uPrev);
				auTransposed[ulOffset] = uValue;
				uPrev = uValue;
				ulOffset += ulVertexSize;
			}
		}

		memcpy(pOut + ulBase * ulVertexSize, auTransposed, ulCount * ulVertexSize);
		memcpy(auLastVertex, &auTransposed[ulVertexSize * (ulCount - 1)], ulVertexSize);
		ulBase += ulCount;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Index codec
//-----------------------------------------------------------------------------
bool DecodeIndexBuffer(
	unsigned int* puDest,
	size_t ulIndexCount,
	const unsigned char* pSrc,
	size_t ulSrcSize)
{
	if (puDest == nullptr || pSrc == nullptr)
	{
		return false;
	}
	if (ulIndexCount == 0 || (ulIndexCount % 3) != 0)
	{
		return false;
	}

	const size_t ulTriangleCount = ulIndexCount / 3;
	if (ulSrcSize < 1 + ulTriangleCount + ulCODEAUX_TABLE_SIZE)
	{
		return false;
	}
	if ((pSrc[0] & 0xf0) != uINDEX_HEADER)
	{
		return false;
	}

	const int iVersion = pSrc[0] & 0x0f;
	if (iVersion > 1)
	{
		return false;
	}

	unsigned int auEdgeFifo[16][2] = {};
	unsigned int auVertexFifo[16] = {};
	size_t ulEdgeOffset = 0;
	size_t ulVertexOffset = 0;
	unsigned int uNext = 0;
	unsigned int uLast = 0;

	// v1 reserves two more fec codes than v0 for the +/-1 shorthand below.
	const unsigned int uFecMax = (iVersion >= 1) ? 13u : 15u;

	const unsigned char* pCode = pSrc + 1;
	const unsigned char* pData = pSrc + 1 + ulTriangleCount;
	const unsigned char* pDataEnd = pSrc + ulSrcSize - ulCODEAUX_TABLE_SIZE;
	const unsigned char* pCodeAuxTable = pSrc + ulSrcSize - ulCODEAUX_TABLE_SIZE;

	unsigned int* puOut = puDest;

	for (size_t t = 0; t < ulTriangleCount; ++t)
	{
		const unsigned char uCodeTri = *pCode++;

		if (uCodeTri < 0xf0)
		{
			const unsigned int uFe = uCodeTri >> 4;
			const unsigned int uA = auEdgeFifo[(ulEdgeOffset - 1 - uFe) & 15][0];
			const unsigned int uB = auEdgeFifo[(ulEdgeOffset - 1 - uFe) & 15][1];
			const unsigned int uFec = uCodeTri & 15;

			unsigned int uC = 0;
			bool bPushC = true;

			if (uFec < uFecMax)
			{
				// ★ -1-fec HERE. The codeaux path below reads the same FIFO with
				// -feb / -fec and no -1; both forms are correct where they stand.
				const unsigned int uCf = auVertexFifo[(ulVertexOffset - 1 - uFec) & 15];
				uC = (uFec == 0) ? uNext : uCf;
				bPushC = (uFec == 0);
				uNext += (uFec == 0) ? 1u : 0u;
			}
			else
			{
				if (uFec == 15)
				{
					if (!DecodeIndexDelta(pData, pDataEnd, uLast, uC))
					{
						return false;
					}
				}
				else
				{
					// v1 only: 13 -> last - 1, 14 -> last + 1.
					uC = (uFec == uFecMax + 1) ? (uLast + 1u) : (uLast - 1u);
				}
				uLast = uC;
			}

			puOut[0] = uA;
			puOut[1] = uB;
			puOut[2] = uC;
			puOut += 3;

			if (bPushC)
			{
				auVertexFifo[ulVertexOffset] = uC;
				ulVertexOffset = (ulVertexOffset + 1) & 15;
			}
			auEdgeFifo[ulEdgeOffset][0] = uC;
			auEdgeFifo[ulEdgeOffset][1] = uB;
			ulEdgeOffset = (ulEdgeOffset + 1) & 15;
			auEdgeFifo[ulEdgeOffset][0] = uA;
			auEdgeFifo[ulEdgeOffset][1] = uC;
			ulEdgeOffset = (ulEdgeOffset + 1) & 15;
		}
		else
		{
			unsigned char uCodeAux = 0;
			if (uCodeTri < 0xfe)
			{
				uCodeAux = pCodeAuxTable[uCodeTri & 15];
			}
			else
			{
				if (pData >= pDataEnd)
				{
					return false;
				}
				uCodeAux = *pData++;
			}

			const unsigned int uFeb = uCodeAux >> 4;
			const unsigned int uFec = uCodeAux & 15;

			// ★ 0xff is the ONLY code where the first corner is a delta rather
			// than a brand-new vertex. It is rare enough to survive casual
			// testing, and getting it wrong desynchronises everything after it.
			unsigned int uA = 0;
			if (uCodeTri == 0xff)
			{
				if (!DecodeIndexDelta(pData, pDataEnd, uLast, uA))
				{
					return false;
				}
				uLast = uA;
			}
			else
			{
				uA = uNext++;
			}

			unsigned int uB = 0;
			if (uFeb == 0)
			{
				uB = uNext++;
			}
			else
			{
				uB = auVertexFifo[(ulVertexOffset - uFeb) & 15];
			}

			unsigned int uC = 0;
			if (uFec == 0)
			{
				uC = uNext++;
			}
			else
			{
				uC = auVertexFifo[(ulVertexOffset - uFec) & 15];
			}

			if (uCodeTri >= 0xfe)
			{
				if (uFeb == 15)
				{
					if (!DecodeIndexDelta(pData, pDataEnd, uLast, uB))
					{
						return false;
					}
					uLast = uB;
				}
				if (uFec == 15)
				{
					if (!DecodeIndexDelta(pData, pDataEnd, uLast, uC))
					{
						return false;
					}
					uLast = uC;
				}
			}

			puOut[0] = uA;
			puOut[1] = uB;
			puOut[2] = uC;
			puOut += 3;

			auVertexFifo[ulVertexOffset] = uA;
			ulVertexOffset = (ulVertexOffset + 1) & 15;
			if (uFeb == 0 || uFeb == 15)
			{
				auVertexFifo[ulVertexOffset] = uB;
				ulVertexOffset = (ulVertexOffset + 1) & 15;
			}
			if (uFec == 0 || uFec == 15)
			{
				auVertexFifo[ulVertexOffset] = uC;
				ulVertexOffset = (ulVertexOffset + 1) & 15;
			}

			auEdgeFifo[ulEdgeOffset][0] = uB;
			auEdgeFifo[ulEdgeOffset][1] = uA;
			ulEdgeOffset = (ulEdgeOffset + 1) & 15;
			auEdgeFifo[ulEdgeOffset][0] = uC;
			auEdgeFifo[ulEdgeOffset][1] = uB;
			ulEdgeOffset = (ulEdgeOffset + 1) & 15;
			auEdgeFifo[ulEdgeOffset][0] = uA;
			auEdgeFifo[ulEdgeOffset][1] = uC;
			ulEdgeOffset = (ulEdgeOffset + 1) & 15;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Vertex filters
//-----------------------------------------------------------------------------
bool ApplyFilter(
	void* pBuffer,
	size_t ulCount,
	size_t ulStride,
	MESHOPT_FILTER eFilter)
{
	if (pBuffer == nullptr)
	{
		return false;
	}
	if (eFilter == MESHOPT_FILTER_NONE)
	{
		return true;
	}

	unsigned char* pBytes = static_cast<unsigned char*>(pBuffer);

	switch (eFilter)
	{
	case MESHOPT_FILTER_EXPONENTIAL:
	{
		// Each 32-bit word is a shared-exponent float: signed 8-bit exponent in
		// the top byte, signed 24-bit mantissa below it.
		if ((ulStride % 4) != 0)
		{
			return false;
		}
		const size_t ulWords = ulCount * ulStride / 4;
		for (size_t i = 0; i < ulWords; ++i)
		{
			unsigned int uWord = 0;
			memcpy(&uWord, pBytes + i * 4, 4);

			const int iExponent = static_cast<int>(uWord) >> 24;
			const int iMantissa = (static_cast<int>(uWord << 8)) >> 8;
			const float fValue = static_cast<float>(std::ldexp(static_cast<double>(iMantissa), iExponent));

			memcpy(pBytes + i * 4, &fValue, 4);
		}
		return true;
	}

	case MESHOPT_FILTER_OCTAHEDRAL:
	{
		// Two stored components plus a shared scale; the third is rebuilt and the
		// triple renormalised back onto the storage range.
		//
		// ★ THE FOLD'S SIGN IS THE TRAP, AND A GOLDEN VECTOR IS WHAT CAUGHT IT.
		// t is min(z, 0) -- so it is NEGATIVE or zero -- and the lower hemisphere
		// is folded by adding t to the component that is already positive. Writing
		// it the other way round (adding |t|) still produces plausible unit-ish
		// normals for every vertex, so nothing short of a byte comparison against
		// the reference notices. Only the lower hemisphere is affected, which on a
		// closed mesh is exactly half its shading.
		if (ulStride == 4)
		{
			for (size_t i = 0; i < ulCount; ++i)
			{
				signed char* pQ = reinterpret_cast<signed char*>(pBytes + i * 4);
				const float fOne = static_cast<float>(pQ[2]);
				float fX = static_cast<float>(pQ[0]);
				float fY = static_cast<float>(pQ[1]);
				const float fZ = fOne - (std::fabs(fX) + std::fabs(fY));

				const float fT = (fZ >= 0.0f) ? 0.0f : fZ;
				fX += (fX >= 0.0f) ? fT : -fT;
				fY += (fY >= 0.0f) ? fT : -fT;

				const float fScale = 127.0f / std::sqrt(fX * fX + fY * fY + fZ * fZ);
				pQ[0] = static_cast<signed char>(std::lround(fX * fScale));
				pQ[1] = static_cast<signed char>(std::lround(fY * fScale));
				pQ[2] = static_cast<signed char>(std::lround(fZ * fScale));
			}
			return true;
		}
		if (ulStride == 8)
		{
			for (size_t i = 0; i < ulCount; ++i)
			{
				short* pQ = reinterpret_cast<short*>(pBytes + i * 8);
				const float fOne = static_cast<float>(pQ[2]);
				float fX = static_cast<float>(pQ[0]);
				float fY = static_cast<float>(pQ[1]);
				const float fZ = fOne - (std::fabs(fX) + std::fabs(fY));

				const float fT = (fZ >= 0.0f) ? 0.0f : fZ;
				fX += (fX >= 0.0f) ? fT : -fT;
				fY += (fY >= 0.0f) ? fT : -fT;

				const float fScale = 32767.0f / std::sqrt(fX * fX + fY * fY + fZ * fZ);
				pQ[0] = static_cast<short>(std::lround(fX * fScale));
				pQ[1] = static_cast<short>(std::lround(fY * fScale));
				pQ[2] = static_cast<short>(std::lround(fZ * fScale));
			}
			return true;
		}
		return false;
	}

	case MESHOPT_FILTER_QUATERNION:
	{
		if (ulStride != 8)
		{
			return false;
		}
		for (size_t i = 0; i < ulCount; ++i)
		{
			short* pQ = reinterpret_cast<short*>(pBytes + i * 8);

			// The largest component is dropped; its index rides in the low two
			// bits of the fourth slot, and the rest share one scale.
			const int iMaxIndex = pQ[3] & 3;
			const float fScale = 1.0f / 32767.0f * static_cast<float>(std::sqrt(2.0));
			const float fXf = static_cast<float>(pQ[0]) * fScale;
			const float fYf = static_cast<float>(pQ[1]) * fScale;
			const float fZf = static_cast<float>(pQ[2]) * fScale;
			const float fWf = std::sqrt(
				(1.0f - fXf * fXf - fYf * fYf - fZf * fZf) > 0.0f
					? (1.0f - fXf * fXf - fYf * fYf - fZf * fZf)
					: 0.0f);

			const float afOut[4] = { fXf, fYf, fZf, fWf };
			// Rotate so the dropped component lands back in its own slot.
			for (int c = 0; c < 4; ++c)
			{
				const float fComponent = afOut[(c + 3 - iMaxIndex) & 3];
				pQ[c] = static_cast<short>(std::lround(fComponent * 32767.0f));
			}
		}
		return true;
	}

	default:
		return false;
	}
}

bool ParseFilterName(const char* szName, MESHOPT_FILTER& eFilterOut)
{
	if (szName == nullptr || szName[0] == '\0' || strcmp(szName, "NONE") == 0)
	{
		eFilterOut = MESHOPT_FILTER_NONE;
		return true;
	}
	if (strcmp(szName, "OCTAHEDRAL") == 0)
	{
		eFilterOut = MESHOPT_FILTER_OCTAHEDRAL;
		return true;
	}
	if (strcmp(szName, "QUATERNION") == 0)
	{
		eFilterOut = MESHOPT_FILTER_QUATERNION;
		return true;
	}
	if (strcmp(szName, "EXPONENTIAL") == 0)
	{
		eFilterOut = MESHOPT_FILTER_EXPONENTIAL;
		return true;
	}
	return false;
}

} // namespace Zenith_Tools_MeshoptDecode

#include "Zenith_Tools_MeshoptDecode.Tests.inl"
