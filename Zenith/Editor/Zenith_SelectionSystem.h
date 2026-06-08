#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
// m_xEntityBoundingBoxes below is std::unordered_map<Zenith_EntityID, ...>, which
// needs the std::hash<Zenith_EntityID> specialization (in Zenith_Entity.h) visible
// BEFORE the member is declared. Scene.h is now an opaque handle that only
// forward-declares Zenith_EntityID, so include Entity.h explicitly (post-7b);
// relying on a transitive hash spec caused "specialization after instantiation".
#include "ZenithECS/Zenith_Entity.h"

#include "Collections/Zenith_HashMap.h"

class Zenith_Entity;

struct BoundingBox
{
	Zenith_Maths::Vector3 m_xMin;
	Zenith_Maths::Vector3 m_xMax;
	
	BoundingBox() : m_xMin(0, 0, 0), m_xMax(0, 0, 0) {}
	BoundingBox(const Zenith_Maths::Vector3& min, const Zenith_Maths::Vector3& max)
		: m_xMin(min), m_xMax(max) {}
	
	Zenith_Maths::Vector3 GetCenter() const
	{
		return (m_xMin + m_xMax) * 0.5f;
	}
	
	Zenith_Maths::Vector3 GetSize() const
	{
		return m_xMax - m_xMin;
	}
	
	bool Intersects(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir, float& outDistance) const;
	bool Contains(const Zenith_Maths::Vector3& point) const;
	void ExpandToInclude(const Zenith_Maths::Vector3& point);
	void Transform(const Zenith_Maths::Matrix4& transform);
};

class Zenith_SelectionSystem
{
public:
void Initialise();
void Shutdown();
	
	// Update bounding boxes for all entities
void UpdateBoundingBoxes();
	
	// Get bounding box for an entity
BoundingBox GetEntityBoundingBox(Zenith_Entity* pxEntity);
	
	// Raycast to select entities - returns EntityID to avoid memory management issues
Zenith_EntityID RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	
	// Debug rendering
void RenderBoundingBoxes();
void RenderSelectedBoundingBox(Zenith_Entity* pxEntity);

private:
BoundingBox CalculateBoundingBox(Zenith_Entity* pxEntity);

	// Test a single candidate hit for RaycastSelect. Returns true iff the model
	// was hit (either by its physics mesh or, as fallback, its cached AABB),
	// with the hit distance stored in fOutDistance. fMaxDistance lets callers
	// early-out when already beyond the closest known hit.
bool TestEntityHit(class Zenith_ModelComponent* pxModel,
		const Zenith_Maths::Vector3& xRayOrigin,
		const Zenith_Maths::Vector3& xRayDir,
		float fMaxDistance,
		float& fOutDistance);

public:
	// ===== Data members (was Zenith_SelectionSystem) =====
	Zenith_HashMap<Zenith_EntityID, BoundingBox> m_xEntityBoundingBoxes;
};

#endif // ZENITH_TOOLS
