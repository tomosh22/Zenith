#pragma once

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

/**
 * Squad role enumeration
 * Defines the tactical role each squad member can fill
 */
enum class SquadRole : uint8_t
{
	LEADER,     // Commands the squad, others follow
	ASSAULT,    // Front-line combat
	SUPPORT,    // Provides suppressing fire
	FLANKER,    // Moves to attack from sides
	OVERWATCH,  // Provides cover from distance
	MEDIC,      // Support/healing role

	COUNT
};

/**
 * Formation slot definition
 * Defines a position within a formation and preferred role
 */
struct Zenith_FormationSlot
{
	Zenith_Maths::Vector3 m_xOffset;        // Offset from leader position (local space)
	SquadRole m_ePreferredRole = SquadRole::ASSAULT;
	float m_fPriority = 1.0f;               // Higher priority slots filled first
};

/**
 * Zenith_Formation - Defines squad formation layouts
 *
 * Formations are defined as a set of slots with local-space offsets
 * from the leader position. When applied, these are transformed to
 * world space based on leader position and facing direction.
 */
class Zenith_Formation
{
public:
	Zenith_Formation() = default;
	Zenith_Formation(const std::string& strName);

	// Built-in formation presets
	static const Zenith_Formation* GetLine();
	static const Zenith_Formation* GetWedge();
	static const Zenith_Formation* GetColumn();
	static const Zenith_Formation* GetCircle();
	static const Zenith_Formation* GetSkirmish();

	// Formation definition
	void AddSlot(const Zenith_Maths::Vector3& xOffset, SquadRole ePreferredRole, float fPriority = 1.0f);
	void ClearSlots();
	void SetSpacing(float fSpacing) { m_fSpacing = fSpacing; }

	// Get world positions for formation slots
	void GetWorldPositions(
		const Zenith_Maths::Vector3& xLeaderPos,
		const Zenith_Maths::Quaternion& xLeaderRot,
		Zenith_Vector<Zenith_Maths::Vector3>& axPositionsOut) const;

	// Get world position for a specific slot
	Zenith_Maths::Vector3 GetWorldPositionForSlot(
		uint32_t uSlotIndex,
		const Zenith_Maths::Vector3& xLeaderPos,
		const Zenith_Maths::Quaternion& xLeaderRot) const;

	// Accessors
	const std::string& GetName() const { return m_strName; }
	uint32_t GetSlotCount() const { return m_axSlots.GetSize(); }
	const Zenith_FormationSlot& GetSlot(uint32_t uIndex) const { return m_axSlots.Get(uIndex); }
	float GetSpacing() const { return m_fSpacing; }

	// Find best slot for a given role
	int32_t FindSlotForRole(SquadRole eRole) const;

private:
	std::string m_strName;
	Zenith_Vector<Zenith_FormationSlot> m_axSlots;
	float m_fSpacing = 2.0f;  // Base spacing multiplier

	// Static formation instances
	static Zenith_Formation s_xLineFormation;
	static Zenith_Formation s_xWedgeFormation;
	static Zenith_Formation s_xColumnFormation;
	static Zenith_Formation s_xCircleFormation;
	static Zenith_Formation s_xSkirmishFormation;
	static bool s_bFormationsInitialised;

	static void InitialiseFormations();
};

/**
 * Get role name as string (for debugging)
 */
const char* GetSquadRoleName(SquadRole eRole);
