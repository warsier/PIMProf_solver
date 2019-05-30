/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*! @file
 *  This file contains a static and dynamic opcode  mix profiler
 */

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include "control_manager.H"

#include "../LLVMAnalysis/Common.h"
#include "PinInstrument.h"

using namespace CONTROLLER;
using namespace PIMProf;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobConfig(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "c", "",
    "specify config file name");
KNOB<string> KnobControlFlow(
    KNOB_MODE_WRITEONCE,
    "pintool",
    "b", "",
    "specify file name containing control flow graph information");


INT32 Usage(std::ostream &out) {
    out << "This pin tool estimates the performance of the given program on a CPU-PIM configuration."
        << std::endl
        << KNOB_BASE::StringKnobSummary()
        << std::endl;
    return -1;
}

int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage(std::cerr);
    }

    if (std::getenv("PIMPROF_ROOT") == NULL) {
        ASSERT(0, "Environment variable PIMPROF_ROOT not set.");
    }
    string rootdir = std::getenv("PIMPROF_ROOT");
    string configfile = KnobConfig.Value();

    if (configfile == "") {
        configfile = rootdir + "/PinInstrument/defaultconfig.ini";
        std::cerr << "## PIMProf: No config file provided. Using default config file.\n";
    }
    
    InstructionLatency::ReadConfig(configfile);
    MemoryLatency::ReadConfig(configfile);
    CostSolver::ReadConfig(configfile);

    string controlflowfile = KnobControlFlow.Value();
    if (controlflowfile == "") {
        ASSERT(0, "Control flow graph file correpsonding to the input program not provided.");
    }
    CostSolver::ReadControlFlowGraph(controlflowfile);

    IMG_AddInstrumentFunction(PinInstrument::ImageInstrument, 0);

    INS_AddInstrumentFunction(MemoryLatency::InstructionInstrument, 0);
    INS_AddInstrumentFunction(InstructionLatency::InstructionInstrument, 0);

    PIN_AddFiniFunction(MemoryLatency::FinishInstrument, 0);
    PIN_AddFiniFunction(PinInstrument::FinishInstrument, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
