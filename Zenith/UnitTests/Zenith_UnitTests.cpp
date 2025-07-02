#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "Core/Zenith_Callback.h"
#include "DataStream/Zenith_DataStream.h"

void Zenith_UnitTests::RunAllTests()
{
	TestDataStream();
	TestCallbacks();
	Zenith_Log("Unit tests passed");
}

void Zenith_UnitTests::TestDataStream()
{
	Zenith_DataStream xStream;

	const char* szTestData = "This is a test string";
	constexpr u_int uTestDataLen = 22;
	xStream.WriteData(szTestData, uTestDataLen);

	xStream << uint32_t(5u);
	xStream << float(2000.f);
	xStream << Zenith_Maths::Vector3(1, 2, 3);
	xStream << std::unordered_map<std::string, std::pair<uint32_t, uint64_t>>({{"Test", {20, 100}}});
	xStream << std::vector<double>({ 3245., -1119. });

	xStream.SetCursor(0);

	char acTestData[uTestDataLen];
	xStream.ReadData(acTestData, uTestDataLen);
	Zenith_Assert(!strcmp(acTestData, szTestData));

	uint32_t u5;
	xStream >> u5;
	Zenith_Assert(u5 == 5);

	float f2000;
	xStream >> f2000;
	Zenith_Assert(f2000 == 2000.f);

	Zenith_Maths::Vector3 x123;
	xStream >> x123;
	Zenith_Assert(x123 == Zenith_Maths::Vector3(1, 2, 3));

	std::unordered_map<std::string, std::pair<uint32_t, uint64_t>> xUnorderedMap;
	xStream >> xUnorderedMap;
	Zenith_Assert((xUnorderedMap.at("Test") == std::pair<uint32_t, uint64_t>(20, 100)));

	std::vector<double> xVector;
	xStream >> xVector;
	Zenith_Assert(xVector.at(0) == 3245. && xVector.at(1) == -1119.);
}

static bool LimitCallback(u_int uData, Zenith_CaptureList<u_int>& xCapture)
{
	return uData < xCapture.Get<0>();
}

static bool LimitCallbackWithScale(u_int uData, Zenith_CaptureList<u_int, float>& xCapture)
{
	return uData * xCapture.Get<1>() < xCapture.Get<0>();
}

static void Filter2(u_int auData[10], Zenith_Callback_Base<bool, u_int>& xCallback)
{
	for (u_int u = 0; u < 10; u++)
	{
		if (xCallback.Execute(auData[u]))
		{
			auData[u] = -1;
		}
	}
}

static void Filter(u_int auData[10], Zenith_Callback_Base<bool, u_int>& xCallback)
{
	Filter2(auData, xCallback);
}

void Zenith_UnitTests::TestCallbacks()
{
	u_int auData[10] = { 0,1,2,3,4,5,6,7,8,9 };
	u_int auDataCopy[10];

	{
		memcpy(auDataCopy, auData, sizeof(auData));

		const u_int uLimit = 5;
		Filter2(auDataCopy, Zenith_Callback_ParamAndCapture<bool, u_int, u_int>(&LimitCallback, { uLimit }));

		for (u_int u = 0; u < 10; u++)
		{
			if (auData[u] < uLimit)
			{
				Zenith_Assert(auDataCopy[u] == -1, "");
			}
			else
			{
				Zenith_Assert(auDataCopy[u] == auData[u], "");
			}
		}
	}

	{
		memcpy(auDataCopy, auData, sizeof(auData));

		const u_int uLimit = 5;
		const float fScale = 2.f;
		Filter2(auDataCopy, Zenith_Callback_ParamAndCapture<bool, u_int, u_int, float>(&LimitCallbackWithScale, { uLimit, fScale }));

		for (u_int u = 0; u < 10; u++)
		{
			if (auData[u] * fScale < uLimit)
			{
				Zenith_Assert(auDataCopy[u] == -1, "");
			}
			else
			{
				Zenith_Assert(auDataCopy[u] == auData[u], "");
			}
		}
	}
}