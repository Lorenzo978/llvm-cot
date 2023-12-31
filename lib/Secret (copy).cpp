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


	for (auto i : InputVector)
	{
			if(llvm::BranchInst::classof(i))
			{
				if(cast<BranchInst>(*i).isConditional())
				{
				
					//save references
					llvm::BasicBlock* Then = cast<BranchInst>(*i).getSuccessor(0);
					llvm::BasicBlock* Else = cast<BranchInst>(*i).getSuccessor(1);
					llvm::BasicBlock* End = cast<BranchInst>(*i).getSuccessor(1)->getSingleSuccessor();
					
					//in the case of a single if: the else part of the branch ist is the end block
					if(Then->getSingleSuccessor() == Else) End = Else;
					
					//creation of the new basic blocks
					llvm::BasicBlock* bbCondition = llvm::BasicBlock::Create(Func.getContext(), "", &Func, End);
					llvm::BasicBlock* bb1 = llvm::BasicBlock::Create(Func.getContext(), "", &Func, End);
					llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(Func.getContext(), "", &Func, End);
					
					//in case of a if-else: the branch of else must be set to bbCondition 
					if(Then->getSingleSuccessor() != Else) Else->getTerminator()->setSuccessor(0,bbCondition);
					
					//useing of a builder to fix the connections between the new blocks
					IRBuilder<> builder(bbCondition);
					builder.SetInsertPoint(bbCondition);
					builder.CreateCondBr(cast<BranchInst>(*i).getCondition(), bb1, bb2);
					builder.SetInsertPoint(bb1);
					builder.CreateBr(End);
					builder.SetInsertPoint(bb2);
					builder.CreateBr(End);
					
					//fix phi of the end block
					for(auto &phi : End->phis()) {
					
						for(unsigned k=0; k < phi.getNumIncomingValues();k++)
						{
								if(phi.getIncomingBlock(k) == Then)
									phi.setIncomingBlock(k, bb1);
								
								if(phi.getIncomingBlock(k) == Else)
									phi.setIncomingBlock(k, bb2);
								
								//in case of a sigle if: the input label is the base block
								if(phi.getIncomingBlock(k) == cast<Instruction>(*i).getParent()) {
									if(k==0) phi.setIncomingBlock(k, bb1);
									else phi.setIncomingBlock(k, bb2);
									}
						}
					}
					
					//fix successors of the original blocks
					if(Then->getSingleSuccessor() != Else) Then->getTerminator()->setSuccessor(0,Else);
					else Then->getTerminator()->setSuccessor(0,bbCondition);
					
					llvm::Instruction* firstBranch = llvm::BranchInst::Create(cast<BranchInst>(*i).getSuccessor(0));
					firstBranch->insertAfter(&cast<Instruction>(*i));
					cast<Instruction>(*i).eraseFromParent();
				}
			}
			
	}

  
}

