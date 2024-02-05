#pragma once

#include "Linxc.h"
#include "string.hpp"
#include "array.hpp"
#include <stdio.h>
#include "vector.hpp"
#include "allocators.hpp"

#include <sys/stat.h>   // For stat().

#if WINDOWS
#include <io.h>
#include <Windows.h>
#define access _access
#endif
#if POSIX
#include <unistd.h>
#endif

namespace io
{
    inline string ReadFile(IAllocator* allocator, const char* path)
    {
        string result = string(allocator);

        FILE *fs = fopen(path, "r");
        if (fs != NULL)
        {
            usize size = 0;
            while (fgetc(fs) != EOF)
            {
                size += 1;
            }
            //fseek(fs, 0, SEEK_END);
            //usize size = ftell(fs);
            //fseek(fs, 0, SEEK_SET);
            fseek(fs, 0, SEEK_SET);

            char* buffer = (char*)allocator->Allocate(size + 1);
            if (buffer != NULL)
            {
                fread(buffer, sizeof(char), size, fs);
                
                buffer[size] = '\0';
                result.buffer = buffer;
                result.length = size + 1;
            }

            fclose(fs);
        }
        return result;
    }

    inline bool FileExists(const char *path)
    {
        return access(path, 0) == 0;
    }

    inline bool DirectoryExists(const char* path)
    {
        if (access(path, 0) == 0) 
        {
            struct stat status;

            stat(path, &status);

            return (status.st_mode & S_IFDIR) != 0;
        }
        return false;
    }

    inline bool NewDirectory(const char* path)
    {
        if (!io::DirectoryExists(path))
        {
        #if WINDOWS
            return CreateDirectoryA(path, NULL);
        #else
            return mkdir(path, 0755) == 0;
        #endif

        }
        return false;
    }

    inline FILE* CreateDirectoriesAndFile(const char* path)
    {
        IAllocator defaultAllocator = GetCAllocator();
        collections::Array<string> paths = SplitString(&defaultAllocator, path, '/');
        if (paths.length <= 1) //C:/ is not a valid file
        {
            return NULL;
        }
        FILE* file = NULL;
        string currentPath = paths.data[0].Clone(&defaultAllocator);
        for (usize i = 0; i < paths.length; i++)
        {
            if (i > 0)
            {
                currentPath.Append("/");
                currentPath.Append(paths.data[i].buffer);
            }
            if (i < paths.length - 1)
            {
                if (!io::DirectoryExists(currentPath.buffer))
                {
                    io::NewDirectory(currentPath.buffer);
                }
            }
            else
            {
                //create file
                file = fopen(currentPath.buffer, "w");
                break;
            }
        }

        currentPath.deinit();
        for (usize i = 0; i < paths.length; i++)
        {
            paths.data[i].deinit();
        }
        paths.deinit();
        return file;
    }

    inline collections::Array<string> GetFilesInDirectory(IAllocator *allocator, const char *dirPath)
    {
        IAllocator defaultAllocator = GetCAllocator();

#if WINDOWS
        WIN32_FIND_DATAA findFileResult;
        char sPath[1024];
        sprintf(sPath, "%s/*.*", dirPath);

        HANDLE handle = FindFirstFileA(sPath, &findFileResult);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return collections::Array<string>();
        }

        collections::vector<string> results = collections::vector<string>(&defaultAllocator);
        while (true)
        {
            if (strcmp(findFileResult.cFileName, ".") != 0 && strcmp(findFileResult.cFileName, "..") != 0)
            {
                //printf("%s\n", &findFileResult.cFileName[0]);
                string replaced = ReplaceChar(allocator, &findFileResult.cFileName[0], '\\', '/');

                string fullPath = string(&defaultAllocator, dirPath);
                fullPath.Append("/");
                fullPath.Append(replaced.buffer);
                if (!io::DirectoryExists(fullPath.buffer))
                {
                    results.Add(replaced);
                }
                else replaced.deinit();
                fullPath.deinit();
            }
            if (!FindNextFileA(handle, &findFileResult))
            {
                break;
            }
        }

        FindClose(handle);

        return results.ToOwnedArrayWith(allocator);
#endif
    }
}