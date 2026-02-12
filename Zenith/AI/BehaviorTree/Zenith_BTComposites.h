#pragma once

#include "AI/BehaviorTree/Zenith_BTNode.h"

/**
 * Zenith_BTSequence - Runs children in order until one fails
 *
 * Returns SUCCESS if all children succeed
 * Returns FAILURE immediately when any child fails
 * Returns RUNNING if a child returns RUNNING (resumes from that child next tick)
 */
class Zenith_BTSequence : public Zenith_BTComposite
{
public:
	Zenith_BTSequence() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Sequence"; }
};

/**
 * Zenith_BTSelector - Runs children until one succeeds (OR node)
 *
 * Returns SUCCESS immediately when any child succeeds
 * Returns FAILURE if all children fail
 * Returns RUNNING if a child returns RUNNING (resumes from that child next tick)
 */
class Zenith_BTSelector : public Zenith_BTComposite
{
public:
	Zenith_BTSelector() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Selector"; }
};

/**
 * Zenith_BTParallel - Runs all children simultaneously
 *
 * Policy determines when to return SUCCESS/FAILURE:
 * - REQUIRE_ONE: SUCCESS when any child succeeds, FAILURE when all fail
 * - REQUIRE_ALL: SUCCESS when all succeed, FAILURE when any fails
 */
class Zenith_BTParallel : public Zenith_BTComposite
{
public:
	enum class Policy : uint8_t
	{
		REQUIRE_ONE,  // Success if any child succeeds
		REQUIRE_ALL   // Success only if all children succeed
	};

	Zenith_BTParallel() = default;
	explicit Zenith_BTParallel(Policy eSuccessPolicy, Policy eFailurePolicy = Policy::REQUIRE_ONE);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Parallel"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetSuccessPolicy(Policy e) { m_eSuccessPolicy = e; }
	void SetFailurePolicy(Policy e) { m_eFailurePolicy = e; }

private:
	void AbortRunningChildren(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard);

	Policy m_eSuccessPolicy = Policy::REQUIRE_ONE;
	Policy m_eFailurePolicy = Policy::REQUIRE_ONE;

	// Track which children have completed this run
	Zenith_Vector<BTNodeStatus> m_axChildResults;
};
