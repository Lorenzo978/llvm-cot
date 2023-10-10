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
#include "llvm/Analysis/PostDominators.h"
#include <map>

#include <string>

using namespace llvm;

struct  newBranch {
	llvm::Value* cond;
	llvm::BasicBlock* then;
	llvm::BasicBlock* els;
};

static void printInputsVectorResult(raw_ostream &OutS, const ResultSecret &InputVector, Function &Func);

llvm::AnalysisKey Secret::Key;

static void getAllUsers(llvm::Value* Inst, std::vector<llvm::Value*>& UsersVector);

Secret::Result Secret::generateInputVector(llvm::Function &Func) {


	std::vector<llvm::Value*> inputsVector;
	for(auto arg = Func.arg_begin(); arg != Func.arg_end(); ++arg) {
		getAllUsers(cast<Value>(arg), inputsVector);
	}
  
 	return inputsVector;
}

Secret::Result Secret::run(llvm::Function &Func, llvm::FunctionAnalysisManager &) {
  return generateInputVector(Func);
}  

PreservedAnalyses InputsVectorPrinter::run(Function &Func, FunctionAnalysisManager &FAM) {
  
	auto &inputsVector = FAM.getResult<Secret>(Func);
	
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

static llvm::PHINode* getPhiIndex(std::vector<llvm::Value*>& Rest, llvm::Loop* loop, std::vector<llvm::Value*> inputsVector) {

	if(Rest.empty()) return NULL;
	llvm::Value* Inst = Rest.front();

	if(llvm::PHINode::classof(Inst)) 
	{ 
		llvm::PHINode* phi = cast<PHINode>(Inst);
		for(unsigned j = 0; j < phi->getNumIncomingValues(); j++) {
			if(phi->getIncomingBlock(j) == loop->getLoopPreheader()) {
				if(std::find(inputsVector.begin(), inputsVector.end(), phi->getIncomingValue(j)) 
					!= inputsVector.end()) return phi;
			}
		}
	}
	else {
		for(auto uses = Inst->use_begin(); uses != Inst->use_end(); ++uses) {
			if(std::find(Rest.begin(), Rest.end(), (*uses).get()) == Rest.end()) 						
				Rest.push_back((*uses).get());
		}
	}

	Rest.erase(Rest.begin());

	return getPhiIndex(Rest, loop, inputsVector);
}

static void findArrays(llvm::Loop* Loop, std::vector<llvm::GetElementPtrInst*>& arraysLoop) {

	for(auto bb : Loop->blocks()) {
		for(auto inst = (*bb).begin(); inst != (*bb).end(); ++inst) {

			if(llvm::GetElementPtrInst::classof(&*inst)) 
			{
  					llvm::GetElementPtrInst* ptr = cast<GetElementPtrInst>(&*inst);
  					
					if(llvm::AllocaInst::classof(ptr->getPointerOperand()) && cast<AllocaInst>(ptr->getPointerOperand())->getAllocatedType()->isArrayTy())
						arraysLoop.push_back(ptr);
			}
		}
	}
}

static unsigned minSizeArrays(std::vector<llvm::GetElementPtrInst*> arraysLoop) {
	
	llvm::GetElementPtrInst* ptr = arraysLoop.front();
	unsigned temp = cast<AllocaInst>(ptr->getPointerOperand())->getAllocatedType()->getArrayNumElements();

	for(auto array : arraysLoop) {
		unsigned cur = cast<AllocaInst>(array->getPointerOperand())->getAllocatedType()->getArrayNumElements();
		if(temp > cur) temp = cur; 
	}

	return temp;
}

static void modifyNumCyclesLoops(const ResultSecret &InputVector, Function &Func, std::vector<llvm::Loop*> allLoopsVector) {


	llvm::DominatorTree DT (Func);
	llvm::LoopInfo LI (DT);

	std::vector<llvm::Value*> inputsVector;
	for(auto arg = Func.arg_begin(); arg != Func.arg_end(); ++arg) {
		getAllUsers(cast<Value>(arg), inputsVector);
	}

  	for(auto loop : allLoopsVector) {
  
  		std::vector<llvm::BranchInst*> InputBrs;

  		llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks;
  		loop->getExitBlocks(exitBlocks);
  	
		for(auto inst : inputsVector) {
			
			if(llvm::Instruction::classof(&*inst) && loop->contains(cast<Instruction>(&*inst)) && llvm::BranchInst::classof(&*inst)) {
				
				llvm::BranchInst* br = cast<BranchInst>(&*inst);
				if(br->isConditional() 
					&& (std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(0)) == exitBlocks.end()
						||  std::find(exitBlocks.begin(), exitBlocks.end(), br->getSuccessor(1)) == exitBlocks.end()))
						{
							InputBrs.push_back(br);
						}
			}
		}
  	
  		if(!InputBrs.empty()) {

			std::vector<llvm::GetElementPtrInst*> arraysLoop;

			findArrays(loop, arraysLoop);

			if(!arraysLoop.empty()) {
				unsigned maxSize = minSizeArrays(arraysLoop);

				
				std::map<llvm::BasicBlock*,std::vector<llvm::Value*>> applyBranchLS;
				std::map<llvm::BasicBlock*,std::vector<llvm::Value*>> applyBranchCmp;
				std::map<llvm::BasicBlock*,llvm::CmpInst::Predicate> applyBranchCmpPred;
				std::map<llvm::BasicBlock*,llvm::Value*> applyBranchPhi;
				std::map<llvm::BasicBlock*,llvm::Value*> applyBranchInit;
				std::map<llvm::BasicBlock*,llvm::Value*> applyBranchEnd;

				for(auto array : arraysLoop) {

					for(auto index = array->idx_begin(); index != array->idx_end(); ++index) {

						std::vector<llvm::Value*> UsersVector;
						getAllUsers((*index).get(), UsersVector);

						std::vector<llvm::Value*> tempApplyBranchLS;

						for(auto brB = InputBrs.begin(); brB != InputBrs.end(); ++brB) {

							tempApplyBranchLS.clear();

							if(std::find(UsersVector.begin(), UsersVector.end(), (*brB)->getCondition()) != UsersVector.end())  {
							
								//-----------------------------------------------------------------------------

								std::vector<llvm::Value*> inlineVector = {cast<Instruction>((*brB)->getCondition())->getOperand(0), cast<Instruction>((*brB)->getCondition())->getOperand(1)};
								applyBranchCmp.insert(std::make_pair((*brB)->getParent(), inlineVector));
							
								applyBranchCmpPred.insert(std::make_pair((*brB)->getParent(), cast<ICmpInst>((*brB)->getCondition())->getPredicate()));

								for(auto temp : UsersVector) {
									if(llvm::LoadInst::classof(temp) || llvm::StoreInst::classof(temp)) tempApplyBranchLS.push_back(temp);
								}
								
								applyBranchLS.insert(std::make_pair((*brB)->getParent(), tempApplyBranchLS));

								//-----------------------------------------------------------------------------

								llvm::User* cmp = cast<User>((*brB)->getCondition());
								
								for(unsigned  j = 0; j < cmp->getNumOperands(); j++) {
							
									if(std::find(UsersVector.begin(), UsersVector.end(), cmp->getOperand(j)) == UsersVector.end() 
										&& std::find(inputsVector.begin(), inputsVector.end(), cmp->getOperand(j)) != inputsVector.end()) {
										
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
											
											llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), maxSize - 1);
											cmp->setOperand(j, vsize);
										}
										else {
											
											llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), maxSize);
											cmp->setOperand(j, vsize);
										}
									}
								}
								
								std::vector<llvm::Value*> UsesVector;
								
								for(auto uses = (*index).get()->use_begin(); uses != (*index).get()->use_end(); ++uses)
										UsesVector.push_back((*uses).get());

								llvm::PHINode* phi = getPhiIndex(UsesVector, loop, inputsVector);

								if(phi != NULL) {
									for(unsigned j = 0; j < phi->getNumIncomingValues(); j++) {
										if(phi->getIncomingBlock(j) == loop->getLoopPreheader()) {
											if(std::find(inputsVector.begin(), inputsVector.end(), phi->getIncomingValue(j)) 
												!= inputsVector.end()) {
												
												
												//--------------------------------------------------------

												applyBranchPhi.insert(std::make_pair((*brB)->getParent(), phi));
												applyBranchInit.insert(std::make_pair((*brB)->getParent(), phi->getIncomingValue(j)));

												//-------------------------------------------------------
											

												if(llvm::ICmpInst::isGT(cast<ICmpInst>((*brB)->getCondition())->getPredicate()) 
													|| llvm::ICmpInst::isGE(cast<ICmpInst>((*brB)->getCondition())->getPredicate())) {
												
													llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), maxSize - 1);
													phi->setIncomingValue(j, vsize);
												}
												else {
													llvm::Value* vsize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(Func.getContext()), 0);
													phi->setIncomingValue(j, vsize);
												}
											}
										}
									}
								}

								applyBranchLS.insert(std::make_pair((*brB)->getParent(), tempApplyBranchLS));
							}
						}
					}
				}

				IRBuilder<> builder(Func.getContext());

				for(auto pair = applyBranchLS.begin(); pair != applyBranchLS.end(); ++pair) {

					llvm::BranchInst* br = cast<BranchInst>(pair->first->getTerminator());
					llvm::Instruction* condbr = cast<Instruction>(br->getCondition());

					builder.SetInsertPoint(condbr);

					llvm::Value* LHS = applyBranchCmp.find(pair->first)->second[0];
					llvm::Value* RHS = applyBranchCmp.find(pair->first)->second[1];

					llvm::CmpInst::Predicate Pred = applyBranchCmpPred.find(pair->first)->second;
					llvm::ICmpInst* cmpInst = new llvm::ICmpInst(Pred, LHS, RHS, "");
					llvm::Value* cmp = cast<Value>(cmpInst);

					auto pinit = applyBranchInit.find(pair->first);
					auto pfin = applyBranchEnd.find(pair->first);
					auto pphi= applyBranchPhi.find(pair->first);

					llvm::Value* init = NULL;
					llvm::Value* phi = NULL;

					llvm::Value* cmp1 = NULL;
					llvm::Value* cmp2 = NULL;

					llvm::Value* finalCmp = NULL;

					if(pinit != applyBranchInit.end()) {
						init = pinit->second;
						phi = pphi->second;

						if(llvm::ICmpInst::isGE(cast<ICmpInst>(cmp)->getPredicate()) || llvm::ICmpInst::isGT(cast<ICmpInst>(cmp)->getPredicate())) cmp1 = builder.CreateICmpSLE(phi, init);
						else cmp1 = builder.CreateICmpSGE(phi, init);

						if(pfin != applyBranchEnd.end()) {
							cmp2 = builder.Insert(cmp);
							finalCmp = builder.CreateAnd(cmp1, cmp2);
						}
						else {
							finalCmp = cmp1;
							cmp->deleteValue();
						}
					}
					else if(pfin != applyBranchEnd.end())  {
						finalCmp = builder.Insert(cmp);
					}

					for(auto inst : pair->second) {
						if(llvm::StoreInst::classof(inst)) {
							llvm::StoreInst* store = cast<StoreInst>(inst); 
							builder.SetInsertPoint(condbr);
							llvm::LoadInst* load = builder.CreateAlignedLoad(store->getValueOperand()->getType(), store->getPointerOperand(), store->getAlign(), store->isVolatile());
							llvm::Value* sel = builder.CreateSelect(finalCmp, store->getValueOperand(), load);
							builder.CreateAlignedStore(sel, store->getPointerOperand(), store->getAlign(), store->isVolatile());
							store->eraseFromParent();
						}
						else {
							std::vector<llvm::Value*> loadUsers;
							getAllUsers(inst, loadUsers);

							builder.SetInsertPoint(condbr);

							std::map<llvm::Value*, llvm::Value*> phiToModify;

							for(auto temp : loadUsers) {
								if(llvm::PHINode::classof(temp) && (*loop).contains(cast<Instruction>(temp))) {
									llvm::PHINode* phi = cast<PHINode>(temp);
									for(unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
										if(std::find(loadUsers.begin(), loadUsers.end(), phi->getIncomingValue(i)) != loadUsers.end()) {
											llvm::Value* sel = builder.CreateSelect(finalCmp, phi->getIncomingValue(i), phi);
											phiToModify.insert(std::make_pair(phi->getIncomingValue(i), sel));
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
									else 
									{
										if(llvm::SelectInst::classof(t) && t != tofix->second) {
											llvm::SelectInst* s = cast<SelectInst>(t);
											if(s->getTrueValue() == tofix->first) s->setTrueValue(tofix->second);
											if(s->getFalseValue() == tofix->first) s->setFalseValue(tofix->second);
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

static void recursiveSerialization(llvm::BasicBlock* bb, std::map<llvm::BasicBlock*, newBranch>& serializedCode, std::vector<llvm::BranchInst*>& condBranch, std::vector<llvm::Loop*>& allLoopsVector, llvm::PostDominatorTree& PDT)
{
	llvm::Instruction* bbTerminator = bb->getTerminator();
	//errs() << *bbTerminator << "\n";
	if(serializedCode.find(bb) != serializedCode.end() || llvm::ReturnInst::classof(bbTerminator))
	{
		if(!condBranch.empty())
		{
			do {
				llvm::BranchInst* lastBranch = condBranch.back();
				llvm::DomTreeNodeBase<llvm::BasicBlock>* node = PDT.getNode(lastBranch->getParent());

				if (node) {
					llvm::DomTreeNodeBase<llvm::BasicBlock>* IPDNode = node->getIDom();
					
					if (IPDNode) {

						llvm::BasicBlock* postDom = IPDNode->getBlock();
						if(serializedCode.find(postDom) != serializedCode.end() || llvm::ReturnInst::classof(postDom->getTerminator())) {

							if(lastBranch->getSuccessor(0) != postDom) {
								for(auto pair = serializedCode.begin(); pair != serializedCode.end(); ++pair) {
									if(pair->second.then == postDom)
									{
										newBranch newbr;
											newbr.cond = NULL ;
											newbr.then = lastBranch->getSuccessor(0);
											newbr.els = NULL;
										pair->second = newbr;
										break;
									}
								}

								condBranch.pop_back();
								recursiveSerialization(lastBranch->getSuccessor(0), serializedCode, condBranch, allLoopsVector, PDT);
								break;
							}
							else condBranch.pop_back();
						}
						else if(serializedCode.find(postDom) == serializedCode.end()) {
							bool find = false;

							auto cur = serializedCode.find(lastBranch->getParent());
							do {
								for(auto p = serializedCode.begin(); p != serializedCode.end(); ++p) {
									if(p->first != cur->first && p->second.then == cur->second.then) {
										find = true;
										break;
									}
								}

								if(!find) cur = serializedCode.find(cur->second.then);
								
							} while(!find);

							if(find) {
								newBranch newbr;
									newbr.cond =  lastBranch->getCondition();
									newbr.then = lastBranch->getSuccessor(0);
									newbr.els = cur->second.then;
							
								cur->second = newbr;

								condBranch.pop_back();
								recursiveSerialization(lastBranch->getSuccessor(0), serializedCode, condBranch, allLoopsVector, PDT);
								break;
							}
							else condBranch.pop_back();
						}
					}
				}
			} while(!condBranch.empty());
		}
	}
	else 
	{
		if(llvm::BranchInst::classof(bbTerminator))
		{
			llvm::BranchInst* brTerminator = llvm::cast<BranchInst>(bbTerminator);
			if(brTerminator->isConditional())
			{
				llvm::Loop* bbLoop = NULL;
				for(auto loop : allLoopsVector)
				{
					llvm::BasicBlock* latchBlock = loop->getLoopLatch();

					if (latchBlock == bb) {
						bbLoop = loop;
						break;
					}
				}

				if(bbLoop != NULL)
				{
					llvm::SmallVector<llvm::BasicBlock*, 8> exitBlocks; 
					bbLoop->getExitBlocks(exitBlocks);
					newBranch newbr;
						newbr.cond =  brTerminator->getCondition();
						newbr.then = brTerminator->getSuccessor(0);
						newbr.els = brTerminator->getSuccessor(1);
					serializedCode.insert(std::make_pair(bb, newbr));
					recursiveSerialization(exitBlocks[0], serializedCode, condBranch, allLoopsVector, PDT);
				}
				else
				{
					condBranch.push_back(brTerminator);
					newBranch newbr;
						newbr.cond = NULL;
						newbr.then = brTerminator->getSuccessor(1);
						newbr.els = NULL;
					serializedCode.insert(std::make_pair(bb, newbr));
					recursiveSerialization(brTerminator->getSuccessor(1), serializedCode, condBranch, allLoopsVector, PDT);
				}

			}
			else
			{
				newBranch newbr;
						newbr.cond = NULL;
						newbr.then = brTerminator->getSuccessor(0);
						newbr.els = NULL;
				serializedCode.insert(std::make_pair(bb, newbr));
				recursiveSerialization(brTerminator->getSuccessor(0), serializedCode, condBranch, allLoopsVector, PDT);
			}
		}
	}
}

static void generateSelect(std::map<std::vector<llvm::BasicBlock*>, llvm::BasicBlock*>& commonDomMap, std::vector<llvm::SelectInst*>& selects, std::map<std::vector<llvm::BasicBlock*>, std::vector<llvm::Value*>>& pairsValue, llvm::DominatorTree& DT) {

	std::map<llvm::BasicBlock*, llvm::SelectInst*> selectBasicBlock;

	
	while(!commonDomMap.empty())
	{
		auto temp = commonDomMap.begin();

		for(auto pair = commonDomMap.begin(); pair != commonDomMap.end(); ++pair) {
			llvm::DomTreeNodeBase<llvm::BasicBlock>* node1 = DT.getNode(temp->second);
			llvm::DomTreeNodeBase<llvm::BasicBlock>* node2 = DT.getNode(pair->second);

			if(node1 && node2){
				if(DT.dominates(node1, node2)) temp = pair;
			}
		}

		llvm::BranchInst* br = cast<BranchInst>(temp->second->getTerminator());
		auto values = pairsValue.find(temp->first);
		
		llvm::SelectInst* sel;
		if(selectBasicBlock.find(temp->first[0]) == selectBasicBlock.end())
		{
			if(selectBasicBlock.find(temp->first[1]) == selectBasicBlock.end())
			{

				sel = llvm::SelectInst::Create(br->getCondition(), values->second[0], values->second[1]);
				selects.push_back(sel);
			}
			else
			{
				auto v1 = selectBasicBlock.find(temp->first[1]);
				sel = llvm::SelectInst::Create(br->getCondition(), values->second[0], v1->second);
				selects.push_back(sel);
				selectBasicBlock.erase(temp->first[1]);
			}
			selectBasicBlock.insert(std::make_pair(temp->first[0], sel));
			selectBasicBlock.insert(std::make_pair(temp->first[1], sel));
		
		}
		else
		{
			if(selectBasicBlock.find(temp->first[1]) == selectBasicBlock.end())
			{
				auto v0 = selectBasicBlock.find(temp->first[0]);
				sel = llvm::SelectInst::Create(br->getCondition(), v0->second, values->second[1]);
				selects.push_back(sel);
				selectBasicBlock.erase(temp->first[0]);
			
			}
			else
			{
				auto v0 = selectBasicBlock.find(temp->first[0]);
				auto v1 = selectBasicBlock.find(temp->first[1]);
				sel = llvm::SelectInst::Create(br->getCondition(), v0->second, v1->second);
				selects.push_back(sel);
				selectBasicBlock.erase(temp->first[0]);
				selectBasicBlock.erase(temp->first[1]);
			}
			selectBasicBlock.insert(std::make_pair(temp->first[0], sel));
			selectBasicBlock.insert(std::make_pair(temp->first[1], sel));
		}

		std::vector<std::vector<llvm::BasicBlock*>> pairToErase;
		for(auto pair = commonDomMap.begin(); pair != commonDomMap.end(); ++pair) {
			if(temp->second == pair->second) pairToErase.push_back(pair->first);
		}

		for(auto first : pairToErase)
			commonDomMap.erase(first);
	}
}

static void modifyPhis(std::vector<llvm::PHINode*> phis, Function &Func, llvm::DominatorTree& DT) {

	IRBuilder<> builder (Func.getContext());

	std::map<std::vector<llvm::BasicBlock*>, llvm::BasicBlock*> commonDomMap;
	std::map<std::vector<llvm::BasicBlock*>, std::vector<llvm::Value*>> pairsValue;
	std::vector<llvm::SelectInst*> selects;
	for(auto phi : phis) {

		commonDomMap.clear();
		selects.clear();
		unsigned numBlocks = phi->getNumIncomingValues();
		for(unsigned i = 0; i < numBlocks - 1; i ++) {
			for(unsigned j = i + 1; j < numBlocks; j++) {

				llvm::BasicBlock* commonDominatorNode = DT.findNearestCommonDominator(phi->getIncomingBlock(i), phi->getIncomingBlock(j));
				
				if (commonDominatorNode) {
					std::vector<llvm::BasicBlock*> temp;
					temp.push_back(phi->getIncomingBlock(i));
					temp.push_back(phi->getIncomingBlock(j));
					std::vector<llvm::Value*> values;
					values.push_back(phi->getIncomingValue(i));
					values.push_back(phi->getIncomingValue(j));
					commonDomMap.insert(std::make_pair(temp, commonDominatorNode));
					pairsValue.insert(std::make_pair(temp, values));
				}
			}
		}

		generateSelect(commonDomMap, selects,pairsValue, DT);

		builder.SetInsertPoint(phi);
		for(auto sel : selects)
			builder.Insert(sel);
		
		phi->replaceAllUsesWith(selects.back());
		phi->eraseFromParent();
	}

}

static void printInputsVectorResult(raw_ostream &OutS,
                                     const ResultSecret &InputVector, Function &Func) {
										
	int i = 0;
	for(auto bb = Func.begin(); bb != Func.end(); ++bb) {
		std::string name = std::to_string(i);
		(*bb).setName(name);
	}

	llvm::DominatorTree DT (Func);
	llvm::LoopInfo LI (DT);
	llvm::PostDominatorTree PDT (Func);

	std::vector<llvm::BranchInst*> condBranch;
	std::vector<llvm::Loop*> allLoopsVector;
	std::map<llvm::BasicBlock*, newBranch> serializedCode;
	
	for(auto loop = LI.begin(); loop != LI.end(); ++loop)  getAllInnerLoops(*loop, allLoopsVector);

	for(auto loop : allLoopsVector) assert(loop->isLoopSimplifyForm() && "expecting loop in sinplify form: use loop-simplify!");

	recursiveSerialization(&(Func.getEntryBlock()), serializedCode, condBranch, allLoopsVector, PDT);

  	/*for(auto pair = serializedCode.begin(); pair != serializedCode.end(); ++pair) {
		errs() << "\n-------------------------------------------------------\n";
		errs() << *(pair->first) << " -> ";
		if(pair->second.cond != NULL) errs() << "Branch: cond: (" << *(pair->second.cond) << ") \n";
		if(pair->second.then != NULL) errs() << "Branch: then: (" << pair->second.then->getName() << ")\n";
		if(pair->second.els != NULL) errs() << "Branch: else: (" << pair->second.els->getName() << ")\n";
	}*/

	std::vector<llvm::PHINode*> phis;

	for(auto bb = Func.begin(); bb != Func.end(); ++bb) {

		for(auto inst = (*bb).begin(); inst != (*bb).end(); ++inst) {
				if(llvm::PHINode::classof(&*inst)) {

					llvm::PHINode* phi = cast<PHINode>(&*inst);
					bool find = false;
					for(auto block : phi->blocks()) {
						for(auto loop : allLoopsVector) {
							if(loop->isLoopLatch(block)) {
								find = true;
								break;
							}
						}
						if(find) break;
					}
					
					if(!find) phis.push_back(phi);
				}
		}
	}


	if(!phis.empty()) modifyPhis(phis, Func, DT);

	IRBuilder<> builder (Func.getContext());

  	for(auto pair = serializedCode.begin(); pair != serializedCode.end(); ++pair) {

		pair->first->getTerminator()->eraseFromParent();

		builder.SetInsertPoint(pair->first);

		if(pair->second.cond != NULL)
			builder.CreateCondBr(pair->second.cond,pair->second.then,pair->second.els);
		else		
			builder.CreateBr(pair->second.then);
	}

	modifyNumCyclesLoops(InputVector, Func, allLoopsVector); 
}
