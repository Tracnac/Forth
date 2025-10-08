#define FF_ENABLE_REPL
#include "forth_fast.h"

int main(void) {
    forth_t vm;
    init_forth(&vm);
    
    printf("Fast Forth VM\n");
    printf("================================\n");
    
    repl(&vm);
    
    return 0;
}
