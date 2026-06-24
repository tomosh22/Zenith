# Perception System

## Overview

The perception system provides sensory awareness for AI agents including sight, hearing, and damage detection. It tracks perceived targets with awareness levels that change over time.

## Architecture

```
Zenith_PerceptionSystem (singleton manager)
├── RegisterAgent()      // Add agent to system
├── Update()             // Process all senses
├── EmitSoundStimulus()  // Trigger hearing detection
├── EmitDamageStimulus() // Trigger damage awareness
└── Query functions      // Get perceived targets
```

## Perceived Targets

Each agent maintains a list of perceived targets:

```cpp
struct Zenith_PerceivedTarget
{
    Zenith_EntityID m_xEntityID;              // Target entity
    Zenith_Maths::Vector3 m_xLastKnownPosition;
    Zenith_Maths::Vector3 m_xEstimatedVelocity;   // For position prediction
    float m_fTimeSinceLastSeen = 0.0f;
    float m_fAwareness = 0.0f;                // 0.0 to 1.0
    bool m_bCurrentlyVisible = false;
    uint32_t m_uStimulusMask = 0;             // SIGHT | HEARING | DAMAGE
    bool m_bHostile = false;                  // Target hostility state
};
```

## Sight Perception

### Configuration

```cpp
struct Zenith_SightConfig
{
    float m_fMaxRange = 30.0f;           // Max sight distance
    float m_fFOVAngle = 90.0f;           // Primary FOV (degrees)
    float m_fPeripheralAngle = 120.0f;   // Peripheral vision angle
    float m_fPeripheralMultiplier = 0.5f; // Awareness gain rate in peripheral
    float m_fEyeHeight = 1.6f;            // Eye height from entity origin
    bool m_bRequireLineOfSight = true;   // Use raycasts for occlusion
    float m_fAwarenessGainRate = 2.0f;   // Awareness gain per second when visible
    float m_fAwarenessDecayRate = 0.5f;  // Awareness loss per second when not visible
};
```

### Detection Zones

1. **Primary FOV**: Full awareness gain rate within m_fFOVAngle
2. **Peripheral**: Reduced gain rate within m_fPeripheralAngle
3. **Behind**: No sight detection

The FOV/peripheral cones are measured against the agent's **forward vector**, derived
each tick by rotating the +Z basis by the agent's transform rotation (`xForward = quat *
(0,0,1)`, projected to XZ). This is correct for **any** facing, including the −Z
hemisphere.

> **Do NOT derive the forward from `glm::eulerAngles(quat).y`.** That asin-based middle
> (yaw) angle collapses for facings more than ~90° off +Z — a 180° (−Z) facing decodes
> to yaw 0, so the forward computes as +Z and the cone points the wrong way, blinding the
> agent to a target directly in front of it. This was a real bug (fixed 2026-06); the
> regression test is `SightConeFacingNegativeZSeesTargetInFront`.

### Awareness Mechanics

- Awareness increases when target visible (gain rate defaults to `2.0` per second)
- Gain rate based on distance (closer = faster)
- Peripheral vision has reduced gain rate
- Awareness decays when target not visible (decay rate defaults to `0.5` per second)
- Target removed automatically when awareness reaches `0.0` (there is no separate forget-threshold field)

### Line of Sight

When `m_bRequireLineOfSight = true`:
- Raycasts from eye position to target
- Checks for collider occlusion
- Blocked targets are not updated; they do not gain awareness while blocked

## Hearing Perception

### Configuration

```cpp
struct Zenith_HearingConfig
{
    float m_fMaxRange = 20.0f;          // Maximum hearing distance
    float m_fLoudnessThreshold = 0.1f;  // Minimum loudness to detect
    bool m_bCheckOcclusion = false;     // Check for wall occlusion
};
```

Set per agent via `Zenith_PerceptionSystem::SetHearingConfig(xAgentID, xConfig)`.

### Sound Stimuli

```cpp
Zenith_PerceptionSystem::EmitSoundStimulus(
    xPosition,    // Sound origin
    fLoudness,    // Base loudness (0-1)
    fRadius,      // Max audible distance
    xSourceID     // Entity that made the sound
);
```

### Detection

- Sound attenuates with distance
- Audible if: `loudness * (1 - distance/radius) >= threshold`
- Detected sounds trigger awareness gain
- Position stored in last known location

### Common Sound Types

| Sound | Loudness | Radius |
|-------|----------|--------|
| Footstep | 0.3 | 10m |
| Gunshot | 1.0 | 50m |
| Voice | 0.5 | 15m |
| Explosion | 1.0 | 100m |

## Damage Perception

### Damage Stimuli

```cpp
Zenith_PerceptionSystem::EmitDamageStimulus(
    xVictimID,    // Entity that was damaged
    xAttackerID   // Entity that dealt damage
);
```

### Detection

- Immediate full awareness of attacker
- Bypasses sight/hearing checks
- Stores attacker's current position
- High priority target selection

## Memory System

### Memory Decay

Awareness gain/decay rates live on `Zenith_SightConfig` (see *Sight Perception*):

```cpp
float m_fAwarenessGainRate = 2.0f;   // Per second when visible
float m_fAwarenessDecayRate = 0.5f;  // Per second when not visible
```

There is no forget-threshold field — a target is removed automatically once its
awareness decays to `0.0`.

### Prediction

When target not visible, system can predict position:
- Uses last known position
- Optionally uses estimated velocity
- Useful for search behaviors

## API Reference

### Registration

```cpp
// Add agent to perception system
Zenith_PerceptionSystem::RegisterAgent(xEntityID);

// Configure sight
Zenith_SightConfig xConfig;
xConfig.m_fMaxRange = 25.0f;
xConfig.m_fFOVAngle = 120.0f;
Zenith_PerceptionSystem::SetSightConfig(xEntityID, xConfig);

// Unregister when entity destroyed
Zenith_PerceptionSystem::UnregisterAgent(xEntityID);
```

### Target Registration

```cpp
// Register entity as perceivable target (bHostile defaults to true)
Zenith_PerceptionSystem::RegisterTarget(xTargetID, true);  // true = hostile

// Change hostility of a registered target later
Zenith_PerceptionSystem::SetTargetHostile(xTargetID, false);

// Unregister
Zenith_PerceptionSystem::UnregisterTarget(xTargetID);
```

### Queries

```cpp
// Get all perceived targets
const auto* pxTargets = Zenith_PerceptionSystem::GetPerceivedTargets(xAgentID);

// Get highest awareness target
Zenith_EntityID xTarget = Zenith_PerceptionSystem::GetPrimaryTarget(xAgentID);

// Get awareness of specific target
float fAwareness = Zenith_PerceptionSystem::GetAwarenessOf(xAgentID, xTargetID);

// Check if an agent is aware of a specific target
bool bAware = Zenith_PerceptionSystem::IsAwareOf(xAgentID, xTargetID);

// Get the most-recent heard sound stimulus (bridges hearing into BT blackboard
// investigate positions); m_bValid is false when nothing has been heard
Zenith_PerceptionSystem::Zenith_LastHeardSound xSound =
    Zenith_PerceptionSystem::GetLastHeardSoundFor(xAgentID);
```

## Debug Visualization

Enable via `Zenith_AIDebugVariables`:

- `s_bDrawSightCones`: FOV cone outline
- `s_bDrawHearingRadius`: Circle showing hearing range
- `s_bDrawDetectionLines`: Lines to perceived targets (color = awareness)
- `s_bDrawMemoryPositions`: Last known positions for lost targets

## Usage in Behavior Trees

### Condition Nodes

```cpp
// Check if has any valid target
Zenith_BTCondition_HasTarget("TargetEntity")

// Check if target visible with minimum awareness
// (min awareness is set via SetMinAwareness, not the constructor)
Zenith_BTCondition_CanSeeTarget cond("TargetEntity");
cond.SetMinAwareness(0.5f);

// Check if any targets perceived
Zenith_BTCondition_HasAwareness cond;
cond.SetMinAwareness(0.3f);
```

### Action Nodes

```cpp
// Find and set primary target
Zenith_BTAction_FindPrimaryTarget()  // Sets "TargetEntity" in blackboard
```

## Performance Considerations

- Agents processed in batches, not all every frame
- Stagger interval prevents perception spike
- Raycast budget limits per-frame LOS checks
- Spatial partitioning accelerates target queries
- Dead targets auto-removed

## Common Issues

**Not Detecting Targets**:
- Ensure target registered with RegisterTarget()
- Check sight config range and FOV
- Verify target within line of sight
- If the agent is blind ONLY at certain facings (e.g. a −Z-facing agent can't see a
  target right in front of it), suspect the forward-vector derivation: it must be
  `quat * +Z`, never `glm::eulerAngles(quat).y` (which collapses past ±90° off +Z). A
  360° FOV "fixes" this only by masking it — prefer the correct forward + a realistic FOV.

**Awareness Not Decaying**:
- Check decay rate is > 0
- Ensure Update() called each frame

**Performance Issues**:
- Reduce m_fMaxRange
- Increase stagger interval
- Disable LOS checks (m_bRequireLineOfSight = false)
