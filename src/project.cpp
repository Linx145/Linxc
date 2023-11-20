#include <project.hpp>
#include <io.hpp>
#include <stdio.h>

LinxcProject::LinxcProject(IAllocator *allocator)
{
    this->allocator = allocator;
    this->includedFiles = collections::vector<string>(allocator);
    this->includeDirectories = collections::vector<string>(allocator);
    this->parser = LinxcParser();
}
void LinxcProject::AddAllFilesFromDirectory(string directoryPath)
{
    collections::Array<string> result = io::GetFilesInDirectory(this->allocator, directoryPath.buffer);

    for (usize i = 0; i < result.length; i++)
    {
        result.data[i].Prepend("/");
        result.data[i].Prepend(directoryPath.buffer);
        this->includedFiles.Add(result.data[i]);
    }
    result.deinit();
}
i32 LinxcProject::Build()
{
    for (usize i = 0; i < this->includedFiles.count; i++)
    {
        string fileContents = io::ReadFile(this->includedFiles.Get(i)->buffer);

        if (fileContents.buffer == NULL)
        {
            continue;
        }

        collections::vector<ERR_MSG> errors = this->parser.ParseFileH(this->includeDirectories.ToRefArray(), *this->includedFiles.Get(i), fileContents);

        for (usize i = 0; i < errors.count; i++)
        {
            printf("%s\n", errors.Get(i)->buffer);
        }
        errors.deinit();

        fileContents.deinit();
    }
    return 0;
}
void LinxcProject::deinit()
{
    for (usize i = 0; i < this->includedFiles.count; i++)
    {
        this->includedFiles.Get(i)->deinit();
    }
    this->includedFiles.deinit();

    for (usize i = 0; i < this->includeDirectories.count; i++)
    {
        this->includeDirectories.Get(i)->deinit();
    }
    this->includeDirectories.deinit();
}