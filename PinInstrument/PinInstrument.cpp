//===- PinInstrument.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "PinInstrument.h"

using namespace PIMProf;

/* ===================================================================== */
/* PinInstrument */
/* ===================================================================== */

// Because PinInstrument has to be a global variable
// and is dependent on the command line argument,
// we have to use a separate function to initialize it.
void PinInstrument::initialize(int argc, char *argv[])
{
// because we assume sizeof(ADDRINT) = 8
# if !(__GNUC__) || !(__x86_64__)
    errormsg() << "Incompatible system" << std::endl;
    ASSERTX(0);
#endif
    PIN_InitSymbols();

    _cost_package.initialize(argc, argv);
    _storage.initialize(&_cost_package, _cost_package._config_reader);
    _instruction_latency.initialize(&_cost_package, _cost_package._config_reader);
    _memory_latency.initialize(&_storage, &_cost_package, _cost_package._config_reader);
    _cost_solver.initialize(&_cost_package, _cost_package._config_reader);

}

// void PinInstrument::ReadControlFlowGraph(const std::string filename)
// {
//     std::ifstream ifs;
//     ifs.open(filename.c_str());
//     std::string curline;

//     getline(ifs, curline);
//     std::stringstream ss(curline);
//     ss >> _cost_package._bbl_size;
//     _cost_package._bbl_size++; // bbl_size = Largest BBLID + 1
// }


void PinInstrument::instrument()
{
    IMG_AddInstrumentFunction(ImageInstrument, (VOID *)this);
    INS_AddInstrumentFunction(InstructionInstrument, (VOID *)this);
    PIN_AddThreadStartFunction(ThreadStart, (VOID *)this);
    PIN_AddThreadFiniFunction(ThreadFinish, (VOID *)this);
    PIN_AddFiniFunction(FinishInstrument, (VOID *)this);
}

void PinInstrument::simulate()
{
    instrument();
    _instruction_latency.instrument();
    _memory_latency.instrument();

    // Never returns
    PIN_StartProgram();
}

VOID PinInstrument::DoAtROIHead(PinInstrument *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi[threadid] = true;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtROITail(PinInstrument *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi[threadid] = false;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtROIDecisionHead(PinInstrument *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroidecision()) {
        if (!pkg._thread_in_roidecision[threadid])
            pkg._enter_roi_cnt++;
        pkg._thread_in_roidecision[threadid] = true;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtROIDecisionTail(PinInstrument *self, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    if (pkg._command_line_parser.enableroidecision()) {
        if (pkg._thread_in_roidecision[threadid])
            pkg._exit_roi_cnt++;
        pkg._thread_in_roidecision[threadid] = false;
    }
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtAnnotationHead(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);

    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    auto it = pkg._bbl_hash.find(bblhash);
    if (it == pkg._bbl_hash.end()) {
        pkg.initializeNewBBL(bblhash);
        it = pkg._bbl_hash.find(bblhash);
    }
    if (isomp) {
        pkg._in_omp_parallel++;
    }

    /// TODO: commented for testing, change back later

    pkg._bbl_parallelizable[it->second] |= (pkg._in_omp_parallel > 0);

    // overwrite _bbl_parallelizable[] if in spawned worker thread
    if (threadid == 1) {
        pkg._bbl_parallelizable[it->second] = true;
    }

    if (pkg._command_line_parser.enableroidecision() && pkg._thread_in_roidecision[threadid]) {
        pkg._roi_decision[it->second] = CostSite::PIM;
        // pkg._bbl_parallelizable[it->second] = true;
    }

    pkg._thread_bbl_scope[threadid].push(it->second);

#ifdef PIMPROFDEBUG
    pkg._bbl_visit_cnt[it->second]++;
#endif
    // infomsg() << "AnnotationHead: " << pkg._thread_bbl_scope[threadid].top() << " " << it->second << " " << isomp << " " << threadid << " " << pkg._in_omp_parallel << std::endl;

    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtAnnotationTail(PinInstrument *self, ADDRINT bblhash_hi, ADDRINT bblhash_lo, ADDRINT isomp, THREADID threadid)
{
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);

    auto bblhash = UUID(bblhash_hi, bblhash_lo);
    if (isomp) {
        pkg._in_omp_parallel--;
    }
    ASSERTX(pkg._thread_bbl_scope[threadid].top() == pkg._bbl_hash[bblhash]);
    pkg._thread_bbl_scope[threadid].pop();

    // infomsg() << "AnnotationTail: " << pkg._thread_bbl_scope[threadid].top() << " " << pkg._bbl_hash[bblhash] << " " << isomp << " "<< threadid << " " << pkg._in_omp_parallel << std::endl;


    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::DoAtAcceleratorHead(PinInstrument *self)
{
    CostPackage &pkg = self->_cost_package;
    pkg._inAcceleratorFunction = true;
    infomsg() << "see EncodeFrame" << std::endl;
}

VOID PinInstrument::DoAtAcceleratorTail(PinInstrument *self)
{
    CostPackage &pkg = self->_cost_package;
    pkg._inAcceleratorFunction = false;
}

VOID PinInstrument::HandleMagicOP(PinInstrument *self, ADDRINT op, THREADID threadid)
{
    switch(op) {
      case MAGIC_OP_PIMPROFROIBEGIN:
        DoAtROIHead(self, threadid); break;
      case MAGIC_OP_PIMPROFROIEND:
        DoAtROITail(self, threadid); break;
      case MAGIC_OP_PIMPROFROIDECISIONBEGIN:
        DoAtROIDecisionHead(self, threadid); break;
      case MAGIC_OP_PIMPROFROIDECISIONEND:
        DoAtROIDecisionTail(self, threadid); break;
      default:
        errormsg() << "Invalid Magic OP " << op << "." << std::endl;
        ASSERTX(0);
    }
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *void_self)
{
    // find annotator head and tail by their names
    RTN annotator_head = RTN_FindByName(img, PIMProfAnnotationHead.c_str());
    RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotationTail.c_str());
    RTN encode_frame = RTN_FindByName(img, "Encode_frame");

    if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
    {
        // Instrument malloc() to print the input argument value and the return value.
        RTN_Open(annotator_head);
        RTN_InsertCall(
            annotator_head,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotationHead,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationHead
            IARG_THREAD_ID,
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotationTail,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2, // Pass all three function argument PIMProfAnnotationHead as an argument of DoAtAnnotationTail
            IARG_THREAD_ID,
            IARG_END);
        RTN_Close(annotator_tail);
    }
    // TODO: dirty hack, fix later
    if (RTN_Valid(encode_frame)) {
        RTN_Open(encode_frame);
        RTN_InsertCall(
            encode_frame,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAcceleratorHead,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_END);
        RTN_InsertCall(
            encode_frame,
            IPOINT_AFTER,
            (AFUNPTR)DoAtAcceleratorTail,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotationHead
            IARG_END);
        RTN_Close(encode_frame);
    }
}


VOID PinInstrument::InstructionInstrument(INS ins, VOID *void_self)
{
    if (INS_IsXchg(ins) && INS_OperandReg(ins, 0) == LEVEL_BASE::REG_RCX && INS_OperandReg(ins, 1) == LEVEL_BASE::REG_RCX) {
        //info("Instrumenting magic op");
        INS_InsertCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)HandleMagicOP,
            IARG_PTR, void_self,
            IARG_REG_VALUE, REG_ECX,
            IARG_THREAD_ID,
            IARG_END);
    }
}

#include <memory>

VOID PinInstrument::ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *void_self)
{
    PinInstrument *self = (PinInstrument *)void_self;
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexWriteLock(&pkg._thread_count_rwmutex);
    pkg._thread_bbl_scope.push_back(BBLScope());
    if (pkg._command_line_parser.enableroi()) {
        pkg._thread_in_roi.push_back(false);
    }
    else {
        pkg._thread_in_roi.push_back(true);
    }

    if (pkg._command_line_parser.enableroidecision()) {
        pkg._thread_in_roidecision.push_back(false);
    }
    else {
        pkg._thread_in_roidecision.push_back(true);
    }

    pkg._previous_instr.push_back(0);
    std::ofstream *ofs = new std::ofstream;
    std::stringstream ss;
    ss << pkg._thread_count;
    std::string tracefile = "MemTrace.out." + ss.str();
    ofs->open(tracefile.c_str(), std::ofstream::out);
    pkg._trace_file.push_back(ofs);

    pkg._thread_count++;
    infomsg() << "ThreadStart:" << threadid << " " << pkg._thread_count << std::endl;
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
    // PIN_RWMutexReadLock(&pkg._thread_count_rwmutex);
    // PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::ThreadFinish(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *void_self)
{
    PinInstrument *self = (PinInstrument *)void_self;
    CostPackage &pkg = self->_cost_package;
    PIN_RWMutexWriteLock(&pkg._thread_count_rwmutex);
    for (auto i: pkg._trace_file) {
        i->close();
        delete i;
    }
    pkg._thread_count--;
    infomsg() << "ThreadEnd:" << threadid << " " << pkg._thread_count << std::endl;
    PIN_RWMutexUnlock(&pkg._thread_count_rwmutex);
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *void_self)
{
    PinInstrument *self = (PinInstrument *)void_self;
    CostPackage &pkg = self->_cost_package;
    std::ofstream ofs(
        pkg._command_line_parser.outputfile().c_str(),
        std::ofstream::out);
    CostSolver::DECISION decision = self->_cost_solver.PrintSolution(ofs);
    ofs.close();
    infomsg() << "parallel:" << pkg._in_omp_parallel << std::endl;

    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    pkg._data_reuse.print(
        ofs,
        pkg._data_reuse.getRoot());
    ofs.close();

    ofs.open("bblcdf.out", std::ofstream::out);
    // TODO: Need bug fix, cause Pin out of memory error
    self->_cost_solver.PrintAnalytics(ofs);
    self->_instruction_latency.WriteConfig("testconfig.ini");
    ofs.close();

    ofs.open(pkg._command_line_parser.statsfile().c_str(), std::ofstream::out);
    self->_storage.WriteStats(ofs);
    ofs << "Number of times entering ROI: " << pkg._enter_roi_cnt << std::endl;
    ofs << "Number of times exiting ROI: " << pkg._exit_roi_cnt << std::endl;
    self->_cost_solver.PrintAnalytics(ofs);
    ofs.close();
}
