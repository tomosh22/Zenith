#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor_Noise.h"

//=============================================================================
// Brush kernels. All sculpt kernels operate on the normalized heightfield
// (1 texel == 1 world metre, value [0,1] == [0,512] m); paint kernels operate
// on the splat / grass maps at their own resolutions (callers pass WORLD
// coordinates; the kernels convert).
//
// Every kernel accumulates its touched rect into the stroke-undo capture
// (AccumulateUndoRect) BEFORE writing — the tile capture relies on it.
//=============================================================================

namespace
{
	// World metres a full-strength raise/lower dab moves the surface. With the
	// stroke spacing of radius*0.25, one slow drag pass raises ~3 m at
	// strength 1 — comparable to UE5's default sculpt rate.
	constexpr float fRAISE_PER_DAB_WORLD = 0.75f;

	float SmoothStep01(float fT)
	{
		fT = std::max(0.0f, std::min(1.0f, fT));
		return fT * fT * (3.0f - 2.0f * fT);
	}
}

float Zenith_TerrainEditor::EvaluateFalloff(Zenith_TerrainBrushFalloff eFalloff, float fNormDistance)
{
	const float fD = std::max(0.0f, std::min(1.0f, fNormDistance));
	switch (eFalloff)
	{
	case Zenith_TerrainBrushFalloff::Smooth: return 1.0f - SmoothStep01(fD);
	case Zenith_TerrainBrushFalloff::Linear: return 1.0f - fD;
	case Zenith_TerrainBrushFalloff::Sphere: return sqrtf(std::max(0.0f, 1.0f - fD * fD));
	case Zenith_TerrainBrushFalloff::Sharp:  return (1.0f - fD) * (1.0f - fD);
	default:                                 return 1.0f - fD;
	}
}

void Zenith_TerrainEditor::ApplyBrushDab(Zenith_TerrainBrushTool eTool, float fWorldX, float fWorldZ,
	float fRadius, float fStrength, float fToolValue)
{
	if (m_xHeightfield.IsEmpty())
	{
		return;
	}
	fRadius = std::max(0.5f, fRadius);
	fStrength = std::max(0.0f, std::min(1.0f, fStrength));

	// The first dab of a stroke anchors the Ramp corridor; outside a stroke
	// (automation dabs) every dab re-anchors itself.
	if (!m_bStrokeStartCaptured || !m_bStrokeActive)
	{
		m_bStrokeStartCaptured = true;
		m_xStrokeStartPos = { fWorldX, fWorldZ };
		m_fStrokeStartHeightNorm = SampleHeightNorm(fWorldX, fWorldZ);
	}

	switch (eTool)
	{
	case Zenith_TerrainBrushTool::SplatPaint:
		ApplySplatDab(fWorldX, fWorldZ, fRadius, fStrength, static_cast<u_int>(fToolValue));
		break;
	case Zenith_TerrainBrushTool::GrassDensity:
		ApplyGrassDab(fWorldX, fWorldZ, fRadius, fStrength, fToolValue);
		break;
	default:
		ApplyHeightDab(eTool, fWorldX, fWorldZ, fRadius, fStrength, fToolValue);
		break;
	}
}

void Zenith_TerrainEditor::ApplyHeightDab(Zenith_TerrainBrushTool eTool, float fPxX, float fPxZ,
	float fRadius, float fStrength, float fToolValue)
{
	const int iMax = static_cast<int>(uHEIGHTFIELD_SIZE) - 1;
	const int iX0 = std::max(0, static_cast<int>(floorf(fPxX - fRadius)));
	const int iX1 = std::min(iMax, static_cast<int>(ceilf(fPxX + fRadius)));
	const int iZ0 = std::max(0, static_cast<int>(floorf(fPxZ - fRadius)));
	const int iZ1 = std::min(iMax, static_cast<int>(ceilf(fPxZ + fRadius)));
	if (iX0 > iX1 || iZ0 > iZ1)
	{
		return;
	}

	if (m_bStrokeActive)
	{
		AccumulateUndoRect(Zenith_TerrainEditMap::Height,
			static_cast<u_int>(iX0), static_cast<u_int>(iZ0),
			static_cast<u_int>(iX1), static_cast<u_int>(iZ1));
	}

	const float fInvRadius = 1.0f / fRadius;
	const float fRaiseNorm = (fRAISE_PER_DAB_WORLD / fTERRAIN_MAX_HEIGHT) * fStrength;
	const float fTargetNorm = std::max(0.0f, std::min(1.0f, fToolValue / fTERRAIN_MAX_HEIGHT));

	// Per-tool precomputation.
	float fStampToTexel = 0.0f;
	if (eTool == Zenith_TerrainBrushTool::Stamp)
	{
		if (m_uStampSize == 0)
		{
			return;  // nothing captured yet (Ctrl+click samples the stamp)
		}
		fStampToTexel = static_cast<float>(m_uStampSize - 1) / (fRadius * 2.0f);
	}

	// Ramp corridor axis (stroke start -> this dab).
	Zenith_Maths::Vector2 xRampAxis = { fPxX - m_xStrokeStartPos.x, fPxZ - m_xStrokeStartPos.y };
	const float fRampLenSq = xRampAxis.x * xRampAxis.x + xRampAxis.y * xRampAxis.y;
	const float fRampEndHeight = SampleHeightNorm(fPxX, fPxZ);

	for (int iZ = iZ0; iZ <= iZ1; iZ++)
	{
		float* pfRow = m_xHeightfield.Row(static_cast<u_int>(iZ));
		const float fDZ = static_cast<float>(iZ) - fPxZ;
		for (int iX = iX0; iX <= iX1; iX++)
		{
			const float fDX = static_cast<float>(iX) - fPxX;
			const float fDistance = sqrtf(fDX * fDX + fDZ * fDZ) * fInvRadius;
			if (fDistance > 1.0f)
			{
				continue;
			}
			const float fW = EvaluateFalloff(m_xBrush.m_eFalloff, fDistance);
			if (fW <= 0.0f)
			{
				continue;
			}

			float fH = pfRow[iX];
			switch (eTool)
			{
			case Zenith_TerrainBrushTool::Raise:
				fH += fW * fRaiseNorm;
				break;

			case Zenith_TerrainBrushTool::Lower:
				fH -= fW * fRaiseNorm;
				break;

			case Zenith_TerrainBrushTool::Smooth:
			{
				// 3x3 neighbourhood average at a radius-scaled tap spacing so
				// big brushes smooth big features.
				const int iTap = std::max(1, static_cast<int>(fRadius / 8.0f));
				const u_int uXL = static_cast<u_int>(std::max(0, iX - iTap));
				const u_int uXR = static_cast<u_int>(std::min(iMax, iX + iTap));
				const u_int uZD = static_cast<u_int>(std::max(0, iZ - iTap));
				const u_int uZU = static_cast<u_int>(std::min(iMax, iZ + iTap));
				const float fAvg = (m_xHeightfield.At(uZD, uXL) + m_xHeightfield.At(uZD, static_cast<u_int>(iX)) + m_xHeightfield.At(uZD, uXR)
					+ m_xHeightfield.At(static_cast<u_int>(iZ), uXL) + fH + m_xHeightfield.At(static_cast<u_int>(iZ), uXR)
					+ m_xHeightfield.At(uZU, uXL) + m_xHeightfield.At(uZU, static_cast<u_int>(iX)) + m_xHeightfield.At(uZU, uXR)) / 9.0f;
				fH += (fAvg - fH) * fW * fStrength * 0.6f;
				break;
			}

			case Zenith_TerrainBrushTool::Flatten:
				fH += (fTargetNorm - fH) * fW * fStrength * 0.35f;
				break;

			case Zenith_TerrainBrushTool::SetHeight:
				fH += (fTargetNorm - fH) * std::min(1.0f, fW * fStrength * 2.0f);
				break;

			case Zenith_TerrainBrushTool::Noise:
			{
				// fToolValue = peak displacement in world metres.
				const float fN = Zenith_TerrainNoise::FBM(
					static_cast<float>(iX) * m_xBrush.m_fNoiseScale,
					static_cast<float>(iZ) * m_xBrush.m_fNoiseScale,
					1337u, 4, 2.0f, 0.5f);
				fH += (fN - 0.5f) * 2.0f * (fToolValue / fTERRAIN_MAX_HEIGHT) * fW * fStrength;
				break;
			}

			case Zenith_TerrainBrushTool::Terrace:
			{
				// fToolValue = step size in world metres.
				const float fStepNorm = std::max(0.5f, fToolValue) / fTERRAIN_MAX_HEIGHT;
				const float fStepped = roundf(fH / fStepNorm) * fStepNorm;
				fH += (fStepped - fH) * fW * fStrength * 0.5f;
				break;
			}

			case Zenith_TerrainBrushTool::Ramp:
			{
				// Flatten the corridor toward the line interpolating from the
				// stroke-start height to this dab's pre-edit height.
				float fT = 1.0f;
				if (fRampLenSq > 1.0e-4f)
				{
					const float fDotProduct = (static_cast<float>(iX) - m_xStrokeStartPos.x) * xRampAxis.x
						+ (static_cast<float>(iZ) - m_xStrokeStartPos.y) * xRampAxis.y;
					fT = std::max(0.0f, std::min(1.0f, fDotProduct / fRampLenSq));
				}
				const float fRampTarget = m_fStrokeStartHeightNorm + (fRampEndHeight - m_fStrokeStartHeightNorm) * fT;
				fH += (fRampTarget - fH) * fW * fStrength * m_xBrush.m_fRampHardness;
				break;
			}

			case Zenith_TerrainBrushTool::Stamp:
			{
				// Additive stamp of the captured region, relative to its
				// centre sample, masked by the falloff.
				const float fStampX = (static_cast<float>(iX) - (fPxX - fRadius)) * fStampToTexel;
				const float fStampZ = (static_cast<float>(iZ) - (fPxZ - fRadius)) * fStampToTexel;
				const int iSX0 = std::max(0, std::min(static_cast<int>(m_uStampSize) - 1, static_cast<int>(fStampX)));
				const int iSZ0 = std::max(0, std::min(static_cast<int>(m_uStampSize) - 1, static_cast<int>(fStampZ)));
				const int iSX1 = std::min(static_cast<int>(m_uStampSize) - 1, iSX0 + 1);
				const int iSZ1 = std::min(static_cast<int>(m_uStampSize) - 1, iSZ0 + 1);
				const float fTX = std::max(0.0f, std::min(1.0f, fStampX - static_cast<float>(iSX0)));
				const float fTZ = std::max(0.0f, std::min(1.0f, fStampZ - static_cast<float>(iSZ0)));
				const float* pfStamp = m_xStampData.GetDataPointer();
				const float fTop = pfStamp[iSZ0 * m_uStampSize + iSX0] * (1.0f - fTX) + pfStamp[iSZ0 * m_uStampSize + iSX1] * fTX;
				const float fBottom = pfStamp[iSZ1 * m_uStampSize + iSX0] * (1.0f - fTX) + pfStamp[iSZ1 * m_uStampSize + iSX1] * fTX;
				const float fStampH = fTop * (1.0f - fTZ) + fBottom * fTZ;
				fH += (fStampH - m_fStampReferenceHeight) * fW * fStrength;
				break;
			}

			default:
				break;
			}

			pfRow[iX] = std::max(0.0f, std::min(1.0f, fH));
		}
	}

	MarkHeightRegionDirty(static_cast<float>(iX0), static_cast<float>(iZ0),
		static_cast<float>(iX1), static_cast<float>(iZ1));
}

void Zenith_TerrainEditor::ApplySplatDab(float fPxX, float fPxZ, float fRadius, float fStrength, u_int uLayer)
{
	if (uLayer >= 4 || m_xSplatmap.GetSize() == 0)
	{
		return;
	}

	// World -> splat texel space (2048 texels over 4096 m).
	const float fScale = static_cast<float>(uSPLATMAP_SIZE) / fTERRAIN_WORLD_SIZE;
	const float fCX = fPxX * fScale;
	const float fCZ = fPxZ * fScale;
	const float fR = std::max(1.0f, fRadius * fScale);

	const int iMax = static_cast<int>(uSPLATMAP_SIZE) - 1;
	const int iX0 = std::max(0, static_cast<int>(floorf(fCX - fR)));
	const int iX1 = std::min(iMax, static_cast<int>(ceilf(fCX + fR)));
	const int iZ0 = std::max(0, static_cast<int>(floorf(fCZ - fR)));
	const int iZ1 = std::min(iMax, static_cast<int>(ceilf(fCZ + fR)));
	if (iX0 > iX1 || iZ0 > iZ1)
	{
		return;
	}

	if (m_bStrokeActive)
	{
		AccumulateUndoRect(Zenith_TerrainEditMap::Splat,
			static_cast<u_int>(iX0), static_cast<u_int>(iZ0),
			static_cast<u_int>(iX1), static_cast<u_int>(iZ1));
	}

	u_int8* pSplat = m_xSplatmap.GetDataPointer();
	const float fInvR = 1.0f / fR;
	for (int iZ = iZ0; iZ <= iZ1; iZ++)
	{
		const float fDZ = static_cast<float>(iZ) - fCZ;
		for (int iX = iX0; iX <= iX1; iX++)
		{
			const float fDX = static_cast<float>(iX) - fCX;
			const float fDistance = sqrtf(fDX * fDX + fDZ * fDZ) * fInvR;
			if (fDistance > 1.0f)
			{
				continue;
			}
			const float fW = EvaluateFalloff(m_xBrush.m_eFalloff, fDistance) * fStrength * 0.35f;
			if (fW <= 0.0f)
			{
				continue;
			}

			u_int8* pTexel = pSplat + (static_cast<size_t>(iZ) * uSPLATMAP_SIZE + iX) * 4;

			// Lerp the painted layer toward full weight, then rescale the
			// other three so the four weights keep summing to 255.
			float afWeights[4] = {
				static_cast<float>(pTexel[0]), static_cast<float>(pTexel[1]),
				static_cast<float>(pTexel[2]), static_cast<float>(pTexel[3])
			};
			afWeights[uLayer] += (255.0f - afWeights[uLayer]) * fW;
			float fOthers = 0.0f;
			for (u_int u = 0; u < 4; u++)
			{
				if (u != uLayer) { fOthers += afWeights[u]; }
			}
			const float fOthersTarget = 255.0f - afWeights[uLayer];
			if (fOthers > 0.001f)
			{
				const float fRescale = fOthersTarget / fOthers;
				for (u_int u = 0; u < 4; u++)
				{
					if (u != uLayer) { afWeights[u] *= fRescale; }
				}
			}
			else
			{
				afWeights[uLayer] = 255.0f;
			}

			// Round, then pin the painted layer to absorb rounding drift so
			// the sum stays exactly 255.
			u_int uSum = 0;
			for (u_int u = 0; u < 4; u++)
			{
				if (u == uLayer) { continue; }
				pTexel[u] = static_cast<u_int8>(std::max(0.0f, std::min(255.0f, roundf(afWeights[u]))));
				uSum += pTexel[u];
			}
			pTexel[uLayer] = static_cast<u_int8>(255 - std::min(255u, uSum));
		}
	}

	m_bSplatGPUDirty = true;
	m_bSessionDirty = true;
}

void Zenith_TerrainEditor::ApplyGrassDab(float fPxX, float fPxZ, float fRadius, float fStrength, float fTargetDensity)
{
	if (m_xGrassDensity.IsEmpty())
	{
		return;
	}

	// World -> grass texel space (1024 texels over 4096 m).
	const float fScale = static_cast<float>(uGRASS_DENSITY_SIZE) / fTERRAIN_WORLD_SIZE;
	const float fCX = fPxX * fScale;
	const float fCZ = fPxZ * fScale;
	const float fR = std::max(1.0f, fRadius * fScale);
	fTargetDensity = std::max(0.0f, std::min(1.0f, fTargetDensity));

	const int iMax = static_cast<int>(uGRASS_DENSITY_SIZE) - 1;
	const int iX0 = std::max(0, static_cast<int>(floorf(fCX - fR)));
	const int iX1 = std::min(iMax, static_cast<int>(ceilf(fCX + fR)));
	const int iZ0 = std::max(0, static_cast<int>(floorf(fCZ - fR)));
	const int iZ1 = std::min(iMax, static_cast<int>(ceilf(fCZ + fR)));
	if (iX0 > iX1 || iZ0 > iZ1)
	{
		return;
	}

	if (m_bStrokeActive)
	{
		AccumulateUndoRect(Zenith_TerrainEditMap::GrassDensity,
			static_cast<u_int>(iX0), static_cast<u_int>(iZ0),
			static_cast<u_int>(iX1), static_cast<u_int>(iZ1));
	}

	const float fInvR = 1.0f / fR;
	for (int iZ = iZ0; iZ <= iZ1; iZ++)
	{
		float* pfRow = m_xGrassDensity.Row(static_cast<u_int>(iZ));
		const float fDZ = static_cast<float>(iZ) - fCZ;
		for (int iX = iX0; iX <= iX1; iX++)
		{
			const float fDX = static_cast<float>(iX) - fCX;
			const float fDistance = sqrtf(fDX * fDX + fDZ * fDZ) * fInvR;
			if (fDistance > 1.0f)
			{
				continue;
			}
			const float fW = EvaluateFalloff(m_xBrush.m_eFalloff, fDistance) * fStrength;
			pfRow[iX] += (fTargetDensity - pfRow[iX]) * std::min(1.0f, fW);
		}
	}

	m_bGrassDirty = true;
	m_bSessionDirty = true;
}

#endif // ZENITH_TOOLS
