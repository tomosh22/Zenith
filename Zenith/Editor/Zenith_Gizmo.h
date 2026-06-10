#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"

// Screen-to-world ray conversion for editor viewport picking. The legacy
// ImGui-drawlist translate gizmo that used to live here was superseded by
// Flux_Gizmos (3D gizmo rendering + interaction).
class Zenith_Gizmo
{
public:
	// Mouse ray casting
	Zenith_Maths::Vector3 ScreenToWorldRay(
		const Zenith_Maths::Vector2& mousePos,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix
	);
};

#endif // ZENITH_TOOLS
