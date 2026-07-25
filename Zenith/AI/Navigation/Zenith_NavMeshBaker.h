#pragma once

#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <cstdint>
#include <string>

class Zenith_NavMesh;

// ============================================================================
// Zenith_NavMeshBaker -- the game-agnostic tools-time half of baked-navmesh
// persistence (ZM-D-147). It is the engine analog of Unity's NavMeshSurface
// "Bake" button / UE's RecastNavMesh build: hand it raw walkable geometry and a
// destination path, and it produces a COMMITTABLE `.znavmesh` that any runtime
// -- windowed, headless or packaged -- can load with no GPU, no terrain
// component and no generator run.
//
// Stateless statics; no cache, no globals. The pipeline is
//   generate (Zenith_NavMeshGenerator::GenerateFromGeometry)
//     -> serialize (Zenith_NavMesh::WriteToDataStream)
//     -> write     (Zenith_DataStream::WriteToFile)
//     -> READ BACK and memcmp
// The read-back is not belt-and-braces: Zenith_FileAccess::WriteFile returns
// void, so comparing the bytes on disk against the bytes we meant to write is
// the only truthful success signal in the stack.
//
// TOTALITY BOUNDARY: Zenith_FileAccess::WriteFile asserts when the path cannot
// be opened at all (it auto-creates parent directories, so this means a bad
// drive / permissions). That arm is unreachable from a well-formed tools call
// and no unit drives it; every other failure comes back as a typed error.
//
// Leaf-safe: this lives in the strict-leaf ZenithAI library, so it names
// FileAccess but never AssetHandling, g_xEngine or any concrete component.
// Callers resolve `game:`-prefixed refs themselves (SentinelAI enforces this).
// ============================================================================

enum Zenith_NavMeshBakeError : u_int
{
	NAVMESH_BAKE_ERROR_NONE = 0,
	NAVMESH_BAKE_ERROR_EMPTY_PATH,          // caller passed no destination
	NAVMESH_BAKE_ERROR_EMPTY_GEOMETRY,      // no vertices and/or no indices
	NAVMESH_BAKE_ERROR_GENERATION_FAILED,   // generator produced no walkable mesh
	NAVMESH_BAKE_ERROR_SERIALIZE_FAILED,    // serializer emitted zero bytes
	NAVMESH_BAKE_ERROR_WRITE_FAILED,        // file absent / wrong size after write
	NAVMESH_BAKE_ERROR_READBACK_MISMATCH,   // bytes on disk != bytes written
};

const char* Zenith_NavMeshBakeErrorToString(Zenith_NavMeshBakeError eError);

struct Zenith_NavMeshBakeResult
{
	bool m_bSuccess = false;
	Zenith_NavMeshBakeError m_eError = NAVMESH_BAKE_ERROR_NONE;
	u_int m_uPolygonCount = 0;
	u_int m_uVertexCount = 0;
	uint64_t m_ulBytesWritten = 0;
};

class Zenith_NavMeshBaker
{
public:
	/**
	 * Generate a navmesh from raw geometry and persist it to strPath.
	 *
	 * @param axVertices  Walkable-surface vertex positions.
	 * @param auIndices   Triangle indices into axVertices.
	 * @param xConfig     Generation parameters (agent size, cell size, ...).
	 * @param strPath     ABSOLUTE destination path. Parent directories are
	 *                    created by the file layer, so a first bake into a fresh
	 *                    `Assets/Navmesh/` works with no mkdir.
	 * @param ppxMeshOut  Optional. On success, receives the generated mesh and
	 *                    the CALLER takes ownership (use it to assert on the
	 *                    bake without re-loading). Left nullptr on any failure;
	 *                    when omitted the baker deletes the mesh itself.
	 *
	 * Asserts (loudly, per the D6 failure doctrine) on the two CALLER defects --
	 * an empty path and empty geometry -- and returns the typed error either
	 * way. A generator that legitimately finds nothing walkable is a DATA
	 * outcome, not a defect: it reports GENERATION_FAILED with a log line and
	 * no assert.
	 */
	static Zenith_NavMeshBakeResult BakeToFile(
		const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		const Zenith_Vector<uint32_t>& auIndices,
		const NavMeshGenerationConfig& xConfig,
		const std::string& strPath,
		Zenith_NavMesh** ppxMeshOut = nullptr);
};
