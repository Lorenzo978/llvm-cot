import sys

config.llvm_tools_dir = "/usr/lib/llvm-16/bin"
config.llvm_shlib_ext = ".so"
config.llvm_shlib_dir = "/home/pietro/Scrivania/llvm-cot/build/lib"

import lit.llvm
# lit_config is a global instance of LitConfig
lit.llvm.initialize(lit_config, config)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join("/home/pietro/Scrivania/llvm-cot/build/test")

# Let the main config do the real work.
lit_config.load_config(config, "/home/pietro/Scrivania/llvm-cot/test/lit.cfg.py")
