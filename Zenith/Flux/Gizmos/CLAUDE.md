# Flux Gizmos System

## Overview

3D transform manipulation gizmos for the editor (tools-only — the whole feature is wrapped in `#ifdef ZENITH_TOOLS`). Provides translate / rotate / scale handles drawn over the target entity, plus the raycast + interaction logic that applies the manipulation back to the entity's transform.

## Files

- `Flux_GizmosImpl.h` - Class declaration (`Flux_GizmosImpl`), `GizmoComponent`/`GizmoMode` enums, `Flux_GizmoDrawPacket` struct
- `Flux_Gizmos.cpp` - Implementation
- `Flux_Gizmos_Shaders.h` - Shader declarations for the Gizmos render feature (`Flux_GizmosShaders::xGizmos` + `apxALL`); included by `Flux_Gizmos.cpp` and used in `BuildPipelines`

## Types

- `GizmoComponent` (enum class) - The interaction target: `None`, `TranslateX/Y/Z`, `RotateX/Y/Z`, `ScaleX/Y/Z`, `ScaleXYZ` (uniform centre handle).
- `GizmoMode` (enum class) - `Translate`, `Rotate`, `Scale`.
- `Flux_GizmoDrawPacket` - Main-thread snapshot of gizmo state (matrix, scale, entity pos, mode, hovered/active component, interacting flag, `m_bValid`) passed to the worker-thread record callback.
- `GizmoGeometry` (nested in `Flux_GizmosImpl`) - One renderable handle: vertex/index buffers, index count, colour, and its `GizmoComponent`.

## Architecture

The Gizmos pass is split across two threads to keep ECS access safe:

- **Main thread (Prepare):** `GatherGizmoPacket` reads the live target transform from the ECS, computes the gizmo matrix / camera-relative scale, snapshots the interaction-highlight state into `m_xDrawPacket`, and (under `ZENITH_DEBUG`) issues the interaction-bound wireframe cubes. `m_xDrawPacket.m_bValid` is false when there is no editable target.
- **Worker thread (Execute):** `ExecuteGizmos` reads ONLY the snapshot packet — no ECS access, no shared-state mutation — and early-outs when the packet is invalid. Mirrors `Flux_StaticMeshesImpl::GatherDrawPacket` / record split.

Three geometry sets are generated up front, one per mode: translation (arrows on X/Y/Z), rotation (circles on X/Y/Z), and scale (cube handles per axis + a uniform centre handle).

## Key Methods

- Targeting / mode: `SetTargetEntity`, `SetGizmoMode`, `GetGizmoMode`, `GetGizmoTargetWithTransform`.
- Interaction: `BeginInteraction`, `UpdateInteraction`, `EndInteraction`, `IsInteracting`, `RaycastGizmo` (identifies the hovered/active `GizmoComponent`), `ApplyTranslation`, `ApplyRotation`, `ApplyScale`.
- Geometry: `GenerateTranslationGizmoGeometry`, `GenerateRotationGizmoGeometry`, `GenerateScaleGizmoGeometry`.

Interaction flow: raycast to identify the component under the cursor, track hovered/active state, then apply the delta back to the target entity's transform via `g_xGizmoTransformAccess`.

## Related Files

- `Zenith/Editor/Zenith_Gizmo.h/cpp` - ScreenToWorldRay utility
