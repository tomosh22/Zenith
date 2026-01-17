#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include <string>

// Forward declarations
class Flux_AnimationClip;

/**
 * Zenith_AnimationAsset - Animation clip asset
 *
 * Wrapper around Flux_AnimationClip that provides registry integration,
 * reference counting, and caching. This is the primary way to load
 * animation clips in the engine.
 *
 * Usage:
 *   // Load from file
 *   Zenith_AnimationAsset* pAnim = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>("game:Anims/walk.zanim");
 *   Flux_AnimationClip* pClip = pAnim->GetClip();
 *
 *   // Create procedural
 *   Zenith_AnimationAsset* pAnim = Zenith_AssetRegistry::Get().Create<Zenith_AnimationAsset>();
 *   pAnim->SetClip(pMyProceduralClip);
 */
class Zenith_AnimationAsset : public Zenith_Asset
{
public:
	Zenith_AnimationAsset();
	~Zenith_AnimationAsset();

	// Non-copyable
	Zenith_AnimationAsset(const Zenith_AnimationAsset&) = delete;
	Zenith_AnimationAsset& operator=(const Zenith_AnimationAsset&) = delete;

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	/**
	 * Get the underlying animation clip
	 * @return Pointer to the clip, or nullptr if not loaded
	 */
	Flux_AnimationClip* GetClip() { return m_pxClip; }
	const Flux_AnimationClip* GetClip() const { return m_pxClip; }

	/**
	 * Check if the animation is valid/loaded
	 */
	bool IsValid() const { return m_pxClip != nullptr; }

	//--------------------------------------------------------------------------
	// Procedural Animation Support
	//--------------------------------------------------------------------------

	/**
	 * Set the clip for procedural animations
	 * Takes ownership of the clip (will delete it on destruction)
	 * @param pxClip Pointer to clip to take ownership of
	 */
	void SetClip(Flux_AnimationClip* pxClip);

	/**
	 * Release ownership of the clip without deleting it
	 * @return Pointer to the clip (caller takes ownership)
	 */
	Flux_AnimationClip* ReleaseClip();

private:
	friend class Zenith_AssetRegistry;
	friend Zenith_Asset* LoadAnimationAsset(const std::string&);

	/**
	 * Load animation from file (private - use Zenith_AssetRegistry::Get)
	 * Supports both .zanim binary format and source formats via Assimp
	 * @param strPath Path to animation file
	 * @return true on success
	 */
	bool LoadFromFile(const std::string& strPath);

	Flux_AnimationClip* m_pxClip = nullptr;
	bool m_bOwnsClip = true;
};
