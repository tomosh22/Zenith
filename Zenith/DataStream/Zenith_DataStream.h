#pragma once
#pragma once

class Zenith_DataStream
{
public:
	Zenith_DataStream() = delete;
	Zenith_DataStream(uint64_t ulSize);
	Zenith_DataStream(void* pData, uint64_t ulSize);
	~Zenith_DataStream();

	void SetCursor(uint64_t ulCursor);

	template<typename T>
	inline void operator<<(T u)
	{
		static_assert(std::is_trivially_copyable<T>(), "Not trivially copyable");
		memcpy(static_cast<uint8_t*>(m_pData) + m_ulCursor, &u, sizeof(T));
		m_ulCursor += sizeof(T);
		Zenith_Assert(m_ulCursor < m_ulDataSize, "DataStream ran out of space");
	}
	template<typename T>
	inline void operator>>(T& u)
	{
		static_assert(std::is_trivially_copyable<T>(), "Not trivially copyable");
		memcpy(&u, static_cast<uint8_t*>(m_pData) + m_ulCursor, sizeof(T));
		m_ulCursor += sizeof(T);
		Zenith_Assert(m_ulCursor < m_ulDataSize, "Ran past end of DataStream");
	}
private:
	void* m_pData = nullptr;
	uint64_t m_ulDataSize = 0;
	uint64_t m_ulCursor = 0;
	bool m_bOwnsData = false;
};