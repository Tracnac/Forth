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
    // Comparisons extended
    OP_ZERO_EQ,     // 0= ( n -- flag )
    OP_ZERO_LT,     // 0< ( n -- flag )
    // I/O
    OP_EMIT,        // EMIT ( c -- )
    OP_KEY,         // KEY ( -- c )
    OP_CR,          // CR ( -- )
    // Memory info
    OP_HERE,        // HERE ( -- addr )
    OP_MAX          // Marker
} opcode_t;

typedef int32_t cell_t;
typedef uint16_t addr_t;

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
            
            // I/O
            case OP_EMIT: {
                cell_t c = POP(vm);
                putchar((int)c);
                fflush(stdout);
                break;
            }
            case OP_KEY: {
                int c = getchar();
                PUSH(vm, c);
                break;
            }
            case OP_CR: {
                putchar('\n');
                fflush(stdout);
                break;
            }
            
            // Memory info
            case OP_HERE: {
                PUSH(vm, vm->here);
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
    const char* p = line;
    
    while ((p = next_token(vm, p))) {
        const char* t = vm->token;
        
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
    
    // Comparisons extended
    addr = vm->here;
    emit_byte(vm, OP_ZERO_EQ);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "0=", addr);
    
    addr = vm->here;
    emit_byte(vm, OP_ZERO_LT);
    emit_byte(vm, OP_EXIT);
    add_word(vm, "0<", addr);
    
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
