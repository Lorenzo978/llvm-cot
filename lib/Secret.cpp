#include "Secret.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Casting.h"
#include <algorithm>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include <map>

using namespace llvm;

static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func);

llvm::AnalysisKey Secret::Key;

Secret::Result Secret::generateInputVector(llvm::Function &Func) {

  std::vector<llvm::Value*> inputsVector;
  for(auto arg = Func.arg_begin(); arg != Func.arg_end(); ++arg) {
      inputsVector.push_back(cast<Value>(arg));
  }


  std::vector<llvm::BasicBlock*> parentsVector;
  long unsigned int cur, pcur, i, j;
  
  do {
  
  	parentsVector.clear();
  	cur = inputsVector.size();
  	i = 0;
  	
  	do {
    		cur = inputsVector.size();
    		llvm::Value* val = inputsVector[i];
    		for(auto istr = val->user_begin(); istr != val->user_end(); ++istr) {
      			if(std::find(inputsVector.begin(),inputsVector.end(),*istr) == inputsVector.end()) {
      				inputsVector.push_back(*istr);
      				if(llvm::BranchInst::classof(*istr) && cast<BranchInst>(*istr)->isConditional()) {
      					llvm::BasicBlock* bb1 = cast<BranchInst>(*istr)->getSuccessor(0);
      					llvm::BasicBlock* bb2 = cast<BranchInst>(*istr)->getSuccessor(1);
      					if(std::find(parentsVector.begin(), parentsVector.end(), bb1) == parentsVector.end()) 
      						parentsVector.push_back(cast<BranchInst>(*istr)->getSuccessor(0));
      					if(std::find(parentsVector.begin(), parentsVector.end(), bb2) == parentsVector.end()) 
      						parentsVector.push_back(cast<BranchInst>(*istr)->getSuccessor(1));
      				}
      			}
    		}
    		i++;
  	} while(inputsVector.size() != cur || i < cur);
  	
  	
  	pcur = parentsVector.size();
  	j = 0;
  	
  	do {
  		pcur = parentsVector.size();
  		llvm::BasicBlock* bb = parentsVector[j];
  		for(auto inst = bb->begin(); inst != bb->end(); ++inst) {
  	
  			if(!((&*inst)->getType()->isVoidTy()) && std::find(inputsVector.begin(),inputsVector.end(),&*inst) == inputsVector.end()) inputsVector.push_back(&*inst);
  			else {
  				if(llvm::BranchInst::classof(&*inst) && cast<BranchInst>(*inst).isConditional()) {
  		
  					if(std::find(inputsVector.begin(),inputsVector.end(),&*inst) == inputsVector.end()) {
  			
  						if(std::find(parentsVector.begin(),parentsVector.end(),cast<BranchInst>(*inst).getSuccessor(0)) == parentsVector.end()) parentsVector.push_back(cast<BranchInst>(*inst).getSuccessor(0));
  				
  						if(std::find(parentsVector.begin(),parentsVector.end(),cast<BranchInst>(*inst).getSuccessor(1)) == parentsVector.end()) parentsVector.push_back(cast<BranchInst>(*inst).getSuccessor(1));
  					}	
  				}	
  			}
  		}
  		j++;
  	} while(parentsVector.size() != pcur || j < pcur);
  	
  
  } while(inputsVector.size() != cur);
  
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
  
  //llvm::DominatorTree DT (Func);
  //llvm::LoopInfo LI (DT);
  
  std::map<llvm::BasicBlock*, llvm::BasicBlock*> bbsMap;
  
  llvm::BasicBlock* last = &(Func.back());
  
  std::vector<llvm::BasicBlock*> tempVector;
 
  for(auto bb = Func.begin(); bb != Func.end(); ++bb) {
  	tempVector.push_back(&*bb);
  }
  
  for(auto bb : tempVector) bbsMap.insert(std::make_pair(bb, llvm::BasicBlock::Create(Func.getContext(), "", &Func)));
  
  IRBuilder<> builder (bbsMap.begin()->second);
  
  for(auto pair = bbsMap.begin(); pair != bbsMap.end(); ++pair) {
  
  	builder.SetInsertPoint(pair->second);
  	
  	for(auto inst = pair->first->begin(); inst != pair->first->end(); ++inst) {
  		if(llvm::BranchInst::classof(&*inst)) {
  			if(cast<BranchInst>(&*inst)->isConditional()) {
  				llvm::BranchInst* br = cast<BranchInst>(&*inst);
  				auto bb1 = bbsMap.find(br->getSuccessor(0));
  				auto bb2 = bbsMap.find(br->getSuccessor(1));
  				builder.CreateCondBr(br->getCondition(), bb1->second, bb2->second);
  			}
  			else {
  				llvm::BranchInst* br = cast<BranchInst>(&*inst);
  				auto bb3 = bbsMap.find(br->getSuccessor(0));
  				builder.CreateBr(bb3->second);
  			}
  		}
  		else if(llvm::PHINode::classof(&*inst)) {
  			llvm::PHINode* phi = cast<PHINode>(&*inst);
  			llvm::PHINode* pnew =  builder.CreatePHI(phi->getType(), phi->getNumIncomingValues());
  			
  			for(unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
  				auto bbPhi = bbsMap.find(phi->getIncomingBlock(i));
  				pnew->addIncoming(phi->getIncomingValue(i),  bbPhi->second);
  			}
  		}
  		else if(llvm::ReturnInst::classof(&*inst))
  			builder.CreateRet(cast<ReturnInst>(&*inst)->getReturnValue());
  	}
  }
  
  

  
}




/*for (auto i : InputVector)
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
			
	}*/

