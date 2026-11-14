# CHAR design: member types of the root header

Section file of `Zenith2/Docs/Design/CHAR.md`. It declares in full the value types and enumerations of `Zenith2/Tools/Authoring/Zenith_AuthoringCharacter.h` that the API sections of the entry document use as members or elements: the control and pose-book members, the clip channel types, the assembly members, the template and scenario types, the validation references and the pose-diff deltas. Each type carries its contract block ([Z2-N-CONTRACT-030], [Z2-N-CONTRACT-111]). The header declares each of these types immediately before the first entry-document type that uses it and after every type it uses itself; the header is one file, so the split into this section file is editorial only. A `Zenith_Array<char>` member holds UTF-8 text without a terminating NUL; a `Zenith_StringView` member borrows text whose lifetime its contract names.

## Scope of this file

| Types | Heading |
|---|---|
| control set members and control outputs, pose book poses | "Control and pose book member types" |
| curve clip channels, markers and events | "Clip member types" |
| assembly slots, parts and loadouts | "Assembly member types" |
| template descriptors, clip binding, scenarios and their results | "Template and scenario types" |
| validation inputs | "Validation member types" |
| pose diff results | "Pose debugging member types" |
| the enumerations replacing raw codes | "Enumerations for coded fields" |

## Control and pose book member types

```cpp
inline constexpr uint32 uCHAR_UNRESOLVED = 0xFFFFFFFFu;   // an index a resolve step has not filled

/// @contract enum Zenith_CharControlKind
/// Ownership: Plain enumeration of the control set document's controls.*.kind (Rigging.md "Control set document").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharControlKind : uint8 { CHAR_CONTROL_KIND_SCALAR = 1, CHAR_CONTROL_KIND_POINT };

/// @contract enum Zenith_CharControlTargetKind
/// Ownership: Plain enumeration of controls.*.target.kind.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharControlTargetKind : uint8
{
	CHAR_CONTROL_TARGET_PARAMETER = 1, CHAR_CONTROL_TARGET_CURVE, CHAR_CONTROL_TARGET_POSE, CHAR_CONTROL_TARGET_JOINT_ROTATION,
	CHAR_CONTROL_TARGET_IK_GOAL,
};

/// @contract value Zenith_CharControlTarget
/// Ownership: Owns its name text; part of a Zenith_CharControl.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_xAxis a unit vector in the joint's local frame; m_fDegreesAtMax degrees.
/// Errors: None; Zenith_CharReadControlSet fills only the members of m_eKind and leaves the others at their defaults;
///   Zenith_CharResolveControlSet fills m_bResolved, m_uJoint and m_uPose (Rigging.md "Control evaluation").
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharControlTarget
{
	Zenith_CharControlTargetKind m_eKind = CHAR_CONTROL_TARGET_PARAMETER;
	uint8 m_uIKGoal = 0;                        // IK_GOAL: the goal index 0 … 7
	uint16 m_uReserved = 0;
	bool m_bResolved = false;                    // Zenith_CharReadControlSet sets it true for PARAMETER, CURVE and IK_GOAL (name-keyed);
	                                             // Zenith_CharResolveControlSet sets it for JOINT_ROTATION and POSE
	uint32 m_uJoint = uCHAR_UNRESOLVED;         // JOINT_ROTATION: the joint index in the skeleton view
	uint32 m_uPose = uCHAR_UNRESOLVED;          // POSE: the pose's index in the pose book m_xPoseBook
	uint64 m_ulNameHash = 0;                    // Zenith_Hash64 of m_strName: the parameter, curve, pose or IK definition
	Zenith_Array<char> m_strName;                // PARAMETER, CURVE: its name; POSE: the pose name; JOINT_ROTATION: the joint name;
	                                             // IK_GOAL: the definition name
	Zenith_PersistentID m_xPoseBook;             // POSE only
	Zenith_Vector3 m_xAxis = { 1.0f, 0.0f, 0.0f };   // JOINT_ROTATION only
	float m_fDegreesAtMax = 0.0f;                // JOINT_ROTATION only
};

/// @contract value Zenith_CharControl
/// Ownership: Owns its name and target; an element of Zenith_CharControlSet::m_axControls.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_fMin, m_fMax, m_fDefault in the control's value units (SCALAR only).
/// Errors: None; the reader refuses min >= max or a default outside the range.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharControl
{
	Zenith_PersistentID m_xID;                   // the controls map key
	Zenith_Array<char> m_strName;
	uint32 m_uGroup = 0;                        // index into Zenith_CharControlSet::m_astrGroups
	Zenith_CharControlKind m_eKind = CHAR_CONTROL_KIND_SCALAR;
	float m_fMin = 0.0f, m_fMax = 1.0f, m_fDefault = 0.0f;
	Zenith_CharControlTarget m_xTarget;
};

/// @contract value Zenith_CharParameterValue
/// Ownership: Plain value; an element of Zenith_CharControlOutputs::m_axParameters.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: m_fValue in the parameter's units (a Bool is 0 or 1).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharParameterValue { uint64 m_ulNameHash = 0; float m_fValue = 0.0f; };

/// @contract value Zenith_CharCurveValue
/// Ownership: Plain value; an element of Zenith_CharControlOutputs::m_axCurves.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: Curve units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharCurveValue { uint64 m_ulNameHash = 0; float m_fValue = 0.0f; };

/// @contract value Zenith_CharPoseWeight
/// Ownership: Plain value; an element of Zenith_CharControlOutputs::m_axPoses.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: m_uPose an index into the pose book's poses; m_fWeight in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseWeight { Zenith_PersistentID m_xPoseBook; uint32 m_uPose = 0; float m_fWeight = 0.0f; };

/// @contract value Zenith_CharJointRotation
/// Ownership: Plain value; an element of Zenith_CharControlOutputs::m_axRotations.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: m_xAxis a unit vector in the joint's local frame; m_fRadians radians.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharJointRotation { uint32 m_uJoint = 0; Zenith_Vector3 m_xAxis = { 1.0f, 0.0f, 0.0f }; float m_fRadians = 0.0f; };

/// @contract value Zenith_CharIKGoalOutput
/// Ownership: Plain value; an element of Zenith_CharControlOutputs::m_axIKGoals.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: m_xModelPosition metres in the preview's model space.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharIKGoalOutput
{
	uint8 m_uGoal = 0;                          // 0 … 7
	uint8 m_auReserved[7] = {};
	uint64 m_ulDefinitionHash = 0;              // Zenith_Hash64 of the IK definition name of the rig document
	Zenith_Vector3 m_xModelPosition = {};
};

/// @contract enum Zenith_CharPoseComponent
/// Ownership: Plain bit flags of Zenith_CharPoseJointEntry::m_uComponents.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharPoseComponent : uint8 { CHAR_POSE_COMPONENT_T = 1u << 0, CHAR_POSE_COMPONENT_R = 1u << 1, CHAR_POSE_COMPONENT_S = 1u << 2 };

/// @contract value Zenith_CharPoseJointEntry
/// Ownership: Owns its joint name; an element of Zenith_CharPose::m_axJoints.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Metres; a unit quaternion (normalised by the reader); scale factors.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseJointEntry
{
	Zenith_Array<char> m_strJoint;               // the poses[].joints key
	uint8 m_uComponents = 0;                    // Zenith_CharPoseComponent bits: the components the document names
	Zenith_Vector3 m_xTranslation = {};
	Zenith_Quaternion m_xRotation = {};          // identity by default
	Zenith_Vector3 m_xScale = { 1.0f, 1.0f, 1.0f };
};

/// @contract value Zenith_CharPoseCurveEntry
/// Ownership: Owns its curve name; an element of Zenith_CharPose::m_axCurves.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Curve units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPoseCurveEntry { Zenith_Array<char> m_strCurve; float m_fValue = 0.0f; };

/// @contract value Zenith_CharPose
/// Ownership: Owns its name and entries; an element of Zenith_CharPoseBook::m_axPoses, in document order.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: As its entries.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPose
{
	Zenith_Array<char> m_strName;
	Zenith_Array<Zenith_CharPoseJointEntry> m_axJoints;   // ascending joint-name byte order (the keyed map's canonical order)
	Zenith_Array<Zenith_CharPoseCurveEntry> m_axCurves;   // ascending curve-name byte order
};
```

## Clip member types

```cpp
/// @contract enum Zenith_CharClipPath
/// Ownership: Plain enumeration; the values equal ANIM's ZICL path codes ("Clip intermediate" of ANIM/Formats.md).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharClipPath : uint8 { CHAR_CLIP_PATH_TRANSLATION = 1, CHAR_CLIP_PATH_ROTATION, CHAR_CLIP_PATH_SCALE, CHAR_CLIP_PATH_CURVE };

/// @contract enum Zenith_CharClipInterpolation
/// Ownership: Plain enumeration; the values equal ANIM's ZICL interpolation codes 1 and 2.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharClipInterpolation : uint8 { CHAR_CLIP_INTERPOLATION_STEP = 1, CHAR_CLIP_INTERPOLATION_LINEAR };

/// @contract value Zenith_CharClipChannel
/// Ownership: Owns its name and keys; an element of Zenith_CharCurveClip::m_axChannels.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_adTimes seconds, strictly increasing; m_afValues N × width floats, width 3 (translation, metres), 4 (rotation,
///   a unit quaternion x, y, z, w), 3 (scale) or 1 (curve units).
/// Errors: None; Zenith_CharRoundClipTimes enforces the order after rounding.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharClipChannel
{
	Zenith_Array<char> m_strName;                // the joint name, or the curve name for CHAR_CLIP_PATH_CURVE
	Zenith_CharClipPath m_ePath = CHAR_CLIP_PATH_CURVE;
	Zenith_CharClipInterpolation m_eInterpolation = CHAR_CLIP_INTERPOLATION_LINEAR;
	Zenith_Array<double> m_adTimes;
	Zenith_Array<float> m_afValues;
};

/// @contract value Zenith_CharClipMarker
/// Ownership: Owns its name; an element of Zenith_CharLipSyncLayout::m_axMarkers.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_dTime seconds from the clip start.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharClipMarker { Zenith_Array<char> m_strName; double m_dTime = 0.0; };

/// @contract value Zenith_CharClipEvent
/// Ownership: Owns its name; an element of Zenith_CharLipSyncLayout::m_axEvents and Zenith_CharScenarioResult::m_axEvents.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_dTime seconds (clip time in a layout, run time in a scenario result).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharClipEvent { Zenith_Array<char> m_strName; double m_dTime = 0.0; sint32 m_iPayload = 0; };
```

## Assembly member types

```cpp
inline constexpr uint32 uCHAR_MAX_MATERIAL_OVERRIDES = 8;
inline constexpr uint32 uCHAR_NO_REGION = 0xFFFFFFFFu;

/// @contract value Zenith_CharSlot
/// Ownership: Owns its name; an element of Zenith_CharAssemblyView::m_axSlots, in document order.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharSlot { Zenith_Array<char> m_strName; bool m_bExclusive = false; };

/// @contract enum Zenith_CharPartKind
/// Ownership: Plain enumeration of parts.*.kind (Modular.md "Assembly document").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharPartKind : uint8 { CHAR_PART_KIND_SKINNED = 1, CHAR_PART_KIND_RIGID_ATTACHMENT };

/// @contract value Zenith_CharPart
/// Ownership: Owns its texts; an element of Zenith_CharAssemblyView::m_axParts, in ascending part ID.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Indices into the view's slots and regions; m_uHiddenRegionMask bit r hides region r.
/// Errors: None; the reader refuses unknown slot and region names.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharPart
{
	Zenith_PersistentID m_xID;
	Zenith_Array<char> m_strName;
	uint32 m_uSlot = 0;                         // index into m_axSlots
	Zenith_CharPartKind m_eKind = CHAR_PART_KIND_SKINNED;
	bool m_bCastsShadows = true;
	bool m_bHairCards = false;
	uint8 m_uMaterialOverrideCount = 0;         // 0 … 8: the length of materialOverrides
	Zenith_PersistentID m_xMesh;
	uint32 m_uBodyRegion = uCHAR_NO_REGION;     // index into m_astrRegions, or uCHAR_NO_REGION
	uint32 m_uHiddenRegionMask = 0;             // 16 regions at most
	Zenith_PersistentID m_axMaterialOverrides[uCHAR_MAX_MATERIAL_OVERRIDES];   // a null ID: no override of slot i
	Zenith_Array<char> m_strSocket;              // RIGID_ATTACHMENT: the socket name as written; empty otherwise
	uint64 m_ulSocketHash = 0;                  // Zenith_Hash64 of m_strSocket, the key of Zenith_CharAssetQuery::HasSocket
};

/// @contract value Zenith_CharLoadout
/// Ownership: Owns its name and indices; an element of Zenith_CharAssemblyView::m_axLoadouts, in ascending loadout ID.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Indices into the view's m_axParts, in the document's order.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharLoadout { Zenith_PersistentID m_xID; Zenith_Array<char> m_strName; Zenith_Array<uint32> m_auParts; };
```

## Template and scenario types

```cpp
struct Zenith_CharPoseSnapshot;                  // "Pose debugging API" of the entry document

/// @contract value Zenith_CharClipCandidate
/// Ownership: Borrowed: m_strName views the caller's text for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: m_fSpeed metres per second (0 unknown); m_fDurationSeconds seconds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharClipCandidate
{
	Zenith_PersistentID m_xClip;
	Zenith_StringView m_strName;                 // the source animation name: <name> of anim:<name> and of anim:<name>@<skeleton>
	float m_fSpeed = 0.0f;                       // the clip's published root speed
	float m_fDurationSeconds = 0.0f;             // the clip's duration, used by the built-in scenarios' derived times
	double m_dFirstWindowEndSeconds = 0.0;       // a lip-sync clip: the time of its first char.lipsync.windowEnd event (e_0); 0 otherwise
};

/// @contract value Zenith_CharRoleBinding
/// Ownership: Borrowed: m_strRole views a role name of a template's static table (outputs) or the caller's text (inputs).
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None (static storage for outputs).
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharRoleBinding { Zenith_StringView m_strRole; Zenith_PersistentID m_xClip; };

/// @contract enum Zenith_CharTemplateRoleFlag
/// Ownership: Plain bit flags of Zenith_CharTemplateRole::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharTemplateRoleFlag : uint8
{
	CHAR_TEMPLATE_ROLE_REQUIRED = 1u << 0,       // an unbound role fails with CHAR_ERROR_BINDING_MISSING
	CHAR_TEMPLATE_ROLE_MULTI = 1u << 1,          // binds every match (the motion-matching template's locomotion role)
};

/// @contract value Zenith_CharTemplateRole
/// Ownership: constexpr data of a template table (static storage).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharTemplateRole
{
	const char* const m_szRole = nullptr;                        // "jumpStart"
	const uint8 m_uFlags = 0;                             // Zenith_CharTemplateRoleFlag bits
	const Zenith_Span<const char* const> m_aszRequiredA = {};   // a match holds one of these tokens; empty: any clip matches
	const Zenith_Span<const char* const> m_aszRequiredB = {};   // and one of these; empty: no second group
	const Zenith_Span<const char* const> m_aszExcluded = {};    // and none of these
};

/// @contract value Zenith_CharTemplateDesc
/// Ownership: constexpr data returned by Zenith_CharGetGraphTemplates (static storage).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharTemplateDesc
{
	const char* const m_szName = nullptr;                        // "char.humanoidLocomotion"
	const char* const m_szDocumentText = nullptr;                // the graph document text with its placeholders (Workflow.md "Graph template catalogue")
	const Zenith_Span<const Zenith_CharTemplateRole> m_axRoles = {};   // binding order
	const Zenith_Span<const char* const> m_aszScenarios = {};          // built-in scenario names (Workflow.md "Preview scenarios")
	const bool m_bRequiresMotionMatchingRuling = false;        // true for char.humanoidMotionMatching: listed only when the ruling says required
};

/// @contract enum Zenith_CharScenarioStepKind
/// Ownership: Plain enumeration of a scenario step's member (set, trigger, playSlot).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharScenarioStepKind : uint8 { CHAR_SCENARIO_STEP_SET = 1, CHAR_SCENARIO_STEP_TRIGGER, CHAR_SCENARIO_STEP_PLAY_SLOT };

/// @contract value Zenith_CharScenarioStep
/// Ownership: Owns its name text; an element of Zenith_CharScenario::m_axSteps (a JSON step setting several parameters
///   becomes one element per parameter, in key byte order).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds; parameter units (a Bool is 0 or 1).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharScenarioStep
{
	double m_dAtSeconds = 0.0;
	Zenith_CharScenarioStepKind m_eKind = CHAR_SCENARIO_STEP_SET;
	uint64 m_ulNameHash = 0;                    // SET, TRIGGER: the parameter; PLAY_SLOT: the slot
	float m_fValue = 0.0f;                       // SET
	Zenith_PersistentID m_xClip;                 // PLAY_SLOT: a GUID, or a role resolved against the binding before the run
	float m_fBlendInSeconds = 0.2f, m_fBlendOutSeconds = 0.25f;   // PLAY_SLOT
	Zenith_Array<char> m_strName;                // the parameter (SET, TRIGGER) or slot (PLAY_SLOT) name; the role is m_strClipRole
	Zenith_Array<char> m_strClipRole;            // PLAY_SLOT given by role; empty when given by GUID
};

/// @contract enum Zenith_CharAssertionKind
/// Ownership: Plain enumeration of a scenario assertion's kind.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharAssertionKind : uint8
{
	CHAR_ASSERTION_STATE_AT = 1, CHAR_ASSERTION_EVENT_COUNT, CHAR_ASSERTION_ROOT_DISTANCE, CHAR_ASSERTION_FINITE, CHAR_ASSERTION_POSE_MATCHES,
};

/// @contract value Zenith_CharScenarioAssertion
/// Ownership: Owns its text; an element of Zenith_CharScenario::m_axAssertions.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds; EVENT_COUNT bounds in events; ROOT_DISTANCE bounds in metres; POSE_MATCHES tolerances in metres, radians.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharScenarioAssertion
{
	Zenith_CharAssertionKind m_eKind = CHAR_ASSERTION_FINITE;
	uint32 m_uLayer = 0;                        // STATE_AT
	double m_dAtSeconds = 0.0;                   // STATE_AT, POSE_MATCHES
	double m_dFromSeconds = 0.0;                 // EVENT_COUNT
	double m_dToSeconds = -1.0;                  // EVENT_COUNT; negative: the end of the run
	uint64 m_ulNameHash = 0;                    // STATE_AT: the state; EVENT_COUNT: the event
	double m_dMin = 0.0, m_dMax = 0.0;           // EVENT_COUNT, ROOT_DISTANCE
	uint32 m_uSnapshot = 0;                     // POSE_MATCHES: index into Zenith_CharScenario::m_apxSnapshots
	float m_fMetres = 1e-4f, m_fRadians = 1e-3f; // POSE_MATCHES
	Zenith_Array<char> m_strName;                // the state or event name, for reports
};

/// @contract value Zenith_CharScenario
/// Ownership: Owns its steps, assertions and snapshot paths; m_apxSnapshots is borrowed: the caller loads each path of
///   m_astrSnapshotPaths with Zenith_CharReadPoseSnapshot and keeps the snapshots alive for the run (the runner reads no file).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds.
/// Errors: None; Zenith_CharRunScenario refuses a POSE_MATCHES index without a snapshot with CHAR_ERROR_INVALID_ARGUMENT.
/// Version: since Z2 Stage 4. Read from zenith2.char-scenario version 1 (Workflow.md "Preview scenarios").
/// @endcontract
struct Zenith_CharScenario
{
	Zenith_Array<char> m_strName;
	double m_dDurationSeconds = 0.0;
	Zenith_Array<Zenith_CharScenarioStep> m_axSteps;           // ascending m_dAtSeconds, file order among equal times
	Zenith_Array<Zenith_CharScenarioAssertion> m_axAssertions; // file order
	Zenith_Array<Zenith_Array<char>> m_astrSnapshotPaths;      // one per distinct poseMatches snapshot, first-use order
	Zenith_Span<const Zenith_CharPoseSnapshot* const> m_apxSnapshots;   // parallel to m_astrSnapshotPaths, caller-set
};

/// @contract value Zenith_CharAssertionOutcome
/// Ownership: Owns its message; an element of Zenith_CharScenarioResult::m_axOutcomes, one per assertion in order.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: m_dObserved: STATE_AT the observed tick's time in seconds, EVENT_COUNT the count, ROOT_DISTANCE metres,
///   FINITE the first non-finite tick's time (or −1), POSE_MATCHES the largest joint distance in metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharAssertionOutcome
{
	uint32 m_uAssertion = 0;
	bool m_bPassed = false;
	double m_dObserved = 0.0;
	uint64 m_ulObservedStateHash = 0;           // STATE_AT: the state the layer was in
	Zenith_Array<char> m_strMessage;             // why it failed, for the report; empty when passed
};

/// @contract value Zenith_CharScenarioResult
/// Ownership: Owns its arrays; filled by Zenith_CharRunScenario (cleared first).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Seconds and metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharScenarioResult
{
	bool m_bPassed = false;                      // every outcome passed
	uint32 m_uTicks = 0;
	uint32 m_uNonFiniteTicks = 0;
	double m_dRootDistanceMetres = 0.0;          // the horizontal length of the composed root motion
	Zenith_Array<Zenith_CharAssertionOutcome> m_axOutcomes;
	Zenith_Array<Zenith_CharClipEvent> m_axEvents;   // every fired event in firing order, m_dTime the run time
};
```

## Validation member types

```cpp
/// @contract value Zenith_CharComponentRef
/// Ownership: Borrowed views into a resolved document the caller keeps alive for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CharComponentRef
{
	Zenith_StringView m_strStableName;           // "Zenith.Animation.Animator"; a record's references sort by it
	Zenith_JsonNode m_xFields;                   // the component object's "fields" (ECS's JSON field-set form)
};

/// @contract value Zenith_CharDocumentRef
/// Ownership: Borrowed views the caller keeps alive for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharDocumentRef
{
	Zenith_PersistentID m_xID;
	Zenith_TypeID m_xDocumentType;               // the registered document type (scene and prefab types included)
	Zenith_StringView m_strPath;                 // game-relative, for diagnostics and sorting
	const Zenith_JsonDocument* m_pxDocument = nullptr;   // the migrated tree
	Zenith_Span<const Zenith_CharEntityRecord> m_axEntities;   // scenes and prefabs: the resolved entity records; else empty
};
```

`Zenith_CharDocumentRef` follows `Zenith_CharEntityRecord` in the header ("Validation API" of the entry document).

## Pose debugging member types

```cpp
/// @contract value Zenith_CharJointDelta
/// Ownership: Plain value; an element of Zenith_CharPoseDiff::m_axJoints, in ascending m_uJointA.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: Metres, radians, a scale difference.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharJointDelta { uint32 m_uJointA = 0; uint32 m_uJointB = 0; float m_fMetres = 0.0f; float m_fRadians = 0.0f; float m_fScale = 0.0f; };

/// @contract value Zenith_CharCurveDelta
/// Ownership: Plain value; an element of Zenith_CharPoseDiff::m_axCurves, in ascending m_uCurveA.
/// Thread affinity: IMMUTABLE once appended; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array.
/// Invalidation: None.
/// Units: Curve units; m_fDelta = value in A − value in B.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CharCurveDelta { uint32 m_uCurveA = 0; uint32 m_uCurveB = 0; float m_fDelta = 0.0f; };
```

## Enumerations for coded fields

These replace the raw `uint8` codes of `Zenith_CharJsonEdit::m_eOp`, `Zenith_CharPoseSnapshot::m_eSourceKind` and `Zenith_CharMaterialFacts::m_eDomain`:

```cpp
/// @contract enum Zenith_CharJsonEditOp
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharJsonEditOp : uint8 { CHAR_JSON_EDIT_OP_SET = 1, CHAR_JSON_EDIT_OP_REMOVE };   // SET creates or replaces

/// @contract enum Zenith_CharPoseSourceKind
/// Ownership: Plain enumeration of a snapshot's source.kind (Debugging.md "Pose snapshot format").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharPoseSourceKind : uint8 { CHAR_POSE_SOURCE_PLAY = 1, CHAR_POSE_SOURCE_PREVIEW, CHAR_POSE_SOURCE_FILE };

/// @contract enum Zenith_CharMaterialDomain
/// Ownership: Plain enumeration; CHAR's mapping of GEOM's material "domain" strings (GEOM/Materials.md "Material
///   document schema": "opaque", "alphaTest", "transparent", "decal"), filled by the asset query implementation.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an implementation that meets another domain string answers false from GetMaterialFacts.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CharMaterialDomain : uint8
{
	CHAR_MATERIAL_DOMAIN_OPAQUE = 1, CHAR_MATERIAL_DOMAIN_ALPHA_TEST, CHAR_MATERIAL_DOMAIN_TRANSPARENT, CHAR_MATERIAL_DOMAIN_DECAL,
};
```
