#include <lexer.hpp>
#include <stdio.h>
#include <string.hpp>

i32 main()
{
    char chars[] = "#include <Linxc.h>\ni32 main() { printf(\"Hello World!\"); }";

    LinxcTokenizer tokenizer = LinxcCreateTokenizer(chars, sizeof(chars) / sizeof(char));

    while (true)
    {
        LinxcToken next = LinxcTokenizerNext(&tokenizer);
        if (next.ID == Linxc_Eof || next.ID == Linxc_Invalid)
        {
            if (next.ID == Linxc_Invalid)
            {
                printf("INVALID!\n");
            }
            break;
        }
        if (next.ID != Linxc_Nl)
        {
            string str = string(tokenizer.buffer, next.start, next.end - next.start);

            printf("%s\n", str.buffer);

            str.deinit();
        }
    }

    printf("Test passed\n");
    return 0;
}