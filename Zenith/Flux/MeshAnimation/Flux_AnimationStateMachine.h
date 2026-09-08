#pragma once
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "Flux_AnimationStateMachineDef.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"

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
	// HOT RELOAD (WU-6.4 / D45)
	//
	// ★ BuildFromDef IS A DEMOLITION, AND THAT IS THE WHOLE PROBLEM. It calls
	// ResetRuntime and then CopyFrom, so a machine rebuilt from an edited def
	// comes back with NO current state, NO transition in flight and every blend
	// tree's playhead at zero — the character snaps to the default state at
	// frame 0 the instant an author touches an unrelated transition's duration.
	// Reloading is the same rebuild with the playback put back afterwards.
	//
	// ★ THE SNAPSHOT HOLDS NAMES AND FRACTIONS, NEVER POINTERS. Every runtime
	// pointer on this object (m_pxCurrentState, m_pxTransitionTargetState) names
	// a Flux_AnimationState that CopyFrom is about to delete, so a snapshot
	// carrying them would be a snapshot of freed memory by the time it was used.
	// A NAME is the only identity that spans the two defs, which is also what
	// makes "a renamed state does not survive" fall out of the design rather
	// than needing a rule of its own.
	//
	// ★ NOT A SYNCHRONISATION POINT, AND IT DOES NOT CREATE ONE (the same rule
	// Flux_AnimationClip::ReplaceContentsFrom carries, D27). Capture, rebuild
	// and restore are three plain non-atomic writes over live data; THE CALLER
	// GUARANTEES NO ANIMATION UPDATE IS IN FLIGHT. The editor calls this from
	// the main thread between frames.
	//=========================================================================

	// ONE LEVEL of one instance's playback, in terms that outlive the def it
	// points into.
	struct RuntimeLevelSnapshot
	{
		// Empty when the machine at this level had not entered a state yet
		// (nothing has ticked it) — a restore then does nothing and the first
		// Update enters the new def's default, which is exactly right.
		std::string m_strCurrentStateName;
		float m_fCurrentNormalizedTime = 0.0f;

		// ★ AN ACTIVE TRANSITION IS CANCELLED, NOT RESUMED, and it is cancelled
		// ONTO ITS TARGET (D45). Resuming would need the source pose snapshot
		// the Flux_CrossFadeTransition is holding, which is a pose over a
		// skeleton the reload may have changed the state set of; and landing on
		// the SOURCE would run the transition's conditions again from scratch,
		// re-firing a one-shot the player has already spent. The target is where
		// the machine was going, so that is where it arrives.
		std::string m_strTransitionTargetName;
		float m_fTransitionTargetNormalizedTime = 0.0f;
		bool m_bTransitioning = false;
	};

	// One instance's playback, THROUGH THE CONTAINER STATES IT WAS INSIDE.
	//
	// ★ INDEX 0 IS THIS MACHINE AND INDEX n IS n CONTAINERS DOWN, which is the
	// only shape that keeps the snapshot free of pointers at every depth: a
	// nested machine's identity is "the container state its parent was in", and
	// that container is named by the level above. A flat vector therefore IS the
	// path, and a level with nothing below it simply ends the vector.
	//
	// ★ THE CHAIN FOLLOWS THE SAME TARGET RULE AS THE LEVEL ITSELF: the level
	// below a transitioning machine is the TARGET state's sub-machine, because
	// the target is the state the reload is going to arrive on.
	//
	// ★ CAPACITY 0, DELIBERATELY. A default-constructed Zenith_Vector eagerly
	// allocates 8, and a RuntimeSnapshot is default-constructed on paths that
	// may never fill it (Flux_AnimationController::ReloadFromControllerDef
	// declares one before it knows whether there is a top-level machine at all).
	struct RuntimeSnapshot
	{
		Zenith_Vector<RuntimeLevelSnapshot> m_xLevels{ 0u };
	};

	// Read the playback out, this machine's level first and then one level per
	// container state it was inside. Pure — changes nothing.
	RuntimeSnapshot CaptureRuntimeSnapshot() const;

	// Put it back onto whatever this machine's def now holds. Returns TRUE when
	// THIS machine landed on the state the snapshot named at level 0 (the
	// current state, or the transition's target when one was in flight) and
	// FALSE when that state is gone and it fell back to the new def's default
	// state at time 0.
	//
	// ★ A SUB-STATE MACHINE'S CURRENT STATE, ITS TIME AND ITS IN-FLIGHT
	// TRANSITION TARGET ARE PRESERVED TOO, DOWN THE WHOLE CHAIN — the child
	// instance the def CopyFrom just replaced has been re-made by the rebuild
	// (Flux_AnimationState::ReadFromDataStream re-news it), so there IS an
	// object to restore onto, reached by name from the level above.
	//
	// ★ THE DESCENT STOPS AT THE FIRST ANCESTOR THAT FELL BACK, and that is the
	// load-bearing rule rather than an optimisation. A level that did not land
	// on its NAMED state is a level whose child chain describes a machine that
	// is no longer there; carrying level n+1 into whatever the default happens
	// to contain would TRANSPLANT a state name into an unrelated sub-graph —
	// silently, since SetState on an unknown name simply returns. The same stop
	// applies when the matched state is no longer a container at all.
	//
	// ★ THE CHILD IS ALWAYS REACHED THROUGH THIS LEVEL'S SetState (D42), which
	// is what re-publishes the shared parameter set onto it. A child restored
	// without that has m_pxSharedParameters == nullptr and reads its own
	// authored declaration defaults, so every condition inside it goes blind to
	// values the game is still setting.
	bool RestoreRuntimeSnapshot(const RuntimeSnapshot& xSnapshot);

	// Capture → BuildFromDef → restore, for a caller holding ONE machine.
	//
	// ★ THE CONTROLLER-LEVEL RELOAD CANNOT CALL THIS, and the reason is worth
	// stating: Flux_AnimationController::BuildFromControllerDef DELETES every
	// layer and therefore every layer's machine, so there is no `this` to reload
	// — it captures with CaptureRuntimeSnapshot before the rebuild and restores
	// onto the machines the rebuild created. Same two primitives, applied across
	// an object-identity boundary this verb does not have to cross.
	//
	// Returns what RestoreRuntimeSnapshot returns. ★ NOTE THE ASYMMETRY WITH
	// Flux_AnimationController::ReloadFromControllerDef, which returns the BUILD
	// contract (a dangling clip/mask) instead: BuildFromDef has no failure mode
	// to report, so this bool reports the one thing a caller here can act on.
	bool ReloadFromDef(const Flux_AnimationStateMachineDef& xNewDef,
		Flux_AnimationClipCollection* pxClipCollection = nullptr);

	// D45's PARAMETER rule, as a pure function over two sets, because it is
	// needed at BOTH levels: here for a standalone machine (which owns its
	// declarations) and on Flux_AnimationController for the shared live set
	// (D42). Copy xPrevious's value onto every parameter xLive declares under
	// the SAME NAME AND THE SAME TYPE; leave every other one at the value it
	// already holds, which after a rebuild is the new def's DEFAULT.
	//
	// ★ NAME **AND** TYPE, so a float "Speed" retyped to an int does not get a
	// bit-reinterpreted 4.0f — the union makes that silent, and the value that
	// comes out the far side is not merely wrong but unrelated. A renamed
	// parameter is simply a name xLive does not carry.
	//
	// ★ A PENDING TRIGGER SURVIVES. A trigger is an input the game has already
	// delivered and not yet spent; dropping it across a reload swallows a jump.
	// A trigger that was NOT pending is left alone (it is never un-set here).
	static void RestoreMatchedParameterValues(const Flux_AnimationParameters& xPrevious,
		Flux_AnimationParameters& xLive);

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

	// WU-6.4. Restore ONE level of a snapshot onto this machine, then descend
	// into the container it landed in. Returns what this LEVEL did — true when it
	// landed on the state the level named (or had no state to restore), false
	// when it fell back to the default — which is what the public
	// RestoreRuntimeSnapshot reports for level 0. A level below is restored on a
	// best-effort basis and its result is deliberately not folded in: a deleted
	// state two containers down is the author's intent, and reporting it as this
	// machine's failure would tell a caller its own state had been lost.
	bool RestoreLevel(const Zenith_Vector<RuntimeLevelSnapshot>& xLevels, u_int uLevel);

	// WU-6.4. The normalized time a state is showing, read from exactly where
	// GetCurrentStateInfo reads it — the ROOT of the state's blend tree. A
	// container state has no tree and therefore no time of its own; it answers 0.
	static float ReadStateNormalizedTime(const Flux_AnimationState* pxState);

	// The inverse of ReadStateNormalizedTime is NOT a method here: it is
	// Flux_BlendTreeNode::SetNormalizedTime, a VIRTUAL on the node hierarchy with
	// per-node semantics (see Flux_BlendTree.h). RestoreRuntimeSnapshot calls it
	// on the root of the state's tree and the tree walks itself down to the leaves,
	// where a fraction becomes SECONDS through
	// Flux_BlendTreeNode_Clip::SetCurrentTimestamp — still the only place in the
	// animation system a playhead is written.
	//
	// It used to be a private static here dispatching on GetNodeTypeName() with
	// strcmp, which is what made the recursion a SECOND, separately-maintained
	// description of the same hierarchy — the def's clip resolution had a third.
	// A node type added without a case silently kept the zero its Reset left; a
	// node type added now inherits the base's no-op, which is the same behaviour
	// stated once, in the class it belongs to.

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
