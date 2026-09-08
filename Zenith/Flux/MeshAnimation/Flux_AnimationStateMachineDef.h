#pragma once
#include "Flux_BlendTree.h"
#include "Collections/Zenith_HashMap.h"

// Callback typedefs for state lifecycle hooks (replaces std::function)
using Flux_AnimStateCallback = void(*)(void* pUserData);
using Flux_AnimStateUpdateCallback = void(*)(void* pUserData, float fDt);

// Forward declarations
class Flux_AnimationClipCollection;
class Zenith_SkeletonAsset;
class Flux_AnimationStateMachine;

//=============================================================================
// Flux_AnimationParameters
// Container for animation parameters (floats, ints, bools, triggers)
//
// ★ ONE TYPE, TWO ROLES, AND THE OWNER TELLS THEM APART (D42). Inside a
// Flux_AnimationStateMachineDef this is the DECLARATION table — the names, the
// types and the DEFAULTS a controller seeds a live set from. On a
// Flux_AnimationController it is the LIVE set: one per controller, shared by the
// top-level state machine, every layer's state machine and every sub-state
// machine, so "Speed" means one value for the whole animator rather than one per
// graph. Only the declarations are serialized; live values are runtime.
//=============================================================================
class Flux_AnimationParameters
{
public:
	enum class ParamType : uint8_t
	{
		Float,
		Int,
		Bool,
		Trigger
	};

	struct Parameter
	{
		ParamType m_eType;
		std::string m_strName;
		union
		{
			float m_fValue;
			int32_t m_iValue;
			bool m_bValue;
		};

		Parameter() : m_eType(ParamType::Float), m_fValue(0.0f) {}
	};

	// Add parameters
	void AddFloat(const std::string& strName, float fDefault = 0.0f);
	void AddInt(const std::string& strName, int32_t iDefault = 0);
	void AddBool(const std::string& strName, bool bDefault = false);
	void AddTrigger(const std::string& strName);

	// Setters
	void SetFloat(const std::string& strName, float fValue);
	void SetInt(const std::string& strName, int32_t iValue);
	void SetBool(const std::string& strName, bool bValue);
	void SetTrigger(const std::string& strName);

	// Getters
	float GetFloat(const std::string& strName) const;
	int32_t GetInt(const std::string& strName) const;
	bool GetBool(const std::string& strName) const;

	// Check if trigger is set without consuming it
	bool PeekTrigger(const std::string& strName) const;

	// Trigger consumption (returns true if trigger was set, then resets it)
	bool ConsumeTrigger(const std::string& strName);

	// Check if parameter exists
	bool HasParameter(const std::string& strName) const;
	ParamType GetParameterType(const std::string& strName) const;

	// Remove parameter
	void RemoveParameter(const std::string& strName);

	// Forget every parameter — the table is empty afterwards.
	void Clear();

	// Get all parameters
	const Zenith_HashMap<std::string, Parameter>& GetParameters() const { return m_xParameters; }

	// ★ DECLARE-IF-ABSENT, NEVER OVERWRITE (D42). Copy every parameter this set
	// declares into xOutLive that xOutLive does not already have, keeping its
	// declared type and DEFAULT value. A name already present is left exactly as
	// it is: two graphs on one controller may both declare "Speed", and the second
	// seeding must not reset the value the first one is already running on.
	void SeedInto(Flux_AnimationParameters& xOutLive) const;

	// Reset all triggers (called at end of frame)
	void ResetTriggers();

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Shared serialization helpers for parameter union values
	static void WriteParamValueToStream(Zenith_DataStream& xStream, ParamType eType, float fVal, int32_t iVal, bool bVal);
	static void ReadParamValueFromStream(Zenith_DataStream& xStream, ParamType eType, float& fVal, int32_t& iVal, bool& bVal);

private:
	Zenith_HashMap<std::string, Parameter> m_xParameters;
};

//=============================================================================
// Flux_TransitionCondition
// Single condition that must be met for a transition to occur
//=============================================================================
struct Flux_TransitionCondition
{
	enum class CompareOp : uint8_t
	{
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterEqual,
		LessEqual
	};

	std::string m_strParameterName;
	CompareOp m_eCompareOp = CompareOp::Equal;
	Flux_AnimationParameters::ParamType m_eParamType = Flux_AnimationParameters::ParamType::Float;

	union
	{
		float m_fThreshold;
		int32_t m_iThreshold;
		bool m_bThreshold;
	};

	Flux_TransitionCondition() : m_fThreshold(0.0f) {}

	// Evaluate this condition against parameter values
	bool Evaluate(const Flux_AnimationParameters& xParams) const;

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Flux_StateTransition
// Defines a transition between two states with conditions
//=============================================================================
struct Flux_StateTransition
{
	std::string m_strTargetStateName;
	Zenith_Vector<Flux_TransitionCondition> m_xConditions;  // All must be true (AND logic)

	float m_fTransitionDuration = 0.15f;   // Blend time in seconds
	float m_fExitTime = -1.0f;             // Normalized time to exit (-1 = any time)
	bool m_bHasExitTime = false;           // Require normalized time to reach exit time
	bool m_bInterruptible = true;          // Can be interrupted by higher priority transitions
	int32_t m_iPriority = 0;               // Higher = checked first

	// Check if this transition can occur (consumes triggers only if all conditions pass)
	bool CanTransition(Flux_AnimationParameters& xParams, float fCurrentNormalizedTime) const;

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Flux_AnimationState
// Single AUTHORED state: its blend tree, its outgoing transitions and, when it
// is a container rather than a pose source, its nested state machine.
//
// ★ THE CALLBACK HOOKS ARE THE ONE RUNTIME THING LEFT ON IT, and they are
// deliberately NOT serialized — a function pointer into this process is not
// authored data. A def copied through the stream (see
// Flux_AnimationStateMachineDef::CopyFrom) therefore arrives with its hooks
// cleared, which is correct: whoever installs a hook owns re-installing it.
//=============================================================================
class Flux_AnimationState
{
public:
	Flux_AnimationState() = default;
	Flux_AnimationState(const std::string& strName);
	~Flux_AnimationState();

	// Accessors
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	Flux_BlendTreeNode* GetBlendTree() const { return m_pxBlendTree; }
	void SetBlendTree(Flux_BlendTreeNode* pxNode) { m_pxBlendTree = pxNode; }

	// Transitions
	void AddTransition(const Flux_StateTransition& xTransition);
	void RemoveTransition(uint32_t uIndex);
	const Zenith_Vector<Flux_StateTransition>& GetTransitions() const { return m_xTransitions; }
	Zenith_Vector<Flux_StateTransition>& GetTransitions() { return m_xTransitions; }

	// Find highest priority transition that can trigger (iMinPriority: skip transitions at or below this priority)
	const Flux_StateTransition* CheckTransitions(Flux_AnimationParameters& xParams, int32_t iMinPriority = INT32_MIN) const;

	// Sub-state machine (nested state machine within this state)
	bool IsSubStateMachine() const { return m_pxSubStateMachine != nullptr; }
	Flux_AnimationStateMachine* GetSubStateMachine() const { return m_pxSubStateMachine; }
	Flux_AnimationStateMachine* CreateSubStateMachine(const std::string& strName);

	// State callbacks (optional, for gameplay hooks)
	Flux_AnimStateCallback m_pfnOnEnter = nullptr;
	Flux_AnimStateCallback m_pfnOnExit = nullptr;
	Flux_AnimStateUpdateCallback m_pfnOnUpdate = nullptr;
	void* m_pCallbackUserData = nullptr;

	// Editor position for visual state machine editor
#ifdef ZENITH_TOOLS
	Zenith_Maths::Vector2 m_xEditorPosition = Zenith_Maths::Vector2(0.0f);
#endif

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::string m_strName;
	Flux_BlendTreeNode* m_pxBlendTree = nullptr;
	Flux_AnimationStateMachine* m_pxSubStateMachine = nullptr;  // Owned, optional nested SM
	Zenith_Vector<Flux_StateTransition> m_xTransitions;
};

//=============================================================================
// Flux_AnimationStateMachineDef (WU-6.1)
//
// ★ THE AUTHORED HALF OF A STATE MACHINE, AND NOTHING ELSE. States,
// transitions, conditions, any-state transitions, the default state, the
// parameter DECLARATIONS with their defaults, and each container state's nested
// definition. There is no current state here, no active transition, no pose
// buffers and no per-instance cache: those live on Flux_AnimationStateMachine,
// which is the INSTANCE, and which used to carry both halves on one object —
// two FLUX_MAX_BONES poses sitting beside m_xStates behind a `// Runtime state`
// comment that marked the split without acting on it.
//
// ★ CLIP REFERENCES ARE BY NAME, resolved through a Flux_AnimationClipCollection
// (Flux_BlendTreeNode_Clip::m_strClipName + ResolveClip). A def therefore names
// the clips it needs and pins none of them, which is what lets it be written to
// a file and read back somewhere the pointers would be meaningless.
//
// SERIALIZATION IS THE PAYLOAD ONLY — no magic, no schema word, no envelope.
// WU-6.2 embeds this in a .zanimctrl and owns the envelope; adding one here
// would mean two.
//=============================================================================
class Flux_AnimationStateMachineDef
{
public:
	Flux_AnimationStateMachineDef() = default;
	Flux_AnimationStateMachineDef(const std::string& strName);
	~Flux_AnimationStateMachineDef();

	// Owns its states (and, through them, blend trees and nested defs).
	Flux_AnimationStateMachineDef(const Flux_AnimationStateMachineDef&) = delete;
	Flux_AnimationStateMachineDef& operator=(const Flux_AnimationStateMachineDef&) = delete;

	// ★ A DEEP COPY, PERFORMED THROUGH THIS DEF'S OWN SERIALIZER. A blend tree is
	// a polymorphic hierarchy with no clone verb, and the one faithful walk of it
	// that already exists — and that a test already pins — is Write/Read. Copying
	// through it means a field added to a node is carried by the copy the day it is
	// carried by the file, instead of being silently dropped by a hand-written
	// clone. Callbacks and clip POINTERS do not survive (neither is authored data);
	// re-resolve clips through the collection afterwards.
	void CopyFrom(const Flux_AnimationStateMachineDef& xSource);

	// Reset ALL FIVE members — the def is empty afterwards, exactly as a
	// default-constructed one is: every state is DELETED (and with it every nested
	// def), the state map, the default-state name, the any-state transitions, the
	// def's own name and the parameter DECLARATIONS are all cleared. Any
	// Flux_AnimationState* kept from AddState/GetState dangles after this call.
	void Clear();

	// Name
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	// State management
	Flux_AnimationState* AddState(const std::string& strName);
	void RemoveState(const std::string& strName);
	Flux_AnimationState* GetState(const std::string& strName);
	const Flux_AnimationState* GetState(const std::string& strName) const;
	bool HasState(const std::string& strName) const;
	const Zenith_HashMap<std::string, Flux_AnimationState*>& GetStates() const { return m_xStates; }

	// Default state (entry point)
	void SetDefaultState(const std::string& strName);
	const std::string& GetDefaultStateName() const { return m_strDefaultStateName; }

	// Any-State transitions (fire from any current state)
	void AddAnyStateTransition(const Flux_StateTransition& xTransition);
	void RemoveAnyStateTransition(uint32_t uIndex);
	const Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() const { return m_xAnyStateTransitions; }
	Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() { return m_xAnyStateTransitions; }

	// Parameter DECLARATIONS + defaults. This is the authored table; a running
	// controller keeps the live values (D42) and is seeded from here.
	Flux_AnimationParameters& GetParameterDeclarations() { return m_xParameterDeclarations; }
	const Flux_AnimationParameters& GetParameterDeclarations() const { return m_xParameterDeclarations; }

	// Declare this def's parameters — and every nested def's — into a live set,
	// never overwriting a name the set already carries. Recurses through container
	// states so a sub-state machine's declarations reach the controller too.
	void SeedParametersInto(Flux_AnimationParameters& xOutLive) const;

	// Resolve every blend tree's clip references (by name) through the collection.
	void ResolveClipReferences(Flux_AnimationClipCollection* pxCollection);

	// Serialization — payload only, see the class comment.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::string m_strName;
	Zenith_HashMap<std::string, Flux_AnimationState*> m_xStates;
	std::string m_strDefaultStateName;
	Zenith_Vector<Flux_StateTransition> m_xAnyStateTransitions;
	Flux_AnimationParameters m_xParameterDeclarations;
};
