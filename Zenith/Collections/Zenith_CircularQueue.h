#pragma once

template<typename T, u_int uCapacity>
class Zenith_CircularQueue
{
public:

	Zenith_CircularQueue()
	{
		m_uCurrentSize = 0;
		m_uFront = 0;
	}

	bool Enqueue(const T& tAdd)
	{
		if (m_uCurrentSize == uCapacity) return false;
		m_atContents[(m_uFront + m_uCurrentSize++) % uCapacity] = tAdd;
		return true;
	}

	bool Dequeue(T& tOut)
	{
		if (m_uCurrentSize == 0) return false;

		tOut = m_atContents[m_uFront];
		m_uFront = (m_uFront + 1) % uCapacity;
		m_uCurrentSize--;

		return true;
	}

private:

	T m_atContents[uCapacity];
	u_int m_uCurrentSize = -1;
	u_int m_uFront = -1;
};