#pragma once

#include "AI/BehaviorTree/Zenith_BTNode.h"
#include <string>

/**
 * Zenith_BTCondition_HasTarget - Check if blackboard has a valid target entity
 */
class Zenith_BTCondition_HasTarget : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_HasTarget() = default;
	explicit Zenith_BTCondition_HasTarget(const std::string& strTargetKey);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "HasTarget"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetTargetKey(const std::string& str) { m_strTargetKey = str; }

private:
	std::string m_strTargetKey = "TargetEntity";
};

/**
 * Zenith_BTCondition_InRange - Check if distance to target is within range
 */
class Zenith_BTCondition_InRange : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_InRange() = default;
	Zenith_BTCondition_InRange(float fRange, const std::string& strTargetKey = "TargetEntity");

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "InRange"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetRange(float f) { m_fRange = f; }
	void SetTargetKey(const std::string& str) { m_strTargetKey = str; }

private:
	float m_fRange = 5.0f;
	std::string m_strTargetKey = "TargetEntity";
};

/**
 * Zenith_BTCondition_CanSeeTarget - Check if agent can see target via perception
 */
class Zenith_BTCondition_CanSeeTarget : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_CanSeeTarget() = default;
	explicit Zenith_BTCondition_CanSeeTarget(const std::string& strTargetKey);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "CanSeeTarget"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetTargetKey(const std::string& str) { m_strTargetKey = str; }
	void SetMinAwareness(float f) { m_fMinAwareness = f; }

private:
	std::string m_strTargetKey = "TargetEntity";
	float m_fMinAwareness = 0.1f;  // Minimum awareness to consider "can see"
};

/**
 * Zenith_BTCondition_BlackboardBool - Check a boolean blackboard value
 */
class Zenith_BTCondition_BlackboardBool : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_BlackboardBool() = default;
	Zenith_BTCondition_BlackboardBool(const std::string& strKey, bool bExpectedValue = true);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "BlackboardBool"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetKey(const std::string& str) { m_strKey = str; }
	void SetExpectedValue(bool b) { m_bExpectedValue = b; }

private:
	std::string m_strKey;
	bool m_bExpectedValue = true;
};

/**
 * Zenith_BTCondition_BlackboardCompare - Compare a float blackboard value
 */
class Zenith_BTCondition_BlackboardCompare : public Zenith_BTLeaf
{
public:
	enum class Comparison
	{
		EQUAL,
		NOT_EQUAL,
		LESS_THAN,
		LESS_EQUAL,
		GREATER_THAN,
		GREATER_EQUAL
	};

	Zenith_BTCondition_BlackboardCompare() = default;
	Zenith_BTCondition_BlackboardCompare(const std::string& strKey, Comparison eComp, float fValue);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "BlackboardCompare"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetKey(const std::string& str) { m_strKey = str; }
	void SetComparison(Comparison e) { m_eComparison = e; }
	void SetValue(float f) { m_fValue = f; }

private:
	std::string m_strKey;
	Comparison m_eComparison = Comparison::GREATER_THAN;
	float m_fValue = 0.0f;
};

/**
 * Zenith_BTCondition_HasAwareness - Check if agent has any perceived targets
 */
class Zenith_BTCondition_HasAwareness : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_HasAwareness() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "HasAwareness"; }

	void SetMinAwareness(float f) { m_fMinAwareness = f; }

private:
	float m_fMinAwareness = 0.1f;
};

/**
 * Zenith_BTCondition_Random - Randomly succeed based on probability
 */
class Zenith_BTCondition_Random : public Zenith_BTLeaf
{
public:
	Zenith_BTCondition_Random() = default;
	explicit Zenith_BTCondition_Random(float fProbability);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Random"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetProbability(float f) { m_fProbability = f; }

private:
	float m_fProbability = 0.5f;
};
