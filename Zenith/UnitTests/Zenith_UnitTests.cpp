#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "DataStream/Zenith_DataStream.h"
#include "Flux/Quads/Flux_Quads.h"

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
	xStream << Flux_Quads::Quad(
		Zenith_Maths::UVector4( 1,2,3,4 ),
		Zenith_Maths::Vector4( 5.f,6.f,7.f,8.f ),
		500u,
		{1,1}
	);

	xStream.SetCursor(0);

	uint32_t u5;
	xStream >> u5;

	float f2000;
	xStream >> f2000;

	Zenith_Maths::Vector3 x123;
	xStream >> x123;

	Flux_Quads::Quad xQuad;
	xStream >> xQuad;

	Zenith_Assert(u5 == 5);
	Zenith_Assert(f2000 == 2000.f);
	Zenith_Assert(x123 == Zenith_Maths::Vector3(1, 2, 3));
	Zenith_Assert(
		xQuad.m_xPosition_Size == Zenith_Maths::UVector4(1, 2, 3, 4) &&
		xQuad.m_xColour == Zenith_Maths::Vector4(5.f, 6.f, 7.f, 8.f) &&
		xQuad.m_uTexture == 500u &&
		xQuad.m_xUVMult_UVAdd == Zenith_Maths::Vector2(1,1)
	);
}