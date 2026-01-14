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
	 * Shutdown the primitives renderer
	 * Destroys all GPU resources (vertex/index buffers)
	 */
	static void Shutdown();

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
	 * Queue a filled triangle for rendering this frame
	 * @param xV0 First vertex in world space
	 * @param xV1 Second vertex in world space
	 * @param xV2 Third vertex in world space (CCW winding for front-facing)
	 * @param xColor RGB color (0-1 range)
	 */
	static void AddTriangle(const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1,
		const Zenith_Maths::Vector3& xV2, const Zenith_Maths::Vector3& xColor);

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

	// ========== HELPER FUNCTIONS (use existing primitives) ==========

	/**
	 * Queue a cross/marker for rendering this frame (3 perpendicular lines)
	 * Useful for marking positions in world space
	 * @param xCenter World-space center position
	 * @param fSize Half-size of each axis line
	 * @param xColor RGB color (0-1 range)
	 */
	static void AddCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a circle for rendering this frame (made of line segments)
	 * @param xCenter World-space center position
	 * @param fRadius Radius in world units
	 * @param xColor RGB color (0-1 range)
	 * @param xNormal Normal vector of the circle plane (default Y-up)
	 * @param uSegments Number of line segments (default 32)
	 */
	static void AddCircle(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor,
		const Zenith_Maths::Vector3& xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), uint32_t uSegments = 32);

	/**
	 * Queue an arrow for rendering this frame (line with arrowhead)
	 * @param xStart Start point in world space
	 * @param xEnd End point (arrowhead location)
	 * @param xColor RGB color (0-1 range)
	 * @param fThickness Line thickness (default 0.02)
	 * @param fHeadSize Arrowhead size multiplier (default 0.15)
	 */
	static void AddArrow(const Zenith_Maths::Vector3& xStart, const Zenith_Maths::Vector3& xEnd,
		const Zenith_Maths::Vector3& xColor, float fThickness = 0.02f, float fHeadSize = 0.15f);

	/**
	 * Queue a cone outline for rendering this frame (made of line segments)
	 * Useful for visualizing vision cones, audio ranges, etc.
	 * @param xApex Apex (tip) of the cone in world space
	 * @param xDirection Direction the cone points
	 * @param fAngle Half-angle of the cone in degrees
	 * @param fLength Length of the cone
	 * @param xColor RGB color (0-1 range)
	 * @param uSegments Number of segments around the cone base (default 16)
	 */
	static void AddConeOutline(const Zenith_Maths::Vector3& xApex, const Zenith_Maths::Vector3& xDirection,
		float fAngle, float fLength, const Zenith_Maths::Vector3& xColor, uint32_t uSegments = 16);

	/**
	 * Queue an arc for rendering this frame (partial circle made of line segments)
	 * @param xCenter World-space center position
	 * @param fRadius Radius in world units
	 * @param fStartAngle Start angle in degrees (0 = forward/+Z)
	 * @param fEndAngle End angle in degrees
	 * @param xColor RGB color (0-1 range)
	 * @param xNormal Normal vector of the arc plane (default Y-up)
	 * @param uSegments Number of line segments (default 16)
	 */
	static void AddArc(const Zenith_Maths::Vector3& xCenter, float fRadius, float fStartAngle, float fEndAngle,
		const Zenith_Maths::Vector3& xColor, const Zenith_Maths::Vector3& xNormal = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		uint32_t uSegments = 16);

	/**
	 * Queue a wireframe polygon for rendering this frame
	 * @param axVertices Array of vertices forming the polygon (in order)
	 * @param uVertexCount Number of vertices
	 * @param xColor RGB color (0-1 range)
	 * @param bClosed If true, draws line from last vertex back to first (default true)
	 */
	static void AddPolygonOutline(const Zenith_Maths::Vector3* axVertices, uint32_t uVertexCount,
		const Zenith_Maths::Vector3& xColor, bool bClosed = true);

	/**
	 * Queue a ground-aligned grid for rendering this frame
	 * @param xCenter Grid center position
	 * @param fSize Total size of the grid (width/height)
	 * @param uDivisions Number of divisions per axis
	 * @param xColor RGB color (0-1 range)
	 */
	static void AddGrid(const Zenith_Maths::Vector3& xCenter, float fSize, uint32_t uDivisions,
		const Zenith_Maths::Vector3& xColor);

	/**
	 * Queue a coordinate axes indicator for rendering this frame
	 * @param xOrigin Origin position
	 * @param fSize Length of each axis
	 */
	static void AddAxes(const Zenith_Maths::Vector3& xOrigin, float fSize);
};
