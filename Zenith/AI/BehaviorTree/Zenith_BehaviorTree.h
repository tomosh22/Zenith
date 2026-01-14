#pragma once

#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

class Zenith_Entity;
class Zenith_DataStream;

/**
 * Zenith_BehaviorTree - Manages a behavior tree instance
 *
 * Owns the root node and handles tree-level execution state.
 * Each AI agent has its own blackboard but can share the tree structure.
 */
class Zenith_BehaviorTree
{
public:
	Zenith_BehaviorTree() = default;
	~Zenith_BehaviorTree();

	// Prevent copying (owns nodes)
	Zenith_BehaviorTree(const Zenith_BehaviorTree&) = delete;
	Zenith_BehaviorTree& operator=(const Zenith_BehaviorTree&) = delete;

	// Allow moving
	Zenith_BehaviorTree(Zenith_BehaviorTree&& xOther) noexcept;
	Zenith_BehaviorTree& operator=(Zenith_BehaviorTree&& xOther) noexcept;

	// ========== Root Node ==========

	/**
	 * Set the root node (takes ownership)
	 */
	void SetRootNode(Zenith_BTNode* pxRoot);

	/**
	 * Get the root node
	 */
	Zenith_BTNode* GetRootNode() const { return m_pxRootNode; }

	// ========== Execution ==========

	/**
	 * Execute one tick of the behavior tree
	 * @param xAgent Entity running this tree
	 * @param xBlackboard Agent's blackboard (state storage)
	 * @param fDt Delta time since last tick
	 * @return Status of the tree after this tick
	 */
	BTNodeStatus Tick(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard, float fDt);

	/**
	 * Reset tree state (for restarting execution)
	 */
	void Reset(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard);

	/**
	 * Abort the currently running tree
	 */
	void Abort(Zenith_Entity& xAgent, Zenith_Blackboard& xBlackboard);

	// ========== Status ==========

	/**
	 * Get the status from the last tick
	 */
	BTNodeStatus GetLastStatus() const { return m_eLastStatus; }

	/**
	 * Get the currently executing node name (for debug)
	 */
	const char* GetCurrentNodeName() const { return m_szCurrentNodeName; }

	// ========== Tree Info ==========

	void SetName(const std::string& str) { m_strName = str; }
	const std::string& GetName() const { return m_strName; }

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// ========== Factory ==========

	/**
	 * Load a behavior tree from a .zbtree asset file
	 */
	static Zenith_BehaviorTree* LoadFromFile(const std::string& strPath);

	/**
	 * Save a behavior tree to a .zbtree asset file
	 */
	static bool SaveToFile(const Zenith_BehaviorTree& xTree, const std::string& strPath);

private:
	Zenith_BTNode* m_pxRootNode = nullptr;
	BTNodeStatus m_eLastStatus = BTNodeStatus::FAILURE;
	std::string m_strName;
	const char* m_szCurrentNodeName = "";
	bool m_bFirstTick = true;
};
