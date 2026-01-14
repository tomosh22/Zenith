#pragma once

#include "AI/BehaviorTree/Zenith_BTNode.h"

/**
 * Zenith_BTInverter - Inverts child result
 *
 * SUCCESS becomes FAILURE
 * FAILURE becomes SUCCESS
 * RUNNING passes through unchanged
 */
class Zenith_BTInverter : public Zenith_BTDecorator
{
public:
	Zenith_BTInverter() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Inverter"; }
};

/**
 * Zenith_BTSucceeder - Always returns SUCCESS
 *
 * Runs child and returns SUCCESS regardless of child result
 * Useful for optional actions that shouldn't fail the parent
 */
class Zenith_BTSucceeder : public Zenith_BTDecorator
{
public:
	Zenith_BTSucceeder() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Succeeder"; }
};

/**
 * Zenith_BTRepeater - Repeats child execution
 *
 * Can repeat a fixed number of times or infinitely
 * Can optionally stop on failure
 */
class Zenith_BTRepeater : public Zenith_BTDecorator
{
public:
	Zenith_BTRepeater() = default;
	explicit Zenith_BTRepeater(int32_t iRepeatCount, bool bStopOnFailure = false);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Repeater"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetRepeatCount(int32_t i) { m_iRepeatCount = i; }
	void SetStopOnFailure(bool b) { m_bStopOnFailure = b; }

	// Special value for infinite repeat
	static constexpr int32_t REPEAT_INFINITE = -1;

private:
	int32_t m_iRepeatCount = REPEAT_INFINITE;   // -1 = infinite
	bool m_bStopOnFailure = false;
	int32_t m_iCurrentIteration = 0;
};

/**
 * Zenith_BTCooldown - Prevents execution for a duration after completion
 *
 * After child completes (SUCCESS or FAILURE), returns FAILURE for duration
 * Useful for rate-limiting actions like attacks
 */
class Zenith_BTCooldown : public Zenith_BTDecorator
{
public:
	Zenith_BTCooldown() = default;
	explicit Zenith_BTCooldown(float fCooldownDuration);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Cooldown"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetCooldownDuration(float f) { m_fCooldownDuration = f; }
	void ResetCooldown() { m_fTimeSinceCompletion = m_fCooldownDuration; }

	bool IsOnCooldown() const { return m_fTimeSinceCompletion < m_fCooldownDuration; }
	float GetRemainingCooldown() const;

private:
	float m_fCooldownDuration = 1.0f;
	float m_fTimeSinceCompletion = 0.0f;  // Starts ready (>= duration)
	bool m_bChildRunning = false;
};

/**
 * Zenith_BTConditionalLoop - Loops while condition is true
 *
 * Runs child repeatedly as long as a blackboard condition is true
 */
class Zenith_BTConditionalLoop : public Zenith_BTDecorator
{
public:
	Zenith_BTConditionalLoop() = default;
	explicit Zenith_BTConditionalLoop(const std::string& strConditionKey);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "ConditionalLoop"; }

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetConditionKey(const std::string& str) { m_strConditionKey = str; }

private:
	std::string m_strConditionKey;
};

/**
 * Zenith_BTTimeLimit - Fails if child takes too long
 *
 * Returns FAILURE if child doesn't complete within time limit
 */
class Zenith_BTTimeLimit : public Zenith_BTDecorator
{
public:
	Zenith_BTTimeLimit() = default;
	explicit Zenith_BTTimeLimit(float fTimeLimit);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "TimeLimit"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetTimeLimit(float f) { m_fTimeLimit = f; }

private:
	float m_fTimeLimit = 5.0f;
	float m_fElapsedTime = 0.0f;
};
