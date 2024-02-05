#include "bindgen.hpp"

void LinxcGenerateCHeaderForNamespace(FILE *fs, LinxcParser *parser, LinxcNamespace *currentNamespace)
{
    for (usize i = 0; i < currentNamespace->types.bucketsCount; i++)
    {
        if (currentNamespace->types.buckets[i].initialized)
        {
            for (usize j = 0; j < currentNamespace->types.buckets[j].entries.count; j++)
            {
                LinxcGenerateTypeCHeader(fs, parser, &currentNamespace->types.buckets[j].entries.ptr[j].value);
            }
        }
    }
    for (usize i = 0; i < currentNamespace->subNamespaces.bucketsCount; i++)
    {
        if (currentNamespace->subNamespaces.buckets[i].initialized)
        {
            for (usize j = 0; j < currentNamespace->subNamespaces.buckets[j].entries.count; j++)
            {
                LinxcGenerateCHeaderForNamespace(fs, parser, &currentNamespace->subNamespaces.buckets[j].entries.ptr[j].value);
            }
        }
    }
}
void LinxcGenerateTypeCHeader(FILE *fs, LinxcParser *parser, LinxcType *type)
{
    string cName = type->GetCName(GetDefaultAllocator());
    fprintf(fs, "typedef void * %s;\n\n", cName.buffer);

    for (usize i = 0; i < type->variables.count; i++)
    {
        LinxcVar *var = &type->variables.ptr[i];
        if (var->type.AsTypeReference().value.templateArgs.length == 0)
        {
            fprintf(fs, "externC ");
            parser->TranspileExpr(fs, &var->type, false, TemplateArgs(), TemplateSpecialization());
            fprintf(fs, " %s_Get%s(%s from);\n", cName.buffer, var->name.buffer, cName.buffer);

            fprintf(fs, "externC void ");
            fprintf(fs, "%s_Set%s(%s to, ", cName.buffer, var->name.buffer, cName.buffer);
            parser->TranspileExpr(fs, &var->type, false, TemplateArgs(), TemplateSpecialization());
            fprintf(fs, " input);\n");
        }
        fprintf(fs, "");
    }
    
    cName.deinit();
}
void LinxcGenerateCHeader(LinxcParser *parser, string outputPath)
{
    FILE *fs;
    if (fopen_s(&fs, outputPath.buffer, "w"))
    {
        fprintf(fs, "#pragma once\n");
        fprintf(fs, "#include \"Linxc.h\"\n\n");

        LinxcGenerateCHeaderForNamespace(fs, parser, &parser->globalNamespace);

        fflush(fs);
        fclose(fs);
    }
}