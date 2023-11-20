#include <ast.hpp>

LinxcNamespace::LinxcNamespace()
{
    this->name = string();
}
LinxcNamespace::LinxcNamespace(string name)
{
    this->name = name;
    this->functions = collections::hashmap<string, LinxcFunc>(&stringHash, &stringEql);
    this->subNamespaces = collections::hashmap<string, LinxcNamespace>(&stringHash, &stringEql);
    this->types = collections::hashmap<string, LinxcType>(&stringHash, &stringEql);
    this->variables = collections::hashmap<string, LinxcVar>(&stringHash, &stringEql);
}