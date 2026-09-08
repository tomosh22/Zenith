#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"   // Zenith_Maths::Vector3 in the IK accessor signatures
                                  // (previously pulled in transitively via the
                                  // now-removed Flux_AnimationController.h include).
#include <string>
#include <cstdint>

// Forward declarations only — this header includes NO Flux header (Wave-19
// ownership-relocation, the TWIN of WS18's Terrain keystone). The heavy
// Flux_AnimationController that used to be a by-value member here now lives in
// an owning Flux subsystem (Flux_AnimationControllerStore, reached via
// g_xEngine.AnimationControllers()), keyed by this entity's stable EntityID
// slot. This component is a thin handle: every accessor forwards into the
// store-owned controller (the forwarding bodies live in the .cpp, which DOES
// include the Flux animation headers — those .cpp edges are allow-listed). A
// forward declaration is NOT an #include, so it introduces no cross-layer
// coupling — the layering gate scans #include edges, not forward decls.
class Flux_AnimationController;
class Flux_AnimationStateMachine;
class Flux_AnimationClip;
class Flux_IKSolver;
class Flux_AnimationLayer;
struct Flux_AnimatorStateInfo;

// Flux_AnimationUpdateMode is an `enum : uint8_t` (Flux_AnimationController.h).
// An opaque enum declaration with the fixed underlying type lets this header
// name it by value in signatures without pulling the Flux header in.
enum Flux_AnimationUpdateMode : uint8_t;

class Zenith_ModelComponent;

// Forward declarations for RegisterProperties (cycle-avoidance — see TransformComponent.h).
template<typename T> class Zenith_Vector;
struct Zenith_PropertyDescriptor;

//=============================================================================
// Zenith_AnimatorStateInfo
//
// EC-side mirror of Flux_AnimatorStateInfo (Flux/MeshAnimation/
// Flux_AnimationStateMachine.h). Field names/types match the Flux POD verbatim,
// so existing call sites that read m_strStateName / m_fNormalizedTime /
// m_bIsTransitioning / etc. and call IsName() work unchanged.
//
// Why a mirror at all: GetCurrentAnimatorStateInfo() returns this struct BY
// VALUE. A by-value return needs a COMPLETE type — a forward declaration is
// insufficient. Returning the Flux type would force this header to #include the
// Flux state-machine header, reintroducing the very EC->Flux coupling Wave-19
// removes. The mirror keeps the by-value return EC-local. The component .cpp
// fills it from the Flux result; an implicit conversion-to-Flux operator
// (DECLARED here against the forward-declared Flux type, DEFINED out-of-line in
// the .cpp where the Flux type is complete) keeps source-compatibility for the
// one caller (Combat_AnimationController.h:273) that assigns the result into a
// `Flux_AnimatorStateInfo` local — that caller already includes the Flux header,
// so the conversion resolves there, NOT in this header.
//=============================================================================
struct Zenith_AnimatorStateInfo
{
	std::string m_strStateName;
	float m_fNormalizedTime = 0.0f;    // fractional = progress [0-1], integer = loop count
	float m_fLength = 0.0f;            // clip duration in seconds
	float m_fSpeed = 1.0f;
	bool m_bHasLooped = false;         // true once normalized time has exceeded 1.0 (past first cycle)
	bool m_bIsTransitioning = false;
	float m_fTransitionProgress = 0.0f;

	bool IsName(const char* szName) const { return m_strStateName == szName; }

	// Implicit conversion to the Flux POD. Declared against the forward-declared
	// Flux_AnimatorStateInfo; defined out-of-line in the .cpp. Only TUs that
	// already see the complete Flux type (the component .cpp; Combat, which
	// includes Flux_AnimationStateMachine.h) can invoke it — no Flux include
	// leaks into this header.
	operator Flux_AnimatorStateInfo() const;
};

class Zenith_AnimatorComponent
{
public:
	Zenith_AnimatorComponent(Zenith_Entity& xEntity);
	~Zenith_AnimatorComponent();

	// Property registration for prefab-variant overrides.
	static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties);

	// Move semantics (required for component pools)
	Zenith_AnimatorComponent(Zenith_AnimatorComponent&& xOther) noexcept;
	Zenith_AnimatorComponent& operator=(Zenith_AnimatorComponent&& xOther) noexcept;

	// No copy
	Zenith_AnimatorComponent(const Zenith_AnimatorComponent&) = delete;
	Zenith_AnimatorComponent& operator=(const Zenith_AnimatorComponent&) = delete;

	// ========== ECS Lifecycle (auto-called via ComponentMeta) ==========
	void OnStart();              // Auto-discovers skeleton from ModelComponent
	void OnUpdate(float fDt);    // Evaluates state machine, updates pose, uploads GPU
	void OnDestroy();

	// ========== Controller Access ==========
	// Forwards to the store-owned controller (Wave-19). Same return type and
	// reference semantics as the previous by-value member accessor.
	Flux_AnimationController& GetController();
	const Flux_AnimationController& GetController() const;

	// ========== State Machine Parameter Shortcuts ==========
	void SetFloat(const std::string& strName, float fValue);
	void SetInt(const std::string& strName, int32_t iValue);
	void SetBool(const std::string& strName, bool bValue);
	void SetTrigger(const std::string& strName);
	float GetFloat(const std::string& strName) const;
	int32_t GetInt(const std::string& strName) const;
	bool GetBool(const std::string& strName) const;

	// ========== Convenience ==========
#ifdef ZENITH_TOOLS
	void PlayAnimation(const std::string& strClipName, float fBlendTime = 0.15f);
#endif
	void CrossFade(const std::string& strStateName, float fDuration = 0.15f);
	void Stop();
	void SetPaused(bool bPaused);
	bool IsPaused() const;
	void SetPlaybackSpeed(float fSpeed);
	float GetPlaybackSpeed() const;

	// ========== Clip Management ==========
	Flux_AnimationClip* AddClipFromFile(const std::string& strPath);
	Flux_AnimationClip* GetClip(const std::string& strName);

	// ========== Animator Controller Asset (.zanimctrl, WU-6.2) ==========
	//
	// Acquire the .zanimctrl at strPath and rebuild this entity's controller from
	// it: clips, the optional top-level state machine, and every layer (id, name,
	// weight, blend mode, emit-events flag, bone mask and its own state machine).
	// Bone masks resolve against the rig this entity's ModelComponent is animating.
	//
	// ★ THE PATH IS RECORDED AND SERIALIZED (component schema 2). This component
	// still writes the whole controller INLINE into a .zscen (see
	// WriteToDataStream) — the inline bytes remain what a scene restores from —
	// and the asset path is written AFTER them, so a loaded entity can say which
	// .zanimctrl it came from. A successful load stores
	// Zenith_AssetRegistry::NormalizeAssetPath(strPath); a failed ASSET LOOKUP
	// leaves the previously recorded path alone (a lookup that found nothing has
	// learned nothing about what this entity is animating).
	//
	// Returns false on a controller that could not be fully built — see
	// Flux_AnimationController::BuildFromControllerDef; every cause is logged with
	// the offending path.
	bool LoadControllerAsset(const std::string& strPath);

	// The .zanimctrl this entity's controller was last built FROM, normalized, or
	// empty when the controller was built in code / restored only from scene bytes.
	//
	// ★ TWO THINGS IT DOES NOT PROMISE, and both are deliberate:
	//   * It can name an asset the live controller was NOT fully built from. The
	//     path is recorded on an INCOMPLETE build too (LoadControllerAsset returned
	//     false because a clip or a bone mask did not resolve), because "which asset
	//     was asked for" is exactly the question a diagnostic asks after a failure.
	//     The BOOL is the completeness verdict; this getter is not.
	//   * It is NOT updated by the "Controller" prefab-variant property override,
	//     which deserializes straight into the store-owned controller (see
	//     RegisterProperties in the .cpp) without going through any asset. A variant
	//     that swaps the whole animation graph therefore leaves this naming whatever
	//     the base was built from — or nothing at all.
	const std::string& GetControllerAssetPath() const { return m_strControllerAssetPath; }

	// ========== State Machine ==========
	Flux_AnimationStateMachine& GetStateMachine();
	Flux_AnimationStateMachine* CreateStateMachine(const std::string& strName = "Default");
	bool HasStateMachine() const;

	// ========== State Info Query ==========
	// Returns the EC-side mirror (see Zenith_AnimatorStateInfo above). Implicitly
	// convertible to Flux_AnimatorStateInfo for callers that include the Flux
	// header. The .cpp fills it from Flux_AnimationController::GetCurrentAnimatorStateInfo.
	Zenith_AnimatorStateInfo GetCurrentAnimatorStateInfo() const;

	// ========== IK ==========
	void SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPos, float fWeight = 1.0f);
	void SetIKTargetModelSpace(const std::string& strChainName, const Zenith_Maths::Vector3& xModelSpacePos, float fWeight = 1.0f);
	// Variant that also drives the end-effector (tip-bone) orientation — both
	// position and rotation in model (skeleton) space. Used for tool aiming
	// (e.g. squaring a racket blade to the ball).
	void SetIKTargetModelSpace(const std::string& strChainName, const Zenith_Maths::Vector3& xModelSpacePos, const Zenith_Maths::Quat& xModelSpaceRot, float fWeight = 1.0f);
	void ClearIKTarget(const std::string& strChainName);

	// ========== Update Mode ==========
	void SetUpdateMode(Flux_AnimationUpdateMode eMode);
	Flux_AnimationUpdateMode GetUpdateMode() const;

	// ========== Initialization State ==========
	bool IsInitialized() const;

	// ========== Serialization ==========
	//
	// On-disk schema version of this component's payload:
	//   1 : the inline controller bytes ONLY (every .zscen written before the
	//       controller-asset path existed).
	//   2 : the inline controller bytes, then the normalized .zanimctrl path.
	// The meta registry stamps this per-component value into the file OUTSIDE the
	// size-prefixed payload and hands the persisted value back to the 2-arg reader
	// (Zenith_ComponentMeta / DeserializeEntityComponents).
	static constexpr u_int uSchemaVersion = 2;

	void WriteToDataStream(Zenith_DataStream& xStream) const;

	// ★ THE REFUSAL CHANNEL, NOT A MIGRATION. A bool versioned reader states a
	// verdict on the payload: false means "this schema is not one I can read", and
	// the wrapper propagates it out through DeserializeEntityComponents to
	// Zenith_SceneData::LoadFromDataStream. There is deliberately NO schema-1
	// branch — a legacy path is exactly what this repo does not keep — so a v1
	// record is refused BEFORE a single payload byte is consumed, and the bounded
	// per-component realign puts the cursor back on the next record so every later
	// component and every later entity still loads.
	bool ReadFromDataStream(Zenith_DataStream& xStream, u_int uPersistedSchemaVersion);

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Resolve the store-owned controller for this component's entity. Defined
	// in the .cpp (forwards to g_xEngine.AnimationControllers().Get(...)).
	Flux_AnimationController& Controller() const;

	void TryDiscoverSkeleton();
	void UpdateWorldMatrix();
	void SyncModelInstanceAnimation();

#ifdef ZENITH_TOOLS
	void RenderStatusAndStateInfoSection();
	void RenderAnimationClipsSection();
	void RenderPlaybackControlsSection();
	void RenderParametersSection();
	void RenderStateMachineSection();
	void RenderLayersSection();
	void RenderUpdateModeSection();
#endif

	Zenith_Entity m_xParentEntity;

	// Cached pointer to the store-owned controller (Wave-19). HEAP-STABLE: the
	// store allocates each controller with `new`, so this pointer survives the
	// store's internal vector growth/compaction, a component-pool relocation
	// (swap-and-pop / Grow) AND a cross-scene MoveEntityToScene (the controller
	// is keyed by the stable EntityID slot). OnStart points it at the
	// GetOrCreate result; OnUpdate dereferences it directly (O(1), no per-frame
	// store lookup, no hash). Nulled on a moved-from component.
	Flux_AnimationController* m_pxController = nullptr;

	Zenith_ModelComponent* m_pxCachedModelComponent = nullptr;
	uint32_t m_uDiscoveryRetryCount = 0;

	// The .zanimctrl LoadControllerAsset last resolved, normalized for
	// serialization. SERIALIZED (schema 2) and TRANSFERRED BY BOTH MOVE
	// OPERATIONS: the component pool relocates by move-CONSTRUCTION only
	// (swap-and-pop and Grow both move-construct into the new slot), so a
	// constructor that dropped this would lose the path on the 17th animator in a
	// scene with nothing logged; move assignment is hand-written-correctness rather
	// than a pool path, and is kept in step for the same reason.
	std::string m_strControllerAssetPath;

	// Set true on the SOURCE of a move. A moved-from component must NOT Destroy
	// the store entry (the moved-TO component now owns the same EntityID-keyed
	// controller) — so its dtor/OnDestroy skip Destroy. Guarantees EXACTLY ONE
	// Destroy per entity, not per component instance.
	bool m_bMovedOut = false;

	// Lets the ECS regression suite inspect m_pxController / m_bMovedOut to pin
	// the relocation invariants (exactly-one-controller, moved-to resolves the
	// same controller, moved-from is safe).
	friend class Zenith_UnitTests;
};
