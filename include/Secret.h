
#ifndef LLVM_TUTOR_SECRET_H
#define LLVM_TUTOR_SECRET_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------
using ResultSecret = std::vector<llvm::Value*>;


struct Secret : public llvm::AnalysisInfoMixin<Secret> {
  using Result = ResultSecret;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);

  Secret::Result generateInputVector(llvm::Function &F);
  static bool isRequired() { return true; }

private:
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<Secret>;
};


class InputsVectorPrinter : public llvm::PassInfoMixin<InputsVectorPrinter> {
public:
  explicit InputsVectorPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &Func,
                              llvm::FunctionAnalysisManager &FAM);

  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};
#endif
