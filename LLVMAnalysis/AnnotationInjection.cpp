//===- AnnotationInjection.cpp - Pass that injects BB annotator --*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include <iostream>

#include "MurmurHash3.h"
#include "Common.h"
#include "InjectMagic.h"

using namespace llvm;

namespace PIMProf {

InjectMode Mode = InjectMode::INVALID;

/********************************************************
* Sniper
********************************************************/

void InjectSniperAnnotationCall(Module &M, BasicBlock &BB) {

    /***** generate arguments for the InlineAsm ******/

    // use the content of BB itself as the hash key
    std::string BB_content;
    raw_string_ostream rso(BB_content);
    rso << BB;
    uint64_t bblhash[2];


    MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

    // errs() << "Before annotator injection: " << BB.getName() << "\n";
    // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
    //     (*i).print(errs());
    //     errs() << "\n";
    // }
    // errs() << "\n";
    // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

    std::string funcname = BB.getParent()->getName();
    uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);

    uint64_t control_head = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp);
    uint64_t control_tail = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONTAIL, isomp);

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());

    InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_START, bblhash[1], bblhash[0], beginning);
    InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_END, bblhash[1], bblhash[0], BB.getTerminator());

    // errs() << "After annotator injection: " << BB.getName() << "\n";
    // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
    //     (*i).print(errs());
    //     errs() << "\n";
    // }
    // errs() << "\n";
    // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";
}


void InjectSniperAnnotationCall(Module &M, Function &F) {
    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*F.getEntryBlock().getFirstInsertionPt());
    InjectSimMagic0(M, SNIPER_SIM_CMD_ROI_START, beginning);
    InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_START, 0, 0, beginning);

    // inject an end call before every return instruction
    for (auto &BB : F) {
        for (auto &I : BB) {
            if (isa<ReturnInst>(I)) {
                InjectSimMagic2(M, SNIPER_SIM_PIMPROF_BBL_END, 0, 0, &I);
                InjectSimMagic0(M, SNIPER_SIM_CMD_ROI_END, &I);
            }
        }
    }
}

/********************************************************
* PIMProf
********************************************************/

void InjectPIMProfAnnotationCall(Module &M, BasicBlock &BB) {

    /***** generate arguments for the InlineAsm ******/

    // use the content of BB itself as the hash key
    std::string BB_content;
    raw_string_ostream rso(BB_content);
    rso << BB;
    uint64_t bblhash[2];


    MurmurHash3_x64_128(BB_content.c_str(), BB_content.size(), 0, bblhash);

    // errs() << "Before annotator injection: " << BB.getName() << "\n";
    // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
    //     (*i).print(errs());
    //     errs() << "\n";
    // }
    // errs() << "\n";
    // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";

    std::string funcname = BB.getParent()->getName();
    uint64_t isomp = (funcname.find(OpenMPIdentifier) != std::string::npos);

    uint64_t control_head = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONHEAD, isomp);
    uint64_t control_tail = ControlValue::GetControlValue(MAGIC_OP_ANNOTATIONTAIL, isomp);

    // need to skip all PHIs and LandingPad instructions
    // check the declaration of getFirstInsertionPt()
    Instruction *beginning = &(*BB.getFirstInsertionPt());

    InjectPIMProfMagic(M, bblhash[1], bblhash[0], control_head, beginning);
    InjectPIMProfMagic(M, bblhash[1], bblhash[0], control_tail, BB.getTerminator());

    // errs() << "After annotator injection: " << BB.getName() << "\n";
    // for (auto i = BB.begin(), ie = BB.end(); i != ie; i++) {
    //     (*i).print(errs());
    //     errs() << "\n";
    // }
    // errs() << "\n";
    // errs() << "Hash = " << bblhash[1] << " " << bblhash[0] << "\n";
}

struct AnnotationInjection : public ModulePass {
    static char ID;
    AnnotationInjection() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
        char *injectmodeenv = NULL;
        injectmodeenv = std::getenv(PIMProfInjectModeEnv.c_str());
        assert(injectmodeenv != NULL);

        if (strcmp(injectmodeenv, "SNIPER") == 0) Mode = InjectMode::SNIPER;
        else if (strcmp(injectmodeenv, "PIMPROF") == 0) Mode = InjectMode::PIMPROF;
        else {
            assert(0 && "Invalid environment variable PIMPROFINJECTMODE");
        }

        // inject annotator function to each basic block
        // attach basic block id to terminator

        if (Mode == InjectMode::PIMPROF) {
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectPIMProfAnnotationCall(M, bb);
                }
            }
        }
        if (Mode == InjectMode::SNIPER) {
            for (auto &func : M) {
                for (auto &bb: func) {
                    InjectSniperAnnotationCall(M, bb);
                }
            }
            for (auto &func : M) {
                if (func.getName() == "main") {
                    InjectSniperAnnotationCall(M, func);
                }
            }
        }
        
        // M.print(errs(), nullptr);
        return true;
    }
};

} // namespace PIMProf

char PIMProf::AnnotationInjection::ID = 0;
static RegisterPass<PIMProf::AnnotationInjection> RegisterMyPass(
    "AnnotationInjection", "Inject annotators to uniquely identify each basic block.");

static void loadPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new PIMProf::AnnotationInjection());
}
static RegisterStandardPasses clangtoolLoader_Ox(PassManagerBuilder::EP_OptimizerLast, loadPass);
static RegisterStandardPasses clangtoolLoader_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
