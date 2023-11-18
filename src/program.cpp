#include <lexer.hpp>
#include <stdio.h>
#include <string.hpp>
#include <io.hpp>

i32 main()
{
    string input = io::ReadFile("C:/Users/Linus/source/repos/Linxc/Tests/HelloWorld.linxc");

    LinxcTokenizer tokenizer = LinxcCreateTokenizer(input.buffer, input.length);

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
    input.deinit();
    return 0;
}