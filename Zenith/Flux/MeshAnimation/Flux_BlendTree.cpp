#include "Zenith.h"
#include "Flux_BlendTree.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Core/Zenith_Core.h"
#include <algorithm>

//=============================================================================
// Flux_BlendTreeNode - Factory
//=============================================================================
Flux_BlendTreeNode* Flux_BlendTreeNode::CreateFromTypeName(const std::string& strTypeName)
{
	if (strTypeName == "Clip")
		return new Flux_BlendTreeNode_Clip();
	if (strTypeName == "Blend")
		return new Flux_BlendTreeNode_Blend();
	if (strTypeName == "BlendSpace1D")
		return new Flux_BlendTreeNode_BlendSpace1D();
	if (strTypeName == "BlendSpace2D")
		return new Flux_BlendTreeNode_BlendSpace2D();
	if (strTypeName == "Additive")
		return new Flux_BlendTreeNode_Additive();
	if (strTypeName == "Masked")
		return new Flux_BlendTreeNode_Masked();
	if (strTypeName == "Select")
		return new Flux_BlendTreeNode_Select();

	Zenith_Log(LOG_CATEGORY_ANIMATION, "[BlendTree] Unknown node type: %s", strTypeName.c_str());
	return nullptr;
}

//=============================================================================
// Flux_BlendTreeNode_Clip
//=============================================================================
Flux_BlendTreeNode_Clip::Flux_BlendTreeNode_Clip(Flux_AnimationClip* pxClip, float fPlaybackRate)
	: m_pxClip(pxClip)
	, m_fPlaybackRate(fPlaybackRate)
	, m_fCurrentTimestamp(0.0f)
{
	if (pxClip)
		m_strClipName = pxClip->GetName();
}

void Flux_BlendTreeNode_Clip::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	if (!m_pxClip)
	{
		// No clip, output identity pose
		xOutPose.Reset();
		return;
	}

	// Advance time
	m_fCurrentTimestamp += fDt * m_fPlaybackRate;

	// Handle looping
	float fDuration = m_pxClip->GetDuration();
	if (fDuration > 0.0f)
	{
		if (m_pxClip->IsLooping())
		{
			m_fCurrentTimestamp = fmod(m_fCurrentTimestamp, fDuration);
			if (m_fCurrentTimestamp < 0.0f)
				m_fCurrentTimestamp += fDuration;
		}
		else
		{
			m_fCurrentTimestamp = glm::clamp(m_fCurrentTimestamp, 0.0f, fDuration);
		}
	}

	// Initialize output pose with bind pose values from skeleton
	// This ensures bones WITHOUT animation channels keep their bind pose
	// SampleFromClip will only update components that have keyframes
	xOutPose.InitFromBindPose(xSkeleton);

	// Sample the clip - only components with keyframes will be overwritten
	xOutPose.SampleFromClip(*m_pxClip, m_fCurrentTimestamp, xSkeleton);
}

float Flux_BlendTreeNode_Clip::GetNormalizedTime() const
{
	if (!m_pxClip || m_pxClip->GetDuration() <= 0.0f)
		return 0.0f;

	return m_fCurrentTimestamp / m_pxClip->GetDuration();
}

void Flux_BlendTreeNode_Clip::Reset()
{
	m_fCurrentTimestamp = 0.0f;
}

bool Flux_BlendTreeNode_Clip::IsFinished() const
{
	if (!m_pxClip || m_pxClip->IsLooping())
		return false;

	return m_fCurrentTimestamp >= m_pxClip->GetDuration();
}

void Flux_BlendTreeNode_Clip::ResolveClip(Flux_AnimationClipCollection* pxCollection)
{
	if (pxCollection && !m_strClipName.empty())
	{
		m_pxClip = pxCollection->GetClip(m_strClipName);
	}
}

void Flux_BlendTreeNode_Clip::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strClipName;
	xStream << m_fPlaybackRate;
}

void Flux_BlendTreeNode_Clip::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strClipName;
	xStream >> m_fPlaybackRate;
	m_fCurrentTimestamp = 0.0f;
}

//=============================================================================
// Flux_BlendTreeNode_Blend
//=============================================================================
Flux_BlendTreeNode_Blend::Flux_BlendTreeNode_Blend(Flux_BlendTreeNode* pxChildA,
	Flux_BlendTreeNode* pxChildB,
	float fBlendWeight)
	: m_pxChildA(pxChildA)
	, m_pxChildB(pxChildB)
	, m_fBlendWeight(fBlendWeight)
{
}

Flux_BlendTreeNode_Blend::~Flux_BlendTreeNode_Blend()
{
	delete m_pxChildA;
	delete m_pxChildB;
}

void Flux_BlendTreeNode_Blend::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	// Evaluate both children
	if (m_pxChildA)
		m_pxChildA->Evaluate(fDt, m_xPoseA, xSkeleton);
	else
		m_xPoseA.Reset();

	if (m_pxChildB)
		m_pxChildB->Evaluate(fDt, m_xPoseB, xSkeleton);
	else
		m_xPoseB.Reset();

	// Blend results
	Flux_SkeletonPose::Blend(xOutPose, m_xPoseA, m_xPoseB, m_fBlendWeight);
}

float Flux_BlendTreeNode_Blend::GetNormalizedTime() const
{
	// Return weighted average of child times
	float fTimeA = m_pxChildA ? m_pxChildA->GetNormalizedTime() : 0.0f;
	float fTimeB = m_pxChildB ? m_pxChildB->GetNormalizedTime() : 0.0f;
	return glm::mix(fTimeA, fTimeB, m_fBlendWeight);
}

void Flux_BlendTreeNode_Blend::Reset()
{
	if (m_pxChildA) m_pxChildA->Reset();
	if (m_pxChildB) m_pxChildB->Reset();
}

bool Flux_BlendTreeNode_Blend::IsFinished() const
{
	// Finished when dominant child is finished
	if (m_fBlendWeight < 0.5f)
		return m_pxChildA ? m_pxChildA->IsFinished() : true;
	else
		return m_pxChildB ? m_pxChildB->IsFinished() : true;
}

void Flux_BlendTreeNode_Blend::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_fBlendWeight;

	// Write child A
	bool bHasChildA = (m_pxChildA != nullptr);
	xStream << bHasChildA;
	if (bHasChildA)
	{
		std::string strType = m_pxChildA->GetNodeTypeName();
		xStream << strType;
		m_pxChildA->WriteToDataStream(xStream);
	}

	// Write child B
	bool bHasChildB = (m_pxChildB != nullptr);
	xStream << bHasChildB;
	if (bHasChildB)
	{
		std::string strType = m_pxChildB->GetNodeTypeName();
		xStream << strType;
		m_pxChildB->WriteToDataStream(xStream);
	}
}

void Flux_BlendTreeNode_Blend::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fBlendWeight;

	// Read child A
	bool bHasChildA = false;
	xStream >> bHasChildA;
	if (bHasChildA)
	{
		std::string strType;
		xStream >> strType;
		m_pxChildA = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxChildA)
			m_pxChildA->ReadFromDataStream(xStream);
	}

	// Read child B
	bool bHasChildB = false;
	xStream >> bHasChildB;
	if (bHasChildB)
	{
		std::string strType;
		xStream >> strType;
		m_pxChildB = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxChildB)
			m_pxChildB->ReadFromDataStream(xStream);
	}
}

//=============================================================================
// Flux_BlendTreeNode_BlendSpace1D
//=============================================================================
Flux_BlendTreeNode_BlendSpace1D::~Flux_BlendTreeNode_BlendSpace1D()
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
		delete m_xBlendPoints.Get(u).m_pxNode;
}

void Flux_BlendTreeNode_BlendSpace1D::AddBlendPoint(Flux_BlendTreeNode* pxNode, float fPosition)
{
	m_xBlendPoints.PushBack({ pxNode, fPosition });
}

void Flux_BlendTreeNode_BlendSpace1D::RemoveBlendPoint(u_int uIndex)
{
	if (uIndex < m_xBlendPoints.GetSize())
	{
		delete m_xBlendPoints.Get(uIndex).m_pxNode;
		m_xBlendPoints.Remove(uIndex);
	}
}

void Flux_BlendTreeNode_BlendSpace1D::SortBlendPoints()
{
	std::sort(m_xBlendPoints.GetDataPointer(), m_xBlendPoints.GetDataPointer() + m_xBlendPoints.GetSize(),
		[](const BlendPoint& a, const BlendPoint& b) {
			return a.m_fPosition < b.m_fPosition;
		});
}

void Flux_BlendTreeNode_BlendSpace1D::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	if (m_xBlendPoints.GetSize() == 0)
	{
		xOutPose.Reset();
		return;
	}

	if (m_xBlendPoints.GetSize() == 1)
	{
		if (m_xBlendPoints.Get(0).m_pxNode)
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		return;
	}

	// Find the two blend points to interpolate between
	u_int uLowerIdx = 0;
	u_int uUpperIdx = m_xBlendPoints.GetSize() - 1;

	for (u_int i = 0; i < m_xBlendPoints.GetSize() - 1; ++i)
	{
		if (m_fParameter >= m_xBlendPoints.Get(i).m_fPosition &&
			m_fParameter <= m_xBlendPoints.Get(i + 1).m_fPosition)
		{
			uLowerIdx = i;
			uUpperIdx = i + 1;
			break;
		}
	}

	// Clamp to edges
	if (m_fParameter <= m_xBlendPoints.Get(0).m_fPosition)
	{
		if (m_xBlendPoints.Get(0).m_pxNode)
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		return;
	}

	if (m_fParameter >= m_xBlendPoints.Get(m_xBlendPoints.GetSize() - 1).m_fPosition)
	{
		if (m_xBlendPoints.Get(m_xBlendPoints.GetSize() - 1).m_pxNode)
			m_xBlendPoints.Get(m_xBlendPoints.GetSize() - 1).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		return;
	}

	// Evaluate both points
	if (m_xBlendPoints.Get(uLowerIdx).m_pxNode)
		m_xBlendPoints.Get(uLowerIdx).m_pxNode->Evaluate(fDt, m_xPoseA, xSkeleton);
	else
		m_xPoseA.Reset();

	if (m_xBlendPoints.Get(uUpperIdx).m_pxNode)
		m_xBlendPoints.Get(uUpperIdx).m_pxNode->Evaluate(fDt, m_xPoseB, xSkeleton);
	else
		m_xPoseB.Reset();

	// Calculate blend factor
	float fRange = m_xBlendPoints.Get(uUpperIdx).m_fPosition - m_xBlendPoints.Get(uLowerIdx).m_fPosition;
	float fBlend = (fRange > 0.0f) ?
		(m_fParameter - m_xBlendPoints.Get(uLowerIdx).m_fPosition) / fRange : 0.0f;

	Flux_SkeletonPose::Blend(xOutPose, m_xPoseA, m_xPoseB, fBlend);
}

float Flux_BlendTreeNode_BlendSpace1D::GetNormalizedTime() const
{
	if (m_xBlendPoints.GetSize() == 0)
		return 0.0f;

	// Return time of nearest blend point
	float fMinDist = FLT_MAX;
	const BlendPoint* pxNearest = nullptr;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		float fDist = std::abs(xPoint.m_fPosition - m_fParameter);
		if (fDist < fMinDist)
		{
			fMinDist = fDist;
			pxNearest = &xPoint;
		}
	}

	return pxNearest && pxNearest->m_pxNode ? pxNearest->m_pxNode->GetNormalizedTime() : 0.0f;
}

void Flux_BlendTreeNode_BlendSpace1D::Reset()
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->Reset();
	}
}

void Flux_BlendTreeNode_BlendSpace1D::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_fParameter;

	uint32_t uNumPoints = static_cast<uint32_t>(m_xBlendPoints.GetSize());
	xStream << uNumPoints;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		xStream << xPoint.m_fPosition;

		bool bHasNode = (xPoint.m_pxNode != nullptr);
		xStream << bHasNode;
		if (bHasNode)
		{
			std::string strType = xPoint.m_pxNode->GetNodeTypeName();
			xStream << strType;
			xPoint.m_pxNode->WriteToDataStream(xStream);
		}
	}
}

void Flux_BlendTreeNode_BlendSpace1D::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fParameter;

	uint32_t uNumPoints = 0;
	xStream >> uNumPoints;

	for (uint32_t i = 0; i < uNumPoints; ++i)
	{
		float fPosition = 0.0f;
		xStream >> fPosition;

		bool bHasNode = false;
		xStream >> bHasNode;

		Flux_BlendTreeNode* pxNode = nullptr;
		if (bHasNode)
		{
			std::string strType;
			xStream >> strType;
			pxNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
			if (pxNode)
				pxNode->ReadFromDataStream(xStream);
		}

		m_xBlendPoints.PushBack({ pxNode, fPosition });
	}

	SortBlendPoints();
}

//=============================================================================
// Flux_BlendTreeNode_BlendSpace2D
//=============================================================================
Flux_BlendTreeNode_BlendSpace2D::~Flux_BlendTreeNode_BlendSpace2D()
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
		delete m_xBlendPoints.Get(u).m_pxNode;
}

void Flux_BlendTreeNode_BlendSpace2D::AddBlendPoint(Flux_BlendTreeNode* pxNode,
	const Zenith_Maths::Vector2& xPosition)
{
	m_xBlendPoints.PushBack({ pxNode, xPosition });
}

void Flux_BlendTreeNode_BlendSpace2D::RemoveBlendPoint(u_int uIndex)
{
	if (uIndex < m_xBlendPoints.GetSize())
	{
		delete m_xBlendPoints.Get(uIndex).m_pxNode;
		m_xBlendPoints.Remove(uIndex);
	}
}

void Flux_BlendTreeNode_BlendSpace2D::ComputeTriangulation()
{
	// Simple triangulation for small point sets
	// For production use, implement Delaunay triangulation
	m_xTriangles.Clear();

	if (m_xBlendPoints.GetSize() < 3)
		return;

	// Simple fan triangulation from first point (works for convex hulls)
	// A proper implementation would use Delaunay triangulation
	for (u_int i = 1; i < m_xBlendPoints.GetSize() - 1; ++i)
	{
		m_xTriangles.PushBack({ 0, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1) });
	}
}

bool Flux_BlendTreeNode_BlendSpace2D::FindContainingTriangle(const Zenith_Maths::Vector2& xPoint,
	uint32_t& uOutIdx0, uint32_t& uOutIdx1, uint32_t& uOutIdx2,
	float& fOutW0, float& fOutW1, float& fOutW2) const
{
	for (u_int uTri = 0; uTri < m_xTriangles.GetSize(); uTri++)
	{
		const std::array<uint32_t, 3>& xTri = m_xTriangles.Get(uTri);
		const Zenith_Maths::Vector2& v0 = m_xBlendPoints.Get(xTri[0]).m_xPosition;
		const Zenith_Maths::Vector2& v1 = m_xBlendPoints.Get(xTri[1]).m_xPosition;
		const Zenith_Maths::Vector2& v2 = m_xBlendPoints.Get(xTri[2]).m_xPosition;

		// Compute barycentric coordinates
		Zenith_Maths::Vector2 v0v1 = v1 - v0;
		Zenith_Maths::Vector2 v0v2 = v2 - v0;
		Zenith_Maths::Vector2 v0p = xPoint - v0;

		float d00 = glm::dot(v0v1, v0v1);
		float d01 = glm::dot(v0v1, v0v2);
		float d11 = glm::dot(v0v2, v0v2);
		float d20 = glm::dot(v0p, v0v1);
		float d21 = glm::dot(v0p, v0v2);

		float denom = d00 * d11 - d01 * d01;
		if (std::abs(denom) < 0.0001f)
			continue;

		float v = (d11 * d20 - d01 * d21) / denom;
		float w = (d00 * d21 - d01 * d20) / denom;
		float u = 1.0f - v - w;

		// Check if point is inside triangle (with small tolerance)
		const float fTolerance = -0.01f;
		if (u >= fTolerance && v >= fTolerance && w >= fTolerance)
		{
			uOutIdx0 = xTri[0];
			uOutIdx1 = xTri[1];
			uOutIdx2 = xTri[2];
			fOutW0 = u;
			fOutW1 = v;
			fOutW2 = w;
			return true;
		}
	}

	return false;
}

void Flux_BlendTreeNode_BlendSpace2D::FindNearestPoints(const Zenith_Maths::Vector2& xPoint,
	Zenith_Vector<Flux_WeightedIndex>& xOutWeights) const
{
	xOutWeights.Clear();

	if (m_xBlendPoints.GetSize() == 0)
		return;

	// Find distances to all points
	// Reuse Flux_WeightedIndex: m_uIndex = point index, m_fWeight = distance (temporarily)
	Zenith_Vector<Flux_WeightedIndex> xDistances;
	for (u_int i = 0; i < m_xBlendPoints.GetSize(); ++i)
	{
		float fDist = glm::length(m_xBlendPoints.Get(i).m_xPosition - xPoint);
		xDistances.PushBack({ i, fDist });
	}

	// Sort by distance (stored in m_fWeight)
	std::sort(xDistances.GetDataPointer(), xDistances.GetDataPointer() + xDistances.GetSize(),
		[](const Flux_WeightedIndex& a, const Flux_WeightedIndex& b) {
			return a.m_fWeight < b.m_fWeight;
		});

	// Use inverse distance weighting for nearest 3 points
	u_int uCount = std::min(xDistances.GetSize(), (u_int)3);
	float fTotalWeight = 0.0f;

	for (u_int i = 0; i < uCount; ++i)
	{
		float fDist = xDistances.Get(i).m_fWeight;
		float fWeight = (fDist > 0.0001f) ? 1.0f / fDist : 1000.0f;
		xOutWeights.PushBack({ xDistances.Get(i).m_uIndex, fWeight });
		fTotalWeight += fWeight;
	}

	// Normalize weights
	if (fTotalWeight > 0.0f)
	{
		for (u_int u = 0; u < xOutWeights.GetSize(); u++)
			xOutWeights.Get(u).m_fWeight /= fTotalWeight;
	}
}

void Flux_BlendTreeNode_BlendSpace2D::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	if (m_xBlendPoints.GetSize() == 0)
	{
		xOutPose.Reset();
		return;
	}

	if (m_xBlendPoints.GetSize() == 1)
	{
		if (m_xBlendPoints.Get(0).m_pxNode)
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		return;
	}

	// Try to find containing triangle
	uint32_t idx0, idx1, idx2;
	float w0, w1, w2;

	if (FindContainingTriangle(m_xParameter, idx0, idx1, idx2, w0, w1, w2))
	{
		// Ensure temp poses array is big enough
		while (m_xTempPoses.GetSize() < 3)
			m_xTempPoses.PushBack(Flux_SkeletonPose());

		// Evaluate the three vertices
		if (m_xBlendPoints.Get(idx0).m_pxNode)
			m_xBlendPoints.Get(idx0).m_pxNode->Evaluate(fDt, m_xTempPoses.Get(0), xSkeleton);
		else
			m_xTempPoses.Get(0).Reset();

		if (m_xBlendPoints.Get(idx1).m_pxNode)
			m_xBlendPoints.Get(idx1).m_pxNode->Evaluate(fDt, m_xTempPoses.Get(1), xSkeleton);
		else
			m_xTempPoses.Get(1).Reset();

		if (m_xBlendPoints.Get(idx2).m_pxNode)
			m_xBlendPoints.Get(idx2).m_pxNode->Evaluate(fDt, m_xTempPoses.Get(2), xSkeleton);
		else
			m_xTempPoses.Get(2).Reset();

		// Blend with barycentric weights
		Flux_SkeletonPose xTemp;
		Flux_SkeletonPose::Blend(xTemp, m_xTempPoses.Get(0), m_xTempPoses.Get(1), w1 / (w0 + w1 + 0.0001f));
		Flux_SkeletonPose::Blend(xOutPose, xTemp, m_xTempPoses.Get(2), w2);
	}
	else
	{
		// Fallback: use inverse distance weighting
		Zenith_Vector<Flux_WeightedIndex> xWeights;
		FindNearestPoints(m_xParameter, xWeights);

		if (xWeights.GetSize() == 0)
		{
			xOutPose.Reset();
			return;
		}

		while (m_xTempPoses.GetSize() < xWeights.GetSize())
			m_xTempPoses.PushBack(Flux_SkeletonPose());

		// Evaluate all weighted points
		for (u_int i = 0; i < xWeights.GetSize(); ++i)
		{
			uint32_t uIdx = xWeights.Get(i).m_uIndex;
			if (m_xBlendPoints.Get(uIdx).m_pxNode)
				m_xBlendPoints.Get(uIdx).m_pxNode->Evaluate(fDt, m_xTempPoses.Get(i), xSkeleton);
			else
				m_xTempPoses.Get(i).Reset();
		}

		// Blend based on weights
		xOutPose.CopyFrom(m_xTempPoses.Get(0));
		float fAccumWeight = xWeights.Get(0).m_fWeight;

		for (u_int i = 1; i < xWeights.GetSize(); ++i)
		{
			float fNewWeight = xWeights.Get(i).m_fWeight;
			float fBlend = fNewWeight / (fAccumWeight + fNewWeight);
			Flux_SkeletonPose::Blend(xOutPose, xOutPose, m_xTempPoses.Get(i), fBlend);
			fAccumWeight += fNewWeight;
		}
	}
}

float Flux_BlendTreeNode_BlendSpace2D::GetNormalizedTime() const
{
	if (m_xBlendPoints.GetSize() == 0)
		return 0.0f;

	// Return time of nearest point
	float fMinDist = FLT_MAX;
	const BlendPoint* pxNearest = nullptr;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		float fDist = glm::length(xPoint.m_xPosition - m_xParameter);
		if (fDist < fMinDist)
		{
			fMinDist = fDist;
			pxNearest = &xPoint;
		}
	}

	return pxNearest && pxNearest->m_pxNode ? pxNearest->m_pxNode->GetNormalizedTime() : 0.0f;
}

void Flux_BlendTreeNode_BlendSpace2D::Reset()
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->Reset();
	}
}

void Flux_BlendTreeNode_BlendSpace2D::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_xParameter.x;
	xStream << m_xParameter.y;

	uint32_t uNumPoints = static_cast<uint32_t>(m_xBlendPoints.GetSize());
	xStream << uNumPoints;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		xStream << xPoint.m_xPosition.x;
		xStream << xPoint.m_xPosition.y;

		bool bHasNode = (xPoint.m_pxNode != nullptr);
		xStream << bHasNode;
		if (bHasNode)
		{
			std::string strType = xPoint.m_pxNode->GetNodeTypeName();
			xStream << strType;
			xPoint.m_pxNode->WriteToDataStream(xStream);
		}
	}
}

void Flux_BlendTreeNode_BlendSpace2D::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_xParameter.x;
	xStream >> m_xParameter.y;

	uint32_t uNumPoints = 0;
	xStream >> uNumPoints;

	for (uint32_t i = 0; i < uNumPoints; ++i)
	{
		Zenith_Maths::Vector2 xPosition;
		xStream >> xPosition.x;
		xStream >> xPosition.y;

		bool bHasNode = false;
		xStream >> bHasNode;

		Flux_BlendTreeNode* pxNode = nullptr;
		if (bHasNode)
		{
			std::string strType;
			xStream >> strType;
			pxNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
			if (pxNode)
				pxNode->ReadFromDataStream(xStream);
		}

		m_xBlendPoints.PushBack({ pxNode, xPosition });
	}

	ComputeTriangulation();
}

//=============================================================================
// Flux_BlendTreeNode_Additive
//=============================================================================
Flux_BlendTreeNode_Additive::Flux_BlendTreeNode_Additive(Flux_BlendTreeNode* pxBaseNode,
	Flux_BlendTreeNode* pxAdditiveNode,
	float fWeight)
	: m_pxBaseNode(pxBaseNode)
	, m_pxAdditiveNode(pxAdditiveNode)
	, m_fAdditiveWeight(fWeight)
{
}

Flux_BlendTreeNode_Additive::~Flux_BlendTreeNode_Additive()
{
	delete m_pxBaseNode;
	delete m_pxAdditiveNode;
}

void Flux_BlendTreeNode_Additive::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	// Evaluate base
	if (m_pxBaseNode)
		m_pxBaseNode->Evaluate(fDt, m_xBasePose, xSkeleton);
	else
		m_xBasePose.Reset();

	// Evaluate additive
	if (m_pxAdditiveNode)
		m_pxAdditiveNode->Evaluate(fDt, m_xAdditivePose, xSkeleton);
	else
		m_xAdditivePose.Reset();

	// Apply additive blend
	Flux_SkeletonPose::AdditiveBlend(xOutPose, m_xBasePose, m_xAdditivePose, m_fAdditiveWeight);
}

float Flux_BlendTreeNode_Additive::GetNormalizedTime() const
{
	return m_pxBaseNode ? m_pxBaseNode->GetNormalizedTime() : 0.0f;
}

void Flux_BlendTreeNode_Additive::Reset()
{
	if (m_pxBaseNode) m_pxBaseNode->Reset();
	if (m_pxAdditiveNode) m_pxAdditiveNode->Reset();
}

void Flux_BlendTreeNode_Additive::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_fAdditiveWeight;

	// Base node
	bool bHasBase = (m_pxBaseNode != nullptr);
	xStream << bHasBase;
	if (bHasBase)
	{
		std::string strType = m_pxBaseNode->GetNodeTypeName();
		xStream << strType;
		m_pxBaseNode->WriteToDataStream(xStream);
	}

	// Additive node
	bool bHasAdditive = (m_pxAdditiveNode != nullptr);
	xStream << bHasAdditive;
	if (bHasAdditive)
	{
		std::string strType = m_pxAdditiveNode->GetNodeTypeName();
		xStream << strType;
		m_pxAdditiveNode->WriteToDataStream(xStream);
	}
}

void Flux_BlendTreeNode_Additive::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fAdditiveWeight;

	// Base node
	bool bHasBase = false;
	xStream >> bHasBase;
	if (bHasBase)
	{
		std::string strType;
		xStream >> strType;
		m_pxBaseNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxBaseNode)
			m_pxBaseNode->ReadFromDataStream(xStream);
	}

	// Additive node
	bool bHasAdditive = false;
	xStream >> bHasAdditive;
	if (bHasAdditive)
	{
		std::string strType;
		xStream >> strType;
		m_pxAdditiveNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxAdditiveNode)
			m_pxAdditiveNode->ReadFromDataStream(xStream);
	}
}

//=============================================================================
// Flux_BlendTreeNode_Masked
//=============================================================================
Flux_BlendTreeNode_Masked::Flux_BlendTreeNode_Masked(Flux_BlendTreeNode* pxBaseNode,
	Flux_BlendTreeNode* pxOverrideNode,
	const Flux_BoneMask& xMask)
	: m_pxBaseNode(pxBaseNode)
	, m_pxOverrideNode(pxOverrideNode)
	, m_xBoneMask(xMask)
{
}

Flux_BlendTreeNode_Masked::~Flux_BlendTreeNode_Masked()
{
	delete m_pxBaseNode;
	delete m_pxOverrideNode;
}

void Flux_BlendTreeNode_Masked::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	// Evaluate base
	if (m_pxBaseNode)
		m_pxBaseNode->Evaluate(fDt, m_xBasePose, xSkeleton);
	else
		m_xBasePose.Reset();

	// Evaluate override
	if (m_pxOverrideNode)
		m_pxOverrideNode->Evaluate(fDt, m_xOverridePose, xSkeleton);
	else
		m_xOverridePose.Reset();

	// Apply masked blend
	Flux_SkeletonPose::MaskedBlend(xOutPose, m_xBasePose, m_xOverridePose, m_xBoneMask.GetWeights());
}

float Flux_BlendTreeNode_Masked::GetNormalizedTime() const
{
	return m_pxBaseNode ? m_pxBaseNode->GetNormalizedTime() : 0.0f;
}

void Flux_BlendTreeNode_Masked::Reset()
{
	if (m_pxBaseNode) m_pxBaseNode->Reset();
	if (m_pxOverrideNode) m_pxOverrideNode->Reset();
}

void Flux_BlendTreeNode_Masked::WriteToDataStream(Zenith_DataStream& xStream) const
{
	m_xBoneMask.WriteToDataStream(xStream);

	// Base node
	bool bHasBase = (m_pxBaseNode != nullptr);
	xStream << bHasBase;
	if (bHasBase)
	{
		std::string strType = m_pxBaseNode->GetNodeTypeName();
		xStream << strType;
		m_pxBaseNode->WriteToDataStream(xStream);
	}

	// Override node
	bool bHasOverride = (m_pxOverrideNode != nullptr);
	xStream << bHasOverride;
	if (bHasOverride)
	{
		std::string strType = m_pxOverrideNode->GetNodeTypeName();
		xStream << strType;
		m_pxOverrideNode->WriteToDataStream(xStream);
	}
}

void Flux_BlendTreeNode_Masked::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xBoneMask.ReadFromDataStream(xStream);

	// Base node
	bool bHasBase = false;
	xStream >> bHasBase;
	if (bHasBase)
	{
		std::string strType;
		xStream >> strType;
		m_pxBaseNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxBaseNode)
			m_pxBaseNode->ReadFromDataStream(xStream);
	}

	// Override node
	bool bHasOverride = false;
	xStream >> bHasOverride;
	if (bHasOverride)
	{
		std::string strType;
		xStream >> strType;
		m_pxOverrideNode = Flux_BlendTreeNode::CreateFromTypeName(strType);
		if (m_pxOverrideNode)
			m_pxOverrideNode->ReadFromDataStream(xStream);
	}
}

//=============================================================================
// Flux_BlendTreeNode_Select
//=============================================================================
Flux_BlendTreeNode_Select::~Flux_BlendTreeNode_Select()
{
	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
		delete m_xChildren.Get(u);
}

void Flux_BlendTreeNode_Select::AddChild(Flux_BlendTreeNode* pxChild)
{
	m_xChildren.PushBack(pxChild);
}

void Flux_BlendTreeNode_Select::RemoveChild(u_int uIndex)
{
	if (uIndex < m_xChildren.GetSize())
	{
		delete m_xChildren.Get(uIndex);
		m_xChildren.Remove(uIndex);
	}
}

void Flux_BlendTreeNode_Select::SetSelectedIndex(int32_t iIndex)
{
	if (iIndex >= 0 && iIndex < static_cast<int32_t>(m_xChildren.GetSize()))
	{
		if (iIndex != m_iSelectedIndex)
		{
			m_iSelectedIndex = iIndex;
			// Reset the newly selected child
			if (m_xChildren.Get(m_iSelectedIndex))
				m_xChildren.Get(m_iSelectedIndex)->Reset();
		}
	}
}

void Flux_BlendTreeNode_Select::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	if (m_iSelectedIndex >= 0 && m_iSelectedIndex < static_cast<int32_t>(m_xChildren.GetSize()))
	{
		if (m_xChildren.Get(m_iSelectedIndex))
		{
			m_xChildren.Get(m_iSelectedIndex)->Evaluate(fDt, xOutPose, xSkeleton);
			return;
		}
	}

	xOutPose.Reset();
}

float Flux_BlendTreeNode_Select::GetNormalizedTime() const
{
	if (m_iSelectedIndex >= 0 && m_iSelectedIndex < static_cast<int32_t>(m_xChildren.GetSize()))
	{
		if (m_xChildren.Get(m_iSelectedIndex))
			return m_xChildren.Get(m_iSelectedIndex)->GetNormalizedTime();
	}
	return 0.0f;
}

void Flux_BlendTreeNode_Select::Reset()
{
	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
	{
		if (m_xChildren.Get(u))
			m_xChildren.Get(u)->Reset();
	}
}

bool Flux_BlendTreeNode_Select::IsFinished() const
{
	if (m_iSelectedIndex >= 0 && m_iSelectedIndex < static_cast<int32_t>(m_xChildren.GetSize()))
	{
		if (m_xChildren.Get(m_iSelectedIndex))
			return m_xChildren.Get(m_iSelectedIndex)->IsFinished();
	}
	return true;
}

void Flux_BlendTreeNode_Select::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_iSelectedIndex;

	uint32_t uNumChildren = static_cast<uint32_t>(m_xChildren.GetSize());
	xStream << uNumChildren;

	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
	{
		const Flux_BlendTreeNode* pxChild = m_xChildren.Get(u);
		bool bHasChild = (pxChild != nullptr);
		xStream << bHasChild;
		if (bHasChild)
		{
			std::string strType = pxChild->GetNodeTypeName();
			xStream << strType;
			pxChild->WriteToDataStream(xStream);
		}
	}
}

void Flux_BlendTreeNode_Select::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_iSelectedIndex;

	uint32_t uNumChildren = 0;
	xStream >> uNumChildren;

	for (uint32_t i = 0; i < uNumChildren; ++i)
	{
		bool bHasChild = false;
		xStream >> bHasChild;

		Flux_BlendTreeNode* pxChild = nullptr;
		if (bHasChild)
		{
			std::string strType;
			xStream >> strType;
			pxChild = Flux_BlendTreeNode::CreateFromTypeName(strType);
			if (pxChild)
				pxChild->ReadFromDataStream(xStream);
		}

		m_xChildren.PushBack(pxChild);
	}
}
