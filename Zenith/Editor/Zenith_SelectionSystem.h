#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"

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
	static void Initialise();
	static void Shutdown();
	
	// Update bounding boxes for all entities
	static void UpdateBoundingBoxes();
	
	// Get bounding box for an entity
	static BoundingBox GetEntityBoundingBox(Zenith_Entity* pxEntity);
	
	// Raycast to select entities - returns EntityID to avoid memory management issues
	static Zenith_EntityID RaycastSelect(const Zenith_Maths::Vector3& rayOrigin, const Zenith_Maths::Vector3& rayDir);
	
	// Debug rendering
	static void RenderBoundingBoxes();
	static void RenderSelectedBoundingBox(Zenith_Entity* pxEntity);

private:
	static BoundingBox CalculateBoundingBox(Zenith_Entity* pxEntity);
};

#endif // ZENITH_TOOLS
