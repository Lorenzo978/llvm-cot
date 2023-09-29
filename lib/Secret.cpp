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
  
  long unsigned int cur = inputsVector.size(), i = 0;
  
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


static void modifyNumCyclesLoop(const ResultSecret &InputVector, Function &Func) {

	 /*
  identifico i paramentri e derivati
  
  identifico la condizione (icmp) -> user
  
  indentivo i branch conditional che appartengono ai loop (di uscita) e sono derivati dai paramentri
  
  prendo gli operandi
  
  trovo l' accesso all, array e il corrispondente indice (vericare che sia un alloca)
  
  per derivate trovo gli users
  
  se questi sono negli operandi modifico la condizione di uscita imponendo la dimensione dell'array
  
  */
  llvm::DominatorTree DT (Func);
  llvm::LoopInfo LI (DT);
  
  
  for(auto loop = LI.begin(); loop != LI.end(); ++loop) {
  
  	std::vector<llvm::BranchInst*> InputBrs;
  	
  	InputBrs.clear();
  	
  	llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks;
  	(*loop)->getExitBlocks(exitBlocks);
  	
  	for(auto inst : InputVector) {
  	
  		if((*loop)->contains(cast<Instruction>(&*inst)) && llvm::BranchInst::classof(&*inst)) {
  			
  			llvm::BranchInst* br = cast<BranchInst>(&*inst);
  			if(br->isConditional() 
  				&& (std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(0)) == exitBlocks.end()
  					||  std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(1)) == exitBlocks.end()))
  				InputBrs.push_back(br);
  		}
  	}
  	
  	if(!InputBrs.empty()) {
  		
  		for(auto bb : (*loop)->blocks()){
  			for(auto inst = (*bb).begin(); inst != (*bb).end(); ++inst) {
  			
  				if(llvm::GetElementPtrInst::classof(&*inst)) {
  					
  					llvm::GetElementPtrInst* ptr = cast<GetElementPtrInst>(&*inst);
  					
  					
  					if(cast<AllocaInst>(ptr->getPointerOperand())->isArrayAllocation()) {
  						
  							for(auto index = ptr->idx_begin(); index != ptr->idx_end(); ++ptr){
  					
  							std::vector<llvm::Value*> UsersVector;
  						
  							UsersVector.clear();
  						
							for(auto user : (*index).get()->users()) UsersVector.push_back(&*user);
					
							long unsigned int size = UsersVector.size(), i = 0;
						
							do {
								size = UsersVector.size();
								llvm::Value* cur = UsersVector[i];
						
								for(auto user : cur->users()) {
									if(std::find(UsersVector.begin(), UsersVector.end(), &*user) == UsersVector.end()) 						
										UsersVector.push_back(&*user);
								}
						
								i++;
					
							} while(size != UsersVector.size() || i < size);
					
					
							for(auto brB = InputBrs.begin(); brB != InputBrs.end(); ++brB) {
						
  								if(std::find(UsersVector.begin(), UsersVector.end(), (*brB)->getCondition()) != UsersVector.end())  {
  								
  									llvm::User* cmp = cast<User>((*brB)->getCondition());
  									for(unsigned  j = 0; j < cmp->getNumOperands(); j++) {
  								
  									if(cmp->getOperand(j) != (*index).get())
  											cmp->setOperand(j, cast<AllocaInst>(ptr->getPointerOperand())->getArraySize());
  									}
  								}
					
							}
  						
  					
  						}
  					}
  				} 
  			
  			}  		
  		}
  	
  	}
  }
  
  
 	
}





static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func) {
  OutS << "=================================================" << "\n";
  OutS << "LLVM-TUTOR: InputVector results\n";
  OutS << "=================================================\n";
  for (auto i : InputVector)
    OutS << *i << "\n";
  OutS << "-------------------------------------------------\n";
 
  modifyNumCyclesLoop(InputVector, Func); 
  
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
  				
  					builder.SetInsertPoint(bb);
	  				llvm::Instruction* end = bb->getTerminator();
					end->eraseFromParent();
					
  					if(queue.size() > 0){
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
  					else builder.CreateBr(tempVector[i+1]);
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


