#include <io.hpp>
#include <stdio.h>

string io::ReadFile(const char *source)
{
    string result;
    result.buffer = NULL;
    result.length = 0;

    FILE *fs;
    if (fopen_s(&fs, source, "r") == 0)
    {
        fseek(fs, 0, SEEK_END);
        usize size = ftell(fs);
        fseek(fs, 0, SEEK_SET);

        char *buffer = (char*)malloc(size);
        fread(buffer, sizeof(char), size, fs);

        result.length = size + 1;
        result.buffer = buffer;
        result.buffer[size] = '\0';

        fclose(fs);
    }
    return result;
};