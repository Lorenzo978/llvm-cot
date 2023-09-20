#include "Secret.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Casting.h"
#include <algorithm>

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

	/*for(auto BB = Func->begin(); BB != Func->end(); ++BB)
	{
		for(auto Inst = BB->begin(); Inst != BB->end(); ++Inst)
		{
			if(Inst->isConditional())
			{
				if(std::find(inputsVector.begin(),inputsVector.end(),cast<Value>(Istr)) == inputsVector.end())
				{
					
				}
			}
		} 
	}*/

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


	for (auto i : InputVector)
	{
			if(llvm::BranchInst::classof(i))
			{
				if(cast<BranchInst>(*i).isConditional())
				{
					unsigned numSuccessor = cast<BranchInst>(*i).getNumSuccessors();
					llvm::Instruction* firstBranch = llvm::BranchInst::Create(cast<BranchInst>(*i).getSuccessor(0));
					llvm::BasicBlock* bbCondition = llvm::BasicBlock::Create(Func.getContext());
					llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(Func.getContext());
					llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(Func.getContext());
					
					bbCondition->insertInto(&Func, cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor());
					llvm::Instruction* conditionBranch = llvm::BranchInst::Create(bb1,bb2,cast<BranchInst>(*i).getCondition());
					conditionBranch->insertBefore(&(bbCondition->front()));

					bb1->insertInto(&Func, cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor());
					llvm::Instruction* toEndBranch1 = llvm::BranchInst::Create(cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor()); // Create(8)
					toEndBranch1->insertBefore(&(bb1->front()));
					
					bb2->insertInto(&Func, cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor());
					llvm::Instruction* toEndBranch2 = llvm::BranchInst::Create(cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor()); // Create(8)
					toEndBranch2->insertBefore(&(bb2->front()));

					for(auto &phi : cast<BranchInst>(*i).getSuccessor(numSuccessor-1)->getSingleSuccessor()->phis())
					{
						for(unsigned k=0; k < phi.getNumIncomingValues();k++)
						{
								if(phi.getIncomingBlock(k) == cast<BranchInst>(*i).getSuccessor(0))
									phi.setIncomingBlock(k, bb1);
								
								if(phi.getIncomingBlock(k) == cast<BranchInst>(*i).getSuccessor(1))
									phi.setIncomingBlock(k, bb2);
						}
					}


					cast<BranchInst>(*i).getSuccessor(0)->getTerminator()->setSuccessor(0,cast<BranchInst>(*i).getSuccessor(1));
					cast<BranchInst>(*i).getSuccessor(1)->getTerminator()->setSuccessor(0,bbCondition);

					firstBranch->insertAfter(&cast<Instruction>(*i));
					cast<Instruction>(*i).eraseFromParent();
					

					//cast<BranchInst>(*i).getSuccessor(numSuccessor-1).getSingleSuccessor()

					//OutS <<   << "\n";
				}
			}
			
	}

  
}
