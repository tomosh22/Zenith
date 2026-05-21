#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Entity.h"
#include "Editor/Zenith_SelectionSystem.h"  // for BoundingBox
#include <unordered_map>

// Phase 5.5d: per-Engine selection-raycast AABB cache.
class Zenith_SelectionSystemImpl
{
public:
	Zenith_SelectionSystemImpl() = default;
	~Zenith_SelectionSystemImpl() = default;

	Zenith_SelectionSystemImpl(const Zenith_SelectionSystemImpl&) = delete;
	Zenith_SelectionSystemImpl& operator=(const Zenith_SelectionSystemImpl&) = delete;

	std::unordered_map<Zenith_EntityID, BoundingBox> m_xEntityBoundingBoxes;
};

#endif // ZENITH_TOOLS
