#include <io.hpp>
#include <io.h>
#include <stdio.h>
#include <vector.linxc>
#include <string.hpp>
#include <allocators.hpp>

#if WINDOWS
#include <Windows.h>
#define access _access
#endif
#if POSIX
#include <unistd.h>
#endif

bool io::FileExists(const char *path)
{
    return access(path, 0) == 0;
}

string io::ReadFile(const char *path)
{
    string result = string();

    FILE *fs;
    if (fopen_s(&fs, path, "r") == 0)
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

        char *buffer = (char*)malloc(size + 1);
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

collections::Array<string> io::GetFilesInDirectory(IAllocator *allocator, const char *dirPath)
{
    #if WINDOWS
    WIN32_FIND_DATAA findFileResult;
    char sPath[1024];
    sprintf(sPath, "%s/*.*", dirPath);

    HANDLE handle = FindFirstFileA(sPath, &findFileResult);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return collections::Array<string>();
    }

    collections::vector<string> results = collections::vector<string>(allocator);
    while (true)
    {
        if (strcmp(findFileResult.cFileName, ".") != 0 && strcmp(findFileResult.cFileName, "..") != 0)
        {
            //printf("%s\n", &findFileResult.cFileName[0]);
            results.Add(string(allocator, &findFileResult.cFileName[0]));
        }
        if (!FindNextFileA(handle, &findFileResult))
        {
            break;
        }
    }

    FindClose(handle);

    return results.ToOwnedArray();
    #endif
}