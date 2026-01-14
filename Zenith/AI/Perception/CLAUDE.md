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
    Zenith_EntityID m_xEntityID;        // Target entity
    Zenith_Maths::Vector3 m_xLastKnownPosition;
    float m_fTimeSinceLastSeen;
    float m_fAwareness;                 // 0.0 to 1.0
    bool m_bCurrentlyVisible;
    uint32_t m_uStimulusMask;           // SIGHT | HEARING | DAMAGE
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
    float m_fHeightOffset = 1.6f;        // Eye height from entity origin
    bool m_bRequireLineOfSight = true;   // Use raycasts for occlusion
};
```

### Detection Zones

1. **Primary FOV**: Full awareness gain rate within m_fFOVAngle
2. **Peripheral**: Reduced gain rate within m_fPeripheralAngle
3. **Behind**: No sight detection

### Awareness Mechanics

- Awareness increases when target visible
- Gain rate based on distance (closer = faster)
- Peripheral vision has reduced gain rate
- Awareness decays when target not visible
- Target forgotten when awareness reaches 0

### Line of Sight

When `m_bRequireLineOfSight = true`:
- Raycasts from eye position to target
- Checks for collider occlusion
- Blocked targets still get reduced awareness gain

## Hearing Perception

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
- Audible if: `loudness * (1 - distance/radius) > threshold`
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
    xAttackerID,  // Entity that dealt damage
    fDamage       // Damage amount
);
```

### Detection

- Immediate full awareness of attacker
- Bypasses sight/hearing checks
- Stores attacker's current position
- High priority target selection

## Memory System

### Memory Decay

```cpp
struct Zenith_SightConfig
{
    float m_fAwarenessGainRate = 0.5f;   // Per second when visible
    float m_fAwarenessDecayRate = 0.1f;  // Per second when not visible
    float m_fForgetThreshold = 0.0f;     // Remove target at this awareness
};
```

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
// Register entity as perceivable target
Zenith_PerceptionSystem::RegisterTarget(xTargetID, "Enemy");

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

// Check if target currently visible
bool bVisible = Zenith_PerceptionSystem::IsTargetVisible(xAgentID, xTargetID);
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
Zenith_BTCondition_CanSeeTarget("TargetEntity", 0.5f)

// Check if any targets perceived
Zenith_BTCondition_HasAwareness(0.3f)
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

**Awareness Not Decaying**:
- Check decay rate is > 0
- Ensure Update() called each frame

**Performance Issues**:
- Reduce m_fMaxRange
- Increase stagger interval
- Disable LOS checks (m_bRequireLineOfSight = false)
