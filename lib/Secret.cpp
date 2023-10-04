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


static void serializeLoop(const ResultSecret &InputVector, Loop &Loop, Function &Func) {
  
  std::map<llvm::BasicBlock*, llvm::BasicBlock*> bbsMap;
  llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks; 
  std::vector<llvm::BasicBlock*> tempVector;

  llvm::DominatorTree DT (Func);
  llvm::LoopInfo LI (DT);

	for(auto bb = Loop.block_begin(); bb != Loop.block_end(); ++bb) tempVector.push_back(*bb);

  for(auto bb : tempVector) 
  {
	bool flag = false;
	exitBlocks.clear();
	for(auto loop : Loop.getSubLoops()) {

		if(loop->getLoopPreheader() == bb){
			loop->getExitBlocks(exitBlocks);
			llvm::BasicBlock* preHeader = llvm::BasicBlock::Create(Func.getContext(), "", &Func);
			llvm::BasicBlock* exitBlock = llvm::BasicBlock::Create(Func.getContext(), "", &Func);
			bbsMap.insert(std::make_pair(bb, preHeader));
			bbsMap.insert(std::make_pair(exitBlocks[0], exitBlock));
			Loop.addBasicBlockToLoop(preHeader, LI);
			Loop.addBasicBlockToLoop(exitBlock, LI);
			flag = true;
		}
	
		if(loop->contains(bb)) flag=true;

		if(flag) break;
	}
  	
  	if(!flag && bbsMap.find(bb) == bbsMap.end()) {
		llvm::BasicBlock* b =   llvm::BasicBlock::Create(Func.getContext(), "", &Func);
		bbsMap.insert(std::make_pair(bb, b));
		Loop.addBasicBlockToLoop(b, LI);
	}
  }

  IRBuilder<> builder (bbsMap.begin()->second);

  for(auto pair = bbsMap.begin(); pair != bbsMap.end(); ++pair) {

  	exitBlocks.clear();

  	builder.SetInsertPoint(pair->second);
  	
  	bool flag = false;
  	
  	for(auto loop : Loop.getSubLoops()) {
  		if(loop->getLoopPreheader() == pair->first)
  		{
  			loop->getExitBlocks(exitBlocks);
  			flag = true;
			break;
  		}
  	}
  	
  	if(flag) {
		auto bb3 = bbsMap.find(exitBlocks[0]);
		builder.CreateBr(bb3->second);
  	}
  	else {
  	  	for(auto inst = pair->first->begin(); inst != pair->first->end(); ++inst) {
	  		if(llvm::BranchInst::classof(&*inst)) {
	  			if(cast<BranchInst>(&*inst)->isConditional()) {
					llvm::BranchInst* br = cast<BranchInst>(&*inst);
					auto bb1 = bbsMap.find(br->getSuccessor(0));
	  				auto bb2 = bbsMap.find(br->getSuccessor(1));
					llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocksLoop;
					Loop.getExitBlocks(exitBlocksLoop);
					if(std::find(exitBlocksLoop.begin(), exitBlocksLoop.end(), br->getSuccessor(0)) != exitBlocksLoop.end() 
						|| std::find(exitBlocksLoop.begin(), exitBlocksLoop.end(), br->getSuccessor(1)) != exitBlocksLoop.end())
							builder.CreateCondBr(br->getCondition(), br->getSuccessor(0), br->getSuccessor(1));
					else builder.CreateCondBr(br->getCondition(), bb1->second, bb2->second);
	  			}
	  			else {
	  				llvm::BranchInst* br = cast<BranchInst>(&*inst);
	  				auto bb3 = bbsMap.find(br->getSuccessor(0));
	  				builder.CreateBr(bb3->second);
	  			}
	  		}
	  		else if(llvm::PHINode::classof(&*inst) ) {

				if(pair->first != Loop.getHeader()) {
					llvm::PHINode* phi = cast<PHINode>(&*inst);
					llvm::PHINode* pnew =  builder.CreatePHI(phi->getType(), phi->getNumIncomingValues());
					
					for(unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
						auto bbPhi = bbsMap.find(phi->getIncomingBlock(i));
						pnew->addIncoming(phi->getIncomingValue(i),  bbPhi->second);
					}
					
					phi->replaceAllUsesWith(pnew);
				}
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
  }

  std::vector<llvm::BasicBlock*> queue;
  
  exitBlocks.clear();
  auto start = bbsMap.find(Loop.getHeader());
   
  std::map<llvm::Loop*, llvm::SmallVector<llvm::BasicBlock*, 8>> exitBlockMap;
  std::map<llvm::Loop*, unsigned> exitSizeMap;

  for(unsigned i = 0; i < tempVector.size(); i++) {

  	llvm::BasicBlock* bb = tempVector[i];

  	bool flag = false;
	bool isInLoop = false;
  	for(auto loop : Loop.getSubLoops()) {
  		
  		if(bb == loop->getLoopPreheader()) {
  			flag = true;
			llvm::SmallVector<llvm::BasicBlock*, 8> tempExitBlock;
  			loop->getExitBlocks(tempExitBlock);
			exitBlockMap.insert(std::make_pair(loop, tempExitBlock));
			exitSizeMap.insert(std::make_pair(loop, tempExitBlock.size()));
  		}

		if(loop->contains(bb)) isInLoop = true;

		if(flag) break;
  	}
  	
  	if(std::any_of(exitSizeMap.begin(), exitSizeMap.end(),[](const auto& pair) { return pair.second > 0; })) {
  		flag = false;
		llvm::Loop* loopKey;
		llvm::SmallVector<llvm::BasicBlock*, 8> tempExitBlock;
  		for(auto loop : Loop.getSubLoops()) {
  			if(loop->contains(bb) || loop->getLoopPreheader() == bb ) {
				loopKey = loop;
				flag = true; 
			}

			loop->getExitBlocks(tempExitBlock);
  			if(std::find(tempExitBlock.begin(), tempExitBlock.end(), bb) != tempExitBlock.end()){
				loopKey = loop;
				flag = true; 
			}

			if(flag) break;
  		}

  		if(flag) {
	  		auto foundExitBlocks = exitBlockMap.find(loopKey);
  			if(std::find(foundExitBlocks->second.begin(), foundExitBlocks->second.end(), bb) != foundExitBlocks->second.end()) {
  				exitSizeMap[loopKey]--;
  				if(exitSizeMap[loopKey] == 0) {
  				
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
		if(!isInLoop)
		{
			std::vector<llvm::Instruction*> toerase;
		
			for(auto inst = bb->begin(); inst != bb->end(); ++inst) {
				if(llvm::PHINode::classof(&*inst)) {
					if(bb != Loop.getHeader()) toerase.push_back(&*inst);
					else {
						llvm::PHINode* phi = cast<PHINode>(&*inst);
						llvm::SmallVector<llvm::BasicBlock*, 8> tempExitingBlock;
  						Loop.getExitingBlocks(tempExitingBlock);
						for(auto exiting : tempExitingBlock) {
							auto pair = bbsMap.find(exiting);
							phi->replaceIncomingBlockWith(pair->first, pair->second);
						}
					}
				}
				 
			}
			
			for(auto inst : toerase) inst->eraseFromParent();
			
			toerase.clear();	
			builder.SetInsertPoint(bb);
			
			llvm::Instruction* end = bb->getTerminator();

			exitBlocks.clear();
			llvm::SmallVector<llvm::BasicBlock*, 8> exitingBlocks;
			Loop.getExitingBlocks(exitingBlocks);
			
			if(llvm::ReturnInst::classof(end) || i == tempVector.size() - 1 || std::find(exitingBlocks.begin(), exitingBlocks.end(), bb) != exitingBlocks.end())
				builder.CreateBr(start->second);
			else builder.CreateBr(tempVector[i+1]);  
		

			end->eraseFromParent();
  		
		}
  	}
  }
}

static void getAllInnerLoops(llvm::Loop* CurrentLoop, std::vector<llvm::Loop*>& InnerLoops) {
    InnerLoops.push_back(CurrentLoop);

    for (llvm::Loop* SubLoop : CurrentLoop->getSubLoops()) {
        getAllInnerLoops(SubLoop, InnerLoops);
    }
}

static void getAllUsers(llvm::Value* Inst, std::vector<llvm::Value*>& UsersVector) {
	UsersVector.push_back(Inst);

	for(auto temp : Inst->users()) {
		if(std::find(UsersVector.begin(), UsersVector.end(), &*temp) == UsersVector.end()) getAllUsers(&*temp, UsersVector);
	}
}

static void modifyNumCyclesLoops(const ResultSecret &InputVector, Function &Func) {


  llvm::DominatorTree DT (Func);
  llvm::LoopInfo LI (DT);

  std::vector<llvm::Loop*> allLoopsVector;

  for(auto loop = LI.begin(); loop != LI.end(); ++loop)  getAllInnerLoops(*loop, allLoopsVector);

  for(auto loop = allLoopsVector.rbegin(); loop != allLoopsVector.rend(); ++loop) {

	assert((*loop)->isLoopSimplifyForm() && "expecting loop in sinplify form: use loop-simplify!");
	//serializeLoop(InputVector, **loop, Func);
  }

  for(auto loop : allLoopsVector) {
  
  	std::vector<llvm::BranchInst*> InputBrs;
  	
  	InputBrs.clear();
  	
  	llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks;
  	loop->getExitBlocks(exitBlocks);
  	
  	for(auto inst : InputVector) {
  	
  		if(llvm::Instruction::classof(&*inst) && loop->contains(cast<Instruction>(&*inst)) && llvm::BranchInst::classof(&*inst)) {
  			
  			llvm::BranchInst* br = cast<BranchInst>(&*inst);
  			if(br->isConditional() 
  				&& (std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(0)) == exitBlocks.end()
  					||  std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(1)) == exitBlocks.end()))
  				InputBrs.push_back(br);
  		}
  	}

  	std::map<llvm::BranchInst*, llvm::BasicBlock*> guardMap;
  	
  	
  	if(!InputBrs.empty()) {

		std::map<llvm::BasicBlock*,std::vector<llvm::Value*>> applyBranchLS;
		std::map<llvm::BasicBlock*,llvm::Value*> applyBranchCmp;
		std::map<llvm::BasicBlock*,llvm::Value*> applyBranchPhi;
		std::map<llvm::BasicBlock*,llvm::Value*> applyBranchInit;
		std::map<llvm::BasicBlock*,llvm::Value*> applyBranchEnd;
  		
  		for(auto bb : loop->blocks()){

			std::vector<llvm::Value*> tempApplyBranchLS;
  		
  			for(auto inst = (*bb).begin(); inst != (*bb).end(); ++inst) {
  			
  				if(llvm::GetElementPtrInst::classof(&*inst)) {
  					
  					llvm::GetElementPtrInst* ptr = cast<GetElementPtrInst>(&*inst);
  					
					if(llvm::AllocaInst::classof(ptr->getPointerOperand())){

						if(cast<AllocaInst>(ptr->getPointerOperand())->getAllocatedType()->isArrayTy()) {

  							if(loop->isGuarded())
  							{
								llvm::BranchInst* guardBr = loop->getLoopGuardBranch();
								
								for(auto phi = guardBr->getSuccessor(0)->begin(); phi != guardBr->getSuccessor(0)->end(); ++phi) {
									if(llvm::PHINode::classof(&*phi))
									{
										llvm::PHINode* phiNode = cast<PHINode>(&*phi);		
										phiNode->removeIncomingValue(guardBr->getParent());					

									}
								}
								
								for(auto phi = guardBr->getSuccessor(1)->begin(); phi != guardBr->getSuccessor(1)->end(); ++phi) {
									if(llvm::PHINode::classof(&*phi))
									{
										llvm::PHINode* phiNode = cast<PHINode>(&*phi);		
										phiNode->removeIncomingValue(guardBr->getParent());					

									}				
								
								}
								guardMap.insert(std::make_pair(guardBr, loop->getLoopPreheader()));
								
							}
  						
  							for(auto index = ptr->idx_begin(); index != ptr->idx_end(); ++index) {
  					
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

									tempApplyBranchLS.clear();
							
	  								if(std::find(UsersVector.begin(), UsersVector.end(), (*brB)->getCondition()) != UsersVector.end())  {
	  								
										//-----------------------------------------------------------------------------

										applyBranchCmp.insert(std::make_pair((*brB)->getParent(), cast<Instruction>((*brB)->getCondition())->clone()));

										for(auto temp : UsersVector) {
											if(llvm::LoadInst::classof(temp) || llvm::StoreInst::classof(temp)) tempApplyBranchLS.push_back(temp);
										}
										
										applyBranchLS.insert(std::make_pair((*brB)->getParent(), tempApplyBranchLS));
										//-----------------------------------------------------------------------------


										llvm::User* cmp = cast<User>((*brB)->getCondition());
	  									
	  									unsigned arraysize = cast<AllocaInst>(ptr->getPointerOperand())->getAllocatedType()->getArrayNumElements();
	  									
	  									for(unsigned  j = 0; j < cmp->getNumOperands(); j++) {
	  								
	  										if(std::find(UsersVector.begin(), UsersVector.end(), cmp->getOperand(j)) == UsersVector.end() 
	  											&& std::find(InputVector.begin(), InputVector.end(), cmp->getOperand(j)) != InputVector.end()) {
												
												//--------------------------
												applyBranchEnd.insert(std::make_pair((*brB)->getParent(), cmp->getOperand(j)));
												//-------------------------
	  											 
											 	if(llvm::ICmpInst::isGT(cast<ICmpInst>((*brB)->getCondition())->getPredicate())) {
											 	
											 		llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), -1);
											 		cmp->setOperand(j, vsize);
											 	}
												else if(llvm::ICmpInst::isGE(cast<ICmpInst>((*brB)->getCondition())->getPredicate())) {
												
													llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), 0);
													cmp->setOperand(j, vsize);
												}
											 	else if(llvm::ICmpInst::isLT(cast<ICmpInst>((*brB)->getCondition())->getPredicate())) {
											 		
											 		llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), arraysize - 1);
											 		cmp->setOperand(j, vsize);
											 	}
											 	else {
											 		
											 		llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), arraysize);
											 		cmp->setOperand(j, vsize);
											 	}
											}
	  									}
	  									
	  									std::vector<llvm::Value*> UsesVector;
								
										UsesVector.clear();
										
										for(auto uses = (*index).get()->use_begin(); uses != (*index).get()->use_end(); ++uses)
											 UsesVector.push_back((*uses).get());
										
										size = UsesVector.size();
										i = 0;
										
										bool find = false;
								
										do {
											size = UsesVector.size();
											
											llvm::Value* cur = UsersVector[i];
											
											if(llvm::PHINode::classof(UsesVector[i])) { 
											
												llvm::PHINode* phi = cast<PHINode>(UsesVector[i]);
												for(unsigned j = 0; j < phi->getNumIncomingValues(); j++) {
													if(phi->getIncomingBlock(j) == loop->getLoopPreheader()) {
														if(std::find(InputVector.begin(), InputVector.end(), phi->getIncomingValue(j)) 
															!= InputVector.end()) {
															
															//--------------------------------------------------------

															applyBranchPhi.insert(std::make_pair((*brB)->getParent(), phi));
															applyBranchInit.insert(std::make_pair((*brB)->getParent(), phi->getIncomingValue(j)));

															//-------------------------------------------------------


															if(llvm::ICmpInst::isGT(cast<ICmpInst>((*brB)->getCondition())->getPredicate()) 
																|| llvm::ICmpInst::isGE(cast<ICmpInst>((*brB)->getCondition())->getPredicate())) {
														 	
														 		llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), arraysize - 1);
														 		phi->setIncomingValue(j, vsize);
														 	}
														 	else {
														 		llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), 0);
														 		phi->setIncomingValue(j, vsize);
														 	}
															find = true;
														}
													}
												
												}
											}
											
											if(find) break;
											
											for(auto uses = cur->use_begin(); uses != cur->use_end(); ++uses) {
												if(std::find(UsesVector.begin(), UsesVector.end(), (*uses).get()) == UsesVector.end()) 						
													UsersVector.push_back((*uses).get());
													
												
											}
									
											i++;
								
										} while(size != UsesVector.size() || i < size );

										applyBranchLS.insert(std::make_pair((*brB)->getParent(), tempApplyBranchLS));
	  								}
								}
  							}
  						}
					}
  				} 
  			} 	
  		}

		IRBuilder<> builder(Func.getContext());

		for(auto pair = guardMap.begin(); pair != guardMap.end(); ++pair) 
		{
			builder.SetInsertPoint(pair->first->getParent());
			pair->first->eraseFromParent();
			builder.CreateBr(pair->second);
		}

		//--------------------------------------------------------------

		for(auto pair = applyBranchLS.begin(); pair != applyBranchLS.end(); ++pair) {

			errs() << *(pair->first) << "-------------------------------------\n"; 

			
			//end: 1 blocco con branch e cmp di ritorno + nuove phi per valori apply or not

			llvm::BasicBlock* then = llvm::BasicBlock::Create(Func.getContext(), "", &Func);
			llvm::BasicBlock* els = llvm::BasicBlock::Create(Func.getContext(), "", &Func);
			llvm::BasicBlock* end = llvm::BasicBlock::Create(Func.getContext(), "", &Func);

			(*loop).addBasicBlockToLoop(then, LI);
			(*loop).addBasicBlockToLoop(els, LI);
			(*loop).addBasicBlockToLoop(end, LI);

			llvm::BranchInst* br = cast<BranchInst>(pair->first->getTerminator());
			llvm::Value* condbr = br->getCondition();

			builder.SetInsertPoint(pair->first);

			llvm::Value* cmp = applyBranchCmp.find(pair->first)->second;
			auto pinit = applyBranchInit.find(pair->first);
			auto pfin = applyBranchEnd.find(pair->first);
			auto pphi= applyBranchPhi.find(pair->first);

			llvm::Value* init = NULL;
			llvm::Value* phi = NULL;

			llvm::Value* cmp1 = NULL;
			llvm::Value* cmp2 = NULL;

			if(pinit != applyBranchInit.end()) {
				init = pinit->second;
				phi = pphi->second;

				if(llvm::ICmpInst::isGE(cast<ICmpInst>(cmp)->getPredicate()) || llvm::ICmpInst::isGT(cast<ICmpInst>(cmp)->getPredicate())) cmp1 = builder.CreateICmpSLE(phi, init);
				else cmp1 = builder.CreateICmpSGE(phi, init);

				if(pfin != applyBranchEnd.end()) {
					cmp2 = builder.Insert(cmp);
					llvm::Value* cond = builder.CreateAnd(cmp1, cmp2);
					builder.CreateCondBr(cond, then, els);
				}
				else {
					builder.CreateCondBr(cmp1, then, els);
					cmp->deleteValue();
				}
			}
			else if(pfin != applyBranchEnd.end())  {
				cmp2 = builder.Insert(cmp);
				builder.CreateCondBr(cmp2, then, els);
			}

			for(auto inst : pair->second) {
				if(llvm::StoreInst::classof(inst)) {
					llvm::StoreInst* store = cast<StoreInst>(inst); 
					builder.SetInsertPoint(cast<Instruction>(inst));
					llvm::LoadInst* load = builder.CreateAlignedLoad(store->getPointerOperandType(), store->getPointerOperand(), store->getAlign(), store->isVolatile());
					//llvm::LoadInst* load = new llvm::LoadInst(store->getPointerOperandType(), store->getPointerOperand(), "", store->isVolatile(), store->getAlign(), store);
					builder.SetInsertPoint(els);
					builder.CreateAlignedStore(load, store->getPointerOperand(), store->getAlign(), store->isVolatile());
					llvm::StoreInst* clone = cast<StoreInst>(store->clone());
					builder.SetInsertPoint(then);
					builder.Insert(clone);
					store->eraseFromParent();
				}
				else {
					std::vector<llvm::Value*> loadUsers;
					getAllUsers(inst, loadUsers);

					builder.SetInsertPoint(end);

					std::map<llvm::Value*, llvm::Value*> phiToModify;

					for(auto temp : loadUsers) {
						if(llvm::PHINode::classof(temp) && (*loop).contains(cast<Instruction>(temp))) {
							llvm::PHINode* phi = cast<PHINode>(temp);
							for(unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
								if(std::find(loadUsers.begin(), loadUsers.end(), phi->getIncomingValue(i)) != loadUsers.end()) {
									llvm::PHINode* newp = builder.CreatePHI(phi->getType(), 2);
									newp->addIncoming(phi->getIncomingValue(i), then);
									newp->addIncoming(phi, els);
									phiToModify.insert(std::make_pair(phi->getIncomingValue(i), newp));
									break;
								}
							}
						}
					}

					for(auto tofix = phiToModify.begin(); tofix != phiToModify.end(); ++tofix) {
						std::vector<llvm::Value*> tofixUsers;
						getAllUsers(tofix->first, tofixUsers);
						for(auto t : tofixUsers) {
							if(llvm::PHINode::classof(t) && t != tofix->second) {
								llvm::PHINode* p = cast<PHINode>(t);
								for(unsigned i = 0; i < p->getNumIncomingValues(); i++) {
									if(p->getIncomingValue(i) == tofix->first) p->setIncomingValue(i, tofix->second); 
								}
							}
						}
					}
				}
			}

			builder.SetInsertPoint(end);
			llvm::Value* newc = builder.Insert(cast<Instruction>(condbr)->clone());
			llvm::BranchInst* newbr = cast<BranchInst>(br->clone());
			newbr->setCondition(newc);
			builder.Insert(newbr);

			cast<Instruction>(condbr)->eraseFromParent();
			br->eraseFromParent();

			builder.SetInsertPoint(then);
			builder.CreateBr(end);

			builder.SetInsertPoint(els);
			builder.CreateBr(end);

			for(llvm::BasicBlock& b : Func) {
				for(llvm::PHINode& p : b.phis()) {
					p.replaceIncomingBlockWith(pair->first, end);
				}
			}
		}
		//------------------------------------------------------------
  	}
  }
}


static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func) {
 
  modifyNumCyclesLoops(InputVector, Func); 
  
  llvm::DominatorTree DT (Func);
  llvm::LoopInfo LI (DT);
  
  std::map<llvm::BasicBlock*, llvm::BasicBlock*> bbsMap;
  llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks; 
  std::vector<llvm::BasicBlock*> tempVector;
  
  for(auto bb = Func.begin(); bb != Func.end(); ++bb) tempVector.push_back(&*bb);
  
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
		
		if((*loop)->contains(bb)) flag=true;
	}	
  	
  	
  	if(!flag && bbsMap.find(bb) == bbsMap.end()) bbsMap.insert(std::make_pair(bb, llvm::BasicBlock::Create(Func.getContext(), "", &Func)));
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
  }
  
  std::vector<llvm::BasicBlock*> queue;
  
  exitBlocks.clear();
  auto start = bbsMap.find(&(Func.getEntryBlock()));
   
  std::map<llvm::Loop*, llvm::SmallVector<llvm::BasicBlock*, 8>> exitBlockMap;
  std::map<llvm::Loop*, unsigned> exitSizeMap;

  for(unsigned i = 0; i < tempVector.size(); i++) {

  	llvm::BasicBlock* bb = tempVector[i];

  	bool flag = false;
	bool isInLoop = false;
  	for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  		
  		if(bb == (*loop)->getLoopPreheader()) {
  			flag = true;
			llvm::SmallVector<llvm::BasicBlock*, 8> tempExitBlock;
  			(*loop)->getExitBlocks(tempExitBlock);
			exitBlockMap.insert(std::make_pair((*loop), tempExitBlock));
			exitSizeMap.insert(std::make_pair((*loop), tempExitBlock.size()));
  		}

		if((*loop)->contains(bb))
			isInLoop = true;
  	}
  	
  	if(std::any_of(exitSizeMap.begin(), exitSizeMap.end(),[](const auto& pair) { return pair.second > 0; })) {
  		flag = false;
		llvm::Loop* loopKey;
		llvm::SmallVector<llvm::BasicBlock*, 8> tempExitBlock;
  		for(auto loop = LI.begin(); loop != LI.end() && flag != true; ++loop) {
  			if((*loop)->contains(bb) || (*loop)->getLoopPreheader() == bb ) 
			{
				loopKey = *loop;
				flag = true; 
			}

			(*loop)->getExitBlocks(tempExitBlock);
  			if(std::find(tempExitBlock.begin(), tempExitBlock.end(), bb) != tempExitBlock.end())
			{
				loopKey = *loop;
				flag = true; 
			}
  		}

  		if(flag) {
	  		auto foundExitBlocks = exitBlockMap.find(loopKey);
  			if(std::find(foundExitBlocks->second.begin(), foundExitBlocks->second.end(), bb) != foundExitBlocks->second.end()) {
  				exitSizeMap[loopKey]--;
  				if(exitSizeMap[loopKey] == 0) {
  				
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
		if(!isInLoop)
		{
			std::vector<llvm::Instruction*> toerase;
		
			for(auto inst = bb->begin(); inst != bb->end(); ++inst) {
				if(llvm::PHINode::classof(&*inst)) toerase.push_back(&*inst);  
			}
			
			for(auto inst : toerase) inst->eraseFromParent();
			
			toerase.clear();	
			builder.SetInsertPoint(bb);
			
			llvm::Instruction* end = bb->getTerminator();
			
			if(llvm::ReturnInst::classof(end))
				builder.CreateBr(start->second);
			else
			{
				if(i == tempVector.size() - 1) {
					builder.CreateBr(start->second);
				}
				else  builder.CreateBr(tempVector[i+1]);  
			}

			end->eraseFromParent();
  		
		}	
  	}

	errs() << *bb << "\n";
  }
}


