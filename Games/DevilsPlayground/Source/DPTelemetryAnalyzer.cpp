#include "Zenith.h"
#include "Source/DPTelemetryAnalyzer.h"
#include "Source/DPTelemetry.h"

#include <cmath>

namespace DPTelemetryAnalyzer
{
	// =========================================================
	// Criterion -> string. Stable -- external tooling keys off these.
	// =========================================================
	const char* CriterionToString(Criterion eCriterion)
	{
		switch (eCriterion)
		{
		case Criterion::None:                return "None";
		case Criterion::HeaderMagicValid:    return "HeaderMagicValid";
		case Criterion::HeaderHasSceneName:  return "HeaderHasSceneName";
		case Criterion::FramesRecorded:      return "FramesRecorded";
		case Criterion::FramesNonEmpty:      return "FramesNonEmpty";
		case Criterion::PossessionFired:     return "PossessionFired";
		case Criterion::AnyPossessedFrame:   return "AnyPossessedFrame";
		case Criterion::AnySprintFrame:      return "AnySprintFrame";
		case Criterion::AnyWalkQuietFrame:   return "AnyWalkQuietFrame";
		case Criterion::AnyHoldingItemFrame: return "AnyHoldingItemFrame";
		case Criterion::InteractFired:       return "InteractFired";
		case Criterion::VictoryFired:        return "VictoryFired";
		case Criterion::RunLostFired:        return "RunLostFired";
		case Criterion::TerminalEventFired:  return "TerminalEventFired";
		case Criterion::PickupFired:         return "PickupFired";
		case Criterion::PossessionChangedFired: return "PossessionChangedFired";
		case Criterion::DoorOpenedFired:        return "DoorOpenedFired";
		case Criterion::ChestOpenedFired:       return "ChestOpenedFired";
		case Criterion::ForgeCraftedFired:      return "ForgeCraftedFired";
		case Criterion::ObjectivePlacedFired:   return "ObjectivePlacedFired";
		case Criterion::PriestMoved:            return "PriestMoved";
		default:                              return "Unknown";
		}
	}

	// =========================================================
	// Pipeline-health preset.
	// =========================================================
	const Criterion akPipelineHealthCriteria[] = {
		Criterion::HeaderMagicValid,
		Criterion::HeaderHasSceneName,
		Criterion::FramesRecorded,
		Criterion::FramesNonEmpty,
	};
	const uint32_t kPipelineHealthCriteriaCount =
		sizeof(akPipelineHealthCriteria) / sizeof(akPipelineHealthCriteria[0]);

	// =========================================================
	// Internal predicate helpers. Each returns a (bPassed, szReason)
	// pair via CriterionResult. szReason points to a static literal so
	// the caller can store the result without a heap allocation.
	// =========================================================
	namespace
	{
		bool AnyFrameWithFlag(const Zenith_Telemetry::Reader& xReader, uint32_t uFlag)
		{
			const auto& axF = xReader.GetFrames();
			const uint32_t uN = axF.GetSize();
			for (uint32_t i = 0; i < uN; ++i)
			{
				const auto& xS = axF.Get(i);
				const uint32_t uNE = xS.axEntities.GetSize();
				for (uint32_t j = 0; j < uNE; ++j)
				{
					if (xS.axEntities.Get(j).uStateFlags & uFlag) return true;
				}
			}
			return false;
		}

		bool AnyEventOfType(const Zenith_Telemetry::Reader& xReader, uint16_t uType)
		{
			const auto& axE = xReader.GetEvents();
			const uint32_t uN = axE.GetSize();
			for (uint32_t i = 0; i < uN; ++i)
			{
				if (axE.Get(i).uEventType == uType) return true;
			}
			return false;
		}

		// Sum of horizontal step distances across every IsPriest-flagged
		// entity, across every consecutive pair of frame samples. The
		// IsPriest tag is stamped by the test's per-frame sampler when
		// it iterates Priest_Behaviour instances (see
		// Test_PersonalityPlaythrough's EmitPositionSample), so this
		// only counts entities that the recording explicitly marks as
		// the priest. A motionless priest returns 0.0; a priest that
		// patrolled 40 m returns ~40.0.
		//
		// Tracks per-entity-key previous position so multiple priests
		// (future multi-priest tests) sum independently rather than
		// teleporting between each other's last-known spots.
		float SumPriestPathLength(const Zenith_Telemetry::Reader& xReader)
		{
			const auto& axF = xReader.GetFrames();
			const uint32_t uN = axF.GetSize();

			// Per-priest last-seen XZ position; key = packed entityID.
			// Small linear-scan associative map -- there's typically ONE
			// priest in DP, so a vector pair beats an unordered_map.
			struct Tracked { uint64_t uKey; float fLastX; float fLastZ; };
			Zenith_Vector<Tracked> axTracked;

			float fTotal = 0.0f;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const auto& xS = axF.Get(i);
				const uint32_t uNE = xS.axEntities.GetSize();
				for (uint32_t j = 0; j < uNE; ++j)
				{
					const auto& xE = xS.axEntities.Get(j);
					if ((xE.uStateFlags & DPTelemetry::StateFlags::IsPriest) == 0u) continue;

					const uint64_t uKey =
						(static_cast<uint64_t>(xE.xId.m_uGeneration) << 32) | xE.xId.m_uIndex;

					Tracked* pxT = nullptr;
					for (uint32_t k = 0; k < axTracked.GetSize(); ++k)
					{
						if (axTracked.Get(k).uKey == uKey) { pxT = &axTracked.Get(k); break; }
					}
					if (pxT == nullptr)
					{
						Tracked xNew{ uKey, xE.xPos.x, xE.xPos.z };
						axTracked.PushBack(xNew);
					}
					else
					{
						const float fDx = xE.xPos.x - pxT->fLastX;
						const float fDz = xE.xPos.z - pxT->fLastZ;
						fTotal += std::sqrt(fDx * fDx + fDz * fDz);
						pxT->fLastX = xE.xPos.x;
						pxT->fLastZ = xE.xPos.z;
					}
				}
			}
			return fTotal;
		}

		CriterionResult Check(Criterion eCriterion,
		                      const Zenith_Telemetry::Reader& xReader,
		                      const Thresholds& xT)
		{
			CriterionResult xR;
			xR.eCriterion = eCriterion;
			xR.bPassed    = false;
			xR.szReason   = "";

			switch (eCriterion)
			{
			case Criterion::HeaderMagicValid:
			{
				const auto& xH = xReader.GetHeader();
				if (xH.uMagic != 0x4D4C545Au) { xR.szReason = "magic != 'ZTLM'"; break; }
				// Accept every version the current Reader handles
				// (see Zenith_Telemetry::Reader::LoadFromFile). v1 was
				// the original; v2 added Header.axObstacles; v3 added
				// extended EntitySnapshot + CameraState + perf + build
				// metadata.
				if (xH.uVersion != 1u && xH.uVersion != 2u && xH.uVersion != 3u)
				                              { xR.szReason = "version != 1 && != 2 && != 3"; break; }
				xR.bPassed = true; xR.szReason = "ok"; break;
			}
			case Criterion::HeaderHasSceneName:
			{
				if (xReader.GetHeader().strSceneName.empty())
					{ xR.szReason = "strSceneName empty"; break; }
				xR.bPassed = true; xR.szReason = "ok"; break;
			}
			case Criterion::FramesRecorded:
			{
				const uint32_t uN = xReader.GetFrames().GetSize();
				if (uN < xT.uMinFrames) { xR.szReason = "frame count below threshold"; break; }
				xR.bPassed = true; xR.szReason = "ok"; break;
			}
			case Criterion::FramesNonEmpty:
			{
				const auto& axF = xReader.GetFrames();
				const uint32_t uN = axF.GetSize();
				if (uN == 0u) { xR.szReason = "no frames recorded"; break; }
				for (uint32_t i = 0; i < uN; ++i)
				{
					if (axF.Get(i).axEntities.GetSize() < xT.uMinSampleEntities)
					{
						xR.szReason = "frame with empty entity list";
						return xR;
					}
				}
				xR.bPassed = true; xR.szReason = "ok"; break;
			}
			case Criterion::AnyPossessedFrame:
			{
				if (AnyFrameWithFlag(xReader, DPTelemetry::StateFlags::Possessed))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no frame had a Possessed entity";
				break;
			}
			case Criterion::AnySprintFrame:
			{
				if (AnyFrameWithFlag(xReader, DPTelemetry::StateFlags::Sprinting))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no sprint frames";
				break;
			}
			case Criterion::AnyWalkQuietFrame:
			{
				if (AnyFrameWithFlag(xReader, DPTelemetry::StateFlags::WalkQuiet))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no walk-quiet frames";
				break;
			}
			case Criterion::AnyHoldingItemFrame:
			{
				if (AnyFrameWithFlag(xReader, DPTelemetry::StateFlags::HoldingItem))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no holding-item frames";
				break;
			}
			case Criterion::PossessionFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::PossessionChanged))
				 || AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::PossessedSwitched))
				 || AnyFrameWithFlag(xReader, DPTelemetry::StateFlags::Possessed))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no possession event or frame";
				break;
			}
			case Criterion::InteractFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Interact))
				 || AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::InteractionBegin)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no Interact / InteractionBegin event";
				break;
			}
			case Criterion::VictoryFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Victory)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no Victory event";
				break;
			}
			case Criterion::RunLostFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::RunLost)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no RunLost event";
				break;
			}
			case Criterion::TerminalEventFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Victory))
				 || AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::RunLost)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "run never terminated (no Victory or RunLost)";
				break;
			}
			case Criterion::PickupFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::ItemPickup)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no ItemPickup event";
				break;
			}
			case Criterion::PossessionChangedFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::PossessionChanged)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no PossessionChanged event";
				break;
			}
			case Criterion::DoorOpenedFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::DoorOpened)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no DoorOpened event";
				break;
			}
			case Criterion::ChestOpenedFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::ChestOpened)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no ChestOpened event";
				break;
			}
			case Criterion::ForgeCraftedFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::ForgeCrafted)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no ForgeCrafted event";
				break;
			}
			case Criterion::ObjectivePlacedFired:
			{
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::ObjectivePlaced)))
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "no ObjectivePlaced event";
				break;
			}
			case Criterion::PriestMoved:
			{
				const float fPath = SumPriestPathLength(xReader);
				if (fPath >= xT.fMinPriestPathLengthM)
					{ xR.bPassed = true; xR.szReason = "ok"; }
				else
					xR.szReason = "priest never moved (path length below threshold)";
				break;
			}
			case Criterion::None:
			default:
				xR.szReason = "criterion not implemented"; break;
			}
			return xR;
		}
	}

	// =========================================================
	// Pure entry point.
	// =========================================================
	Verdict Analyze(const Zenith_Telemetry::Reader& xReader,
	                const Criterion* aeCriteria,
	                uint32_t uCount,
	                const Thresholds& xT)
	{
		Verdict xV;
		xV.uFrameCount = xReader.GetFrames().GetSize();
		xV.uEventCount = xReader.GetEvents().GetSize();

		bool bAllPassed = (uCount > 0u);
		for (uint32_t i = 0; i < uCount; ++i)
		{
			const CriterionResult xR = Check(aeCriteria[i], xReader, xT);
			xV.axResults.PushBack(xR);
			if (!xR.bPassed) bAllPassed = false;
		}
		xV.bOverallPass = bAllPassed;
		return xV;
	}

	Verdict AnalyzeFile(const char* szBinaryPath,
	                    const Criterion* aeCriteria,
	                    uint32_t uCount,
	                    const Thresholds& xT)
	{
		Zenith_Telemetry::Reader xReader;
		Verdict xV;
		if (!xReader.LoadFromFile(szBinaryPath))
		{
			// Synthesize a single failed result so callers see the cause.
			CriterionResult xR;
			xR.eCriterion = Criterion::HeaderMagicValid;
			xR.bPassed    = false;
			xR.szReason   = "file could not be loaded";
			xV.axResults.PushBack(xR);
			xV.bOverallPass = false;
			return xV;
		}
		return Analyze(xReader, aeCriteria, uCount, xT);
	}
}
