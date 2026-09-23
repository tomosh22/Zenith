#pragma once


#include "Collections/Zenith_Vector.h"

/**
 * Zenith_Less<T> - default ordering trait for Zenith_PriorityQueue.
 *
 * Mirrors std::less<T>: operator()(a, b) returns true when a ranks lower
 * than b under operator<. Specialise (or pass a custom Compare with the
 * same signature) for types with no operator<, or to invert the ordering.
 */
template<typename T>
struct Zenith_Less
{
	bool operator()(const T& xLHS, const T& xRHS) const
	{
		return xLHS < xRHS;
	}
};

/**
 * Zenith_PriorityQueue<T, Compare> - binary heap over Zenith_Vector
 *
 * Ordering convention matches std::priority_queue exactly: Compare(a, b)
 * returning true means "a has LOWER priority than b", and Top()/Pop()
 * return the HIGHEST-priority element under Compare. The default
 * Zenith_Less<T> (operator<) therefore makes this a MAX-heap, same as
 * std::priority_queue<T>. For a MIN-heap — the common open-list shape used
 * by A* and Dijkstra, where the lowest cost must pop first — pass a comparator
 * built on operator> (see Zenith_Pathfinding.cpp's AStarNodeCompare for the
 * pattern), exactly mirroring the old std::priority_queue<T, std::vector<T>,
 * std::greater<T>> idiom this container replaces.
 *
 * No exceptions: Pop()/Top() assert on an empty queue (matching every other
 * Collections container's no-exceptions convention — see Zenith_Vector's
 * GetFront()/GetBack(), which follow the identical assert-then-proceed
 * shape) rather than throwing. Growth is delegated entirely to the backing
 * Zenith_Vector<T>, so allocation goes through the engine allocator and
 * follows the same exponential growth policy as every other dynamic
 * container in this directory.
 *
 * No decrease-key: this is a plain binary heap with no external handle into
 * heap slots, so an entry already in the queue cannot have its priority
 * updated in place. Zenith_Pathfinding::FindPath's A* open list does not
 * need one — it tracks best-known g-costs in a side Zenith_HashMap and
 * treats a stale, since-superseded heap entry as a cheap no-op via the
 * existing closed-set check on pop, the same lazy-deletion pattern it used
 * with std::priority_queue before this migration. A decrease-key variant
 * (heap slot indices held in a side map, sifted on update) can be added
 * later if a consumer actually needs it; nothing here forecloses that.
 *
 * THREAD SAFETY: not thread-safe. External synchronisation is required for
 * multithreaded use, matching Zenith_CircularQueue.
 */
template<typename T, typename Compare = Zenith_Less<T>>
class Zenith_PriorityQueue
{
public:
	Zenith_PriorityQueue() = default;

	explicit Zenith_PriorityQueue(u_int uInitialCapacity)
	: m_xHeap(uInitialCapacity)
	{}

	void Push(const T& xValue)
	{
		m_xHeap.PushBack(xValue);
		SiftUp(m_xHeap.GetSize() - 1);
	}

	void Push(T&& xValue)
	{
		m_xHeap.PushBack(std::move(xValue));
		SiftUp(m_xHeap.GetSize() - 1);
	}

	template<typename... Args>
	void Emplace(Args&&... args)
	{
		m_xHeap.EmplaceBack(std::forward<Args>(args)...);
		SiftUp(m_xHeap.GetSize() - 1);
	}

	// Remove the highest-priority element. Asserts on an empty queue and,
	// in release builds where Zenith_Assert is a no-op, refuses gracefully
	// rather than underflowing GetSize() — callers must check Empty() first.
	void Pop()
	{
		Zenith_Assert(!Empty(), "Zenith_PriorityQueue::Pop: queue is empty");
		if (Empty()) return;

		const u_int uLast = m_xHeap.GetSize() - 1;
		if (uLast != 0)
		{
			std::swap(m_xHeap.Get(0), m_xHeap.Get(uLast));
		}
		m_xHeap.PopBack();
		if (!Empty()) SiftDown(0);
	}

	// Delegates to Zenith_Vector::GetFront() (index 0, one assert) rather than
	// Get(0) (a second, redundant bounds assert on top of this one) — same
	// single-assert-then-proceed shape as GetFront()/GetBack() themselves.
	T& Top()
	{
		Zenith_Assert(!Empty(), "Zenith_PriorityQueue::Top: queue is empty");
		return m_xHeap.GetFront();
	}

	const T& Top() const
	{
		Zenith_Assert(!Empty(), "Zenith_PriorityQueue::Top: queue is empty");
		return m_xHeap.GetFront();
	}

	bool Empty() const { return m_xHeap.GetSize() == 0; }
	u_int GetSize() const { return m_xHeap.GetSize(); }
	u_int GetCapacity() const { return m_xHeap.GetCapacity(); }

	void Reserve(u_int uCapacity) { m_xHeap.Reserve(uCapacity); }
	void Clear() { m_xHeap.Clear(); }

private:
	void SiftUp(u_int uIndex)
	{
		Compare xCompare;
		while (uIndex > 0)
		{
			const u_int uParent = (uIndex - 1) / 2;
			// Stop once the parent already outranks (or ties) the child;
			// otherwise swap up and keep going.
			if (!xCompare(m_xHeap.Get(uParent), m_xHeap.Get(uIndex))) break;
			std::swap(m_xHeap.Get(uParent), m_xHeap.Get(uIndex));
			uIndex = uParent;
		}
	}

	void SiftDown(u_int uIndex)
	{
		const u_int uSize = m_xHeap.GetSize();
		Compare xCompare;
		for (;;)
		{
			const u_int uLeft = uIndex * 2 + 1;
			const u_int uRight = uIndex * 2 + 2;
			u_int uHighest = uIndex;

			if (uLeft < uSize && xCompare(m_xHeap.Get(uHighest), m_xHeap.Get(uLeft))) uHighest = uLeft;
			if (uRight < uSize && xCompare(m_xHeap.Get(uHighest), m_xHeap.Get(uRight))) uHighest = uRight;

			if (uHighest == uIndex) break;
			std::swap(m_xHeap.Get(uIndex), m_xHeap.Get(uHighest));
			uIndex = uHighest;
		}
	}

	Zenith_Vector<T> m_xHeap;
};
