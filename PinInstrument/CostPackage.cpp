//===- CostPackage.cpp - Utils for instrumentation ------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//
#include "CostPackage.h"

using namespace PIMProf;

/* ===================================================================== */
/* CostPackage */
/* ===================================================================== */

void CostPackage::initialize(int argc, char *argv[])
{
    PIN_RWMutexInit(&_thread_count_rwmutex);
    _command_line_parser.initialize(argc, argv);
    _config_reader = ConfigReader(_command_line_parser.configfile());
    // initialize global BBL
    initializeNewBBL(UUID(0, 0));
}

void CostPackage::initializeNewBBL(UUID bblhash) {
    _bbl_hash[bblhash] = _bbl_size;
    _bbl_size++;
    if (_command_line_parser.enableroi()) {
        _bbl_parallelizable.push_back(true);
    }
    else {
        _bbl_parallelizable.push_back(false);
    }
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _bbl_instruction_cost[i].push_back(0);
    }
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _bbl_memory_cost[i].push_back(0);
    }
    if (_command_line_parser.enableroidecision()) {
        _roi_decision.push_back(CostSite::CPU);
    }
    for (UINT32 i = 0; i < MAX_COST_SITE; i++) {
        _bbl_instruction_memory_cost[i].push_back(0);
    }
#ifdef PIMPROFDEBUG
    _bbl_visit_cnt.push_back(0);
    _bbl_instr_cnt.push_back(0);
    _simd_instr_cnt.push_back(0);
    _cache_miss.push_back(0);
#endif
}