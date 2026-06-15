#pragma once

#include "AI/Navigation/Zenith_NavMeshGenerator.h"

class Zenith_SceneData;

/**
 * Zenith_AINavGeometry - Engine-side bridge that builds navmesh input geometry
 * from concrete scene components.
 *
 * This helper lives in EntityComponent/ (the engine side) rather than in the
 * AI/ leaf library because it reads concrete component types
 * (Zenith_ColliderComponent + Zenith_TransformComponent) off the active scene
 * entities. The AI/ leaf must name no concrete component, so the
 * scene-geometry collection step is hosted here.
 *
 * It produces neutral vertex/index geometry which the pure leaf generator
 * Zenith_NavMeshGenerator::GenerateFromGeometry then consumes — keeping the
 * leaf free of any ECS / component dependency.
 */
class Zenith_AINavGeometry
{
public:
	/**
	 * Generate a navigation mesh from scene static geometry.
	 * Collects OBB triangles from the scene's static ColliderComponents, then
	 * hands the resulting geometry to the pure leaf generator.
	 * @param xScene Scene containing ColliderComponents
	 * @param xConfig Generation parameters
	 * @return Newly allocated NavMesh (caller owns), or nullptr on failure
	 */
	static Zenith_NavMesh* GenerateFromScene(Zenith_SceneData& xScene, const NavMeshGenerationConfig& xConfig);
};
