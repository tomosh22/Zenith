#pragma once

#include "Flux/Flux.h"
#include "Maths/Zenith_Maths.h"
#include <vector>
#include <string>

class Flux_MeshGeometry;
class Zenith_SkeletonAsset;
class Flux_AnimationClip;

//=============================================================================
// Flux_AnimationTexture
// Vertex Animation Texture (VAT) for GPU-driven skeletal animation.
//
// Instead of sending bone matrices per instance (~6.4KB/instance for 100 bones),
// we bake all animation frames into a texture and sample vertex positions directly.
// This reduces per-instance data to just animation index + time (8 bytes).
//
// Texture Layout:
//   Width:  Vertex count (padded to power of 2)
//   Height: NumAnimations × FramesPerAnimation
//   Format: RGBA16F (xyz = position, w = unused/normal.x)
//
// Usage:
//   1. Call BakeFromAnimation() with mesh, skeleton, and animation clips
//   2. Export to .zanmt file with Export()
//   3. Load with LoadFromFile() in runtime
//   4. Bind position texture to vertex shader
//   5. Sample: texelFetch(animTex, ivec2(vertexID, animFrame), 0).xyz
//=============================================================================

class Flux_AnimationTexture
{
public:
	//-------------------------------------------------------------------------
	// File format header
	//-------------------------------------------------------------------------
	struct Header
	{
		uint32_t m_uMagic = 0x544E415A;  // 'ZANT' (Zenith ANimation Texture)
		uint32_t m_uVersion = 1;
		uint32_t m_uVertexCount;          // Original vertex count
		uint32_t m_uTextureWidth;         // Padded width (power of 2)
		uint32_t m_uTextureHeight;        // NumAnimations × FramesPerAnimation
		uint32_t m_uNumAnimations;        // Number of animation clips
		uint32_t m_uFramesPerAnimation;   // Frames per animation clip
		float m_fFrameDuration;           // Seconds per frame
	};

	//-------------------------------------------------------------------------
	// Animation clip info (stored per clip)
	//-------------------------------------------------------------------------
	struct AnimationInfo
	{
		std::string m_strName;            // Animation name (e.g., "Idle", "Walk")
		uint32_t m_uFirstFrame;           // First frame index in texture
		uint32_t m_uFrameCount;           // Number of frames in this animation
		float m_fDuration;                // Total animation duration in seconds
		bool m_bLooping;                  // Whether animation should loop
	};

	Flux_AnimationTexture();
	~Flux_AnimationTexture();

	// Non-copyable
	Flux_AnimationTexture(const Flux_AnimationTexture&) = delete;
	Flux_AnimationTexture& operator=(const Flux_AnimationTexture&) = delete;

	//-------------------------------------------------------------------------
	// Creation / Loading
	//-------------------------------------------------------------------------

	// Bake vertex positions for all animation frames into texture
	// Returns false if baking fails
	bool BakeFromAnimations(
		const Flux_MeshGeometry* pxMesh,
		const Zenith_SkeletonAsset* pxSkeleton,
		const std::vector<Flux_AnimationClip*>& axAnimations,
		uint32_t uFramesPerSecond = 30
	);

	// Load from .zanmt file
	static Flux_AnimationTexture* LoadFromFile(const std::string& strPath);

	// Export to .zanmt file
	bool Export(const std::string& strPath) const;

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------

	const Flux_Texture* GetPositionTexture() const { return &m_xPositionTexture; }
	Flux_Texture* GetPositionTexture() { return &m_xPositionTexture; }

	uint32_t GetVertexCount() const { return m_xHeader.m_uVertexCount; }
	uint32_t GetTextureWidth() const { return m_xHeader.m_uTextureWidth; }
	uint32_t GetTextureHeight() const { return m_xHeader.m_uTextureHeight; }
	uint32_t GetNumAnimations() const { return m_xHeader.m_uNumAnimations; }
	uint32_t GetFramesPerAnimation() const { return m_xHeader.m_uFramesPerAnimation; }
	float GetFrameDuration() const { return m_xHeader.m_fFrameDuration; }

	// Get animation info by index
	const AnimationInfo* GetAnimationInfo(uint32_t uIndex) const;

	// Find animation by name (returns nullptr if not found)
	const AnimationInfo* FindAnimation(const std::string& strName) const;

	// Get frame index for a given animation time
	uint32_t GetFrameIndex(uint32_t uAnimIndex, float fNormalizedTime) const;

	//-------------------------------------------------------------------------
	// GPU Resource Management
	//-------------------------------------------------------------------------

	// Upload texture data to GPU (called after loading)
	void CreateGPUResources();

	// Release GPU resources
	void DestroyGPUResources();

	bool HasGPUResources() const { return m_bGPUResourcesCreated; }

private:
	//-------------------------------------------------------------------------
	// Helper functions
	//-------------------------------------------------------------------------
	static uint32_t NextPowerOfTwo(uint32_t v);

	// Evaluate skeletal animation at given time and output transformed positions
	void EvaluateAnimationFrame(
		const Flux_MeshGeometry* pxMesh,
		const Zenith_SkeletonAsset* pxSkeleton,
		const Flux_AnimationClip* pxAnimation,
		float fTime,
		std::vector<Zenith_Maths::Vector4>& axOutPositions
	) const;

	//-------------------------------------------------------------------------
	// Data
	//-------------------------------------------------------------------------
	Header m_xHeader = {};
	std::vector<AnimationInfo> m_axAnimations;
	std::vector<uint16_t> m_axTextureData;  // RGBA16F data (4 × uint16 per pixel)

	Flux_Texture m_xPositionTexture;
	bool m_bGPUResourcesCreated = false;
};
