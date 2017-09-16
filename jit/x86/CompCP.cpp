#if defined(ARCH_X86) || defined(ARCH_AMD64)

#include "mednafen/masmem.h"

#include "jit/MIPSCodeUtils.h"
#include "jit/x86/Jit.h"
#include "jit/x86/RegCache.h"

#define _RS MIPS_GET_RS(op)
#define _RT MIPS_GET_RT(op)
#define _RD MIPS_GET_RD(op)
using namespace MIPSAnalyst;

namespace MIPSComp
{

using namespace Gen;
using namespace X64JitConstants;

void Jit::Comp_Cp0(MIPSOpcode op){
    uint32 type = (op.encoding & 0x03E00000) >> 21;
    switch(type){
        case 0: //MFc0
            JitComp_MF0(op);
            break;
        case 2: //MFc0
            JitComp_CF0(op);
            break;
        case 4: //MFc0
            JitComp_MT0(op);
            break;
        case 6: //MFc0
            JitComp_CT0(op);
            break;
        default: //Who knows?
            ERROR_LOG(COP0, "Unkown opcode in Comp_Cp0: %08x\n", op.encoding);
    }
}

void Jit::JitComp_MF0(MIPSOpcode op){
    //Move rd (CP0) to rt (CPU)
    MOV(32, R(EAX), MIPSSTATE_VAR_ELEM32(CP0.Regs[0],_RD));
    MOV(32, gpr.R(_RT), R(EAX));
}

void Jit::JitComp_CF0(MIPSOpcode op){
    //Move control register rd into CPU register rt (Unused?)
    MOV(32, R(EAX), MIPSSTATE_VAR_ELEM32(CP0.Regs[0],_RD));
    MOV(32, gpr.R(_RT), R(EAX));
}

void Jit::JitComp_MT0(MIPSOpcode op){
    //Move _rt(CPU) t- _rd (CP0)
    MOV(32, R(EAX), gpr.R(_RT));
    MOV(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0],_RD), R(EAX));
}

void Jit::JitComp_CT0(MIPSOpcode op){
    //Move CPUT rt to CP0 rd
    MOV(32, R(EAX), gpr.R(_RT));
    MOV(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0],_RD), R(EAX));
}

void Jit::JitComp_BC0(MIPSOpcode op){ //TODO ??
    
}

void Jit::JitComp_Exception(uint32_t code){
    ABI_CallFunctionCCC((void *)&Exception_Helper, code, js.compilerPC, js.inDelaySlot);
    //We need to set the PC to be the handler
    MOV(32, MIPSSTATE_VAR(pc), R(EAX));
    //Now we need to exit this block, by going back to the dispatcher.
    JMP(outerLoop, true);
}

uint32_t Jit::Exception_Helper(uint32_t code, uint32_t PC, uint32_t inDelaySlot){
    uint32_t handler = 0x80000080;
    if(currentMIPS->CP0.SR & (1 << 22))	// BEV
       handler = 0xBFC00180;
 
    currentMIPS->CP0.EPC = PC;
    //We need to execute the branch again, otherwise we'll miss it
    if(inDelaySlot)
    {
        currentMIPS->CP0.EPC -= 4;
        currentMIPS->CP0.TAR = PC;
    }
    // "Push" IEc and KUc(so that the new IEc and KUc are 0)
    currentMIPS->CP0.SR = (currentMIPS->CP0.SR & ~0x3F) | ((currentMIPS->CP0.SR << 2) & 0x3F);
 
    // Setup cause register
    currentMIPS->CP0.CAUSE &= 0x0000FF00;
    currentMIPS->CP0.CAUSE |= code << 2;
 
    // If EPC was adjusted -= 4 because we are after a branch instruction, set bit 31.
    currentMIPS->CP0.CAUSE |= inDelaySlot << 31;
    currentMIPS->CP0.CAUSE |= inDelaySlot << 30;
   //TODO Check this out?
   //Sets what coprocessor the error is for, (for CP Unusable) maybe not needed?
    //CP0.CAUSE |= (instr << 2) & (0x3 << 28); // CE
 
    return(handler);
}

}

#endif