#include "Zenith.h"
#include "AssetHandling/Zenith_Image.h"

void Zenith_Image::MinMax(float& fMinOut, float& fMaxOut) const
{
	if (IsEmpty())
	{
		fMinOut = 0.0f;
		fMaxOut = 0.0f;
		return;
	}

	const float* pfData = m_xData.GetDataPointer();
	const u_int uCount = m_uWidth * m_uHeight;
	float fMin = pfData[0];
	float fMax = pfData[0];
	for (u_int u = 1; u < uCount; u++)
	{
		const float fValue = pfData[u];
		if (fValue < fMin) fMin = fValue;
		if (fValue > fMax) fMax = fValue;
	}
	fMinOut = fMin;
	fMaxOut = fMax;
}

void Zenith_Image::FlipVertical()
{
	if (IsEmpty()) return;

	float* pfData = m_xData.GetDataPointer();
	for (u_int uY = 0; uY < m_uHeight / 2; uY++)
	{
		float* pfRowTop = pfData + uY * m_uWidth;
		float* pfRowBottom = pfData + (m_uHeight - 1 - uY) * m_uWidth;
		for (u_int uX = 0; uX < m_uWidth; uX++)
		{
			const float fTemp = pfRowTop[uX];
			pfRowTop[uX] = pfRowBottom[uX];
			pfRowBottom[uX] = fTemp;
		}
	}
}
