#include "Zenith.h"
#include "AI/Squad/Zenith_Formation.h"

// Static formation instances
Zenith_Formation Zenith_Formation::s_xLineFormation;
Zenith_Formation Zenith_Formation::s_xWedgeFormation;
Zenith_Formation Zenith_Formation::s_xColumnFormation;
Zenith_Formation Zenith_Formation::s_xCircleFormation;
Zenith_Formation Zenith_Formation::s_xSkirmishFormation;
bool Zenith_Formation::s_bFormationsInitialised = false;

Zenith_Formation::Zenith_Formation(const std::string& strName)
	: m_strName(strName)
{
}

void Zenith_Formation::InitialiseFormations()
{
	if (s_bFormationsInitialised)
	{
		return;
	}

	// Line formation: members spread horizontally
	// L = Leader position (0,0,0)
	//    [2]  [0/L]  [1]  [3]  [4]
	s_xLineFormation = Zenith_Formation("Line");
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), SquadRole::LEADER, 10.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), SquadRole::ASSAULT, 5.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(-2.0f, 0.0f, 0.0f), SquadRole::ASSAULT, 5.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f), SquadRole::FLANKER, 3.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(-4.0f, 0.0f, 0.0f), SquadRole::FLANKER, 3.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(6.0f, 0.0f, 0.0f), SquadRole::SUPPORT, 2.0f);
	s_xLineFormation.AddSlot(Zenith_Maths::Vector3(-6.0f, 0.0f, 0.0f), SquadRole::SUPPORT, 2.0f);

	// Wedge formation: V-shape with leader at front
	//        [0/L]
	//      [1]  [2]
	//    [3]      [4]
	//  [5]          [6]
	s_xWedgeFormation = Zenith_Formation("Wedge");
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), SquadRole::LEADER, 10.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(-1.5f, 0.0f, -2.0f), SquadRole::ASSAULT, 5.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(1.5f, 0.0f, -2.0f), SquadRole::ASSAULT, 5.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(-3.0f, 0.0f, -4.0f), SquadRole::FLANKER, 3.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(3.0f, 0.0f, -4.0f), SquadRole::FLANKER, 3.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(-4.5f, 0.0f, -6.0f), SquadRole::SUPPORT, 2.0f);
	s_xWedgeFormation.AddSlot(Zenith_Maths::Vector3(4.5f, 0.0f, -6.0f), SquadRole::OVERWATCH, 2.0f);

	// Column formation: single file line
	// [0/L]
	// [1]
	// [2]
	// [3]
	// [4]
	s_xColumnFormation = Zenith_Formation("Column");
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), SquadRole::LEADER, 10.0f);
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -2.0f), SquadRole::ASSAULT, 5.0f);
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f), SquadRole::ASSAULT, 4.0f);
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -6.0f), SquadRole::SUPPORT, 3.0f);
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -8.0f), SquadRole::SUPPORT, 2.0f);
	s_xColumnFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -10.0f), SquadRole::OVERWATCH, 1.0f);

	// Circle formation: defensive perimeter
	//      [1]
	//   [5]   [2]
	// [4] [0/L] [3]
	//   [7]   [6]
	s_xCircleFormation = Zenith_Formation("Circle");
	s_xCircleFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), SquadRole::LEADER, 10.0f);
	const float fRadius = 3.0f;
	const uint32_t uNumSlots = 6;
	for (uint32_t u = 0; u < uNumSlots; ++u)
	{
		float fAngle = (static_cast<float>(u) / static_cast<float>(uNumSlots)) * 2.0f * 3.14159f;
		Zenith_Maths::Vector3 xOffset(
			cosf(fAngle) * fRadius,
			0.0f,
			sinf(fAngle) * fRadius
		);
		SquadRole eRole = (u % 2 == 0) ? SquadRole::ASSAULT : SquadRole::SUPPORT;
		s_xCircleFormation.AddSlot(xOffset, eRole, 5.0f - static_cast<float>(u) * 0.5f);
	}

	// Skirmish formation: spread out for combat
	//   [1]     [2]
	//      [0/L]
	//   [3]     [4]
	//      [5]
	s_xSkirmishFormation = Zenith_Formation("Skirmish");
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), SquadRole::LEADER, 10.0f);
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(-3.0f, 0.0f, 2.0f), SquadRole::ASSAULT, 5.0f);
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(3.0f, 0.0f, 2.0f), SquadRole::ASSAULT, 5.0f);
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(-4.0f, 0.0f, -2.0f), SquadRole::FLANKER, 3.0f);
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(4.0f, 0.0f, -2.0f), SquadRole::FLANKER, 3.0f);
	s_xSkirmishFormation.AddSlot(Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f), SquadRole::OVERWATCH, 2.0f);
	s_xSkirmishFormation.SetSpacing(3.0f);

	s_bFormationsInitialised = true;
}

const Zenith_Formation* Zenith_Formation::GetLine()
{
	InitialiseFormations();
	return &s_xLineFormation;
}

const Zenith_Formation* Zenith_Formation::GetWedge()
{
	InitialiseFormations();
	return &s_xWedgeFormation;
}

const Zenith_Formation* Zenith_Formation::GetColumn()
{
	InitialiseFormations();
	return &s_xColumnFormation;
}

const Zenith_Formation* Zenith_Formation::GetCircle()
{
	InitialiseFormations();
	return &s_xCircleFormation;
}

const Zenith_Formation* Zenith_Formation::GetSkirmish()
{
	InitialiseFormations();
	return &s_xSkirmishFormation;
}

void Zenith_Formation::AddSlot(const Zenith_Maths::Vector3& xOffset, SquadRole ePreferredRole, float fPriority)
{
	Zenith_FormationSlot xSlot;
	xSlot.m_xOffset = xOffset;
	xSlot.m_ePreferredRole = ePreferredRole;
	xSlot.m_fPriority = fPriority;
	m_axSlots.PushBack(xSlot);
}

void Zenith_Formation::ClearSlots()
{
	m_axSlots.Clear();
}

void Zenith_Formation::GetWorldPositions(
	const Zenith_Maths::Vector3& xLeaderPos,
	const Zenith_Maths::Quaternion& xLeaderRot,
	Zenith_Vector<Zenith_Maths::Vector3>& axPositionsOut) const
{
	axPositionsOut.Clear();
	axPositionsOut.Reserve(m_axSlots.GetSize());

	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		axPositionsOut.PushBack(GetWorldPositionForSlot(u, xLeaderPos, xLeaderRot));
	}
}

Zenith_Maths::Vector3 Zenith_Formation::GetWorldPositionForSlot(
	uint32_t uSlotIndex,
	const Zenith_Maths::Vector3& xLeaderPos,
	const Zenith_Maths::Quaternion& xLeaderRot) const
{
	if (uSlotIndex >= m_axSlots.GetSize())
	{
		return xLeaderPos;
	}

	// Apply spacing multiplier to offset
	Zenith_Maths::Vector3 xScaledOffset = m_axSlots.Get(uSlotIndex).m_xOffset * m_fSpacing;

	// Rotate offset by leader rotation
	Zenith_Maths::Vector3 xRotatedOffset = Zenith_Maths::RotateVector(xScaledOffset, xLeaderRot);

	// Add to leader position
	return xLeaderPos + xRotatedOffset;
}

int32_t Zenith_Formation::FindSlotForRole(SquadRole eRole) const
{
	// First pass: find exact role match with highest priority
	int32_t iBestSlot = -1;
	float fBestPriority = -1.0f;

	for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
	{
		if (m_axSlots.Get(u).m_ePreferredRole == eRole && m_axSlots.Get(u).m_fPriority > fBestPriority)
		{
			iBestSlot = static_cast<int32_t>(u);
			fBestPriority = m_axSlots.Get(u).m_fPriority;
		}
	}

	// If no exact match, return highest priority slot
	if (iBestSlot < 0 && m_axSlots.GetSize() > 0)
	{
		for (uint32_t u = 0; u < m_axSlots.GetSize(); ++u)
		{
			if (m_axSlots.Get(u).m_fPriority > fBestPriority)
			{
				iBestSlot = static_cast<int32_t>(u);
				fBestPriority = m_axSlots.Get(u).m_fPriority;
			}
		}
	}

	return iBestSlot;
}

const char* GetSquadRoleName(SquadRole eRole)
{
	switch (eRole)
	{
	case SquadRole::LEADER:    return "Leader";
	case SquadRole::ASSAULT:   return "Assault";
	case SquadRole::SUPPORT:   return "Support";
	case SquadRole::FLANKER:   return "Flanker";
	case SquadRole::OVERWATCH: return "Overwatch";
	case SquadRole::MEDIC:     return "Medic";
	default:                   return "Unknown";
	}
}
