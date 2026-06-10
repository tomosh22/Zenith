#pragma once

#include "Collections/Zenith_Vector.h"

// Lightweight single-channel 32-bit float image (e.g. a terrain heightmap).
//
// Native float-image container used by the terrain and texture export tools.
// Storage is row-major: element (uY, uX) lives at index uY * m_uWidth + uX.
class Zenith_Image
{
public:
	Zenith_Image() = default;

	Zenith_Image(u_int uWidth, u_int uHeight)
		: m_xData(uWidth * uHeight)
		, m_uWidth(uWidth)
		, m_uHeight(uHeight)
	{
		// Zenith_Vector has no "resize to N defaults", so fill explicitly. This is
		// a one-time tools bake cost; the procedural paths overwrite every element.
		const u_int uCount = uWidth * uHeight;
		for (u_int u = 0; u < uCount; u++)
		{
			m_xData.PushBack(0.0f);
		}
	}

	u_int GetWidth() const { return m_uWidth; }   // width in pixels
	u_int GetHeight() const { return m_uHeight; }  // height in pixels
	bool IsEmpty() const { return m_uWidth == 0 || m_uHeight == 0; }

	// Element access at (row uY, column uX). Bounds-checked.
	float At(u_int uY, u_int uX) const { return m_xData.Get(uY * m_uWidth + uX); }
	float& At(u_int uY, u_int uX) { return m_xData.Get(uY * m_uWidth + uX); }

	// Pointer to the first element of row uY (m_uWidth contiguous floats).
	const float* Row(u_int uY) const { return m_xData.GetDataPointer() + uY * m_uWidth; }
	float* Row(u_int uY) { return m_xData.GetDataPointer() + uY * m_uWidth; }

	// Scan for the minimum and maximum stored value.
	void MinMax(float& fMinOut, float& fMaxOut) const;

	// Flip the image about its horizontal axis, in place.
	void FlipVertical();

private:
	Zenith_Vector<float> m_xData;	// row-major, size == m_uWidth * m_uHeight
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
};
