#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "DataStream/Zenith_DataStream.h"

void Zenith_UnitTests::RunAllTests()
{
	TestDataStream();
}

void Zenith_UnitTests::TestDataStream()
{
	Zenith_DataStream xStream(1024);
	xStream << uint32_t(5u);
	xStream << float(2000.f);
	xStream << Zenith_Maths::Vector3(1, 2, 3);

	xStream.SetCursor(0);

	uint32_t u5;
	xStream >> u5;

	float f2000;
	xStream >> f2000;

	Zenith_Maths::Vector3 x123;
	xStream >> x123;

	Zenith_Assert(u5 == 5);
	Zenith_Assert(f2000 == 2000.f);
	Zenith_Assert(x123 == Zenith_Maths::Vector3(1, 2, 3));
}