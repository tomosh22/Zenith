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
		// v3 fields (always written by the current writer; older Readers
		// pass uVersion < 3 and skip the read of these fields).
		xS << xAITargetPos.x; xS << xAITargetPos.y; xS << xAITargetPos.z;
		xS << uAIIntent;
		xS << uHeldItemTag;
		xS << uReserved;
		xS << fSecondaryFloat;
	}
	void EntitySnapshot::ReadFromDataStream(Zenith_DataStream& xS, uint32_t uVersion)
	{
		xS >> xId.m_uIndex;
		xS >> xId.m_uGeneration;
		xS >> xPos.x; xS >> xPos.y; xS >> xPos.z;
		xS >> xForward.x; xS >> xForward.y; xS >> xForward.z;
		xS >> uStateFlags;
		if (uVersion >= 3u)
		{
			xS >> xAITargetPos.x; xS >> xAITargetPos.y; xS >> xAITargetPos.z;
			xS >> uAIIntent;
			xS >> uHeldItemTag;
			xS >> uReserved;
			xS >> fSecondaryFloat;
		}
		else
		{
			xAITargetPos    = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
			uAIIntent       = 0;
			uHeldItemTag    = 0;
			uReserved       = 0;
			fSecondaryFloat = 0.0f;
		}
	}

	// =========================================================
	// CameraState (v3)
	// =========================================================
	void CameraState::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << xPos.x;    xS << xPos.y;    xS << xPos.z;
		xS << xLookAt.x; xS << xLookAt.y; xS << xLookAt.z;
		xS << fOrbitYawRad;
		xS << fOrbitDistance;
		xS << fFovRadians;
		xS << bValid;
	}
	void CameraState::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> xPos.x;    xS >> xPos.y;    xS >> xPos.z;
		xS >> xLookAt.x; xS >> xLookAt.y; xS >> xLookAt.z;
		xS >> fOrbitYawRad;
		xS >> fOrbitDistance;
		xS >> fFovRadians;
		xS >> bValid;
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
		// v3 trailing block. Older Readers pass uVersion < 3 and skip.
		xCamera.WriteToDataStream(xS);
		xS << fFrameWallMs;
	}
	void FrameSample::ReadFromDataStream(Zenith_DataStream& xS, uint32_t uVersion)
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
			xE.ReadFromDataStream(xS, uVersion);
			axEntities.PushBack(xE);
		}
		if (uVersion >= 3u)
		{
			xCamera.ReadFromDataStream(xS);
			xS >> fFrameWallMs;
		}
		else
		{
			xCamera      = CameraState{};
			fFrameWallMs = 0.0f;
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
		// v3 field.
		xS.WriteData(szSource, sizeof(szSource));
	}
	void EventPayload::ReadFromDataStream(Zenith_DataStream& xS, uint32_t uVersion)
	{
		for (int i = 0; i < 4; ++i) xS >> afFloats[i];
		for (int i = 0; i < 4; ++i) xS >> aiInts[i];
		xS >> xEntityA.m_uIndex; xS >> xEntityA.m_uGeneration;
		xS >> xEntityB.m_uIndex; xS >> xEntityB.m_uGeneration;
		xS.ReadData(szLabel, sizeof(szLabel));
		szLabel[sizeof(szLabel) - 1] = '\0';
		if (uVersion >= 3u)
		{
			xS.ReadData(szSource, sizeof(szSource));
			szSource[sizeof(szSource) - 1] = '\0';
		}
		else
		{
			szSource[0] = '\0';
		}
	}

	void Event::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << uFrameIdx;
		xS << fTimeS;
		xS << uEventType;
		xS << uReserved;
		xPayload.WriteToDataStream(xS);
	}
	void Event::ReadFromDataStream(Zenith_DataStream& xS, uint32_t uVersion)
	{
		xS >> uFrameIdx;
		xS >> fTimeS;
		xS >> uEventType;
		xS >> uReserved;
		xPayload.ReadFromDataStream(xS, uVersion);
	}

	// =========================================================
	// SceneObstacle
	// =========================================================
	void SceneObstacle::WriteToDataStream(Zenith_DataStream& xS) const
	{
		xS << fCentreX;
		xS << fCentreZ;
		xS << fHalfExtentX;
		xS << fHalfExtentZ;
		xS << fYawRadians;
	}
	void SceneObstacle::ReadFromDataStream(Zenith_DataStream& xS)
	{
		xS >> fCentreX;
		xS >> fCentreZ;
		xS >> fHalfExtentX;
		xS >> fHalfExtentZ;
		xS >> fYawRadians;
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
		// v2+ obstacle list.
		const uint32_t uNumObs = axObstacles.GetSize();
		xS << uNumObs;
		for (uint32_t i = 0; i < uNumObs; ++i)
		{
			axObstacles.Get(i).WriteToDataStream(xS);
		}
		// v3+ build / personality metadata.
		WriteString(xS, strBuildConfig);
		WriteString(xS, strBuildHash);
		WriteString(xS, strPersonalityName);
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
		// v2+ adds the obstacles list. v1 files leave axObstacles empty
		// (default state). Magic + version are validated by Reader, so
		// hitting this read with uVersion==1 just means we skip the
		// obstacle payload and treat the file as obstacle-free.
		axObstacles.Clear();
		if (uVersion >= 2u)
		{
			uint32_t uNumObs = 0;
			xS >> uNumObs;
			// Safety cap: a level with > 64k walls is almost certainly
			// a corrupt count; bail to zero so the reader doesn't try
			// to allocate gigabytes.
			if (uNumObs > 65536u) uNumObs = 0;
			for (uint32_t i = 0; i < uNumObs; ++i)
			{
				SceneObstacle xO;
				xO.ReadFromDataStream(xS);
				axObstacles.PushBack(xO);
			}
		}
		// v3+ build / personality metadata.
		strBuildConfig.clear();
		strBuildHash.clear();
		strPersonalityName.clear();
		if (uVersion >= 3u)
		{
			ReadString(xS, strBuildConfig);
			ReadString(xS, strBuildHash);
			ReadString(xS, strPersonalityName);
		}
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
		// Stamp fTimeS from the recorder's frame index when the caller
		// left it unset (default 0.0). Lets DP-side EmitEvent paths
		// produce meaningful event timestamps in the JSON without
		// every site computing iFrame * fixed_dt manually. Callers who
		// explicitly set fTimeS (the round-trip test for one) get to
		// keep their value -- only stamp when the field is zero.
		if (xCopy.fTimeS == 0.0f && m_xHeader.fFixedDt > 0.0f)
		{
			xCopy.fTimeS = static_cast<float>(m_uFrameIdx) * m_xHeader.fFixedDt;
		}
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
		// Accept v1 (legacy, no obstacles), v2 (obstacles), and v3 (current --
		// extended EntitySnapshot / CameraState / EventPayload.szSource +
		// build metadata in Header). Anything else is a forward-incompat
		// file produced by a newer writer than this reader knows how to
		// parse.
		if (m_xHeader.uVersion != 1u && m_xHeader.uVersion != 2u && m_xHeader.uVersion != 3u)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"Zenith_Telemetry::Reader: unknown version %u in %s", m_xHeader.uVersion, szBinaryPath);
			return false;
		}

		// Walk records until End sentinel.
		const uint32_t uVer = m_xHeader.uVersion;
		while (xStream.GetCursor() < xStream.GetSize())
		{
			uint8_t uT = 0;
			xStream >> uT;
			const RecordType eT = static_cast<RecordType>(uT);
			if (eT == RecordType::End) break;
			if (eT == RecordType::FrameSample)
			{
				FrameSample xS;
				xS.ReadFromDataStream(xStream, uVer);
				m_axFrames.PushBack(xS);
			}
			else if (eT == RecordType::Event)
			{
				Event xE;
				xE.ReadFromDataStream(xStream, uVer);
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
		xOut << "    \"samplePeriodFrames\": " << m_xHeader.uSamplePeriodFrames << ",\n";
		// v3 build / personality metadata. Always emitted so downstream
		// readers can treat the fields as guaranteed-present (empty string
		// when the writer didn't populate them).
		xOut << "    \"buildConfig\": \""     << m_xHeader.strBuildConfig     << "\",\n";
		xOut << "    \"buildHash\": \""       << m_xHeader.strBuildHash       << "\",\n";
		xOut << "    \"personalityName\": \"" << m_xHeader.strPersonalityName << "\",\n";
		// Static-scene obstacles. Always emitted (empty array if none) so
		// downstream readers (visualiser, etc.) don't need a fallback path
		// when the writer was older or didn't populate them.
		xOut << "    \"obstacles\": [";
		const uint32_t uNumObs = m_xHeader.axObstacles.GetSize();
		for (uint32_t i = 0; i < uNumObs; ++i)
		{
			const SceneObstacle& xO = m_xHeader.axObstacles.Get(i);
			char buf[256];
			std::snprintf(buf, sizeof(buf),
				"{\"cx\":%.3f,\"cz\":%.3f,\"hx\":%.3f,\"hz\":%.3f,\"yaw\":%.5f}",
				xO.fCentreX, xO.fCentreZ, xO.fHalfExtentX, xO.fHalfExtentZ, xO.fYawRadians);
			xOut << buf;
			if (i + 1 < uNumObs) xOut << ",";
		}
		xOut << "]\n";
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
				xOut << ",\"flags\":" << xE.uStateFlags;
				// v3 fields. Always emitted so downstream consumers can
				// rely on the keys being present; the values default to
				// 0 / (0,0,0) for older v2 recordings.
				xOut << ",\"aiTarget\":";
				WriteVec3(xE.xAITargetPos);
				xOut << ",\"aiIntent\":" << static_cast<int>(xE.uAIIntent);
				xOut << ",\"heldItem\":" << static_cast<int>(xE.uHeldItemTag);
				xOut << ",\"life\":"     << xE.fSecondaryFloat;
				xOut << "}";
				if (j + 1 < uNE) xOut << ",";
			}
			xOut << "]";
			// v3 camera state + per-frame perf. Wrapped in nested objects
			// so they're easy to skip in client code that only cares
			// about positions.
			xOut << ",\"camera\":{";
			xOut << "\"pos\":";
			WriteVec3(xS.xCamera.xPos);
			xOut << ",\"lookAt\":";
			WriteVec3(xS.xCamera.xLookAt);
			xOut << ",\"yaw\":"      << xS.xCamera.fOrbitYawRad
			     << ",\"dist\":"     << xS.xCamera.fOrbitDistance
			     << ",\"fov\":"      << xS.xCamera.fFovRadians
			     << ",\"valid\":"    << static_cast<int>(xS.xCamera.bValid)
			     << "}";
			xOut << ",\"frameMs\":" << xS.fFrameWallMs;
			xOut << "}";
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
			xOut << ",\"label\":\"" << strLabel << "\"";
			// v3: szSource. Same fixed-size, manually find terminator.
			size_t uSrcLen = 0;
			for (; uSrcLen < sizeof(xE.xPayload.szSource); ++uSrcLen)
			{
				if (xE.xPayload.szSource[uSrcLen] == '\0') break;
			}
			std::string strSource(xE.xPayload.szSource, uSrcLen);
			xOut << ",\"source\":\"" << strSource << "\"}";
			xOut << "}";
			if (i + 1 < uNE) xOut << ",";
			xOut << "\n";
		}
		xOut << "  ]\n";
		xOut << "}\n";
		return true;
	}

	// =========================================================
	// CSV export (v3)
	//
	// Two row-based files for spreadsheet / pandas analysis:
	//   - frames.csv: one row per entity per sampled frame
	//   - events.csv: one row per event
	//
	// Either path may be null to skip writing that file. Field separator is
	// comma; string fields are wrapped in double-quotes with embedded
	// double-quotes escaped per RFC 4180.
	// =========================================================
	static void WriteCsvString(std::ofstream& xOut, const char* sz, size_t uMaxLen)
	{
		xOut << '"';
		size_t uI = 0;
		for (; uI < uMaxLen && sz[uI] != '\0'; ++uI)
		{
			if (sz[uI] == '"') xOut << "\"\""; // escape per RFC 4180
			else               xOut << sz[uI];
		}
		xOut << '"';
	}

	bool Reader::ExportCsv(const char* szFramesCsvPath,
	                       const char* szEventsCsvPath,
	                       const char* (*pfnEventTypeToString)(uint16_t)) const
	{
		if (szFramesCsvPath != nullptr)
		{
			std::ofstream xOut(szFramesCsvPath);
			if (!xOut.is_open()) return false;

			// Header row.
			xOut << "frame,t,entity_idx,entity_gen,pos_x,pos_y,pos_z,"
			     << "fwd_x,fwd_y,fwd_z,flags,"
			     << "ai_intent,ai_target_x,ai_target_y,ai_target_z,"
			     << "held_item,life,"
			     << "cam_pos_x,cam_pos_y,cam_pos_z,"
			     << "cam_lookat_x,cam_lookat_y,cam_lookat_z,"
			     << "cam_yaw,cam_dist,cam_fov,cam_valid,frame_ms\n";

			const uint32_t uNF = m_axFrames.GetSize();
			for (uint32_t i = 0; i < uNF; ++i)
			{
				const FrameSample& xS = m_axFrames.Get(i);
				const uint32_t uNE = xS.axEntities.GetSize();
				for (uint32_t j = 0; j < uNE; ++j)
				{
					const EntitySnapshot& xE = xS.axEntities.Get(j);
					char buf[512];
					std::snprintf(buf, sizeof(buf),
						"%u,%.4f,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u,"
						"%u,%.4f,%.4f,%.4f,%u,%.4f,"
						"%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.5f,%.4f,%.5f,%u,%.4f\n",
						xS.uFrameIdx, xS.fTimeS,
						xE.xId.m_uIndex, xE.xId.m_uGeneration,
						xE.xPos.x, xE.xPos.y, xE.xPos.z,
						xE.xForward.x, xE.xForward.y, xE.xForward.z,
						xE.uStateFlags,
						static_cast<unsigned>(xE.uAIIntent),
						xE.xAITargetPos.x, xE.xAITargetPos.y, xE.xAITargetPos.z,
						static_cast<unsigned>(xE.uHeldItemTag),
						xE.fSecondaryFloat,
						xS.xCamera.xPos.x, xS.xCamera.xPos.y, xS.xCamera.xPos.z,
						xS.xCamera.xLookAt.x, xS.xCamera.xLookAt.y, xS.xCamera.xLookAt.z,
						xS.xCamera.fOrbitYawRad, xS.xCamera.fOrbitDistance,
						xS.xCamera.fFovRadians,
						static_cast<unsigned>(xS.xCamera.bValid),
						xS.fFrameWallMs);
					xOut << buf;
				}
			}
		}

		if (szEventsCsvPath != nullptr)
		{
			std::ofstream xOut(szEventsCsvPath);
			if (!xOut.is_open()) return false;

			xOut << "frame,t,type,type_name,"
			     << "float0,float1,float2,float3,int0,int1,int2,int3,"
			     << "entityA_idx,entityA_gen,entityB_idx,entityB_gen,"
			     << "label,source\n";

			const uint32_t uN = m_axEvents.GetSize();
			for (uint32_t i = 0; i < uN; ++i)
			{
				const Event& xE = m_axEvents.Get(i);
				const char* szName = (pfnEventTypeToString != nullptr)
					? pfnEventTypeToString(xE.uEventType)
					: nullptr;
				char buf[512];
				std::snprintf(buf, sizeof(buf),
					"%u,%.4f,%u,",
					xE.uFrameIdx, xE.fTimeS, static_cast<unsigned>(xE.uEventType));
				xOut << buf;
				if (szName != nullptr) WriteCsvString(xOut, szName, 64);
				else                   xOut << "\"\"";
				std::snprintf(buf, sizeof(buf),
					",%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d,%u,%u,%u,%u,",
					xE.xPayload.afFloats[0], xE.xPayload.afFloats[1],
					xE.xPayload.afFloats[2], xE.xPayload.afFloats[3],
					xE.xPayload.aiInts[0], xE.xPayload.aiInts[1],
					xE.xPayload.aiInts[2], xE.xPayload.aiInts[3],
					xE.xPayload.xEntityA.m_uIndex, xE.xPayload.xEntityA.m_uGeneration,
					xE.xPayload.xEntityB.m_uIndex, xE.xPayload.xEntityB.m_uGeneration);
				xOut << buf;
				WriteCsvString(xOut, xE.xPayload.szLabel,  sizeof(xE.xPayload.szLabel));
				xOut << ",";
				WriteCsvString(xOut, xE.xPayload.szSource, sizeof(xE.xPayload.szSource));
				xOut << "\n";
			}
		}

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
