#include "Zenith.h"
#include "Source/DPTelemetryAnalyzer.h"
#include "Source/DPTelemetry.h"

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
				if (xH.uVersion != 1u)        { xR.szReason = "version != 1"; break; }
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
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Possession))
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
				if (AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::PossessionChanged))
				 || AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Possession))
				 || AnyEventOfType(xReader, static_cast<uint16_t>(DPTelemetry::DPEventType::Unpossession)))
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
