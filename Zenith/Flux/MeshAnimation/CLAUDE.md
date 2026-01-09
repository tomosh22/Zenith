# Mesh Animation System

## Overview

The mesh animation system provides skeletal animation for 3D models. It consists of animation clips that store keyframe data, skeleton instances that manage runtime bone state, and integration with the GPU skinning pipeline.

## Core Components

### Flux_AnimationClip
Stores animation keyframe data loaded from `.zanim` files.

**Bone Channels:** Each animated bone has a `Flux_BoneChannel` containing:
- Position keyframes (Vector3 + time)
- Rotation keyframes (Quaternion + time)
- Scale keyframes (Vector3 + time)

**Sampling:** `SamplePosition/Rotation/Scale(float fTime)` interpolates between keyframes. Uses linear interpolation for position/scale, spherical linear interpolation (slerp) for rotation.

**Loading:** `LoadFromAssimp()` imports from Assimp's `aiAnimation` structure. `LoadFromZanimFile()` loads the binary format.

### Flux_SkeletonInstance
Runtime skeleton state for a single animated entity.

**Per-Bone State:**
- Local position, rotation, scale (current pose)
- Model-space transform (computed from hierarchy)
- Skinning matrix (for GPU upload)

**Key Methods:**
- `SetToBindPose()` - Reset to skeleton asset's bind pose
- `SetBoneLocalTransform()` - Set individual bone's local TRS
- `ComputeSkinningMatrices()` - Walk hierarchy, compute skinning matrices
- `UploadToGPU()` - Upload skinning matrices to constant buffer

**Bone Ordering:** Bones are stored in hierarchical order (parents before children), allowing single-pass model-space computation without recursion.

### Flux_AnimationController
Manages animation playback for an entity.

**Playback State:** Tracks current animation clip, playback time, speed, and looping mode.

**Update:** Each frame advances time and samples the animation clip to update skeleton instance bone transforms.

**Blending:** Supports crossfading between animations via weighted bone transform blending.

## Skinning Equation

The fundamental skinning equation transforms vertices from mesh-local space to world space:

```
skinnedPos = sum(weight[i] * skinningMatrix[boneIndex[i]] * meshLocalPos)
```

Where each skinning matrix is:
```
skinningMatrix = modelSpaceTransform * inverseBindPose
```

**At Bind Pose:** When all bones are at their bind pose transforms, `modelSpaceTransform * inverseBindPose` should equal the expected bind pose world position for each bone.

## GPU Integration

**Constant Buffer:** Skinning matrices are uploaded to a per-instance constant buffer indexed by bone ID.

**Vertex Shader:** Animated mesh vertices include bone indices (uvec4) and weights (vec4). The shader samples the bone matrix buffer and computes the weighted sum.

**Triple Buffering:** Bone buffers are multi-buffered to match `MAX_FRAMES_IN_FLIGHT`, preventing GPU access conflicts.

## Animation Data Flow

1. **Export:** Assimp `aiAnimation` converted to `Flux_AnimationClip`, saved as `.zanim`
2. **Load:** `.zanim` loaded into `Flux_AnimationClip` at runtime
3. **Update:** `Flux_AnimationController::Update()` advances time, samples clip
4. **Apply:** Sampled TRS values written to `Flux_SkeletonInstance` bones
5. **Compute:** `ComputeSkinningMatrices()` walks bone hierarchy
6. **Upload:** Skinning matrices uploaded to GPU constant buffer
7. **Render:** Animated mesh shader samples bone matrices for vertex skinning

## Animation State Machine

### Flux_AnimationStateMachine
High-level animation control using a state-based model (Flux_AnimationStateMachine.h/cpp).

**Components:**
- **States** (`Flux_AnimationState`): Named states with blend trees and outgoing transitions
- **Transitions** (`Flux_StateTransition`): Rules for changing states with conditions, duration, and priority
- **Parameters** (`Flux_AnimationParameters`): Float, Int, Bool, and Trigger values used in transition conditions
- **Conditions** (`Flux_TransitionCondition`): Comparisons (Greater, Less, Equal, etc.) against parameters

**Key Design Decisions:**

1. **Transitions Only Checked When Not Transitioning**: The `Update()` method only checks for new transitions when `m_pxActiveTransition == nullptr`. This prevents:
   - Same transition being restarted every frame (causing transitions to never complete)
   - Lower priority transitions from interrupting higher priority ones

2. **Trigger Consumption**: Trigger parameters are consumed (reset to false) when evaluated. This ensures one-shot transitions.

3. **Exit Time Transitions**: Transitions with `m_bHasExitTime = true` only fire after the source animation reaches `m_fExitTime` (normalized 0-1).

4. **Priority Ordering**: Transitions are sorted by priority (highest first). When checking transitions, the first valid one wins.

**Common Pitfall - Transition Restart Bug:**
If transitions are checked during an active transition AND the transition condition is still true (e.g., Speed > 0.1 remains true), calling `StartTransition()` will restart the transition, resetting elapsed time. The fix is to only check transitions when not already transitioning.

## File Structure

```
MeshAnimation/
  Flux_AnimationClip.h/cpp           - Animation keyframe storage
  Flux_AnimationController.h/cpp     - Playback control
  Flux_AnimationStateMachine.h/cpp   - State machine for animation control
  Flux_SkeletonInstance.h/cpp        - Runtime skeleton state
  Flux_BonePose.h/cpp                - Bone transform utilities
  Flux_BlendTree.h/cpp               - Animation blending
  Flux_InverseKinematics.h/cpp       - IK solving
```

## Constants

- `MAX_BONES = 100` - Maximum bones per skeleton (in Flux_SkeletonInstance.h, must match shader)
- `BONES_PER_VERTEX_LIMIT = 4` - Maximum bone influences per vertex (defined in AssetHandling/Zenith_MeshAsset.h)
