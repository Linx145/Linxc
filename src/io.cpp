#include <io.hpp>
#include <io.h>
#include <stdio.h>
#include <vector.linxc>
#include <string.hpp>

#if WINDOWS
#include <Windows.h>
#endif

bool FileExists(const char *path)
{
    return access(path, 0) == 0;
}

string io::ReadFile(const char *path)
{
    string result = string();
    result.buffer = NULL;
    result.length = 0;

    FILE *fs;
    if (fopen_s(&fs, path, "r") == 0)
    {
        fseek(fs, 0, SEEK_END);
        usize size = ftell(fs);
        fseek(fs, 0, SEEK_SET);

        char *buffer = (char*)malloc(size + 1);
        fread(buffer, sizeof(char), size, fs);

        result.length = size + 1;
        result.buffer = buffer;
        result.buffer[size] = '\0';

        fclose(fs);
    }
    return result;
}

collections::Array<string> io::GetFilesInDirectory(IAllocator *allocator, const char *dirPath)
{
    #if WINDOWS
    WIN32_FIND_DATA findFileResult;
    char sPath[1024];
    sprintf(sPath, "%s/*.*", dirPath);

    HANDLE handle = FindFirstFile(sPath, &findFileResult);
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
        if (!FindNextFile(handle, &findFileResult))
        {
            break;
        }
    }

    FindClose(handle);

    return results.ToOwnedArray();
    #endif
}