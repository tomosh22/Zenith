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
 *   // Load an owning handle (GetView<T>() for a raw transient view)
 *   AnimationHandle xAnim = Zenith_AssetRegistry::Acquire<Zenith_AnimationAsset>("game:Anims/walk.zanim");
 *   Flux_AnimationClip* pClip = xAnim.GetDirect()->GetClip();
 *
 *   // Create procedural (Create<T>() returns an owning handle)
 *   AnimationHandle xAnim = Zenith_AssetRegistry::Create<Zenith_AnimationAsset>();
 *   xAnim.GetDirect()->SetClip(pMyProceduralClip);
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

	//--------------------------------------------------------------------------
	// Reload (WU-2.1 / D26 + D27)
	//--------------------------------------------------------------------------

	/**
	 * ★ RE-READ THE .zanim FROM DISK **INTO THE CLIP THIS ASSET ALREADY OWNS**.
	 *
	 * There used to be no reload path at all. LoadFromFile is private and one-shot,
	 * so the only way to see an edited .zanim was Zenith_AssetRegistry::ForceUnload
	 * (which ignores refcounts entirely) plus a fresh acquire — and that yields a
	 * DIFFERENT Flux_AnimationClip address. Controllers borrow the clip POINTER
	 * (Flux_AnimationClipCollection::AddClipReference) and state machines resolve
	 * their clip references through that collection, so moving the clip leaves every
	 * borrower pointing at freed memory. GetClip() returns the same pointer across a
	 * reload, and that pointer observes the new content.
	 *
	 * ★ TRANSACTIONAL (D27). The file is read and validated into a TEMPORARY clip
	 * first; the live clip is only touched once the parse has succeeded, via
	 * Flux_AnimationClip::ReplaceContentsFrom. A refused envelope, a stale schema, a
	 * missing file or a truncated one returns FALSE with the live clip still on its
	 * previous contents, byte for byte. A half-applied clip is worse than a failed
	 * load. The temporary is a STACK LOCAL, so this path cannot allocate a second
	 * Flux_AnimationClip and cannot leak one however many times it is called.
	 *
	 * ★ THE CLIP NAME MAY NOT CHANGE (D28). A file whose clip is named differently is
	 * refused by ReplaceContentsFrom (assert + false, clip untouched): the collection
	 * is name-keyed and a rename underneath a live clip corrupts two lookups
	 * silently. Renaming is Save As.
	 *
	 * ★ SYNCHRONISATION IS THE CALLER'S RESPONSIBILITY, AND THERE IS NO LOCK HERE.
	 * The swap is a plain non-atomic write over data an animation update may be
	 * sampling. Call this on the MAIN THREAD, between frames, with no
	 * Flux_AnimationController::Update in flight — which is exactly where the editor
	 * calls it. This deliberately does not invent a threading mechanism: taking a
	 * lock here would only move the tear into the sampler, which takes none.
	 *
	 * Only the binary .zanim format reloads. A source-format (Assimp) path is refused
	 * — re-importing a .glb is an import, not a reload, and cannot round-trip the
	 * clip's authored metadata.
	 *
	 * @return true when the live clip now holds the file's contents
	 */
	bool ReloadFromDisk();

	/**
	 * As ReloadFromDisk(), but from an explicitly named file rather than this asset's
	 * registry path. Accepts a prefixed path ("game:Anims/walk.zanim") or a plain
	 * filesystem path; both go through Zenith_AssetRegistry::ResolvePath. This is the
	 * primitive the no-argument overload calls, and it is what lets a caller reload an
	 * asset that the registry does not know about (a unit test, or a document layer
	 * that owns its own file). The name-immutability rule applies here too, so this is
	 * NOT a way to point an asset at a different clip.
	 */
	bool ReloadFromDisk(const std::string& strPath);

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend struct Zenith_AssetLoadTraits;   // DoLoad calls private LoadFromFile

	/**
	 * Load animation from file (private - use Zenith_AssetRegistry::Get)
	 * Supports both .zanim binary format and source formats via Assimp
	 * @param strPath Path to animation file, already resolved to a filesystem path
	 * @return SUCCESS, or an error code on failure
	 *
	 * ★ A REFUSED .zanim IS AN ERROR NOW. This used to `return true` unconditionally
	 * on the .zanim branch — Flux_AnimationClip::ReadFromDataStream was void, so a
	 * corrupt or stale file produced an assert, an EMPTY clip, and a successfully
	 * loaded asset holding it. It now returns whatever Flux_AnimationClip::ParseStream
	 * reported, and the registry deletes the asset rather than caching an empty one.
	 */
	Zenith_Status LoadFromFile(const std::string& strPath);

	/**
	 * The single .zanim read path, shared by LoadFromFile and both ReloadFromDisk
	 * overloads: read the file, parse it into a stack-local staging clip, and only on
	 * success replace the live clip's contents in place (allocating the live clip
	 * first if this asset has none yet). Nothing observable changes on any failure.
	 * @param strResolvedPath A filesystem path — already through ResolvePath.
	 */
	Zenith_Status LoadZanimIntoLiveClip(const std::string& strResolvedPath);

	Flux_AnimationClip* m_pxClip = nullptr;
	bool m_bOwnsClip = true;
};
