# Behavior Tree System

## Overview

The behavior tree system provides a flexible framework for AI decision-making. Trees are composed of nodes that execute hierarchically, returning SUCCESS, FAILURE, or RUNNING status.

## Node Hierarchy

```
Zenith_BTNode (abstract base)
├── Zenith_BTComposite (multiple children)
│   ├── Zenith_BTSequence
│   ├── Zenith_BTSelector
│   └── Zenith_BTParallel
├── Zenith_BTDecorator (single child)
│   ├── Zenith_BTInverter
│   ├── Zenith_BTSucceeder
│   ├── Zenith_BTRepeater
│   ├── Zenith_BTCooldown
│   ├── Zenith_BTConditionalLoop
│   └── Zenith_BTTimeLimit
└── Zenith_BTLeaf (no children)
    ├── Actions (Zenith_BTAction_*)
    └── Conditions (Zenith_BTCondition_*)
```

## Execution Model

### Status Values

| Status | Meaning |
|--------|---------|
| `SUCCESS` | Node completed successfully |
| `FAILURE` | Node failed to complete |
| `RUNNING` | Node still executing, resume next tick |

### Tick Cycle

1. Tree ticks at configurable rate (default 10 Hz)
2. `Execute()` called on root, propagates down
3. Nodes return status to parent
4. RUNNING nodes remembered, resumed next tick
5. OnEnter/OnExit called at state transitions

### Lifecycle Methods

- `OnEnter()`: Called when node starts executing
- `Execute()`: Main logic, returns status
- `OnExit()`: Called when node completes (success or failure)
- `OnAbort()`: Called when node interrupted (parallel abort)

## Composite Nodes

### Zenith_BTSequence

Runs children in order until one fails:
- Returns SUCCESS if all children succeed
- Returns FAILURE on first child failure
- Returns RUNNING if child is running (resumes there)

**Use for**: "Do A, then B, then C"

### Zenith_BTSelector

Tries children until one succeeds:
- Returns SUCCESS on first child success
- Returns FAILURE if all children fail
- Returns RUNNING if child is running

**Use for**: "Try A, else try B, else try C"

### Zenith_BTParallel

Runs all children simultaneously:
- `REQUIRE_ONE`: Success when any child succeeds
- `REQUIRE_ALL`: Success when all children succeed
- Aborts running children when policy met

**Use for**: "Do A and B at the same time"

## Decorator Nodes

### Zenith_BTInverter
Inverts child result (SUCCESS↔FAILURE), RUNNING unchanged

### Zenith_BTSucceeder
Always returns SUCCESS regardless of child result

### Zenith_BTRepeater
Repeats child N times (or infinite with N=0)

### Zenith_BTCooldown
After child succeeds, blocks execution for duration

### Zenith_BTConditionalLoop
Repeats child while condition is true

### Zenith_BTTimeLimit
Aborts child if it runs longer than duration

## Built-in Actions

| Action | Description | Blackboard Keys |
|--------|-------------|-----------------|
| `Wait` | Waits for duration | - |
| `MoveTo` | Navigates to position | "TargetPosition" |
| `MoveToEntity` | Navigates to entity | "TargetEntity" |
| `SetBlackboardBool` | Sets bool value | configurable |
| `SetBlackboardFloat` | Sets float value | configurable |
| `Log` | Logs message | - |
| `FindPrimaryTarget` | Sets target from perception | "TargetEntity" |

## Built-in Conditions

| Condition | Description | Parameters |
|-----------|-------------|------------|
| `HasTarget` | Checks if target entity valid | m_strTargetKey |
| `InRange` | Checks distance to target | m_fRange, m_strTargetKey |
| `CanSeeTarget` | Checks perception awareness | m_strTargetKey, m_fMinAwareness |
| `BlackboardBool` | Checks bool value | m_strKey, m_bExpectedValue |
| `BlackboardCompare` | Compares float value | m_strKey, m_eComparison, m_fValue |
| `HasAwareness` | Checks if any targets perceived | m_fMinAwareness |
| `Random` | Random probability | m_fProbability |

## Blackboard

Type-safe key-value store for behavior tree state:

### Supported Types
- `float`: `SetFloat()` / `GetFloat()`
- `int32_t`: `SetInt()` / `GetInt()`
- `bool`: `SetBool()` / `GetBool()`
- `Vector3`: `SetVector3()` / `GetVector3()`
- `EntityID`: `SetEntityID()` / `GetEntityID()`

### Usage

```cpp
Zenith_Blackboard& xBB = xAIComponent.GetBlackboard();

// Write values
xBB.SetFloat("Health", 100.0f);
xBB.SetEntityID("Target", xTargetID);

// Read values (with defaults)
float fHealth = xBB.GetFloat("Health", 0.0f);
bool bAlerted = xBB.GetBool("IsAlerted", false);

// Check and remove
if (xBB.HasKey("Target"))
{
    xBB.RemoveKey("Target");
}
```

## Creating Custom Nodes

### Custom Action

```cpp
class Zenith_BTAction_MyAction : public Zenith_BTLeaf
{
public:
    BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt) override
    {
        // Your logic here
        return BTNodeStatus::SUCCESS;
    }

    const char* GetTypeName() const override { return "MyAction"; }
};
```

### Custom Condition

```cpp
class Zenith_BTCondition_MyCondition : public Zenith_BTLeaf
{
public:
    BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt) override
    {
        if (CheckCondition())
            return BTNodeStatus::SUCCESS;
        return BTNodeStatus::FAILURE;
    }

    const char* GetTypeName() const override { return "MyCondition"; }
};
```

## Tree Construction Example

```cpp
// Build a patrol/attack behavior
auto* pxRoot = new Zenith_BTSelector();

// Attack branch
auto* pxAttackSeq = new Zenith_BTSequence();
pxAttackSeq->AddChild(new Zenith_BTCondition_HasTarget("Target"));
pxAttackSeq->AddChild(new Zenith_BTAction_MoveToEntity("Target", 2.0f));
// pxAttackSeq->AddChild(new Zenith_BTAction_Attack("Target"));

// Patrol branch
auto* pxPatrolSeq = new Zenith_BTSequence();
pxPatrolSeq->AddChild(new Zenith_BTAction_FindPrimaryTarget());
pxPatrolSeq->AddChild(new Zenith_BTAction_MoveTo("PatrolPosition"));
pxPatrolSeq->AddChild(new Zenith_BTAction_Wait(2.0f));

pxRoot->AddChild(pxAttackSeq);
pxRoot->AddChild(pxPatrolSeq);

// Create tree
auto* pxTree = new Zenith_BehaviorTree(pxRoot);
xAIComponent.SetBehaviorTree(pxTree);
```

## Debugging

### Debug Variables

- `s_bDrawCurrentNode`: Shows current node name above agent
- `s_bDrawBlackboardValues`: Shows key blackboard values

### Logging

All nodes log to LOG_CATEGORY_AI. Enable verbose logging to see:
- Node execution (OnEnter/OnExit/Execute)
- Status changes
- Blackboard modifications

## Performance Notes

- Trees tick at 10 Hz by default, not every frame
- Avoid expensive operations in Execute()
- Use Cooldown decorator to prevent rapid re-evaluation
- Complex trees can be split into subtrees
