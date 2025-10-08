CC?=clang

# Aggressive optimization flags for maximum performance
CFLAGS?=-std=c11 -O3 -Wall -Wextra -pedantic
# Advanced Clang-specific optimizations
OPT_FLAGS=-march=native -mtune=native -flto -ffast-math -funroll-loops \
          -fvectorize -fslp-vectorize -finline-functions -fomit-frame-pointer \
          -momit-leaf-frame-pointer -fstrict-aliasing -fno-exceptions \
          -fvisibility=hidden -fvisibility-inlines-hidden

SRC_DIR=./src
BUILD_DIR=./build

all: $(BUILD_DIR) forth_fast bench_full

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

forth_fast: $(SRC_DIR)/forth_fast.c $(SRC_DIR)/forth_fast.h
	$(CC) $(CFLAGS) $(OPT_FLAGS) $(SRC_DIR)/forth_fast.c -o $(BUILD_DIR)/$@

bench_full: $(SRC_DIR)/bench_full.c $(SRC_DIR)/forth_fast.h
	$(CC) $(CFLAGS) $(OPT_FLAGS) -DNDEBUG $(SRC_DIR)/bench_full.c -o $(BUILD_DIR)/$@

run: forth_fast
	$(BUILD_DIR)/forth_fast

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
