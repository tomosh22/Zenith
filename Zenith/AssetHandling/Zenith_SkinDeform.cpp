#include "Zenith.h"

#include "AssetHandling/Zenith_SkinDeform.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

#include <algorithm>
#include <cmath>

namespace
{
	// Fixed bin counts. Every scan below is a histogram, and the counts are
	// CONSTANTS rather than functions of vertex count so the same mesh measures
	// the same on every machine and in every config -- BindIsDeterministic and
	// WarpIsDeterministic both lean on that.
	constexpr u_int uSCAN_BINS = 128u;
	constexpr float fARM_WEIGHT_EPSILON = 0.05f;

	// One vertical slice's cross-section, as the scans need it.
	struct SliceStat
	{
		u_int m_uCount = 0u;
		float m_fMinX = 0.0f, m_fMaxX = 0.0f;
		float m_fMinZ = 0.0f, m_fMaxZ = 0.0f;
		float m_fSumAbsX = 0.0f;

		void Add(float fX, float fZ)
		{
			if (m_uCount == 0u) { m_fMinX = m_fMaxX = fX; m_fMinZ = m_fMaxZ = fZ; }
			else
			{
				m_fMinX = std::min(m_fMinX, fX); m_fMaxX = std::max(m_fMaxX, fX);
				m_fMinZ = std::min(m_fMinZ, fZ); m_fMaxZ = std::max(m_fMaxZ, fZ);
			}
			m_fSumAbsX += std::fabs(fX);
			++m_uCount;
		}
		// The mean of the two half-extents: a cross-section's "radius" for an
		// ellipse-ish limb, and stable against the ring subdivision that would
		// make a single-axis extent jitter.
		float Radius() const { return 0.25f * ((m_fMaxX - m_fMinX) + (m_fMaxZ - m_fMinZ)); }
		float MeanAbsX() const { return (m_uCount > 0u) ? (m_fSumAbsX / static_cast<float>(m_uCount)) : 0.0f; }
	};

	u_int BinOf(float fValue, float fLo, float fHi, u_int uBins)
	{
		if (fHi - fLo <= 1.0e-8f) { return 0u; }
		const float fT = (fValue - fLo) / (fHi - fLo);
		const int iBin = static_cast<int>(fT * static_cast<float>(uBins));
		return static_cast<u_int>(std::clamp(iBin, 0, static_cast<int>(uBins) - 1));
	}

	float BinCentre(u_int uBin, float fLo, float fHi, u_int uBins)
	{
		return fLo + (static_cast<float>(uBin) + 0.5f) * (fHi - fLo) / static_cast<float>(uBins);
	}

	// The lowest interior bin whose radius is strictly below both of its
	// neighbours AND below both ends of the band. Returns false when the profile
	// is monotonic across the band, which is the honest answer for a limb that
	// simply tapers away -- Zenithmon's legs end in a point and have no ankle.
	bool FindInteriorRadiusMinimum(const SliceStat* pxBins, u_int uLo, u_int uHi, u_int& uOut)
	{
		if (uHi < uLo + 2u) { return false; }
		u_int uBest = uLo;
		float fBest = 0.0f;
		bool bAny = false;
		for (u_int u = uLo; u <= uHi; ++u)
		{
			if (pxBins[u].m_uCount == 0u) { continue; }
			const float fR = pxBins[u].Radius();
			if (!bAny || fR < fBest) { bAny = true; fBest = fR; uBest = u; }
		}
		if (!bAny || uBest == uLo || uBest == uHi) { return false; }
		uOut = uBest;
		return true;
	}

	float MedianRadius(const SliceStat* pxBins, u_int uLo, u_int uHi)
	{
		float afR[uSCAN_BINS];
		u_int uN = 0u;
		for (u_int u = uLo; u <= uHi && uN < uSCAN_BINS; ++u)
		{
			if (pxBins[u].m_uCount > 0u) { afR[uN++] = pxBins[u].Radius(); }
		}
		if (uN == 0u) { return 0.0f; }
		std::sort(afR, afR + uN);
		return afR[uN / 2u];
	}

	float ArmWeightOf(const Zenith_SkinDeformView& xView, u_int uVert, u_int64 ulMask)
	{
		if (!xView.HasSkinning()) { return 0.0f; }
		const glm::uvec4& xIdx = xView.m_pxBoneIndices[uVert];
		const glm::vec4& xW = xView.m_pxBoneWeights[uVert];
		float fSum = 0.0f;
		for (int i = 0; i < 4; ++i)
		{
			const u_int uBone = xIdx[i];
			if (uBone < 64u && ((ulMask >> uBone) & 1ull) != 0ull) { fSum += xW[i]; }
		}
		return std::clamp(fSum, 0.0f, 1.0f);
	}

	float PiecewiseLinear(const float* pfSrc, const float* pfDst, u_int uCount, float fY)
	{
		if (uCount < 2u) { return fY; }
		// ★ EXTRAPOLATE, NEVER CLAMP, past either end. Zenithmon appends hats and
		// hair ABOVE the crown; clamping would flatten every one of them onto the
		// scalp. The end segments are the identity wherever the endpoints are
		// pinned, so extrapolation past them is the identity too.
		if (fY <= pfSrc[0])
		{
			const float fD = pfSrc[1] - pfSrc[0];
			return (fD > 1.0e-8f) ? (pfDst[0] + (fY - pfSrc[0]) * (pfDst[1] - pfDst[0]) / fD) : pfDst[0];
		}
		for (u_int u = 0u; u + 1u < uCount; ++u)
		{
			if (fY <= pfSrc[u + 1u])
			{
				const float fD = pfSrc[u + 1u] - pfSrc[u];
				const float fT = (fD > 1.0e-8f) ? ((fY - pfSrc[u]) / fD) : 0.0f;
				return pfDst[u] + fT * (pfDst[u + 1u] - pfDst[u]);
			}
		}
		const u_int uL = uCount - 1u;
		const float fD = pfSrc[uL] - pfSrc[uL - 1u];
		return (fD > 1.0e-8f)
			? (pfDst[uL] + (fY - pfSrc[uL]) * (pfDst[uL] - pfDst[uL - 1u]) / fD)
			: pfDst[uL];
	}

	const char* szBODY_ANCHOR_NAMES[ZENITH_HUMAN_BODY_ANCHOR_COUNT] =
	{
		"sole", "ankle", "knee", "hip", "shoulder", "neck", "head", "crown"
	};
	const char* szARM_ANCHOR_NAMES[ZENITH_HUMAN_ARM_ANCHOR_COUNT] =
	{
		"fingertip", "wrist", "elbow", "shoulder"
	};
}

//==============================================================================
// View adapter
//==============================================================================

Zenith_SkinDeformView Zenith_MakeSkinDeformView(Zenith_MeshAsset& xMesh)
{
	Zenith_SkinDeformView xView;
	xView.m_uNumVerts = xMesh.GetNumVerts();
	xView.m_pxPositions = xMesh.m_xPositions.GetDataPointer();
	xView.m_pxNormals = (xMesh.m_xNormals.GetSize() == xView.m_uNumVerts)
		? xMesh.m_xNormals.GetDataPointer() : nullptr;
	if (xMesh.m_xBoneIndices.GetSize() == xView.m_uNumVerts &&
		xMesh.m_xBoneWeights.GetSize() == xView.m_uNumVerts)
	{
		xView.m_pxBoneIndices = xMesh.m_xBoneIndices.GetDataPointer();
		xView.m_pxBoneWeights = xMesh.m_xBoneWeights.GetDataPointer();
	}
	return xView;
}

//==============================================================================
// Landmark measurement
//==============================================================================

namespace
{
	// * THE COORDINATE THAT RUNS PROXIMAL -> DISTAL ALONG THE ARM, and the ONLY
	// thing the two poses disagree about. Hanging down, the arm runs down -Y from
	// the shoulder; held out in a T-pose, the right arm runs out +X. Both are read
	// as "bigger is nearer the body", so FindJointByWeightCrossing's single
	// scan-from-the-proximal-end works unchanged for either.
	float LimbCoordOf(const Zenith_Maths::Vector3& xP, bool bLateral)
	{
		return bLateral ? -xP.x : xP.y;
	}

	// Mean weight, per limb bin, on the bones DISTAL of one joint -- taken over
	// the right arm only, since the two arms sit at opposite X.
	void BuildDistalWeightProfile(const Zenith_SkinDeformView& xView, u_int64 ulArmMask,
		u_int64 ulDistalMask, float fLo, float fHi, float* pfOut, u_int* puCounts,
		bool bLateral)
	{
		for (u_int u = 0u; u < uSCAN_BINS; ++u) { pfOut[u] = 0.0f; puCounts[u] = 0u; }
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (ArmWeightOf(xView, v, ulArmMask) <= fARM_WEIGHT_EPSILON) { continue; }
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (xP.x <= 0.0f) { continue; }
			const u_int uBin = BinOf(LimbCoordOf(xP, bLateral), fLo, fHi, uSCAN_BINS);
			pfOut[uBin] += ArmWeightOf(xView, v, ulDistalMask);
			++puCounts[uBin];
		}
		for (u_int u = 0u; u < uSCAN_BINS; ++u)
		{
			if (puCounts[u] > 0u) { pfOut[u] /= static_cast<float>(puCounts[u]); }
		}
	}

	// The height at which that profile crosses 0.5, scanning DOWN from the top --
	// i.e. the joint. A chain whose distal weight never reaches 0.5 (Zenithmon's
	// arm ends at a 0.45 hand blend, because it lofts no hand) reports its
	// maximum instead, which is that arm's end and the right answer for it.
	bool FindJointByWeightCrossing(const float* pfProfile, const u_int* puCounts,
		float fLoY, float fHiY, float& fOut)
	{
		int iPrev = -1;
		int iBestMax = -1;
		float fBestMax = -1.0f;
		for (int i = static_cast<int>(uSCAN_BINS) - 1; i >= 0; --i)
		{
			const u_int u = static_cast<u_int>(i);
			if (puCounts[u] == 0u) { continue; }
			if (pfProfile[u] > fBestMax) { fBestMax = pfProfile[u]; iBestMax = i; }
			if (pfProfile[u] >= 0.5f)
			{
				const float fHere = BinCentre(u, fLoY, fHiY, uSCAN_BINS);
				if (iPrev < 0) { fOut = fHere; return true; }
				// Linear between the straddling bins: a loft's blend ramp is
				// linear in Y, so this recovers the authored ring rather than the
				// bin it landed in.
				const u_int uPrev = static_cast<u_int>(iPrev);
				const float fThere = BinCentre(uPrev, fLoY, fHiY, uSCAN_BINS);
				const float fD = pfProfile[uPrev] - pfProfile[u];
				const float fT = (std::fabs(fD) > 1.0e-6f) ? ((0.5f - pfProfile[u]) / -fD) : 0.0f;
				fOut = fHere + std::clamp(fT, 0.0f, 1.0f) * (fThere - fHere);
				return true;
			}
			iPrev = i;
		}
		if (iBestMax < 0) { return false; }
		fOut = BinCentre(static_cast<u_int>(iBestMax), fLoY, fHiY, uSCAN_BINS);
		return true;
	}

	// The arm chain of a mesh whose arms HANG.
	//
	// ★ EVERY JOINT COMES FROM THE MESH'S OWN SKIN WEIGHTS, not from its shape.
	// That is the whole trick here, and it took three wrong shapes to arrive at.
	// A hanging arm has no abrupt feature at the shoulder -- it tapers smoothly
	// from wrist to deltoid, so "the radius blows up past 1.6x the median" fired
	// 16 cm low. Its own centreline turning inboard is better but reads the top of
	// the STRAIGHT part, not the joint. And a bounded radius minimum for the wrist
	// competes with four splayed fingers, which are thinner across the limb than
	// any wrist is.
	//
	// A loft has already answered all three questions: the ring whose weights are
	// 50/50 across a joint IS that joint, by the author's own statement. Reading
	// the crossing recovers StickFigure's authored shoulder (1.100) to 0.5 cm, its
	// elbow (0.700) to 1.5 cm and its wrist (0.400) to 2 cm -- an independent
	// check no radius heuristic came close to passing.
	bool MeasureArmChainArmsDown(const Zenith_SkinDeformView& xView, u_int64 ulArmMask,
		float fCrownY, Zenith_HumanLandmarks& xOut)
	{
		if (!xView.HasSkinning()) { return false; }

		float fArmMinY = 0.0f, fArmMaxY = 0.0f;
		bool bAny = false;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (ArmWeightOf(xView, v, ulArmMask) <= fARM_WEIGHT_EPSILON) { continue; }
			const float fY = xView.m_pxPositions[v].y;
			if (!bAny) { fArmMinY = fArmMaxY = fY; bAny = true; }
			else { fArmMinY = std::min(fArmMinY, fY); fArmMaxY = std::max(fArmMaxY, fY); }
		}
		if (!bAny || (fArmMaxY - fArmMinY) < 1.0e-4f) { return false; }
		fArmMaxY = std::min(fArmMaxY, fCrownY);

		float afProfile[uSCAN_BINS];
		u_int auCounts[uSCAN_BINS];

		float fShoulderY = 0.0f, fElbowY = 0.0f, fWristY = 0.0f;
		BuildDistalWeightProfile(xView, ulArmMask, ulArmMask, fArmMinY, fArmMaxY, afProfile, auCounts, false);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fArmMinY, fArmMaxY, fShoulderY)) { return false; }
		BuildDistalWeightProfile(xView, ulArmMask, ulZENITH_STICKFIGURE_BELOW_ELBOW_MASK,
			fArmMinY, fArmMaxY, afProfile, auCounts, false);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fArmMinY, fArmMaxY, fElbowY)) { return false; }
		BuildDistalWeightProfile(xView, ulArmMask, ulZENITH_STICKFIGURE_BELOW_WRIST_MASK,
			fArmMinY, fArmMaxY, afProfile, auCounts, false);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fArmMinY, fArmMaxY, fWristY)) { return false; }

		const float fFingertipY = fArmMinY;
		if (fShoulderY <= fElbowY || fElbowY <= fWristY || fWristY < fFingertipY)
		{
			return false;   // an arm whose joints are out of order is not an arm
		}

		// A hand is only a hand if there is meaningfully more arm below the wrist
		// than measurement noise. 5% of the limb is about a knuckle.
		xOut.m_bArmHasHand = (fWristY - fFingertipY) > 0.05f * (fShoulderY - fFingertipY);

		xOut.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] = fFingertipY;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] = fWristY;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW] = fElbowY;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] = fShoulderY;
		xOut.m_bArmChainFound = true;

		xOut.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER] = fShoulderY;
		xOut.m_abBodyFound[ZENITH_HUMAN_BODY_SHOULDER] = true;

		// The half-width is the arm COLUMN's own X -- the line a hanging arm's
		// joint sits on, which is what a bone needs. The deltoid RING's centre is
		// 5 cm further inboard (the deltoid slopes out from the torso to meet the
		// column), and the deltoid's outer SURFACE is 6 cm further out; neither is
		// where a shoulder rotates. Taken over the lower 70% so the slope is out
		// of the sample.
		SliceStat axBins[uSCAN_BINS];
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (ArmWeightOf(xView, v, ulArmMask) <= fARM_WEIGHT_EPSILON) { continue; }
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (xP.x <= 0.0f) { continue; }
			axBins[BinOf(xP.y, fArmMinY, fArmMaxY, uSCAN_BINS)].Add(xP.x, xP.z);
		}
		const u_int uColumnHi = static_cast<u_int>(0.70f * static_cast<float>(uSCAN_BINS - 1u));
		float afColumn[uSCAN_BINS];
		u_int uColumnN = 0u;
		for (u_int u = 0u; u <= uColumnHi; ++u)
		{
			if (axBins[u].m_uCount > 0u) { afColumn[uColumnN++] = axBins[u].MeanAbsX(); }
		}
		if (uColumnN == 0u) { return false; }
		std::sort(afColumn, afColumn + uColumnN);
		xOut.m_fShoulderHalfX = afColumn[uColumnN / 2u];
		return xOut.m_fShoulderHalfX > 1.0e-4f;
	}

	// ** A T-POSED MESH THAT ALREADY HAS WEIGHTS SHOULD BE READ FROM THEM. The
	// radius scan below exists for an artist's UNRIGGED import, where measuring is
	// what makes weights possible in the first place -- but StickFigure and
	// Zenithmon's humans are T-posed AND skinned, and for them the crossing is
	// simply the better instrument. It is the same reading the arms-down path uses,
	// along the lateral axis instead of the vertical one: the ring whose weights
	// are 50/50 across a joint IS that joint, by the author's own statement, with
	// no band to tune and no radius feature to hope for.
	//
	// * THE DIFFERENCE IS NOT SMALL. On the shipped StickFigure the radius sweep
	// puts the elbow 11 cm and the wrist 9 cm further out than the bone they are
	// supposed to coincide with -- enough to fail a 5 cm tolerance and, more to the
	// point, enough for a warp anchored on it to bend the arm in the wrong place.
	bool MeasureArmChainTPoseFromWeights(const Zenith_SkinDeformView& xView, u_int64 ulArmMask,
		Zenith_HumanLandmarks& xOut)
	{
		if (!xView.HasSkinning()) { return false; }

		// Binned on -x so "bigger" means "nearer the body" on this axis too, which
		// is what lets the crossing scan be shared verbatim between the two poses.
		float fLo = 0.0f, fHi = 0.0f, fSumY = 0.0f;
		u_int uN = 0u;
		bool bAny = false;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (ArmWeightOf(xView, v, ulArmMask) <= fARM_WEIGHT_EPSILON) { continue; }
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (xP.x <= 0.0f) { continue; }
			const float fN = -xP.x;
			if (!bAny) { fLo = fHi = fN; bAny = true; }
			else { fLo = std::min(fLo, fN); fHi = std::max(fHi, fN); }
			fSumY += xP.y;
			++uN;
		}
		if (!bAny || uN == 0u || (fHi - fLo) < 1.0e-4f) { return false; }

		float afProfile[uSCAN_BINS];
		u_int auCounts[uSCAN_BINS];
		float fShoulderN = 0.0f, fElbowN = 0.0f, fWristN = 0.0f;
		BuildDistalWeightProfile(xView, ulArmMask, ulArmMask, fLo, fHi, afProfile, auCounts, true);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fLo, fHi, fShoulderN)) { return false; }
		BuildDistalWeightProfile(xView, ulArmMask, ulZENITH_STICKFIGURE_BELOW_ELBOW_MASK,
			fLo, fHi, afProfile, auCounts, true);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fLo, fHi, fElbowN)) { return false; }
		BuildDistalWeightProfile(xView, ulArmMask, ulZENITH_STICKFIGURE_BELOW_WRIST_MASK,
			fLo, fHi, afProfile, auCounts, true);
		if (!FindJointByWeightCrossing(afProfile, auCounts, fLo, fHi, fWristN)) { return false; }

		// Back onto the lateral axis, which is how the T_POSE chain is expressed.
		const float fShoulderX = -fShoulderN;
		const float fElbowX = -fElbowN;
		const float fWristX = -fWristN;
		const float fTipX = -fLo;
		if (fShoulderX >= fElbowX || fElbowX >= fWristX || fWristX > fTipX)
		{
			return false;   // an arm whose joints are out of order is not an arm
		}

		xOut.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] = fShoulderX;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW] = fElbowX;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] = fWristX;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] = fTipX;
		xOut.m_bArmChainFound = true;
		xOut.m_bArmHasHand = (fTipX - fWristX) > 0.05f * (fTipX - fShoulderX);

		xOut.m_fShoulderHalfX = fShoulderX;
		// The arm's own centreline height IS the shoulder's height in a T-pose.
		xOut.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER] = fSumY / static_cast<float>(uN);
		xOut.m_abBodyFound[ZENITH_HUMAN_BODY_SHOULDER] = true;
		return true;
	}

	// The arm chain of a T-posed mesh with NO weights -- an artist's import, where
	// measuring is what makes weights possible. The arm is isolated by the only
	// thing that distinguishes it: it reaches further sideways than anything else.
	bool MeasureArmChainTPose(const Zenith_SkinDeformView& xView, float fHeight,
		Zenith_HumanLandmarks& xOut)
	{
		float fMaxAbsX = 0.0f;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			fMaxAbsX = std::max(fMaxAbsX, std::fabs(xView.m_pxPositions[v].x));
		}
		if (fMaxAbsX <= 1.0e-4f) { return false; }

		// Pass 1 -- the centreline, from the outer 45% which can only be arm.
		float fSumY = 0.0f;
		u_int uN = 0u;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (std::fabs(xView.m_pxPositions[v].x) > 0.55f * fMaxAbsX) { fSumY += xView.m_pxPositions[v].y; ++uN; }
		}
		if (uN == 0u) { return false; }
		const float fCentreY = fSumY / static_cast<float>(uN);

		// Pass 2 -- the radius profile along the limb, restricted to the
		// centreline band so the torso's full height cannot leak into it.
		SliceStat axBins[uSCAN_BINS];
		const float fBand = 0.10f * fHeight;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (std::fabs(xP.y - fCentreY) > fBand) { continue; }
			axBins[BinOf(std::fabs(xP.x), 0.0f, fMaxAbsX, uSCAN_BINS)].Add(xP.y, xP.z);
		}

		const u_int uArmOnlyLo = static_cast<u_int>(0.35f * static_cast<float>(uSCAN_BINS));
		const float fMedian = MedianRadius(axBins, uArmOnlyLo, uSCAN_BINS - 1u);
		if (fMedian <= 0.0f) { return false; }

		bool bFound = false;
		float fShoulderHalfX = 0.0f;
		for (int i = static_cast<int>(uArmOnlyLo); i >= 0; --i)
		{
			const u_int u = static_cast<u_int>(i);
			if (axBins[u].m_uCount == 0u) { continue; }
			if (axBins[u].Radius() > 1.6f * fMedian)
			{
				fShoulderHalfX = BinCentre(u, 0.0f, fMaxAbsX, uSCAN_BINS);
				bFound = true;
				break;
			}
		}
		if (!bFound) { return false; }

		const float fLimb = fMaxAbsX - fShoulderHalfX;
		if (fLimb <= 1.0e-4f) { return false; }

		// Same band as the arms-down scan, for the same reason (see there).
		const u_int uLo = BinOf(fShoulderHalfX + 0.58f * fLimb, 0.0f, fMaxAbsX, uSCAN_BINS);
		const u_int uHi = BinOf(fShoulderHalfX + 0.82f * fLimb, 0.0f, fMaxAbsX, uSCAN_BINS);
		u_int uWristBin = 0u;
		const float fWristX = FindInteriorRadiusMinimum(axBins, uLo, uHi, uWristBin)
			? BinCentre(uWristBin, 0.0f, fMaxAbsX, uSCAN_BINS)
			: (fShoulderHalfX + 0.72f * fLimb);

		xOut.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] = fShoulderHalfX;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW] = 0.5f * (fShoulderHalfX + fWristX);
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] = fWristX;
		xOut.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] = fMaxAbsX;
		xOut.m_bArmChainFound = true;
		xOut.m_bArmHasHand = (fMaxAbsX - fWristX) > 0.05f * (fMaxAbsX - fShoulderHalfX);

		xOut.m_fShoulderHalfX = fShoulderHalfX;
		xOut.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER] = fCentreY;
		xOut.m_abBodyFound[ZENITH_HUMAN_BODY_SHOULDER] = true;
		return true;
	}
}

bool Zenith_MeasureHumanLandmarks(const Zenith_SkinDeformView& xView, ZENITH_HUMAN_POSE ePose,
	Zenith_HumanLandmarks& xOut)
{
	xOut = Zenith_HumanLandmarks();
	xOut.m_ePose = ePose;
	if (!xView.IsValid()) { return false; }

	//--- Sole and crown. The only two that cannot fail on a real mesh.
	float fSoleY = xView.m_pxPositions[0].y;
	float fCrownY = fSoleY;
	for (u_int v = 1u; v < xView.m_uNumVerts; ++v)
	{
		const float fY = xView.m_pxPositions[v].y;
		fSoleY = std::min(fSoleY, fY);
		fCrownY = std::max(fCrownY, fY);
	}
	const float fHeight = fCrownY - fSoleY;
	if (fHeight <= 1.0e-4f) { return false; }

	xOut.m_afBodyY[ZENITH_HUMAN_BODY_SOLE] = fSoleY;
	xOut.m_abBodyFound[ZENITH_HUMAN_BODY_SOLE] = true;
	xOut.m_afBodyY[ZENITH_HUMAN_BODY_CROWN] = fCrownY;
	xOut.m_abBodyFound[ZENITH_HUMAN_BODY_CROWN] = true;

	const u_int64 ulArmMask = ulZENITH_STICKFIGURE_ARM_BONE_MASK;

	//--- The arm, which also settles the shoulder.
	const bool bArm = (ePose == ZENITH_HUMAN_POSE_T_POSE)
		? (MeasureArmChainTPoseFromWeights(xView, ulArmMask, xOut) ||
		   MeasureArmChainTPose(xView, fHeight, xOut))
		: MeasureArmChainArmsDown(xView, ulArmMask, fCrownY, xOut);
	(void)bArm;   // an armless mesh is still measurable for the body chain

	//--- The leg profile: one leg only, arms excluded, over the whole body.
	SliceStat axLeg[uSCAN_BINS];
	for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
	{
		if (ArmWeightOf(xView, v, ulArmMask) > fARM_WEIGHT_EPSILON) { continue; }
		const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
		if (xP.x <= 0.0f) { continue; }
		axLeg[BinOf(xP.y, fSoleY, fCrownY, uSCAN_BINS)].Add(xP.x, xP.z);
	}

	//--- Ankle: the leg's radius minimum in the lower 25%, and only there.
	{
		const u_int uHi = BinOf(fSoleY + 0.25f * fHeight, fSoleY, fCrownY, uSCAN_BINS);
		u_int uBin = 0u;
		if (FindInteriorRadiusMinimum(axLeg, 0u, uHi, uBin))
		{
			xOut.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE] = BinCentre(uBin, fSoleY, fCrownY, uSCAN_BINS);
			xOut.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE] = true;
		}
	}

	//--- Knee: the MIDPOINT of the hip plane and the ankle, and deliberately not a
	//    search. There is no trustworthy knee feature on a smooth leg -- the male's
	//    cross-section dips to 0.0389 against 0.0398 and 0.0400 either side, and
	//    StickFigure's dips 1.5%, which is subdivision noise. Searching it found a
	//    "knee" 8 cm from the authored one; the midpoint is deterministic,
	//    anthropometrically standard, and lands within 0.003 of the male's measured
	//    minimum.
	//
	//    ★ THE UPPER END IS THE RIG'S HIP PLANE, NOT A MEASURED CROTCH. A crotch
	//    scan is a two-cluster test, and a LOFT is a hollow shell: its torso rings
	//    carry no vertex at x = 0, so every slice reads as "two clusters" and the
	//    scan reports the top of the legs. Making the test robust for the loft
	//    breaks it for an artist mesh, whose thighs part by a hundredth of a
	//    height. The plane both sides agree on is Root, and Root is pinned.
	{
		const float fLegLo = xOut.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE]
			? xOut.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE] : fSoleY;
		const float fHipPlane = Zenith_HumanProportionsLegacy().HipY();
		if (fHipPlane > fLegLo + 1.0e-4f)
		{
			xOut.m_afBodyY[ZENITH_HUMAN_BODY_KNEE] = 0.5f * (fLegLo + fHipPlane);
			xOut.m_abBodyFound[ZENITH_HUMAN_BODY_KNEE] = true;
		}
	}

	//--- Hip half-width: the thigh's centreline, mid-way up the femur.
	if (xOut.m_abBodyFound[ZENITH_HUMAN_BODY_KNEE])
	{
		const float fKnee = xOut.m_afBodyY[ZENITH_HUMAN_BODY_KNEE];
		const float fTop = Zenith_HumanProportionsLegacy().HipY();
		SliceStat xThigh;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			if (ArmWeightOf(xView, v, ulArmMask) > fARM_WEIGHT_EPSILON) { continue; }
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (xP.x <= 0.0f) { continue; }
			if (xP.y < fKnee + 0.35f * (fTop - fKnee) || xP.y > fKnee + 0.75f * (fTop - fKnee)) { continue; }
			xThigh.Add(xP.x, xP.z);
		}
		xOut.m_fHipHalfX = xThigh.MeanAbsX();
	}

	//--- Neck: the narrowest point of the central column ABOVE the shoulder.
	if (xOut.m_abBodyFound[ZENITH_HUMAN_BODY_SHOULDER] && xOut.m_fShoulderHalfX > 0.0f)
	{
		const float fShoulderY = xOut.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER];
		SliceStat axCol[uSCAN_BINS];
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (std::fabs(xP.x) > 0.60f * xOut.m_fShoulderHalfX) { continue; }
			axCol[BinOf(xP.y, fSoleY, fCrownY, uSCAN_BINS)].Add(xP.x, xP.z);
		}
		// ★ BOUNDED ABOVE BY THE HEAD PLANE, not by the crown. A skull tapers to a
		// point at the top, so an unbounded "narrowest thing above the shoulder"
		// finds the CROWN -- and because that is the last bin, the interior-minimum
		// test then rejects it and the neck is reported missing rather than wrong.
		// A neck is between a shoulder and a head by definition; say so.
		const u_int uLo = BinOf(fShoulderY, fSoleY, fCrownY, uSCAN_BINS);
		const u_int uNeckHi = BinOf(Zenith_HumanProportionsLegacy().HeadY(), fSoleY, fCrownY, uSCAN_BINS);
		u_int uBin = 0u;
		if (FindInteriorRadiusMinimum(axCol, uLo, uNeckHi, uBin))
		{
			xOut.m_afBodyY[ZENITH_HUMAN_BODY_NECK] = BinCentre(uBin, fSoleY, fCrownY, uSCAN_BINS);
			xOut.m_abBodyFound[ZENITH_HUMAN_BODY_NECK] = true;
		}
	}

	//--- The foot, and with it WHICH WAY THE BODY FACES.
	//
	// ★★ A HEEL IS TALL AND A TOE IS THIN, so the extremes of the foot slab are
	// not interchangeable and "which is the toe" is a MEASUREMENT, not a naming
	// convention. This used to read max-Z as the toe by definition, which meant it
	// agreed with whatever the caller already believed and could never say
	// otherwise -- and a 180-degree-wrong character duly shipped through a
	// screenshot pass, because at head-thumbnail size the back of a head reads as
	// a face.
	//
	// One leg only (x > 0): the two feet sit at opposite X and merging them says
	// nothing about either.
	{
		const float fCut = xOut.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE]
			? xOut.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE] : (fSoleY + 0.06f * fHeight);

		float fMinZ = 0.0f, fMaxZ = 0.0f;
		bool bAny = false;
		for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
		{
			const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
			if (xP.y >= fCut || xP.x <= 0.0f) { continue; }
			if (!bAny) { fMinZ = fMaxZ = xP.z; bAny = true; }
			else { fMinZ = std::min(fMinZ, xP.z); fMaxZ = std::max(fMaxZ, xP.z); }
		}

		if (bAny && (fMaxZ - fMinZ) > 1.0e-4f)
		{
			// ★★ THE ANKLE SITS AT THE BACK OF THE FOOT, and that 3:1 lever is the
			// discriminator -- NOT which end is taller.
			//
			// "The heel is the taller end" is true of a bare foot and false of a
			// trainer with a built-up toe box, and it got this exactly wrong on the
			// artist mesh while getting it right on the generated loft. Two meshes,
			// opposite answers, no way to tell from inside the test. How far the
			// foot reaches PAST THE LEG is anatomy rather than footwear: measured
			// from the shin's own axis, a foot runs about three times further
			// forward than back, on everybody, in every shoe.
			//
			// The shin band is taken just ABOVE the ankle, so it is leg and not
			// foot, and one leg only -- the two sit at opposite X and averaging
			// them describes neither.
			float fShinSum = 0.0f;
			u_int fShinCount = 0u;
			const float fShinLo = fCut;
			const float fShinHi = fCut + 0.10f * fHeight;
			for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
			{
				const Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
				if (xP.x <= 0.0f || xP.y < fShinLo || xP.y > fShinHi) { continue; }
				fShinSum += xP.z;
				++fShinCount;
			}
			if (fShinCount > 0u)
			{
				const float fShinZ = fShinSum / static_cast<float>(fShinCount);
				const float fForward = fMaxZ - fShinZ;   // reach toward +Z
				const float fBack = fShinZ - fMinZ;      // reach toward -Z
				const float fBigger = std::max(fForward, fBack);
				// A real foot is 3:1; anything under 1.4:1 is not a foot sticking
				// out of a leg, and the honest answer there is "unmeasured".
				if (fBigger > 1.0e-5f && std::fabs(fForward - fBack) > 0.29f * fBigger)
				{
					xOut.m_fFacingSign = (fForward > fBack) ? 1.0f : -1.0f;
					xOut.m_bFootFacingMeasured = true;
					xOut.m_fToeZ = (fForward > fBack) ? fMaxZ : fMinZ;
					xOut.m_fHeelZ = (fForward > fBack) ? fMinZ : fMaxZ;
				}
			}
			if (!xOut.m_bFootFacingMeasured)
			{
				xOut.m_fHeelZ = fMinZ;
				xOut.m_fToeZ = fMaxZ;
			}
		}
	}

	xOut.m_bValid = true;
	return true;
}

void Zenith_LogHumanLandmarks(const char* szWho, const Zenith_HumanLandmarks& xLandmarks)
{
	if (!xLandmarks.m_bValid)
	{
		Zenith_Warning(LOG_CATEGORY_ASSET, "[HumanLandmarks] %s: INVALID (degenerate scan)", szWho);
		return;
	}
	Zenith_Log(LOG_CATEGORY_ASSET, "[HumanLandmarks] %s: pose=%s sole=%.6f crown=%.6f height=%.6f",
		szWho, (xLandmarks.m_ePose == ZENITH_HUMAN_POSE_T_POSE) ? "T-POSE" : "ARMS-DOWN",
		xLandmarks.SoleY(), xLandmarks.CrownY(), xLandmarks.Height());

	for (u_int u = 0u; u < ZENITH_HUMAN_BODY_ANCHOR_COUNT; ++u)
	{
		if (xLandmarks.m_abBodyFound[u])
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "[HumanLandmarks]   %-9s y=%+.6f  frac=%.4f",
				szBODY_ANCHOR_NAMES[u], xLandmarks.m_afBodyY[u], xLandmarks.Frac(xLandmarks.m_afBodyY[u]));
		}
		else if (u == ZENITH_HUMAN_BODY_HIP || u == ZENITH_HUMAN_BODY_HEAD)
		{
			// Never measured, by design: both are PINNED planes the warp supplies
			// on both sides. See Zenith_MakeHumanWarp.
			Zenith_Log(LOG_CATEGORY_ASSET, "[HumanLandmarks]   %-9s PINNED (rig plane, not measured)",
				szBODY_ANCHOR_NAMES[u]);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "[HumanLandmarks]   %-9s NOT PRESENT ON THIS MESH", szBODY_ANCHOR_NAMES[u]);
		}
	}
	if (xLandmarks.m_bArmChainFound)
	{
		for (u_int u = 0u; u < ZENITH_HUMAN_ARM_ANCHOR_COUNT; ++u)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "[HumanLandmarks]   arm.%-9s %+.6f  frac=%.4f",
				szARM_ANCHOR_NAMES[u], xLandmarks.m_afArmChain[u],
				(xLandmarks.m_ePose == ZENITH_HUMAN_POSE_T_POSE)
					? (xLandmarks.m_afArmChain[u] / xLandmarks.Height())
					: xLandmarks.Frac(xLandmarks.m_afArmChain[u]));
		}
	}
	Zenith_Log(LOG_CATEGORY_ASSET,
		"[HumanLandmarks]   facing=%s (toes +Z is FORWARD)",
		xLandmarks.m_bFootFacingMeasured
			? ((xLandmarks.m_fFacingSign > 0.0f) ? "FORWARD (+Z) - correct" : "BACKWARDS (-Z) - the orienter must turn it")
			: "not measurable from this foot");
	Zenith_Log(LOG_CATEGORY_ASSET,
		"[HumanLandmarks]   shoulderHalfX=%.6f (frac %.4f)  hipHalfX=%.6f (frac %.4f)  toeZ=%+.4f heelZ=%+.4f  hand=%s  "
		"| rig ankleHeightAboveSole=%.6f  <-- THIS is RenderTest's k_fAnkleHeight",
		xLandmarks.m_fShoulderHalfX, xLandmarks.m_fShoulderHalfX / xLandmarks.Height(),
		xLandmarks.m_fHipHalfX, xLandmarks.m_fHipHalfX / xLandmarks.Height(),
		xLandmarks.m_fToeZ, xLandmarks.m_fHeelZ,
		xLandmarks.m_bArmHasHand ? "yes" : "NO (arm ends at the wrist)",
		Zenith_HumanProportionsRealistic().AnkleHeightAboveSole());
}

//==============================================================================
// The warp
//==============================================================================

bool Zenith_HumanWarp::IsMonotonic() const
{
	for (u_int u = 0u; u + 1u < m_uNumBodyAnchors; ++u)
	{
		if (m_afBodySrcY[u + 1u] <= m_afBodySrcY[u]) { return false; }
		if (m_afBodyDstY[u + 1u] <= m_afBodyDstY[u]) { return false; }
	}
	for (u_int u = 0u; u + 1u < m_uNumArmAnchors; ++u)
	{
		if (m_afArmSrcY[u + 1u] <= m_afArmSrcY[u]) { return false; }
		if (m_afArmDstY[u + 1u] <= m_afArmDstY[u]) { return false; }
	}
	return true;
}

float Zenith_HumanWarp::MapY(float fY, float fArmWeight) const
{
	const float fBody = PiecewiseLinear(m_afBodySrcY, m_afBodyDstY, m_uNumBodyAnchors, fY);
	if (m_uNumArmAnchors < 2u || fArmWeight <= 0.0f)
	{
		return fBody;
	}
	const float fArm = PiecewiseLinear(m_afArmSrcY, m_afArmDstY, m_uNumArmAnchors, fY);
	const float fW = std::clamp(fArmWeight, 0.0f, 1.0f);
	return fBody + (fArm - fBody) * fW;
}

float Zenith_HumanWarp::ArmWeight(const glm::uvec4& xIndices, const glm::vec4& xWeights) const
{
	float fSum = 0.0f;
	for (int i = 0; i < 4; ++i)
	{
		const u_int uBone = xIndices[i];
		if (uBone < 64u && ((m_ulArmBoneMask >> uBone) & 1ull) != 0ull) { fSum += xWeights[i]; }
	}
	return std::clamp(fSum, 0.0f, 1.0f);
}

bool Zenith_MakeHumanWarp(const Zenith_HumanLandmarks& xMeasured,
	const Zenith_HumanProportions& xTo, Zenith_HumanWarp& xOut)
{
	xOut = Zenith_HumanWarp();
	xOut.m_ulArmBoneMask = ulZENITH_STICKFIGURE_ARM_BONE_MASK;

	if (!xMeasured.m_bValid || xMeasured.m_ePose != ZENITH_HUMAN_POSE_ARMS_DOWN)
	{
		// A T-pose measurement describes a mesh that is about to be RE-BOUND, not
		// re-proportioned. Warping from one would map a sideways arm through the
		// body's height map.
		return false;
	}
	if (!xTo.IsOrdered()) { return false; }

	const float fSole = xMeasured.SoleY();
	const float fCrown = xMeasured.CrownY();

	//--- Body chain. Source = this mesh's own measured landmark; target = the
	//    RIG's plane, except at the two pinned endpoints.
	auto AppendBody = [&xOut](float fSrc, float fDst)
	{
		xOut.m_afBodySrcY[xOut.m_uNumBodyAnchors] = fSrc;
		xOut.m_afBodyDstY[xOut.m_uNumBodyAnchors] = fDst;
		++xOut.m_uNumBodyAnchors;
	};

	AppendBody(fSole, fSole);   // PINNED: bounds, colliders and spawn lifts are tuned against it
	if (xMeasured.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE])
	{
		AppendBody(xMeasured.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], xTo.AnkleY());
	}
	if (xMeasured.m_abBodyFound[ZENITH_HUMAN_BODY_KNEE])
	{
		AppendBody(xMeasured.m_afBodyY[ZENITH_HUMAN_BODY_KNEE], xTo.KneeY());
	}
	// The hip is a PIN, not a measurement. See MeasureCrotch: a loft's two-cluster
	// scan reports its torso's bottom cap, so mapping that onto the rig's hip
	// would haul the crotch up by 12 cm on a mesh whose silhouette never split
	// there. The Root bone does not move, so neither does this plane.
	AppendBody(xTo.HipY(), xTo.HipY());
	if (xMeasured.m_abBodyFound[ZENITH_HUMAN_BODY_SHOULDER])
	{
		AppendBody(xMeasured.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER], xTo.ShoulderY());
	}
	if (xMeasured.m_abBodyFound[ZENITH_HUMAN_BODY_NECK])
	{
		AppendBody(xMeasured.m_afBodyY[ZENITH_HUMAN_BODY_NECK], xTo.NeckY());
	}
	// The head plane is pinned on BOTH sides, which is what keeps the skull rigid
	// -- the segment above it is the identity, so the authored head size survives
	// the whole warp and the neck below absorbs the shoulder's descent. Dropped
	// rather than forced if it would break the ordering.
	{
		const float fHead = xTo.HeadY();
		const float fPrevSrc = xOut.m_afBodySrcY[xOut.m_uNumBodyAnchors - 1u];
		const float fPrevDst = xOut.m_afBodyDstY[xOut.m_uNumBodyAnchors - 1u];
		if (fHead > fPrevSrc && fHead > fPrevDst && fHead < fCrown)
		{
			AppendBody(fHead, fHead);
		}
	}
	AppendBody(fCrown, fCrown);   // PINNED

	//--- Arm chain. Ascending by construction of the enum.
	if (xMeasured.m_bArmChainFound)
	{
		const float afDst[ZENITH_HUMAN_ARM_ANCHOR_COUNT] =
		{
			xTo.FingertipY(), xTo.WristY(), xTo.ElbowY(), xTo.ShoulderY()
		};
		// The fingertip anchor is dropped for an arm with no hand (see
		// m_bArmHasHand). The chain then ends at the wrist and the map
		// EXTRAPOLATES below it along the wrist-to-elbow slope, which puts that
		// arm's end cap just below the rig's wrist -- where it belongs.
		const u_int uFirst = xMeasured.m_bArmHasHand ? 0u : 1u;
		for (u_int u = uFirst; u < ZENITH_HUMAN_ARM_ANCHOR_COUNT; ++u)
		{
			xOut.m_afArmSrcY[xOut.m_uNumArmAnchors] = xMeasured.m_afArmChain[u];
			xOut.m_afArmDstY[xOut.m_uNumArmAnchors] = afDst[u];
			++xOut.m_uNumArmAnchors;
		}

		if (xMeasured.m_fShoulderHalfX > 0.0f)
		{
			xOut.m_fLateralShiftX = xTo.ShoulderHalfX() - xMeasured.m_fShoulderHalfX;
		}
	}

	// ★ ASSERTED ON THE REAL ARRAYS, at construction, not assumed of the idea of
	// them. This is the check that catches a sole sitting ABOVE the bone it was
	// derived from, and it fires before a single vertex has moved.
	if (!xOut.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ASSET,
			"[HumanWarp] anchor arrays are not monotonic (%u body, %u arm) - refusing to warp",
			xOut.m_uNumBodyAnchors, xOut.m_uNumArmAnchors);
		xOut = Zenith_HumanWarp();
		return false;
	}
	return true;
}

bool Zenith_SkinWarpVertices(const Zenith_SkinDeformView& xView, const Zenith_HumanWarp& xWarp)
{
	if (!xView.IsValid() || !xWarp.IsValid()) { return false; }

	for (u_int v = 0u; v < xView.m_uNumVerts; ++v)
	{
		Zenith_Maths::Vector3& xP = xView.m_pxPositions[v];
		const float fArmW = xView.HasSkinning()
			? xWarp.ArmWeight(xView.m_pxBoneIndices[v], xView.m_pxBoneWeights[v])
			: 0.0f;
		xP.y = xWarp.MapY(xP.y, fArmW);
		if (xP.x != 0.0f)
		{
			xP.x += xWarp.m_fLateralShiftX * ((xP.x > 0.0f) ? 1.0f : -1.0f) * fArmW;
		}
	}
	// NORMALS ARE NOW WRONG, deliberately. This form is only legal where they are
	// rebuilt afterwards; the header says which call sites those are.
	return true;
}

bool Zenith_SkinWarpRing(float& fY, float& fCx, float fArmWeight, const Zenith_HumanWarp& xWarp)
{
	if (!xWarp.IsValid()) { return false; }
	fY = xWarp.MapY(fY, fArmWeight);
	if (fCx != 0.0f)
	{
		fCx += xWarp.m_fLateralShiftX * ((fCx > 0.0f) ? 1.0f : -1.0f) * std::clamp(fArmWeight, 0.0f, 1.0f);
	}
	return true;
}

//==============================================================================
// The rebind
#include "AssetHandling/Zenith_SkinDeform.Tests.inl"
