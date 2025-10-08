// Fast Forth VM - Switch dispatch with inline primitives
// Strategy: Bytecode + switch dispatch, optimized for modern CPUs AND fantasy 8-bit CPUs
// No computed goto, no function pointers, just clean fast code
#ifndef FORTH_FAST_H
#define FORTH_FAST_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Configuration
#ifndef FF_STACK_DEPTH
#define FF_STACK_DEPTH 128
#endif
#ifndef FF_RET_DEPTH
#define FF_RET_DEPTH 64
#endif
#ifndef FF_DICT_SIZE
#define FF_DICT_SIZE 4096
#endif
#ifndef FF_MAX_WORDS
#define FF_MAX_WORDS 128
#endif
#ifndef FF_NAME_MAX
#define FF_NAME_MAX 15
#endif

// Bytecode opcodes - small numbers, great for 8-bit CPUs
typedef enum {
    OP_EXIT = 0,    // Return from word
    OP_LIT,         // Push next cell as literal
    OP_CALL,        // Call word (next cell = address)
    OP_ADD,         // +
    OP_SUB,         // -
    OP_MUL,         // *
    OP_DIV,         // /
    OP_DUP,         // DUP
    OP_DROP,        // DROP
    OP_SWAP,        // SWAP
    OP_OVER,        // OVER
    OP_DOT,         // .
    // Bitwise ops
    OP_AND,         // AND
    OP_OR,          // OR
    OP_XOR,         // XOR
    OP_NOT,         // NOT
    // Comparisons
    OP_LT,          // Less than
    OP_GT,          // Greater than
    OP_EQ,          // Equal
    OP_LE,          // Less than or equal <=
    OP_GE,          // Greater than or equal >=
    OP_NE,          // Not equal <>
    // Branches
    OP_BRANCH,      // Unconditional branch
    OP_BRANCH_IF_ZERO, // Branch if top of stack is zero
    // Loops
    OP_DO,          // (limit index -- )
    OP_LOOP,        // Loop
    OP_I,           // Push current loop index
    // Memory
    OP_LOAD,        // Load cell from address
    OP_STORE,       // Store cell to address
    OP_LOAD_BYTE,   // Load byte from address
    OP_STORE_BYTE,  // Store byte to address
    // Stack ops extended
    OP_ROT,         // ROT ( a b c -- b c a )
    OP_2DUP,        // 2DUP ( a b -- a b a b )
    OP_2DROP,       // 2DROP ( a b -- )
    OP_NIP,         // NIP ( a b -- b )
    OP_TUCK,        // TUCK ( a b -- b a b )
    // Return stack
    OP_TO_R,        // >R ( n -- ) R: ( -- n )
    OP_R_FROM,      // R> ( -- n ) R: ( n -- )
    OP_R_FETCH,     // R@ ( -- n ) R: ( n -- n )
    // Arithmetic extended
    OP_MOD,         // MOD ( a b -- remainder )
    OP_NEGATE,      // NEGATE ( n -- -n )
    OP_ABS,         // ABS ( n -- |n| )
    OP_MIN,         // MIN ( a b -- min )
    OP_MAX_OP,      // MAX ( a b -- max )
    OP_DIVMOD,      // /MOD ( a b -- rem quot )
    OP_1PLUS,       // 1+ ( n -- n+1 )
    OP_1MINUS,      // 1- ( n -- n-1 )
    // Comparisons extended
    OP_ZERO_EQ,     // 0= ( n -- flag )
    OP_ZERO_LT,     // 0< ( n -- flag )
    OP_ZERO_NE,     // 0<> ( n -- flag )
    // Stack extended
    OP_QDUP,        // ?DUP ( n -- n n | 0 )
    // Memory extended
    OP_PLUSSTORE,   // +! ( n addr -- )
    OP_ALLOT,       // ALLOT ( n -- ) allocate n bytes
    // I/O
    OP_EMIT,        // EMIT ( c -- )
    OP_KEY,         // KEY ( -- c )
    OP_CR,          // CR ( -- )
    OP_TYPE,        // TYPE ( addr len -- ) print string
    // Memory info
    OP_HERE,        // HERE ( -- addr )
    // Debug/Introspection
    OP_DOT_S,       // .S ( -- ) show stack
    OP_DEPTH,       // DEPTH ( -- n )
    OP_CLEAR,       // CLEAR ( ... -- ) clear stack
    OP_WORDS,       // WORDS ( -- ) list all words
    OP_SEE,         // SEE ( -- ) decompile word (parsed)
    OP_MAX          // Marker
} opcode_t;

typedef int32_t cell_t;
typedef uint16_t addr_t;

// I/O callbacks for flexibility (can be overridden for embedded systems)
typedef struct {
    int (*getchar_fn)(void);
    void (*putchar_fn)(int c);
    void (*flush_fn)(void);
    FILE* (*fopen_fn)(const char* filename, const char* mode);
    int (*fclose_fn)(FILE* fp);
    char* (*fgets_fn)(char* buf, int size, FILE* fp);
    int (*fputs_fn)(const char* str, FILE* fp);
} forth_io_t;

// Default flush implementation
static void default_flush(void) {
    fflush(stdout);
}

// Default putchar wrapper (fixes type mismatch warning)
static void default_putchar(int c) {
    putchar(c);
}

typedef struct {
    char name[FF_NAME_MAX + 1];
    addr_t addr;    // Address in dictionary where code starts
    uint8_t flags;
} word_t;

typedef struct {
    // Data stack
    cell_t ds[FF_STACK_DEPTH];
    int sp;
    
    // Return stack (mixed: addresses for calls, cells for loop counters)
    cell_t rs[FF_RET_DEPTH];
    int rp;
    
    // Dictionary (bytecode)
    uint8_t dict[FF_DICT_SIZE];
    addr_t here;    // Next free position
    
    // Word list
    word_t words[FF_MAX_WORDS];
    int word_count;
    
    // Compilation state
    int compiling;
    char token[FF_NAME_MAX + 1];
    
    // Compile-time stack for control flow (IF/THEN/ELSE, DO/LOOP)
    addr_t cstack[32];
    int csp;
    
    // I/O callbacks
    forth_io_t io;
    
    // Primitives start address (words defined before this are built-in)
    int builtin_count;
} forth_t;

// Stack operations - simple and fast
#define PUSH(vm, val) do { if ((vm)->sp < FF_STACK_DEPTH) (vm)->ds[(vm)->sp++] = (val); } while(0)
#define POP(vm) ((vm)->sp > 0 ? (vm)->ds[--(vm)->sp] : 0)
#define TOS(vm) ((vm)->ds[(vm)->sp - 1])
#define NOS(vm) ((vm)->ds[(vm)->sp - 2])

// Dictionary operations
static inline int emit_byte(forth_t* vm, uint8_t b) {
    if (vm->here >= FF_DICT_SIZE) return 0;
    vm->dict[vm->here++] = b;
    return 1;
}

static inline int emit_cell(forth_t* vm, cell_t c) {
    for (size_t i = 0; i < sizeof(cell_t); i++) {
        if (!emit_byte(vm, (c >> (i * 8)) & 0xFF)) return 0;
    }
    return 1;
}

static inline int emit_addr(forth_t* vm, addr_t a) {
    if (!emit_byte(vm, a & 0xFF)) return 0;
    if (!emit_byte(vm, (a >> 8) & 0xFF)) return 0;
    return 1;
}

static inline cell_t read_cell(forth_t* vm, addr_t* pc) {
    cell_t c = 0;
    for (size_t i = 0; i < sizeof(cell_t); i++) {
        c |= ((cell_t)vm->dict[(*pc)++]) << (i * 8);
    }
    return c;
}

static inline addr_t read_addr(forth_t* vm, addr_t* pc) {
    addr_t a = vm->dict[(*pc)++];
    a |= ((addr_t)vm->dict[(*pc)++]) << 8;
    return a;
}

// Patch an address at a given location (for forward branches)
static inline void patch_addr(forth_t* vm, addr_t location, addr_t target) {
    vm->dict[location] = target & 0xFF;
    vm->dict[location + 1] = (target >> 8) & 0xFF;
}

// Word lookup
static word_t* find_word(forth_t* vm, const char* name) {
    for (int i = vm->word_count - 1; i >= 0; i--) {
        if (strcmp(vm->words[i].name, name) == 0) {
            return &vm->words[i];
        }
    }
    return NULL;
}

// Add a word
static word_t* add_word(forth_t* vm, const char* name, addr_t addr) {
    if (vm->word_count >= FF_MAX_WORDS) return NULL;
    word_t* w = &vm->words[vm->word_count++];
    strncpy(w->name, name, FF_NAME_MAX);
    w->name[FF_NAME_MAX] = '\0';
    w->addr = addr;
    w->flags = 0;
    return w;
}

// THE HEART: Fast interpreter with switch dispatch
// This is the secret sauce - inline everything, let compiler optimize
static inline void execute(forth_t* vm, addr_t start) {
    addr_t pc = start;
    while (1) {
        uint8_t op = vm->dict[pc++];
        switch (op) {
            case OP_EXIT:
                if (vm->rp == 0) return;  // Exit interpreter
                pc = vm->rs[--vm->rp];    // Return from word
                break;
                
            case OP_LIT: {
                cell_t val = read_cell(vm, &pc);
                PUSH(vm, val);
                break;
            }
            
            case OP_CALL: {
                addr_t addr = read_addr(vm, &pc);
                vm->rs[vm->rp++] = pc;    // Save return address
                pc = addr;                 // Jump to word
                break;
            }
            
            case OP_ADD: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a + b);
                break;
            }
            
            case OP_SUB: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a - b);
                break;
            }
            
            case OP_MUL: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a * b);
                break;
            }
            
            case OP_DIV: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, b ? a / b : 0);
                break;
            }
            
            case OP_DUP: {
                if (vm->sp > 0) {
                    PUSH(vm, TOS(vm));
                }
                break;
            }
            
            case OP_DROP: {
                if (vm->sp > 0) vm->sp--;
                break;
            }
            
            case OP_SWAP: {
                if (vm->sp >= 2) {
                    cell_t tmp = TOS(vm);
                    TOS(vm) = NOS(vm);
                    NOS(vm) = tmp;
                }
                break;
            }
            
            case OP_OVER: {
                if (vm->sp >= 2) {
                    PUSH(vm, NOS(vm));
                }
                break;
            }
            
            case OP_DOT: {
                if (vm->sp > 0) {
                    printf("%d ", (int)POP(vm));
                    fflush(stdout);
                }
                break;
            }
            case OP_AND: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a & b);
                break;
            }
            case OP_OR: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a | b);
                break;
            }
            case OP_XOR: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a ^ b);
                break;
            }
            case OP_NOT: {
                cell_t a = POP(vm);
                PUSH(vm, ~a);
                break;
            }
            case OP_LT: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a < b ? -1 : 0);
                break;
            }
            case OP_GT: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a > b ? -1 : 0);
                break;
            }
            case OP_EQ: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a == b ? -1 : 0);
                break;
            }
            case OP_LE: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a <= b ? -1 : 0);
                break;
            }
            case OP_GE: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a >= b ? -1 : 0);
                break;
            }
            case OP_NE: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a != b ? -1 : 0);
                break;
            }
            case OP_BRANCH: {
                addr_t target = read_addr(vm, &pc);
                pc = target;
                break;
            }
            case OP_BRANCH_IF_ZERO: {
                addr_t target = read_addr(vm, &pc);
                cell_t cond = POP(vm);
                if (cond == 0) pc = target;
                break;
            }
            case OP_DO: {
                // (limit index -- ) R: ( -- limit index)
                cell_t index = POP(vm);
                cell_t limit = POP(vm);
                vm->rs[vm->rp++] = limit;
                vm->rs[vm->rp++] = index;
                break;
            }
            case OP_LOOP: {
                addr_t loop_addr = read_addr(vm, &pc);
                cell_t index = vm->rs[vm->rp - 1] + 1;  // Index is at rp-1
                cell_t limit = vm->rs[vm->rp - 2];       // Limit is at rp-2
                if (index < limit) {
                    vm->rs[vm->rp - 1] = index;  // Update index
                    pc = loop_addr;
                } else {
                    vm->rp -= 2;  // Pop both limit and index
                }
                break;
            }
            case OP_I: {
                // Push current loop index
                if (vm->rp >= 2) {
                    PUSH(vm, vm->rs[vm->rp - 1]);
                }
                break;
            }
            case OP_LOAD: {
                cell_t addr = POP(vm);
                if (addr >= 0 && addr + sizeof(cell_t) <= FF_DICT_SIZE) {
                    cell_t val = 0;
                    for (size_t i = 0; i < sizeof(cell_t); i++) {
                        val |= ((cell_t)vm->dict[addr + i]) << (i * 8);
                    }
                    PUSH(vm, val);
                } else {
                    PUSH(vm, 0);
                }
                break;
            }
            case OP_STORE: {
                cell_t addr = POP(vm);
                cell_t val = POP(vm);
                if (addr >= 0 && addr + sizeof(cell_t) <= FF_DICT_SIZE) {
                    for (size_t i = 0; i < sizeof(cell_t); i++) {
                        vm->dict[addr + i] = (val >> (i * 8)) & 0xFF;
                    }
                }
                break;
            }
            case OP_LOAD_BYTE: {
                cell_t addr = POP(vm);
                if (addr >= 0 && addr < FF_DICT_SIZE) {
                    PUSH(vm, vm->dict[addr]);
                } else {
                    PUSH(vm, 0);
                }
                break;
            }
            case OP_STORE_BYTE: {
                cell_t addr = POP(vm);
                cell_t val = POP(vm);
                if (addr >= 0 && addr < FF_DICT_SIZE) {
                    vm->dict[addr] = val & 0xFF;
                }
                break;
            }
            
            // Stack ops extended
            case OP_ROT: {
                // ( a b c -- b c a )
                if (vm->sp >= 3) {
                    cell_t c = vm->ds[vm->sp - 1];
                    cell_t b = vm->ds[vm->sp - 2];
                    cell_t a = vm->ds[vm->sp - 3];
                    vm->ds[vm->sp - 3] = b;
                    vm->ds[vm->sp - 2] = c;
                    vm->ds[vm->sp - 1] = a;
                }
                break;
            }
            case OP_2DUP: {
                // ( a b -- a b a b )
                if (vm->sp >= 2) {
                    cell_t b = vm->ds[vm->sp - 1];
                    cell_t a = vm->ds[vm->sp - 2];
                    PUSH(vm, a);
                    PUSH(vm, b);
                }
                break;
            }
            case OP_2DROP: {
                // ( a b -- )
                if (vm->sp >= 2) {
                    vm->sp -= 2;
                }
                break;
            }
            case OP_NIP: {
                // ( a b -- b )
                if (vm->sp >= 2) {
                    vm->ds[vm->sp - 2] = vm->ds[vm->sp - 1];
                    vm->sp--;
                }
                break;
            }
            case OP_TUCK: {
                // ( a b -- b a b )
                if (vm->sp >= 2) {
                    cell_t b = vm->ds[vm->sp - 1];
                    cell_t a = vm->ds[vm->sp - 2];
                    vm->ds[vm->sp - 2] = b;
                    vm->ds[vm->sp - 1] = a;
                    PUSH(vm, b);
                }
                break;
            }
            
            // Return stack
            case OP_TO_R: {
                // >R ( n -- ) R: ( -- n )
                cell_t val = POP(vm);
                if (vm->rp < FF_RET_DEPTH) {
                    vm->rs[vm->rp++] = val;
                }
                break;
            }
            case OP_R_FROM: {
                // R> ( -- n ) R: ( n -- )
                if (vm->rp > 0) {
                    PUSH(vm, vm->rs[--vm->rp]);
                }
                break;
            }
            case OP_R_FETCH: {
                // R@ ( -- n ) R: ( n -- n )
                if (vm->rp > 0) {
                    PUSH(vm, vm->rs[vm->rp - 1]);
                }
                break;
            }
            
            // Arithmetic extended
            case OP_MOD: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, b ? a % b : 0);
                break;
            }
            case OP_NEGATE: {
                cell_t a = POP(vm);
                PUSH(vm, -a);
                break;
            }
            case OP_ABS: {
                cell_t a = POP(vm);
                PUSH(vm, a < 0 ? -a : a);
                break;
            }
            case OP_MIN: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a < b ? a : b);
                break;
            }
            case OP_MAX_OP: {
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                PUSH(vm, a > b ? a : b);
                break;
            }
            case OP_DIVMOD: {
                // /MOD ( a b -- rem quot )
                cell_t b = POP(vm);
                cell_t a = POP(vm);
                if (b) {
                    PUSH(vm, a % b);  // remainder
                    PUSH(vm, a / b);  // quotient
                } else {
                    PUSH(vm, 0);
                    PUSH(vm, 0);
                }
                break;
            }
            case OP_1PLUS: {
                if (vm->sp > 0) {
                    TOS(vm)++;
                }
                break;
            }
            case OP_1MINUS: {
                if (vm->sp > 0) {
                    TOS(vm)--;
                }
                break;
            }
            
            // Comparisons extended
            case OP_ZERO_EQ: {
                cell_t a = POP(vm);
                PUSH(vm, a == 0 ? -1 : 0);
                break;
            }
            case OP_ZERO_LT: {
                cell_t a = POP(vm);
                PUSH(vm, a < 0 ? -1 : 0);
                break;
            }
            case OP_ZERO_NE: {
                cell_t a = POP(vm);
                PUSH(vm, a != 0 ? -1 : 0);
                break;
            }
            
            // Stack extended
            case OP_QDUP: {
                // ?DUP ( n -- n n | 0 )
                if (vm->sp > 0 && TOS(vm) != 0) {
                    PUSH(vm, TOS(vm));
                }
                break;
            }
            
            // Memory extended
            case OP_PLUSSTORE: {
                // +! ( n addr -- )
                cell_t addr = POP(vm);
                cell_t val = POP(vm);
                if (addr >= 0 && addr + sizeof(cell_t) <= FF_DICT_SIZE) {
                    cell_t old_val = 0;
                    for (size_t i = 0; i < sizeof(cell_t); i++) {
                        old_val |= ((cell_t)vm->dict[addr + i]) << (i * 8);
                    }
                    cell_t new_val = old_val + val;
                    for (size_t i = 0; i < sizeof(cell_t); i++) {
                        vm->dict[addr + i] = (new_val >> (i * 8)) & 0xFF;
                    }
                }
                break;
            }
            case OP_ALLOT: {
                // ALLOT ( n -- ) allocate n bytes in dictionary
                cell_t n = POP(vm);
                if (n > 0 && vm->here + n <= FF_DICT_SIZE) {
                    vm->here += n;
                }
                break;
            }
            
            // I/O
            case OP_EMIT: {
                cell_t c = POP(vm);
                if (vm->io.putchar_fn) {
                    vm->io.putchar_fn((int)c);
                    if (vm->io.flush_fn) vm->io.flush_fn();
                }
                break;
            }
            case OP_KEY: {
                int c = vm->io.getchar_fn ? vm->io.getchar_fn() : -1;
                PUSH(vm, c);
                break;
            }
            case OP_CR: {
                if (vm->io.putchar_fn) {
                    vm->io.putchar_fn('\n');
                    if (vm->io.flush_fn) vm->io.flush_fn();
                }
                break;
            }
            case OP_TYPE: {
                // TYPE ( addr len -- ) print string
                cell_t len = POP(vm);
                cell_t addr = POP(vm);
                if (addr >= 0 && addr + len <= FF_DICT_SIZE && vm->io.putchar_fn) {
                    for (cell_t i = 0; i < len; i++) {
                        vm->io.putchar_fn(vm->dict[addr + i]);
                    }
                    if (vm->io.flush_fn) vm->io.flush_fn();
                }
                break;
            }
            
            // Memory info
            case OP_HERE: {
                PUSH(vm, vm->here);
                break;
            }
            
            // Debug/Introspection
            case OP_DOT_S: {
                // .S ( -- ) show stack non-destructively
                printf("<");
                printf("%d", vm->sp);
                printf("> ");
                for (int i = 0; i < vm->sp; i++) {
                    printf("%d ", (int)vm->ds[i]);
                }
                fflush(stdout);
                break;
            }
            case OP_DEPTH: {
                PUSH(vm, vm->sp);
                break;
            }
            case OP_CLEAR: {
                vm->sp = 0;
                break;
            }
            case OP_WORDS: {
                printf("Words: ");
                for (int i = 0; i < vm->word_count; i++) {
                    printf("%s ", vm->words[i].name);
                }
                printf("\n");
                fflush(stdout);
                break;
            }
            case OP_SEE: {
                // This is a special case - handled in interpret_line
                // Should not be executed in bytecode
                break;
            }
            
            default:
                fprintf(stderr, "Unknown opcode: %d at pc=%d\n", op, pc - 1);
                return;
        }
    }
}

// Token parsing
static const char* next_token(forth_t* vm, const char* in) {
    while (*in && isspace((unsigned char)*in)) in++;
    if (!*in) return NULL;
    
    char* out = vm->token;
    size_t len = 0;
    while (*in && !isspace((unsigned char)*in) && len < FF_NAME_MAX) {
        *out++ = (char)toupper((unsigned char)*in++);
        len++;
    }
    *out = '\0';
    
    while (*in && !isspace((unsigned char)*in)) in++;
    return in;
}

// Interpret a token
static int interpret_token(forth_t* vm, const char* tok) {
    // Ignore parenthesis comments
    if (strcmp(tok, "(") == 0) {
        return 1;  // Comment, just skip
    }
    
    // Check for special words first
    if (strcmp(tok, ":") == 0) {
        return 1;  // Handle in interpreter
    }
    if (strcmp(tok, ";") == 0) {
        return 1;  // Handle in interpreter
    }
    
    // Handle I specially - must be inlined, not called
    if (strcmp(tok, "I") == 0) {
        if (vm->compiling) {
            emit_byte(vm, OP_I);  // Emit directly, no OP_CALL
        } else {
            // In immediate mode, just push current loop index if available
            if (vm->rp >= 2) {
                PUSH(vm, vm->rs[vm->rp - 1]);
            }
        }
        return 1;
    }
    
    // Look up word
    word_t* w = find_word(vm, tok);
    if (w) {
        if (vm->compiling) {
            // Compile a call to this word
            emit_byte(vm, OP_CALL);
            emit_addr(vm, w->addr);
        } else {
            // Execute immediately
            execute(vm, w->addr);
        }
        return 1;
    }
    
    // Try to parse as number
    char* end;
    long val = strtol(tok, &end, 10);
    if (*tok && *end == '\0') {
        if (vm->compiling) {
            emit_byte(vm, OP_LIT);
            emit_cell(vm, (cell_t)val);
        } else {
            PUSH(vm, (cell_t)val);
        }
        return 1;
    }
    
    return 0;  // Unknown word
}

// Interpret a line
static int interpret_line(forth_t* vm, const char* line) {
    // Handle backslash comments - create a temporary buffer
    char line_buf[256];
    const char* p = line;
    
    // Check for backslash comment
    const char* backslash = strchr(line, '\\');
    if (backslash) {
        // Only treat as comment if it's at start or preceded by whitespace
        if (backslash == line || isspace((unsigned char)*(backslash - 1))) {
            // Copy only the part before the comment
            size_t len = backslash - line;
            if (len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
            if (len > 0) {
                memcpy(line_buf, line, len);
                line_buf[len] = '\0';
                p = line_buf;
            } else {
                return 1;  // Empty line or just comment
            }
        }
    }
    
    while ((p = next_token(vm, p))) {
        const char* t = vm->token;
        
        // Handle parenthesis comments
        if (strcmp(t, "(") == 0) {
            // Skip until closing )
            while (*p && *p != ')') p++;
            if (*p == ')') p++;  // Skip the )
            continue;
        }
        
        // Handle colon definition
        if (strcmp(t, ":") == 0) {
            p = next_token(vm, p);
            if (!p) return 0;
            
            // Start compiling
            addr_t word_addr = vm->here;
            add_word(vm, vm->token, word_addr);
            vm->compiling = 1;
            continue;
        }
        
        // Handle semicolon (end definition)
        if (strcmp(t, ";") == 0) {
            emit_byte(vm, OP_EXIT);
            vm->compiling = 0;
            continue;
        }
        
        // Handle BYE/QUIT/EXIT - exit the REPL
        if (strcmp(t, "BYE") == 0 || strcmp(t, "QUIT") == 0 || strcmp(t, "EXIT") == 0) {
            exit(0);
        }
        
        // Handle CONSTANT - create a constant
        if (strcmp(t, "CONSTANT") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "CONSTANT needs a name\n");
                return 0;
            }
            // Value is on stack
            if (vm->sp < 1) {
                fprintf(stderr, "CONSTANT needs a value on stack\n");
                return 0;
            }
            cell_t val = POP(vm);
            // Create a word that pushes the constant value
            addr_t word_addr = vm->here;
            emit_byte(vm, OP_LIT);
            emit_cell(vm, val);
            emit_byte(vm, OP_EXIT);
            add_word(vm, vm->token, word_addr);
            continue;
        }
        
        // Handle VARIABLE - create a variable
        if (strcmp(t, "VARIABLE") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "VARIABLE needs a name\n");
                return 0;
            }
            // Allocate space in dictionary for one cell
            addr_t var_addr = vm->here;
            for (int i = 0; i < (int)sizeof(cell_t); i++) {
                emit_byte(vm, 0);
            }
            // Create a word that pushes the variable's address
            addr_t word_addr = vm->here;
            emit_byte(vm, OP_LIT);
            emit_cell(vm, var_addr);
            emit_byte(vm, OP_EXIT);
            add_word(vm, vm->token, word_addr);
            continue;
        }
        
        // Handle SEE - decompile a word
        if (strcmp(t, "SEE") == 0 || strcmp(t, "LIST") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "SEE needs a word name\n");
                return 0;
            }
            word_t* w = find_word(vm, vm->token);
            if (!w) {
                fprintf(stderr, "? %s\n", vm->token);
                return 0;
            }
            
            printf(": %s\n", w->name);
            addr_t pc = w->addr;
            int indent = 2;
            
            while (pc < vm->here) {
                uint8_t op = vm->dict[pc++];
                printf("%*s", indent, "");
                
                if (op == OP_EXIT) {
                    printf(";\n");
                    break;
                } else if (op == OP_LIT) {
                    cell_t val = read_cell(vm, &pc);
                    printf("LIT %d\n", (int)val);
                } else if (op == OP_CALL) {
                    addr_t addr = read_addr(vm, &pc);
                    // Find word name
                    const char* name = "?";
                    for (int i = 0; i < vm->word_count; i++) {
                        if (vm->words[i].addr == addr) {
                            name = vm->words[i].name;
                            break;
                        }
                    }
                    printf("%s\n", name);
                } else if (op == OP_BRANCH) {
                    addr_t target = read_addr(vm, &pc);
                    printf("BRANCH -> %d\n", target);
                } else if (op == OP_BRANCH_IF_ZERO) {
                    addr_t target = read_addr(vm, &pc);
                    printf("BRANCH0 -> %d\n", target);
                } else if (op == OP_DO) {
                    printf("DO\n");
                } else if (op == OP_LOOP) {
                    addr_t target = read_addr(vm, &pc);
                    printf("LOOP -> %d\n", target);
                } else {
                    // Map opcode to name
                    const char* opnames[] = {
                        "EXIT", "LIT", "CALL", "+", "-", "*", "/",
                        "DUP", "DROP", "SWAP", "OVER", ".",
                        "AND", "OR", "XOR", "NOT", "<", ">", "=",
                        "BRANCH", "BRANCH0", "DO", "LOOP", "I",
                        "@", "!", "C@", "C!",
                        "ROT", "2DUP", "2DROP", "NIP", "TUCK",
                        ">R", "R>", "R@",
                        "MOD", "NEGATE", "ABS", "MIN", "MAX",
                        "0=", "0<",
                        "EMIT", "KEY", "CR", "TYPE",
                        "HERE", ".S", "DEPTH", "CLEAR", "WORDS", "SEE"
                    };
                    if (op < sizeof(opnames)/sizeof(opnames[0])) {
                        printf("%s\n", opnames[op]);
                    } else {
                        printf("OP_%d\n", op);
                    }
                }
            }
            continue;
        }
        
        // Handle LOAD - load a file
        if (strcmp(t, "LOAD") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "LOAD needs a filename\n");
                return 0;
            }
            
            if (!vm->io.fopen_fn || !vm->io.fgets_fn || !vm->io.fclose_fn) {
                fprintf(stderr, "File I/O not available\n");
                return 0;
            }
            
            FILE* fp = vm->io.fopen_fn(vm->token, "r");
            if (!fp) {
                fprintf(stderr, "Cannot open %s\n", vm->token);
                return 0;
            }
            
            char line[256];
            while (vm->io.fgets_fn(line, sizeof(line), fp)) {
                if (!interpret_line(vm, line)) {
                    vm->io.fclose_fn(fp);
                    return 0;
                }
            }
            vm->io.fclose_fn(fp);
            printf("Loaded %s\n", vm->token);
            continue;
        }
        
        // Handle SAVE - save user-defined words
        if (strcmp(t, "SAVE") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "SAVE needs a filename\n");
                return 0;
            }
            
            if (!vm->io.fopen_fn || !vm->io.fputs_fn || !vm->io.fclose_fn) {
                fprintf(stderr, "File I/O not available\n");
                return 0;
            }
            
            FILE* fp = vm->io.fopen_fn(vm->token, "w");
            if (!fp) {
                fprintf(stderr, "Cannot create %s\n", vm->token);
                return 0;
            }
            
            // Save only user-defined words (after builtins)
            for (int i = vm->builtin_count; i < vm->word_count; i++) {
                word_t* w = &vm->words[i];
                char buf[512];
                snprintf(buf, sizeof(buf), ": %s ", w->name);
                vm->io.fputs_fn(buf, fp);
                
                // Decompile the word
                addr_t pc = w->addr;
                while (pc < vm->here) {
                    uint8_t op = vm->dict[pc++];
                    if (op == OP_EXIT) {
                        vm->io.fputs_fn(";\n", fp);
                        break;
                    } else if (op == OP_LIT) {
                        cell_t val = read_cell(vm, &pc);
                        snprintf(buf, sizeof(buf), "%d ", (int)val);
                        vm->io.fputs_fn(buf, fp);
                    } else if (op == OP_CALL) {
                        addr_t addr = read_addr(vm, &pc);
                        for (int j = 0; j < vm->word_count; j++) {
                            if (vm->words[j].addr == addr) {
                                snprintf(buf, sizeof(buf), "%s ", vm->words[j].name);
                                vm->io.fputs_fn(buf, fp);
                                break;
                            }
                        }
                    } else if (op == OP_BRANCH) {
                        // Check if this is a ." pattern:
                        // BRANCH -> string_data -> LIT addr -> LIT len -> TYPE
                        addr_t branch_target = read_addr(vm, &pc);
                        addr_t saved_pc = pc;
                        
                        // Check if pattern matches ." (BRANCH skips string, then LIT, LIT, TYPE)
                        if (branch_target > pc && branch_target < vm->here &&
                            vm->dict[branch_target] == OP_LIT) {
                            
                            addr_t check_pc = branch_target;
                            check_pc++; // Skip OP_LIT
                            cell_t str_addr = read_cell(vm, &check_pc);
                            
                            if (vm->dict[check_pc] == OP_LIT) {
                                check_pc++; // Skip second OP_LIT
                                cell_t str_len = read_cell(vm, &check_pc);
                                
                                if (vm->dict[check_pc] == OP_TYPE && 
                                    str_addr == saved_pc && 
                                    str_addr + str_len == branch_target) {
                                    // It's a ." pattern!
                                    vm->io.fputs_fn(".\" ", fp);
                                    for (cell_t i = 0; i < str_len; i++) {
                                        char c = vm->dict[str_addr + i];
                                        if (c == '"' || c == '\\') {
                                            fputc('\\', fp);
                                        }
                                        fputc(c, fp);
                                    }
                                    vm->io.fputs_fn("\" ", fp);
                                    
                                    // Skip the string data, LIT, LIT, TYPE
                                    pc = check_pc + 1;
                                    continue;
                                }
                            }
                        }
                        
                        // Not a ." pattern, treat as ELSE or other branch
                        vm->io.fputs_fn("ELSE ", fp);
                    } else {
                        // Map opcodes to words
                        const char* op_word = NULL;
                        if (op == OP_ADD) op_word = "+ ";
                        else if (op == OP_SUB) op_word = "- ";
                        else if (op == OP_MUL) op_word = "* ";
                        else if (op == OP_DIV) op_word = "/ ";
                        else if (op == OP_DUP) op_word = "DUP ";
                        else if (op == OP_DROP) op_word = "DROP ";
                        else if (op == OP_SWAP) op_word = "SWAP ";
                        else if (op == OP_OVER) op_word = "OVER ";
                        else if (op == OP_DOT) op_word = ". ";
                        else if (op == OP_EMIT) op_word = "EMIT ";
                        else if (op == OP_CR) op_word = "CR ";
                        else if (op == OP_I) op_word = "I ";
                        else if (op == OP_DO) op_word = "DO ";
                        else if (op == OP_LOOP) {
                            read_addr(vm, &pc);
                            op_word = "LOOP ";
                        }
                        else if (op == OP_BRANCH_IF_ZERO) {
                            read_addr(vm, &pc);
                            op_word = "IF ";
                        }
                        
                        if (op_word) {
                            vm->io.fputs_fn(op_word, fp);
                        }
                    }
                }
            }
            
            vm->io.fclose_fn(fp);
            printf("Saved %d words to %s\n", vm->word_count - vm->builtin_count, vm->token);
            continue;
        }
        
        // Handle SAVEB - save bytecode to binary file
        if (strcmp(t, "SAVEB") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "SAVEB needs a filename\n");
                return 0;
            }
            
            if (!vm->io.fopen_fn || !vm->io.fclose_fn) {
                fprintf(stderr, "File I/O not available\n");
                return 0;
            }
            
            FILE* fp = vm->io.fopen_fn(vm->token, "wb");
            if (!fp) {
                fprintf(stderr, "Cannot create %s\n", vm->token);
                return 0;
            }
            
            // Write header: magic number, version, metadata
            const uint32_t magic = 0x46545448;  // "FTTH" (Fast Forth)
            const uint16_t version = 1;
            fwrite(&magic, sizeof(magic), 1, fp);
            fwrite(&version, sizeof(version), 1, fp);
            fwrite(&vm->here, sizeof(vm->here), 1, fp);
            fwrite(&vm->word_count, sizeof(vm->word_count), 1, fp);
            fwrite(&vm->builtin_count, sizeof(vm->builtin_count), 1, fp);
            
            // Write dictionary
            fwrite(vm->dict, 1, vm->here, fp);
            
            // Write word table
            fwrite(vm->words, sizeof(word_t), vm->word_count, fp);
            
            fclose(fp);
            printf("Saved bytecode (%d bytes, %d words) to %s\n", 
                   vm->here, vm->word_count, vm->token);
            continue;
        }
        
        // Handle LOADB - load bytecode from binary file
        if (strcmp(t, "LOADB") == 0) {
            p = next_token(vm, p);
            if (!p) {
                fprintf(stderr, "LOADB needs a filename\n");
                return 0;
            }
            
            if (!vm->io.fopen_fn || !vm->io.fclose_fn) {
                fprintf(stderr, "File I/O not available\n");
                return 0;
            }
            
            FILE* fp = vm->io.fopen_fn(vm->token, "rb");
            if (!fp) {
                fprintf(stderr, "Cannot open %s\n", vm->token);
                return 0;
            }
            
            // Read and verify header
            uint32_t magic;
            uint16_t version;
            if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != 0x46545448) {
                fprintf(stderr, "Invalid bytecode file: bad magic\n");
                fclose(fp);
                return 0;
            }
            if (fread(&version, sizeof(version), 1, fp) != 1 || version != 1) {
                fprintf(stderr, "Unsupported bytecode version\n");
                fclose(fp);
                return 0;
            }
            
            addr_t saved_here;
            int saved_word_count, saved_builtin_count;
            fread(&saved_here, sizeof(saved_here), 1, fp);
            fread(&saved_word_count, sizeof(saved_word_count), 1, fp);
            fread(&saved_builtin_count, sizeof(saved_builtin_count), 1, fp);
            
            // Validate sizes
            if (saved_here > FF_DICT_SIZE || saved_word_count > FF_MAX_WORDS) {
                fprintf(stderr, "Bytecode too large for VM\n");
                fclose(fp);
                return 0;
            }
            
            // Read dictionary
            if (fread(vm->dict, 1, saved_here, fp) != saved_here) {
                fprintf(stderr, "Failed to read dictionary\n");
                fclose(fp);
                return 0;
            }
            
            // Read word table
            if (fread(vm->words, sizeof(word_t), saved_word_count, fp) != 
                (size_t)saved_word_count) {
                fprintf(stderr, "Failed to read word table\n");
                fclose(fp);
                return 0;
            }
            
            vm->here = saved_here;
            vm->word_count = saved_word_count;
            vm->builtin_count = saved_builtin_count;
            
            fclose(fp);
            printf("Loaded bytecode (%d bytes, %d words) from %s\n", 
                   vm->here, vm->word_count, vm->token);
            continue;
        }
        
        // Handle ." (dot-quote) - print string literal
        if (strcmp(t, ".\"") == 0) {
            // Find the closing quote
            while (*p && isspace((unsigned char)*p)) p++;
            const char* str_start = p;
            while (*p && *p != '"') p++;
            if (*p != '"') {
                fprintf(stderr, "Unterminated string in .\"\n");
                return 0;
            }
            size_t str_len = p - str_start;
            p++; // Skip closing quote
            
            if (vm->compiling) {
                // Emit BRANCH to skip over string data
                emit_byte(vm, OP_BRANCH);
                addr_t branch_loc = vm->here;
                emit_addr(vm, 0);  // Placeholder
                
                // Store string in dictionary
                addr_t str_addr = vm->here;
                for (size_t i = 0; i < str_len; i++) {
                    emit_byte(vm, (uint8_t)str_start[i]);
                }
                
                // Patch branch to jump here
                patch_addr(vm, branch_loc, vm->here);
                
                // Emit TYPE instruction with address and length
                emit_byte(vm, OP_LIT);
                emit_cell(vm, str_addr);
                emit_byte(vm, OP_LIT);
                emit_cell(vm, (cell_t)str_len);
                emit_byte(vm, OP_TYPE);
            } else {
                // Immediate mode - just print it
                for (size_t i = 0; i < str_len; i++) {
                    putchar(str_start[i]);
                }
                fflush(stdout);
            }
            continue;
        }
        
        // Handle IF (compile-only)
        if (strcmp(t, "IF") == 0) {
            if (!vm->compiling) {
                fprintf(stderr, "IF only works in compilation mode\n");
                return 0;
            }
            emit_byte(vm, OP_BRANCH_IF_ZERO);
            vm->cstack[vm->csp++] = vm->here;  // Save location to patch
            emit_addr(vm, 0);  // Placeholder
            continue;
        }
        
        // Handle THEN (compile-only)
        if (strcmp(t, "THEN") == 0) {
            if (!vm->compiling || vm->csp == 0) {
                fprintf(stderr, "THEN without IF\n");
                return 0;
            }
            addr_t if_addr = vm->cstack[--vm->csp];
            patch_addr(vm, if_addr, vm->here);  // Patch IF to jump here
            continue;
        }
        
        // Handle ELSE (compile-only)
        if (strcmp(t, "ELSE") == 0) {
            if (!vm->compiling || vm->csp == 0) {
                fprintf(stderr, "ELSE without IF\n");
                return 0;
            }
            emit_byte(vm, OP_BRANCH);  // Unconditional jump over ELSE clause
            addr_t else_addr = vm->here;
            emit_addr(vm, 0);  // Placeholder
            
            addr_t if_addr = vm->cstack[--vm->csp];
            patch_addr(vm, if_addr, vm->here);  // Patch IF to jump here
            vm->cstack[vm->csp++] = else_addr;  // Save ELSE location for THEN
            continue;
        }
        
        // Handle DO (compile-only)
        if (strcmp(t, "DO") == 0) {
            if (!vm->compiling) {
                fprintf(stderr, "DO only works in compilation mode\n");
                return 0;
            }
            emit_byte(vm, OP_DO);
            vm->cstack[vm->csp++] = vm->here;  // Save address AFTER OP_DO for LOOP to jump back to
            continue;
        }
        
        // Handle LOOP (compile-only)
        if (strcmp(t, "LOOP") == 0) {
            if (!vm->compiling || vm->csp == 0) {
                fprintf(stderr, "LOOP without DO\n");
                return 0;
            }
            emit_byte(vm, OP_LOOP);
            addr_t loop_start = vm->cstack[--vm->csp];
            emit_addr(vm, loop_start);  // Jump back to DO
            continue;
        }
        
        // Handle BEGIN (compile-only)
        if (strcmp(t, "BEGIN") == 0) {
            if (!vm->compiling) {
                fprintf(stderr, "BEGIN only works in compilation mode\n");
                return 0;
            }
            vm->cstack[vm->csp++] = vm->here;  // Mark loop start
            continue;
        }
        
        // Handle WHILE (compile-only)
        if (strcmp(t, "WHILE") == 0) {
            if (!vm->compiling || vm->csp == 0) {
                fprintf(stderr, "WHILE without BEGIN\n");
                return 0;
            }
            emit_byte(vm, OP_BRANCH_IF_ZERO);  // Exit loop if TOS is false (zero)
            vm->cstack[vm->csp++] = vm->here;  // Save location to patch for exit
            emit_addr(vm, 0);  // Placeholder for exit address
            continue;
        }
        
        // Handle REPEAT (compile-only)
        if (strcmp(t, "REPEAT") == 0) {
            if (!vm->compiling || vm->csp < 2) {
                fprintf(stderr, "REPEAT without BEGIN/WHILE\n");
                return 0;
            }
            addr_t while_addr = vm->cstack[--vm->csp];  // WHILE's branch location
            addr_t begin_addr = vm->cstack[--vm->csp];  // BEGIN location
            emit_byte(vm, OP_BRANCH);  // Unconditional jump back to BEGIN
            emit_addr(vm, begin_addr);
            patch_addr(vm, while_addr, vm->here);  // WHILE exits to here
            continue;
        }
        
        // Interpret/compile token
        if (!interpret_token(vm, t)) {
            fprintf(stderr, "? %s\n", t);
            return 0;
        }
    }
    
    return 1;
}

// Initialize VM
static void init_forth(forth_t* vm) {
    memset(vm, 0, sizeof(*vm));
    
    // Setup default I/O callbacks
    vm->io.getchar_fn = getchar;
    vm->io.putchar_fn = default_putchar;
    vm->io.flush_fn = default_flush;
    vm->io.fopen_fn = fopen;
    vm->io.fclose_fn = fclose;
    vm->io.fgets_fn = fgets;
    vm->io.fputs_fn = fputs;
    
    // Add primitive words that just execute inline bytecode
    // + primitive
    addr_t addr = vm->here;
    emit_byte(vm, OP_ADD);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "+", addr);
    
    // - primitive
    addr = vm->here;
    emit_byte(vm, OP_SUB);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "-", addr);
    
    // * primitive
    addr = vm->here;
    emit_byte(vm, OP_MUL);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "*", addr);
    
    // / primitive
    addr = vm->here;
    emit_byte(vm, OP_DIV);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "/", addr);
    
    // DUP primitive
    addr = vm->here;
    emit_byte(vm, OP_DUP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "DUP", addr);
    
    // DROP primitive
    addr = vm->here;
    emit_byte(vm, OP_DROP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "DROP", addr);
    
    // SWAP primitive
    addr = vm->here;
    emit_byte(vm, OP_SWAP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "SWAP", addr);
    
    // OVER primitive
    addr = vm->here;
    emit_byte(vm, OP_OVER);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "OVER", addr);
    
    // . primitive
    addr = vm->here;
    emit_byte(vm, OP_DOT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, ".", addr);
    
    // Bitwise operations
    addr = vm->here;
    emit_byte(vm, OP_AND);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "AND", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_OR);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "OR", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_XOR);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "XOR", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_NOT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "NOT", addr);
    
    // Comparisons
    addr = vm->here;
    emit_byte(vm, OP_LT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "<", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_GT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, ">", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_EQ);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "=", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_LE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "<=", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_GE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, ">=", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_NE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "<>", addr);
    
    // Memory operations
    addr = vm->here;
    emit_byte(vm, OP_LOAD);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "@", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_STORE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "!", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_LOAD_BYTE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "C@", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_STORE_BYTE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "C!", addr);
    
    // Loop index
    addr = vm->here;
    emit_byte(vm, OP_I);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "I", addr);
    
    // Stack ops extended
    addr = vm->here;
    emit_byte(vm, OP_ROT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "ROT", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_2DUP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "2DUP", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_2DROP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "2DROP", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_NIP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "NIP", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_TUCK);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "TUCK", addr);
    
    // Return stack
    addr = vm->here;
    emit_byte(vm, OP_TO_R);
    emit_byte(vm, OP_EXIT);
    add_word(vm, ">R", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_R_FROM);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "R>", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_R_FETCH);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "R@", addr);
    
    // Arithmetic extended
    addr = vm->here;
    emit_byte(vm, OP_MOD);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "MOD", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_NEGATE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "NEGATE", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_ABS);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "ABS", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_MIN);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "MIN", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_MAX_OP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "MAX", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_DIVMOD);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "/MOD", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_1PLUS);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "1+", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_1MINUS);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "1-", addr);
    
    // Comparisons extended
    addr = vm->here;
    emit_byte(vm, OP_ZERO_EQ);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "0=", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_ZERO_LT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "0<", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_ZERO_NE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "0<>", addr);
    
    // Stack extended
    addr = vm->here;
    emit_byte(vm, OP_QDUP);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "?DUP", addr);
    
    // Memory extended
    addr = vm->here;
    emit_byte(vm, OP_PLUSSTORE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "+!", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_ALLOT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "ALLOT", addr);
    
    // I/O
    addr = vm->here;
    emit_byte(vm, OP_EMIT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "EMIT", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_KEY);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "KEY", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_CR);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "CR", addr);
    
    // Memory info
    addr = vm->here;
    emit_byte(vm, OP_HERE);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "HERE", addr);
    
    // Debug/Introspection
    addr = vm->here;
    emit_byte(vm, OP_DOT_S);
    emit_byte(vm, OP_EXIT);
    add_word(vm, ".S", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_DEPTH);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "DEPTH", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_CLEAR);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "CLEAR", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_WORDS);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "WORDS", addr);
    
    // Mark end of built-in words
    vm->builtin_count = vm->word_count;
}

// REPL
#ifdef FF_ENABLE_REPL
static void repl(forth_t* vm) {
    char line[256];
    while (1) {
        printf(vm->compiling ? "  " : "ok ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        if (interpret_line(vm, line) && !vm->compiling) {
            printf("\n");
        }
    }
    printf("\n");
}
#endif

#endif // FORTH_FAST_H
