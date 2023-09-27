#include "Secret.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func);

llvm::AnalysisKey Secret::Key;

Secret::Result Secret::generateInputVector(llvm::Function &Func) {

  std::vector<llvm::Value*> inputsVector;
  for(auto arg = Func.arg_begin(); arg != Func.arg_end(); ++arg) {
      inputsVector.push_back(cast<Value>(arg));
  }

  long unsigned int cur = inputsVector.size(), i=0;

  do {
    cur = inputsVector.size();
    llvm::Value* val = inputsVector[i];
    for(auto istr = val->user_begin(); istr != val->user_end(); ++istr) {
      if(std::find(inputsVector.begin(),inputsVector.end(),*istr) == inputsVector.end())
      	inputsVector.push_back(*istr);
    }
    i++;
  } while(inputsVector.size() != cur || i < cur);

  return inputsVector;
}

Secret::Result Secret::run(llvm::Function &Func, llvm::FunctionAnalysisManager &) {
  return generateInputVector(Func);
}  

PreservedAnalyses InputsVectorPrinter::run(Function &Func, FunctionAnalysisManager &FAM) {
  
  auto &inputsVector = FAM.getResult<Secret>(Func);

  OS << "Printing analysis 'Secret Pass' for function '"
     << Func.getName() << "':\n";


  printInputsVectorResult(OS, inputsVector, Func);
  return PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInputVectorPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "InputVector", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {

          PB.registerPipelineParsingCallback(
              [&](StringRef Name, FunctionPassManager &FPM,
                  ArrayRef<PassBuilder::PipelineElement>) {
                if (Name == "print<inputsVector>") {
                  FPM.addPass(InputsVectorPrinter(llvm::errs()));
                  return true;
                }
                return false;
              });

          PB.registerVectorizerStartEPCallback(
              [](llvm::FunctionPassManager &PM,
                 llvm::OptimizationLevel Level) {
                PM.addPass(InputsVectorPrinter(llvm::errs()));
              });

          PB.registerAnalysisRegistrationCallback(
              [](FunctionAnalysisManager &FAM) {
                FAM.registerPass([&] { return Secret(); });
              });
          }
        };
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getInputVectorPluginInfo();
}

//------------------------------------------------------------------------------
// Helper functions - implementation
//------------------------------------------------------------------------------
static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func) {
  OutS << "=================================================" << "\n";
  OutS << "LLVM-TUTOR: InputVector results\n";
  OutS << "=================================================\n";
  for (auto i : InputVector)
    OutS << *i << "\n";
  OutS << "-------------------------------------------------\n";


	std::vector<llvm::BasicBlock*> VisitedBlocks;
	std::vector<llvm::Instruction*> ToRemoveInst;
	llvm::BasicBlock* InitialBlock;
	for (auto i : InputVector)
	{
			if(llvm::BranchInst::classof(i))
			{
				if(cast<BranchInst>(*i).isConditional())
				{	
					VisitedBlocks.push_back(cast<BranchInst>(*i).getSuccessor(0));
					VisitedBlocks.push_back(cast<BranchInst>(*i).getSuccessor(1));
					InitialBlock = llvm::BasicBlock::Create(Func.getContext(), "", &Func, cast<BranchInst>(*i).getSuccessor(0));
					IRBuilder<> builder(InitialBlock);
					builder.SetInsertPoint(InitialBlock);
					while(VisitedBlocks.size()>0)
					{
						llvm::BasicBlock* CurrentBlock = VisitedBlocks.front();
						
						for(auto j = CurrentBlock->begin(); j != CurrentBlock->end(); ++j)
						{
							llvm::Instruction* Inst = reinterpret_cast<llvm::Instruction*>(&j);
							if(!llvm::ICmpInst::classof(Inst) && !llvm::BranchInst::classof(Inst) && !llvm::ReturnInst::classof(Inst) && !llvm::PHINode::classof(Inst))
							{
								ToRemoveInst.push_back(Inst);
							}
						}
						
						for(auto j : ToRemoveInst)
						{
							llvm::Instruction* Inst = j->clone();
							builder.Insert(Inst);
							j->eraseFromParent();
							
						}
						
						ToRemoveInst.clear();
						
						if(llvm::BranchInst::classof(CurrentBlock->getTerminator()))
						{	
						
							for(unsigned j=0; j < CurrentBlock->getTerminator()->getNumSuccessors();j++)
							{
								if(std::find(VisitedBlocks.begin(),VisitedBlocks.end(),CurrentBlock->getTerminator()->getSuccessor(j)) == VisitedBlocks.end())
									VisitedBlocks.push_back(CurrentBlock->getTerminator()->getSuccessor(j));
							}
							
						}
						VisitedBlocks.erase(VisitedBlocks.begin());
					}
					
					builder.CreateCondBr(cast<BranchInst>(*i).getCondition(), cast<BranchInst>(*i).getSuccessor(0), cast<BranchInst>(*i).getSuccessor(1));
					
					llvm::Instruction* firstBranch = llvm::BranchInst::Create(InitialBlock);
					firstBranch->insertAfter(&cast<Instruction>(*i));
					cast<Instruction>(*i).eraseFromParent();
					break;
				}
			}
			
	}

  
}
