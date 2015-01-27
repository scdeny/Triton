
#include "pin.H"
#include "Triton.h"


VOID movRegReg(std::string insDis, ADDRINT insAddr, CONTEXT *ctx, REG reg1, REG reg2, INT32 opcode)
{
  if (_analysisStatus == LOCKED)
    return;

  std::stringstream expr, taint;

  UINT64 reg1_ID  = translatePinRegToID(reg1);
  UINT64 reg2_ID  = translatePinRegToID(reg2);
  UINT64 size     = (REG_Size(reg1) * 8) - (REG_Size(reg2) * 8);

  if (symbolicEngine->symbolicReg[reg2_ID] != (UINT64)-1){
    switch(opcode){
      case XED_ICLASS_MOV:
        expr << "#" << std::dec << symbolicEngine->symbolicReg[reg2_ID];
        break;
      case XED_ICLASS_MOVSX:
        expr << "((_ sign_extend " << std::dec << size << ") #" << symbolicEngine->symbolicReg[reg2_ID] << ")";
        break;
      case XED_ICLASS_MOVZX:
        expr << "((_ zero_extend " << std::dec << size << ") #" << symbolicEngine->symbolicReg[reg2_ID] << ")";
        break;
    }
  }
  else{
    switch(opcode){
      case XED_ICLASS_MOV:
        expr << smt2lib_bv(PIN_GetContextReg(ctx, getHighReg(reg2)), REG_Size(reg1));
        break;
      case XED_ICLASS_MOVSX:
        expr << "((_ sign_extend " << std::dec << size << ") " << smt2lib_bv(PIN_GetContextReg(ctx, getHighReg(reg2)), REG_Size(reg2)) << ")";
        break;
      case XED_ICLASS_MOVZX:
        expr << "((_ zero_extend " << std::dec << size << ") " << smt2lib_bv(PIN_GetContextReg(ctx, getHighReg(reg2)), REG_Size(reg2)) << ")";
        break;
    }
  }

    
  symbolicElement *elem = symbolicEngine->newSymbolicElement(expr);
  symbolicEngine->symbolicReg[reg1_ID] = elem->getID();

  if (taintEngine->isRegTainted(reg2_ID))
    taintEngine->taintReg(reg1_ID);
  else
    taintEngine->untaintReg(reg1_ID);

  elem->isTainted = taintEngine->getRegStatus(reg1_ID);

  if (elem->isTainted)
    taint << "#" << symbolicEngine->symbolicReg[reg1_ID] << " is controllable";

  std::cout << boost::format(outputInstruction) % boost::io::group(hex, showbase, insAddr) % insDis % elem->getExpression() % taint.str();

  return;
}


VOID movRegImm(std::string insDis, ADDRINT insAddr, REG reg1, UINT64 imm, INT32 opcode)
{
  if (_analysisStatus == LOCKED)
    return;

  std::stringstream expr, taint;

  UINT64 reg1_ID = translatePinRegToID(reg1);

  expr << smt2lib_bv(imm, REG_Size(reg1));
   
  symbolicElement *elem = symbolicEngine->newSymbolicElement(expr);
  symbolicEngine->symbolicReg[reg1_ID] = elem->getID();

  taintEngine->untaintReg(reg1_ID);

  std::cout << boost::format(outputInstruction) % boost::io::group(hex, showbase, insAddr) % insDis % elem->getExpression() % taint.str();

  return;
}


VOID movRegMem(std::string insDis, ADDRINT insAddr, REG reg1, UINT64 mem, UINT32 readSize, INT32 opcode)
{
  if (_analysisStatus == LOCKED)
    return;

  std::list<UINT64>::iterator i;
  std::stringstream expr, taint;

  UINT64 reg1_ID  = translatePinRegToID(reg1);
  UINT64 size     = (REG_Size(reg1) * 8) - (readSize * 8);

  if (symbolicEngine->isMemoryReference(mem) != -1){
    switch(opcode){
      case XED_ICLASS_MOV:
        expr << "#" << std::dec << symbolicEngine->isMemoryReference(mem);
        break;
      case XED_ICLASS_MOVSX:
        expr << "((_ sign_extend " << std::dec << size << ") #" << std::dec << symbolicEngine->isMemoryReference(mem) << ")";
        break;
      case XED_ICLASS_MOVZX:
        expr << "((_ zero_extend " << std::dec << size << ") #" << std::dec << symbolicEngine->isMemoryReference(mem) << ")";
        break;
    }
  }
  else{

    if (taintEngine->isMemoryTainted(mem)){
      UINT64 symVarID = symbolicEngine->getUniqueSymVarID();
      switch(opcode){
        case XED_ICLASS_MOV:
          expr << "SymVar_" << std::dec << symVarID;
          break;
        case XED_ICLASS_MOVSX:
          expr << "((_ sign_extend " << std::dec << size << ") " << "SymVar_" << std::dec << symVarID << ")";
          break;
        case XED_ICLASS_MOVZX:
          expr << "((_ zero_extend " << std::dec << size << ") " << "SymVar_" << std::dec << symVarID << ")";
          break;
      } 
      symbolicEngine->addSmt2LibVarDecl(symVarID, readSize);
      symbolicEngine->addSymVarMemoryReference(mem, symVarID);
    }
    else {
      switch(opcode){
        case XED_ICLASS_MOV:
          expr << smt2lib_bv(derefMem(mem, readSize), readSize);
          break;
        case XED_ICLASS_MOVSX:
          expr << "((_ sign_extend " << std::dec << size << ") " << smt2lib_bv(derefMem(mem, readSize), readSize) << ")";
          break;
        case XED_ICLASS_MOVZX:
          expr << "((_ zero_extend " << std::dec << size << ") " << smt2lib_bv(derefMem(mem, readSize), readSize) << ")";
          break;
      }
    }
  }
    
  symbolicElement *elem = symbolicEngine->newSymbolicElement(expr);
  symbolicEngine->symbolicReg[reg1_ID] = elem->getID();
  elem->isTainted = !TAINTED;
  taintEngine->untaintReg(reg1_ID);

  /* Check if the source addr is tainted */
  if (taintEngine->isMemoryTainted(mem)){
      taintEngine->taintReg(reg1_ID);
      elem->isTainted = TAINTED;
  }

  if (elem->isTainted)
    taint << "#" << symbolicEngine->symbolicReg[reg1_ID] << " is controllable";

  std::cout << boost::format(outputInstruction) % boost::io::group(hex, showbase, insAddr) % insDis % elem->getExpression() % taint.str();

  return;
}


VOID movMemReg(std::string insDis, ADDRINT insAddr, CONTEXT *ctx, REG reg1, UINT64 mem, UINT32 writeSize, INT32 opcode)
{
  if (_analysisStatus == LOCKED)
    return;

  std::list<UINT64>::iterator i;
  std::stringstream expr, taint;

  UINT64 reg1_ID = translatePinRegToID(reg1);

  if (symbolicEngine->symbolicReg[reg1_ID] != (UINT64)-1)
    expr << "#" << symbolicEngine->symbolicReg[reg1_ID];
  else 
    expr << smt2lib_bv(PIN_GetContextReg(ctx, getHighReg(reg1)), writeSize);

  symbolicElement *elem = symbolicEngine->newSymbolicElement(expr);
  elem->isTainted = !TAINTED;

  /* If expr reg is tainted, we taint the memory area */
  if (taintEngine->isRegTainted(reg1_ID)){
    unsigned int offset = 0;
    for (; offset < writeSize ; offset++){
      if (taintEngine->isMemoryTainted(mem+offset) == false)
        taintEngine->addAddress(mem+offset);
    }
    elem->isTainted = TAINTED;
  }

  /* If expr reg is not tainted, we untaint the memory area */
  if (taintEngine->isRegTainted(reg1_ID) == false){
    unsigned int offset = 0;
    for (; offset < writeSize ; offset++){
      taintEngine->removeAddress(mem+offset);
    }
    elem->isTainted = !TAINTED;
  }

  if (elem->isTainted)
    taint << "Memory area " << std::hex << mem << " is controllable";

  /* Link the memory reference to the symbolic expression */
  symbolicEngine->addMemoryReference(mem, elem->getID());

  std::cout << boost::format(outputInstruction) % boost::io::group(hex, showbase, insAddr) % insDis % elem->getExpression() % taint.str();

  return;
}


VOID movMemImm(std::string insDis, ADDRINT insAddr, UINT64 imm, UINT64 mem, UINT32 writeSize, INT32 opcode)
{
  if (_analysisStatus == LOCKED)
    return;

  std::list<UINT64>::iterator i;
  std::stringstream expr, taint;

  expr << smt2lib_bv(imm, writeSize);

  symbolicElement *elem = symbolicEngine->newSymbolicElement(expr);
  elem->isTainted = !TAINTED;

  /* We remove the taint if the memory area is tainted */
  unsigned int offset = 0;
  for (; offset < writeSize ; offset++){
    taintEngine->removeAddress(mem+offset);
  }

  /* Link the memory reference to the symbolic expression */
  symbolicEngine->addMemoryReference(mem, elem->getID());

  std::cout << boost::format(outputInstruction) % boost::io::group(hex, showbase, insAddr) % insDis % elem->getExpression() % taint.str();

  return;
}


