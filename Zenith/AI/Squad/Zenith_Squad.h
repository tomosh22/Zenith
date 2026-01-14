#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "AI/Squad/Zenith_Formation.h"
#include <unordered_map>

/**
 * Squad member information
 */
struct Zenith_SquadMember
{
	Zenith_EntityID m_xEntityID;
	SquadRole m_eRole = SquadRole::ASSAULT;
	int32_t m_iFormationSlot = -1;               // Assigned slot in current formation
	Zenith_Maths::Vector3 m_xFormationOffset;    // Target offset from leader
	bool m_bAlive = true;
};

/**
 * Shared target knowledge within squad
 */
struct Zenith_SharedTarget
{
	Zenith_EntityID m_xTargetID;
	Zenith_Maths::Vector3 m_xLastKnownPosition;
	float m_fTimeLastSeen = 0.0f;
	Zenith_EntityID m_xReportedBy;               // Who reported this target
	bool m_bEngaged = false;                     // Is someone engaging this target
	Zenith_EntityID m_xEngagedBy;                // Who is engaging
};

/**
 * Squad order types
 */
enum class SquadOrderType : uint8_t
{
	NONE,
	MOVE_TO,        // Move squad to position
	ATTACK,         // Attack a target
	DEFEND,         // Defend a position
	FLANK,          // Flank a target
	SUPPRESS,       // Suppress target area
	REGROUP,        // Regroup at leader position
	RETREAT,        // Fall back to position
	HOLD_POSITION   // Stop and hold current position
};

/**
 * Active squad order
 */
struct Zenith_SquadOrder
{
	SquadOrderType m_eType = SquadOrderType::NONE;
	Zenith_Maths::Vector3 m_xTargetPosition;
	Zenith_EntityID m_xTargetEntity;
	float m_fTimeIssued = 0.0f;
};

/**
 * Zenith_Squad - Manages a group of AI agents working together
 *
 * Provides:
 * - Formation management (positioning members relative to leader)
 * - Role assignment (leader, assault, flanker, support, overwatch)
 * - Shared knowledge (target positions shared between members)
 * - Coordinated orders (attack, flank, suppress, regroup)
 * - Tactical decision-making
 */
class Zenith_Squad
{
public:
	Zenith_Squad();
	explicit Zenith_Squad(const std::string& strName);
	~Zenith_Squad() = default;

	// Member management
	void AddMember(Zenith_EntityID xEntity, SquadRole eRole = SquadRole::ASSAULT);
	void RemoveMember(Zenith_EntityID xEntity);
	bool HasMember(Zenith_EntityID xEntity) const;
	Zenith_SquadMember* GetMember(Zenith_EntityID xEntity);
	const Zenith_SquadMember* GetMember(Zenith_EntityID xEntity) const;
	uint32_t GetMemberCount() const { return m_axMembers.GetSize(); }
	uint32_t GetAliveMemberCount() const;

	// Leader management
	void SetLeader(Zenith_EntityID xEntity);
	Zenith_EntityID GetLeader() const { return m_xLeaderID; }
	bool HasLeader() const { return m_xLeaderID.IsValid(); }

	// Formation
	void SetFormation(const Zenith_Formation* pxFormation);
	const Zenith_Formation* GetFormation() const { return m_pxFormation; }
	void UpdateFormationPositions();
	Zenith_Maths::Vector3 GetFormationPositionFor(Zenith_EntityID xEntity) const;

	// Orders
	void OrderMoveTo(const Zenith_Maths::Vector3& xPosition);
	void OrderAttack(Zenith_EntityID xTarget);
	void OrderDefend(const Zenith_Maths::Vector3& xPosition);
	void OrderFlank(Zenith_EntityID xTarget);
	void OrderSuppress(const Zenith_Maths::Vector3& xTargetArea);
	void OrderRegroup();
	void OrderRetreat(const Zenith_Maths::Vector3& xFallbackPosition);
	void OrderHoldPosition();
	void ClearOrder();
	const Zenith_SquadOrder& GetCurrentOrder() const { return m_xCurrentOrder; }

	// Shared knowledge
	void ShareTargetInfo(Zenith_EntityID xTarget, const Zenith_Maths::Vector3& xPosition, Zenith_EntityID xReportedBy);
	bool IsTargetKnown(Zenith_EntityID xTarget) const;
	const Zenith_SharedTarget* GetSharedTarget(Zenith_EntityID xTarget) const;
	const Zenith_Vector<Zenith_SharedTarget>& GetAllSharedTargets() const { return m_axSharedTargets; }
	void SetTargetEngaged(Zenith_EntityID xTarget, Zenith_EntityID xEngagedBy);
	bool IsTargetEngaged(Zenith_EntityID xTarget) const;
	Zenith_EntityID GetPriorityTarget() const;

	// Update
	void Update(float fDt);

	// Role management
	void AssignRole(Zenith_EntityID xEntity, SquadRole eRole);
	SquadRole GetMemberRole(Zenith_EntityID xEntity) const;
	Zenith_Vector<Zenith_EntityID> GetMembersWithRole(SquadRole eRole) const;

	// Mark member as dead (doesn't remove from squad, just marks)
	void MarkMemberDead(Zenith_EntityID xEntity);
	void MarkMemberAlive(Zenith_EntityID xEntity);
	bool IsMemberAlive(Zenith_EntityID xEntity) const;

	// Accessors
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	// Get all members (for iteration)
	const Zenith_Vector<Zenith_SquadMember>& GetMembers() const { return m_axMembers; }

#ifdef ZENITH_TOOLS
	// Debug visualization
	void DebugDraw() const;
#endif

private:
	std::string m_strName;
	Zenith_Vector<Zenith_SquadMember> m_axMembers;
	Zenith_EntityID m_xLeaderID;
	const Zenith_Formation* m_pxFormation = nullptr;
	Zenith_SquadOrder m_xCurrentOrder;
	Zenith_Vector<Zenith_SharedTarget> m_axSharedTargets;

	// Timing
	float m_fTargetKnowledgeTimeout = 30.0f;     // Forget targets after this time
	float m_fFormationUpdateInterval = 0.5f;
	float m_fTimeSinceFormationUpdate = 0.0f;

	// Internal helpers
	void AutoAssignLeader();
	void AssignFormationSlots();
	void UpdateSharedKnowledge(float fDt);
	int32_t FindMemberIndex(Zenith_EntityID xEntity) const;
};

/**
 * Zenith_SquadManager - Global manager for all squads
 *
 * THREAD SAFETY: All SquadManager and Squad operations must be called from the
 * main thread only. Formation updates read shared state without synchronization
 * and concurrent access from other threads will result in undefined behavior.
 *
 * Initialise() must be called before any other SquadManager functions.
 */
class Zenith_SquadManager
{
public:
	static void Initialise();
	static void Shutdown();
	static void Update(float fDt);

	// Squad management
	static Zenith_Squad* CreateSquad(const std::string& strName);
	static void DestroySquad(Zenith_Squad* pxSquad);
	static Zenith_Squad* GetSquadByName(const std::string& strName);
	static Zenith_Squad* GetSquadForEntity(Zenith_EntityID xEntity);

	// Query
	static uint32_t GetSquadCount();
	static const Zenith_Vector<Zenith_Squad*>& GetAllSquads();

#ifdef ZENITH_TOOLS
	static void DebugDrawAllSquads();
#endif

private:
	static Zenith_Vector<Zenith_Squad*> s_axSquads;
	static bool s_bInitialised;
};
