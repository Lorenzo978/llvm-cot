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


	llvm::Instruction* End = Func.getEntryBlock().getTerminator();
	
	llvm::BasicBlock* NewEntry = Func.getEntryBlock().splitBasicBlockBefore(End);
	
	llvm::Instruction* NewEnd = NewEntry->getTerminator();
	
	std::vector<llvm::Instruction*> TempVector;
	
	for (auto Bblock = Func.begin(); Bblock != Func.end(); ++Bblock) {
	
		if(&*Bblock != NewEntry) { 
			for (auto Inst = (*Bblock).begin(); Inst != (*Bblock).end(); ++Inst) {
				if(! (llvm::BranchInst::classof(&*Inst) || llvm::CmpInst::classof(&*Inst) || llvm::PHINode::classof(&*Inst) || llvm::ReturnInst::classof(&*Inst) || 		llvm::SelectInst::classof(&*Inst) || llvm::SwitchInst::classof(&*Inst))) 
					TempVector.push_back(&*Inst);
			}
			
			for (auto i : TempVector) {
				i->moveBefore(NewEnd);
				NewEnd = i;
			}
			
			TempVector.clear();

		}
	}
  
}
