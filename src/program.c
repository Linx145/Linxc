#include <lexer.h>
#include <stdio.h>

i32 main()
{
    char chars[] = "i32 main() { printf(\"Hello World!\"); }";

    LinxcTokenizer tokenizer = LinxcCreateTokenizer(chars, sizeof(chars) / sizeof(char));

    while (true)
    {
        LinxcToken next = LinxcTokenizerNext(&tokenizer);
        
    }

    printf("Test passed");
    return 0;
}