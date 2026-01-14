#pragma once

#include "Collections/Zenith_Vector.h"
#include <string>

class Zenith_Entity;
class Zenith_Blackboard;
class Zenith_DataStream;

/**
 * BTNodeStatus - Result of behavior tree node execution
 */
enum class BTNodeStatus
{
	SUCCESS,	// Node completed successfully
	FAILURE,	// Node failed
	RUNNING		// Node still executing, will continue next tick
};

/**
 * Zenith_BTNode - Abstract base class for all behavior tree nodes
 *
 * Behavior tree nodes form a hierarchical decision tree that is ticked each frame.
 * Each node returns SUCCESS, FAILURE, or RUNNING to indicate its status.
 */
class Zenith_BTNode
{
public:
	Zenith_BTNode() = default;
	virtual ~Zenith_BTNode() = default;

	/**
	 * Execute this node for one tick
	 * @param xAgent The entity executing this behavior tree
	 * @param xBlackboard Shared state storage for the tree
	 * @param fDt Delta time since last tick
	 * @return Status of this node after execution
	 */
	virtual BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt) = 0;

	/**
	 * Called when this node becomes active (transitions from inactive to running)
	 */
	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) {}

	/**
	 * Called when this node completes (SUCCESS or FAILURE)
	 */
	virtual void OnExit(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) {}

	/**
	 * Called when this node is interrupted by a higher-priority branch
	 */
	virtual void OnAbort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) {}

	/**
	 * Get the type name for serialization and debugging
	 */
	virtual const char* GetTypeName() const = 0;

	/**
	 * Serialization support
	 */
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream);

	// Node name for debugging/editor
	const std::string& GetNodeName() const { return m_strNodeName; }
	void SetNodeName(const std::string& strName) { m_strNodeName = strName; }

	// Get last execution status
	BTNodeStatus GetLastStatus() const { return m_eLastStatus; }

	// Ownership tracking - used to prevent double-delete when nodes are shared incorrectly
	bool HasParent() const { return m_pxParent != nullptr; }
	Zenith_BTNode* GetParent() const { return m_pxParent; }

protected:
	friend class Zenith_BTComposite;
	friend class Zenith_BTDecorator;

	void SetParent(Zenith_BTNode* pxParent) { m_pxParent = pxParent; }

	std::string m_strNodeName;
	BTNodeStatus m_eLastStatus = BTNodeStatus::FAILURE;
	Zenith_BTNode* m_pxParent = nullptr;  // Used for ownership tracking (not serialized)
};

/**
 * Zenith_BTComposite - Base class for nodes with multiple children
 *
 * Composites control the flow of execution through their children.
 * Examples: Sequence, Selector, Parallel
 */
class Zenith_BTComposite : public Zenith_BTNode
{
public:
	Zenith_BTComposite() = default;
	virtual ~Zenith_BTComposite();

	/**
	 * Add a child node (takes ownership)
	 */
	void AddChild(Zenith_BTNode* pxChild);

	/**
	 * Get child at index
	 */
	Zenith_BTNode* GetChild(uint32_t uIndex) const;

	/**
	 * Get number of children
	 */
	uint32_t GetChildCount() const { return m_axChildren.GetSize(); }

	/**
	 * Get all children
	 */
	const Zenith_Vector<Zenith_BTNode*>& GetChildren() const { return m_axChildren; }

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// Reset child index on enter
	virtual void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard) override;

protected:
	Zenith_Vector<Zenith_BTNode*> m_axChildren;
	uint32_t m_uCurrentChild = 0;
};

/**
 * Zenith_BTDecorator - Base class for nodes with a single child
 *
 * Decorators modify the behavior of their child node.
 * Examples: Inverter, Repeater, Cooldown
 */
class Zenith_BTDecorator : public Zenith_BTNode
{
public:
	Zenith_BTDecorator() = default;
	virtual ~Zenith_BTDecorator();

	/**
	 * Set the child node (takes ownership)
	 */
	void SetChild(Zenith_BTNode* pxChild);

	/**
	 * Get the child node
	 */
	Zenith_BTNode* GetChild() const { return m_pxChild; }

	// Serialization
	virtual void WriteToDataStream(Zenith_DataStream& xStream) const override;
	virtual void ReadFromDataStream(Zenith_DataStream& xStream) override;

protected:
	Zenith_BTNode* m_pxChild = nullptr;
};

/**
 * Zenith_BTLeaf - Base class for leaf nodes (no children)
 *
 * Leaf nodes perform actual actions or check conditions.
 * Examples: MoveTo, Attack, HasTarget, InRange
 */
class Zenith_BTLeaf : public Zenith_BTNode
{
public:
	Zenith_BTLeaf() = default;
	virtual ~Zenith_BTLeaf() = default;
};
