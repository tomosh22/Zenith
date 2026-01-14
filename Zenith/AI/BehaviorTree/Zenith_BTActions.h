#pragma once

#include "AI/BehaviorTree/Zenith_BTNode.h"
#include <string>

class Zenith_NavMeshAgent;

/**
 * Zenith_BTAction_Wait - Wait for a duration before succeeding
 */
class Zenith_BTAction_Wait : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_Wait() = default;
	explicit Zenith_BTAction_Wait(float fDuration);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Wait"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetDuration(float f) { m_fDuration = f; }

	// If set, reads duration from blackboard key instead
	void SetDurationKey(const std::string& str) { m_strDurationKey = str; }

private:
	float m_fDuration = 1.0f;
	float m_fElapsed = 0.0f;
	std::string m_strDurationKey;
};

/**
 * Zenith_BTAction_MoveTo - Move agent to a position using navigation
 *
 * Reads target position from blackboard key (default: "TargetPosition")
 * Requires NavMeshAgent to be set on AIAgentComponent
 */
class Zenith_BTAction_MoveTo : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_MoveTo() = default;
	explicit Zenith_BTAction_MoveTo(const std::string& strTargetKey);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "MoveTo"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;
	virtual void OnExit(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;
	virtual void OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetTargetKey(const std::string& str) { m_strTargetKey = str; }
	void SetAcceptanceRadius(float f) { m_fAcceptanceRadius = f; }

private:
	std::string m_strTargetKey = "TargetPosition";
	float m_fAcceptanceRadius = 0.5f;
	bool m_bPathRequested = false;
};

/**
 * Zenith_BTAction_MoveToEntity - Move to another entity's position
 *
 * Reads target entity from blackboard key (default: "TargetEntity")
 */
class Zenith_BTAction_MoveToEntity : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_MoveToEntity() = default;
	explicit Zenith_BTAction_MoveToEntity(const std::string& strTargetKey);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "MoveToEntity"; }

	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;
	virtual void OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	void SetTargetKey(const std::string& str) { m_strTargetKey = str; }
	void SetAcceptanceRadius(float f) { m_fAcceptanceRadius = f; }
	void SetRepathInterval(float f) { m_fRepathInterval = f; }

private:
	std::string m_strTargetKey = "TargetEntity";
	float m_fAcceptanceRadius = 2.0f;
	float m_fRepathInterval = 0.5f;
	float m_fTimeSinceRepath = 0.0f;
	bool m_bPathRequested = false;
};

/**
 * Zenith_BTAction_SetBlackboardValue - Set a blackboard value
 */
class Zenith_BTAction_SetBlackboardBool : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_SetBlackboardBool() = default;
	Zenith_BTAction_SetBlackboardBool(const std::string& strKey, bool bValue);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "SetBlackboardBool"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

private:
	std::string m_strKey;
	bool m_bValue = false;
};

/**
 * Zenith_BTAction_SetBlackboardFloat - Set a float blackboard value
 */
class Zenith_BTAction_SetBlackboardFloat : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_SetBlackboardFloat() = default;
	Zenith_BTAction_SetBlackboardFloat(const std::string& strKey, float fValue);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "SetBlackboardFloat"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

private:
	std::string m_strKey;
	float m_fValue = 0.0f;
};

/**
 * Zenith_BTAction_Log - Debug action that logs a message
 */
class Zenith_BTAction_Log : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_Log() = default;
	explicit Zenith_BTAction_Log(const std::string& strMessage);

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "Log"; }

	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

private:
	std::string m_strMessage;
};

/**
 * Zenith_BTAction_FindPrimaryTarget - Updates blackboard with perception primary target
 */
class Zenith_BTAction_FindPrimaryTarget : public Zenith_BTLeaf
{
public:
	Zenith_BTAction_FindPrimaryTarget() = default;

	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) override;
	virtual const char* GetTypeName() const override { return "FindPrimaryTarget"; }

	void SetOutputKey(const std::string& str) { m_strOutputKey = str; }

private:
	std::string m_strOutputKey = "TargetEntity";
};
