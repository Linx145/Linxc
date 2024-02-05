#pragma once
#include "parser.hpp"
#include "stdio.h"

void LinxcGenerateCHeaderForNamespace(FILE *fs, LinxcParser *parser, LinxcNamespace *currentNamespace);

/// @brief 
/// Generates a C header from struct information. Works on structs written in the Linxc dialect
/// The type will always result in a typedef'd void pointer, with functions to retrieve the values of the pointer.
/// Bindings will not be generated for templated types or functions
void LinxcGenerateTypeCHeader(FILE *fs, LinxcParser *parser, LinxcType *type);

void LinxcGenerateCHeader(LinxcParser *parser, string outputPath);