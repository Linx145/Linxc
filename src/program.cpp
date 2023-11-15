#include <lexer.h>
#include <stdio.h>
#include <string.linxc>

i32 main()
{
    char chars[] = "i32 main() { printf(\"Hello World!\"); }";

    LinxcTokenizer tokenizer = LinxcCreateTokenizer(chars, sizeof(chars) / sizeof(char));

    while (true)
    {
        LinxcToken next = LinxcTokenizerNext(&tokenizer);


        if (next.ID != Linxc_Nl)
        {
            string str = string(tokenizer.buffer, next.start, next.end - next.start);

            printf("%s\n", str.buffer);

            str.deinit();
        }

        if (next.ID == Linxc_Eof || next.ID == Linxc_Invalid)
        {
            break;
        }
    }

    printf("Test passed");
    return 0;
}