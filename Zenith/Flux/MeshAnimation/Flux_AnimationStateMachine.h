#pragma once
#include "Flux_BonePose.h"
#include "Flux_BlendTree.h"
#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_Vector.h"
#include <unordered_map>
#include <functional>
#include <variant>

// Forward declarations
class Flux_AnimationClipCollection;
class Zenith_SkeletonAsset;

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
	std::unordered_map<std::string, Parameter> m_xParameters;
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

	// Check if this transition can occur
	bool CanTransition(const Flux_AnimationParameters& xParams, float fCurrentNormalizedTime) const;

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
	void RemoveTransition(size_t uIndex);
	const Zenith_Vector<Flux_StateTransition>& GetTransitions() const { return m_xTransitions; }
	Zenith_Vector<Flux_StateTransition>& GetTransitions() { return m_xTransitions; }

	// Find highest priority transition that can trigger
	const Flux_StateTransition* CheckTransitions(const Flux_AnimationParameters& xParams) const;

	// State callbacks (optional, for gameplay hooks)
	std::function<void()> m_fnOnEnter;
	std::function<void()> m_fnOnExit;
	std::function<void(float)> m_fnOnUpdate;

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
	Zenith_Vector<Flux_StateTransition> m_xTransitions;
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
	Flux_AnimationParameters& GetParameters() { return m_xParameters; }
	const Flux_AnimationParameters& GetParameters() const { return m_xParameters; }

	// Update the state machine (call each frame)
	// Returns the resulting skeleton pose
	void Update(float fDt,
		Flux_SkeletonPose& xOutPose,
		const Zenith_SkeletonAsset& xSkeleton);

	// Check if currently in a transition
	bool IsTransitioning() const { return m_pxActiveTransition != nullptr; }

	// Get all states for iteration
	const std::unordered_map<std::string, Flux_AnimationState*>& GetStates() const { return m_xStates; }

	// Name
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

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

	std::string m_strName;
	std::unordered_map<std::string, Flux_AnimationState*> m_xStates;
	std::string m_strDefaultStateName;

	// Runtime state
	Flux_AnimationState* m_pxCurrentState = nullptr;
	Flux_CrossFadeTransition* m_pxActiveTransition = nullptr;
	Flux_AnimationState* m_pxTransitionTargetState = nullptr;
	Flux_AnimationParameters m_xParameters;

	// Poses
	Flux_SkeletonPose m_xCurrentPose;
	Flux_SkeletonPose m_xTargetPose;
};
