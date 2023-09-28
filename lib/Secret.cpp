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
#include "llvm/ADT/SmallVector.h"
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


  /*std::vector<llvm::BasicBlock*> parentsVector;
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
  	
  
  } while(inputsVector.size() != cur);*/
  
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
  
  llvm::DominatorTree DT (Func);
  llvm::LoopInfo LI (DT);
  
  std::map<llvm::BasicBlock*, llvm::BasicBlock*> bbsMap;
  llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks; 
  std::vector<llvm::BasicBlock*> tempVector;
 
  
  for(auto bb = Func.begin(); bb != Func.end(); ++bb) {
  		tempVector.push_back(&*bb);
  }
  
  for(auto bb : tempVector) 
  {
    	bool flag = false;
  	exitBlocks.clear();
    	for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  		if((*loop)->getLoopPreheader() == bb){
  			(*loop)->getExitBlocks(exitBlocks);
  			bbsMap.insert(std::make_pair(bb, llvm::BasicBlock::Create(Func.getContext(), "", &Func)));
  			bbsMap.insert(std::make_pair(exitBlocks[0], llvm::BasicBlock::Create(Func.getContext(), "", &Func)));
  			flag = true;
  		}
  		
  		if((*loop)->contains(bb))
  			flag=true;
  			
  		
  	}
  	
  	
  	if(!flag && bbsMap.find(bb) == bbsMap.end())
  		bbsMap.insert(std::make_pair(bb, llvm::BasicBlock::Create(Func.getContext(), "", &Func)));
  }
  
  IRBuilder<> builder (bbsMap.begin()->second);
  
  for(auto pair = bbsMap.begin(); pair != bbsMap.end(); ++pair) {
  	exitBlocks.clear();

  
  	builder.SetInsertPoint(pair->second);
  	
  	bool flag = false;
  	
  	for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  		if((*loop)->getLoopPreheader() == pair->first)
  		{
  			(*loop)->getExitBlocks(exitBlocks);
  			flag = true;
  		}
  	}	
  	
  	if(flag)
  	{
		auto bb3 = bbsMap.find(exitBlocks[0]);
		builder.CreateBr(bb3->second);
  	}
  	else
  	{
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
	  		else if(llvm::PHINode::classof(&*inst) ) {
	  			llvm::PHINode* phi = cast<PHINode>(&*inst);
	  			llvm::PHINode* pnew =  builder.CreatePHI(phi->getType(), phi->getNumIncomingValues());
	  			
	  			for(unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
	  				auto bbPhi = bbsMap.find(phi->getIncomingBlock(i));
	  				pnew->addIncoming(phi->getIncomingValue(i),  bbPhi->second);
	  			}
	  			
	  			phi->replaceAllUsesWith(pnew);
	  		}
	  		else if(llvm::ReturnInst::classof(&*inst))
	  			builder.CreateRet(cast<ReturnInst>(&*inst)->getReturnValue());
	  		else if(llvm::SwitchInst::classof(&*inst)) {
	  			llvm::SwitchInst* sw = cast<SwitchInst>(&*inst);
	  			auto def =  bbsMap.find(sw->getDefaultDest());
	  			llvm::SwitchInst* nsw = builder.CreateSwitch(sw->getCondition(), def->second, sw->getNumCases());
	  			for(unsigned i = 0; i < sw->getNumSuccessors(); i++) {
	  				llvm::BasicBlock* next = sw->getSuccessor(i);
	  				if(next != def->first) {
	  					auto bb = bbsMap.find(next);
	  					nsw->addCase(sw->findCaseDest(next), bb->second);
	  				}	
	  			}
	  		}
	  	}
  	}
  	

  	//errs() << flag << "\n";
  	//errs() << *(pair->first) << "\n";
  	//errs() << *(pair->second) << "\n";
  }
  
  
  errs() << "-----------------\n";
  
  std::vector<llvm::BasicBlock*> queue;
  unsigned exit = 0;
  
  
  exitBlocks.clear();
   auto start = bbsMap.find(&(Func.getEntryBlock()));
   
  for(unsigned i = 0; i < tempVector.size(); i++) {

  	llvm::BasicBlock* bb = tempVector[i];

  	bool flag = false;
  	for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  		
  		if(bb == (*loop)->getLoopPreheader()) {
  			flag = true;
  			(*loop)->getExitBlocks(exitBlocks);
  			exit = exitBlocks.size();
  		}
  	}
  	
  	if(exit != 0) {
  		flag = false;
  		for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  			if((*loop)->contains(bb) || (*loop)->getLoopPreheader() == bb ) flag = true; 
  		}
  		
  		if(std::find(exitBlocks.begin(), exitBlocks.end(), bb) != exitBlocks.end())
  			flag=true;
  		
  		if(flag) {
  			if(std::find(exitBlocks.begin(), exitBlocks.end(), bb) != exitBlocks.end()) {
  				exit--;
  				if(exit == 0) {
  					if(queue.size() > 0){
  					
	  					builder.SetInsertPoint(bb);
	  					llvm::Instruction* end = bb->getTerminator();
						end->eraseFromParent();
						builder.CreateBr(queue[0]);
						
	  					for(unsigned j = 0; j < queue.size(); j++) {
	  						llvm::BasicBlock* x = queue[j];
	  						builder.SetInsertPoint(x);
	  						end = x->getTerminator();
	  						end->eraseFromParent();
	  						if(j == queue.size() - 1) builder.CreateBr(tempVector[i+1]);
	  						else builder.CreateBr(queue[j+1]);
	  					}
  					}
  				}
  			}
  		}
  		else queue.push_back(bb);
  	}
  	else {
  		std::vector<llvm::Instruction*> toerase;
  	
  		for(auto inst = bb->begin(); inst != bb->end(); ++inst) {
  			if(llvm::PHINode::classof(&*inst)) toerase.push_back(&*inst);  
  		}
  		
  		for(auto inst : toerase) inst->eraseFromParent();
  		
  		toerase.clear();	
		builder.SetInsertPoint(bb);
  		
  		llvm::Instruction* end = bb->getTerminator();
  		end->eraseFromParent();
  		
  		if(i == tempVector.size() - 1) {
  			//auto start = bbsMap.find(&(Func.getEntryBlock()));
  			//errs() << *(start->second) << "\n";
  			builder.CreateBr(start->second);
  		}
  		else  builder.CreateBr(tempVector[i+1]);  
  		
  			
  	}
  	
  	errs() << *bb << "\n";
  }
  
	errs() << "-----------------\n";
  for(auto pair = bbsMap.begin(); pair != bbsMap.end(); ++pair) 
  	errs() << *(pair->second) << "\n";
  
}


