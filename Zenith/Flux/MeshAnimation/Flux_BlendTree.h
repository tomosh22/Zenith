#pragma once
#include "Flux_BonePose.h"
#include "Flux_AnimationClip.h"
#include "DataStream/Zenith_DataStream.h"
#include <vector>
#include <memory>
#include <functional>

// Forward declarations
class Flux_MeshGeometry;
class Flux_AnimationClipCollection;
class Zenith_SkeletonAsset;

//=============================================================================
// Flux_BlendTreeNode
// Base class for all blend tree nodes
//=============================================================================
class Flux_BlendTreeNode
{
public:
	virtual ~Flux_BlendTreeNode() = default;

	// Evaluate this node and output a pose (legacy mesh geometry path)
	virtual void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Flux_MeshGeometry& xGeometry) = 0;

	// Evaluate this node using skeleton asset (model instance path)
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

	// Factory method for creating nodes from type name
	static Flux_BlendTreeNode* CreateFromTypeName(const std::string& strTypeName);
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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Clip"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Accessors
	Flux_AnimationClip* GetClip() const { return m_pxClip; }
	void SetClip(Flux_AnimationClip* pxClip) { m_pxClip = pxClip; }

	float GetPlaybackRate() const { return m_fPlaybackRate; }
	void SetPlaybackRate(float fRate) { m_fPlaybackRate = fRate; }

	float GetCurrentTimestamp() const { return m_fCurrentTimestamp; }
	void SetCurrentTimestamp(float fTime) { m_fCurrentTimestamp = fTime; }

	// For resolving clip reference after deserialization
	void SetClipName(const std::string& strName) { m_strClipName = strName; }
	const std::string& GetClipName() const { return m_strClipName; }
	void ResolveClip(Flux_AnimationClipCollection* pxCollection);

private:
	Flux_AnimationClip* m_pxClip = nullptr;
	std::string m_strClipName;  // For serialization
	float m_fPlaybackRate = 1.0f;
	float m_fCurrentTimestamp = 0.0f;
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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Blend"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "BlendSpace1D"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Add/remove blend points
	void AddBlendPoint(Flux_BlendTreeNode* pxNode, float fPosition);
	void RemoveBlendPoint(size_t uIndex);
	void SortBlendPoints();  // Call after adding all points

	// Accessors
	float GetParameter() const { return m_fParameter; }
	void SetParameter(float fValue) { m_fParameter = fValue; }

	const std::vector<BlendPoint>& GetBlendPoints() const { return m_xBlendPoints; }

private:
	std::vector<BlendPoint> m_xBlendPoints;
	float m_fParameter = 0.0f;

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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "BlendSpace2D"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Add/remove blend points
	void AddBlendPoint(Flux_BlendTreeNode* pxNode, const Zenith_Maths::Vector2& xPosition);
	void RemoveBlendPoint(size_t uIndex);

	// Compute triangulation for efficient sampling
	void ComputeTriangulation();

	// Accessors
	const Zenith_Maths::Vector2& GetParameter() const { return m_xParameter; }
	void SetParameter(const Zenith_Maths::Vector2& xValue) { m_xParameter = xValue; }

private:
	// Find the triangle containing the parameter point and compute barycentric weights
	bool FindContainingTriangle(const Zenith_Maths::Vector2& xPoint,
		uint32_t& uOutIdx0, uint32_t& uOutIdx1, uint32_t& uOutIdx2,
		float& fOutW0, float& fOutW1, float& fOutW2) const;

	// Fallback: find nearest points if no containing triangle
	void FindNearestPoints(const Zenith_Maths::Vector2& xPoint,
		std::vector<std::pair<size_t, float>>& xOutWeights) const;

	std::vector<BlendPoint> m_xBlendPoints;
	std::vector<std::array<uint32_t, 3>> m_xTriangles;  // Delaunay triangulation
	Zenith_Maths::Vector2 m_xParameter = Zenith_Maths::Vector2(0.0f);

	// Temporary poses for blending
	std::vector<Flux_SkeletonPose> m_xTempPoses;
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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "Additive"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;

	const char* GetNodeTypeName() const override { return "Masked"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

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
		const Flux_MeshGeometry& xGeometry) override;

	void Evaluate(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton) override;

	float GetNormalizedTime() const override;
	void Reset() override;
	bool IsFinished() const override;

	const char* GetNodeTypeName() const override { return "Select"; }

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Add children
	void AddChild(Flux_BlendTreeNode* pxChild);
	void RemoveChild(size_t uIndex);

	// Accessors
	int32_t GetSelectedIndex() const { return m_iSelectedIndex; }
	void SetSelectedIndex(int32_t iIndex);

private:
	std::vector<Flux_BlendTreeNode*> m_xChildren;
	int32_t m_iSelectedIndex = 0;
};
