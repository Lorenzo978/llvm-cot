#!/bin/bash

# Check if a filename argument is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

# Set LLVM_DIR variable
export LLVM_DIR=/usr/lib/llvm-16

# Extract the file name without extension
filename_with_extension="$1"
filename_without_extension="${filename_with_extension%.*}_exec"

# Create the build directory if it doesn't exist
build_dir="build"
mkdir -p "$build_dir"
cd "$build_dir" || exit


make

# Step 1: Compile to LLVM IR
$LLVM_DIR/bin/clang -O1 -emit-llvm -c "../inputs/$1" -S -o "test.ll"

# Step 2: Apply the first LLVM pass
$LLVM_DIR/bin/opt --passes=loop-simplify "test.ll" -S -o "test2.ll"

# Step 3: Apply the second LLVM pass
$LLVM_DIR/bin/opt -load-pass-plugin ./lib/libSecret.so --passes="print<inputsVector>" "test2.ll" -S -o "output.ll"

# Step 4: Generate object file
$LLVM_DIR/bin/llc -O0 -filetype=obj "output.ll" -o "output.o" -relocation-model=pic

# Step 5: Generate asm in arm cortex x64
$LLVM_DIR/bin/llc -O0 -mtriple=armv7m-none-eabi -filetype=asm "output.ll" -o "output.s"

# Step 6: Compile to an executable
gcc -O0 -o "${filename_without_extension}" "output.o" -pie


echo "Compilation complete. Executable: ${filename_without_extension}"

echo "Execution of compiled file:"

./${filename_without_extension}


