#pragma once
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachineDef.h"
#include "Collections/Zenith_HashMap.h"

// Forward declarations
class Flux_AnimationClipCollection;
class Zenith_SkeletonAsset;

//=============================================================================
// Flux_AnimatorStateInfo
// Runtime state introspection (Unity's GetCurrentAnimatorStateInfo())
//=============================================================================
struct Flux_AnimatorStateInfo
{
	std::string m_strStateName;
	float m_fNormalizedTime = 0.0f;    // fractional = progress [0-1], integer = loop count
	float m_fLength = 0.0f;            // clip duration in seconds
	float m_fSpeed = 1.0f;
	bool m_bHasLooped = false;         // true once normalized time has exceeded 1.0 (past first cycle)
	bool m_bIsTransitioning = false;
	float m_fTransitionProgress = 0.0f;

	bool IsName(const char* szName) const;
};

//=============================================================================
// Flux_AnimationStateMachine
//
// ★ THE MUTABLE HALF. Everything AUTHORED — states, transitions, any-state
// transitions, the default state, the parameter declarations — lives in a
// Flux_AnimationStateMachineDef (Flux_AnimationStateMachineDef.h). What is left
// here is one instance's playback: which state it is in, which transition is in
// flight, and the two FLUX_MAX_BONES poses that transition blends between.
//
// ★ THE DEF IS OWNED BY VALUE, AND BuildFromDef COPIES (WU-6.1). A non-owning
// pointer would let two instances of one def share the def's blend trees — and a
// blend-tree leaf carries its own PLAYHEAD (Flux_BlendTreeNode_Clip::
// m_fCurrentTimestamp). Two characters on "the same controller" would then share
// one clock and step each other's clips: not a def/instance split at all, but
// two instances wearing one instance's state. Copying is also what keeps the
// IMPERATIVE authoring path — AddState / AddTransition / GetParameters().Add* —
// working exactly as before: those forward into the owned def, which is how
// every game and every existing test builds a state machine today.
//
// ★ PARAMETERS ARE THE CONTROLLER'S (D42). GetParameters() returns the shared
// live set when one has been published (Flux_AnimationController owns exactly
// one and publishes it to the top-level SM, every layer's SM and every sub-SM),
// and the def's DECLARATION table otherwise. That is what makes a value set on
// the animator visible to a condition inside a layer's sub-state machine — which
// it was not: a controller with layers has a NULL m_pxStateMachine, so
// Flux_AnimationController::SetFloat was a silent no-op for every layered game.
//=============================================================================
class Flux_AnimationStateMachine
{
public:
	Flux_AnimationStateMachine() = default;
	Flux_AnimationStateMachine(const std::string& strName);
	~Flux_AnimationStateMachine();

	// Owns runtime state + its def; copying one would double-own both.
	Flux_AnimationStateMachine(const Flux_AnimationStateMachine&) = delete;
	Flux_AnimationStateMachine& operator=(const Flux_AnimationStateMachine&) = delete;

	//=========================================================================
	// Definition
	//=========================================================================

	// Replace this machine's definition with a COPY of xDef and reset the
	// runtime half (no current state, no transition in flight). A def names its
	// clips rather than pointing at them, so pass the collection that owns them —
	// or call ResolveClipReferences yourself afterwards — or every clip leaf will
	// pose the bind pose.
	void BuildFromDef(const Flux_AnimationStateMachineDef& xDef,
		Flux_AnimationClipCollection* pxClipCollection = nullptr);

	Flux_AnimationStateMachineDef& GetDef() { return m_xDef; }
	const Flux_AnimationStateMachineDef& GetDef() const { return m_xDef; }

	//=========================================================================
	// Imperative authoring — forwards into the owned def
	//=========================================================================

	Flux_AnimationState* AddState(const std::string& strName) { return m_xDef.AddState(strName); }
	void RemoveState(const std::string& strName);
	Flux_AnimationState* GetState(const std::string& strName) { return m_xDef.GetState(strName); }
	const Flux_AnimationState* GetState(const std::string& strName) const { return m_xDef.GetState(strName); }
	bool HasState(const std::string& strName) const { return m_xDef.HasState(strName); }

	// Default state (entry point)
	void SetDefaultState(const std::string& strName) { m_xDef.SetDefaultState(strName); }
	const std::string& GetDefaultStateName() const { return m_xDef.GetDefaultStateName(); }

	// Get all states for iteration
	const Zenith_HashMap<std::string, Flux_AnimationState*>& GetStates() const { return m_xDef.GetStates(); }

	// Name
	const std::string& GetName() const { return m_xDef.GetName(); }
	void SetName(const std::string& strName) { m_xDef.SetName(strName); }

	// Any-State transitions (fire from any current state)
	void AddAnyStateTransition(const Flux_StateTransition& xTransition) { m_xDef.AddAnyStateTransition(xTransition); }
	void RemoveAnyStateTransition(uint32_t uIndex) { m_xDef.RemoveAnyStateTransition(uIndex); }
	const Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() const { return m_xDef.GetAnyStateTransitions(); }
	Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() { return m_xDef.GetAnyStateTransitions(); }

	// Resolve clip references in blend trees (by name, through the collection)
	void ResolveClipReferences(Flux_AnimationClipCollection* pxCollection) { m_xDef.ResolveClipReferences(pxCollection); }

	//=========================================================================
	// Runtime
	//=========================================================================

	// Current state
	Flux_AnimationState* GetCurrentState() const { return m_pxCurrentState; }
	const std::string& GetCurrentStateName() const;

	// Force state change (ignores transitions)
	void SetState(const std::string& strStateName);

	// Parameters. The published controller-wide set when there is one (D42), the
	// def's declaration table otherwise — which is what a standalone state machine
	// (a unit test, an editor scratch graph) reads and writes.
	Flux_AnimationParameters& GetParameters() { return m_pxSharedParameters ? *m_pxSharedParameters : m_xDef.GetParameterDeclarations(); }
	const Flux_AnimationParameters& GetParameters() const { return m_pxSharedParameters ? *m_pxSharedParameters : m_xDef.GetParameterDeclarations(); }
	void SetSharedParameters(Flux_AnimationParameters* pxSharedParams) { m_pxSharedParameters = pxSharedParams; }
	Flux_AnimationParameters* GetSharedParameters() const { return m_pxSharedParameters; }

	// Update the state machine (call each frame)
	// Returns the resulting skeleton pose
	void Update(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton);

	// Check if currently in a transition
	bool IsTransitioning() const { return m_pxActiveTransition != nullptr; }

	//=========================================================================
	// Event-span collection (WU-5A / D34, D37)
	//
	// Hand over the clip-event spans this state machine's evaluated leaves
	// produced during the last Update, then CLEAR them. Call once per Update,
	// after it — see Flux_BlendTreeNode::CollectEventSpans for why a span that
	// is never collected is a span that fires again next frame.
	//
	// ★ DURING A CROSSFADE EXACTLY ONE SIDE IS COLLECTED (D37): the one at
	// weight >= 0.5, ties (exactly 0.5) to the TARGET. Both states' clips are
	// audible in the pose at once, but a footstep is not a pose — firing both
	// sides' events across a transition double-fires every one of them.
	//
	// ★ AND TODAY THE OUTGOING SIDE HAS NOTHING TO GIVE. UpdateTransition
	// evaluates only the TARGET state; the source contributes a pose SNAPSHOT
	// frozen at StartTransition, so its blend tree does not advance and produces
	// no span at all. The >= 0.5 branch is still written both ways round rather
	// than short-circuited, because the rule is about which side MAY emit, and
	// the day the source starts advancing is not the day to rediscover that.
	//
	// pxOutSpans may be NULL: "walk and clear, discard" — what a silenced layer
	// needs (D36).
	//=========================================================================
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans);

	// State info query (Unity's GetCurrentAnimatorStateInfo)
	Flux_AnimatorStateInfo GetCurrentStateInfo() const;

	// Force-crossfade to a named state, bypassing transition conditions (Unity's Animator.CrossFade)
	void CrossFade(const std::string& strStateName, float fDuration = 0.15f);

	// Serialization. The DEF is what is written; the runtime half is not data.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	void StartTransition(const Flux_StateTransition& xTransition);
	void UpdateTransition(float fDt, const Zenith_SkeletonAsset& xSkeleton);
	void CompleteTransition();
	void EvaluateState(Flux_AnimationState* pxState, float fDt, Flux_SkeletonPose& xOutPose, const Zenith_SkeletonAsset& xSkeleton);

	// Drop every pointer into the def and cancel any transition in flight. Called
	// whenever the def underneath is replaced.
	void ResetRuntime();

	// WU-5A: one state's spans — its blend tree's, or its sub-state machine's.
	static void CollectStateEventSpans(Flux_AnimationState* pxState, Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans);

	// Check any-state transitions (skips transitions targeting current state and below iMinPriority)
	const Flux_StateTransition* CheckAnyStateTransitions(int32_t iMinPriority = INT32_MIN);

	// The AUTHORED half, owned. See the class comment for why it is a copy.
	Flux_AnimationStateMachineDef m_xDef;

	// Runtime state
	Flux_AnimationState* m_pxCurrentState = nullptr;
	Flux_CrossFadeTransition* m_pxActiveTransition = nullptr;
	Flux_AnimationState* m_pxTransitionTargetState = nullptr;
	bool m_bActiveTransitionInterruptible = true;  // Whether the current active transition can be interrupted
	int32_t m_iActiveTransitionPriority = 0;       // Priority of the current active transition
	Flux_AnimationParameters* m_pxSharedParameters = nullptr;  // Non-owned: the controller's live set (D42)

	// Poses
	Flux_SkeletonPose m_xCurrentPose;
	Flux_SkeletonPose m_xTargetPose;
};
