#pragma once
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_Vector.h"
#include <unordered_map>
#include <variant>

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

	// Get all parameters
	const std::unordered_map<std::string, Parameter>& GetParameters() const { return m_xParameters; }

	// Reset all triggers (called at end of frame)
	void ResetTriggers();

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::unordered_map<std::string, Parameter> m_xParameters; // #TODO: Replace with engine hash map
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
// Single state in the state machine with its blend tree and transitions
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
// Complete animation state machine with states, transitions, and parameters
//=============================================================================
class Flux_AnimationStateMachine
{
public:
	Flux_AnimationStateMachine() = default;
	Flux_AnimationStateMachine(const std::string& strName);
	~Flux_AnimationStateMachine();

	// State management
	Flux_AnimationState* AddState(const std::string& strName);
	void RemoveState(const std::string& strName);
	Flux_AnimationState* GetState(const std::string& strName);
	const Flux_AnimationState* GetState(const std::string& strName) const;
	bool HasState(const std::string& strName) const;

	// Default state (entry point)
	void SetDefaultState(const std::string& strName);
	const std::string& GetDefaultStateName() const { return m_strDefaultStateName; }

	// Current state
	Flux_AnimationState* GetCurrentState() const { return m_pxCurrentState; }
	const std::string& GetCurrentStateName() const;

	// Force state change (ignores transitions)
	void SetState(const std::string& strStateName);

	// Parameters (shared across all states)
	// If shared parameters are set (by parent sub-SM), those are used instead of local
	Flux_AnimationParameters& GetParameters() { return m_pxSharedParameters ? *m_pxSharedParameters : m_xParameters; }
	const Flux_AnimationParameters& GetParameters() const { return m_pxSharedParameters ? *m_pxSharedParameters : m_xParameters; }
	void SetSharedParameters(Flux_AnimationParameters* pxSharedParams) { m_pxSharedParameters = pxSharedParams; }

	// Update the state machine (call each frame)
	// Returns the resulting skeleton pose
	void Update(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton);

	// Check if currently in a transition
	bool IsTransitioning() const { return m_pxActiveTransition != nullptr; }

	// State info query (Unity's GetCurrentAnimatorStateInfo)
	Flux_AnimatorStateInfo GetCurrentStateInfo() const;

	// Force-crossfade to a named state, bypassing transition conditions (Unity's Animator.CrossFade)
	void CrossFade(const std::string& strStateName, float fDuration = 0.15f);

	// Get all states for iteration
	const std::unordered_map<std::string, Flux_AnimationState*>& GetStates() const { return m_xStates; }

	// Name
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	// Any-State transitions (fire from any current state)
	void AddAnyStateTransition(const Flux_StateTransition& xTransition);
	void RemoveAnyStateTransition(uint32_t uIndex);
	const Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() const { return m_xAnyStateTransitions; }
	Zenith_Vector<Flux_StateTransition>& GetAnyStateTransitions() { return m_xAnyStateTransitions; }

	// Resolve clip references in blend trees
	void ResolveClipReferences(Flux_AnimationClipCollection* pxCollection);

	// Load from file
	static Flux_AnimationStateMachine* LoadFromFile(const std::string& strPath);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	void StartTransition(const Flux_StateTransition& xTransition);
	void UpdateTransition(float fDt, const Zenith_SkeletonAsset& xSkeleton);
	void CompleteTransition();

	// Check any-state transitions (skips transitions targeting current state and below iMinPriority)
	const Flux_StateTransition* CheckAnyStateTransitions(int32_t iMinPriority = INT32_MIN);

	std::string m_strName;
	std::unordered_map<std::string, Flux_AnimationState*> m_xStates; // #TODO: Replace with engine hash map
	std::string m_strDefaultStateName;
	Zenith_Vector<Flux_StateTransition> m_xAnyStateTransitions;

	// Runtime state
	Flux_AnimationState* m_pxCurrentState = nullptr;
	Flux_CrossFadeTransition* m_pxActiveTransition = nullptr;
	Flux_AnimationState* m_pxTransitionTargetState = nullptr;
	bool m_bActiveTransitionInterruptible = true;  // Whether the current active transition can be interrupted
	int32_t m_iActiveTransitionPriority = 0;       // Priority of the current active transition
	Flux_AnimationParameters m_xParameters;
	Flux_AnimationParameters* m_pxSharedParameters = nullptr;  // Non-owned, from parent SM

	// Poses
	Flux_SkeletonPose m_xCurrentPose;
	Flux_SkeletonPose m_xTargetPose;
};
