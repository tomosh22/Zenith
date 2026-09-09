#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Animation/Zenith_BonePickGeometry.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include <string>

class Flux_SkeletonInstance;
// The preview SUBJECT (D5). Forward-declared rather than included: the accessors
// below only hand the pointers out, and Flux_ModelInstance.h / Flux_MeshInstance.h
// would pull the asset-handle + buffer headers into every panel that opens a
// session. The .cpp includes both.
class Flux_ModelInstance;
class Flux_MeshInstance;
class Zenith_MaterialAsset;
// STRUCT, matching its definition — a `class` forward declaration of a struct
// trips MSVC C4099 in every TU that sees both.
struct Zenith_EditorPrefs;

//=============================================================================
// Zenith_AnimationPreviewSession (WU-2.4) — ONE editor panel's live preview of
// ONE clip: its own controller, its own skeleton instance, its own clock.
//
// ★ THE SESSION OWNS A CONTROLLER; IT NEVER BORROWS AN ENTITY'S (D30). The
// Properties panel's animator inspector ticks the entity's controller itself
// whenever the editor is Stopped (Zenith_AnimatorComponent::RenderPropertiesPanel).
// A second panel that also advanced that controller would double-tick it — the
// clip would run at 2x with both panels open and at 1x with one, which reads as
// "the preview speed is wrong" rather than as two drivers. A private controller
// removes the sharing entirely; the DRIVE GUARD below
// (Flux_AnimationController::TryBeginFrameDrive) is what stops the remaining
// case, two panels aimed at the SAME controller.
//
// ★ THE SESSION HOLDS A DEEP COPY OF THE CLIP, and that is forced as well as
// desirable. Zenith_AnimationDocument::GetClip() is const (the document is the
// only writer), while direct playback needs a mutable Flux_AnimationClip* to put
// in the controller's name-keyed collection. Copying also means a scrub samples a
// stable snapshot rather than a clip a keyframe drag is halfway through editing.
// RefreshClipFrom() is how a panel pushes the document's new content across
// without losing the play head or the rig choice.
//
// ★ THE SESSION DRIVES ITS OWN RENDER VIEW, AND SHARES NOTHING (D32 is spent).
// D32 gave the panel HasPreviewSlot()/GetPreviewSlotOwnerName()/ReclaimPreviewSlot()
// because there was ONE preview view and two editors wanting it. There are two
// now — kuFluxViewSlotPreviewMaterial and kuFluxViewSlotPreviewAnim, each with
// its own persistent LDR — so this session stages slot 6 and nothing else stages
// slot 6. Nothing to arbitrate means nothing to be dispossessed OF, which is why
// the panel's placeholder-plus-Reclaim UI is DELETED rather than disabled: a
// button for a state that can no longer occur is a promise the code cannot keep.
//
// Flux_PreviewSlotArbiter is KEPT and is untouched from here. It still arbitrates
// the MATERIAL slot, which still has two claimants of its own — the material
// editor panel's liveness window and the --preview-test-view diagnostic — and
// Flux_MaterialPreviewController is its only owner now.
//
// ★ THE SESSION SUBMITS ITS OWN MESH, AND IT DOES SO AS A PULL SOURCE (D5). A
// resolved rig registers this object with
// Flux_RendererImpl::RegisterExternalSceneItemSource (keyed on `this`), and the
// GPU-scene sync polls it once per frame for one Flux_ExternalSceneItem per
// submesh: the session's model matrix, that submesh's Flux_MeshInstance, the
// aligned material, THE SESSION'S OWN Flux_SkeletonInstance, and the view mask of
// slot 6 alone. PULL rather than PUSH because a panel draws on frames the editor
// may never sync: a pushed item would sit in the renderer's list holding a raw
// mesh pointer until the next real frame, which is a use-after-free if the rig
// re-resolved in between.
//
// ★ THE SKELETON ON THE ITEM IS THE SESSION'S, NEVER THE MODEL INSTANCE'S. A
// Flux_ModelInstance builds a skeleton of its own from the model's skeleton path
// and NOTHING ever animates it, so an item carrying that one would compute-skin a
// bind pose — the mesh would stand in a T-pose while the clip played underneath
// it, which reads as a broken clip rather than as a wrong pointer. The bone
// palette dedups by skeleton POINTER, so this is also what shares one palette
// block across every submesh of the preview.
//=============================================================================

//-----------------------------------------------------------------------------
// Why a rig could not be resolved. Each case is its own value because the panel
// says something different about each one: a clip with no recorded rig wants a
// picker, a clip whose recorded rig does not load wants the path shown.
//-----------------------------------------------------------------------------
enum Zenith_AnimPreviewRigStatus : u_int
{
	ZENITH_ANIMPREVIEW_RIG_OK,
	ZENITH_ANIMPREVIEW_RIG_NOT_OPEN,
	ZENITH_ANIMPREVIEW_RIG_NO_SKELETON_PATH,
	ZENITH_ANIMPREVIEW_RIG_SKELETON_UNRESOLVED,
	ZENITH_ANIMPREVIEW_RIG_NO_MODEL_PATH,
	ZENITH_ANIMPREVIEW_RIG_MODEL_UNRESOLVED,
};

//-----------------------------------------------------------------------------
// "No bone is selected." A sentinel rather than a separate bool because every
// consumer already has to handle "the index does not resolve" — a rig change can
// invalidate a perfectly valid-looking index — and a bool beside it would give
// the same fact two representations that can disagree.
//-----------------------------------------------------------------------------
inline constexpr u_int kuINVALID_BONE_SELECTION = 0xFFFFFFFFu;

enum Zenith_AnimPreviewOpenResult : u_int
{
	// The clip is loaded, the rig resolved and the direct-play node is armed.
	ZENITH_ANIMPREVIEW_OPEN_OK,
	// The clip is loaded and the session is open, but no rig resolved — the panel
	// prompts and calls SetRigOverride. NOT a failure: everything except playback
	// (the track list, the duration, the event list) is already usable.
	ZENITH_ANIMPREVIEW_OPEN_NEEDS_RIG,
};

//=============================================================================
// The session.
//=============================================================================
class Zenith_AnimationPreviewSession
{
public:
	Zenith_AnimationPreviewSession();
	explicit Zenith_AnimationPreviewSession(const std::string& strDisplayName);
	~Zenith_AnimationPreviewSession();

	// ★ NON-COPYABLE. It owns a Flux_AnimationController (itself non-copyable), a
	// heap Flux_SkeletonInstance whose address that controller caches, the heap
	// renderable below, the activation edge of one render view, and a renderer
	// pull-source registration keyed on ITS OWN ADDRESS — a copy would give two
	// objects one instance to delete, one view to raise and lower against each
	// other, and a source row only one of them could ever unregister.
	Zenith_AnimationPreviewSession(const Zenith_AnimationPreviewSession&) = delete;
	Zenith_AnimationPreviewSession& operator=(const Zenith_AnimationPreviewSession&) = delete;

	//-------------------------------------------------------------------------
	// Lifecycle
	//-------------------------------------------------------------------------

	// Deep-copies xClip and resolves the rig (see NeedsRigSelection).
	// strClipAssetPath is the clip's asset path — it is the key the remembered rig
	// choice is stored under, and may be empty for a clip that has no file
	// (nothing is then remembered).
	//
	// The preview VIEW is not touched here: opening is not a frame, and the view
	// is raised by the first UpdatePreviewView() a rigged session gets.
	Zenith_AnimPreviewOpenResult Open(const Flux_AnimationClip& xClip, const std::string& strClipAssetPath);

	// Re-copy the clip's contents WITHOUT disturbing the play head or the rig —
	// what a panel calls when its document reports an edit. Refused
	// (false, nothing changes) when the session is not open. The clip NAME is
	// re-applied from the source, so a Save As is a re-Open, not a refresh.
	bool RefreshClipFrom(const Flux_AnimationClip& xClip);

	// Stops playback, drops the clip and the rig, and DEACTIVATES the animation
	// preview view. Idempotent.
	//
	// ★ THE DEACTIVATION IS NOT TIDINESS. Nothing else can lower slot 6: the
	// material preview's janitor (Flux_MaterialPreviewController::Update, run every
	// frame from the GPU-scene sync) lowers slot 5 and only slot 5, and the panel
	// stops calling into the session the moment it closes the clip. A view left
	// active is a full per-view pass chain plus its transients rendered every frame
	// for a preview nobody is looking at.
	void Close();

	bool IsOpen() const { return m_bOpen; }
	const std::string& GetClipAssetPath() const { return m_strClipAssetPath; }
	const std::string& GetDisplayName() const { return m_strDisplayName; }
	void SetDisplayName(const std::string& strName) { m_strDisplayName = strName; }

	//-------------------------------------------------------------------------
	// Rig (D31)
	//
	// The skeleton comes from the clip's m_strSkeletonPath and the mesh from its
	// m_strPreviewModelPath; a remembered per-clip choice (see the preference
	// store below) is tried FIRST, because it exists only when a human made it and
	// a human's answer outranks a metadata field they already found wanting.
	//-------------------------------------------------------------------------

	bool NeedsRigSelection() const { return m_eRigStatus != ZENITH_ANIMPREVIEW_RIG_OK; }
	Zenith_AnimPreviewRigStatus GetRigStatus() const { return m_eRigStatus; }

	// Point the session at an explicit rig. Re-resolves immediately; on success
	// the choice is written to the preference store under the clip's asset path
	// (nothing is remembered for a clip with no path, or with no store wired).
	// Returns true iff the rig resolved.
	bool SetRigOverride(const std::string& strSkeletonPath, const std::string& strPreviewModelPath);

	const std::string& GetSkeletonPath() const { return m_strSkeletonPath; }
	const std::string& GetPreviewModelPath() const { return m_strPreviewModelPath; }

	// ★ A PREVIEW MESH MAY BE A .zasset/.zmesh MESH RATHER THAN A .zmodel BUNDLE,
	// and both are accepted. The generated tree and bush sway clips name a skinned
	// mesh asset because those sets bake no .zmodel at all; refusing them would
	// make every one of them permanently "needs rig selection" with nothing a user
	// could pick to fix it. True when the resolved preview mesh came in as a bare
	// mesh asset (the panel labels it, and a future submission path needs to know
	// there is no material binding to read).
	bool IsPreviewMeshBareMeshAsset() const { return m_bPreviewIsBareMesh; }

	// Where a per-clip rig choice is remembered. Not owned. The panel wires the
	// editor's own Zenith_EditorPrefs; a unit passes its own instance. Null (the
	// default) means "remember nothing", which is also what a headless run gets.
	void SetPreferenceStore(Zenith_EditorPrefs* pxPrefs) { m_pxPrefs = pxPrefs; }
	Zenith_EditorPrefs* GetPreferenceStore() const { return m_pxPrefs; }

	//-------------------------------------------------------------------------
	// Playback. All of it is a no-op until the rig resolves.
	//-------------------------------------------------------------------------

	// Advance the session's OWN controller. Does nothing while paused.
	void Tick(float fDt);

	void Play() { m_bPlaying = true; }
	void Pause() { m_bPlaying = false; }
	bool IsPlaying() const { return m_bPlaying; }

	// Scrub. Works while paused — that is the whole point of a scrub — and emits
	// no animation events (see Flux_AnimationController::SeekDirectPlay).
	bool Seek(float fTimeSeconds);

	float GetTime() const { return m_xController.GetDirectPlayTime(); }
	float GetDuration() const { return m_xClip.GetDuration(); }
	float GetNormalizedTime() const;

	void SetPlaybackSpeed(float fSpeed) { m_xController.SetPlaybackSpeed(fSpeed); }
	float GetPlaybackSpeed() const { return m_xController.GetPlaybackSpeed(); }

	// Loops the SESSION'S COPY of the clip only — a preview loop toggle is a
	// viewing preference and must not write back into the document.
	void SetLooping(bool bLooping) { m_xClip.SetLooping(bLooping); }
	bool IsLooping() const { return m_xClip.IsLooping(); }

	//-------------------------------------------------------------------------
	// The evaluated rig — what a viewport draws and what a unit measures.
	//-------------------------------------------------------------------------

	Flux_AnimationController& Controller() { return m_xController; }
	const Flux_AnimationController& Controller() const { return m_xController; }
	Flux_SkeletonInstance* GetSkeletonInstance() const { return m_pxSkeletonInstance; }
	const Flux_AnimationClip& GetClip() const { return m_xClip; }

	// How many bones the resolved rig has, or 0. What SelectBone range-checks
	// against, and what a caller iterating bones asks first.
	u_int GetBoneCount() const;

	//-------------------------------------------------------------------------
	// How many times the rig has been RESOLVED. Starts at 0 and increments on
	// every ResolveRig — including a re-resolve to the SAME rig, which is the
	// case that matters.
	//
	// ★ IT IS NOT A BONE COUNT AND IT IS NOT AN INSTANCE POINTER, and neither of
	// those would do. ResolveRigInternal DELETES and re-creates the skeleton
	// instance on every call, so every live pose and every latched bone rotation
	// an editor gesture is holding belongs to an object that no longer exists —
	// but the bone COUNT is unchanged when the rig is the same one, so the
	// shape-change reset below never fires, and the freed instance's address is
	// routinely handed straight back by the allocator, so a pointer comparison is
	// blind exactly when it matters. A counter cannot be either.
	//
	// The PANEL reads this once per frame and cancels its in-flight manipulator
	// gestures when it moves (Zenith_EditorPanel_Animation::CancelAllPoseGestures).
	// The session cancels its OWN drag here only when the rig changed SHAPE, which
	// is a narrower rule and always was.
	u_int GetRigGeneration() const { return m_uRigGeneration; }

	//-------------------------------------------------------------------------
	// POSE AUTHORING (Phase 4).
	//
	// ★ BONE SELECTION LIVES HERE, NOT IN Zenith_SelectionSystem, and that is a
	// decision rather than an omission. A bone index is not an entity: it has no
	// generation counter, it is meaningless without the skeleton it indexes, and
	// the skeleton instance is owned by THIS object. Widening the entity
	// selection system to carry one would cost four unrelated edits
	// (Zenith_EntityID returns, an EntityID-keyed AABB cache, a
	// Zenith_ModelComponent* precise phase, an unordered_set of ids in the
	// editor state) and buy nothing.
	//
	// ★ IT IS DELIBERATELY NOT UNDOABLE, matching entity selection, which is
	// also not. It is therefore not on the document and not in any command.
	//-------------------------------------------------------------------------

	// An out-of-range index CLEARS the selection rather than storing something
	// that will never resolve.
	void SelectBone(u_int uBoneIndex);
	void ClearBoneSelection() { m_uSelectedBoneIndex = kuINVALID_BONE_SELECTION; }
	u_int GetSelectedBoneIndex() const { return m_uSelectedBoneIndex; }
	bool HasBoneSelection() const { return m_uSelectedBoneIndex != kuINVALID_BONE_SELECTION; }

	// Hover is a per-frame paint hint, so it is set unfiltered and reset by
	// whoever owns the frame — no range check, no side effects.
	void SetHoveredBoneIndex(u_int uBoneIndex) { m_uHoveredBoneIndex = uBoneIndex; }
	u_int GetHoveredBoneIndex() const { return m_uHoveredBoneIndex; }
	bool HasBoneHover() const { return m_uHoveredBoneIndex != kuINVALID_BONE_SELECTION; }

	// Model space -> world space for the pick geometry and every space
	// conversion above it.
	//
	// ★ IDENTITY FOR THE WHOLE OF PHASE 4, and the accessor exists anyway. The
	// preview camera orbits the ORIGIN (Flux_PreviewOrbitCameraPos), so a
	// non-identity value buys nothing today and adds a term to every conversion
	// — but every helper still takes it explicitly, so the general case is
	// testable and the day a session places its subject somewhere else nothing
	// has to be re-derived.
	const Zenith_Maths::Matrix4& GetSessionModelMatrix() const { return m_xSessionModelMatrix; }
	void SetSessionModelMatrix(const Zenith_Maths::Matrix4& xModel);

	// ★ THE PRECONDITION NOBODY CAN SEE (design note §3.2).
	// Flux_SkeletonInstance::GetBoneModelTransform returns a CACHE written only by
	// ComputeSkinningMatrices; SetBoneLocalTransform neither updates nor
	// invalidates it. So a live pose write followed immediately by a model-space
	// read returns the PREVIOUS geometry — pick shapes one write stale, an
	// overlay one write behind the bone — with no assert and no symptom other
	// than lag. This is the one call that brings model space current, and the
	// rule is: every pose write is followed by RefreshDerivedPose() before
	// anything reads a model matrix.
	//
	// Tick() and Seek() do NOT need it — Flux_AnimationController::
	// ApplyOutputPoseToSkeleton already ends in ComputeSkinningMatrices, and both
	// of them route through it. The hazard is the DIRECT writes (the drag).
	void RefreshDerivedPose();

	// The bone hit volumes for the CURRENT pose, in world space. Rebuilt lazily
	// the first time it is asked for after the pose (or the session model matrix)
	// moved, so a frame that neither picks nor draws an overlay pays nothing.
	const Zenith_BonePickSet& GetBonePickSet() const;

	// Nearest bone along the ray. False (and uOutBone untouched) on a miss.
	bool PickBone(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir,
		u_int& uOutBone) const;

	// Diagnostic — how many times the pick set has actually been rebuilt.
	//
	// ★ UNGATED, and for the same reason the panel's rect diagnostics are: "the
	// pick missed" has several causes, and a unit that can only see the bool
	// cannot tell "the ray was wrong" from "the geometry was never refreshed
	// after the pose moved". This is what makes the §3.2 precondition a checkable
	// property rather than a paragraph.
	u_int GetPickSetBuildCount() const { return m_uPickSetBuildCount; }

	//-------------------------------------------------------------------------
	// The LIVE DRAG pose (design note §4.2). WU-4.1 lands the state; WU-4.3 puts
	// the manipulator on top of it.
	//
	// ★ TWO LAYERS, AND ONLY ONE OF THEM IS UNDOABLE. The live pose — these bone
	// rotations — is written every drag frame, is never undoable and is never
	// serialized. The KEYS, on the document, are written ONCE on release and are
	// both. Nothing is pushed on the undo stack during a drag and nothing is
	// written to the document during one.
	//
	// ★ WHILE A DRAG IS IN FLIGHT THE SESSION MUST NOT RE-EVALUATE FROM THE CLIP,
	// or a per-frame evaluate would stomp the drag between two mouse moves.
	// BeginBoneDrag latches the bone's local rotation and suspends Tick();
	// EndBoneDrag resumes it.
	//-------------------------------------------------------------------------

	// False for an unresolved rig or an out-of-range bone; a drag already in
	// flight is refused rather than silently retargeted.
	bool BeginBoneDrag(u_int uBoneIndex);

	// Write the dragged bone's new LOCAL rotation and bring model space current.
	// Position and scale are read back unchanged — Flux_SkeletonInstance's setter
	// takes all three and there is no rotation-only overload.
	bool UpdateBoneDrag(const Zenith_Maths::Quat& xNewBoneLocalRotation);

	// Ends the drag. The pose STAYS where the drag left it (that is the point of
	// HasUnkeyedPose below); resuming evaluation is what a later seek or tick
	// then discards.
	void EndBoneDrag();

	bool IsBoneDragActive() const { return m_bBoneDragActive; }
	u_int GetDragBoneIndex() const { return m_uDragBoneIndex; }
	const Zenith_Maths::Quat& GetDragInitialRotation() const { return m_xDragInitialRotation; }
	Zenith_Maths::Quat GetBoneLocalRotation(u_int uBoneIndex) const;

	// TRUE when the live pose has been dragged away from what the clip evaluates
	// to and no key has been written.
	//
	// ★ THE PANEL MUST SHOW THIS. With auto-key off, a released drag writes no
	// key and creates no undo entry, and the next seek re-evaluates from the clip
	// and destroys it. That is the one thing a user can silently lose, so it is
	// not allowed to be silent. Making the drag itself undoable was rejected: an
	// undo entry restoring a pose the document never contained is a lie about
	// what was saved.
	bool HasUnkeyedPose() const { return m_bUnkeyedPose; }
	// Called by whoever writes the keys (WU-4.3), once the pose IS in the clip.
	void ClearUnkeyedPose() { m_bUnkeyedPose = false; }

	// Auto-key is a per-EDITING-SESSION preference, not clip content, so it lives
	// here rather than on the document. WU-4.3's Action_SetAutoKey drives it.
	void SetAutoKey(bool bEnabled) { m_bAutoKey = bEnabled; }
	bool GetAutoKey() const { return m_bAutoKey; }

	//-------------------------------------------------------------------------
	// THE PREVIEW SUBJECT (D5) — what the view actually draws.
	//
	// One renderable per submesh, created when the rig resolves and destroyed by
	// ReleaseRig. A .zmodel gives a Flux_ModelInstance (one mesh instance per
	// binding); a bare .zasset/.zmesh gives a single Flux_MeshInstance and no
	// material at all. The accessors below are what the pull source reads, and
	// they are public so a unit can compare the submitted items against them
	// rather than against a second copy of the same rule.
	//-------------------------------------------------------------------------

	// 0 with no rig, 1 for a bare mesh, one per model binding otherwise.
	u_int GetPreviewSubmeshCount() const;
	// Null for an out-of-range index, or for a binding whose mesh failed to load
	// (Flux_ModelInstance skips those, so the count already excludes them).
	Flux_MeshInstance* GetPreviewSubmeshInstance(u_int uIndex) const;
	// The material aligned with that submesh, or NULL — which the renderer reads
	// as "the blank material". A bare mesh asset carries no binding, so it is
	// always null there.
	//
	// ★ THE ALIGNMENT IS APPROXIMATE BY CONSTRUCTION and the header says so
	// rather than the call site guessing. Flux_ModelInstance keeps ONE material
	// entry per binding MATERIAL, not per binding, so GetMaterial(i) lines up
	// with GetMeshInstance(i) only while every binding has exactly one. A model
	// that breaks that is clamped to the last entry and logged once at resolve:
	// a possibly-wrong material still draws the submesh, where an out-of-range
	// read would be undefined behaviour on a path with no other symptom.
	Zenith_MaterialAsset* GetPreviewSubmeshMaterial(u_int uIndex) const;

	// The two owning pointers, for a unit that needs to see which shape resolved.
	Flux_ModelInstance* GetPreviewModelInstance() const { return m_pxModelInstance; }
	Flux_MeshInstance* GetPreviewMeshInstance() const { return m_pxMeshInstance; }

	// Is this session currently registered as a renderer pull source? True from a
	// successful rig resolve until ReleaseRig (or Close).
	bool IsSubmittingToRenderer() const { return m_bSourceRegistered; }

	//-------------------------------------------------------------------------
	// The animation preview view (kuFluxViewSlotPreviewAnim).
	//
	// ★ THE PAIR IS EDGE-TRIGGERED AND BOTH HALVES ARE IDEMPOTENT, because the
	// only thing either of them may do per frame is stage constants: the registry
	// reports whether SetViewActive CHANGED the active set, and only a change
	// costs a graph rebuild. So a panel may call either one every frame without
	// asking itself which edge it is on — which is the only way the hidden-panel
	// call site below can be correct, since a hidden panel has no idea whether it
	// was hidden last frame too.
	//-------------------------------------------------------------------------

	// Per-frame staging: raises the view and fills its ViewConstants + target dims
	// from the orbit state. A session that is not open, or whose rig did not
	// resolve, LOWERS the view instead of staging into it — there is no pose to
	// draw, and an active full-pipeline view is not free.
	//
	// A run with no Flux_GraphicsImpl (before Flux comes up, or after shutdown
	// dropped it) returns early. That is NOT the headless case: the Null backend
	// builds a graphics object like every other, and the boot unit batch runs with
	// a live registry.
	void UpdatePreviewView();

	// Lower the view. Called by Close(), by the rig teardown, and by the PANEL
	// above its own early returns — a hidden or unopened panel never reaches
	// UpdatePreviewView, and nothing else in the engine lowers this slot.
	void DeactivatePreviewView();

	// Orbit camera, same clamps and the same pure builders as the material
	// preview (Flux_PreviewClampPitch / Flux_PreviewApplyZoom).
	void OrbitCamera(float fDeltaYaw, float fDeltaPitch);
	void ZoomCamera(float fDelta);
	void GetCameraOrbit(float& fOutYaw, float& fOutPitch, float& fOutDistance) const;

private:
	// Resolve m_strSkeletonPath / m_strPreviewModelPath into asset pins, rebuild
	// the skeleton instance and re-arm the direct-play node. Sets m_eRigStatus.
	// The wrapper also drops the bone selection when the rig changed shape; the
	// Internal half is the resolve itself, which has six early returns and so
	// cannot carry that check at the bottom of its own body.
	void ResolveRig();
	void ResolveRigInternal();
	void ReleaseRig();
	// Build the renderable for the resolved preview mesh (D5). Called once per
	// successful resolve, after the skeleton instance exists so a rig that fails
	// leaves no orphan renderable behind.
	void CreatePreviewMeshInstances();
	void DestroyPreviewMeshInstances();
	// Register/unregister this session as the renderer's pull source, keyed on
	// `this`. Both are safe with no Flux (they simply do nothing), and the
	// unregister runs BEFORE the renderables are destroyed — a source left
	// registered is polled on the very next frame.
	void RegisterExternalItemSource();
	void UnregisterExternalItemSource();
	// Put the working clip into the controller's collection and start direct play.
	void ArmDirectPlay();
	// The remembered choice for m_strClipAssetPath, if any.
	bool TryLoadRememberedRig(std::string& strOutSkeleton, std::string& strOutModel) const;
	void RememberRig() const;

	// The pose (or the session model matrix) moved: the pick set is no longer a
	// description of where the bones are. Cheap and unconditional — the rebuild
	// itself is what is deferred.
	void MarkPickSetDirty() { m_bPickSetDirty = true; }
	// Drop the selection, the hover and any drag. Called on Close and whenever
	// the rig changes shape underneath them.
	void ResetBoneAuthoringState();

	std::string m_strDisplayName;
	std::string m_strClipAssetPath;

	// ★ DECLARED BEFORE m_xController ON PURPOSE. Members are destroyed in reverse
	// declaration order, and the controller's clip collection holds a BORROWED
	// pointer to this clip; destroying the clip first would leave the collection
	// pointing at a dead object for the length of its own destructor.
	Flux_AnimationClip m_xClip;

	std::string m_strSkeletonPath;
	std::string m_strPreviewModelPath;
	SkeletonHandle m_xSkeleton;
	MeshHandle m_xPreviewMesh;
	ModelHandle m_xPreviewModel;

	// The session's OWN skeleton instance — never an entity's (D30).
	Flux_SkeletonInstance* m_pxSkeletonInstance = nullptr;

	// The preview subject (D5). EXACTLY ONE of these is non-null while a rig is
	// resolved: a .zmodel bundle gives the model instance, a bare .zasset/.zmesh
	// the mesh instance. Both are owned here and destroyed by ReleaseRig.
	//
	// ★ THE MODEL INSTANCE'S OWN SKELETON IS DELIBERATELY UNUSED. It exists (the
	// factory builds one from the model's skeleton path), nothing animates it, and
	// the submission carries m_pxSkeletonInstance instead — see the class comment.
	Flux_ModelInstance* m_pxModelInstance = nullptr;
	Flux_MeshInstance* m_pxMeshInstance = nullptr;
	// Mirrors the renderer's source table so the unregister is exactly as
	// idempotent as the view teardown beside it.
	bool m_bSourceRegistered = false;

	Zenith_EditorPrefs* m_pxPrefs = nullptr;

	Zenith_AnimPreviewRigStatus m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_NOT_OPEN;
	bool m_bOpen = false;
	// TRUE between the session's own raise of the animation-preview view and its
	// lowering of it. DeactivatePreviewView() lowers ONLY when this is set: the
	// panel calls it on every hidden frame, and a panel that never raised the view
	// must not lower one somebody else (the render-graph oracle's sample C) raised.
	bool m_bRaisedPreviewView = false;
	bool m_bPlaying = true;
	bool m_bPreviewIsBareMesh = false;

	//-------------------------------------------------------------------------
	// Pose authoring (Phase 4).
	//-------------------------------------------------------------------------
	u_int m_uSelectedBoneIndex = kuINVALID_BONE_SELECTION;
	u_int m_uHoveredBoneIndex = kuINVALID_BONE_SELECTION;
	Zenith_Maths::Matrix4 m_xSessionModelMatrix = Zenith_Maths::Matrix4(1.0f);

	// ★ THE BONE COUNT THE SELECTION WAS MADE AGAINST. A rig swap can hand back
	// an instance with a different skeleton in which the same index means a
	// different bone — so the selection is dropped when this moves, rather than
	// silently retargeting to whatever bone now sits at that index.
	u_int m_uSelectionRigBoneCount = 0u;

	// See GetRigGeneration. Bumped by ResolveRig, unconditionally — including on
	// the failure paths, because a resolve that FAILED also released the instance
	// every holder of a bone index was pointing at.
	u_int m_uRigGeneration = 0u;

	bool m_bBoneDragActive = false;
	bool m_bUnkeyedPose = false;
	bool m_bAutoKey = false;
	u_int m_uDragBoneIndex = kuINVALID_BONE_SELECTION;
	Zenith_Maths::Quat m_xDragInitialRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	// ★ MUTABLE, because PickBone and GetBonePickSet are const and rebuild
	// lazily. The alternative is a non-const PickBone, which would make every
	// const reader of the session (an overlay, a unit assertion) take a mutable
	// reference to ask a read-only question.
	mutable Zenith_BonePickSet m_xPickSet;
	mutable bool m_bPickSetDirty = true;
	mutable u_int m_uPickSetBuildCount = 0u;

	// Orbit state, seeded to the material preview's defaults so the two editors
	// frame their subject the same way.
	float m_fCameraYaw = 0.6f;
	float m_fCameraPitch = 0.35f;
	float m_fCameraDistance = 3.0f;

	// ★ LAST MEMBER. See the m_xClip note above.
	Flux_AnimationController m_xController;
};

// Force-link anchor: Zenith_AnimatorComponent's editor panel calls this so this
// TU survives /OPT:REF. Nothing references the session for real until the
// preview panel lands (Phase 3), and an .obj the linker never pulls in takes its
// ZENITH_TEST registrars with it — the unit count moves by zero and nothing reds.
// Same idiom, same reason, as Zenith_AnimationDocument_ForceLink.
bool Zenith_AnimationPreviewSession_ForceLink();

#endif // ZENITH_TOOLS
