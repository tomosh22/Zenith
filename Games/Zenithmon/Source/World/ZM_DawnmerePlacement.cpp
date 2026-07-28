#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

#include <cmath>

float ZM_DawnmereVesperYaw()
{
	const float fDeltaX = fZM_DAWNMERE_TOWN_CENTER_X - fZM_DAWNMERE_VESPER_X;
	const float fDeltaZ = fZM_DAWNMERE_TOWN_CENTER_Z - fZM_DAWNMERE_VESPER_Z;
	// X FIRST. See the header: this is the +Z-forward convention, not the usual
	// maths-library atan2(y, x).
	return std::atan2(fDeltaX, fDeltaZ);
}

Zenith_Maths::Quat ZM_DawnmereVesperFacing()
{
	return Zenith_Maths::AngleAxis(
		ZM_DawnmereVesperYaw(), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
}
