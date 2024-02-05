#pragma once

#include "Linxc.h"

inline u32 GetHash(u8* buffer, usize len)
{
    u32 hash = 7;
    for (usize i = 0; i < len; i++)
    {
        hash = hash * 31 + buffer[i];
    }
    return hash;
}

inline u32 GetHash(const char* ptr)
{
    u32 hash = 7;
    usize i = 0;
    while (true)
    {
        if (ptr[i] == '\0')
        {
            break;
        }
        else
        {
            hash = hash * 31 + ptr[i];
        }
        i += 1;
    }
    return hash;
}

template<typename T>
u32 PointerHash(T* ptr)
{
    return (usize)ptr;
}

template<typename T>
bool PointerEql(T* A, T* B)
{
    return A == B;
}

inline u32 CombineHash(u32 left, u32 right)
{
    return left ^ (right + 0x9e3779b9 + (left << 6) + (left >> 2));
}

inline u32 i32Hash(i32 value)
{
    return (u32)value;
}
inline bool i32Eql(i32 A, i32 B)
{
    return A == B;
}