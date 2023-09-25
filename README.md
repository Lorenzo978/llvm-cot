COT Project
=========
### Installation
```bash
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-add-repository "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-16 main"
sudo apt-get update
sudo apt-get install -y llvm-16 llvm-16-dev llvm-16-tools clang-16
```
This will install all the required header files, libraries and tools in
`/usr/lib/llvm-16/`.

### Build
In the `llvm-cot` main folder:
```bash
cd build
cmake -DLT_LLVM_INSTALL_DIR=/usr/lib/llvm-16 ../
make
```
### Generate test code and passes
Always in `llvm-cot/build` folder:
```bash
export LLVM_DIR=/usr/lib/llvm-16
$LLVM_DIR/bin/clang -O1 -emit-llvm -c ../inputs/test.c -S -o test.ll
$LLVM_DIR/bin/opt -load-pass-plugin ./lib/libSecret.so --passes="print<inputsVector>" -disable-output test.ll

$LLVM_DIR/bin/opt -load-pass-plugin ./lib/libSecretNew.so --passes="print<inputsVector>" test.ll -S -o test2.ll
```
To compile the passes use `make` inside `build` directory.
