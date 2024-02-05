#pragma once

#include "Linxc.h"
#include "string.hpp"
#include "array.hpp"
#include "option.hpp"
#include <math.h>

namespace path
{
    string SwapExtension(IAllocator* allocator, string path, const char* newExtension)
    {
        usize dotPosition = path.length + 1; //impossible number
        usize finalDirPosition = path.length + 1;
        usize i = 0;

        while (path.buffer[i] != '\0')
        {
            if (path.buffer[i] == '.')
            {
                dotPosition = i;
            }
            else if (path.buffer[i] == '/' || path.buffer[i] == '\\')
            {
                finalDirPosition = i;
            }
            i++;
        }

        if (dotPosition != path.length + 1 && (finalDirPosition == path.length + 1 || finalDirPosition < dotPosition))
        {
            string result = string(allocator, path.buffer, dotPosition);
            result.Append(newExtension);
            return result;
        }
        else
        {
            string result = string(allocator, path.buffer);
            result.Append(newExtension);
            return result;
        }
    }
    //Including the '.'
    string GetExtension(IAllocator *allocator, string path)
    {
        usize dotPosition = path.length + 1; //impossible number
        usize finalDirPosition = path.length + 1;
        usize i = 0;

        while (path.buffer[i] != '\0')
        {
            if (path.buffer[i] == '.')
            {
                dotPosition = i;
            }
            else if (path.buffer[i] == '/' || path.buffer[i] == '\\')
            {
                finalDirPosition = i;
            }
            i++;
        }

        if (dotPosition != path.length + 1 && (finalDirPosition == path.length + 1 || finalDirPosition < dotPosition))
        {
            return string(allocator, path.buffer + dotPosition, path.length - dotPosition);
        }
        else
        {
            return string();
        }
    }
    string GetDirectory(IAllocator *allocator, string path)
    {
        option<usize> lastWindows = FindLast(path.buffer, '\\');
        option<usize> lastNormalFileSystem = FindLast(path.buffer, '/');

        usize actualLastIndex = 0;
        if (lastWindows.present && lastNormalFileSystem.present)
        {
            actualLastIndex = fmax(lastWindows.value, lastNormalFileSystem.value);
        }
        else if (lastWindows.present)
        {
            actualLastIndex = lastWindows.value;
        }
        else if (lastNormalFileSystem.present)
        {
            actualLastIndex = lastNormalFileSystem.value;
        }
        else
        {
            return string();
        }

        return string(allocator, path.buffer, actualLastIndex);
    }
}