#include "Zenith.h"
#include "Telemetry/Zenith_Telemetry.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace Zenith_Telemetry
{
	// =========================================================
	// Helper: write/read std::string explicitly so the header stays
	// free of the std::string operator<< that DataStream already has
	// (the template dispatch chooses the std::string overload only via
	// the global free operator; we use it directly here for clarity).
	// =========================================================
	static void WriteString(Zenith_DataStream& xS, const std::string& str)
	{
		const uint32_t uLen = static_cast<uint32_t>(str.size());
		xS << uLen;
		if (uLen > 0)
		{
			xS.WriteData(str.data(), uLen);
		}
	}
	static void ReadString(Zenith_DataStream& xS, std::string& str)
	{
		uint32_t uLen = 0;
		xS >> uLen;
		// Safety cap matching DataStream's std::string handler.
		if (uLen > 1024u * 1024u) uLen = 0;
		str.assign(uLen, '\0');
		if (uLen > 0)
		{
			xS.ReadData(str.data(), uLen);
		}
	}

	// =========================================================
	// EntitySnapshot
	// =========================================================
	void EntitySnapshot::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << xId.m_uIndex;
		xS << xId.m_uGeneration;
		xS << xPos.x; xS << xPos.y; xS << xPos.z;
		xS << xForward.x; xS << xForward.y; xS << xForward.z;
		xS << uStateFlags;
	}
	void EntitySnapshot::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> xId.m_uIndex;
		xS >> xId.m_uGeneration;
		xS >> xPos.x; xS >> xPos.y; xS >> xPos.z;
		xS >> xForward.x; xS >> xForward.y; xS >> xForward.z;
		xS >> uStateFlags;
	}

	// =========================================================
	// FrameSample
	// =========================================================
	void FrameSample::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << uFrameIdx;
		xS << fTimeS;
		const uint32_t uN = axEntities.GetSize();
		xS << uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			axEntities.Get(i).WriteToDataStream(xS);
		}
	}
	void FrameSample::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> uFrameIdx;
		xS >> fTimeS;
		uint32_t uN = 0;
		xS >> uN;
		// Sanity cap: 64k entities per sample. Levels in DP have far fewer.
		if (uN > 65536u) uN = 0;
		axEntities.Clear();
		for (uint32_t i = 0; i < uN; ++i)
		{
			EntitySnapshot xE;
			xE.ReadFromDataStream(xS);
			axEntities.PushBack(xE);
		}
	}

	// =========================================================
	// EventPayload + Event
	// =========================================================
	void EventPayload::WriteToDataStream(Zenith_DataStream& xS) const
	{
		for (int i = 0; i < 4; ++i) xS << afFloats[i];
		for (int i = 0; i < 4; ++i) xS << aiInts[i];
		xS << xEntityA.m_uIndex; xS << xEntityA.m_uGeneration;
		xS << xEntityB.m_uIndex; xS << xEntityB.m_uGeneration;
		xS.WriteData(szLabel, sizeof(szLabel));
	}
	void EventPayload::ReadFromDataStream(Zenith_DataStream& xS)
	{
		for (int i = 0; i < 4; ++i) xS >> afFloats[i];
		for (int i = 0; i < 4; ++i) xS >> aiInts[i];
		xS >> xEntityA.m_uIndex; xS >> xEntityA.m_uGeneration;
		xS >> xEntityB.m_uIndex; xS >> xEntityB.m_uGeneration;
		xS.ReadData(szLabel, sizeof(szLabel));
		szLabel[sizeof(szLabel) - 1] = '\0';
	}

	void Event::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << uFrameIdx;
		xS << fTimeS;
		xS << uEventType;
		xS << uReserved;
		xPayload.WriteToDataStream(xS);
	}
	void Event::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> uFrameIdx;
		xS >> fTimeS;
		xS >> uEventType;
		xS >> uReserved;
		xPayload.ReadFromDataStream(xS);
	}

	// =========================================================
	// Header
	// =========================================================
	void Header::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << uMagic;
		xS << uVersion;
		xS << uSeed;
		xS << uStartUTCMs;
		WriteString(xS, strSceneName);
		xS << fFixedDt;
		xS << uSamplePeriodFrames;
	}
	void Header::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> uMagic;
		xS >> uVersion;
		xS >> uSeed;
		xS >> uStartUTCMs;
		ReadString(xS, strSceneName);
		xS >> fFixedDt;
		xS >> uSamplePeriodFrames;
	}

	// =========================================================
	// Recorder
	// =========================================================
	Recorder::Recorder() = default;

	void Recorder::Begin(const Header& xHeader)
	{
		m_xHeader   = xHeader;
		m_bRecording = true;
		m_bPaused    = false;
		m_uFrameIdx  = 0;
		m_axFrames.Clear();
		m_axEvents.Clear();
		if (m_xHeader.uSamplePeriodFrames == 0u)
		{
			m_xHeader.uSamplePeriodFrames = 6u; // default 10 Hz @ 60fps
		}
	}

	void Recorder::RecordFrame(const FrameSample& xSample)
	{
		if (!m_bRecording || m_bPaused) return;
		// Stamp with current frame for consistency. Tests pass an empty
		// xSample and rely on the recorder to assign uFrameIdx.
		FrameSample xCopy = xSample;
		xCopy.uFrameIdx = m_uFrameIdx;
		m_axFrames.PushBack(xCopy);
	}

	void Recorder::RecordEvent(const Event& xEvt)
	{
		if (!m_bRecording || m_bPaused) return;
		Event xCopy = xEvt;
		xCopy.uFrameIdx = m_uFrameIdx;
		m_axEvents.PushBack(xCopy);
	}

	// Serialize the recorded run as: Header, interleaved (RecordType byte
	// + payload), End sentinel. Interleaving by frame index preserves the
	// temporal ordering for readers that walk it sequentially.
	static void SerializeRun(const Header& xHeader,
	                         const Zenith_Vector<FrameSample>& axFrames,
	                         const Zenith_Vector<Event>& axEvents,
	                         Zenith_DataStream& xOut)
	{
		xHeader.WriteToDataStream(xOut);

		// Merge-walk the two arrays in frame-index order. Both arrays are
		// already in chronological order since the recorder appends only.
		uint32_t uF = 0;
		uint32_t uE = 0;
		const uint32_t uNF = axFrames.GetSize();
		const uint32_t uNE = axEvents.GetSize();
		while (uF < uNF || uE < uNE)
		{
			const bool bFrameNext =
				(uF < uNF) &&
				(uE >= uNE || axFrames.Get(uF).uFrameIdx <= axEvents.Get(uE).uFrameIdx);
			if (bFrameNext)
			{
				const uint8_t uT = static_cast<uint8_t>(RecordType::FrameSample);
				xOut << uT;
				axFrames.Get(uF).WriteToDataStream(xOut);
				++uF;
			}
			else
			{
				const uint8_t uT = static_cast<uint8_t>(RecordType::Event);
				xOut << uT;
				axEvents.Get(uE).WriteToDataStream(xOut);
				++uE;
			}
		}
		const uint8_t uEnd = static_cast<uint8_t>(RecordType::End);
		xOut << uEnd;
	}

	bool Recorder::End(const char* szBinaryPath,
	                   const char* szJsonPathOrNull,
	                   const char* (*pfnEventTypeToString)(uint16_t))
	{
		if (!m_bRecording) return false;
		m_bRecording = false;

		// Estimate buffer size: header ~64 bytes + 1 byte type tag per
		// record + ~60 bytes per event + ~(40 + 32*entities) per sample.
		// Round up generously to avoid the auto-resize hot path.
		uint64_t ulEstimate = 256u
		                    + static_cast<uint64_t>(m_axEvents.GetSize()) * 100u
		                    + static_cast<uint64_t>(m_axFrames.GetSize()) * 1024u;
		if (ulEstimate < 4096u) ulEstimate = 4096u;
		Zenith_DataStream xStream(ulEstimate);
		SerializeRun(m_xHeader, m_axFrames, m_axEvents, xStream);

		xStream.WriteToFile(szBinaryPath);

		if (szJsonPathOrNull != nullptr)
		{
			Reader xReader;
			if (!xReader.LoadFromFile(szBinaryPath)) return false;
			if (!xReader.ExportJson(szJsonPathOrNull, pfnEventTypeToString)) return false;
		}
		return true;
	}

	// =========================================================
	// Reader
	// =========================================================
	bool Reader::LoadFromFile(const char* szBinaryPath)
	{
		m_axFrames.Clear();
		m_axEvents.Clear();

		Zenith_DataStream xStream;
		xStream.ReadFromFile(szBinaryPath);
		if (!xStream.IsValid()) return false;
		xStream.SetCursor(0);

		m_xHeader.ReadFromDataStream(xStream);
		if (m_xHeader.uMagic != 0x4D4C545A)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Zenith_Telemetry::Reader: bad magic 0x%08X in %s", m_xHeader.uMagic, szBinaryPath);
			return false;
		}
		if (m_xHeader.uVersion != 1u)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Zenith_Telemetry::Reader: unknown version %u in %s", m_xHeader.uVersion, szBinaryPath);
			return false;
		}

		// Walk records until End sentinel.
		while (xStream.GetCursor() < xStream.GetSize())
		{
			uint8_t uT = 0;
			xStream >> uT;
			const RecordType eT = static_cast<RecordType>(uT);
			if (eT == RecordType::End) break;
			if (eT == RecordType::FrameSample)
			{
				FrameSample xS;
				xS.ReadFromDataStream(xStream);
				m_axFrames.PushBack(xS);
			}
			else if (eT == RecordType::Event)
			{
				Event xE;
				xE.ReadFromDataStream(xStream);
				m_axEvents.PushBack(xE);
			}
			else
			{
				Zenith_Error(LOG_CATEGORY_CORE,
					"Zenith_Telemetry::Reader: unknown record type %u", static_cast<unsigned>(uT));
				return false;
			}
		}
		return true;
	}

	bool Reader::ExportJson(const char* szJsonPath,
	                        const char* (*pfnEventTypeToString)(uint16_t)) const
	{
		std::ofstream xOut(szJsonPath);
		if (!xOut.is_open()) return false;

		auto WriteVec3 = [&xOut](const Zenith_Maths::Vector3& v)
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "[%.4f,%.4f,%.4f]", v.x, v.y, v.z);
			xOut << buf;
		};
		auto WriteEntId = [&xOut](const Zenith_EntityID& xId)
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "{\"idx\":%u,\"gen\":%u}", xId.m_uIndex, xId.m_uGeneration);
			xOut << buf;
		};

		xOut << "{\n";

		// Header
		xOut << "  \"header\": {\n";
		xOut << "    \"magic\": \"0x" << std::hex << m_xHeader.uMagic << std::dec << "\",\n";
		xOut << "    \"version\": " << m_xHeader.uVersion << ",\n";
		xOut << "    \"seed\": " << m_xHeader.uSeed << ",\n";
		xOut << "    \"startUTCMs\": " << m_xHeader.uStartUTCMs << ",\n";
		xOut << "    \"sceneName\": \"" << m_xHeader.strSceneName << "\",\n";
		xOut << "    \"fixedDt\": " << m_xHeader.fFixedDt << ",\n";
		xOut << "    \"samplePeriodFrames\": " << m_xHeader.uSamplePeriodFrames << "\n";
		xOut << "  },\n";

		// Frames
		xOut << "  \"frames\": [\n";
		const uint32_t uNF = m_axFrames.GetSize();
		for (uint32_t i = 0; i < uNF; ++i)
		{
			const FrameSample& xS = m_axFrames.Get(i);
			xOut << "    {\"frame\":" << xS.uFrameIdx << ",\"t\":" << xS.fTimeS << ",\"entities\":[";
			const uint32_t uNE = xS.axEntities.GetSize();
			for (uint32_t j = 0; j < uNE; ++j)
			{
				const EntitySnapshot& xE = xS.axEntities.Get(j);
				xOut << "{\"id\":";
				WriteEntId(xE.xId);
				xOut << ",\"pos\":";
				WriteVec3(xE.xPos);
				xOut << ",\"fwd\":";
				WriteVec3(xE.xForward);
				xOut << ",\"flags\":" << xE.uStateFlags << "}";
				if (j + 1 < uNE) xOut << ",";
			}
			xOut << "]}";
			if (i + 1 < uNF) xOut << ",";
			xOut << "\n";
		}
		xOut << "  ],\n";

		// Events
		xOut << "  \"events\": [\n";
		const uint32_t uNE = m_axEvents.GetSize();
		for (uint32_t i = 0; i < uNE; ++i)
		{
			const Event& xE = m_axEvents.Get(i);
			xOut << "    {\"frame\":" << xE.uFrameIdx << ",\"t\":" << xE.fTimeS
			     << ",\"type\":" << xE.uEventType;
			if (pfnEventTypeToString != nullptr)
			{
				const char* szName = pfnEventTypeToString(xE.uEventType);
				if (szName != nullptr)
				{
					xOut << ",\"name\":\"" << szName << "\"";
				}
			}
			xOut << ",\"payload\":{"
			     << "\"floats\":["
			     << xE.xPayload.afFloats[0] << "," << xE.xPayload.afFloats[1] << ","
			     << xE.xPayload.afFloats[2] << "," << xE.xPayload.afFloats[3] << "],"
			     << "\"ints\":["
			     << xE.xPayload.aiInts[0] << "," << xE.xPayload.aiInts[1] << ","
			     << xE.xPayload.aiInts[2] << "," << xE.xPayload.aiInts[3] << "],"
			     << "\"entityA\":";
			WriteEntId(xE.xPayload.xEntityA);
			xOut << ",\"entityB\":";
			WriteEntId(xE.xPayload.xEntityB);
			// szLabel is fixed-size, may not be null-terminated -- find the
			// terminator manually (std::strnlen is not in <cstring> portably).
			size_t uLabLen = 0;
			for (; uLabLen < sizeof(xE.xPayload.szLabel); ++uLabLen)
			{
				if (xE.xPayload.szLabel[uLabLen] == '\0') break;
			}
			std::string strLabel(xE.xPayload.szLabel, uLabLen);
			xOut << ",\"label\":\"" << strLabel << "\"}";
			xOut << "}";
			if (i + 1 < uNE) xOut << ",";
			xOut << "\n";
		}
		xOut << "  ]\n";
		xOut << "}\n";
		return true;
	}

	// =========================================================
	// Singleton
	// =========================================================
	Recorder& GetRecorder()
	{
		static Recorder s_xRecorder;
		return s_xRecorder;
	}
}
