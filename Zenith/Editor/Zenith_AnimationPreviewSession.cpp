#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimationPreviewSession.h"
#include "Editor/Zenith_EditorPrefs.h"

#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/RenderViews/Flux_RenderViews.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"   // the pure orbit/view-constants builders
#include "Flux/Flux_GraphicsImpl.h"                            // RenderViews()
#include "Flux/Flux_RendererImpl.h"                            // RequestGraphRebuild

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cctype>
#include <cstring>

bool Zenith_AnimationPreviewSession_ForceLink()
{
	// ★ THE ONLY REASON THIS EXISTS. Nothing calls the session for real until the
	// preview panel lands (Phase 3), and MSVC's /OPT:REF drops an .obj no live
	// symbol references — taking the ZENITH_TEST registrars at the bottom of this
	// TU with it, so the unit count moves by zero and nothing reds. WU-2.2 was bitten
	// by exactly this. Zenith_AnimatorComponent's editor panel names this function.
	return true;
}

//=============================================================================
// Local helpers
//=============================================================================

namespace
{
	bool EndsWithNoCase(const std::string& strValue, const char* szSuffix)
	{
		const size_t uSuffixLength = std::strlen(szSuffix);
		if (strValue.size() < uSuffixLength)
		{
			return false;
		}
		const size_t uStart = strValue.size() - uSuffixLength;
		for (size_t u = 0; u < uSuffixLength; ++u)
		{
			const int iLhs = std::tolower(static_cast<unsigned char>(strValue[uStart + u]));
			const int iRhs = std::tolower(static_cast<unsigned char>(szSuffix[u]));
			if (iLhs != iRhs)
			{
				return false;
			}
		}
		return true;
	}

	// A collection keyed by name asserts on an empty one (see
	// Flux_AnimationClipCollection::AddClipReference), so the session's WORKING
	// COPY is given a name when the source has none. This never reaches a file:
	// the copy is the session's, and the document is the only writer of the clip.
	std::string FallbackClipName(const std::string& strClipAssetPath)
	{
		if (strClipAssetPath.empty())
		{
			return "PreviewClip";
		}
		size_t uLeaf = strClipAssetPath.find_last_of("/\\:");
		uLeaf = (uLeaf == std::string::npos) ? 0 : uLeaf + 1;
		std::string strName = strClipAssetPath.substr(uLeaf);
		const size_t uDot = strName.find_last_of('.');
		if (uDot != std::string::npos && uDot > 0)
		{
			strName = strName.substr(0, uDot);
		}
		return strName.empty() ? std::string("PreviewClip") : strName;
	}
}

//=============================================================================
// Zenith_AnimationPreviewSession
//=============================================================================

Zenith_AnimationPreviewSession::Zenith_AnimationPreviewSession()
	: m_strDisplayName("Animation Preview")
{
}

Zenith_AnimationPreviewSession::Zenith_AnimationPreviewSession(const std::string& strDisplayName)
	: m_strDisplayName(strDisplayName)
{
}

Zenith_AnimationPreviewSession::~Zenith_AnimationPreviewSession()
{
	Close();
}

//-----------------------------------------------------------------------------
// Lifecycle
//-----------------------------------------------------------------------------

Zenith_AnimPreviewOpenResult Zenith_AnimationPreviewSession::Open(
	const Flux_AnimationClip& xClip, const std::string& strClipAssetPath)
{
	Close();

	m_strClipAssetPath = Zenith_AssetRegistry::NormalizeAssetPath(strClipAssetPath);

	// Deep copy — see the header. The document's clip is const and direct playback
	// needs a mutable clip; copying also isolates the scrub from a live edit.
	m_xClip = xClip;
	if (m_xClip.GetName().empty())
	{
		m_xClip.SetName(FallbackClipName(m_strClipAssetPath));
	}

	m_bOpen = true;
	m_bPlaying = true;

	// The rig: a remembered choice first (a human's answer outranks a metadata
	// field they already found wanting), then the clip's own D7 metadata.
	std::string strRememberedSkeleton;
	std::string strRememberedModel;
	if (TryLoadRememberedRig(strRememberedSkeleton, strRememberedModel))
	{
		m_strSkeletonPath = strRememberedSkeleton;
		m_strPreviewModelPath = strRememberedModel;
		ResolveRig();
	}
	if (m_eRigStatus != ZENITH_ANIMPREVIEW_RIG_OK)
	{
		m_strSkeletonPath = m_xClip.GetMetadata().m_strSkeletonPath;
		m_strPreviewModelPath = m_xClip.GetMetadata().m_strPreviewModelPath;
		ResolveRig();
	}

	// Last-opened-wins (D32). The claim happens even when the rig did not resolve:
	// the panel that was just opened is the one the user is looking at, and it
	// still has a placeholder to draw.
	Flux_PreviewSlotArbiter::Claim(this, m_strDisplayName);

	return (m_eRigStatus == ZENITH_ANIMPREVIEW_RIG_OK)
		? ZENITH_ANIMPREVIEW_OPEN_OK
		: ZENITH_ANIMPREVIEW_OPEN_NEEDS_RIG;
}

bool Zenith_AnimationPreviewSession::RefreshClipFrom(const Flux_AnimationClip& xClip)
{
	if (!m_bOpen)
	{
		return false;
	}

	const float fTime = m_xController.GetDirectPlayTime();

	// ★ THE CLIP OBJECT DOES NOT MOVE — its CONTENTS are overwritten in place. The
	// controller's collection holds a borrowed pointer to m_xClip and the
	// direct-play node holds the same address (the reason WU-2.1 gave
	// Flux_AnimationClip::ReplaceContentsFrom), so allocating a fresh clip here
	// would leave both pointing at the old one. Plain copy-assignment rather than
	// ReplaceContentsFrom because that verb REFUSES a name change by design, and a
	// Save As legitimately renames the clip under this session — handled below.
	const std::string strPreviousName = m_xClip.GetName();
	m_xClip = xClip;
	if (m_xClip.GetName().empty())
	{
		m_xClip.SetName(FallbackClipName(m_strClipAssetPath));
	}

	if (m_xClip.GetName() != strPreviousName)
	{
		// The collection is NAME-keyed, so a renamed clip has to be re-registered
		// under its new key or every lookup finds the stale one.
		m_xController.GetClipCollection().Clear();
		ArmDirectPlay();
	}

	// Keep the play head where the user left it, folded into the new duration.
	m_xController.SeekDirectPlay(fTime);

	// The pose was re-evaluated, so the shapes moved — but the SELECTION did not:
	// a clip refresh changes the clip's content, never the rig it animates, so
	// every bone index still means the same bone.
	MarkPickSetDirty();
	return true;
}

void Zenith_AnimationPreviewSession::Close()
{
	Flux_PreviewSlotArbiter::Release(this);

	m_xController.Stop();
	// ★ NOT GetClipCollection().Clear() ANY MORE. That emptied the borrowed clip
	// pointers and left every asset handle the controller had taken — the skeleton
	// one in particular — held for the rest of the session object's life. A session
	// owned by a panel that outlives Zenith_AssetRegistry::Shutdown then released
	// those handles into a registry that had already force-deleted the assets, which
	// is a write into freed memory that only ASSERTS when the freed word happens to
	// read zero. ReleaseAssetReferences drops the handles and the collection as one.
	m_xController.ReleaseAssetReferences();
	ReleaseRig();

	m_xClip = Flux_AnimationClip();
	m_strClipAssetPath.clear();
	m_strSkeletonPath.clear();
	m_strPreviewModelPath.clear();
	m_bPreviewIsBareMesh = false;
	m_bOpen = false;
	m_bPlaying = false;
	m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_NOT_OPEN;

	// ★ A BONE SELECTION IS SCOPED TO THE RIG IT INDEXES, and Close drops the rig.
	// Leaving an index behind would have the next clip opened in this session
	// start with a bone "selected" that means whatever sits at that index in a
	// skeleton the user has never seen.
	//
	// The AUTO-KEY flag deliberately survives: it is a tool mode the user set on
	// the panel, not a property of the clip that happened to be open.
	ResetBoneAuthoringState();
	m_uSelectionRigBoneCount = 0u;
	MarkPickSetDirty();
}

void Zenith_AnimationPreviewSession::ResetBoneAuthoringState()
{
	m_uSelectedBoneIndex = kuINVALID_BONE_SELECTION;
	m_uHoveredBoneIndex = kuINVALID_BONE_SELECTION;
	m_bBoneDragActive = false;
	m_bUnkeyedPose = false;
	m_uDragBoneIndex = kuINVALID_BONE_SELECTION;
	m_xDragInitialRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
}

//-----------------------------------------------------------------------------
// Rig
//-----------------------------------------------------------------------------

void Zenith_AnimationPreviewSession::ReleaseRig()
{
	// The controller is told to forget the instance BEFORE it is deleted: it caches
	// the raw pointer and nothing else would clear it.
	m_xController.Initialize(nullptr);

	if (m_pxSkeletonInstance != nullptr)
	{
		m_pxSkeletonInstance->Destroy();
		delete m_pxSkeletonInstance;
		m_pxSkeletonInstance = nullptr;
	}

	m_xSkeleton.Clear();
	m_xPreviewMesh.Clear();
	m_xPreviewModel.Clear();

	// The pick shapes describe an instance that no longer exists.
	MarkPickSetDirty();
}

//-----------------------------------------------------------------------------
// A rig that changed SHAPE invalidates every bone index anybody is holding.
//
// ★ THE TEST IS THE BONE COUNT, and it is deliberately not "did the instance
// pointer move". ResolveRigInternal deletes and re-creates the instance on every
// call — including the one a SetRigOverride to the SAME rig makes — so a pointer
// comparison would drop the selection on a no-op re-resolve. A count that stays
// the same is not proof that the skeleton is the same one, but it is the cheap
// half of the check, and the expensive half (comparing bone names) buys nothing
// while a session only ever re-resolves to a rig a human just named.
//
// ★ THE WRAPPER EXISTS BECAUSE THE RESOLVE HAS SIX EARLY RETURNS. Putting the
// comparison at the bottom of the body would skip it on every failure path —
// which is exactly the path a rig swap that does not load takes.
//-----------------------------------------------------------------------------
void Zenith_AnimationPreviewSession::ResolveRig()
{
	const u_int uPreviousBoneCount = m_uSelectionRigBoneCount;

	ResolveRigInternal();

	const u_int uNewBoneCount = GetBoneCount();
	if (uNewBoneCount != uPreviousBoneCount)
	{
		ResetBoneAuthoringState();
	}
	m_uSelectionRigBoneCount = uNewBoneCount;
	MarkPickSetDirty();
}

void Zenith_AnimationPreviewSession::ResolveRigInternal()
{
	ReleaseRig();
	m_bPreviewIsBareMesh = false;

	if (!m_bOpen)
	{
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_NOT_OPEN;
		return;
	}

	if (m_strSkeletonPath.empty())
	{
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_NO_SKELETON_PATH;
		return;
	}

	Zenith_SkeletonAsset* pxSkeleton = Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(m_strSkeletonPath);
	if (pxSkeleton == nullptr || pxSkeleton->GetNumBones() == 0)
	{
		// A zero-bone skeleton is treated as unresolved rather than as a rig: it
		// produces no pose, so a preview built on one is a blank window with no
		// error anywhere.
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_SKELETON_UNRESOLVED;
		return;
	}

	if (m_strPreviewModelPath.empty())
	{
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_NO_MODEL_PATH;
		return;
	}

	// ★ BOTH A .zmodel BUNDLE AND A BARE .zasset/.zmesh ARE ACCEPTED, dispatched on
	// the EXTENSION because the registry is typed and the two are different asset
	// classes. The generated tree and bush sway clips record a skinned .zasset —
	// those sets bake no .zmodel at all — so refusing a bare mesh would leave every
	// one of them permanently "needs rig selection" with nothing a user could pick
	// to fix it.
	if (EndsWithNoCase(m_strPreviewModelPath, ZENITH_MODEL_EXT))
	{
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::GetView<Zenith_ModelAsset>(m_strPreviewModelPath);
		if (pxModel == nullptr)
		{
			m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_MODEL_UNRESOLVED;
			return;
		}
		m_xPreviewModel.Set(pxModel);
	}
	else if (EndsWithNoCase(m_strPreviewModelPath, ZENITH_MESH_ASSET_EXT) ||
		EndsWithNoCase(m_strPreviewModelPath, ZENITH_MESH_EXT))
	{
		Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(m_strPreviewModelPath);
		if (pxMesh == nullptr)
		{
			m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_MODEL_UNRESOLVED;
			return;
		}
		m_xPreviewMesh.Set(pxMesh);
		m_bPreviewIsBareMesh = true;
	}
	else
	{
		// An extension nothing here knows how to load is a MODEL_UNRESOLVED, not a
		// silent success: the panel shows the path and offers the picker.
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_MODEL_UNRESOLVED;
		return;
	}

	m_xSkeleton.Set(pxSkeleton);
	m_pxSkeletonInstance = Flux_SkeletonInstance::CreateFromAsset(pxSkeleton);
	if (m_pxSkeletonInstance == nullptr)
	{
		m_xSkeleton.Clear();
		m_xPreviewMesh.Clear();
		m_xPreviewModel.Clear();
		m_bPreviewIsBareMesh = false;
		m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_SKELETON_UNRESOLVED;
		return;
	}

	m_xController.Initialize(m_pxSkeletonInstance);
	ArmDirectPlay();
	m_eRigStatus = ZENITH_ANIMPREVIEW_RIG_OK;
}

void Zenith_AnimationPreviewSession::ArmDirectPlay()
{
	// Non-OWNING: the clip lives on this session (see the member ordering note in
	// the header), so the collection must borrow rather than take it.
	m_xController.GetClipCollection().AddClipReference(&m_xClip);
	m_xController.PlayClip(m_xClip.GetName(), 0.0f);
	m_xController.SeekDirectPlay(0.0f);
}

bool Zenith_AnimationPreviewSession::SetRigOverride(
	const std::string& strSkeletonPath, const std::string& strPreviewModelPath)
{
	if (!m_bOpen)
	{
		return false;
	}

	m_strSkeletonPath = Zenith_AssetRegistry::NormalizeAssetPath(strSkeletonPath);
	m_strPreviewModelPath = Zenith_AssetRegistry::NormalizeAssetPath(strPreviewModelPath);
	ResolveRig();

	if (m_eRigStatus == ZENITH_ANIMPREVIEW_RIG_OK)
	{
		RememberRig();
	}
	return m_eRigStatus == ZENITH_ANIMPREVIEW_RIG_OK;
}

bool Zenith_AnimationPreviewSession::TryLoadRememberedRig(
	std::string& strOutSkeleton, std::string& strOutModel) const
{
	if (m_pxPrefs == nullptr || m_strClipAssetPath.empty())
	{
		return false;
	}
	Zenith_EditorPrefs_AnimRigChoice xChoice;
	if (!m_pxPrefs->TryGetAnimRigChoice(m_strClipAssetPath, xChoice))
	{
		return false;
	}
	strOutSkeleton = xChoice.m_strSkeletonPath;
	strOutModel = xChoice.m_strPreviewModelPath;
	return true;
}

void Zenith_AnimationPreviewSession::RememberRig() const
{
	if (m_pxPrefs == nullptr || m_strClipAssetPath.empty())
	{
		return;
	}
	m_pxPrefs->SetAnimRigChoice(m_strClipAssetPath, m_strSkeletonPath, m_strPreviewModelPath);
}

//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------

void Zenith_AnimationPreviewSession::Tick(float fDt)
{
	if (!m_bOpen || !m_bPlaying || m_eRigStatus != ZENITH_ANIMPREVIEW_RIG_OK)
	{
		return;
	}
	// ★ A DRAG SUSPENDS EVALUATION. The clip and the drag are two writers of the
	// same bone rotations, and a per-frame evaluate between two mouse moves would
	// stomp the drag — the bone would twitch back to the clip's pose on every
	// frame the user was not moving fast enough.
	if (m_bBoneDragActive)
	{
		return;
	}
	// No drive guard here: this controller is the session's OWN (D30) and nothing
	// else can reach it. The guard exists for the entity controller the animator
	// inspector shares.
	m_xController.Update(fDt);
	MarkPickSetDirty();
}

bool Zenith_AnimationPreviewSession::Seek(float fTimeSeconds)
{
	if (!m_bOpen || m_eRigStatus != ZENITH_ANIMPREVIEW_RIG_OK)
	{
		return false;
	}
	if (!m_xController.SeekDirectPlay(fTimeSeconds))
	{
		return false;
	}

	// ★ SEEKING IS THE EXPLICIT DISCARD OF AN UNKEYED POSE (§4.4). The controller
	// has just re-evaluated every bone from the clip, so whatever the drag left
	// behind is gone — the flag has to go with it or the panel would keep warning
	// about a pose that no longer exists.
	//
	// ★ THE SELECTION SURVIVES. It names a BONE, not a moment; a scrub is not a
	// reason to stop pointing at the elbow.
	m_bUnkeyedPose = false;
	MarkPickSetDirty();
	return true;
}

float Zenith_AnimationPreviewSession::GetNormalizedTime() const
{
	const float fDuration = m_xClip.GetDuration();
	if (fDuration <= 0.0f)
	{
		return 0.0f;
	}
	return m_xController.GetDirectPlayTime() / fDuration;
}

//-----------------------------------------------------------------------------
// Pose authoring (Phase 4) — bone selection, derived pose, pick geometry, drag
//-----------------------------------------------------------------------------

u_int Zenith_AnimationPreviewSession::GetBoneCount() const
{
	return (m_pxSkeletonInstance != nullptr) ? static_cast<u_int>(m_pxSkeletonInstance->GetNumBones()) : 0u;
}

void Zenith_AnimationPreviewSession::SelectBone(u_int uBoneIndex)
{
	// An out-of-range index CLEARS rather than being stored. Storing it would
	// give HasBoneSelection() a true that every consumer then has to re-validate,
	// and the first one that forgot would index a bone that is not there.
	m_uSelectedBoneIndex = (uBoneIndex < GetBoneCount()) ? uBoneIndex : kuINVALID_BONE_SELECTION;
}

void Zenith_AnimationPreviewSession::SetSessionModelMatrix(const Zenith_Maths::Matrix4& xModel)
{
	m_xSessionModelMatrix = xModel;
	// The pick shapes are stored in WORLD space, so the model matrix is baked
	// into every one of them; moving it invalidates the lot.
	MarkPickSetDirty();
}

void Zenith_AnimationPreviewSession::RefreshDerivedPose()
{
	if (m_pxSkeletonInstance != nullptr)
	{
		m_pxSkeletonInstance->ComputeSkinningMatrices();
	}
	MarkPickSetDirty();
}

const Zenith_BonePickSet& Zenith_AnimationPreviewSession::GetBonePickSet() const
{
	if (!m_bPickSetDirty)
	{
		return m_xPickSet;
	}

	m_xPickSet.m_xShapes.Clear();
	m_xPickSet.m_fSkeletonExtent = 0.0f;

	const Zenith_SkeletonAsset* pxAsset =
		(m_pxSkeletonInstance != nullptr) ? m_pxSkeletonInstance->GetSourceSkeleton() : nullptr;
	if (m_pxSkeletonInstance != nullptr && pxAsset != nullptr)
	{
		Zenith_BuildBonePickSet(*m_pxSkeletonInstance, *pxAsset, m_xSessionModelMatrix, m_xPickSet);
	}

	m_bPickSetDirty = false;
	++m_uPickSetBuildCount;
	return m_xPickSet;
}

bool Zenith_AnimationPreviewSession::PickBone(const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir, u_int& uOutBone) const
{
	const Zenith_BonePickSet& xSet = GetBonePickSet();
	// Distance is discarded here on purpose: a caller picking a bone wants the
	// bone. Zenith_RaycastBonePickSet leaves BOTH outputs untouched on a miss, so
	// the local is never read uninitialised.
	float fDistance = 0.0f;
	return Zenith_RaycastBonePickSet(xSet, xRayOrigin, xRayDir, uOutBone, fDistance);
}

Zenith_Maths::Quat Zenith_AnimationPreviewSession::GetBoneLocalRotation(u_int uBoneIndex) const
{
	if (m_pxSkeletonInstance == nullptr || uBoneIndex >= GetBoneCount())
	{
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	}
	return m_pxSkeletonInstance->GetBoneLocalRotation(uBoneIndex);
}

bool Zenith_AnimationPreviewSession::BeginBoneDrag(u_int uBoneIndex)
{
	if (m_pxSkeletonInstance == nullptr || uBoneIndex >= GetBoneCount())
	{
		return false;
	}
	if (m_bBoneDragActive)
	{
		// A drag already in flight is REFUSED rather than retargeted: the second
		// caller's mouse-up would end the first caller's transaction, and the
		// latched initial rotation would belong to neither bone.
		return false;
	}

	m_bBoneDragActive = true;
	m_uDragBoneIndex = uBoneIndex;
	m_xDragInitialRotation = m_pxSkeletonInstance->GetBoneLocalRotation(uBoneIndex);
	return true;
}

bool Zenith_AnimationPreviewSession::UpdateBoneDrag(const Zenith_Maths::Quat& xNewBoneLocalRotation)
{
	if (!m_bBoneDragActive || m_pxSkeletonInstance == nullptr)
	{
		return false;
	}
	if (m_uDragBoneIndex >= GetBoneCount())
	{
		return false;
	}

	// SetBoneLocalTransform takes all three components and there is no
	// rotation-only overload, so position and scale are read back unchanged.
	m_pxSkeletonInstance->SetBoneLocalTransform(m_uDragBoneIndex,
		m_pxSkeletonInstance->GetBoneLocalPosition(m_uDragBoneIndex),
		xNewBoneLocalRotation,
		m_pxSkeletonInstance->GetBoneLocalScale(m_uDragBoneIndex));

	// ★ THE §3.2 PRECONDITION, HONOURED AT THE ONE PLACE A LIVE POSE IS WRITTEN.
	// Without this the pick shapes and any overlay would trail the bone by one
	// drag frame, which reads as lag rather than as a missing call.
	RefreshDerivedPose();

	m_bUnkeyedPose = true;
	return true;
}

void Zenith_AnimationPreviewSession::EndBoneDrag()
{
	// ★ THE POSE STAYS WHERE THE DRAG LEFT IT. Nothing is written to the document
	// here and nothing is pushed on an undo stack — that is WU-4.3's Set Key, and
	// with auto-key off it may never happen, which is exactly what HasUnkeyedPose
	// is for.
	m_bBoneDragActive = false;
	m_uDragBoneIndex = kuINVALID_BONE_SELECTION;
}

//-----------------------------------------------------------------------------
// Preview slot + view staging
//-----------------------------------------------------------------------------

bool Zenith_AnimationPreviewSession::ReclaimPreviewSlot()
{
	if (!m_bOpen)
	{
		return false;
	}
	return Flux_PreviewSlotArbiter::Claim(this, m_strDisplayName) || HasPreviewSlot();
}

void Zenith_AnimationPreviewSession::OrbitCamera(float fDeltaYaw, float fDeltaPitch)
{
	m_fCameraYaw += fDeltaYaw;
	m_fCameraPitch = Flux_PreviewClampPitch(m_fCameraPitch + fDeltaPitch);
}

void Zenith_AnimationPreviewSession::ZoomCamera(float fDelta)
{
	m_fCameraDistance = Flux_PreviewApplyZoom(m_fCameraDistance, fDelta);
}

void Zenith_AnimationPreviewSession::GetCameraOrbit(float& fOutYaw, float& fOutPitch, float& fOutDistance) const
{
	fOutYaw = m_fCameraYaw;
	fOutPitch = m_fCameraPitch;
	fOutDistance = m_fCameraDistance;
}

void Zenith_AnimationPreviewSession::UpdatePreviewView()
{
	// Headless unit runs (and any run before Flux is up) have no registry to stage
	// into. TryGetFluxGraphics is the one accessor that answers that without a
	// separate Has* call.
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr)
	{
		return;
	}

	Flux_RenderViewRegistry& xViews = pxGraphics->RenderViews();

	if (!m_bOpen || !HasPreviewSlot())
	{
		// ★ A DISPOSSESSED SESSION MUST NOT DEACTIVATE THE VIEW. The slot is shared;
		// whoever holds it is very likely staging into it this same frame, and
		// tearing the view down from here would flicker their preview off. Only a
		// frame in which NOBODY owns the slot is safe to deactivate on.
		if (Flux_PreviewSlotArbiter::GetOwner() == nullptr)
		{
			if (xViews.SetViewActive(kuFluxViewSlotPreviewMaterial, false))
			{
				g_xEngine.FluxRenderer().RequestGraphRebuild();
			}
		}
		return;
	}

	if (xViews.SetViewActive(kuFluxViewSlotPreviewMaterial, true))
	{
		// The active view set changed: per-view transients and passes must be
		// (de)declared, so the next frame recompiles the graph from scratch.
		g_xEngine.FluxRenderer().RequestGraphRebuild();
	}

	// Staged exactly the way Flux_MaterialPreviewController stages it, through the
	// SAME pure builders — two different fills of one slot's constants would make
	// the preview's framing depend on which editor last touched it.
	Flux_RenderView& xView = xViews.View(kuFluxViewSlotPreviewMaterial);
	xView.m_xTargetDims = Zenith_Maths::UVector2(kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);

	Flux_ViewConstants& xVC = xView.m_xConstants;
	Flux_PreviewBuildViewConstants(m_fCameraYaw, m_fCameraPitch, m_fCameraDistance, xVC);
	xVC.m_xSunDir_Pad    = Zenith_Maths::Vector4(Flux_PreviewLightDir(0.8f, 0.7f), 0.0f);
	xVC.m_xSunColour_Pad = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 3.0f);
	xVC.m_uViewFlags     = 0u;
	// The MATERIAL preview's slot, shared with the material editor through the
	// arbiter until D3 moves this session onto kuFluxViewSlotPreviewAnim.
	xVC.m_uViewSlot      = kuFluxViewSlotPreviewMaterial;
	// The preview view never jitters and never runs velocity/TAA, but the GPU cull
	// reads m_xViewProjMatNoJitter for EVERY active view — so stage it.
	xVC.m_xViewProjMatNoJitter     = xVC.m_xViewProjMat;
	xVC.m_xPrevViewProjMatNoJitter = xVC.m_xViewProjMat;
	xVC.m_xJitterUV_PrevJitterUV   = Zenith_Maths::Vector4(0.0f);

	// ★ NO MESH IS SUBMITTED YET, AND THAT IS A REPORTED GAP, NOT AN OVERSIGHT.
	// The external-item seam the material preview draws through
	// (Flux_RendererImpl::Flux_ExternalSceneItem) carries a world matrix, a
	// Flux_MeshInstance and a material — and NO skeleton. An animated preview needs
	// the compute-skinning path to know which Flux_SkeletonInstance to skin the
	// instance with, so submitting through it today would draw the mesh frozen at
	// bind pose while the session's rig animated underneath, which looks like a
	// broken clip rather than a missing feature. Extending that struct means
	// editing Flux_RendererImpl.h, which this unit does not own.
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_AnimationPreviewSession.Tests.inl"
#endif

#endif // ZENITH_TOOLS
