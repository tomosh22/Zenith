#include "Zenith.h"
#include "Zenith_Tools_GlbImport.h"
#include "Zenith_Tools_HumanModelExport.h"
#include "Zenith_Tools_MeshoptDecode.h"
#include "Zenith_Tools_TextureExport.h"

#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "stb/stb_image.h"

// rapidjson ships inside the Assimp include tree this project already puts on
// the tools include path, so the glTF JSON chunk costs no new dependency.
#include <rapidjson/document.h>

#include <filesystem>
#include <string>
#include <vector>

// See the header for why this path exists at all and what it deliberately
// produces. The short version: Assimp cannot open a meshopt-compressed .glb, and
// the output has to be indistinguishable from a generated bundle.
namespace Zenith_Tools_GlbImport
{
namespace
{
	namespace Meshopt = Zenith_Tools_MeshoptDecode;

	constexpr u_int uGLB_MAGIC = 0x46546C67u;   // 'glTF'
	constexpr u_int uGLB_CHUNK_JSON = 0x4E4F534Au;
	constexpr u_int uGLB_CHUNK_BIN = 0x004E4942u;

	constexpr int iGLTF_MODE_TRIANGLES = 4;

	// glTF accessor component types.
	constexpr int iCT_BYTE = 5120;
	constexpr int iCT_UNSIGNED_BYTE = 5121;
	constexpr int iCT_SHORT = 5122;
	constexpr int iCT_UNSIGNED_SHORT = 5123;
	constexpr int iCT_UNSIGNED_INT = 5125;
	constexpr int iCT_FLOAT = 5126;

	//-------------------------------------------------------------------------
	// Small JSON conveniences. rapidjson's own accessors throw an assert on a
	// type mismatch, and a malformed asset must be a logged refusal rather than
	// a crashed tools boot, so every read goes through one of these.
	//-------------------------------------------------------------------------
	const rapidjson::Value* FindMember(const rapidjson::Value& xObj, const char* szName)
	{
		if (!xObj.IsObject())
		{
			return nullptr;
		}
		auto xIt = xObj.FindMember(szName);
		return (xIt != xObj.MemberEnd()) ? &xIt->value : nullptr;
	}

	int GetInt(const rapidjson::Value& xObj, const char* szName, int iFallback)
	{
		const rapidjson::Value* pxValue = FindMember(xObj, szName);
		return (pxValue != nullptr && pxValue->IsInt()) ? pxValue->GetInt() : iFallback;
	}

	float GetFloat(const rapidjson::Value& xObj, const char* szName, float fFallback)
	{
		const rapidjson::Value* pxValue = FindMember(xObj, szName);
		return (pxValue != nullptr && pxValue->IsNumber())
			? static_cast<float>(pxValue->GetDouble())
			: fFallback;
	}

	const char* GetString(const rapidjson::Value& xObj, const char* szName, const char* szFallback)
	{
		const rapidjson::Value* pxValue = FindMember(xObj, szName);
		return (pxValue != nullptr && pxValue->IsString()) ? pxValue->GetString() : szFallback;
	}

	// An array member, or nullptr when absent / not an array. uMinSize guards the
	// callers that index fixed slots (a 4-element colour, a 16-element matrix).
	const rapidjson::Value* GetArray(const rapidjson::Value& xObj, const char* szName, u_int uMinSize)
	{
		const rapidjson::Value* pxValue = FindMember(xObj, szName);
		if (pxValue == nullptr || !pxValue->IsArray() || pxValue->Size() < uMinSize)
		{
			return nullptr;
		}
		return pxValue;
	}

	//-------------------------------------------------------------------------
	// One decoded bufferView. A meshopt-compressed view owns its decoded bytes;
	// an ordinary one points into the GLB's BIN chunk.
	//-------------------------------------------------------------------------
	struct ResolvedBufferView
	{
		std::vector<unsigned char> m_xOwned;
		const unsigned char* m_pData = nullptr;
		size_t m_ulSize = 0;
		size_t m_ulStride = 0;
	};

	// Number of components per accessor "type" string.
	u_int ComponentCountForType(const char* szType)
	{
		if (strcmp(szType, "SCALAR") == 0) return 1u;
		if (strcmp(szType, "VEC2") == 0) return 2u;
		if (strcmp(szType, "VEC3") == 0) return 3u;
		if (strcmp(szType, "VEC4") == 0) return 4u;
		if (strcmp(szType, "MAT4") == 0) return 16u;
		return 0u;
	}

	u_int ComponentByteSize(int iComponentType)
	{
		switch (iComponentType)
		{
		case iCT_BYTE:
		case iCT_UNSIGNED_BYTE:  return 1u;
		case iCT_SHORT:
		case iCT_UNSIGNED_SHORT: return 2u;
		case iCT_UNSIGNED_INT:
		case iCT_FLOAT:          return 4u;
		default:                 return 0u;
		}
	}

	//-------------------------------------------------------------------------
	// The parsed document plus everything derived from it that more than one
	// stage needs.
	//-------------------------------------------------------------------------
	struct GlbDocument
	{
		rapidjson::Document m_xJson;
		std::vector<unsigned char> m_xFileBytes;
		const unsigned char* m_pBin = nullptr;
		size_t m_ulBinSize = 0;
		std::vector<ResolvedBufferView> m_xViews;
		std::string m_strSourcePath;
	};

	bool ReadWholeFile(const std::string& strPath, std::vector<unsigned char>& xOut)
	{
		std::error_code xEc;
		const std::uintmax_t ulSize = std::filesystem::file_size(std::filesystem::path(strPath), xEc);
		if (xEc || ulSize == 0)
		{
			return false;
		}
		FILE* pFile = nullptr;
		if (fopen_s(&pFile, strPath.c_str(), "rb") != 0 || pFile == nullptr)
		{
			return false;
		}
		xOut.resize(static_cast<size_t>(ulSize));
		const size_t ulRead = fread(xOut.data(), 1, xOut.size(), pFile);
		fclose(pFile);
		return ulRead == xOut.size();
	}

	// GLB container: a 12-byte header then length-prefixed chunks. Only the JSON
	// and BIN chunks matter; anything else is skipped by its own length, which is
	// what keeps a future chunk type from breaking the parse.
	bool ParseGlbContainer(GlbDocument& xDoc)
	{
		const std::vector<unsigned char>& xBytes = xDoc.m_xFileBytes;
		if (xBytes.size() < 12)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s is too small to be a GLB", xDoc.m_strSourcePath.c_str());
			return false;
		}

		u_int uMagic = 0, uVersion = 0, uLength = 0;
		memcpy(&uMagic, xBytes.data() + 0, 4);
		memcpy(&uVersion, xBytes.data() + 4, 4);
		memcpy(&uLength, xBytes.data() + 8, 4);

		if (uMagic != uGLB_MAGIC)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s is not a binary glTF (bad magic)", xDoc.m_strSourcePath.c_str());
			return false;
		}
		if (uVersion != 2u)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s is glTF version %u; only 2 is supported",
				xDoc.m_strSourcePath.c_str(), uVersion);
			return false;
		}
		if (uLength > xBytes.size())
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s declares %u bytes but is %zu",
				xDoc.m_strSourcePath.c_str(), uLength, xBytes.size());
			return false;
		}

		const unsigned char* pJson = nullptr;
		size_t ulJsonSize = 0;

		size_t ulOffset = 12;
		while (ulOffset + 8 <= static_cast<size_t>(uLength))
		{
			u_int uChunkLength = 0, uChunkType = 0;
			memcpy(&uChunkLength, xBytes.data() + ulOffset + 0, 4);
			memcpy(&uChunkType, xBytes.data() + ulOffset + 4, 4);
			ulOffset += 8;

			if (ulOffset + uChunkLength > static_cast<size_t>(uLength))
			{
				Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has a chunk running past the file end",
					xDoc.m_strSourcePath.c_str());
				return false;
			}

			if (uChunkType == uGLB_CHUNK_JSON)
			{
				pJson = xBytes.data() + ulOffset;
				ulJsonSize = uChunkLength;
			}
			else if (uChunkType == uGLB_CHUNK_BIN)
			{
				xDoc.m_pBin = xBytes.data() + ulOffset;
				xDoc.m_ulBinSize = uChunkLength;
			}

			// Chunks are 4-byte aligned; the declared length may not be.
			ulOffset += uChunkLength;
			ulOffset = (ulOffset + 3u) & ~static_cast<size_t>(3u);
		}

		if (pJson == nullptr || ulJsonSize == 0)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has no JSON chunk", xDoc.m_strSourcePath.c_str());
			return false;
		}

		const std::string strJson(reinterpret_cast<const char*>(pJson), ulJsonSize);
		xDoc.m_xJson.Parse(strJson.c_str());
		if (xDoc.m_xJson.HasParseError() || !xDoc.m_xJson.IsObject())
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has malformed glTF JSON", xDoc.m_strSourcePath.c_str());
			return false;
		}
		return true;
	}

	// Turn every bufferView into readable bytes, decompressing the meshopt ones.
	// Doing this up front means the accessor reader below never has to know that
	// the extension exists.
	bool ResolveBufferViews(GlbDocument& xDoc)
	{
		const rapidjson::Value* pxViews = FindMember(xDoc.m_xJson, "bufferViews");
		if (pxViews == nullptr || !pxViews->IsArray())
		{
			xDoc.m_xViews.clear();
			return true;   // a document with no views is legal, just useless
		}

		xDoc.m_xViews.resize(pxViews->Size());

		for (rapidjson::SizeType i = 0; i < pxViews->Size(); ++i)
		{
			const rapidjson::Value& xView = (*pxViews)[i];
			ResolvedBufferView& xResolved = xDoc.m_xViews[i];
			xResolved.m_ulStride = static_cast<size_t>(GetInt(xView, "byteStride", 0));

			const rapidjson::Value* pxExtensions = FindMember(xView, "extensions");
			const rapidjson::Value* pxMeshopt = (pxExtensions != nullptr)
				? FindMember(*pxExtensions, "EXT_meshopt_compression")
				: nullptr;

			if (pxMeshopt != nullptr)
			{
				const size_t ulSrcOffset = static_cast<size_t>(GetInt(*pxMeshopt, "byteOffset", 0));
				const size_t ulSrcLength = static_cast<size_t>(GetInt(*pxMeshopt, "byteLength", 0));
				const size_t ulCount = static_cast<size_t>(GetInt(*pxMeshopt, "count", 0));
				const size_t ulStride = static_cast<size_t>(GetInt(*pxMeshopt, "byteStride", 0));
				const char* szMode = GetString(*pxMeshopt, "mode", "");
				const char* szFilter = GetString(*pxMeshopt, "filter", "NONE");

				if (xDoc.m_pBin == nullptr || ulSrcOffset + ulSrcLength > xDoc.m_ulBinSize)
				{
					Zenith_Error(LOG_CATEGORY_TOOLS,
						"GLB_IMPORT: bufferView %u names compressed bytes outside the BIN chunk", i);
					return false;
				}
				if (ulCount == 0 || ulStride == 0)
				{
					Zenith_Error(LOG_CATEGORY_TOOLS,
						"GLB_IMPORT: bufferView %u has a zero meshopt count/stride", i);
					return false;
				}

				const unsigned char* pSrc = xDoc.m_pBin + ulSrcOffset;
				xResolved.m_xOwned.assign(ulCount * ulStride, 0);
				if (xResolved.m_ulStride == 0)
				{
					xResolved.m_ulStride = ulStride;
				}

				if (strcmp(szMode, "ATTRIBUTES") == 0)
				{
					if (!Meshopt::DecodeVertexBuffer(
						xResolved.m_xOwned.data(), ulCount, ulStride, pSrc, ulSrcLength))
					{
						Zenith_Error(LOG_CATEGORY_TOOLS,
							"GLB_IMPORT: meshopt vertex decode failed for bufferView %u", i);
						return false;
					}

					Meshopt::MESHOPT_FILTER eFilter = Meshopt::MESHOPT_FILTER_NONE;
					if (!Meshopt::ParseFilterName(szFilter, eFilter))
					{
						Zenith_Error(LOG_CATEGORY_TOOLS,
							"GLB_IMPORT: bufferView %u names unknown meshopt filter '%s'", i, szFilter);
						return false;
					}
					if (!Meshopt::ApplyFilter(xResolved.m_xOwned.data(), ulCount, ulStride, eFilter))
					{
						Zenith_Error(LOG_CATEGORY_TOOLS,
							"GLB_IMPORT: meshopt filter '%s' rejected stride %zu on bufferView %u",
							szFilter, ulStride, i);
						return false;
					}
				}
				else if (strcmp(szMode, "TRIANGLES") == 0)
				{
					// The decoder speaks 32-bit indices; the view's own stride
					// decides how they are stored back for the accessor to read.
					std::vector<unsigned int> xIndices(ulCount, 0u);
					if (!Meshopt::DecodeIndexBuffer(xIndices.data(), ulCount, pSrc, ulSrcLength))
					{
						Zenith_Error(LOG_CATEGORY_TOOLS,
							"GLB_IMPORT: meshopt index decode failed for bufferView %u", i);
						return false;
					}
					for (size_t k = 0; k < ulCount; ++k)
					{
						if (ulStride == 2)
						{
							const unsigned short uNarrow = static_cast<unsigned short>(xIndices[k]);
							if (static_cast<unsigned int>(uNarrow) != xIndices[k])
							{
								Zenith_Error(LOG_CATEGORY_TOOLS,
									"GLB_IMPORT: bufferView %u decoded index %u does not fit 16 bits",
									i, xIndices[k]);
								return false;
							}
							memcpy(xResolved.m_xOwned.data() + k * 2, &uNarrow, 2);
						}
						else if (ulStride == 4)
						{
							memcpy(xResolved.m_xOwned.data() + k * 4, &xIndices[k], 4);
						}
						else
						{
							Zenith_Error(LOG_CATEGORY_TOOLS,
								"GLB_IMPORT: bufferView %u has index stride %zu (expected 2 or 4)", i, ulStride);
							return false;
						}
					}
				}
				else
				{
					Zenith_Error(LOG_CATEGORY_TOOLS,
						"GLB_IMPORT: bufferView %u has unknown meshopt mode '%s'", i, szMode);
					return false;
				}

				xResolved.m_pData = xResolved.m_xOwned.data();
				xResolved.m_ulSize = xResolved.m_xOwned.size();
			}
			else
			{
				// Uncompressed: a slice of the BIN chunk. External-URI buffers are
				// refused rather than half-supported -- a .glb that needs sidecar
				// files is not the self-contained thing this path is for.
				const size_t ulOffset = static_cast<size_t>(GetInt(xView, "byteOffset", 0));
				const size_t ulLength = static_cast<size_t>(GetInt(xView, "byteLength", 0));
				if (xDoc.m_pBin == nullptr || ulOffset + ulLength > xDoc.m_ulBinSize)
				{
					Zenith_Error(LOG_CATEGORY_TOOLS,
						"GLB_IMPORT: bufferView %u lies outside the BIN chunk (external buffers are not supported)", i);
					return false;
				}
				xResolved.m_pData = xDoc.m_pBin + ulOffset;
				xResolved.m_ulSize = ulLength;
			}
		}
		return true;
	}

	//-------------------------------------------------------------------------
	// Accessor reads. Everything the mesh build needs is either float components
	// or unsigned integers, so there are exactly two readers.
	//-------------------------------------------------------------------------
	bool ReadAccessorFloats(
		const GlbDocument& xDoc,
		int iAccessor,
		std::vector<float>& xOut,
		u_int& uComponentsOut,
		u_int& uCountOut)
	{
		const rapidjson::Value* pxAccessors = FindMember(xDoc.m_xJson, "accessors");
		if (pxAccessors == nullptr || !pxAccessors->IsArray() ||
			iAccessor < 0 || static_cast<rapidjson::SizeType>(iAccessor) >= pxAccessors->Size())
		{
			return false;
		}
		const rapidjson::Value& xAccessor = (*pxAccessors)[static_cast<rapidjson::SizeType>(iAccessor)];

		const int iComponentType = GetInt(xAccessor, "componentType", 0);
		const u_int uCount = static_cast<u_int>(GetInt(xAccessor, "count", 0));
		const u_int uComponents = ComponentCountForType(GetString(xAccessor, "type", ""));
		const u_int uComponentSize = ComponentByteSize(iComponentType);
		const bool bNormalized = [&]() {
			const rapidjson::Value* pxNormalized = FindMember(xAccessor, "normalized");
			return pxNormalized != nullptr && pxNormalized->IsBool() && pxNormalized->GetBool();
		}();

		if (uCount == 0 || uComponents == 0 || uComponentSize == 0)
		{
			return false;
		}

		const int iView = GetInt(xAccessor, "bufferView", -1);
		if (iView < 0 || static_cast<size_t>(iView) >= xDoc.m_xViews.size())
		{
			// A sparse or zero-filled accessor. Not something a mesh attribute
			// legitimately uses here, so refuse rather than emit silent zeroes.
			return false;
		}
		const ResolvedBufferView& xView = xDoc.m_xViews[static_cast<size_t>(iView)];

		const size_t ulElementSize = static_cast<size_t>(uComponents) * uComponentSize;
		const size_t ulStride = (xView.m_ulStride != 0) ? xView.m_ulStride : ulElementSize;
		const size_t ulBase = static_cast<size_t>(GetInt(xAccessor, "byteOffset", 0));

		if (ulBase + (uCount - 1) * ulStride + ulElementSize > xView.m_ulSize)
		{
			return false;
		}

		xOut.assign(static_cast<size_t>(uCount) * uComponents, 0.0f);

		for (u_int i = 0; i < uCount; ++i)
		{
			const unsigned char* pElement = xView.m_pData + ulBase + static_cast<size_t>(i) * ulStride;
			for (u_int c = 0; c < uComponents; ++c)
			{
				const unsigned char* pComponent = pElement + static_cast<size_t>(c) * uComponentSize;
				float fValue = 0.0f;
				switch (iComponentType)
				{
				case iCT_FLOAT:
					memcpy(&fValue, pComponent, 4);
					break;
				case iCT_BYTE:
				{
					signed char cValue = 0;
					memcpy(&cValue, pComponent, 1);
					// glTF: a normalized signed value maps -128 and -127 both to -1.
					const float fNormalized = static_cast<float>(cValue) / 127.0f;
					fValue = bNormalized ? (fNormalized < -1.0f ? -1.0f : fNormalized)
					                     : static_cast<float>(cValue);
					break;
				}
				case iCT_UNSIGNED_BYTE:
				{
					const unsigned char uValue = *pComponent;
					fValue = bNormalized ? static_cast<float>(uValue) / 255.0f : static_cast<float>(uValue);
					break;
				}
				case iCT_SHORT:
				{
					short iValue = 0;
					memcpy(&iValue, pComponent, 2);
					const float fNormalized = static_cast<float>(iValue) / 32767.0f;
					fValue = bNormalized ? (fNormalized < -1.0f ? -1.0f : fNormalized)
					                     : static_cast<float>(iValue);
					break;
				}
				case iCT_UNSIGNED_SHORT:
				{
					unsigned short uValue = 0;
					memcpy(&uValue, pComponent, 2);
					fValue = bNormalized ? static_cast<float>(uValue) / 65535.0f : static_cast<float>(uValue);
					break;
				}
				case iCT_UNSIGNED_INT:
				{
					u_int uValue = 0;
					memcpy(&uValue, pComponent, 4);
					fValue = static_cast<float>(uValue);
					break;
				}
				default:
					return false;
				}
				xOut[static_cast<size_t>(i) * uComponents + c] = fValue;
			}
		}

		uComponentsOut = uComponents;
		uCountOut = uCount;
		return true;
	}

	bool ReadAccessorIndices(const GlbDocument& xDoc, int iAccessor, std::vector<u_int>& xOut)
	{
		const rapidjson::Value* pxAccessors = FindMember(xDoc.m_xJson, "accessors");
		if (pxAccessors == nullptr || !pxAccessors->IsArray() ||
			iAccessor < 0 || static_cast<rapidjson::SizeType>(iAccessor) >= pxAccessors->Size())
		{
			return false;
		}
		const rapidjson::Value& xAccessor = (*pxAccessors)[static_cast<rapidjson::SizeType>(iAccessor)];

		const int iComponentType = GetInt(xAccessor, "componentType", 0);
		const u_int uCount = static_cast<u_int>(GetInt(xAccessor, "count", 0));
		const u_int uComponentSize = ComponentByteSize(iComponentType);
		if (uCount == 0 || uComponentSize == 0)
		{
			return false;
		}

		const int iView = GetInt(xAccessor, "bufferView", -1);
		if (iView < 0 || static_cast<size_t>(iView) >= xDoc.m_xViews.size())
		{
			return false;
		}
		const ResolvedBufferView& xView = xDoc.m_xViews[static_cast<size_t>(iView)];
		const size_t ulStride = (xView.m_ulStride != 0) ? xView.m_ulStride : uComponentSize;
		const size_t ulBase = static_cast<size_t>(GetInt(xAccessor, "byteOffset", 0));

		if (ulBase + (uCount - 1) * ulStride + uComponentSize > xView.m_ulSize)
		{
			return false;
		}

		xOut.assign(uCount, 0u);
		for (u_int i = 0; i < uCount; ++i)
		{
			const unsigned char* pElement = xView.m_pData + ulBase + static_cast<size_t>(i) * ulStride;
			switch (iComponentType)
			{
			case iCT_UNSIGNED_BYTE:
				xOut[i] = *pElement;
				break;
			case iCT_UNSIGNED_SHORT:
			{
				unsigned short uValue = 0;
				memcpy(&uValue, pElement, 2);
				xOut[i] = uValue;
				break;
			}
			case iCT_UNSIGNED_INT:
				memcpy(&xOut[i], pElement, 4);
				break;
			default:
				return false;
			}
		}
		return true;
	}

	//-------------------------------------------------------------------------
	// Node transforms. Baked into vertices exactly the way the Assimp path bakes
	// them, so an imported model and an Assimp-imported one agree on what "the
	// model's own space" means.
	//-------------------------------------------------------------------------
	Zenith_Maths::Matrix4 LocalTransformOfNode(const rapidjson::Value& xNode)
	{
		if (const rapidjson::Value* pxMatrix = GetArray(xNode, "matrix", 16u))
		{
			Zenith_Maths::Matrix4 xMatrix(1.0f);
			// glTF stores column-major, which is glm's own layout.
			for (u_int c = 0; c < 4; ++c)
			{
				for (u_int r = 0; r < 4; ++r)
				{
					const rapidjson::Value& xComponent = (*pxMatrix)[c * 4 + r];
					xMatrix[c][r] = xComponent.IsNumber() ? static_cast<float>(xComponent.GetDouble()) : 0.0f;
				}
			}
			return xMatrix;
		}

		Zenith_Maths::Vector3 xTranslation(0.0f);
		Zenith_Maths::Vector3 xScale(1.0f);
		Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);

		if (const rapidjson::Value* pxT = GetArray(xNode, "translation", 3u))
		{
			for (u_int i = 0; i < 3; ++i)
			{
				xTranslation[i] = static_cast<float>((*pxT)[i].GetDouble());
			}
		}
		if (const rapidjson::Value* pxS = GetArray(xNode, "scale", 3u))
		{
			for (u_int i = 0; i < 3; ++i)
			{
				xScale[i] = static_cast<float>((*pxS)[i].GetDouble());
			}
		}
		if (const rapidjson::Value* pxR = GetArray(xNode, "rotation", 4u))
		{
			// glTF orders a quaternion x,y,z,w; glm's constructor takes w first.
			xRotation = Zenith_Maths::Quat(
				static_cast<float>((*pxR)[3].GetDouble()),
				static_cast<float>((*pxR)[0].GetDouble()),
				static_cast<float>((*pxR)[1].GetDouble()),
				static_cast<float>((*pxR)[2].GetDouble()));
		}

		Zenith_Maths::Matrix4 xMatrix(1.0f);
		xMatrix = glm::translate(xMatrix, xTranslation);
		xMatrix = xMatrix * glm::mat4_cast(xRotation);
		xMatrix = glm::scale(xMatrix, xScale);
		return xMatrix;
	}

	// One primitive, flattened into the arrays the mesh build appends from.
	struct PrimitiveInstance
	{
		int m_iMesh = -1;
		int m_iPrimitive = -1;
		int m_iMaterial = -1;
		Zenith_Maths::Matrix4 m_xWorld = Zenith_Maths::Matrix4(1.0f);
	};

	void GatherNode(
		const GlbDocument& xDoc,
		const rapidjson::Value& xNodes,
		int iNodeIndex,
		const Zenith_Maths::Matrix4& xParent,
		std::vector<PrimitiveInstance>& xOut,
		u_int uDepth)
	{
		// glTF node graphs are trees, but a malformed file can name a cycle and
		// this walk would never return. A depth cap is cheaper than a visited set
		// and cannot be defeated by a re-entered subtree.
		constexpr u_int uMAX_NODE_DEPTH = 64u;
		if (uDepth > uMAX_NODE_DEPTH ||
			iNodeIndex < 0 || static_cast<rapidjson::SizeType>(iNodeIndex) >= xNodes.Size())
		{
			return;
		}
		const rapidjson::Value& xNode = xNodes[static_cast<rapidjson::SizeType>(iNodeIndex)];
		const Zenith_Maths::Matrix4 xWorld = xParent * LocalTransformOfNode(xNode);

		const int iMesh = GetInt(xNode, "mesh", -1);
		if (iMesh >= 0)
		{
			const rapidjson::Value* pxMeshes = FindMember(xDoc.m_xJson, "meshes");
			if (pxMeshes != nullptr && pxMeshes->IsArray() &&
				static_cast<rapidjson::SizeType>(iMesh) < pxMeshes->Size())
			{
				const rapidjson::Value* pxPrimitives =
					FindMember((*pxMeshes)[static_cast<rapidjson::SizeType>(iMesh)], "primitives");
				if (pxPrimitives != nullptr && pxPrimitives->IsArray())
				{
					for (rapidjson::SizeType p = 0; p < pxPrimitives->Size(); ++p)
					{
						const rapidjson::Value& xPrimitive = (*pxPrimitives)[p];
						if (GetInt(xPrimitive, "mode", iGLTF_MODE_TRIANGLES) != iGLTF_MODE_TRIANGLES)
						{
							continue;   // lines/points carry no surface to shade
						}
						PrimitiveInstance xInstance;
						xInstance.m_iMesh = iMesh;
						xInstance.m_iPrimitive = static_cast<int>(p);
						xInstance.m_iMaterial = GetInt(xPrimitive, "material", -1);
						xInstance.m_xWorld = xWorld;
						xOut.push_back(xInstance);
					}
				}
			}
		}

		if (const rapidjson::Value* pxChildren = FindMember(xNode, "children"))
		{
			if (pxChildren->IsArray())
			{
				for (rapidjson::SizeType c = 0; c < pxChildren->Size(); ++c)
				{
					if ((*pxChildren)[c].IsInt())
					{
						GatherNode(xDoc, xNodes, (*pxChildren)[c].GetInt(), xWorld, xOut, uDepth + 1u);
					}
				}
			}
		}
	}

	std::vector<PrimitiveInstance> GatherPrimitives(const GlbDocument& xDoc)
	{
		std::vector<PrimitiveInstance> xOut;

		const rapidjson::Value* pxNodes = FindMember(xDoc.m_xJson, "nodes");
		if (pxNodes == nullptr || !pxNodes->IsArray())
		{
			return xOut;
		}

		const int iScene = GetInt(xDoc.m_xJson, "scene", 0);
		const rapidjson::Value* pxScenes = FindMember(xDoc.m_xJson, "scenes");
		if (pxScenes != nullptr && pxScenes->IsArray() &&
			iScene >= 0 && static_cast<rapidjson::SizeType>(iScene) < pxScenes->Size())
		{
			const rapidjson::Value* pxRoots =
				FindMember((*pxScenes)[static_cast<rapidjson::SizeType>(iScene)], "nodes");
			if (pxRoots != nullptr && pxRoots->IsArray())
			{
				for (rapidjson::SizeType i = 0; i < pxRoots->Size(); ++i)
				{
					if ((*pxRoots)[i].IsInt())
					{
						GatherNode(xDoc, *pxNodes, (*pxRoots)[i].GetInt(),
							Zenith_Maths::Matrix4(1.0f), xOut, 0u);
					}
				}
			}
		}
		return xOut;
	}

	//-------------------------------------------------------------------------
	// Image extraction
	//-------------------------------------------------------------------------
	struct DecodedImage
	{
		std::vector<unsigned char> m_xRGBA;
		int m_iWidth = 0;
		int m_iHeight = 0;
		bool IsValid() const { return m_iWidth > 0 && m_iHeight > 0 && !m_xRGBA.empty(); }
	};

	// glTF texture index -> image index -> bytes. Both indirections are resolved
	// here so callers only ever hold a texture index.
	bool DecodeTextureImage(const GlbDocument& xDoc, int iTexture, DecodedImage& xOut)
	{
		const rapidjson::Value* pxTextures = FindMember(xDoc.m_xJson, "textures");
		if (pxTextures == nullptr || !pxTextures->IsArray() ||
			iTexture < 0 || static_cast<rapidjson::SizeType>(iTexture) >= pxTextures->Size())
		{
			return false;
		}
		const int iImage = GetInt((*pxTextures)[static_cast<rapidjson::SizeType>(iTexture)], "source", -1);

		const rapidjson::Value* pxImages = FindMember(xDoc.m_xJson, "images");
		if (pxImages == nullptr || !pxImages->IsArray() ||
			iImage < 0 || static_cast<rapidjson::SizeType>(iImage) >= pxImages->Size())
		{
			return false;
		}
		const rapidjson::Value& xImage = (*pxImages)[static_cast<rapidjson::SizeType>(iImage)];

		const unsigned char* pEncoded = nullptr;
		size_t ulEncodedSize = 0;
		std::vector<unsigned char> xFromFile;

		const int iView = GetInt(xImage, "bufferView", -1);
		if (iView >= 0 && static_cast<size_t>(iView) < xDoc.m_xViews.size())
		{
			pEncoded = xDoc.m_xViews[static_cast<size_t>(iView)].m_pData;
			ulEncodedSize = xDoc.m_xViews[static_cast<size_t>(iView)].m_ulSize;
		}
		else
		{
			const char* szUri = GetString(xImage, "uri", "");
			if (szUri[0] == '\0' || strncmp(szUri, "data:", 5) == 0)
			{
				// Base64 data URIs are not supported: a .glb that embeds images as
				// text has already given up the only advantage of being binary.
				return false;
			}
			const std::filesystem::path xDir =
				std::filesystem::path(xDoc.m_strSourcePath).parent_path();
			if (!ReadWholeFile((xDir / szUri).string(), xFromFile))
			{
				return false;
			}
			pEncoded = xFromFile.data();
			ulEncodedSize = xFromFile.size();
		}

		if (pEncoded == nullptr || ulEncodedSize == 0)
		{
			return false;
		}

		int iChannels = 0;
		unsigned char* pPixels = stbi_load_from_memory(
			pEncoded, static_cast<int>(ulEncodedSize),
			&xOut.m_iWidth, &xOut.m_iHeight, &iChannels, STBI_rgb_alpha);
		if (pPixels == nullptr)
		{
			return false;
		}
		xOut.m_xRGBA.assign(pPixels,
			pPixels + static_cast<size_t>(xOut.m_iWidth) * xOut.m_iHeight * 4);
		stbi_image_free(pPixels);
		return true;
	}
}

//-----------------------------------------------------------------------------
// The import itself
//-----------------------------------------------------------------------------
// ★ PARSE AND DECODE TO MEMORY, AND NOTHING ELSE. No tangents, no bounds,
// nothing written. A caller that MOVES vertices -- which is exactly what
// Zenith_Tools_HumanSkinBind does, twice -- has to generate tangents after the
// vertices stop moving, so generating them here would guarantee they are wrong
// for that caller and silently right for everyone else. The node transform IS
// baked in, because that is part of reading the file rather than of using it.
bool LoadGlbMesh(const std::string& strGlbPath, Zenith_MeshAsset& xMesh, GlbImportResult& xResult)
{
	GlbDocument xDoc;
	xDoc.m_strSourcePath = strGlbPath;
	if (!ReadWholeFile(strGlbPath, xDoc.m_xFileBytes))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: could not read %s", strGlbPath.c_str());
		return false;
	}
	if (!ParseGlbContainer(xDoc) || !ResolveBufferViews(xDoc))
	{
		return false;
	}

	const std::filesystem::path xPath(strGlbPath);
	const std::string strBaseName = (xPath.parent_path() / xPath.stem()).string();
	const std::string strModelName = xPath.stem().string();

	// ---- Geometry -----------------------------------------------------------
	const std::vector<PrimitiveInstance> xPrimitives = GatherPrimitives(xDoc);
	if (xPrimitives.empty())
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s contains no triangle primitives", strGlbPath.c_str());
		return false;
	}

	const rapidjson::Value* pxMeshes = FindMember(xDoc.m_xJson, "meshes");

	u_int uMaterialSlots = 0;

	for (const PrimitiveInstance& xInstance : xPrimitives)
	{
		const rapidjson::Value& xPrimitive =
			(*FindMember((*pxMeshes)[static_cast<rapidjson::SizeType>(xInstance.m_iMesh)], "primitives"))
			[static_cast<rapidjson::SizeType>(xInstance.m_iPrimitive)];

		const rapidjson::Value* pxAttributes = FindMember(xPrimitive, "attributes");
		if (pxAttributes == nullptr)
		{
			continue;
		}

		std::vector<float> xPositions, xNormals, xUVs;
		u_int uComponents = 0, uCount = 0;
		if (!ReadAccessorFloats(xDoc, GetInt(*pxAttributes, "POSITION", -1), xPositions, uComponents, uCount) ||
			uComponents != 3u)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has a primitive with no readable POSITION",
				strGlbPath.c_str());
			return false;
		}

		u_int uNormalComponents = 0, uNormalCount = 0;
		const bool bHasNormals =
			ReadAccessorFloats(xDoc, GetInt(*pxAttributes, "NORMAL", -1), xNormals, uNormalComponents, uNormalCount) &&
			uNormalComponents == 3u && uNormalCount == uCount;

		u_int uUVComponents = 0, uUVCount = 0;
		const bool bHasUVs =
			ReadAccessorFloats(xDoc, GetInt(*pxAttributes, "TEXCOORD_0", -1), xUVs, uUVComponents, uUVCount) &&
			uUVComponents == 2u && uUVCount == uCount;

		std::vector<u_int> xIndices;
		const int iIndicesAccessor = GetInt(xPrimitive, "indices", -1);
		if (iIndicesAccessor >= 0)
		{
			if (!ReadAccessorIndices(xDoc, iIndicesAccessor, xIndices))
			{
				Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has an unreadable index accessor",
					strGlbPath.c_str());
				return false;
			}
		}
		else
		{
			xIndices.resize(uCount);
			for (u_int i = 0; i < uCount; ++i)
			{
				xIndices[i] = i;
			}
		}
		if (xIndices.size() % 3 != 0)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s has an index count that is not a multiple of 3",
				strGlbPath.c_str());
			return false;
		}

		const u_int uVertexBase = xMesh.GetNumVerts();
		const u_int uIndexBase = xMesh.GetNumIndices();

		// The node transform is baked into positions and normals, so the exported
		// mesh needs no scene hierarchy to sit where the artist put it. Normals
		// take the inverse-transpose so a non-uniform node scale cannot shear them.
		const Zenith_Maths::Matrix4& xWorld = xInstance.m_xWorld;
		const Zenith_Maths::Matrix3 xNormalMatrix =
			Zenith_Maths::Matrix3(glm::transpose(glm::inverse(xWorld)));

		for (u_int i = 0; i < uCount; ++i)
		{
			const Zenith_Maths::Vector4 xLocal(
				xPositions[static_cast<size_t>(i) * 3 + 0],
				xPositions[static_cast<size_t>(i) * 3 + 1],
				xPositions[static_cast<size_t>(i) * 3 + 2],
				1.0f);
			const Zenith_Maths::Vector4 xWorldPos = xWorld * xLocal;

			Zenith_Maths::Vector3 xNormal(0.0f, 1.0f, 0.0f);
			if (bHasNormals)
			{
				xNormal = xNormalMatrix * Zenith_Maths::Vector3(
					xNormals[static_cast<size_t>(i) * 3 + 0],
					xNormals[static_cast<size_t>(i) * 3 + 1],
					xNormals[static_cast<size_t>(i) * 3 + 2]);
				const float fLength = glm::length(xNormal);
				xNormal = (fLength > 1e-8f) ? (xNormal / fLength) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			}

			const Zenith_Maths::Vector2 xUV = bHasUVs
				? Zenith_Maths::Vector2(xUVs[static_cast<size_t>(i) * 2 + 0],
					xUVs[static_cast<size_t>(i) * 2 + 1])
				: Zenith_Maths::Vector2(0.0f, 0.0f);

			xMesh.AddVertex(
				Zenith_Maths::Vector3(xWorldPos.x, xWorldPos.y, xWorldPos.z),
				xNormal,
				xUV);
		}

		for (size_t t = 0; t + 2 < xIndices.size(); t += 3)
		{
			if (xIndices[t] >= uCount || xIndices[t + 1] >= uCount || xIndices[t + 2] >= uCount)
			{
				Zenith_Error(LOG_CATEGORY_TOOLS,
					"GLB_IMPORT: %s has an index past the end of its own vertex array", strGlbPath.c_str());
				return false;
			}
			xMesh.AddTriangle(
				uVertexBase + xIndices[t + 0],
				uVertexBase + xIndices[t + 1],
				uVertexBase + xIndices[t + 2]);
		}

		const u_int uMaterialSlot = (xInstance.m_iMaterial >= 0)
			? static_cast<u_int>(xInstance.m_iMaterial)
			: 0u;
		xMesh.AddSubmesh(uIndexBase, static_cast<u_int>(xIndices.size()), uMaterialSlot);
		uMaterialSlots = (uMaterialSlot + 1u > uMaterialSlots) ? (uMaterialSlot + 1u) : uMaterialSlots;

		if (!bHasNormals)
		{
			Zenith_Warning(LOG_CATEGORY_TOOLS,
				"GLB_IMPORT: %s has a primitive with no NORMAL; normals will be generated", strGlbPath.c_str());
		}
	}

	if (xMesh.GetNumVerts() == 0 || xMesh.GetNumIndices() == 0)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s produced an empty mesh", strGlbPath.c_str());
		return false;
	}

	// ★★ THIS ENGINE WINDS THE OTHER WAY FROM glTF, and an import is INSIDE-OUT
	// until that is fixed.
	//
	// Zenith's outward normal is cross(C-A, B-A) (Zenith_Tools_TestAssetExport.cpp
	// :1284 states it, and the rock exporter re-winds its icosphere for exactly
	// this reason); glTF, like OpenGL, uses the other handedness. Copying indices
	// verbatim therefore leaves every triangle of a closed mesh facing inward.
	//
	// ★ THE SYMPTOM IS NOT "the shading looks odd". Normals come from the file and
	// stay correct, so lighting is fine and nothing errors. Backface culling keeps
	// the FAR surface, so you see through the model to its far side -- on a prop
	// that is a subtle wrongness nobody names, and on a CHARACTER it reads as
	// FACING BACKWARDS, with anything attached behind them drawing in front. That
	// is what shipped here, and it survived a screenshot pass because the picture
	// is of a plausible person looking the wrong way, not of anything broken.
	//
	// MEASURED, not assumed: the signed volume of a closed mesh states its winding
	// directly, so a source that already winds the engine's way is left alone. The
	// reference is the shipped StickFigure, whose sum is NEGATIVE under this
	// formula.
	{
		double dVolume = 0.0;
		for (u_int i = 0u; i + 2u < xMesh.m_xIndices.GetSize(); i += 3u)
		{
			const Zenith_Maths::Vector3& a = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(i));
			const Zenith_Maths::Vector3& b = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(i + 1u));
			const Zenith_Maths::Vector3& c = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(i + 2u));
			dVolume += glm::dot(a, glm::cross(b, c));
		}
		if (dVolume > 0.0)
		{
			for (u_int i = 0u; i + 2u < xMesh.m_xIndices.GetSize(); i += 3u)
			{
				const u_int uSwap = xMesh.m_xIndices.Get(i + 1u);
				xMesh.m_xIndices.Get(i + 1u) = xMesh.m_xIndices.Get(i + 2u);
				xMesh.m_xIndices.Get(i + 2u) = uSwap;
			}
			Zenith_Log(LOG_CATEGORY_TOOLS,
				"GLB_IMPORT: %s winds the glTF way (signed volume %+.6f); re-wound to the engine's "
				"cross(C-A, B-A) convention", strGlbPath.c_str(), dVolume / 6.0);
		}
	}

	xResult.m_uNumVerts = xMesh.GetNumVerts();
	xResult.m_uNumIndices = xMesh.GetNumIndices();
	xResult.m_uNumSubmeshes = xMesh.GetNumSubmeshes();
	return true;
}

// The material/texture half of an import: every channel convention lives here
// and NOWHERE ELSE, so a specialised exporter that builds its own geometry
// still writes the same bundle the generic path would.
bool ExportGlbMaterials(const std::string& strGlbPath, const std::string& strBaseName,
	Zenith_Vector<std::string>& xRefsOut, u_int& uTexturesOut)
{
	GlbDocument xDoc;
	xDoc.m_strSourcePath = strGlbPath;
	if (!ReadWholeFile(strGlbPath, xDoc.m_xFileBytes) ||
		!ParseGlbContainer(xDoc) || !ResolveBufferViews(xDoc))
	{
		return false;
	}
	const std::string strModelName = std::filesystem::path(strBaseName).filename().string();

	u_int uMaterialSlots = 0;
	for (const PrimitiveInstance& xInstance : GatherPrimitives(xDoc))
	{
		const u_int uSlot = (xInstance.m_iMaterial >= 0) ? static_cast<u_int>(xInstance.m_iMaterial) : 0u;
		uMaterialSlots = (uSlot + 1u > uMaterialSlots) ? (uSlot + 1u) : uMaterialSlots;
	}

	const rapidjson::Value* pxMaterials = FindMember(xDoc.m_xJson, "materials");
	const u_int uMaterialCount = (uMaterialSlots > 0u) ? uMaterialSlots : 1u;


	for (u_int m = 0; m < uMaterialCount; ++m)
	{
		// Material 0 owns the canonical bundle names, so a single-material model
		// -- which is what a prop is -- lands on exactly the paths a generator
		// would have written. See the header.
		const std::string strSuffix = (m == 0) ? std::string() : ("_mat" + std::to_string(m));

		const std::string strAlbedoPath = strBaseName + strSuffix + "_albedo.ztxtr";
		const std::string strNormalPath = strBaseName + strSuffix + "_normal.ztxtr";
		const std::string strRmPath = strBaseName + strSuffix + "_rm.ztxtr";
		const std::string strAoPath = strBaseName + strSuffix + "_ao.ztxtr";
		const std::string strMaterialPath = strBaseName + strSuffix + ".zmtrl";

		const rapidjson::Value* pxMaterial = nullptr;
		if (pxMaterials != nullptr && pxMaterials->IsArray() &&
			static_cast<rapidjson::SizeType>(m) < pxMaterials->Size())
		{
			pxMaterial = &(*pxMaterials)[static_cast<rapidjson::SizeType>(m)];
		}

		int iBaseColorTexture = -1;
		int iMetallicRoughnessTexture = -1;
		int iNormalTexture = -1;
		int iOcclusionTexture = -1;
		int iEmissiveTexture = -1;

		// glTF's own defaults, which are NOT the engine's: an absent
		// metallicFactor/roughnessFactor means 1.0, i.e. "the texture speaks for
		// itself". Substituting the engine's 0.5 here would quietly halve every
		// imported roughness map.
		float fMetallic = 1.0f;
		float fRoughness = 1.0f;
		float fNormalScale = 1.0f;
		float fOcclusionStrength = 1.0f;
		Zenith_Maths::Vector4 xBaseColor(1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_Maths::Vector3 xEmissive(0.0f, 0.0f, 0.0f);
		bool bTwoSided = false;
		float fAlphaCutoff = 0.5f;
		MaterialBlendMode eBlendMode = MATERIAL_BLEND_OPAQUE;

		if (pxMaterial != nullptr)
		{
			if (const rapidjson::Value* pxPbr = FindMember(*pxMaterial, "pbrMetallicRoughness"))
			{
				fMetallic = GetFloat(*pxPbr, "metallicFactor", 1.0f);
				fRoughness = GetFloat(*pxPbr, "roughnessFactor", 1.0f);
				if (const rapidjson::Value* pxFactor = GetArray(*pxPbr, "baseColorFactor", 4u))
				{
					for (u_int i = 0; i < 4; ++i)
					{
						xBaseColor[i] = static_cast<float>((*pxFactor)[i].GetDouble());
					}
				}
				if (const rapidjson::Value* pxTex = FindMember(*pxPbr, "baseColorTexture"))
				{
					iBaseColorTexture = GetInt(*pxTex, "index", -1);
				}
				if (const rapidjson::Value* pxTex = FindMember(*pxPbr, "metallicRoughnessTexture"))
				{
					iMetallicRoughnessTexture = GetInt(*pxTex, "index", -1);
				}
			}
			if (const rapidjson::Value* pxTex = FindMember(*pxMaterial, "normalTexture"))
			{
				iNormalTexture = GetInt(*pxTex, "index", -1);
				fNormalScale = GetFloat(*pxTex, "scale", 1.0f);
			}
			if (const rapidjson::Value* pxTex = FindMember(*pxMaterial, "occlusionTexture"))
			{
				iOcclusionTexture = GetInt(*pxTex, "index", -1);
				fOcclusionStrength = GetFloat(*pxTex, "strength", 1.0f);
			}
			if (const rapidjson::Value* pxTex = FindMember(*pxMaterial, "emissiveTexture"))
			{
				iEmissiveTexture = GetInt(*pxTex, "index", -1);
			}
			if (const rapidjson::Value* pxEmissive = GetArray(*pxMaterial, "emissiveFactor", 3u))
			{
				for (u_int i = 0; i < 3; ++i)
				{
					xEmissive[i] = static_cast<float>((*pxEmissive)[i].GetDouble());
				}
			}
			if (const rapidjson::Value* pxDoubleSided = FindMember(*pxMaterial, "doubleSided"))
			{
				bTwoSided = pxDoubleSided->IsBool() && pxDoubleSided->GetBool();
			}

			const char* szAlphaMode = GetString(*pxMaterial, "alphaMode", "OPAQUE");
			if (strcmp(szAlphaMode, "MASK") == 0)
			{
				eBlendMode = MATERIAL_BLEND_MASKED;
				fAlphaCutoff = GetFloat(*pxMaterial, "alphaCutoff", 0.5f);
			}
			else if (strcmp(szAlphaMode, "BLEND") == 0)
			{
				eBlendMode = MATERIAL_BLEND_TRANSLUCENT;
			}
		}

		//-- Texture writes. The slot decides the colour space, exactly: base
		//   colour and emissive are DISPLAYED colour and export as the sRGB BC
		//   twin (the sampler decodes them to linear); normal, roughness/metallic
		//   and occlusion are data and stay linear UNORM.
		bool bWroteAlbedo = false;
		bool bWroteNormal = false;
		bool bWroteRm = false;
		bool bWroteAo = false;

		DecodedImage xImage;
		if (iBaseColorTexture >= 0 && DecodeTextureImage(xDoc, iBaseColorTexture, xImage) && xImage.IsValid())
		{
			Zenith_Tools_TextureExport::ExportFromDataCompressed(
				xImage.m_xRGBA.data(), strAlbedoPath, xImage.m_iWidth, xImage.m_iHeight,
				TextureCompressionMode::BC1, TextureColourSpace::SRGB);
			bWroteAlbedo = true;
			++uTexturesOut;
		}

		xImage = DecodedImage();
		if (iNormalTexture >= 0 && DecodeTextureImage(xDoc, iNormalTexture, xImage) && xImage.IsValid())
		{
			Zenith_Tools_TextureExport::ExportFromDataCompressed(
				xImage.m_xRGBA.data(), strNormalPath, xImage.m_iWidth, xImage.m_iHeight,
				TextureCompressionMode::BC5, TextureColourSpace::Linear);
			bWroteNormal = true;
			++uTexturesOut;
		}

		// ★ NO SWIZZLE. glTF packs roughness in G and metallic in B, and Flux's
		// SampleRoughnessMetallic reads exactly .gb. The two conventions already
		// agree; "fixing" one would break the other.
		xImage = DecodedImage();
		if (iMetallicRoughnessTexture >= 0 &&
			DecodeTextureImage(xDoc, iMetallicRoughnessTexture, xImage) && xImage.IsValid())
		{
			Zenith_Tools_TextureExport::ExportFromDataCompressed(
				xImage.m_xRGBA.data(), strRmPath, xImage.m_iWidth, xImage.m_iHeight,
				TextureCompressionMode::BC1, TextureColourSpace::Linear);
			bWroteRm = true;
			++uTexturesOut;
		}

		xImage = DecodedImage();
		if (iOcclusionTexture >= 0 && DecodeTextureImage(xDoc, iOcclusionTexture, xImage) && xImage.IsValid())
		{
			Zenith_Tools_TextureExport::ExportFromDataCompressed(
				xImage.m_xRGBA.data(), strAoPath, xImage.m_iWidth, xImage.m_iHeight,
				TextureCompressionMode::BC1, TextureColourSpace::Linear);
			bWroteAo = true;
			++uTexturesOut;
		}

		if (!bWroteAo)
		{
			// ★ NEUTRAL WHITE RATHER THAN NOTHING. Many exporters ship no occlusion
			// map at all (glTF's own convention would fold it into the R channel of
			// the metallic-roughness texture, which gltfpack leaves at 255). The
			// slot's absent-texture default is white too, so this changes no pixel
			// -- but a generator that checks "are all my files present" reads a
			// MISSING file as "not baked" and overwrites the whole imported bundle
			// on the next boot. Writing it keeps the bundle complete.
			constexpr int iNEUTRAL_AO_SIZE = 4;
			unsigned char auWhite[iNEUTRAL_AO_SIZE * iNEUTRAL_AO_SIZE * 4];
			memset(auWhite, 0xFF, sizeof(auWhite));
			Zenith_Tools_TextureExport::ExportFromDataCompressed(
				auWhite, strAoPath, iNEUTRAL_AO_SIZE, iNEUTRAL_AO_SIZE, TextureCompressionMode::BC1, TextureColourSpace::Linear);
			bWroteAo = true;
			++uTexturesOut;
		}

		//-- The .zmtrl
		{
			Zenith_AssetHandle<Zenith_MaterialAsset> xMaterialHandle =
				Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMaterialAsset = xMaterialHandle.GetDirect();
			pxMaterialAsset->SetName(strModelName + strSuffix);

			if (bWroteAlbedo)
			{
				pxMaterialAsset->SetDiffuseTexture(TextureHandle(strAlbedoPath));
			}
			if (bWroteNormal)
			{
				pxMaterialAsset->SetNormalTexture(TextureHandle(strNormalPath));
			}
			if (bWroteRm)
			{
				pxMaterialAsset->SetRoughnessMetallicTexture(TextureHandle(strRmPath));
			}
			if (bWroteAo)
			{
				pxMaterialAsset->SetOcclusionTexture(TextureHandle(strAoPath));
			}

			if (iEmissiveTexture >= 0)
			{
				DecodedImage xEmissiveImage;
				if (DecodeTextureImage(xDoc, iEmissiveTexture, xEmissiveImage) && xEmissiveImage.IsValid())
				{
					const std::string strEmissivePath = strBaseName + strSuffix + "_emissive.ztxtr";
					Zenith_Tools_TextureExport::ExportFromDataCompressed(
						xEmissiveImage.m_xRGBA.data(), strEmissivePath,
						xEmissiveImage.m_iWidth, xEmissiveImage.m_iHeight, TextureCompressionMode::BC1, TextureColourSpace::SRGB);
					pxMaterialAsset->SetEmissiveTexture(TextureHandle(strEmissivePath));
					++uTexturesOut;
				}
			}

			pxMaterialAsset->SetBaseColor(xBaseColor);
			pxMaterialAsset->SetMetallic(fMetallic);
			pxMaterialAsset->SetRoughness(fRoughness);
			pxMaterialAsset->SetNormalStrength(fNormalScale);
			pxMaterialAsset->SetOcclusionStrength(fOcclusionStrength);
			pxMaterialAsset->SetEmissiveColor(xEmissive);
			pxMaterialAsset->SetTwoSided(bTwoSided);
			pxMaterialAsset->SetBlendMode(eBlendMode);
			pxMaterialAsset->SetAlphaCutoff(fAlphaCutoff);
			pxMaterialAsset->SaveToFile(strMaterialPath);
		}

		xRefsOut.PushBack(Zenith_AssetRegistry::NormalizeAssetPath(strMaterialPath));
	}

	return true;
}

GlbImportResult ImportGlbFile(const std::string& strGlbPath)
{
	GlbImportResult xResult;
	Zenith_MeshAsset xMesh;
	if (!LoadGlbMesh(strGlbPath, xMesh, xResult))
	{
		return xResult;
	}

	const std::filesystem::path xPath(strGlbPath);
	const std::string strBaseName = (xPath.parent_path() / xPath.stem()).string();
	const std::string strModelName = xPath.stem().string();

	// ★ TANGENTS ARE ALWAYS REGENERATED. glTF may carry a TANGENT attribute, but
	// gltfpack strips it whenever the normal map can be reconstructed from UVs,
	// and a mesh with no tangents renders its normal map as noise. Deriving them
	// from positions + UVs matches what the Assimp path asks for with
	// aiProcess_CalcTangentSpace, so both importers agree.
	xMesh.GenerateTangents();
	xMesh.ComputeBounds();

	const std::string strMeshPath = strBaseName + ".zmesh";
	{
		std::error_code xEc;
		std::filesystem::create_directories(xPath.parent_path(), xEc);
	}
	xMesh.Export(strMeshPath.c_str());

	Zenith_Vector<std::string> xMaterialRefs;
	if (!ExportGlbMaterials(strGlbPath, strBaseName, xMaterialRefs, xResult.m_uNumTexturesWritten))
	{
		return xResult;
	}

	// ---- The .zmodel --------------------------------------------------------
	const std::string strModelPath = strBaseName + ".zmodel";
	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModelHandle =
			Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModelHandle.GetDirect();
		pxModel->SetName(strModelName);
		// STATIC: no skeleton, no animations. A rigged .glb would need the skin
		// path wiring that Zenith_Tools_MeshExport already owns for Assimp, and
		// silently dropping a rig is worse than not claiming to support one.
		pxModel->AddMeshByPath(strMeshPath, xMaterialRefs);
		pxModel->Export(strModelPath.c_str());
	}

	std::error_code xEc;
	if (!std::filesystem::exists(std::filesystem::path(strMeshPath), xEc) ||
		!std::filesystem::exists(std::filesystem::path(strModelPath), xEc))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "GLB_IMPORT: %s wrote no mesh/model to disk", strGlbPath.c_str());
		return xResult;
	}

	const Zenith_Maths::Vector3& xMin = xMesh.GetBoundsMin();
	const Zenith_Maths::Vector3& xMax = xMesh.GetBoundsMax();
	xResult.m_afBoundsMin[0] = xMin.x; xResult.m_afBoundsMin[1] = xMin.y; xResult.m_afBoundsMin[2] = xMin.z;
	xResult.m_afBoundsMax[0] = xMax.x; xResult.m_afBoundsMax[1] = xMax.y; xResult.m_afBoundsMax[2] = xMax.z;
	xResult.m_uNumVerts = xMesh.GetNumVerts();
	xResult.m_uNumIndices = xMesh.GetNumIndices();
	xResult.m_uNumSubmeshes = xMesh.GetNumSubmeshes();
	xResult.m_bSuccess = true;

	Zenith_Log(LOG_CATEGORY_TOOLS,
		"GLB_IMPORT: %s -> %u verts, %u indices, %u submesh(es), %u texture(s); "
		"size %.4f x %.4f x %.4f m",
		strGlbPath.c_str(), xResult.m_uNumVerts, xResult.m_uNumIndices,
		xResult.m_uNumSubmeshes, xResult.m_uNumTexturesWritten,
		xMax.x - xMin.x, xMax.y - xMin.y, xMax.z - xMin.z);

	return xResult;
}

void ImportGlbsInDirectory(const std::string& strDirectory)
{
	// The assets tree is gitignored, so a fresh clone (notably CI) legitimately
	// has none. recursive_directory_iterator THROWS on a missing path, and an
	// unhandled throw here would kill the tools boot before any export ran.
	std::error_code xEc;
	if (!std::filesystem::exists(strDirectory, xEc) || xEc)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "GLB_IMPORT: source dir absent, skipping: %s", strDirectory.c_str());
		return;
	}

	for (const auto& xEntry : std::filesystem::recursive_directory_iterator(strDirectory, xEc))
	{
		if (!xEntry.is_regular_file(xEc))
		{
			continue;
		}
		const std::filesystem::path& xPath = xEntry.path();
		if (xPath.extension() != ".glb")
		{
			continue;
		}

		// ★ A HUMANOID BELONGS TO Zenith_Tools_HumanModelExport, and the routing is
		// its to state. See IsHumanoidSourcePath for the double-import hazard this
		// closes; it replaced a committed .zbind sidecar whose only surviving job
		// was to say the same thing one file at a time.
		if (Zenith_Tools_HumanModelExport::IsHumanoidSourcePath(xPath.string()))
		{
			Zenith_Log(LOG_CATEGORY_TOOLS,
				"GLB_IMPORT: %s is a humanoid source - the human binder owns this model, skipping",
				xPath.string().c_str());
			continue;
		}

		(void)ImportGlbFile(xPath.string());
	}
}

} // namespace Zenith_Tools_GlbImport
