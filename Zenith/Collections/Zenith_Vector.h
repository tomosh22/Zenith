#pragma once
#include "DataStream/Zenith_DataStream.h"

template<typename T>
class Zenith_Vector
{
public:
    Zenith_Vector()
        : Zenith_Vector(uDEFAULT_INITIAL_COUNT) {}

    explicit Zenith_Vector(size_t uCapacity)
        : m_pxData(static_cast<T*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(T)))),
        m_uSize(0),
        m_uCapacity(uCapacity) {}

    Zenith_Vector(const Zenith_Vector& other)
        : m_pxData(nullptr), m_uSize(0), m_uCapacity(0)
    {
        CopyFromOther(other);
    }

    Zenith_Vector(Zenith_Vector&& other) noexcept
        : m_pxData(other.m_pxData),
        m_uSize(other.m_uSize),
        m_uCapacity(other.m_uCapacity)
    {
        other.m_pxData = nullptr;
        other.m_uSize = 0;
        other.m_uCapacity = 0;
    }

    Zenith_Vector& operator=(const Zenith_Vector& other)
    {
        if (this != &other)
        {
            Clear();
            Zenith_MemoryManagement::Deallocate(m_pxData);
            CopyFromOther(other);
        }
        return *this;
    }

    Zenith_Vector& operator=(Zenith_Vector&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            Zenith_MemoryManagement::Deallocate(m_pxData);

            m_pxData = other.m_pxData;
            m_uSize = other.m_uSize;
            m_uCapacity = other.m_uCapacity;

            other.m_pxData = nullptr;
            other.m_uSize = 0;
            other.m_uCapacity = 0;
        }
        return *this;
    }

    ~Zenith_Vector()
    {
        Clear();
        Zenith_MemoryManagement::Deallocate(m_pxData);
    }

    size_t GetSize() const { return m_uSize; }
    size_t GetCapacity() const { return m_uCapacity; }

    void PushBack(const T& value)
    {
        EnsureCapacity();
        #include "Memory/Zenith_MemoryManagement_Disabled.h"
        new (&m_pxData[m_uSize]) T(value);
        #include "Memory/Zenith_MemoryManagement_Enabled.h"
        m_uSize++;
    }

    void PushBack(T&& value)
    {
        EnsureCapacity();
        #include "Memory/Zenith_MemoryManagement_Disabled.h"
        new (&m_pxData[m_uSize]) T(std::move(value));
        #include "Memory/Zenith_MemoryManagement_Enabled.h"
        m_uSize++;
    }

    T& Get(size_t index) const
    {
        Zenith_Assert(index < m_uSize, "Index out of range in Get()");
        return m_pxData[index];
    }

    T& GetFront() const
    {
        Zenith_Assert(m_uSize > 0, "GetFront called on empty vector");
        return m_pxData[0];
    }

    T& GetBack() const
    {
        Zenith_Assert(m_uSize > 0, "GetBack called on empty vector");
        return m_pxData[m_uSize - 1];
    }

    T* GetDataPointer() const { return m_pxData; }

    void Remove(size_t index)
    {
        Zenith_Assert(index < m_uSize, "Index out of range in Remove()");
        m_pxData[index].~T();
        for (size_t i = index; i < m_uSize - 1; ++i)
        {
            #include "Memory/Zenith_MemoryManagement_Disabled.h"
            new (&m_pxData[i]) T(std::move(m_pxData[i + 1]));
            #include "Memory/Zenith_MemoryManagement_Enabled.h"
            m_pxData[i + 1].~T();
        }
        m_uSize--;
    }

    void Clear()
    {
        for (size_t i = 0; i < m_uSize; ++i)
        {
            m_pxData[i].~T();
        }
        m_uSize = 0;
    }

    void Reserve(size_t newCapacity)
    {
        if (newCapacity <= m_uCapacity)
            return;

        T* newData = static_cast<T*>(Zenith_MemoryManagement::Allocate(newCapacity * sizeof(T)));
        for (size_t i = 0; i < m_uSize; ++i)
        {
            #include "Memory/Zenith_MemoryManagement_Disabled.h"
            new (&newData[i]) T(std::move(m_pxData[i]));
            #include "Memory/Zenith_MemoryManagement_Enabled.h"
            m_pxData[i].~T();
        }

        Zenith_MemoryManagement::Deallocate(m_pxData);
        m_pxData = newData;
        m_uCapacity = newCapacity;
    }

    void ReadFromDataStream(Zenith_DataStream& stream)
    {
        size_t size;
        stream >> size;
        Clear();
        Reserve(size);
        for (size_t i = 0; i < size; ++i)
        {
            T value;
            stream >> value;
            PushBack(std::move(value));
        }
    }

    void WriteToDataStream(Zenith_DataStream& stream) const
    {
        stream << m_uSize;
        for (Iterator it(*this); !it.Done(); it.Next())
        {
            stream << it.GetData();
        }
    }

    class Iterator
    {
    public:
        explicit Iterator(const Zenith_Vector& vec)
            : m_vec(vec), m_index(0) {}

        void Next()
        {
            Zenith_Assert(m_index < m_vec.GetSize() + 1, "Iterator advanced past end");
            ++m_index;
        }

        bool Done() const { return m_index == m_vec.GetSize(); }

        T& GetData() { return m_vec.Get(m_index); }

    private:
        const Zenith_Vector<T>& m_vec;
        size_t m_index;
    };

private:
    static constexpr size_t uDEFAULT_INITIAL_COUNT = 8;

    void EnsureCapacity()
    {
        if (m_uSize >= m_uCapacity)
        {
            Resize();
        }
    }

    void Resize()
    {
        size_t newCapacity = (m_uCapacity > 0) ? m_uCapacity * 2 : uDEFAULT_INITIAL_COUNT;
        Reserve(newCapacity);
    }

    void CopyFromOther(const Zenith_Vector& other)
    {
        m_pxData = static_cast<T*>(Zenith_MemoryManagement::Allocate(other.m_uCapacity * sizeof(T)));
        m_uSize = other.m_uSize;
        m_uCapacity = other.m_uCapacity;

        for (size_t i = 0; i < m_uSize; ++i)
        {
            #include "Memory/Zenith_MemoryManagement_Disabled.h"
            new (&m_pxData[i]) T(other.m_pxData[i]);
            #include "Memory/Zenith_MemoryManagement_Enabled.h"
        }
    }

    T* m_pxData = nullptr;
    size_t m_uSize = 0;
    size_t m_uCapacity = 0;
};
