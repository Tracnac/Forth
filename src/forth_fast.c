#define FF_ENABLE_REPL
#include "forth_fast.h"

int main(int argc, char** argv) {
    forth_t vm;
    init_forth(&vm);
    
    int quiet = 0;
    int arg_start = 1;
    
    // Check for -q flag
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        arg_start = 2;
    }
    
    if (!quiet) {
        printf("Fast Forth VM\n");
        printf("================================\n");
    }
    
    // If a .fbc file is provided, load and execute it
    if (argc > arg_start) {
        const char* filename = argv[arg_start];
        size_t len = strlen(filename);
        
        // Check if it's a .fbc file
        if (len > 4 && strcasecmp(filename + len - 4, ".fbc") == 0) {
            // Load bytecode file
            FILE* fp = fopen(filename, "rb");
            if (!fp) {
                fprintf(stderr, "Cannot open %s\n", filename);
                return 1;
            }
            
            // Read and verify header
            uint32_t magic;
            uint16_t version;
            if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != 0x46545448) {
                fprintf(stderr, "Invalid bytecode file: bad magic\n");
                fclose(fp);
                return 1;
            }
            if (fread(&version, sizeof(version), 1, fp) != 1 || version != 1) {
                fprintf(stderr, "Unsupported bytecode version\n");
                fclose(fp);
                return 1;
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
                return 1;
            }
            
            // Read dictionary
            if (fread(vm.dict, 1, saved_here, fp) != saved_here) {
                fprintf(stderr, "Failed to read dictionary\n");
                fclose(fp);
                return 1;
            }
            
            // Read word table
            if (fread(vm.words, sizeof(word_t), saved_word_count, fp) != 
                (size_t)saved_word_count) {
                fprintf(stderr, "Failed to read word table\n");
                fclose(fp);
                return 1;
            }
            
            vm.here = saved_here;
            vm.word_count = saved_word_count;
            vm.builtin_count = saved_builtin_count;
            
            fclose(fp);
            if (!quiet) {
                printf("Loaded bytecode from %s (%d bytes, %d words)\n", 
                       filename, vm.here, vm.word_count);
                printf("--------------------------------\n");
            }
            
            // If more arguments, execute them as Forth code and exit
            if (argc > arg_start + 1) {
                for (int i = arg_start + 1; i < argc; i++) {
                    interpret_line(&vm, argv[i]);
                }
                return 0;  // Exit after executing commands
            }
        } else {
            // Load as regular Forth source file
            FILE* fp = fopen(filename, "r");
            if (!fp) {
                fprintf(stderr, "Cannot open %s\n", filename);
                return 1;
            }
            
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (!interpret_line(&vm, line)) {
                    fclose(fp);
                    return 1;
                }
            }
            fclose(fp);
            if (!quiet) {
                printf("Loaded %s\n", filename);
            }
        }
    }
    
    repl(&vm);
    
    return 0;
}
