#include "Zenith.h"
#include "EntityComponent/Components/Zenith_NavMeshComponent.h"

#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#ifdef ZENITH_TOOLS
#include "AI/Zenith_AIWorldHooks.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#endif

const char* Zenith_NavMeshLoadStateToString(Zenith_NavMeshLoadState eState)
{
	switch (eState)
	{
	case NAVMESH_LOAD_STATE_UNLOADED:	return "UNLOADED";
	case NAVMESH_LOAD_STATE_LOADED:		return "LOADED";
	case NAVMESH_LOAD_STATE_FAILED:		return "FAILED";
	}
	return "UNKNOWN";
}

//=============================================================================
// Lifetime
//=============================================================================

Zenith_NavMeshComponent::~Zenith_NavMeshComponent()
{
	if (m_bMovedOut)
	{
		return;
	}
	Unload();
}

Zenith_NavMeshComponent::Zenith_NavMeshComponent(Zenith_NavMeshComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_strAssetRef(std::move(xOther.m_strAssetRef))
	, m_pxNavMesh(xOther.m_pxNavMesh)
	, m_eLoadState(xOther.m_eLoadState)
	, m_strFailureReason(std::move(xOther.m_strFailureReason))
#ifdef ZENITH_TOOLS
	, m_bDebugDrawEnabled(xOther.m_bDebugDrawEnabled)
	, m_xDebugDrawFlags(xOther.m_xDebugDrawFlags)
#endif
{
	// The moved-to component is now the SOLE owner of the mesh.
	xOther.m_pxNavMesh = nullptr;
	xOther.m_eLoadState = NAVMESH_LOAD_STATE_UNLOADED;
	xOther.m_bMovedOut = true;
}

Zenith_NavMeshComponent& Zenith_NavMeshComponent::operator=(Zenith_NavMeshComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Release anything this destination already owns, or the assignment
		// leaks it.
		if (!m_bMovedOut)
		{
			Unload();
		}

		m_xParentEntity = xOther.m_xParentEntity;
		m_strAssetRef = std::move(xOther.m_strAssetRef);
		m_pxNavMesh = xOther.m_pxNavMesh;
		m_eLoadState = xOther.m_eLoadState;
		m_strFailureReason = std::move(xOther.m_strFailureReason);
		m_bMovedOut = false;
#ifdef ZENITH_TOOLS
		m_bDebugDrawEnabled = xOther.m_bDebugDrawEnabled;
		m_xDebugDrawFlags = xOther.m_xDebugDrawFlags;
		m_bAssetRefEditPrimed = false;
#endif

		xOther.m_pxNavMesh = nullptr;
		xOther.m_eLoadState = NAVMESH_LOAD_STATE_UNLOADED;
		xOther.m_bMovedOut = true;
	}
	return *this;
}

void Zenith_NavMeshComponent::OnStart()
{
	if (m_strAssetRef.empty())
	{
		// An unconfigured component is inert, not broken -- the ref may be set
		// from code a frame later.
		return;
	}

	if (m_pxNavMesh != nullptr)
	{
		// Already loaded, so do NOT reload. The runtime adoption recipe is
		// AddComponent + SetAssetRef, which loads immediately -- but OnStart is
		// deferred to the entity's first Update, so it would otherwise fire
		// afterwards and free-then-reallocate the mesh, dangling any pointer the
		// caller had already taken from GetNavMesh().
		return;
	}

	LoadFromAssetRef();
}

void Zenith_NavMeshComponent::OnDestroy()
{
	if (m_bMovedOut)
	{
		return;
	}
	Unload();
}

//=============================================================================
// Asset ref / loading
//=============================================================================

std::string Zenith_NavMeshComponent::GetResolvedPath() const
{
	if (m_strAssetRef.empty())
	{
		return std::string();
	}
	return Zenith_AssetRegistry::ResolvePath(m_strAssetRef);
}

bool Zenith_NavMeshComponent::LoadFromAssetRef()
{
	Unload();

	// A component that is loading again is live again, whatever it was before:
	// clearing the moved-out mark here is what stops a revived moved-from
	// instance from skipping its own destructor's free and leaking.
	m_bMovedOut = false;

	if (m_strAssetRef.empty())
	{
		m_eLoadState = NAVMESH_LOAD_STATE_UNLOADED;
		m_strFailureReason.clear();
		return false;
	}

	// The AI leaf never sees the `game:` scheme; resolution happens here, and
	// --assets-root is already folded into the registry's roots upstream.
	const std::string strAbsolutePath = Zenith_AssetRegistry::ResolvePath(m_strAssetRef);

	// LoadFromFile asserts + logs the precise violation (missing file, bad magic,
	// truncation, bad index, ...) and returns nullptr.
	m_pxNavMesh = Zenith_NavMesh::LoadFromFile(strAbsolutePath);
	if (m_pxNavMesh == nullptr)
	{
		m_eLoadState = NAVMESH_LOAD_STATE_FAILED;
		m_strFailureReason = "could not load '" + strAbsolutePath + "'";
		return false;
	}

	m_eLoadState = NAVMESH_LOAD_STATE_LOADED;
	m_strFailureReason.clear();
	return true;
}

bool Zenith_NavMeshComponent::SetAssetRef(const std::string& strAssetRef)
{
	m_strAssetRef = strAssetRef;
#ifdef ZENITH_TOOLS
	m_bAssetRefEditPrimed = false;
#endif
	return LoadFromAssetRef();
}

bool Zenith_NavMeshComponent::Reload()
{
	return LoadFromAssetRef();
}

void Zenith_NavMeshComponent::Unload()
{
	delete m_pxNavMesh;
	m_pxNavMesh = nullptr;
	m_eLoadState = NAVMESH_LOAD_STATE_UNLOADED;
	m_strFailureReason.clear();
}

//=============================================================================
// Serialization -- the ref, and only the ref.
//=============================================================================

void Zenith_NavMeshComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strAssetRef;
}

void Zenith_NavMeshComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Assign the ref WITHOUT loading: the load belongs to OnStart, which runs
	// once the whole scene is deserialized.
	xStream >> m_strAssetRef;
}

#ifdef ZENITH_TOOLS

//=============================================================================
// Editor visualisation + debugging suite
//=============================================================================

void Zenith_NavMeshComponent::OnUpdate(float fDt)
{
	(void)fDt;

	if (!m_bDebugDrawEnabled || m_pxNavMesh == nullptr)
	{
		return;
	}

	// Draw calls land on the Zenith_AI_DebugDraw* seam, which no-ops under a
	// Null render backend -- no extra gating needed for headless runs.
	m_pxNavMesh->DebugDraw(m_xDebugDrawFlags);
	DebugDrawProbes();
}

bool Zenith_NavMeshComponent::TryGetMainCameraPosition(Zenith_Maths::Vector3& xOut)
{
	Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
	if (pxCamera == nullptr)
	{
		return false;
	}
	pxCamera->GetPosition(xOut);
	return true;
}

bool Zenith_NavMeshComponent::TryGetOwnEntityPosition(Zenith_Maths::Vector3& xOut)
{
	Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	if (pxTransform == nullptr)
	{
		return false;
	}

	pxTransform->GetPosition(xOut);
	return true;
}

void Zenith_NavMeshComponent::RunPointProbe()
{
	m_bProbePointValid = false;
	m_bProbeOnNavMesh = false;
	m_bProbeProjected = false;
	m_bProbeFoundPolygon = false;

	if (m_pxNavMesh == nullptr)
	{
		return;
	}

	m_bProbePointValid = true;
	m_bProbeOnNavMesh = m_pxNavMesh->IsPointOnNavMesh(m_xProbePoint);
	m_bProbeProjected = m_pxNavMesh->ProjectPoint(m_xProbePoint, m_xProbeProjected);
	m_bProbeFoundPolygon = m_pxNavMesh->FindNearestPolygon(m_xProbePoint, m_uProbePolygon, m_xProbeNearest);
}

void Zenith_NavMeshComponent::RunPathProbe()
{
	m_bPathProbeRun = false;
	m_axPathWaypoints.Clear();
	m_fPathDistance = 0.0f;

	if (m_pxNavMesh == nullptr)
	{
		return;
	}

	const Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(*m_pxNavMesh, m_xPathStart, m_xPathEnd);

	m_bPathProbeRun = true;
	m_uPathStatus = static_cast<u_int>(xResult.m_eStatus);
	m_fPathDistance = xResult.m_fTotalDistance;
	for (u_int u = 0; u < xResult.m_axWaypoints.GetSize(); ++u)
	{
		m_axPathWaypoints.PushBack(xResult.m_axWaypoints.Get(u));
	}
}

void Zenith_NavMeshComponent::DebugDrawProbes() const
{
	const Zenith_Maths::Vector3 xProbeColor(1.0f, 0.4f, 0.8f);
	const Zenith_Maths::Vector3 xPathColor(1.0f, 0.85f, 0.1f);

	if (m_bProbePointValid)
	{
		Zenith_AI_DebugDrawCross(m_xProbePoint, 0.5f, xProbeColor);
		if (m_bProbeProjected)
		{
			Zenith_AI_DebugDrawLine(m_xProbePoint, m_xProbeProjected, xProbeColor, 0.02f);
			Zenith_AI_DebugDrawSphere(m_xProbeProjected, 0.25f, xProbeColor);
		}
	}

	for (u_int u = 0; u + 1 < m_axPathWaypoints.GetSize(); ++u)
	{
		Zenith_AI_DebugDrawLine(m_axPathWaypoints.Get(u), m_axPathWaypoints.Get(u + 1), xPathColor, 0.05f);
	}
}

void Zenith_NavMeshComponent::RenderAssetSection()
{
	if (!ImGui::CollapsingHeader("Asset", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (!m_bAssetRefEditPrimed)
	{
		// Prime the edit buffer from the live ref once, so typing is not fought
		// by a per-frame overwrite.
		snprintf(m_szAssetRefEdit, sizeof(m_szAssetRefEdit), "%s", m_strAssetRef.c_str());
		m_bAssetRefEditPrimed = true;
	}

	ImGui::InputText("Asset ref", m_szAssetRefEdit, sizeof(m_szAssetRefEdit));
	ImGui::SameLine();
	if (ImGui::Button("Apply"))
	{
		SetAssetRef(std::string(m_szAssetRefEdit));
	}

	const std::string strResolved = GetResolvedPath();
	ImGui::TextWrapped("Resolved: %s", strResolved.empty() ? "(no ref)" : strResolved.c_str());

	if (!strResolved.empty())
	{
		std::error_code xEC;
		const bool bExists = std::filesystem::exists(strResolved, xEC) && !xEC;
		if (bExists)
		{
			const std::uintmax_t ulSize = std::filesystem::file_size(strResolved, xEC);
			ImGui::Text("On disk: yes (%llu bytes)", xEC ? 0ull : static_cast<uint64_t>(ulSize));

			// Peek the header rather than loading the whole file again.
			uint32_t auHeader[2] = {};
			if (Zenith_FileAccess::ReadPrefix(strResolved.c_str(), auHeader, sizeof(auHeader)) &&
				memcmp(auHeader, "ZNAV", 4) == 0)
			{
				ImGui::Text("Wire format: ZNAV v%u (this build writes v%u)",
					auHeader[1], uZENITH_NAVMESH_WIRE_VERSION);
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Wire format: NOT a ZNAV file");
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "On disk: NO");
		}
	}

	const Zenith_NavMeshLoadState eState = GetLoadState();
	const ImVec4 xStateColor = (eState == NAVMESH_LOAD_STATE_LOADED)
		? ImVec4(0.35f, 0.9f, 0.35f, 1.0f)
		: (eState == NAVMESH_LOAD_STATE_FAILED ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f)
			: ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
	ImGui::TextColored(xStateColor, "Load state: %s", Zenith_NavMeshLoadStateToString(eState));
	if (eState == NAVMESH_LOAD_STATE_FAILED)
	{
		ImGui::TextWrapped("Reason: %s", m_strFailureReason.c_str());
	}

	if (ImGui::Button("Reload"))
	{
		Reload();
	}
	ImGui::SameLine();
	if (ImGui::Button("Unload"))
	{
		Unload();
	}
}

void Zenith_NavMeshComponent::RenderStatsSection()
{
	if (!ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (m_pxNavMesh == nullptr)
	{
		ImGui::TextDisabled("(no mesh loaded)");
		return;
	}

	// Every number here comes from the unit-tested helper -- the panel only
	// formats, so it cannot drift from the mesh.
	const Zenith_NavMeshStats xStats = Zenith_NavMeshStats::Compute(*m_pxNavMesh);

	ImGui::Text("Vertices: %u", xStats.m_uVertexCount);
	ImGui::Text("Polygons: %u", xStats.m_uPolygonCount);
	ImGui::Text("Bounds min: (%.2f, %.2f, %.2f)", xStats.m_xBoundsMin.x, xStats.m_xBoundsMin.y, xStats.m_xBoundsMin.z);
	ImGui::Text("Bounds max: (%.2f, %.2f, %.2f)", xStats.m_xBoundsMax.x, xStats.m_xBoundsMax.y, xStats.m_xBoundsMax.z);
	ImGui::Text("Extents:    (%.2f, %.2f, %.2f)", xStats.m_xExtents.x, xStats.m_xExtents.y, xStats.m_xExtents.z);
	ImGui::Separator();
	ImGui::Text("Walkable area: %.2f", xStats.m_fTotalArea);
	ImGui::Text("Polygon area  min %.3f / avg %.3f / max %.3f",
		xStats.m_fMinPolygonArea, xStats.m_fAveragePolygonArea, xStats.m_fMaxPolygonArea);
	ImGui::Separator();
	ImGui::Text("Portal links: %u", xStats.m_uPortalLinkCount);
	ImGui::Text("Isolated polygons: %u", xStats.m_uIsolatedPolygonCount);
	ImGui::Text("Blocked polygons: %u", xStats.m_uBlockedPolygonCount);
	ImGui::Text("Approx memory: %llu bytes", xStats.m_ulApproxMemoryBytes);
}

void Zenith_NavMeshComponent::RenderVisualizationSection()
{
	if (!ImGui::CollapsingHeader("Visualization"))
	{
		return;
	}

	ImGui::Checkbox("Debug draw", &m_bDebugDrawEnabled);
	ImGui::Checkbox("Edges", &m_xDebugDrawFlags.m_bEdges);
	ImGui::Checkbox("Boundary edges", &m_xDebugDrawFlags.m_bBoundaryEdges);
	ImGui::Checkbox("Filled polygons", &m_xDebugDrawFlags.m_bFilled);
	ImGui::Checkbox("Adjacency links", &m_xDebugDrawFlags.m_bAdjacencyLinks);
	ImGui::Checkbox("Centers + normals", &m_xDebugDrawFlags.m_bCentersAndNormals);
	ImGui::Checkbox("Highlight blocked", &m_xDebugDrawFlags.m_bHighlightBlocked);
	ImGui::DragFloat("Surface offset", &m_xDebugDrawFlags.m_fSurfaceOffset, 0.01f, 0.0f, 2.0f, "%.2f m");
}

void Zenith_NavMeshComponent::RenderPointProbeSection()
{
	if (!ImGui::CollapsingHeader("Point probe"))
	{
		return;
	}

	ImGui::DragFloat3("Point", &m_xProbePoint.x, 0.25f);

	if (ImGui::Button("Use main camera"))
	{
		TryGetMainCameraPosition(m_xProbePoint);
	}
	ImGui::SameLine();
	if (ImGui::Button("Use this entity"))
	{
		TryGetOwnEntityPosition(m_xProbePoint);
	}
	ImGui::SameLine();
	if (ImGui::Button("Probe"))
	{
		RunPointProbe();
	}

	if (!m_bProbePointValid)
	{
		ImGui::TextDisabled("(not probed)");
		return;
	}

	ImGui::Text("IsPointOnNavMesh: %s", m_bProbeOnNavMesh ? "yes" : "no");
	if (m_bProbeProjected)
	{
		ImGui::Text("ProjectPoint: (%.2f, %.2f, %.2f)",
			m_xProbeProjected.x, m_xProbeProjected.y, m_xProbeProjected.z);
	}
	else
	{
		ImGui::Text("ProjectPoint: failed");
	}

	if (m_bProbeFoundPolygon)
	{
		ImGui::Text("FindNearestPolygon: %u at (%.2f, %.2f, %.2f)",
			m_uProbePolygon, m_xProbeNearest.x, m_xProbeNearest.y, m_xProbeNearest.z);
	}
	else
	{
		ImGui::Text("FindNearestPolygon: none in range");
	}
}

void Zenith_NavMeshComponent::RenderPathProbeSection()
{
	if (!ImGui::CollapsingHeader("Path probe"))
	{
		return;
	}

	ImGui::DragFloat3("Start", &m_xPathStart.x, 0.25f);
	ImGui::DragFloat3("End", &m_xPathEnd.x, 0.25f);

	if (ImGui::Button("Start = camera"))
	{
		TryGetMainCameraPosition(m_xPathStart);
	}
	ImGui::SameLine();
	if (ImGui::Button("End = this entity"))
	{
		TryGetOwnEntityPosition(m_xPathEnd);
	}
	ImGui::SameLine();
	if (ImGui::Button("Find path"))
	{
		RunPathProbe();
	}

	if (!m_bPathProbeRun)
	{
		ImGui::TextDisabled("(not run)");
		return;
	}

	const char* szStatus = "FAILED";
	if (m_uPathStatus == static_cast<u_int>(Zenith_PathResult::Status::SUCCESS))
	{
		szStatus = "SUCCESS";
	}
	else if (m_uPathStatus == static_cast<u_int>(Zenith_PathResult::Status::PARTIAL))
	{
		szStatus = "PARTIAL (nearest reachable)";
	}

	ImGui::Text("Status: %s", szStatus);
	ImGui::Text("Waypoints: %u", m_axPathWaypoints.GetSize());
	ImGui::Text("Length: %.2f m", m_fPathDistance);
}

void Zenith_NavMeshComponent::RenderPropertiesPanel()
{
	RenderAssetSection();
	RenderStatsSection();
	RenderVisualizationSection();
	RenderPointProbeSection();
	RenderPathProbeSection();
}

#endif // ZENITH_TOOLS

#ifdef ZENITH_TESTING
#include "EntityComponent/Components/Zenith_NavMeshComponent.Tests.inl"
#endif
