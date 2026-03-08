#pragma once

#include "UI/Zenith_UIStyle.h"

namespace Zenith_UI {

class Zenith_UICanvas;

class UIStyleRenderer
{
public:
	// Render a styled rectangle — handles shadow, border, fill, gradient, rounded corners
	static void RenderStyledRect(
		Zenith_UICanvas& xCanvas,
		const UIStyle& xStyle,
		const Zenith_Maths::Vector4& xBounds,
		float fAlpha = 1.0f
	);
};

} // namespace Zenith_UI
