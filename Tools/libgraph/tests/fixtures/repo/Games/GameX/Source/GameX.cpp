// Scenario 17: GameX's vcxproj uses PrecompiledHeader NotUsing, but its
// .cpp files still include "FxPch.h" literally.
#include "FxPch.h"

#include "GameX_Component.h"

int main()
{
	GameX_Component xComponent;
	(void)xComponent;
	return 0;
}
