//==============================================================================
// FILE:
//    MBAAdd.cpp
//
// DESCRIPTION:
//    This pass performs a substitution for 8-bit integer add
//    instruction based on this Mixed Boolean-Airthmetic expression:
//      a + b == (((a ^ b) + 2 * (a & b)) * 39 + 23) * 151 + 111
//    See formula (3) in [1].
//
// USAGE:
//    1. Legacy pass manager:
//      $ opt -load <BUILD_DIR>/lib/libMBAAdd.so `\`
//        --legacy-mba-add [-mba-ratio=<ratio>] <bitcode-file>
//      with the optional ratio in the range [0, 1.0].
//    2. New pass maanger:
//      $ opt -load-pass-plugin <BUILD_DIR>/lib/libMBAAdd.so `\`
//        -passes=-"mba-add" <bitcode-file>
//      The command line option is not available for the new PM
//
//  
// [1] "Defeating MBA-based Obfuscation" Ninon Eyrolles, Louis Goubin, Marion
//     Videau
//
// License: MIT
//==============================================================================
#include "MBAAdd.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Ratio.h"

#include <random>

using namespace llvm;

#define DEBUG_TYPE "mba-add"

STATISTIC(SubstCount, "The # of substituted instructions");

// Pass Option declaration
static cl::opt<Ratio, false, llvm::cl::parser<Ratio>> MBARatio{
    "mba-ratio",
    cl::desc("Only apply the mba pass on <ratio> of the candidates"),
    cl::value_desc("ratio"), cl::init(1.), cl::Optional};

//-----------------------------------------------------------------------------
// MBAAdd Implementation
//-----------------------------------------------------------------------------
bool MBAAdd::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;

  // Get a (rather naive) random number generator that will be used to decide
  // whether to replace the current instruction or not.
  // FIXME We should be using 'Module::createRNG' here instead. However, that
  // method requires a pointer to 'Pass' on input and passes
  // for the new pass manager _do_not_ inherit from llvm::Pass. In other words,
  // 'createRNG' cannot be used here and there's no other way of obtaining
  // llvm::RandomNumberGenerator. Last checked for LLVM 8.
  std::mt19937_64 RNG;
  RNG.seed(1234);
  std::uniform_real_distribution<double> Dist(0., 1.);

  // Loop over all instructions in the block. Replacing instructions requires
  // iterators, hence a for-range loop wouldn't be suitable
  for (auto Inst = BB.begin(), IE = BB.end(); Inst != IE; ++Inst) {
  
    LLVM_DEBUG(dbgs() << *Inst << "\n");

    Changed = true;

    // Update the statistics
    ++SubstCount;
  }
  return Changed;
}

PreservedAnalyses MBAAdd::run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
  bool Changed = false;

  for (auto &BB : F) {
    Changed |= runOnBasicBlock(BB);
  }
  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

bool LegacyMBAAdd::runOnFunction(llvm::Function &F) {
  bool Changed = false;

  for (auto &BB : F) {
    Changed |= Impl.runOnBasicBlock(BB);
  }
  return Changed;
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getMBAAddPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mba-add", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "mba-add") {
                    FPM.addPass(MBAAdd());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMBAAddPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
char LegacyMBAAdd::ID = 0;

static RegisterPass<LegacyMBAAdd> X(/*PassArg=*/"legacy-mba-add",
                                    /*Name=*/"MBAAdd",
                                    /*CFGOnly=*/true,
                                    /*is_analysis=*/false);
