#if defined(ARCH_X86) || defined(ARCH_AMD64)

#include "mednafen/masmem.h"

#include "jit/MIPSCodeUtils.h"
#include "jit/x86/Jit.h"
#include "jit/x86/RegCache.h"
#include "mednafen/psx/gte.h"
#include "mednafen/jittimestamp.h"

#define _RS MIPS_GET_RS(op)
#define _RT MIPS_GET_RT(op)
#define _RD MIPS_GET_RD(op)
using namespace MIPSAnalyst;

namespace MIPSComp
{

using namespace Gen;
using namespace X64JitConstants;

void Jit::Comp_Cp0(MIPSOpcode op){
    uint32 type = (op.encoding >> 21) & 0x1f;
    switch(type){
        case 0: //MFc0
            JitComp_MF0(op);
            break;
        case 2: //CFc0
            JitComp_Exception(op, EXCEPTION_COPU);
            break;
        case 4: //MTc0
            JitComp_MT0(op);
            break;
        case 6: //CTc0
            JitComp_Exception(op, EXCEPTION_COPU);
            break;
        default: //Who knows?
            ERROR_LOG(COP0, "Unkown opcode in Comp_Cp0: %08x\n", op.encoding);
    }
}
void Jit::Comp_Cp1(MIPSOpcode op){
    JitComp_Exception(op, EXCEPTION_COPU);
}

void Jit::Comp_Cp2(MIPSOpcode op){
    uint32 type = (op.encoding >> 21) & 0x1f;
    MIPSGPReg rt = _RT;
    if (type >= 0x10 && type <= 0x1F)
    {
       //TODO Compile the instructions themselves.
       ABI_CallFunction((void *)JITTS_update_gte_done);
       ABI_CallFunctionC((void *)GTE_Instruction, op.encoding);
       ABI_CallFunctionR((void *)JITTS_increment_gte_done, EAX);

    }
    uint32 rd = (op.encoding >> 11) & 0x1F;
    gpr.Lock(rt);
    gpr.MapReg(rt, true, true);

    switch(type){
        case 0: //MFc2        
            ABI_CallFunction((void *)JITTS_update_gte_done);
            ABI_CallFunctionC((void *)GTE_ReadDR, rd);
            MOV(32, gpr.R(rt), R(EAX));
            break;
        case 2: //CFc2
            ABI_CallFunction((void *)JITTS_update_gte_done);
            ABI_CallFunctionC((void *)GTE_ReadCR, rd);
            MOV(32, gpr.R(rt), R(EAX));
            break;
        case 4: //MTc2
            ABI_CallFunction((void *)JITTS_update_gte_done);
            ABI_CallFunctionCA((void *)GTE_WriteDR, rd, gpr.R(rt));
            break;
        case 6: //CTc2
            ABI_CallFunction((void *)JITTS_update_gte_done);
            ABI_CallFunctionCA((void *)GTE_WriteCR, rd, gpr.R(rt));
            break;
        default: //Who knows?
            ERROR_LOG(COP2, "Unkown opcode in Comp_Cp2: %08x\n", op.encoding);
    }
    gpr.UnlockAll();
}


void Jit::JitComp_MF0(MIPSOpcode op){
    //Move rd (CP0) to rt (CPU)
    MIPSGPReg rt = _RT;
    gpr.Lock(rt);
    gpr.MapReg(rt, true, true);
    MOV(32, gpr.R(rt), MIPSSTATE_VAR_ELEM32(CP0.Regs[0],_RD));
    gpr.UnlockAll();
}

void Jit::JitComp_MT0(MIPSOpcode op){
    //Move _rt(CPU) t- _rd (CP0)
    MIPSGPReg rt = _RT;
    MIPSGPReg rd = _RD;

    gpr.Lock(rt);
    gpr.MapReg(rt, true, true);
    switch(rd){
    case 7:
        MOV(32, R(EAX), gpr.R(rt));
        AND(32, R(EAX), Imm32(0xFF80003F));
        MOV(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0], rd), R(EAX));
        break;
    case 13:
        AND(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0], rd), Imm32(~(0x3 << 8)));
        MOV(32, R(EAX), gpr.R(rt));
        AND(32, R(EAX), Imm32(0x3 << 8));
        OR(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0], rd), R(EAX));
        break;
    case 12:
        MOV(32, R(EAX), Imm32(~((0x3 << 26) | (0x3 << 23) | (0x3 << 6))));
        AND(32, R(EAX), Imm32(0x3 << 8));
        MOV(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0], rd), R(EAX));
        break;
    default:
        MOV(32, MIPSSTATE_VAR_ELEM32(CP0.Regs[0], rd), gpr.R(rt));
        break;
    }
    gpr.UnlockAll();
}

void Jit::JitComp_BC0(MIPSOpcode op){ //TODO ??
    
}

void caught(uint32_t code, uint32_t instr, uint32_t pc){
    INFO_LOG(EX, "Caught in exception, code = %u, instr = 0x%08x, pc = 0x%08x\n", code, instr, pc);
}

void Jit::JitComp_Exception(MIPSOpcode op, uint32_t code){
    //ABI_PushAllCalleeSavedRegsAndAdjustStack();
    
    ABI_CallFunctionCCC((void *)caught, code, op.encoding, js.compilerPC);
    
    ABI_CallFunctionCCCC((void *)&Exception_Helper, code, js.compilerPC, js.inDelaySlot, op.encoding);
    
    //ABI_PopAllCalleeSavedRegsAndAdjustStack();
    //We need to set the PC to be the handler
    MOV(32, MIPSSTATE_VAR(pc), R(EAX));
    //Now we need to exit this block, by going back to the dispatcher.
    WriteSyscallExit();
}

uint32_t Jit::Exception_Helper(uint32_t code, uint32_t PC, uint32_t inDelaySlot, uint32_t instr){
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
   currentMIPS->CP0.CAUSE |= (instr << 2) & (0x3 << 28); // CE
 
    return(handler);
}

}

#endif