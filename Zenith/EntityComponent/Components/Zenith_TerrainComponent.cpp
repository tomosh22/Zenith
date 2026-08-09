#include "Zenith.h"
#include "Flux/Flux_RendererImpl.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Core/Zenith_BakedMeshReader.h"
#include "Maths/Zenith_FrustumCulling.h"
// Wave 3 PART B: terrain render-record gather (so Flux_Terrain drops Zenith_TerrainComponent.h).
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

// The Flux GPU state these methods operate on now lives on the owning
// Flux_TerrainStreamingState (Wave-18 relocation). Accessing
// m_pxStreamingState->m_x... throughout is the O(1) pointer hop the component
// already paid for the streaming state; no map lookup was added.
//
// NOTE: the full Flux_MeshGeometry type (used by LoadCombinedPhysicsGeometry /
// LoadAndCombineLowLODChunks / InitializeUnifiedBuffers) is pulled in
// transitively by Flux/Flux_GraphicsImpl.h above (it owns Flux_MeshGeometry
// members by value). Deliberately NOT adding a direct
// #include "Flux/MeshGeometry/Flux_MeshGeometry.h" here: that would introduce a
// new Zenith_TerrainComponent.cpp => Flux/MeshGeometry edge the layering gate
// would (correctly) flag. The .cpp's existing allow-listed Flux dependencies
// already carry the type.
//
// The same applies to Flux_TerrainVertexQuant's packed-position accessors, which
// the chunk validator below needs to decode a quantised vertex: they arrive with
// Flux/Terrain/Flux_TerrainStreamingManagerImpl.h (which includes them because its
// chunk-vertex hook contract hands out those very bytes). A direct include here
// would be a new EntityComponent => Flux edge, and the ratchet's rule is that a new
// one is redesigned, never allow-listed.

// LOD distance thresholds from unified config (distance squared)
// Used for debug visualization - actual thresholds are in Flux_TerrainConfig.h

// Static instance counter for terrain components - used to manage streaming manager lifecycle
uint32_t Zenith_TerrainComponent::s_uInstanceCount = 0;

namespace
{
	struct TerrainChunkSourceSnapshot
	{
		Zenith_Vector<u_int8> m_auVertexData;
		Zenith_Vector<uint32_t> m_auIndices;
		Zenith_Vector<Zenith_Maths::Vector3> m_axPositions;
		Zenith_Vector<Zenith_Maths::Vector3> m_axNormals;
		Zenith_Maths::Vector4 m_xMaterialColor = Zenith_Maths::Vector4(1.0f);
		uint32_t m_uVertexCount = 0;
		uint32_t m_uIndexCount = 0;
	};


	// Grid geometry derived once by ValidateGridDimensions and threaded through
	// the two later stages, so none of them re-derives it (and none of them can
	// derive it differently).
	struct TerrainGridExtents
	{
		uint32_t m_uVerticesPerEdge = 0;
		uint32_t m_uQuadsPerEdge = 0;
		float m_fMinX = 0.0f;
		float m_fMinZ = 0.0f;
		float m_fSpacingX = 0.0f;
		float m_fSpacingZ = 0.0f;
	};

	static constexpr float fGRID_EPSILON = 1.0e-3f;

	// STAGE 1 - counts, buffer sizes, and the world-space grid extents.
	// Rejects anything whose declared counts disagree with a square grid, whose
	// streams are the wrong length, or whose positions are non-finite/degenerate.
	bool ValidateGridDimensions(const TerrainChunkSourceSnapshot& xSnapshot, bool bRequireNormals,
		TerrainGridExtents& xExtentsOut)
	{
		uint32_t uVerticesPerEdge = 1u;
		while (static_cast<uint64_t>(uVerticesPerEdge) * uVerticesPerEdge < xSnapshot.m_uVertexCount)
			uVerticesPerEdge++;
		if (uVerticesPerEdge < 2u || uVerticesPerEdge * uVerticesPerEdge != xSnapshot.m_uVertexCount)
			return false;

		const uint32_t uQuadsPerEdge = uVerticesPerEdge - 1u;
		if (xSnapshot.m_uIndexCount != uQuadsPerEdge * uQuadsPerEdge * 6u ||
			xSnapshot.m_axPositions.GetSize() != xSnapshot.m_uVertexCount ||
			static_cast<size_t>(xSnapshot.m_auVertexData.GetSize()) != static_cast<size_t>(xSnapshot.m_uVertexCount) * Zenith_TerrainChunkLayout::uVERTEX_STRIDE ||
			(bRequireNormals && xSnapshot.m_axNormals.GetSize() != xSnapshot.m_uVertexCount))
		{
			return false;
		}

		float fMinX = (std::numeric_limits<float>::max)();
		float fMaxX = (std::numeric_limits<float>::lowest)();
		float fMinZ = (std::numeric_limits<float>::max)();
		float fMaxZ = (std::numeric_limits<float>::lowest)();
		for (const Zenith_Maths::Vector3& xPosition : xSnapshot.m_axPositions)
		{
			if (!std::isfinite(xPosition.x) || !std::isfinite(xPosition.y) || !std::isfinite(xPosition.z))
				return false;
			fMinX = std::min(fMinX, xPosition.x);
			fMaxX = std::max(fMaxX, xPosition.x);
			fMinZ = std::min(fMinZ, xPosition.z);
			fMaxZ = std::max(fMaxZ, xPosition.z);
		}

		const float fSpacingX = (fMaxX - fMinX) / static_cast<float>(uQuadsPerEdge);
		const float fSpacingZ = (fMaxZ - fMinZ) / static_cast<float>(uQuadsPerEdge);
		if (!std::isfinite(fSpacingX) || !std::isfinite(fSpacingZ) || fSpacingX <= 1.0e-6f || fSpacingZ <= 1.0e-6f)
			return false;

		xExtentsOut.m_uVerticesPerEdge = uVerticesPerEdge;
		xExtentsOut.m_uQuadsPerEdge = uQuadsPerEdge;
		xExtentsOut.m_fMinX = fMinX;
		xExtentsOut.m_fMinZ = fMinZ;
		xExtentsOut.m_fSpacingX = fSpacingX;
		xExtentsOut.m_fSpacingZ = fSpacingZ;
		return true;
	}

	// STAGE 2 - every vertex lands on exactly one distinct grid slot, its packed
	// vertex-buffer position agrees with the position stream, and (when present)
	// its normal is finite and non-degenerate. Fills the per-vertex grid
	// coordinates stage 3 needs.
	bool ValidateVertexGrid(const TerrainChunkSourceSnapshot& xSnapshot, const TerrainGridExtents& xExtents,
		Zenith_Vector<uint32_t>& auVertexGridXOut, Zenith_Vector<uint32_t>& auVertexGridZOut)
	{
		auVertexGridXOut.Resize(xSnapshot.m_uVertexCount, 0u);
		auVertexGridZOut.Resize(xSnapshot.m_uVertexCount, 0u);
		Zenith_Vector<int32_t> aiGridOwners(xSnapshot.m_uVertexCount);
		aiGridOwners.Resize(xSnapshot.m_uVertexCount, -1);
		const Flux_PosQuant xPositionQuant = Flux_MakeTerrainPosQuant();

		for (uint32_t u = 0u; u < xSnapshot.m_uVertexCount; ++u)
		{
			const Zenith_Maths::Vector3& xPosition = xSnapshot.m_axPositions.Get(u);
			const float fGridX = (xPosition.x - xExtents.m_fMinX) / xExtents.m_fSpacingX;
			const float fGridZ = (xPosition.z - xExtents.m_fMinZ) / xExtents.m_fSpacingZ;
			const int32_t iGridX = static_cast<int32_t>(std::round(fGridX));
			const int32_t iGridZ = static_cast<int32_t>(std::round(fGridZ));
			if (iGridX < 0 || iGridZ < 0 || iGridX >= static_cast<int32_t>(xExtents.m_uVerticesPerEdge) ||
				iGridZ >= static_cast<int32_t>(xExtents.m_uVerticesPerEdge) ||
				std::fabs(xPosition.x - (xExtents.m_fMinX + iGridX * xExtents.m_fSpacingX)) > fGRID_EPSILON ||
				std::fabs(xPosition.z - (xExtents.m_fMinZ + iGridZ * xExtents.m_fSpacingZ)) > fGRID_EPSILON)
			{
				return false;
			}

			const uint32_t uSlot = static_cast<uint32_t>(iGridZ) * xExtents.m_uVerticesPerEdge + static_cast<uint32_t>(iGridX);
			if (aiGridOwners.Get(uSlot) != -1)
				return false;
			aiGridOwners.Get(uSlot) = static_cast<int32_t>(u);
			auVertexGridXOut.Get(u) = static_cast<uint32_t>(iGridX);
			auVertexGridZOut.Get(u) = static_cast<uint32_t>(iGridZ);

			// Cross-stream agreement: the packed vertex buffer the GPU fetches must
			// describe the same vertex as the position stream physics and the AABBs
			// are built from. The packed lane is QUANTISED, so the comparison is
			// against one quantisation step per axis, not the grid epsilon — the two
			// streams cannot be equal, only consistent. (There is no finiteness check
			// on the decoded value any more: a snorm16 lane decodes through a constant
			// scale and bias, so it is finite by construction. Non-finite authored
			// positions are still rejected, one stage earlier, on the stream itself.)
			const Zenith_Maths::Vector3 xPacked = Flux_ReadTerrainVertexPosition(
				xSnapshot.m_auVertexData.GetDataPointer() + static_cast<size_t>(u) * Zenith_TerrainChunkLayout::uVERTEX_STRIDE,
				xPositionQuant);
			if (std::fabs(xPacked.x - xPosition.x) > Zenith_TerrainChunkLayout::PositionQuantStep(0u) ||
				std::fabs(xPacked.y - xPosition.y) > Zenith_TerrainChunkLayout::PositionQuantStep(1u) ||
				std::fabs(xPacked.z - xPosition.z) > Zenith_TerrainChunkLayout::PositionQuantStep(2u))
			{
				return false;
			}
		}

		if (xSnapshot.m_axNormals.GetSize() != 0u)
		{
			if (xSnapshot.m_axNormals.GetSize() != xSnapshot.m_uVertexCount)
				return false;
			for (const Zenith_Maths::Vector3& xNormal : xSnapshot.m_axNormals)
			{
				const float fLengthSq = xNormal.x * xNormal.x + xNormal.y * xNormal.y + xNormal.z * xNormal.z;
				if (!std::isfinite(xNormal.x) || !std::isfinite(xNormal.y) || !std::isfinite(xNormal.z) ||
					!std::isfinite(fLengthSq) || fLengthSq <= 1.0e-6f)
				{
					return false;
				}
			}
		}
		return true;
	}

	// The corner bit a vertex contributes to its cell's 4-bit occupancy mask.
	uint8_t TerrainCellCornerBit(const Zenith_Vector<uint32_t>& auVertexGridX,
		const Zenith_Vector<uint32_t>& auVertexGridZ,
		uint32_t uVertexIndex, uint32_t uMinGridX, uint32_t uMinGridZ)
	{
		const uint32_t uLocalX = auVertexGridX.Get(uVertexIndex) - uMinGridX;
		const uint32_t uLocalZ = auVertexGridZ.Get(uVertexIndex) - uMinGridZ;
		return static_cast<uint8_t>(1u << (uLocalZ * 2u + uLocalX));
	}

	// One triangle: in-range distinct indices, confined to a single grid cell,
	// clockwise in XZ, and split along the diagonal the exporter emits for that
	// cell. Records its corner mask into the per-cell tally.
	bool ValidateTerrainTriangle(const TerrainChunkSourceSnapshot& xSnapshot, const TerrainGridExtents& xExtents,
		const Zenith_Vector<uint32_t>& auVertexGridX, const Zenith_Vector<uint32_t>& auVertexGridZ,
		uint32_t uA, uint32_t uB, uint32_t uC,
		Zenith_Vector<uint8_t>& auTriangleCounts, Zenith_Vector<uint8_t>& auFirstMasks,
		Zenith_Vector<uint8_t>& auSecondMasks)
	{
		if (uA >= xSnapshot.m_uVertexCount || uB >= xSnapshot.m_uVertexCount || uC >= xSnapshot.m_uVertexCount ||
			uA == uB || uA == uC || uB == uC)
		{
			return false;
		}

		const uint32_t uMinGridX = std::min({ auVertexGridX.Get(uA), auVertexGridX.Get(uB), auVertexGridX.Get(uC) });
		const uint32_t uMaxGridX = std::max({ auVertexGridX.Get(uA), auVertexGridX.Get(uB), auVertexGridX.Get(uC) });
		const uint32_t uMinGridZ = std::min({ auVertexGridZ.Get(uA), auVertexGridZ.Get(uB), auVertexGridZ.Get(uC) });
		const uint32_t uMaxGridZ = std::max({ auVertexGridZ.Get(uA), auVertexGridZ.Get(uB), auVertexGridZ.Get(uC) });
		if (uMaxGridX != uMinGridX + 1u || uMaxGridZ != uMinGridZ + 1u)
			return false;

		const Zenith_Maths::Vector3& xA = xSnapshot.m_axPositions.Get(uA);
		const Zenith_Maths::Vector3& xB = xSnapshot.m_axPositions.Get(uB);
		const Zenith_Maths::Vector3& xC = xSnapshot.m_axPositions.Get(uC);
		const float fProjectedTwiceArea = (xB.x - xA.x) * (xC.z - xA.z) - (xB.z - xA.z) * (xC.x - xA.x);
		// The terrain exporter emits clockwise XZ winding (a,c,b / c,a,d).
		// Reject a reversed or degenerate triangle rather than accepting a
		// topologically complete grid whose physics/render faces disagree.
		if (!std::isfinite(fProjectedTwiceArea) || fProjectedTwiceArea >= -1.0e-6f)
			return false;

		const uint8_t uMask = static_cast<uint8_t>(
			TerrainCellCornerBit(auVertexGridX, auVertexGridZ, uA, uMinGridX, uMinGridZ) |
			TerrainCellCornerBit(auVertexGridX, auVertexGridZ, uB, uMinGridX, uMinGridZ) |
			TerrainCellCornerBit(auVertexGridX, auVertexGridZ, uC, uMinGridX, uMinGridZ));
		// ExportChunkBatch writes the N x N interior first, then stitches the
		// positive-X and positive-Z edge vertices. Interior cells use the
		// a-c split (0xB/0xD); either stitched positive edge uses the opposite
		// diagonal (0x7/0xE). Both retain the same clockwise winding.
		const bool bPositiveEdgeCell =
			uMinGridX + 1u == xExtents.m_uQuadsPerEdge ||
			uMinGridZ + 1u == xExtents.m_uQuadsPerEdge;
		const bool bExpectedTriangleMask = bPositiveEdgeCell
			? (uMask == 0x7u || uMask == 0xEu)
			: (uMask == 0xBu || uMask == 0xDu);
		if (!bExpectedTriangleMask)
			return false;

		const uint32_t uCell = uMinGridZ * xExtents.m_uQuadsPerEdge + uMinGridX;
		if (auTriangleCounts.Get(uCell) == 0u)
			auFirstMasks.Get(uCell) = uMask;
		else if (auTriangleCounts.Get(uCell) == 1u)
			auSecondMasks.Get(uCell) = uMask;
		else
			return false;
		auTriangleCounts.Get(uCell)++;
		return true;
	}

	// STAGE 3 - every cell is covered by exactly two triangles split along the
	// exporter's diagonal for that cell.
	bool ValidateIndexTopology(const TerrainChunkSourceSnapshot& xSnapshot, const TerrainGridExtents& xExtents,
		const Zenith_Vector<uint32_t>& auVertexGridX, const Zenith_Vector<uint32_t>& auVertexGridZ)
	{
		const uint32_t uQuadsPerEdge = xExtents.m_uQuadsPerEdge;
		const uint32_t uCellCount = uQuadsPerEdge * uQuadsPerEdge;
		Zenith_Vector<uint8_t> auTriangleCounts(uCellCount);
		Zenith_Vector<uint8_t> auFirstMasks(uCellCount);
		Zenith_Vector<uint8_t> auSecondMasks(uCellCount);
		auTriangleCounts.Resize(uCellCount, 0u);
		auFirstMasks.Resize(uCellCount, 0u);
		auSecondMasks.Resize(uCellCount, 0u);

		for (uint32_t uTriangle = 0u; uTriangle < xSnapshot.m_uIndexCount; uTriangle += 3u)
		{
			if (!ValidateTerrainTriangle(xSnapshot, xExtents, auVertexGridX, auVertexGridZ,
				xSnapshot.m_auIndices.Get(uTriangle),
				xSnapshot.m_auIndices.Get(uTriangle + 1u),
				xSnapshot.m_auIndices.Get(uTriangle + 2u),
				auTriangleCounts, auFirstMasks, auSecondMasks))
			{
				return false;
			}
		}

		for (uint32_t uCell = 0u; uCell < uCellCount; ++uCell)
		{
			if (auTriangleCounts.Get(uCell) != 2u)
				return false;
			const uint8_t uMissingA = static_cast<uint8_t>((~auFirstMasks.Get(uCell)) & 0xFu);
			const uint8_t uMissingB = static_cast<uint8_t>((~auSecondMasks.Get(uCell)) & 0xFu);
			const uint32_t uCellX = uCell % uQuadsPerEdge;
			const uint32_t uCellZ = uCell / uQuadsPerEdge;
			const bool bPositiveEdgeCell =
				uCellX + 1u == uQuadsPerEdge ||
				uCellZ + 1u == uQuadsPerEdge;
			const bool bExpectedDiagonal = bPositiveEdgeCell
				? ((uMissingA == 0x8u && uMissingB == 0x1u) ||
					(uMissingA == 0x1u && uMissingB == 0x8u))
				: ((uMissingA == 0x4u && uMissingB == 0x2u) ||
					(uMissingA == 0x2u && uMissingB == 0x4u));
			if (!bExpectedDiagonal)
				return false;
		}
		return true;
	}

	// The three stages run in order and each depends on the previous one having
	// passed: stage 2 needs the extents stage 1 derives, stage 3 needs the
	// per-vertex grid coordinates stage 2 fills. Rejection ORDER is therefore
	// unchanged from the single function this replaced, which matters - the
	// Terrain suite's fixture mutations each pin one specific rejection.
	bool ValidateTerrainGridTopology(const TerrainChunkSourceSnapshot& xSnapshot, bool bRequireNormals)
	{
		TerrainGridExtents xExtents;
		if (!ValidateGridDimensions(xSnapshot, bRequireNormals, xExtents))
			return false;

		Zenith_Vector<uint32_t> auVertexGridX(xSnapshot.m_uVertexCount);
		Zenith_Vector<uint32_t> auVertexGridZ(xSnapshot.m_uVertexCount);
		if (!ValidateVertexGrid(xSnapshot, xExtents, auVertexGridX, auVertexGridZ))
			return false;

		return ValidateIndexTopology(xSnapshot, xExtents, auVertexGridX, auVertexGridZ);
	}

	bool TryReadTerrainChunkSnapshot(const std::string& strPath, uint32_t uExpectedVertexCount,
		uint32_t uExpectedIndexCount, bool bRequireNormals, TerrainChunkSourceSnapshot& xSnapshotOut)
	{
		Zenith_BakedMeshReader xReader(strPath.c_str());
		uint32_t uElementCount = 0;
		if (!xReader.Read(uElementCount) || uElementCount != Zenith_TerrainChunkLayout::uELEMENT_COUNT)
			return false;

		uint32_t uExpectedOffset = 0;
		for (uint32_t u = 0; u < uElementCount; ++u)
		{
			Flux_BufferElement xElement;
			const Zenith_TerrainChunkLayout::Element& xExpected = Zenith_TerrainChunkLayout::axELEMENTS[u];
			if (!xReader.Read(xElement) || xElement.m_eType != xExpected.m_eType ||
				xElement.m_uSize != xExpected.m_uSize || xElement.m_uOffset != uExpectedOffset)
			{
				return false;
			}
			uExpectedOffset += xElement.m_uSize;
		}

		uint32_t uBoneCount = 0;
		uint32_t uBoneMapCount = 0;
		if (!xReader.Read(xSnapshotOut.m_uVertexCount) || !xReader.Read(xSnapshotOut.m_uIndexCount) ||
			!xReader.Read(uBoneCount) || !xReader.Read(uBoneMapCount) ||
			xSnapshotOut.m_uVertexCount != uExpectedVertexCount || xSnapshotOut.m_uIndexCount != uExpectedIndexCount ||
			uBoneCount != 0u || uBoneMapCount != 0u)
		{
			return false;
		}

		if (!xReader.Read(xSnapshotOut.m_xMaterialColor))
			return false;

		const uint64_t ulVertexDataSize = static_cast<uint64_t>(xSnapshotOut.m_uVertexCount) * Zenith_TerrainChunkLayout::uVERTEX_STRIDE;
		const uint64_t ulIndexDataSize = static_cast<uint64_t>(xSnapshotOut.m_uIndexCount) * sizeof(uint32_t);
		static constexpr uint64_t ulATTRIBUTE_FLAG_COUNT = 9u;
		if (!xReader.HasRemaining(ulVertexDataSize + ulIndexDataSize + ulATTRIBUTE_FLAG_COUNT))
			return false;

		xSnapshotOut.m_auVertexData.Resize(static_cast<u_int>(ulVertexDataSize), 0u);
		xSnapshotOut.m_auIndices.Resize(xSnapshotOut.m_uIndexCount, 0u);
		xSnapshotOut.m_axPositions.Resize(xSnapshotOut.m_uVertexCount, Zenith_Maths::Vector3(0.0f));
		xSnapshotOut.m_axNormals.Resize(xSnapshotOut.m_uVertexCount, Zenith_Maths::Vector3(0.0f));
		if (!xReader.ReadAttribute(ulVertexDataSize, true, xSnapshotOut.m_auVertexData.GetDataPointer()))
			return false;
		if (!xReader.ReadAttribute(ulIndexDataSize, true, xSnapshotOut.m_auIndices.GetDataPointer()))
			return false;

		const uint64_t ulVector3DataSize = static_cast<uint64_t>(xSnapshotOut.m_uVertexCount) * sizeof(Zenith_Maths::Vector3);
		const uint64_t ulVector4DataSize = static_cast<uint64_t>(xSnapshotOut.m_uVertexCount) * sizeof(Zenith_Maths::Vector4);
		const uint64_t ulBoneAttributeSize = static_cast<uint64_t>(xSnapshotOut.m_uVertexCount) * MAX_BONES_PER_VERTEX * sizeof(uint32_t);
		bool bNormalsPresent = false;
		const bool bDecoded = xReader.ReadAttribute(ulVector3DataSize, true, xSnapshotOut.m_axPositions.GetDataPointer()) &&
			xReader.ReadAttribute(ulVector3DataSize, bRequireNormals, xSnapshotOut.m_axNormals.GetDataPointer(), &bNormalsPresent) &&
			xReader.ReadAttribute(ulVector3DataSize, false) &&           // tangents
			xReader.ReadAttribute(ulVector3DataSize, false) &&           // bitangents
			xReader.ReadAttribute(ulVector4DataSize, false) &&           // colours
			xReader.ReadAttribute(ulBoneAttributeSize, false) &&         // bone IDs
			xReader.ReadAttribute(ulBoneAttributeSize, false) &&         // bone weights
			xReader.IsAtEnd();
		if (!bDecoded)
			return false;
		if (!bNormalsPresent)
			xSnapshotOut.m_axNormals.Clear();
		return ValidateTerrainGridTopology(xSnapshotOut, bRequireNormals);
	}
}

bool Zenith_TerrainComponent::TryLoadTerrainChunkSource(const std::string& strPath,
	uint32_t uExpectedVertexCount, uint32_t uExpectedIndexCount, bool bRequireNormals,
	Flux_MeshGeometry& xGeometryOut)
{
	TerrainChunkSourceSnapshot xSnapshot;
	if (!TryReadTerrainChunkSnapshot(strPath, uExpectedVertexCount, uExpectedIndexCount, bRequireNormals, xSnapshot))
		return false;

	const uint64_t ulVertexDataSize = xSnapshot.m_auVertexData.GetSize();
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(xSnapshot.m_uIndexCount) * sizeof(uint32_t);
	const uint64_t ulPositionDataSize = static_cast<uint64_t>(xSnapshot.m_uVertexCount) * sizeof(Zenith_Maths::Vector3);
	u_int8* pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(ulVertexDataSize));
	Flux_MeshGeometry::IndexType* puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(ulIndexDataSize));
	Zenith_Maths::Vector3* pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(ulPositionDataSize));
	Zenith_Maths::Vector3* pxNormals = bRequireNormals
		? static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(ulPositionDataSize)) : nullptr;
	if (pVertexData == nullptr || puIndices == nullptr || pxPositions == nullptr || (bRequireNormals && pxNormals == nullptr))
	{
		if (pVertexData != nullptr) Zenith_MemoryManagement::Deallocate(pVertexData);
		if (puIndices != nullptr) Zenith_MemoryManagement::Deallocate(puIndices);
		if (pxPositions != nullptr) Zenith_MemoryManagement::Deallocate(pxPositions);
		if (pxNormals != nullptr) Zenith_MemoryManagement::Deallocate(pxNormals);
		return false;
	}

	std::memcpy(pVertexData, xSnapshot.m_auVertexData.GetDataPointer(), static_cast<size_t>(ulVertexDataSize));
	std::memcpy(puIndices, xSnapshot.m_auIndices.GetDataPointer(), static_cast<size_t>(ulIndexDataSize));
	std::memcpy(pxPositions, xSnapshot.m_axPositions.GetDataPointer(), static_cast<size_t>(ulPositionDataSize));
	if (bRequireNormals)
		std::memcpy(pxNormals, xSnapshot.m_axNormals.GetDataPointer(), static_cast<size_t>(ulPositionDataSize));

	for (uint32_t u = 0u; u < Zenith_TerrainChunkLayout::uELEMENT_COUNT; ++u)
		xGeometryOut.m_xBufferLayout.GetElements().PushBack(Flux_BufferElement(Zenith_TerrainChunkLayout::axELEMENTS[u].m_eType));
	xGeometryOut.m_xBufferLayout.CalculateOffsetsAndStrides();
	xGeometryOut.m_uNumVerts = xSnapshot.m_uVertexCount;
	xGeometryOut.m_uNumIndices = xSnapshot.m_uIndexCount;
	xGeometryOut.m_uNumBones = 0u;
	xGeometryOut.m_pVertexData = pVertexData;
	xGeometryOut.m_puIndices = puIndices;
	xGeometryOut.m_pxPositions = pxPositions;
	xGeometryOut.m_pxNormals = pxNormals;
	xGeometryOut.m_xMaterialColor = xSnapshot.m_xMaterialColor;
	xGeometryOut.m_strSourcePath = strPath;
	xGeometryOut.m_ulReservedVertexDataSize = ulVertexDataSize;
	xGeometryOut.m_ulReservedIndexDataSize = ulIndexDataSize;
	xGeometryOut.m_ulReservedPositionDataSize = ulPositionDataSize;
	return true;
}

void Zenith_TerrainComponent::IncrementInstanceCount()
{
	s_uInstanceCount++;
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent instance count: %u (incremented)", s_uInstanceCount);
}

void Zenith_TerrainComponent::DecrementInstanceCount()
{
	if (s_uInstanceCount > 0)
	{
		s_uInstanceCount--;
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent instance count: %u (decremented)", s_uInstanceCount);
	}
	// Note: the previous "shut down the streaming manager when last terrain
	// is destroyed" side effect was removed when state ownership moved onto
	// the component. The manager no longer owns any per-terrain state, so
	// there is nothing to free here — each component frees its own state in
	// the destructor. The instance counter stays as a debug aid.
}

// Default constructor for deserialization. Out-of-line so the per-terrain
// Flux_TerrainStreamingState (forward-declared in the component header) can
// be allocated here with the full type visible from the manager header.
Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
	, m_pxPhysicsGeometry(nullptr)
{
	IncrementInstanceCount();

	// Each terrain owns its own streaming state. Initialise() pre-allocates
	// the cached chunk-data scratch buffer and resets per-frame counters;
	// the allocators get sized later in RegisterTerrainBuffers (it has the
	// vertex stride needed to derive vertex-count budgets). The relocated GPU
	// state (buffers + scalars + m_bCullingResourcesInitialized) default-inits
	// inside Flux_TerrainStreamingState.
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize();
	m_pxStreamingState->m_strTerrainAssetDirectory = GetTerrainAssetDirectory();
}

Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_MaterialAsset& xMaterial0, Zenith_MaterialAsset& xMaterial1, Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
	IncrementInstanceCount();

	// Allocate this terrain's own streaming state — same pattern as the
	// default (deserialization) constructor. The unified-buffer scalars and
	// the culling-init flag now live on (and default-init inside) the state.
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize();
	m_pxStreamingState->m_strTerrainAssetDirectory = GetTerrainAssetDirectory();

	// Store material handles (auto ref-counting)
	m_axMaterials[0].Set(&xMaterial0);
	m_axMaterials[1].Set(&xMaterial1);

	// Ensure all 4 material slots have valid assets (blank fallback for 2-3)
	for (u_int u = 2; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (!m_axMaterials[u].GetDirect())
		{
			auto xhBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxBlank = xhBlank.GetDirect();
			if (pxBlank)
			{
				pxBlank->SetName("Terrain_Mat" + std::to_string(u));
				m_axMaterials[u].Set(pxBlank);
			}
		}
	}

	// Ensure streaming manager is initialized — defensive, normally
	// g_xEngine.Terrain().Initialise() does this once at engine startup.
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		xTerrainStreaming.Initialize();
	}

#pragma region Render
{
	// Initialize render resources (LOW LOD meshes, unified buffers, culling)
	// This is the same initialization performed during deserialization
	InitializeRenderResources();
}
#pragma endregion

#pragma region Physics
{
	LoadCombinedPhysicsGeometry();
}
#pragma endregion
}

Zenith_TerrainComponent::~Zenith_TerrainComponent()
{
	// A moved-from component owns nothing — its state pointer was stolen and
	// nulled, and DecrementInstanceCount must NOT run (the moved-to component
	// is the live one, and IncrementInstanceCount fired once at construction).
	if (m_pxStreamingState == nullptr)
	{
		// Physics geometry was also stolen (nulled) by the move; delete is a
		// no-op on null but kept for symmetry / clarity.
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
		return;
	}

	DestroyCullingResources();

	// Take this terrain out of the manager's registry FIRST so no concurrent
	// PreRenderUpdate iteration can pick up a state that's about to be freed.
	g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);

	// Destroy the owned unified buffers (they now live ON the streaming state)
	// BEFORE freeing the state — preserves the documented destroy order:
	// DestroyCullingResources -> unregister -> destroy unified buffers ->
	// delete state.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Unified terrain buffers destroyed");

	m_pxStreamingState->Shutdown();
	delete m_pxStreamingState;
	m_pxStreamingState = nullptr;

	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// MaterialHandle members (m_axMaterials[]) auto-release when destroyed

	DecrementInstanceCount();
}

// ========== Move semantics (Wave-18) ==========
// STEAL the owned state + physics geometry + material/splat handles from the
// source, null the source's owning pointers, and REPOINT the streaming state's
// owner back-pointer at *this so the manager registry / per-frame resolver keep
// dereferencing the live component. See the header for why the implicit move
// (a shallow pointer copy → double-free on pool relocation) is wrong.
Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_TerrainComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxPhysicsGeometry(xOther.m_pxPhysicsGeometry)
	, m_bTerrainGeometryUnusable(xOther.m_bTerrainGeometryUnusable)
	, m_pxStreamingState(xOther.m_pxStreamingState)
	, m_strTerrainAssetSet(std::move(xOther.m_strTerrainAssetSet))
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
		m_axMaterials[u] = std::move(xOther.m_axMaterials[u]);
	m_xSplatmap = std::move(xOther.m_xSplatmap);
	// The source's already-synchronized state is stolen with its validated set,
	// so its cached directory remains component-local and unchanged.

	// Wave 3: the old "repoint m_pxStreamingState->m_pxOwner = this" is gone — the
	// Flux state no longer stores a Zenith_TerrainComponent back-pointer (it was never
	// dereferenced; only an identity/validation the streaming manager no longer needs).
	// The double-free fix is purely the state-steal + null-source below.

	// Null the source so its destructor frees nothing (no double-free).
	xOther.m_pxStreamingState = nullptr;
	xOther.m_pxPhysicsGeometry = nullptr;
}

Zenith_TerrainComponent& Zenith_TerrainComponent::operator=(Zenith_TerrainComponent&& xOther) noexcept
{
	if (this == &xOther)
		return *this;

	// Release anything this component currently owns before stealing. Mirrors
	// the destructor's order (cull -> unregister -> unified buffers -> state).
	if (m_pxStreamingState)
	{
		DestroyCullingResources();
		g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);
		auto& xVulkanMemory = g_xEngine.FluxMemory();
		xVulkanMemory.DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
		xVulkanMemory.DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);
		m_pxStreamingState->Shutdown();
		delete m_pxStreamingState;
		m_pxStreamingState = nullptr;
		// This component had its own instance-count slot; the source keeps its
		// own (transferred below). Balance the count for the state we just
		// destroyed so the net count is unchanged by the assignment.
		DecrementInstanceCount();
	}
	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// Steal from the source.
	m_xParentEntity            = xOther.m_xParentEntity;
	m_pxPhysicsGeometry        = xOther.m_pxPhysicsGeometry;
	m_bTerrainGeometryUnusable = xOther.m_bTerrainGeometryUnusable;
	m_pxStreamingState         = xOther.m_pxStreamingState;
	m_strTerrainAssetSet       = std::move(xOther.m_strTerrainAssetSet);
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
		m_axMaterials[u] = std::move(xOther.m_axMaterials[u]);
	m_xSplatmap = std::move(xOther.m_xSplatmap);
	// The state and validated set move as one ownership unit; no global path is
	// consulted and no other component's cached directory is touched.

	// Wave 3: no m_pxOwner back-pointer to repoint (see the move-ctor note).

	xOther.m_pxStreamingState  = nullptr;
	xOther.m_pxPhysicsGeometry = nullptr;

	return *this;
}

// ========== Out-of-line buffer / stride / draw-count forwarders ==========
// Each forwards into the owning Flux_TerrainStreamingState. The full state +
// buffer-wrapper types are visible here (Flux_TerrainStreamingManagerImpl.h);
// the header only forward-declares them.

const Flux_VertexBuffer& Zenith_TerrainComponent::GetUnifiedVertexBuffer() const
{
	return m_pxStreamingState->m_xUnifiedVertexBuffer;
}

const Flux_IndexBuffer& Zenith_TerrainComponent::GetUnifiedIndexBuffer() const
{
	return m_pxStreamingState->m_xUnifiedIndexBuffer;
}

uint32_t Zenith_TerrainComponent::GetVertexStride() const
{
	return m_pxStreamingState->m_uVertexStride;
}

const Flux_MeshGeometry& Zenith_TerrainComponent::GetPhysicsMeshGeometry() const
{
	return *m_pxPhysicsGeometry;
}

const Flux_IndirectBuffer& Zenith_TerrainComponent::GetIndirectDrawBuffer() const
{
	return m_pxStreamingState->m_xIndirectDrawBuffer;
}

const Flux_IndirectBuffer& Zenith_TerrainComponent::GetVisibleCountBuffer() const
{
	return m_pxStreamingState->m_xVisibleCountBuffer;
}

uint32_t Zenith_TerrainComponent::GetMaxDrawCount() const
{
	return Flux_TerrainConfig::TOTAL_CHUNKS;
}

Flux_ReadWriteBuffer& Zenith_TerrainComponent::GetLODLevelBuffer()
{
	return m_pxStreamingState->m_xLODLevelBuffer;
}

bool Zenith_TerrainComponent::IsValidTerrainAssetSetName(const std::string& strSet)
{
	if (strSet.empty())
	{
		return true;
	}
	if (strSet.size() > 64)
	{
		return false;
	}

	const auto IsASCIIAlphaNumeric = [](char cCharacter)
	{
		return (cCharacter >= 'A' && cCharacter <= 'Z') ||
			(cCharacter >= 'a' && cCharacter <= 'z') ||
			(cCharacter >= '0' && cCharacter <= '9');
	};

	if (!IsASCIIAlphaNumeric(strSet[0]))
	{
		return false;
	}
	for (size_t uIndex = 1; uIndex < strSet.size(); uIndex++)
	{
		const char cCharacter = strSet[uIndex];
		if (!IsASCIIAlphaNumeric(cCharacter) && cCharacter != '_' && cCharacter != '-')
		{
			return false;
		}
	}
	return true;
}

bool Zenith_TerrainComponent::TryResolveTerrainAssetDirectory(const std::string& strSet, std::string& strDirectoryOut)
{
	strDirectoryOut.clear();
	if (!IsValidTerrainAssetSetName(strSet))
	{
		return false;
	}

	const std::string strLegacyDirectory = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
	if (strSet.empty())
	{
		strDirectoryOut = strLegacyDirectory;
		return true;
	}

	const std::filesystem::path xTerrainRoot =
		(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").lexically_normal();
	const std::filesystem::path xResolvedDirectory = (xTerrainRoot / strSet).lexically_normal();

	// Compare path components, not string prefixes (Terrain2 is not beneath
	// Terrain). A named set must add at least one component below the root.
	auto xRootComponent = xTerrainRoot.begin();
	auto xResolvedComponent = xResolvedDirectory.begin();
	for (; xRootComponent != xTerrainRoot.end(); ++xRootComponent, ++xResolvedComponent)
	{
		if (xResolvedComponent == xResolvedDirectory.end() || *xRootComponent != *xResolvedComponent)
		{
			return false;
		}
	}
	if (xResolvedComponent == xResolvedDirectory.end())
	{
		return false;
	}

	strDirectoryOut = xResolvedDirectory.generic_string();
	while (!strDirectoryOut.empty() && strDirectoryOut.back() == '/')
	{
		strDirectoryOut.pop_back();
	}
	strDirectoryOut += '/';
	return true;
}

bool Zenith_TerrainComponent::SetTerrainAssetSet(const std::string& strSet)
{
	std::string strResolvedDirectory;
	if (!TryResolveTerrainAssetDirectory(strSet, strResolvedDirectory))
	{
		return false;
	}

	m_strTerrainAssetSet = strSet;
	if (m_pxStreamingState)
	{
		m_pxStreamingState->m_strTerrainAssetDirectory = std::move(strResolvedDirectory);
	}
	return true;
}

const std::string& Zenith_TerrainComponent::GetTerrainAssetSet() const
{
	return m_strTerrainAssetSet;
}

std::string Zenith_TerrainComponent::GetTerrainAssetDirectory() const
{
	std::string strDirectory;
	if (!TryResolveTerrainAssetDirectory(m_strTerrainAssetSet, strDirectory))
	{
		// Stored values can only arrive through the validating setter. Preserve a
		// safe legacy fallback even if memory corruption violates that invariant.
		strDirectory = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
		Zenith_Assert(false, "Zenith_TerrainComponent stored an invalid terrain asset-set name");
	}
	return strDirectory;
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Serialization version
	uint32_t uVersion = 4;
	xStream << uVersion;

	// Serialize the combined physics source path for reference; reconstruction
	// resolves per-chunk files from this component's terrain asset directory.
	std::string strPhysicsGeometryPath = m_pxPhysicsGeometry ? Zenith_AssetRegistry::NormalizeAssetPath(m_pxPhysicsGeometry->m_strSourcePath) : "";
	xStream << strPhysicsGeometryPath;

	// Version 3: Serialize 4 materials + splatmap path
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		Zenith_MaterialAsset* pxMat = m_axMaterials[u].GetDirect();
		if (pxMat)
		{
			pxMat->WriteToDataStream(xStream);
		}
		else
		{
			Zenith_MaterialAsset xEmptyMat;
			xEmptyMat.WriteToDataStream(xStream);
		}
	}

	// Splatmap path
	std::string strSplatmapPath = Zenith_AssetRegistry::NormalizeAssetPath(m_xSplatmap.GetPath());
	xStream << strSplatmapPath;

	// Version 4: append the terrain asset-set name after the complete v3 payload.
	xStream << m_strTerrainAssetSet;
}

// ReadFromDataStream helpers — keep the top-level function focused on the
// version dispatch by factoring the per-version material-read passes out.

void Zenith_TerrainComponent::AssignTerrainMaterialSlot(u_int uSlot, const std::string& strEntityName, Zenith_DataStream& xStream)
{
	Zenith_Assert(uSlot < TERRAIN_MATERIAL_COUNT, "Terrain material slot %u out of range", uSlot);
	auto xhNewMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	if (Zenith_MaterialAsset* pxNewMat = xhNewMat.GetDirect())
	{
		pxNewMat->SetName(strEntityName + "_Terrain_Mat" + std::to_string(uSlot));
		m_axMaterials[uSlot].Set(pxNewMat);
		pxNewMat->ReadFromDataStream(xStream);
	}
	else
	{
		// Registry couldn't allocate — still drain the stream so later data is aligned.
		Zenith_MaterialAsset xTempMat;
		xTempMat.ReadFromDataStream(xStream);
	}
}

void Zenith_TerrainComponent::ReadMaterialsV3(const std::string& strEntityName, Zenith_DataStream& xStream)
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		AssignTerrainMaterialSlot(u, strEntityName, xStream);
	}

	std::string strSplatmapPath;
	xStream >> strSplatmapPath;
	if (!strSplatmapPath.empty())
	{
		m_xSplatmap.SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strSplatmapPath));
	}
}

void Zenith_TerrainComponent::ReadMaterialsV2(const std::string& strEntityName, Zenith_DataStream& xStream)
{
	for (u_int u = 0; u < 2; u++)
	{
		AssignTerrainMaterialSlot(u, strEntityName, xStream);
	}
}

void Zenith_TerrainComponent::ReadMaterialsV1Legacy(Zenith_DataStream& xStream)
{
	// Legacy format stored two base colors and no material assets.
	// operator>> returns void, so each component gets its own statement.
	Zenith_Maths::Vector4 xMat0Color, xMat1Color;
	xStream >> xMat0Color.x;
	xStream >> xMat0Color.y;
	xStream >> xMat0Color.z;
	xStream >> xMat0Color.w;
	xStream >> xMat1Color.x;
	xStream >> xMat1Color.y;
	xStream >> xMat1Color.z;
	xStream >> xMat1Color.w;

	if (m_axMaterials[0].GetDirect()) m_axMaterials[0].GetDirect()->SetBaseColor(xMat0Color);
	if (m_axMaterials[1].GetDirect()) m_axMaterials[1].GetDirect()->SetBaseColor(xMat1Color);
}

void Zenith_TerrainComponent::BackfillMissingMaterialSlots(const std::string& strEntityName)
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (!m_axMaterials[u].GetDirect())
		{
			auto xhBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			if (Zenith_MaterialAsset* pxBlank = xhBlank.GetDirect())
			{
				pxBlank->SetName(strEntityName + "_Terrain_Mat" + std::to_string(u));
				m_axMaterials[u].Set(pxBlank);
			}
		}
	}
}

void Zenith_TerrainComponent::ReadSerializedFields(Zenith_DataStream& xStream)
{
	// Deserialization always begins from the safe legacy set. A rejected v4
	// candidate therefore cannot preserve a stale destination value.
	SetTerrainAssetSet("");

	uint32_t uVersion;
	xStream >> uVersion;

	std::string strPhysicsGeometryPath;
	xStream >> strPhysicsGeometryPath;  // Read-through; actual chunks loaded below.

	const std::string strEntityName = m_xParentEntity.GetName().empty()
		? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
		: m_xParentEntity.GetName();

	if      (uVersion >= 3) ReadMaterialsV3(strEntityName, xStream);
	else if (uVersion >= 2) ReadMaterialsV2(strEntityName, xStream);
	else                    ReadMaterialsV1Legacy(xStream);

	// Version 4 appends the set after the complete v3 payload. Versions 1-3
	// retain the explicit empty default installed at parser entry.
	if (uVersion >= 4)
	{
		std::string strTerrainAssetSet;
		xStream >> strTerrainAssetSet;
		if (!SetTerrainAssetSet(strTerrainAssetSet))
		{
			Zenith_Warning(LOG_CATEGORY_TERRAIN,
				"Terrain deserialization rejected an invalid terrain asset-set name; using legacy Terrain/.");
		}
	}

	BackfillMissingMaterialSlots(strEntityName);
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Streaming manager may have been shut down after the previous terrain
	// was destroyed — bring it back up before we touch chunks.
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ReadFromDataStream - Re-initializing streaming manager (was shut down)");
		xTerrainStreaming.Initialize();
	}

	// Parse and synchronize the terrain set before either loader resolves a
	// component-relative chunk path.
	ReadSerializedFields(xStream);
	LoadCombinedPhysicsGeometry();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Initializing render resources...");
	InitializeRenderResources();
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Render resources initialized successfully");
}

// ========== Render Resources Initialization ==========

void Zenith_TerrainComponent::InitializeRenderResources()
{
	// Reassert the component-owned directory at each initialization attempt so
	// streaming always consumes this terrain's current serialized set.
	m_pxStreamingState->m_strTerrainAssetDirectory = GetTerrainAssetDirectory();

	// Reset the unusable flag at the start of each load attempt so the
	// editor's regenerate/retry path can recover after the missing chunk
	// files are produced. Without this, a stale "unusable" set by a
	// previous failed load would short-circuit the retry even when the
	// new files load fine.
	m_bTerrainGeometryUnusable = false;

	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "InitializeRenderResources - Re-initializing streaming manager (was shut down)");
		xTerrainStreaming.Initialize();
	}

	// NOTE: Materials are stored in m_axMaterials[] handles by the caller
	// (constructor or ReadFromDataStream) before this method is invoked

	uint32_t uLowLODTotalVerts = 0;
	uint32_t uLowLODTotalIndices = 0;
	CalculateLowLODBufferSizes(uLowLODTotalVerts, uLowLODTotalIndices);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLowLODTotalVerts, (uLowLODTotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLowLODTotalIndices, (uLowLODTotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// Pre-allocate per-chunk init data so the streaming manager doesn't re-read
	// every chunk file just to learn its vertex/index counts and AABB.
	Flux_TerrainChunkInitData* pxChunkInitData = new Flux_TerrainChunkInitData[TOTAL_CHUNKS];
	Flux_MeshGeometry* pxLowLODGeometry = nullptr;
	LoadAndCombineLowLODChunks(uLowLODTotalVerts, uLowLODTotalIndices, pxChunkInitData, pxLowLODGeometry);

	// LoadAndCombineLowLODChunks flips m_bTerrainGeometryUnusable when
	// chunk (0,0) is missing — without it we have no canonical vertex
	// layout to size the unified buffer with. Bail out cleanly so the
	// component lands in the "renders nothing, no physics body" state
	// instead of crashing partway through buffer creation.
	if (m_bTerrainGeometryUnusable)
	{
		delete pxLowLODGeometry;
		delete[] pxChunkInitData;
		// Deserialization loads physics before render resources. If the LOW
		// authority is then found missing/invalid, discard that already-built
		// body so the anchor contract is independent of construction order.
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Zenith_TerrainComponent::InitializeRenderResources - skipping unified-buffer / streaming / culling init because terrain geometry is unusable");
		return;
	}

	InitializeUnifiedBuffers(*pxLowLODGeometry);

	delete pxLowLODGeometry;
	pxLowLODGeometry = nullptr;

	xTerrainStreaming.RegisterTerrainBuffers(m_pxStreamingState, pxChunkInitData);
	delete[] pxChunkInitData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain render geometry facade setup complete (references component-owned buffers)");

	InitializeCullingResources();
}

void Zenith_TerrainComponent::CalculateLowLODBufferSizes(uint32_t& uTotalVertsOut, uint32_t& uTotalIndicesOut) const
{
	// Each exporter output is a self-contained 16x16-quad LOW chunk. The
	// previous seam allowance over-reserved every interior chunk even though
	// no extra seam geometry exists in the serialized source.
	uTotalVertsOut = Zenith_TerrainChunkLayout::uLOW_CHUNK_VERTEX_COUNT * TOTAL_CHUNKS;
	uTotalIndicesOut = Zenith_TerrainChunkLayout::uLOW_CHUNK_INDEX_COUNT * TOTAL_CHUNKS;
}

void Zenith_TerrainComponent::LogSparseLoadDiagnostics(const char* szSourceKind,
	const TerrainSparseLoadDiagnostics& xDiagnostics)
{
	if (xDiagnostics.m_uSkippedCount == 0u)
		return;
	Zenith_Warning(LOG_CATEGORY_TERRAIN,
		"Terrain sparse %s load skipped %u missing/invalid non-anchor chunk(s); logging %u sample coordinate(s).",
		szSourceKind, xDiagnostics.m_uSkippedCount, xDiagnostics.m_uSampleCount);
	for (uint32_t u = 0u; u < xDiagnostics.m_uSampleCount; ++u)
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "Terrain sparse %s sample: (%u,%u)",
			szSourceKind, xDiagnostics.m_auSampleX[u], xDiagnostics.m_auSampleY[u]);
	}
}

bool Zenith_TerrainComponent::CombineTerrainChunkGridCore(uint32_t uGridSize,
	uint32_t uTotalVerts, uint32_t uTotalIndices,
	TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
	Flux_TerrainChunkInitData* pxChunkInitData,
	Flux_MeshGeometry*& pxCombinedGeometryOut,
	TerrainSparseLoadDiagnostics& xDiagnosticsOut)
{
	xDiagnosticsOut = {};
	pxCombinedGeometryOut = nullptr;
	if (uGridSize == 0u || uGridSize > CHUNK_GRID_SIZE || pfnLoadChunk == nullptr)
		return false;
	if (pxChunkInitData != nullptr)
	{
		// A retry may reuse caller-owned init storage. Clear every coordinate in
		// the requested grid before loading so a now-missing sparse chunk cannot
		// retain stale counts/AABB from an earlier complete bake.
		for (uint32_t uX = 0u; uX < uGridSize; ++uX)
		{
			for (uint32_t uY = 0u; uY < uGridSize; ++uY)
			{
				pxChunkInitData[Flux_TerrainConfig::ChunkCoordsToIndex(uX, uY)] = {};
			}
		}
	}

	pxCombinedGeometryOut = new Flux_MeshGeometry();
	if (!pfnLoadChunk(pLoadContext, 0u, 0u, *pxCombinedGeometryOut))
	{
		delete pxCombinedGeometryOut;
		pxCombinedGeometryOut = nullptr;
		return false;
	}
	xDiagnosticsOut.m_bAnchorLoaded = true;
	Flux_MeshGeometry& xCombinedGeometry = *pxCombinedGeometryOut;
	if (uTotalVerts < xCombinedGeometry.GetNumVerts() || uTotalIndices < xCombinedGeometry.GetNumIndices())
	{
		delete pxCombinedGeometryOut;
		pxCombinedGeometryOut = nullptr;
		return false;
	}

	if (pxChunkInitData != nullptr)
	{
		const uint32_t uAnchorIndex = Flux_TerrainConfig::ChunkCoordsToIndex(0u, 0u);
		pxChunkInitData[uAnchorIndex].m_uVertexCount = xCombinedGeometry.GetNumVerts();
		pxChunkInitData[uAnchorIndex].m_uIndexCount = xCombinedGeometry.GetNumIndices();
		pxChunkInitData[uAnchorIndex].m_xAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
			xCombinedGeometry.m_pxPositions, xCombinedGeometry.GetNumVerts());
	}

	const uint64_t ulVertexDataSize = static_cast<uint64_t>(uTotalVerts) * xCombinedGeometry.GetBufferLayout().GetStride();
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(uTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulPositionDataSize = static_cast<uint64_t>(uTotalVerts) * sizeof(Zenith_Maths::Vector3);
	u_int8* pExpandedVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(
		xCombinedGeometry.m_pVertexData, ulVertexDataSize));
	if (pExpandedVertexData == nullptr)
	{
		delete pxCombinedGeometryOut;
		pxCombinedGeometryOut = nullptr;
		return false;
	}
	xCombinedGeometry.m_pVertexData = pExpandedVertexData;
	Flux_MeshGeometry::IndexType* puExpandedIndices =
		static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(
			xCombinedGeometry.m_puIndices, ulIndexDataSize));
	if (puExpandedIndices == nullptr)
	{
		delete pxCombinedGeometryOut;
		pxCombinedGeometryOut = nullptr;
		return false;
	}
	xCombinedGeometry.m_puIndices = puExpandedIndices;
	Zenith_Maths::Vector3* pxExpandedPositions =
		static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(
			xCombinedGeometry.m_pxPositions, ulPositionDataSize));
	if (pxExpandedPositions == nullptr)
	{
		delete pxCombinedGeometryOut;
		pxCombinedGeometryOut = nullptr;
		return false;
	}
	xCombinedGeometry.m_pxPositions = pxExpandedPositions;
	xCombinedGeometry.m_ulReservedVertexDataSize = ulVertexDataSize;
	xCombinedGeometry.m_ulReservedIndexDataSize = ulIndexDataSize;
	xCombinedGeometry.m_ulReservedPositionDataSize = ulPositionDataSize;

	auto RecordSkippedChunk = [&](uint32_t uX, uint32_t uY)
	{
		xDiagnosticsOut.m_uSkippedCount++;
		if (xDiagnosticsOut.m_uSampleCount < uMAX_SPARSE_WARNING_SAMPLES)
		{
			xDiagnosticsOut.m_auSampleX[xDiagnosticsOut.m_uSampleCount] = uX;
			xDiagnosticsOut.m_auSampleY[xDiagnosticsOut.m_uSampleCount] = uY;
			xDiagnosticsOut.m_uSampleCount++;
		}
	};

	for (uint32_t uX = 0u; uX < uGridSize; ++uX)
	{
		for (uint32_t uY = 0u; uY < uGridSize; ++uY)
		{
			if (uX == 0u && uY == 0u)
				continue;
			Flux_MeshGeometry xChunkGeometry;
			if (!pfnLoadChunk(pLoadContext, uX, uY, xChunkGeometry))
			{
				RecordSkippedChunk(uX, uY);
				continue;
			}

			const uint32_t uChunkIndex = Flux_TerrainConfig::ChunkCoordsToIndex(uX, uY);
			if (pxChunkInitData != nullptr)
			{
				pxChunkInitData[uChunkIndex].m_uVertexCount = xChunkGeometry.GetNumVerts();
				pxChunkInitData[uChunkIndex].m_uIndexCount = xChunkGeometry.GetNumIndices();
				pxChunkInitData[uChunkIndex].m_xAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
					xChunkGeometry.m_pxPositions, xChunkGeometry.GetNumVerts());
			}

			const uint32_t uPreviousVertexCount = xCombinedGeometry.GetNumVerts();
			const uint32_t uPreviousIndexCount = xCombinedGeometry.GetNumIndices();
			Flux_MeshGeometry::Combine(xCombinedGeometry, xChunkGeometry);
			if (xCombinedGeometry.GetNumVerts() != uPreviousVertexCount + xChunkGeometry.GetNumVerts() ||
				xCombinedGeometry.GetNumIndices() != uPreviousIndexCount + xChunkGeometry.GetNumIndices())
			{
				delete pxCombinedGeometryOut;
				pxCombinedGeometryOut = nullptr;
				return false;
			}
		}
	}
	return true;
}

bool Zenith_TerrainComponent::LoadAndCombineLowLODChunksCore(uint32_t uGridSize,
	uint32_t uTotalVerts, uint32_t uTotalIndices,
	TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
	Flux_TerrainChunkInitData* pxChunkInitData,
	Flux_MeshGeometry*& pxLowLODGeometryOut,
	TerrainSparseLoadDiagnostics& xDiagnosticsOut)
{
	const bool bCombined = CombineTerrainChunkGridCore(uGridSize, uTotalVerts, uTotalIndices,
		pfnLoadChunk, pLoadContext, pxChunkInitData, pxLowLODGeometryOut, xDiagnosticsOut);
	if (!bCombined)
	{
		m_bTerrainGeometryUnusable = true;
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
		return false;
	}
	m_bTerrainGeometryUnusable = false;
	m_pxStreamingState->m_uVertexStride = pxLowLODGeometryOut->GetBufferLayout().GetStride();
	m_pxStreamingState->m_uLowLODVertexCount = pxLowLODGeometryOut->GetNumVerts();
	m_pxStreamingState->m_uLowLODIndexCount = pxLowLODGeometryOut->GetNumIndices();
	return true;
}

bool Zenith_TerrainComponent::LoadCombinedPhysicsGeometryCore(uint32_t uGridSize,
	TerrainChunkLoadCallback pfnLoadChunk, void* pLoadContext,
	TerrainSparseLoadDiagnostics& xDiagnosticsOut)
{
	if (m_bTerrainGeometryUnusable)
		return false;
	if (m_pxPhysicsGeometry != nullptr)
		return true;
	const uint32_t uTotalVerts = Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_VERTEX_COUNT * uGridSize * uGridSize;
	const uint32_t uTotalIndices = Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_INDEX_COUNT * uGridSize * uGridSize;
	const bool bCombined = CombineTerrainChunkGridCore(uGridSize, uTotalVerts, uTotalIndices,
		pfnLoadChunk, pLoadContext, nullptr, m_pxPhysicsGeometry, xDiagnosticsOut);
	if (!bCombined)
	{
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
	}
	return bCombined;
}

void Zenith_TerrainComponent::LoadAndCombineLowLODChunks(uint32_t uTotalVerts, uint32_t uTotalIndices, Flux_TerrainChunkInitData* pxChunkInitData, Flux_MeshGeometry*& pxLowLODGeometryOut)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOW LOD meshes for all %u chunks...", TOTAL_CHUNKS);
	struct DirectoryLoadContext
	{
		std::string m_strDirectory;
	};
	DirectoryLoadContext xContext{ GetTerrainAssetDirectory() };
	auto LoadChunk = [](void* pContext, uint32_t uX, uint32_t uY, Flux_MeshGeometry& xGeometryOut) -> bool
	{
		const DirectoryLoadContext& xLoadContext = *static_cast<const DirectoryLoadContext*>(pContext);
		const std::string strPath = xLoadContext.m_strDirectory + "Render_LOW_" +
			std::to_string(uX) + "_" + std::to_string(uY) + ZENITH_MESH_EXT;
		return TryLoadTerrainChunkSource(strPath,
			Zenith_TerrainChunkLayout::uLOW_CHUNK_VERTEX_COUNT,
			Zenith_TerrainChunkLayout::uLOW_CHUNK_INDEX_COUNT, false, xGeometryOut);
	};

	TerrainSparseLoadDiagnostics xDiagnostics;
	if (!LoadAndCombineLowLODChunksCore(CHUNK_GRID_SIZE, uTotalVerts, uTotalIndices,
		LoadChunk, &xContext, pxChunkInitData, pxLowLODGeometryOut, xDiagnostics))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Terrain LOW LOD chunk (0,0) failed validation (Render_LOW_0_0%s missing or invalid). Marking terrain geometry unusable; this terrain will not render and will not produce a physics body.",
			ZENITH_MESH_EXT);
		return;
	}
	LogSparseLoadDiagnostics("LOW LOD", xDiagnostics);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD mesh combination complete: %u vertices, %u indices",
		pxLowLODGeometryOut->GetNumVerts(), pxLowLODGeometryOut->GetNumIndices());
}

void Zenith_TerrainComponent::InitializeUnifiedBuffers(const Flux_MeshGeometry& xLowLODGeometry)
{
	const uint64_t ulLowLODVertexSize = xLowLODGeometry.GetVertexDataSize();
	const uint64_t ulLowLODIndexSize = xLowLODGeometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLowLODVertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLowLODIndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Initializing unified terrain buffers (owned by TerrainComponent):");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertex buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODVertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Index buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODIndexSize / (1024.0f * 1024.0f), STREAMING_INDEX_BUFFER_SIZE_MB,
		ulUnifiedIndexSize / (1024.0f * 1024.0f));

	uint8_t* pUnifiedVertexData = new uint8_t[ulUnifiedVertexSize];
	uint32_t* pUnifiedIndexData = new uint32_t[ulUnifiedIndexSize / sizeof(uint32_t)];

	memcpy(pUnifiedVertexData, xLowLODGeometry.m_pVertexData, ulLowLODVertexSize);
	memcpy(pUnifiedIndexData, xLowLODGeometry.m_puIndices, ulLowLODIndexSize);

	memset(pUnifiedVertexData + ulLowLODVertexSize, 0, STREAMING_VERTEX_BUFFER_SIZE);
	memset(pUnifiedIndexData + (ulLowLODIndexSize / sizeof(uint32_t)), 0, STREAMING_INDEX_BUFFER_SIZE);

	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_pxStreamingState->m_xUnifiedVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_pxStreamingState->m_xUnifiedIndexBuffer);

	m_pxStreamingState->m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_pxStreamingState->m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD region: vertices [0, %u), indices [0, %u)", m_pxStreamingState->m_uLowLODVertexCount, m_pxStreamingState->m_uLowLODIndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_pxStreamingState->m_uLowLODVertexCount, m_pxStreamingState->m_uLowLODIndexCount);
}

// ========== Physics Geometry Loading ==========

void Zenith_TerrainComponent::LoadCombinedPhysicsGeometry()
{
	if (m_pxPhysicsGeometry != nullptr)
	{
		return;  // Already loaded
	}

	// If render geometry is already known to be unusable, there's no
	// physics body to build either — without chunk (0,0) we don't know
	// the canonical mesh layout. Leave m_pxPhysicsGeometry null and let
	// callers gate on HasPhysicsGeometry().
	if (m_bTerrainGeometryUnusable)
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"Skipping physics geometry load — terrain geometry already marked unusable.");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading and combining all physics chunks...");
	struct DirectoryLoadContext
	{
		std::string m_strDirectory;
	};
	DirectoryLoadContext xContext{ GetTerrainAssetDirectory() };
	auto LoadChunk = [](void* pContext, uint32_t uX, uint32_t uY, Flux_MeshGeometry& xGeometryOut) -> bool
	{
		const DirectoryLoadContext& xLoadContext = *static_cast<const DirectoryLoadContext*>(pContext);
		const std::string strPath = xLoadContext.m_strDirectory + "Physics_" +
			std::to_string(uX) + "_" + std::to_string(uY) + ZENITH_MESH_EXT;
		return TryLoadTerrainChunkSource(strPath,
			Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_VERTEX_COUNT,
			Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_INDEX_COUNT, true, xGeometryOut);
	};

	TerrainSparseLoadDiagnostics xDiagnostics;
	if (!LoadCombinedPhysicsGeometryCore(CHUNK_GRID_SIZE, LoadChunk, &xContext, xDiagnostics))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Terrain physics chunk (0,0) failed validation (Physics_0_0%s missing or invalid). Terrain will have no physics body.",
			ZENITH_MESH_EXT);
		return;
	}
	LogSparseLoadDiagnostics("physics", xDiagnostics);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Physics mesh combined: %u vertices, %u indices (%u chunk(s) skipped)",
		m_pxPhysicsGeometry->GetNumVerts(), m_pxPhysicsGeometry->GetNumIndices(), xDiagnostics.m_uSkippedCount);
}

// ========== GPU-Driven Culling Implementation ==========

void Zenith_TerrainComponent::InitializeCullingResources()
{
	if (m_pxStreamingState->m_bCullingResourcesInitialized)
	{
		Zenith_Assert(false, "Zenith_TerrainComponent::InitializeCullingResources() called when already initialized");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling with LOD support");

	// ========== CREATE GPU BUFFERS ==========

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	// Frustum planes buffer (6 planes, updated per frame)
	xVulkanMemory.InitialiseDynamicConstantBuffer(
		nullptr,
		sizeof(Zenith_CameraDataGPU),
		m_pxStreamingState->m_xFrustumPlanesBuffer
	);

	// Indirect draw command buffer (one command per chunk, max)
	// Structure: VkDrawIndexedIndirectCommand (5 uint32_t values)
	// Initialize to zeros so all commands start with indexCount=0
	const size_t indirectBufferSize = sizeof(uint32_t) * 5 * TOTAL_CHUNKS;
	uint32_t* pZeroBuffer = new uint32_t[5 * TOTAL_CHUNKS];
	memset(pZeroBuffer, 0, indirectBufferSize);

	xVulkanMemory.InitialiseIndirectBuffer(
		indirectBufferSize,
		m_pxStreamingState->m_xIndirectDrawBuffer
	);

	// Upload the zero-initialized data
	xVulkanMemory.UploadBufferData(m_pxStreamingState->m_xIndirectDrawBuffer.GetBuffer().m_xVRAMHandle, pZeroBuffer, indirectBufferSize);
	delete[] pZeroBuffer;

	// Visible chunk counter (single atomic uint32_t)
	xVulkanMemory.InitialiseIndirectBuffer(
		sizeof(uint32_t),
		m_pxStreamingState->m_xVisibleCountBuffer
	);

	// LOD level buffer (one uint32_t per potential draw call). Zero-initialise
	// so the GPU LOD hysteresis (Flux_TerrainCulling.slang reads
	// LODLevelBuffer[chunkIndex] as priorLOD before writing) sees a
	// deterministic 0 (== HIGH) on the first frame instead of whatever
	// VRAM the allocator handed us. Garbage values can't trigger the
	// hysteresis branches (they require priorLOD == 0 or 1) but a zero-
	// init buffer makes the first-frame behaviour predictable.
	{
		uint32_t* pZero = new uint32_t[TOTAL_CHUNKS];
		memset(pZero, 0, sizeof(uint32_t) * TOTAL_CHUNKS);
		xVulkanMemory.InitialiseReadWriteBuffer(
			pZero,
			sizeof(uint32_t) * TOTAL_CHUNKS,
			m_pxStreamingState->m_xLODLevelBuffer
		);
		delete[] pZero;
	}

	// Build chunk data (AABBs + LOD metadata) and upload to GPU
	BuildChunkData();

	m_pxStreamingState->m_bCullingResourcesInitialized = true;

	// New per-terrain GPU buffers (chunk data, indirect, count, LOD level)
	// just appeared. g_xEngine.Terrain().SetupRenderGraph reads
	// m_bCullingResourcesInitialized when declaring per-component buffer
	// dependencies, so the next graph compile must rebuild to pick them up.
	g_xEngine.FluxRenderer().RequestGraphRebuild();

	// Note: m_xChunkDataBuffer no longer needs MarkBufferHostWritten here.
	// It's now a Flux_DynamicReadWriteBuffer (frame-indexed, host-visible),
	// and is intentionally not declared in the render graph for the same
	// reason m_xFrustumPlanesBuffer isn't — declaring a frame-indexed buffer
	// via GetBuffer() at compile time locks the graph to frame 0's instance.
	// vkSubmit's implicit host-write-available barrier covers visibility for
	// host-coherent buffers without a manual TransferWrite→ShaderRead
	// barrier, which only the staged-upload path required.

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Culling resources initialized with %u terrain chunks, %u LOD levels", TOTAL_CHUNKS, LOD_COUNT);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - LOD distances: HIGH<%.1f, LOW=always",
		sqrtf(LOD_MAX_DISTANCE_SQ[LOD_HIGH]));
}

void Zenith_TerrainComponent::DestroyCullingResources()
{
	if (!m_pxStreamingState || !m_pxStreamingState->m_bCullingResourcesInitialized)
	{
		return;
	}

	// Drop the rebuild flag BEFORE queueing buffer destruction so the next
	// graph compile rebuilds SetupRenderGraph against the new (smaller) set
	// of live terrain components. The deferred-destroy path
	// (QueueVRAMDeletion + MAX_FRAMES_IN_FLIGHT+1 grace) keeps the buffers
	// alive long enough for any in-flight command lists to finish reading
	// them before the GPU side actually frees the memory.
	g_xEngine.FluxRenderer().RequestGraphRebuild();

	// Cleanup GPU resources - queue for deferred deletion to avoid destroying in-use resources.
	// Chunk data buffer is frame-indexed; DestroyDynamicReadWriteBuffer queues
	// every frame slot for deferred deletion.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyDynamicReadWriteBuffer(m_pxStreamingState->m_xChunkDataBuffer);
	xVulkanMemory.DestroyDynamicConstantBuffer(m_pxStreamingState->m_xFrustumPlanesBuffer);
	xVulkanMemory.DestroyIndirectBuffer(m_pxStreamingState->m_xIndirectDrawBuffer);
	xVulkanMemory.DestroyIndirectBuffer(m_pxStreamingState->m_xVisibleCountBuffer);
	xVulkanMemory.DestroyReadWriteBuffer(m_pxStreamingState->m_xLODLevelBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Culling resources destroyed");

	m_pxStreamingState->m_bCullingResourcesInitialized = false;
}

void Zenith_TerrainComponent::BuildChunkData()
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::BuildChunkData() - Building chunk data using streaming manager");

	// Get chunk data from streaming manager (includes AABBs and current LOD
	// allocations) — component-aware overload routes through this terrain's
	// own state, no cross-terrain pollution.
	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	g_xEngine.TerrainStreaming().BuildChunkDataForGPU(m_pxStreamingState, pxChunkData);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data retrieved from streaming manager for %u chunks", TOTAL_CHUNKS);

	// Allocate the per-frame buffers. InitialiseDynamicReadWriteBuffer also
	// performs the initial upload into every frame's slot, so the GPU has
	// valid chunk metadata in slot 0 by the time the first frame's compute
	// runs (the orphan-read validator in the render graph would otherwise
	// trip on a Read declaration with no preceding writer).
	g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TOTAL_CHUNKS,
		m_pxStreamingState->m_xChunkDataBuffer
	);

	// Cleanup CPU data
	delete[] pxChunkData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data with %u LOD levels uploaded to GPU", LOD_COUNT);
}

void Zenith_TerrainComponent::UpdateChunkLODAllocations()
{
	// Wave 3: GPU chunk-data upload relocated to Flux_TerrainStreamingManagerImpl (operates on
	// the Flux state). Thin forwarder kept for API compatibility.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UpdateChunkLODAllocations(*m_pxStreamingState);
}

void Zenith_TerrainComponent::ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Zenith_FrustumPlaneGPU* pxOutPlanes)
{
	// Use existing frustum extraction code
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	// Convert to GPU format
	for (int i = 0; i < 6; ++i)
	{
		pxOutPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}
}

void Zenith_TerrainComponent::UploadFrustumPlanesForFrame(const Zenith_Maths::Matrix4& xViewProjMatrix)
{
	// Wave 3: relocated to Flux_TerrainStreamingManagerImpl. Thin forwarder.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UploadFrustumPlanesForFrame(*m_pxStreamingState, xViewProjMatrix);
}

void Zenith_TerrainComponent::UpdateCullingAndLod(Flux_CommandBuffer& xCmdList)
{
	// Wave 3: relocated to Flux_TerrainStreamingManagerImpl. Thin forwarder.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UpdateCullingAndLod(*m_pxStreamingState, xCmdList);
}
// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp
// ============================================================================
// Wave 3 PART B: terrain render-record gather. Flux_Terrain consumes these neutral
// records (Flux state + asset handles) instead of querying/holding Zenith_TerrainComponent.
// ============================================================================
static void Zenith_GatherTerrainRecordsImpl(Zenith_Vector<Flux_TerrainRenderRecord>& xOut)
{
	g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
		[&xOut](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
	{
		Flux_TerrainRenderRecord xRec;
		xRec.m_pxState = xTerrain.m_pxStreamingState;
		for (u_int m = 0; m < 4; ++m)
			xRec.m_apxMaterials[m] = xTerrain.GetMaterial(m);
		xRec.m_pxSplatmap = xTerrain.GetSplatmapTexture();
		xOut.PushBack(xRec);
	});
}

Zenith_TerrainGatherFn g_pfnZenithTerrainGather = &Zenith_GatherTerrainRecordsImpl;
