#pragma once

#include "Maths/Zenith_Maths.h"

/**
 * Flux_Primitives - Debug primitive renderer
 *
 * Renders simple debug shapes (spheres, cubes, lines, etc.) into the GBuffer
 * at RENDER_ORDER_PRIMITIVES. All primitives are generated procedurally at runtime
 * and rendered using shared vertex/index buffers with per-instance transforms.
 *
 * Usage:
 *   Flux_Primitives::AddSphere(position, radius, color);
 *   Flux_Primitives::AddCube(center, halfExtents, color);
 *   Flux_Primitives::AddLine(start, end, color, thickness);
 *
 * Call patterns are similar to immediate-mode debug drawing. Primitives are cleared
 * each frame automatically after rendering.
 */
class Flux_Primitives
{
public:
	/**
	 * Initialize the primitives renderer
	 * - Creates shared vertex/index buffers for unit sphere, cube, etc.
	 * - Compiles and builds GBuffer pipeline
	 * - Registers with Flux rendering system
	 *
	 * Called once at engine startup from Flux::LateInitialise()
	 */
	static void Initialise();

	/**
	 * Clear state when scene resets (e.g., Play/Stop transitions)
	 * Resets command lists to prevent stale GPU resource references
	 */
	static void Reset();

	/**
	 * Submit the render task to the task system
	 * Called once per frame from SubmitRenderTasks() in Zenith_Core.cpp
	 */
	static void SubmitRenderTask();

	/**
	 * Wait for the render task to complete
	 * Called once per frame from WaitForRenderTasks() in Zenith_Core.cpp
	 */
	static void WaitForRenderTask();

	/**
	 * Queue a sphere for rendering this frame
	 * @param xCenter World-space center position
	 * @param fRadius Radius in world units
	 * @param xColor RGB color (0-1 range), alpha unused
	 */
	static void AddSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a cube for rendering this frame
	 * @param xCenter World-space center position
	 * @param xHalfExtents Half-size along each axis (full size = halfExtents * 2)
	 * @param xColor RGB color (0-1 range), alpha unused
	 */
	static void AddCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a wireframe cube for rendering this frame
	 * @param xCenter World-space center position
	 * @param xHalfExtents Half-size along each axis
	 * @param xColor RGB color (0-1 range), alpha unused
	 */
	static void AddWireframeCube(const Zenith_Maths::Vector3& xCenter, const Zenith_Maths::Vector3& xHalfExtents, const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a line for rendering this frame
	 * @param xStart Start point in world space
	 * @param xEnd End point in world space
	 * @param xColor RGB color (0-1 range), alpha unused
	 * @param fThickness Line thickness in world units (default 0.02)
	 */
	static void AddLine(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, const Zenith_Maths::Vector3& xColor, float fThickness = 0.02f);

	/**
	 * Queue a capsule for rendering this frame
	 * @param xStart Bottom center point in world space
	 * @param xEnd Top center point in world space
	 * @param fRadius Capsule radius
	 * @param xColor RGB color (0-1 range), alpha unused
	 */
	static void AddCapsule(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a cylinder for rendering this frame
	 * @param xStart Bottom center point in world space
	 * @param xEnd Top center point in world space
	 * @param fRadius Cylinder radius
	 * @param xColor RGB color (0-1 range), alpha unused
	 */
	static void AddCylinder(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd, float fRadius, const Zenith_Maths::Vector3& xColor);

	/**
	 * Clear all queued primitives
	 * Called automatically after rendering each frame, but can be called manually if needed
	 */
	static void Clear();

	/**
	 * Internal render function executed on worker thread
	 * Records Flux_CommandList with all queued primitives
	 * Public because it's used as a task callback
	 */
	static void Render(void*);
};
