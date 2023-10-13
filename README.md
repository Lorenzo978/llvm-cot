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

### Build project
In the `llvm-cot` main folder:
```bash
cmake -DLT_LLVM_INSTALL_DIR=/usr/lib/llvm-16 ../
```
### Build Secret Pass and run a test
In order to compile and run the pass on a particular test (all tests are in the `inputs` folder), execute in the `llvm-cot` main folder the following scripts:
```bash
./compile.sh <testtorun.c>
# Example with blowfish:
./compile.sh blowfish.c
```
This will automatically compile the pass, apply the correct transformations before our pass is executed and finally execute the generate program. In the build folder, after running the script the following files appear:
- `output.ll`: IR code with our pass applied
- `output.s`: Assembly file for ARM Cortex M4 architecture
- `filename_exec`: executable
The other `ll` files are the intermediate files needed before `output.ll` is generated.
