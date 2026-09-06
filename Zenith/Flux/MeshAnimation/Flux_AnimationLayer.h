#pragma once
#include "Flux_AnimationStateMachine.h"
#include <string>

//=============================================================================
// Flux_LayerBlendMode
// How this layer's output is combined with the layers below it
//=============================================================================
enum Flux_LayerBlendMode : uint8_t
{
	LAYER_BLEND_OVERRIDE,   // Replace lower layers (masked by avatar mask)
	LAYER_BLEND_ADDITIVE    // Add on top of lower layers
};

//=============================================================================
// Flux_AnimationLayer
// A single animation layer with its own state machine, weight, and bone mask
// Layers allow multiple independent state machines to compose a final pose
// (e.g., lower body locomotion + upper body combat overlay)
//=============================================================================
class Flux_AnimationLayer
{
public:
	Flux_AnimationLayer() = default;
	Flux_AnimationLayer(const std::string& strName);
	~Flux_AnimationLayer();

	// Non-copyable, movable
	Flux_AnimationLayer(const Flux_AnimationLayer&) = delete;
	Flux_AnimationLayer& operator=(const Flux_AnimationLayer&) = delete;
	Flux_AnimationLayer(Flux_AnimationLayer&& xOther) noexcept;
	Flux_AnimationLayer& operator=(Flux_AnimationLayer&& xOther) noexcept;

	// Name
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	// Weight (0 = no effect, 1 = full effect)
	float GetWeight() const { return m_fWeight; }
	void SetWeight(float fWeight) { m_fWeight = glm::clamp(fWeight, 0.0f, 1.0f); }

	// Blend mode
	Flux_LayerBlendMode GetBlendMode() const { return m_eBlendMode; }
	void SetBlendMode(Flux_LayerBlendMode eMode) { m_eBlendMode = eMode; }

	// Avatar mask (per-bone weights for this layer)
	const Flux_BoneMask& GetAvatarMask() const { return m_xAvatarMask; }
	void SetAvatarMask(const Flux_BoneMask& xMask) { m_xAvatarMask = xMask; m_bHasAvatarMask = true; }
	bool HasAvatarMask() const { return m_bHasAvatarMask; }

	// State machine (each layer has its own)
	Flux_AnimationStateMachine& GetStateMachine();
	const Flux_AnimationStateMachine* GetStateMachinePtr() const { return m_pxStateMachine; }
	Flux_AnimationStateMachine* CreateStateMachine(const std::string& strName = "Default");

	// Output pose
	const Flux_SkeletonPose& GetOutputPose() const { return m_xOutputPose; }

	// Update this layer's state machine
	void Update(float fDt, const Zenith_SkeletonAsset& xSkeleton);

	//=========================================================================
	// Animation events (WU-5A / D36)
	//
	// ★ EVERY LAYER EMITS INDEPENDENTLY, and that is the correct default rather
	// than a simplification. A masked upper/lower split is TWO animations
	// playing at once on one skeleton — the legs' footsteps and the arms'
	// weapon-swing beats are both real, and nothing above can tell which of the
	// two a listener meant. Arbitration (D35) is WITHIN a layer, never across.
	//
	// This flag is the explicit way to silence ONE layer — a cosmetic overlay,
	// or a second copy of the locomotion clip used only for its pose. It is
	// deliberately NOT derived from the layer's WEIGHT: a layer's weight is
	// animated (that is what fading a layer in IS), so tying events to it would
	// make a footstep fire or not depending on where in the fade the frame
	// landed.
	//=========================================================================
	void SetEmitEvents(bool bEmit) { m_bEmitEvents = bEmit; }
	bool GetEmitEvents() const { return m_bEmitEvents; }

	// Hand over (and clear) this layer's clip-event spans. A silenced layer is
	// still WALKED — with a null sink — so re-enabling it cannot fire a span the
	// silenced frames left pending.
	void CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>* pxOutSpans);

	// Initialize pose storage
	void InitializePose(uint32_t uNumBones);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::string m_strName;
	float m_fWeight = 1.0f;
	Flux_LayerBlendMode m_eBlendMode = LAYER_BLEND_OVERRIDE;
	bool m_bHasAvatarMask = false;
	// WU-5A (D36). NOT serialized — D41 changes no schema; a game that wants a
	// layer silent sets it after building the layer, beside SetBlendMode.
	bool m_bEmitEvents = true;
	Flux_BoneMask m_xAvatarMask;
	Flux_AnimationStateMachine* m_pxStateMachine = nullptr;  // Owned
	Flux_SkeletonPose m_xOutputPose;
};
