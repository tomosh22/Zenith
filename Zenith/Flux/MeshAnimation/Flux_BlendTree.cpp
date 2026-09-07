#include "Zenith.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachineDef.h"   // D48: Flux_AnimationParameters
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

void Flux_BlendTreeNode::WriteChildNode(Zenith_DataStream& xStream, const Flux_BlendTreeNode* pxChild)
{
	bool bHasChild = (pxChild != nullptr);
	xStream << bHasChild;
	if (bHasChild)
	{
		std::string strType = pxChild->GetNodeTypeName();
		xStream << strType;
		pxChild->WriteToDataStream(xStream);
	}
}

Flux_BlendTreeNode* Flux_BlendTreeNode::ReadChildNode(Zenith_DataStream& xStream)
{
	bool bHasChild = false;
	xStream >> bHasChild;
	if (bHasChild)
	{
		std::string strType;
		xStream >> strType;
		Flux_BlendTreeNode* pxChild = CreateFromTypeName(strType);
		if (pxChild)
			pxChild->ReadFromDataStream(xStream);
		return pxChild;
	}
	return nullptr;
}

void Flux_BlendTreeNode::EvaluateChildOrReset(Flux_BlendTreeNode* pxChild, float fDt,
	Flux_SkeletonPose& xPose, const Zenith_SkeletonAsset& xSkeleton,
	float fChildEvalWeight)
{
	if (pxChild)
	{
		// WU-5A (D34): the child's contribution to its layer, set BEFORE the
		// evaluate so a leaf reached through it records the right weight.
		pxChild->SetEvalWeight(fChildEvalWeight);
		pxChild->Evaluate(fDt, xPose, xSkeleton);
	}
	else
	{
		xPose.Reset();
	}
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
		// No clip, output identity pose. Nothing crossed anything, so there is
		// no span to report either.
		m_bSpanPending = false;
		xOutPose.Reset();
		return;
	}

	const float fDuration = m_pxClip->GetDuration();
	const bool bLooping = m_pxClip->IsLooping();
	const float fStep = fDt * m_fPlaybackRate;
	const float fBeforeTimestamp = m_fCurrentTimestamp;

	// Advance time
	m_fCurrentTimestamp += fStep;

	// Handle looping
	if (fDuration > 0.0f)
	{
		if (bLooping)
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

	//-------------------------------------------------------------------------
	// WU-5A (D34/D38/D39): record the crossing this step made.
	//
	// ★ THE MARK MOVES WHATEVER THE STEP WAS. A reverse or zero step records a
	// span that emits nothing, rather than recording no span at all — leaving
	// m_fPreviousTimestamp behind is what would make the next forward frame scan
	// from the old mark and fire the whole skipped stretch at once (D39).
	//
	// ★ THE WRAP IS READ OFF THE RAW ADVANCED TIME, NOT OFF `curr < prev`. A
	// step longer than the clip wraps and still lands ABOVE prev, which `curr <
	// prev` reads as "no wrap" and silently drops that loop's events.
	//-------------------------------------------------------------------------
	m_fPreviousTimestamp = fBeforeTimestamp;
	m_bLastStepForward = fStep > 0.0f;
	m_bLastStepWrapped = bLooping && m_bLastStepForward && fDuration > 0.0f
		&& (fBeforeTimestamp + fStep) >= fDuration;
	m_bLastStepReachedEnd = !bLooping && m_bLastStepForward && fDuration > 0.0f
		&& m_fCurrentTimestamp >= fDuration && fBeforeTimestamp < fDuration;
	m_bSpanPending = true;

	// Initialize output pose with bind pose values from skeleton
	// This ensures bones WITHOUT animation channels keep their bind pose
	// SampleFromClip will only update components that have keyframes
	xOutPose.InitFromBindPose(xSkeleton);

	// Sample the clip - only components with keyframes will be overwritten
	xOutPose.SampleFromClip(*m_pxClip, m_fCurrentTimestamp, xSkeleton);
}

void Flux_BlendTreeNode_Clip::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	// Clearing happens whether or not anyone wanted the span — see the base
	// class declaration: a pending span that survives a collect is one that
	// fires again next frame, and the frame after that.
	if (!m_bSpanPending)
		return;
	m_bSpanPending = false;

	if (!pxOutSpans || !m_pxClip)
		return;

	const float fDuration = m_pxClip->GetDuration();
	if (fDuration <= 0.0f)
		return;  // no range, so no normalized time and nothing an event can sit at

	const float fInvDuration = 1.0f / fDuration;

	Flux_ClipEventSpan xSpan;
	xSpan.m_pxClip = m_pxClip;
	xSpan.m_fPrevNormalizedTime = m_fPreviousTimestamp * fInvDuration;
	xSpan.m_fCurrNormalizedTime = m_fCurrentTimestamp * fInvDuration;
	xSpan.m_fWeight = m_fEvalWeight;
	xSpan.m_bForward = m_bLastStepForward;
	xSpan.m_bWrapped = m_bLastStepWrapped;
	xSpan.m_bLooping = m_pxClip->IsLooping();
	xSpan.m_bReachedEnd = m_bLastStepReachedEnd;
	pxOutSpans->PushBack(xSpan);
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
	// WU-5A: a restart is not a crossing. Carrying the old mark (or an
	// uncollected span) into a state that has just been entered would fire the
	// tail of the PREVIOUS visit's playthrough on the new visit's first frame.
	m_fPreviousTimestamp = 0.0f;
	m_bSpanPending = false;
	m_bLastStepForward = false;
	m_bLastStepWrapped = false;
	m_bLastStepReachedEnd = false;
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
	// D41: no schema change — the event bookkeeping is per-frame playback state
	// and is reset alongside the timestamp, never read from or written to disk.
	m_fCurrentTimestamp = 0.0f;
	m_fPreviousTimestamp = 0.0f;
	m_bSpanPending = false;
	m_bLastStepForward = false;
	m_bLastStepWrapped = false;
	m_bLastStepReachedEnd = false;
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
	// Evaluate both children. The blend is convex, so the two child weights are
	// this node's own contribution split by m_fBlendWeight (D34/D35).
	EvaluateChildOrReset(m_pxChildA, fDt, m_xPoseA, xSkeleton, m_fEvalWeight * (1.0f - m_fBlendWeight));
	EvaluateChildOrReset(m_pxChildB, fDt, m_xPoseB, xSkeleton, m_fEvalWeight * m_fBlendWeight);

	// Blend results
	Flux_SkeletonPose::Blend(xOutPose, m_xPoseA, m_xPoseB, m_fBlendWeight);
}

void Flux_BlendTreeNode_Blend::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	if (m_pxChildA) m_pxChildA->CollectEventSpans(pxOutSpans);
	if (m_pxChildB) m_pxChildB->CollectEventSpans(pxOutSpans);
}

void Flux_BlendTreeNode_Blend::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	if (m_pxChildA) m_pxChildA->ResolveParameters(xParams);
	if (m_pxChildB) m_pxChildB->ResolveParameters(xParams);
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
	WriteChildNode(xStream, m_pxChildA);
	WriteChildNode(xStream, m_pxChildB);
}

void Flux_BlendTreeNode_Blend::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fBlendWeight;
	m_pxChildA = ReadChildNode(xStream);
	m_pxChildB = ReadChildNode(xStream);
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

Flux_BlendTreeNode* Flux_BlendTreeNode_BlendSpace1D::GetBlendPointNode(u_int uIndex) const
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return nullptr;
	return m_xBlendPoints.Get(uIndex).m_pxNode;
}

bool Flux_BlendTreeNode_BlendSpace1D::GetBlendPointPosition(u_int uIndex, float& fOut) const
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return false;
	fOut = m_xBlendPoints.Get(uIndex).m_fPosition;
	return true;
}

bool Flux_BlendTreeNode_BlendSpace1D::SetBlendPointPosition(u_int uIndex, float fPosition, u_int* puOutNewIndex)
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return false;
	// Written as a POSITIVE range test: NaN fails every comparison, so this
	// rejects it without a separate check, and a refusal changes nothing.
	if (!(fPosition >= -3.0e38f && fPosition <= 3.0e38f))
		return false;

	// The node pointer is the point's only identity across the sort — the
	// position is exactly the thing being changed, and an index is what the sort
	// is about to invalidate.
	Flux_BlendTreeNode* pxMoved = m_xBlendPoints.Get(uIndex).m_pxNode;
	m_xBlendPoints.Get(uIndex).m_fPosition = fPosition;
	SortBlendPoints();

	if (puOutNewIndex)
	{
		*puOutNewIndex = uIndex;
		for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
		{
			if (m_xBlendPoints.Get(u).m_pxNode == pxMoved)
			{
				*puOutNewIndex = u;
				break;
			}
		}
	}
	return true;
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
		{
			m_xBlendPoints.Get(0).m_pxNode->SetEvalWeight(m_fEvalWeight);
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		}
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
		{
			m_xBlendPoints.Get(0).m_pxNode->SetEvalWeight(m_fEvalWeight);
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		}
		return;
	}

	if (m_fParameter >= m_xBlendPoints.Get(m_xBlendPoints.GetSize() - 1).m_fPosition)
	{
		Flux_BlendTreeNode* pxLast = m_xBlendPoints.Get(m_xBlendPoints.GetSize() - 1).m_pxNode;
		if (pxLast)
		{
			pxLast->SetEvalWeight(m_fEvalWeight);
			pxLast->Evaluate(fDt, xOutPose, xSkeleton);
		}
		return;
	}

	// Calculate blend factor
	float fRange = m_xBlendPoints.Get(uUpperIdx).m_fPosition - m_xBlendPoints.Get(uLowerIdx).m_fPosition;
	float fBlend = (fRange > 0.0f) ?
		(m_fParameter - m_xBlendPoints.Get(uLowerIdx).m_fPosition) / fRange : 0.0f;

	// Evaluate both points. fBlend is computed BEFORE the evaluates now because
	// each child needs its own share of this node's weight (D34/D35) — a
	// parameter exactly midway gives both 0.5, and D35's lowest-index tiebreak
	// then picks uLowerIdx.
	EvaluateChildOrReset(m_xBlendPoints.Get(uLowerIdx).m_pxNode, fDt, m_xPoseA, xSkeleton, m_fEvalWeight * (1.0f - fBlend));
	EvaluateChildOrReset(m_xBlendPoints.Get(uUpperIdx).m_pxNode, fDt, m_xPoseB, xSkeleton, m_fEvalWeight * fBlend);

	Flux_SkeletonPose::Blend(xOutPose, m_xPoseA, m_xPoseB, fBlend);
}

void Flux_BlendTreeNode_BlendSpace1D::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->CollectEventSpans(pxOutSpans);
	}
}

void Flux_BlendTreeNode_BlendSpace1D::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	// ★ A BOUND NAME THAT IS NOT DECLARED LEAVES THE LITERAL ALONE. GetFloat
	// returns 0.0f for an unknown name, and silently snapping a walk/run blend to
	// zero because a parameter was misspelled reads as "the run animation stopped
	// working" — a stuck literal at least still plays what was authored, and the
	// binding is visible in the def.
	if (!m_strParameterName.empty() && xParams.HasParameter(m_strParameterName))
		m_fParameter = xParams.GetFloat(m_strParameterName);

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->ResolveParameters(xParams);
	}
}

// Walk a blend-point list and return the nearest entry by some caller-supplied
// distance metric (1D uses scalar abs-delta, 2D uses vector length). Returns
// nullptr only when the list is empty.
template<typename PointT, typename DistFn>
static const PointT* FindNearestBlendPoint(const Zenith_Vector<PointT>& xPoints, DistFn fnDist)
{
	if (xPoints.GetSize() == 0) return nullptr;

	float fMinDist = FLT_MAX;
	const PointT* pxNearest = nullptr;
	for (u_int u = 0; u < xPoints.GetSize(); u++)
	{
		const PointT& xPoint = xPoints.Get(u);
		float fDist = fnDist(xPoint);
		if (fDist < fMinDist)
		{
			fMinDist = fDist;
			pxNearest = &xPoint;
		}
	}
	return pxNearest;
}

float Flux_BlendTreeNode_BlendSpace1D::GetNormalizedTime() const
{
	const BlendPoint* pxNearest = FindNearestBlendPoint(m_xBlendPoints,
		[this](const BlendPoint& xP) { return std::abs(xP.m_fPosition - m_fParameter); });
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
	// D48: the BINDING is authored data and rides beside the literal it overrides.
	xStream << m_strParameterName;

	uint32_t uNumPoints = static_cast<uint32_t>(m_xBlendPoints.GetSize());
	xStream << uNumPoints;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		xStream << xPoint.m_fPosition;
		WriteChildNode(xStream, xPoint.m_pxNode);
	}
}

void Flux_BlendTreeNode_BlendSpace1D::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fParameter;
	xStream >> m_strParameterName;

	uint32_t uNumPoints = 0;
	xStream >> uNumPoints;

	for (uint32_t i = 0; i < uNumPoints; ++i)
	{
		float fPosition = 0.0f;
		xStream >> fPosition;
		Flux_BlendTreeNode* pxNode = ReadChildNode(xStream);
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

Flux_BlendTreeNode* Flux_BlendTreeNode_BlendSpace2D::GetBlendPointNode(u_int uIndex) const
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return nullptr;
	return m_xBlendPoints.Get(uIndex).m_pxNode;
}

bool Flux_BlendTreeNode_BlendSpace2D::GetBlendPointPosition(u_int uIndex, Zenith_Maths::Vector2& xOut) const
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return false;
	xOut = m_xBlendPoints.Get(uIndex).m_xPosition;
	return true;
}

bool Flux_BlendTreeNode_BlendSpace2D::SetBlendPointPosition(u_int uIndex, const Zenith_Maths::Vector2& xPosition)
{
	if (uIndex >= m_xBlendPoints.GetSize())
		return false;
	if (!(xPosition.x >= -3.0e38f && xPosition.x <= 3.0e38f
	   && xPosition.y >= -3.0e38f && xPosition.y <= 3.0e38f))
		return false;

	m_xBlendPoints.Get(uIndex).m_xPosition = xPosition;
	// ★ THE TRIANGULATION IS DERIVED FROM THE POSITIONS, so a move that did not
	// re-derive it would leave the sampler interpolating over the shape the
	// points used to make.
	ComputeTriangulation();
	return true;
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
		{
			m_xBlendPoints.Get(0).m_pxNode->SetEvalWeight(m_fEvalWeight);
			m_xBlendPoints.Get(0).m_pxNode->Evaluate(fDt, xOutPose, xSkeleton);
		}
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

		// Evaluate the three vertices — barycentric weights ARE the children's
		// shares of this node's contribution (D34).
		EvaluateChildOrReset(m_xBlendPoints.Get(idx0).m_pxNode, fDt, m_xTempPoses.Get(0), xSkeleton, m_fEvalWeight * w0);
		EvaluateChildOrReset(m_xBlendPoints.Get(idx1).m_pxNode, fDt, m_xTempPoses.Get(1), xSkeleton, m_fEvalWeight * w1);
		EvaluateChildOrReset(m_xBlendPoints.Get(idx2).m_pxNode, fDt, m_xTempPoses.Get(2), xSkeleton, m_fEvalWeight * w2);

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

		// Evaluate all weighted points (inverse-distance weights, already
		// normalized, so they are the children's shares directly).
		for (u_int i = 0; i < xWeights.GetSize(); ++i)
		{
			uint32_t uIdx = xWeights.Get(i).m_uIndex;
			EvaluateChildOrReset(m_xBlendPoints.Get(uIdx).m_pxNode, fDt, m_xTempPoses.Get(i), xSkeleton,
				m_fEvalWeight * xWeights.Get(i).m_fWeight);
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

void Flux_BlendTreeNode_BlendSpace2D::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->CollectEventSpans(pxOutSpans);
	}
}

void Flux_BlendTreeNode_BlendSpace2D::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	// The two axes bind INDEPENDENTLY: a locomotion space commonly drives X from
	// "Speed" and leaves Y (strafe) on its authored literal.
	if (!m_strParameterNameX.empty() && xParams.HasParameter(m_strParameterNameX))
		m_xParameter.x = xParams.GetFloat(m_strParameterNameX);
	if (!m_strParameterNameY.empty() && xParams.HasParameter(m_strParameterNameY))
		m_xParameter.y = xParams.GetFloat(m_strParameterNameY);

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		if (m_xBlendPoints.Get(u).m_pxNode)
			m_xBlendPoints.Get(u).m_pxNode->ResolveParameters(xParams);
	}
}

float Flux_BlendTreeNode_BlendSpace2D::GetNormalizedTime() const
{
	const BlendPoint* pxNearest = FindNearestBlendPoint(m_xBlendPoints,
		[this](const BlendPoint& xP) { return glm::length(xP.m_xPosition - m_xParameter); });
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
	// D48: one binding per axis, beside the literals they override.
	xStream << m_strParameterNameX;
	xStream << m_strParameterNameY;

	uint32_t uNumPoints = static_cast<uint32_t>(m_xBlendPoints.GetSize());
	xStream << uNumPoints;

	for (u_int u = 0; u < m_xBlendPoints.GetSize(); u++)
	{
		const BlendPoint& xPoint = m_xBlendPoints.Get(u);
		xStream << xPoint.m_xPosition.x;
		xStream << xPoint.m_xPosition.y;
		WriteChildNode(xStream, xPoint.m_pxNode);
	}
}

void Flux_BlendTreeNode_BlendSpace2D::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_xParameter.x;
	xStream >> m_xParameter.y;
	xStream >> m_strParameterNameX;
	xStream >> m_strParameterNameY;

	uint32_t uNumPoints = 0;
	xStream >> uNumPoints;

	for (uint32_t i = 0; i < uNumPoints; ++i)
	{
		Zenith_Maths::Vector2 xPosition;
		xStream >> xPosition.x;
		xStream >> xPosition.y;

		Flux_BlendTreeNode* pxNode = ReadChildNode(xStream);
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
	EvaluateChildOrReset(m_pxBaseNode, fDt, m_xBasePose, xSkeleton, m_fEvalWeight);
	EvaluateChildOrReset(m_pxAdditiveNode, fDt, m_xAdditivePose, xSkeleton, m_fEvalWeight * m_fAdditiveWeight);
	Flux_SkeletonPose::AdditiveBlend(xOutPose, m_xBasePose, m_xAdditivePose, m_fAdditiveWeight);
}

void Flux_BlendTreeNode_Additive::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	if (m_pxBaseNode) m_pxBaseNode->CollectEventSpans(pxOutSpans);
	if (m_pxAdditiveNode) m_pxAdditiveNode->CollectEventSpans(pxOutSpans);
}

void Flux_BlendTreeNode_Additive::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	if (m_pxBaseNode) m_pxBaseNode->ResolveParameters(xParams);
	if (m_pxAdditiveNode) m_pxAdditiveNode->ResolveParameters(xParams);
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
	WriteChildNode(xStream, m_pxBaseNode);
	WriteChildNode(xStream, m_pxAdditiveNode);
}

void Flux_BlendTreeNode_Additive::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fAdditiveWeight;
	m_pxBaseNode = ReadChildNode(xStream);
	m_pxAdditiveNode = ReadChildNode(xStream);
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
	// The override branch's share is the mask's LARGEST per-bone entry — the most
	// of it that reaches the pose anywhere. A mask that is zero everywhere gives
	// it weight zero, which is what stops a fully masked-out branch emitting.
	const Zenith_Vector<float>& xMaskWeights = m_xBoneMask.GetWeights();
	float fMaxMaskWeight = 0.0f;
	for (u_int u = 0; u < xMaskWeights.GetSize(); u++)
		fMaxMaskWeight = glm::max(fMaxMaskWeight, xMaskWeights.Get(u));

	EvaluateChildOrReset(m_pxBaseNode, fDt, m_xBasePose, xSkeleton, m_fEvalWeight);
	EvaluateChildOrReset(m_pxOverrideNode, fDt, m_xOverridePose, xSkeleton, m_fEvalWeight * fMaxMaskWeight);
	Flux_SkeletonPose::MaskedBlend(xOutPose, m_xBasePose, m_xOverridePose, xMaskWeights);
}

void Flux_BlendTreeNode_Masked::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	if (m_pxBaseNode) m_pxBaseNode->CollectEventSpans(pxOutSpans);
	if (m_pxOverrideNode) m_pxOverrideNode->CollectEventSpans(pxOutSpans);
}

void Flux_BlendTreeNode_Masked::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	if (m_pxBaseNode) m_pxBaseNode->ResolveParameters(xParams);
	if (m_pxOverrideNode) m_pxOverrideNode->ResolveParameters(xParams);
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
	WriteChildNode(xStream, m_pxBaseNode);
	WriteChildNode(xStream, m_pxOverrideNode);
}

void Flux_BlendTreeNode_Masked::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xBoneMask.ReadFromDataStream(xStream);
	m_pxBaseNode = ReadChildNode(xStream);
	m_pxOverrideNode = ReadChildNode(xStream);
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

Flux_BlendTreeNode* Flux_BlendTreeNode_Select::GetSelectedChild() const
{
	if (m_iSelectedIndex >= 0 && m_iSelectedIndex < static_cast<int32_t>(m_xChildren.GetSize()))
		return m_xChildren.Get(m_iSelectedIndex);
	return nullptr;
}

void Flux_BlendTreeNode_Select::Evaluate(float fDt,
	Flux_SkeletonPose& xOutPose,
	const Zenith_SkeletonAsset& xSkeleton)
{
	EvaluateChildOrReset(GetSelectedChild(), fDt, xOutPose, xSkeleton, m_fEvalWeight);
}

void Flux_BlendTreeNode_Select::CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans)
{
	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
	{
		if (m_xChildren.Get(u))
			m_xChildren.Get(u)->CollectEventSpans(pxOutSpans);
	}
}

void Flux_BlendTreeNode_Select::ResolveParameters(const Flux_AnimationParameters& xParams)
{
	// Every child, not just the selected one — see the base declaration: a branch
	// that becomes selected next frame must already be holding this frame's value.
	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
	{
		if (m_xChildren.Get(u))
			m_xChildren.Get(u)->ResolveParameters(xParams);
	}
}

float Flux_BlendTreeNode_Select::GetNormalizedTime() const
{
	Flux_BlendTreeNode* pxChild = GetSelectedChild();
	return pxChild ? pxChild->GetNormalizedTime() : 0.0f;
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
	Flux_BlendTreeNode* pxChild = GetSelectedChild();
	return pxChild ? pxChild->IsFinished() : true;
}

void Flux_BlendTreeNode_Select::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_iSelectedIndex;

	uint32_t uNumChildren = static_cast<uint32_t>(m_xChildren.GetSize());
	xStream << uNumChildren;

	for (u_int u = 0; u < m_xChildren.GetSize(); u++)
		WriteChildNode(xStream, m_xChildren.Get(u));
}

void Flux_BlendTreeNode_Select::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_iSelectedIndex;

	uint32_t uNumChildren = 0;
	xStream >> uNumChildren;

	for (uint32_t i = 0; i < uNumChildren; ++i)
		m_xChildren.PushBack(ReadChildNode(xStream));
}

#include "Flux/MeshAnimation/Flux_BlendTree.Tests.inl"
