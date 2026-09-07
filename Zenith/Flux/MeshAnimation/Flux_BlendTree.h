#pragma once
#include "Flux_BonePose.h"
#include <array>

// Forward declarations
class Flux_AnimationClipCollection;
class Zenith_SkeletonAsset;
// D48: a blend space reads its position from the live parameter set by NAME. The
// set is declared in Flux_AnimationStateMachineDef.h, which includes THIS header
// (a state owns a blend tree), so the reference is forward-declared here and the
// .cpp includes the definition — the same direction the clip collection above is
// reached through.
class Flux_AnimationParameters;

struct Flux_WeightedIndex
{
	uint32_t m_uIndex;
	float m_fWeight;
};

//=============================================================================
// Flux_ClipEventSpan (WU-5A / D34)
//
// ★ ONE CONTROLLER-LEVEL TIME CANNOT SEE A BLEND TREE'S LEAVES. Every clip leaf
// in a tree runs its own clock — a 1D blend space between a 0.9 s walk and a
// 1.4 s run has two playheads at two normalized times, advancing at two rates —
// so "which events did the playhead cross this frame" has no single answer above
// the leaf. Each evaluated leaf therefore reports ITS OWN crossing, and the
// layer above decides which report is allowed to fire (D35/D36/D37).
//
// Times here are NORMALIZED [0,1] fractions of the clip, matching
// Flux_AnimationEvent::m_fNormalizedTime (D4). The span is HALF-OPEN
// [prev, curr) with two named exceptions, both flagged rather than inferred:
//
//  • m_bWrapped — the step crossed the loop point, so the span is
//    [prev, 1) U [0, curr). It is set from the RAW advanced time
//    (prev + dt*rate >= duration), NOT from `curr < prev`, because a step longer
//    than the clip lands back ABOVE prev and would otherwise read as no wrap.
//  • m_bReachedEnd — a NON-looping clip hit its duration on this step, so the
//    top end closes: [prev, curr] rather than [prev, curr). Without this an
//    event authored at exactly 1.0 could never fire on a clip that stops there.
//
// m_bForward is the SIGN OF THE STEP, kept explicitly because the times alone
// cannot tell a reverse step from a wrap. A non-forward span emits NOTHING
// (D39) — and the leaf's previous time still moved, so the next forward frame
// scans from where the playhead actually is instead of replaying the skipped
// span as a burst.
//=============================================================================
struct Flux_ClipEventSpan
{
	const Flux_AnimationClip* m_pxClip = nullptr;
	float m_fPrevNormalizedTime = 0.0f;
	float m_fCurrNormalizedTime = 0.0f;
	// The blend weight this leaf carried in its layer for this evaluate. Zero
	// means the leaf contributed nothing to the pose and may never emit (D35).
	float m_fWeight = 0.0f;
	bool m_bForward = false;
	bool m_bWrapped = false;
	bool m_bLooping = false;
	bool m_bReachedEnd = false;
};

//=============================================================================
// Flux_BlendTreeNode
// Base class for all blend tree nodes
//=============================================================================
class Flux_BlendTreeNode
{
public:
	virtual ~Flux_BlendTreeNode() = default;

	// Evaluate this node and output a pose
	virtual void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) = 0;

	// Get normalized time progress [0-1] (for exit time conditions)
	virtual float GetNormalizedTime() const = 0;

	// Reset the node (restart playback)
	virtual void Reset() = 0;

	// Check if this node has finished (for non-looping clips)
	virtual bool IsFinished() const { return false; }

	// Get node type name for serialization/debugging
	virtual const char* GetNodeTypeName() const = 0;

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const = 0;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) = 0;

	//=========================================================================
	// Event-span reporting (WU-5A / D34)
	//=========================================================================

	// The weight this node's PARENT handed it for the evaluate about to run (or
	// the one that just ran). A composite sets it on each child immediately
	// before calling that child's Evaluate, so by the time a leaf is reached the
	// value is the product of every fraction down the path — which is exactly
	// the leaf's contribution to its layer's pose. The ROOT of a tree is given
	// 1.0 by whoever evaluates it.
	//
	// It is deliberately NOT recomputed by a second walk. Re-deriving a blend
	// space's selection outside Evaluate would be a guard comparing a value
	// against a re-computation of itself — it would agree with the evaluate that
	// produced it right up until one of the two changed.
	void SetEvalWeight(float fWeight) { m_fEvalWeight = fWeight; }
	float GetEvalWeight() const { return m_fEvalWeight; }

	// ★ WALK EVERY CHILD, EVEN THE ONES THIS FRAME DID NOT EVALUATE, and pass
	// pxOutSpans straight down. A leaf holds its pending span until something
	// collects it, and COLLECTING IS WHAT CLEARS IT: a blend space whose
	// parameter moved off a point, or a Select whose index changed, would
	// otherwise keep handing out the span from the last frame that DID evaluate
	// it, once per frame, forever.
	//
	// pxOutSpans may be NULL, and that is a real mode rather than a defensive
	// check — it means "walk and clear, discard the result", which is what a
	// layer with events silenced (D36) needs so that re-enabling it does not
	// fire a span the silenced frames accumulated.
	virtual void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) { (void)pxOutSpans; }

	//=========================================================================
	// Named parameter bindings (D48)
	//
	// ★ A BLEND SPACE USED TO BE FROZEN AT ITS DESERIALIZED LITERAL. Nothing
	// passed Flux_AnimationParameters into a tree — Evaluate takes none, and
	// Flux_AnimationStateMachine::EvaluateState called it with only (dt, pose,
	// skeleton) — so the ONLY thing that could move a blend position was
	// SetParameter, which no game and no engine path ever called. This walk is
	// the repair, and it follows the pattern the file already uses for clips:
	// the def stores a NAME, and the name is resolved against the live set.
	//
	// Called once per evaluate, on the ROOT of a state's tree, immediately
	// before Evaluate. Composites forward it to every child (not just the ones
	// they are about to evaluate) so a branch that becomes selected later is
	// already holding the current value rather than one frame of the old one.
	// A node with no binding reads nothing and is left at its literal.
	//=========================================================================
	virtual void ResolveParameters(const Flux_AnimationParameters& xParams) { (void)xParams; }

	// Factory method for creating nodes from type name
	static Flux_BlendTreeNode* CreateFromTypeName(const std::string& strTypeName);

	// Shared serialization helpers for child nodes
	static void WriteChildNode(Zenith_DataStream& xStream, const Flux_BlendTreeNode* pxChild);
	static Flux_BlendTreeNode* ReadChildNode(Zenith_DataStream& xStream);

	// Shared evaluation helper — evaluates child or resets pose if null.
	// fChildEvalWeight is the ABSOLUTE weight the child carries in its layer
	// (the caller has already multiplied in its own); it defaults to 1.0 for the
	// callers that hand a child the whole of their own contribution.
	static void EvaluateChildOrReset(Flux_BlendTreeNode* pxChild, float fDt,
		Flux_SkeletonPose& xPose, const Zenith_SkeletonAsset& xSkeleton,
		float fChildEvalWeight = 1.0f);

protected:
	float m_fEvalWeight = 1.0f;
};

//=============================================================================
// Flux_BlendTreeNode_Clip
// Leaf node that plays a single animation clip
//=============================================================================
class Flux_BlendTreeNode_Clip : public Flux_BlendTreeNode
{
public:
	Flux_BlendTreeNode_Clip() = default;
	Flux_BlendTreeNode_Clip(Flux_AnimationClip* pxClip, float fPlaybackRate = 1.0f);

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Clip"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// WU-5A: hand over (and clear) the span this leaf's last Evaluate produced.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;

	// Accessors
	Flux_AnimationClip* GetClip() const { return m_pxClip; }
	void SetClip(Flux_AnimationClip* pxClip) { m_pxClip = pxClip; }

	float GetPlaybackRate() const { return m_fPlaybackRate; }
	void SetPlaybackRate(float fRate) { m_fPlaybackRate = fRate; }

	float GetCurrentTimestamp() const { return m_fCurrentTimestamp; }

	// ★ A SET IS A SCRUB, AND IT MOVES THE MARK WITH IT (D40). The previous
	// timestamp follows the new one and any pending span is dropped: leaving the
	// mark behind would make the next Evaluate report [old mark, new time) and
	// fire every event the playhead was DROPPED past, which is the burst D40
	// exists to prevent.
	void SetCurrentTimestamp(float fTime)
	{
		m_fCurrentTimestamp = fTime;
		m_fPreviousTimestamp = fTime;
		m_bSpanPending = false;
	}

	// The clip time this leaf was at BEFORE its last Evaluate, in seconds.
	float GetPreviousTimestamp() const { return m_fPreviousTimestamp; }

	// For resolving clip reference after deserialization
	void SetClipName(const std::string& strName) { m_strClipName = strName; }
	const std::string& GetClipName() const { return m_strClipName; }
	void ResolveClip(Flux_AnimationClipCollection* pxCollection);

private:
	Flux_AnimationClip* m_pxClip = nullptr;
	std::string m_strClipName;  // For serialization
	float m_fPlaybackRate = 1.0f;
	float m_fCurrentTimestamp = 0.0f;

	// WU-5A event bookkeeping (D34/D38/D39). None of it is serialized — it
	// describes one frame of playback, not the authored tree (D41: no schema
	// change).
	float m_fPreviousTimestamp = 0.0f;
	bool m_bSpanPending = false;
	bool m_bLastStepForward = false;
	bool m_bLastStepWrapped = false;
	bool m_bLastStepReachedEnd = false;
};

//=============================================================================
// Flux_BlendTreeNode_Blend
// Blends between two child nodes based on a weight parameter
//=============================================================================
class Flux_BlendTreeNode_Blend : public Flux_BlendTreeNode
{
public:
	Flux_BlendTreeNode_Blend() = default;
	Flux_BlendTreeNode_Blend(Flux_BlendTreeNode* pxChildA,
		Flux_BlendTreeNode* pxChildB,
		float fBlendWeight = 0.0f);
	~Flux_BlendTreeNode_Blend();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Blend"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Accessors
	Flux_BlendTreeNode* GetChildA() const { return m_pxChildA; }
	Flux_BlendTreeNode* GetChildB() const { return m_pxChildB; }
	void SetChildA(Flux_BlendTreeNode* pxChild) { m_pxChildA = pxChild; }
	void SetChildB(Flux_BlendTreeNode* pxChild) { m_pxChildB = pxChild; }

	float GetBlendWeight() const { return m_fBlendWeight; }
	void SetBlendWeight(float fWeight) { m_fBlendWeight = glm::clamp(fWeight, 0.0f, 1.0f); }

private:
	Flux_BlendTreeNode* m_pxChildA = nullptr;  // Weight 0
	Flux_BlendTreeNode* m_pxChildB = nullptr;  // Weight 1
	float m_fBlendWeight = 0.0f;

	// Temporary poses for blending
	Flux_SkeletonPose m_xPoseA;
	Flux_SkeletonPose m_xPoseB;
};

//=============================================================================
// Flux_BlendTreeNode_BlendSpace1D
// Blends between multiple clips based on a single parameter (e.g., speed)
//=============================================================================
class Flux_BlendTreeNode_BlendSpace1D : public Flux_BlendTreeNode
{
public:
	struct BlendPoint
	{
		Flux_BlendTreeNode* m_pxNode = nullptr;
		float m_fPosition = 0.0f;  // Position on the blend axis
	};

	Flux_BlendTreeNode_BlendSpace1D() = default;
	~Flux_BlendTreeNode_BlendSpace1D();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "BlendSpace1D"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// ★ WALKED IN BLEND-POINT INDEX ORDER, which is what makes D35's "ties go to
	// the LOWEST leaf index" mean something stable: a parameter sitting exactly
	// between two points gives both 0.5, and the earlier point wins.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;

	// D48: read m_fParameter from the named controller parameter, then forward
	// to every blend point's child.
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Add/remove blend points
	void AddBlendPoint(Flux_BlendTreeNode* pxNode, float fPosition);
	void RemoveBlendPoint(u_int uIndex);
	void SortBlendPoints();  // Call after adding all points

	// Accessors.
	//
	// ★ THIS IS ALSO "THE LAST EVALUATED BLEND POSITION", AND THERE IS NO SECOND
	// NAME FOR IT (WU-7.3). ResolveParameters runs on the root of a state's tree
	// immediately before Evaluate (D48) and writes m_fParameter from the live set,
	// so the value read back here is exactly the position the pose that just came
	// out was blended at. An editor drawing a "live dot" wants this; adding a
	// GetLastEvaluatedBlendPosition() beside it would be one number with two
	// spellings that can only ever agree.
	float GetParameter() const { return m_fParameter; }

	// ★ THE MANUAL OVERRIDE, AND ONLY WHEN NOTHING IS BOUND. A bound name is
	// re-read on EVERY evaluate, so a SetParameter on a bound space is overwritten
	// before the next pose — which is the correct precedence (the graph's own
	// authored binding beats a poke from outside) but is a trap if you expect the
	// poke to stick. Serialized, so an UNBOUND space still starts where it was
	// authored.
	void SetParameter(float fValue) { m_fParameter = fValue; }

	// The controller parameter this space's position tracks. Empty = unbound.
	// SERIALIZED — the binding is authored data, the value it reads is not.
	const std::string& GetParameterName() const { return m_strParameterName; }
	void SetParameterName(const std::string& strName) { m_strParameterName = strName; }
	bool HasParameterBinding() const { return !m_strParameterName.empty(); }

	const Zenith_Vector<BlendPoint>& GetBlendPoints() const { return m_xBlendPoints; }

	//=========================================================================
	// Editing / inspection (WU-7.3). ADDITIVE — no field moved and nothing on
	// the wire changed; these exist so an editor can read and move a point
	// without reaching into m_xBlendPoints past the const accessor above.
	//=========================================================================

	u_int GetBlendPointCount() const { return m_xBlendPoints.GetSize(); }

	// The child a blend point plays, or null. A NON-const pointer out of a const
	// method, exactly as Flux_AnimationState::GetBlendTree hands one out: the
	// node OWNS its children, and an editor has to reach one to rename its clip.
	Flux_BlendTreeNode* GetBlendPointNode(u_int uIndex) const;
	bool GetBlendPointPosition(u_int uIndex, float& fOut) const;

	// ★ IT RE-SORTS, AND THAT IS WHY IT REPORTS WHERE THE POINT ENDED UP.
	// Evaluate walks this list assuming ASCENDING positions (it scans for the
	// bracketing pair and clamps at both ends), and ReadFromDataStream sorts on
	// the way in — so a list left unsorted by an edit would blend the wrong pair
	// until the next save/load quietly fixed it. Moving a point past a neighbour
	// therefore RENUMBERS, and puOutNewIndex is the only thing that can tell a
	// caller holding an index where its point went.
	//
	// Refused (false, nothing changed) for an index past the end and for a
	// non-finite position.
	bool SetBlendPointPosition(u_int uIndex, float fPosition, u_int* puOutNewIndex = nullptr);

private:
	Zenith_Vector<BlendPoint> m_xBlendPoints;
	float m_fParameter = 0.0f;
	std::string m_strParameterName;

	// Temporary poses
	Flux_SkeletonPose m_xPoseA;
	Flux_SkeletonPose m_xPoseB;
};

//=============================================================================
// Flux_BlendTreeNode_BlendSpace2D
// Blends between multiple clips based on two parameters (e.g., speed + direction)
//=============================================================================
class Flux_BlendTreeNode_BlendSpace2D : public Flux_BlendTreeNode
{
public:
	struct BlendPoint
	{
		Flux_BlendTreeNode* m_pxNode = nullptr;
		Zenith_Maths::Vector2 m_xPosition;  // Position in 2D parameter space
	};

	Flux_BlendTreeNode_BlendSpace2D() = default;
	~Flux_BlendTreeNode_BlendSpace2D();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "BlendSpace2D"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;

	// D48: read each axis from its own named controller parameter (either may be
	// bound independently), then forward to every blend point's child.
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Add/remove blend points
	void AddBlendPoint(Flux_BlendTreeNode* pxNode, const Zenith_Maths::Vector2& xPosition);
	void RemoveBlendPoint(u_int uIndex);

	// Compute triangulation for efficient sampling
	void ComputeTriangulation();

	// Accessors. As on the 1D space, this IS the last evaluated blend position:
	// ResolveParameters writes it from the live set immediately before Evaluate,
	// so it is what an editor's live dot draws and it needs no second spelling.
	const Zenith_Maths::Vector2& GetParameter() const { return m_xParameter; }

	// The manual override — see Flux_BlendTreeNode_BlendSpace1D::SetParameter for
	// the precedence rule. A bound AXIS is re-read every evaluate; an unbound one
	// keeps whatever was set here.
	void SetParameter(const Zenith_Maths::Vector2& xValue) { m_xParameter = xValue; }

	// The controller parameters this space's two axes track. Empty = unbound.
	// Both are SERIALIZED.
	const std::string& GetParameterNameX() const { return m_strParameterNameX; }
	const std::string& GetParameterNameY() const { return m_strParameterNameY; }
	void SetParameterNameX(const std::string& strName) { m_strParameterNameX = strName; }
	void SetParameterNameY(const std::string& strName) { m_strParameterNameY = strName; }
	bool HasParameterBinding() const { return !m_strParameterNameX.empty() || !m_strParameterNameY.empty(); }

	const Zenith_Vector<BlendPoint>& GetBlendPoints() const { return m_xBlendPoints; }

	//=========================================================================
	// Editing / inspection (WU-7.3). ADDITIVE, as on the 1D space.
	//=========================================================================

	u_int GetBlendPointCount() const { return m_xBlendPoints.GetSize(); }
	Flux_BlendTreeNode* GetBlendPointNode(u_int uIndex) const;
	bool GetBlendPointPosition(u_int uIndex, Zenith_Maths::Vector2& xOut) const;

	// ★ IT RE-TRIANGULATES, AND INDICES ARE STABLE — the opposite of the 1D
	// space on both counts. A 2D space has no order to keep (FindContainingTriangle
	// walks m_xTriangles, which ComputeTriangulation derives from the CURRENT
	// point positions), so a move renumbers nothing and the triangulation is the
	// thing that has to follow. Skipping it leaves the sampler barycentric over
	// the positions the points USED to have.
	bool SetBlendPointPosition(u_int uIndex, const Zenith_Maths::Vector2& xPosition);

private:
	// Find the triangle containing the parameter point and compute barycentric weights
	bool FindContainingTriangle(const Zenith_Maths::Vector2& xPoint,
		uint32_t& uOutIdx0, uint32_t& uOutIdx1, uint32_t& uOutIdx2,
		float& fOutW0, float& fOutW1, float& fOutW2) const;

	// Fallback: find nearest points if no containing triangle
	void FindNearestPoints(const Zenith_Maths::Vector2& xPoint,
		Zenith_Vector<Flux_WeightedIndex>& xOutWeights) const;

	Zenith_Vector<BlendPoint> m_xBlendPoints;
	Zenith_Vector<std::array<uint32_t, 3>> m_xTriangles;  // Delaunay triangulation
	Zenith_Maths::Vector2 m_xParameter = Zenith_Maths::Vector2(0.0f);
	std::string m_strParameterNameX;
	std::string m_strParameterNameY;

	// Temporary poses for blending
	Zenith_Vector<Flux_SkeletonPose> m_xTempPoses;
};

//=============================================================================
// Flux_BlendTreeNode_Additive
// Applies an additive animation on top of a base animation
//=============================================================================
class Flux_BlendTreeNode_Additive : public Flux_BlendTreeNode
{
public:
	Flux_BlendTreeNode_Additive() = default;
	Flux_BlendTreeNode_Additive(Flux_BlendTreeNode* pxBaseNode,
		Flux_BlendTreeNode* pxAdditiveNode,
		float fWeight = 1.0f);
	~Flux_BlendTreeNode_Additive();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "Additive"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// An additive node is NOT a convex blend, so there is no pair of weights
	// summing to one to hand down: the base carries this node's whole weight and
	// the additive layer carries it scaled by m_fAdditiveWeight. A zero additive
	// weight therefore silences the additive branch's events, which matches what
	// it does to the pose.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Accessors
	Flux_BlendTreeNode* GetBaseNode() const { return m_pxBaseNode; }
	Flux_BlendTreeNode* GetAdditiveNode() const { return m_pxAdditiveNode; }
	void SetBaseNode(Flux_BlendTreeNode* pxNode) { m_pxBaseNode = pxNode; }
	void SetAdditiveNode(Flux_BlendTreeNode* pxNode) { m_pxAdditiveNode = pxNode; }

	float GetAdditiveWeight() const { return m_fAdditiveWeight; }
	void SetAdditiveWeight(float fWeight) { m_fAdditiveWeight = glm::clamp(fWeight, 0.0f, 1.0f); }

private:
	Flux_BlendTreeNode* m_pxBaseNode = nullptr;
	Flux_BlendTreeNode* m_pxAdditiveNode = nullptr;
	float m_fAdditiveWeight = 1.0f;

	Flux_SkeletonPose m_xBasePose;
	Flux_SkeletonPose m_xAdditivePose;
};

//=============================================================================
// Flux_BlendTreeNode_Masked
// Blends between two nodes using a per-bone mask
//=============================================================================
class Flux_BlendTreeNode_Masked : public Flux_BlendTreeNode
{
public:
	Flux_BlendTreeNode_Masked() = default;
	Flux_BlendTreeNode_Masked(Flux_BlendTreeNode* pxBaseNode,
		Flux_BlendTreeNode* pxOverrideNode,
		const Flux_BoneMask& xMask);
	~Flux_BlendTreeNode_Masked();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "Masked"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// A per-bone mask is not a scalar weight, so the override branch is given
	// this node's weight scaled by the mask's LARGEST per-bone entry — the most
	// this branch reaches on any bone. A mask that is zero everywhere silences
	// its override branch's events, and an all-ones mask ties with the base, at
	// which point D35's lowest-leaf-index rule picks the base.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Accessors
	Flux_BlendTreeNode* GetBaseNode() const { return m_pxBaseNode; }
	Flux_BlendTreeNode* GetOverrideNode() const { return m_pxOverrideNode; }
	void SetBaseNode(Flux_BlendTreeNode* pxNode) { m_pxBaseNode = pxNode; }
	void SetOverrideNode(Flux_BlendTreeNode* pxNode) { m_pxOverrideNode = pxNode; }

	const Flux_BoneMask& GetBoneMask() const { return m_xBoneMask; }
	void SetBoneMask(const Flux_BoneMask& xMask) { m_xBoneMask = xMask; }

private:
	Flux_BlendTreeNode* m_pxBaseNode = nullptr;
	Flux_BlendTreeNode* m_pxOverrideNode = nullptr;
	Flux_BoneMask m_xBoneMask;

	Flux_SkeletonPose m_xBasePose;
	Flux_SkeletonPose m_xOverridePose;
};

//=============================================================================
// Flux_BlendTreeNode_Select
// Selects one of multiple children based on an integer parameter
// (useful for one-shot animations like attack variations)
//=============================================================================
class Flux_BlendTreeNode_Select : public Flux_BlendTreeNode
{
public:
	Flux_BlendTreeNode_Select() = default;
	~Flux_BlendTreeNode_Select();

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Select"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Walks EVERY child, not just the selected one — see the base class: the
	// unselected branches are exactly the ones holding a span nothing has
	// cleared, and only the selected one has a fresh pending span to report.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans) override;
	void ResolveParameters(const Flux_AnimationParameters& xParams) override;

	// Add children
	void AddChild(Flux_BlendTreeNode* pxChild);
	void RemoveChild(u_int uIndex);

	// Accessors
	int32_t GetSelectedIndex() const { return m_iSelectedIndex; }
	void SetSelectedIndex(int32_t iIndex);
	const Zenith_Vector<Flux_BlendTreeNode*>& GetChildren() const { return m_xChildren; }

	Flux_BlendTreeNode* GetSelectedChild() const;

private:
	Zenith_Vector<Flux_BlendTreeNode*> m_xChildren;
	int32_t m_iSelectedIndex = 0;
};
