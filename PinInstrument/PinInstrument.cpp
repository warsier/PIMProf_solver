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
# if !(__GNUC__) || !(__x86_64__)
    errormsg() << "Incompatible system" << std::endl;
    ASSERTX(0);
#endif
    PIN_InitSymbols();

    _command_line_parser.initialize(argc, argv);
    
    // ReadControlFlowGraph(_command_line_parser.controlflowfile());

    _config_reader = ConfigReader(_command_line_parser.configfile());
    _cost_package.initialize();
    _storage.initialize(&_cost_package, _config_reader);
    _instruction_latency.initialize(&_cost_package, _config_reader);
    _memory_latency.initialize(&_storage, &_cost_package, _config_reader);
    _cost_solver.initialize(&_cost_package, _config_reader);

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
    PIN_AddFiniFunction(PinInstrument::FinishInstrument, (VOID *)this);
}

void PinInstrument::simulate()
{
    instrument();
    _instruction_latency.instrument();
    _memory_latency.instrument();

    // Never returns
    PIN_StartProgram();
}

VOID PinInstrument::DoAtAnnotatorHead(PinInstrument *self, ADDRINT bblhash, ADDRINT isomp)
{
    CostPackage &pkg = self->_cost_package;
    auto it = pkg._BBL_hash.find(bblhash);
    if (it == pkg._BBL_hash.end()) {
        pkg._BBL_hash[bblhash] = pkg._bbl_size;
        it = pkg._BBL_hash.find(bblhash);
        pkg._bbl_size++;
        pkg._inOpenMPRegion.push_back(isomp);
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            pkg._BBL_instruction_cost[i].push_back(0);
        }
        for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
            pkg._BBL_memory_cost[i].push_back(0);
        }
    }
    pkg._bbl_scope.push(it->second);

    // infomsg() << bblid << " " << isomp << std::endl;
}

VOID PinInstrument::DoAtAnnotatorTail(PinInstrument *self, ADDRINT bblhash, ADDRINT isomp)
{
    CostPackage &pkg = self->_cost_package;
    ASSERTX(pkg._bbl_scope.top() == pkg._BBL_hash[bblhash]);
    pkg._bbl_scope.pop();
}

VOID PinInstrument::ImageInstrument(IMG img, VOID *void_self)
{
    // find annotator head and tail by their names
    RTN annotator_head = RTN_FindByName(img, PIMProfAnnotatorHead.c_str());
    RTN annotator_tail = RTN_FindByName(img, PIMProfAnnotatorTail.c_str());

    if (RTN_Valid(annotator_head) && RTN_Valid(annotator_tail))
    {
        // Instrument malloc() to print the input argument value and the return value.
        RTN_Open(annotator_head);
        RTN_InsertCall(
            annotator_head,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorHead,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 0, // Pass the first function argument PIMProfAnnotatorHead as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 1, // Pass the second function argument PIMProfAnnotatorHead as an argument of DoAtAnnotatorHead
            IARG_END);
        RTN_Close(annotator_head);

        RTN_Open(annotator_tail);
        RTN_InsertCall(
            annotator_tail,
            IPOINT_BEFORE,
            (AFUNPTR)DoAtAnnotatorTail,
            IARG_PTR, void_self, // Pass the pointer of bbl_scope as an argument of DoAtAnnotatorHead
            IARG_FUNCARG_CALLSITE_VALUE, 0, // The first argument of DoAtAnnotatorTail
            IARG_FUNCARG_CALLSITE_VALUE, 1, // The second argument of DoAtAnnotatorTail
            IARG_END);
        RTN_Close(annotator_tail);
    }
}

VOID PinInstrument::FinishInstrument(INT32 code, VOID *void_self)
{
    PinInstrument *self = (PinInstrument *)void_self;
    infomsg() << self->_command_line_parser.outputfile().c_str() << std::endl;
    std::ofstream ofs(self->_command_line_parser.outputfile().c_str(), std::ofstream::out);
    CostSolver::DECISION decision = self->_cost_solver.PrintSolution(ofs);
    ofs.close();
    
    ofs.open("BBLBlockCost.out", std::ofstream::out);

    self->_cost_solver.PrintBBLDecisionStat(ofs, decision, false);
    
    ofs.close();
    ofs.open("BBLReuseCost.dot", std::ofstream::out);
    self->_cost_package._data_reuse.print(ofs, self->_cost_package._data_reuse.getRoot());
    ofs.close();
}
