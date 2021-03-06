//===-- Instruction.cpp - Implement the Instruction class -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Changes Copyright (C) 2011-2012 LunarG, Inc.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Instruction class for the VMCore library.
//
//===----------------------------------------------------------------------===//

#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/LeakDetector.h"
using namespace llvm;

Instruction::Instruction(const Type *ty, unsigned it, Use *Ops, unsigned NumOps,
                         Instruction *InsertBefore)
  : User(ty, Value::InstructionVal + it, Ops, NumOps), Parent(0) {
  // Make sure that we get added to a basicblock
  LeakDetector::addGarbageObject(this);

  // If requested, insert this instruction into a basic block...
  if (InsertBefore) {
    assert(InsertBefore->getParent() &&
           "Instruction to insert before is not in a basic block!");
    InsertBefore->getParent()->getInstList().insert(InsertBefore, this);
  }
}

Instruction::Instruction(const Type *ty, unsigned it, Use *Ops, unsigned NumOps,
                         BasicBlock *InsertAtEnd)
  : User(ty, Value::InstructionVal + it, Ops, NumOps), Parent(0) {
  // Make sure that we get added to a basicblock
  LeakDetector::addGarbageObject(this);

  // append this instruction into the basic block
  assert(InsertAtEnd && "Basic block to append to may not be NULL!");
  InsertAtEnd->getInstList().push_back(this);
}


// Out of line virtual method, so the vtable, etc has a home.
Instruction::~Instruction() {
  assert(Parent == 0 && "Instruction still linked in the program!");
  if (hasMetadataHashEntry())
    clearMetadataHashEntries();
}


void Instruction::setParent(BasicBlock *P) {
  if (getParent()) {
    if (!P) LeakDetector::addGarbageObject(this);
  } else {
    if (P) LeakDetector::removeGarbageObject(this);
  }

  Parent = P;
}

void Instruction::removeFromParent() {
  getParent()->getInstList().remove(this);
}

void Instruction::eraseFromParent() {
  getParent()->getInstList().erase(this);
}

/// insertBefore - Insert an unlinked instructions into a basic block
/// immediately before the specified instruction.
void Instruction::insertBefore(Instruction *InsertPos) {
  InsertPos->getParent()->getInstList().insert(InsertPos, this);
}

/// insertAfter - Insert an unlinked instructions into a basic block
/// immediately after the specified instruction.
void Instruction::insertAfter(Instruction *InsertPos) {
  InsertPos->getParent()->getInstList().insertAfter(InsertPos, this);
}

/// moveBefore - Unlink this instruction from its current basic block and
/// insert it into the basic block that MovePos lives in, right before
/// MovePos.
void Instruction::moveBefore(Instruction *MovePos) {
  MovePos->getParent()->getInstList().splice(MovePos,getParent()->getInstList(),
                                             this);
}


const char *Instruction::getOpcodeName(unsigned OpCode) {
  switch (OpCode) {
  // Terminators
  case Ret:    return "ret";
  case Br:     return "br";
  case Switch: return "switch";
  case IndirectBr: return "indirectbr";
  case Invoke: return "invoke";
  case Unwind: return "unwind";
  case Unreachable: return "unreachable";

  // Standard binary operators...
  case Add: return "add";
  case FAdd: return "fadd";
  case Sub: return "sub";
  case FSub: return "fsub";
  case Mul: return "mul";
  case FMul: return "fmul";
  case UDiv: return "udiv";
  case SDiv: return "sdiv";
  case FDiv: return "fdiv";
  case URem: return "urem";
  case SRem: return "srem";
  case FRem: return "frem";

  // Logical operators...
  case And: return "and";
  case Or : return "or";
  case Xor: return "xor";

  // Memory instructions...
  case Alloca:        return "alloca";
  case Load:          return "load";
  case Store:         return "store";
  case GetElementPtr: return "getelementptr";

  // Convert instructions...
  case Trunc:     return "trunc";
  case ZExt:      return "zext";
  case SExt:      return "sext";
  case FPTrunc:   return "fptrunc";
  case FPExt:     return "fpext";
  case FPToUI:    return "fptoui";
  case FPToSI:    return "fptosi";
  case UIToFP:    return "uitofp";
  case SIToFP:    return "sitofp";
  case IntToPtr:  return "inttoptr";
  case PtrToInt:  return "ptrtoint";
  case BitCast:   return "bitcast";

  // Other instructions...
  case ICmp:           return "icmp";
  case FCmp:           return "fcmp";
  case PHI:            return "phi";
  case Select:         return "select";
  case Call:           return "call";
  case Shl:            return "shl";
  case LShr:           return "lshr";
  case AShr:           return "ashr";
  case VAArg:          return "va_arg";
  case ExtractElement: return "extractelement";
  case InsertElement:  return "insertelement";
  case ShuffleVector:  return "shufflevector";
  case ExtractValue:   return "extractvalue";
  case InsertValue:    return "insertvalue";

  default: return "<Invalid operator> ";
  }

  return 0;
}

/// isIdenticalTo - Return true if the specified instruction is exactly
/// identical to the current one.  This means that all operands match and any
/// extra information (e.g. load is volatile) agree.
bool Instruction::isIdenticalTo(const Instruction *I) const {
  return isIdenticalToWhenDefined(I) &&
         SubclassOptionalData == I->SubclassOptionalData;
}

/// isIdenticalToWhenDefined - This is like isIdenticalTo, except that it
/// ignores the SubclassOptionalData flags, which specify conditions
/// under which the instruction's result is undefined.
bool Instruction::isIdenticalToWhenDefined(const Instruction *I) const {
  if (getOpcode() != I->getOpcode() ||
      getNumOperands() != I->getNumOperands() ||
      getType() != I->getType())
    return false;

  // We have two instructions of identical opcode and #operands.  Check to see
  // if all operands are the same.
  for (unsigned i = 0, e = getNumOperands(); i != e; ++i)
    if (getOperand(i) != I->getOperand(i))
      return false;

  // Check special state that is a part of some instructions.
  if (const LoadInst *LI = dyn_cast<LoadInst>(this))
    return LI->isVolatile() == cast<LoadInst>(I)->isVolatile() &&
           LI->getAlignment() == cast<LoadInst>(I)->getAlignment();
  if (const StoreInst *SI = dyn_cast<StoreInst>(this))
    return SI->isVolatile() == cast<StoreInst>(I)->isVolatile() &&
           SI->getAlignment() == cast<StoreInst>(I)->getAlignment();
  if (const CmpInst *CI = dyn_cast<CmpInst>(this))
    return CI->getPredicate() == cast<CmpInst>(I)->getPredicate();
  if (const CallInst *CI = dyn_cast<CallInst>(this))
    return CI->isTailCall() == cast<CallInst>(I)->isTailCall() &&
           CI->getCallingConv() == cast<CallInst>(I)->getCallingConv() &&
           CI->getAttributes() == cast<CallInst>(I)->getAttributes();
  if (const InvokeInst *CI = dyn_cast<InvokeInst>(this))
    return CI->getCallingConv() == cast<InvokeInst>(I)->getCallingConv() &&
           CI->getAttributes() == cast<InvokeInst>(I)->getAttributes();
  if (const InsertValueInst *IVI = dyn_cast<InsertValueInst>(this)) {
    if (IVI->getNumIndices() != cast<InsertValueInst>(I)->getNumIndices())
      return false;
    for (unsigned i = 0, e = IVI->getNumIndices(); i != e; ++i)
      if (IVI->idx_begin()[i] != cast<InsertValueInst>(I)->idx_begin()[i])
        return false;
    return true;
  }
  if (const ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(this)) {
    if (EVI->getNumIndices() != cast<ExtractValueInst>(I)->getNumIndices())
      return false;
    for (unsigned i = 0, e = EVI->getNumIndices(); i != e; ++i)
      if (EVI->idx_begin()[i] != cast<ExtractValueInst>(I)->idx_begin()[i])
        return false;
    return true;
  }

  return true;
}

// isSameOperationAs
// This should be kept in sync with isEquivalentOperation in
// lib/Transforms/IPO/MergeFunctions.cpp.
bool Instruction::isSameOperationAs(const Instruction *I) const {
  if (getOpcode() != I->getOpcode() ||
      getNumOperands() != I->getNumOperands() ||
      getType() != I->getType())
    return false;

  // We have two instructions of identical opcode and #operands.  Check to see
  // if all operands are the same type
  for (unsigned i = 0, e = getNumOperands(); i != e; ++i)
    if (getOperand(i)->getType() != I->getOperand(i)->getType())
      return false;

  // Check special state that is a part of some instructions.
  if (const LoadInst *LI = dyn_cast<LoadInst>(this))
    return LI->isVolatile() == cast<LoadInst>(I)->isVolatile() &&
           LI->getAlignment() == cast<LoadInst>(I)->getAlignment();
  if (const StoreInst *SI = dyn_cast<StoreInst>(this))
    return SI->isVolatile() == cast<StoreInst>(I)->isVolatile() &&
           SI->getAlignment() == cast<StoreInst>(I)->getAlignment();
  if (const CmpInst *CI = dyn_cast<CmpInst>(this))
    return CI->getPredicate() == cast<CmpInst>(I)->getPredicate();
  if (const CallInst *CI = dyn_cast<CallInst>(this))
    return CI->isTailCall() == cast<CallInst>(I)->isTailCall() &&
           CI->getCallingConv() == cast<CallInst>(I)->getCallingConv() &&
           CI->getAttributes() == cast<CallInst>(I)->getAttributes();
  if (const InvokeInst *CI = dyn_cast<InvokeInst>(this))
    return CI->getCallingConv() == cast<InvokeInst>(I)->getCallingConv() &&
           CI->getAttributes() ==
             cast<InvokeInst>(I)->getAttributes();
  if (const InsertValueInst *IVI = dyn_cast<InsertValueInst>(this)) {
    if (IVI->getNumIndices() != cast<InsertValueInst>(I)->getNumIndices())
      return false;
    for (unsigned i = 0, e = IVI->getNumIndices(); i != e; ++i)
      if (IVI->idx_begin()[i] != cast<InsertValueInst>(I)->idx_begin()[i])
        return false;
    return true;
  }
  if (const ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(this)) {
    if (EVI->getNumIndices() != cast<ExtractValueInst>(I)->getNumIndices())
      return false;
    for (unsigned i = 0, e = EVI->getNumIndices(); i != e; ++i)
      if (EVI->idx_begin()[i] != cast<ExtractValueInst>(I)->idx_begin()[i])
        return false;
    return true;
  }

  return true;
}

/// isUsedOutsideOfBlock - Return true if there are any uses of I outside of the
/// specified block.  Note that PHI nodes are considered to evaluate their
/// operands in the corresponding predecessor block.
bool Instruction::isUsedOutsideOfBlock(const BasicBlock *BB) const {
  for (const_use_iterator UI = use_begin(), E = use_end(); UI != E; ++UI) {
    // PHI nodes uses values in the corresponding predecessor block.  For other
    // instructions, just check to see whether the parent of the use matches up.
    const User *U = *UI;
    const PHINode *PN = dyn_cast<PHINode>(U);
    if (PN == 0) {
      if (cast<Instruction>(U)->getParent() != BB)
        return true;
      continue;
    }

    if (PN->getIncomingBlock(UI) != BB)
      return true;
  }
  return false;
}

/// mayReadFromMemory - Return true if this instruction may read memory.
///
bool Instruction::mayReadFromMemory() const {
  switch (getOpcode()) {
  default: return false;
  case Instruction::VAArg:
  case Instruction::Load:
    return true;
  case Instruction::Call:
    return !cast<CallInst>(this)->doesNotAccessMemory();
  case Instruction::Invoke:
    return !cast<InvokeInst>(this)->doesNotAccessMemory();
  case Instruction::Store:
    return cast<StoreInst>(this)->isVolatile();
  }
}

/// mayWriteToMemory - Return true if this instruction may modify memory.
///
bool Instruction::mayWriteToMemory() const {
  switch (getOpcode()) {
  default: return false;
  case Instruction::Store:
  case Instruction::VAArg:
    return true;
  case Instruction::Call:
    return !cast<CallInst>(this)->onlyReadsMemory();
  case Instruction::Invoke:
    return !cast<InvokeInst>(this)->onlyReadsMemory();
  case Instruction::Load:
    return cast<LoadInst>(this)->isVolatile();
  }
}

/// mayThrow - Return true if this instruction may throw an exception.
///
bool Instruction::mayThrow() const {
  if (const CallInst *CI = dyn_cast<CallInst>(this))
    return !CI->doesNotThrow();
  return false;
}

/// isAssociative - Return true if the instruction is associative:
///
///   Associative operators satisfy:  x op (y op z) === (x op y) op z
///
/// In LLVM, the Add, Mul, And, Or, and Xor operators are associative.
///
bool Instruction::isAssociative(unsigned Opcode) {
  return Opcode == And || Opcode == Or || Opcode == Xor ||
         Opcode == Add || Opcode == Mul;
}

/// isCommutative - Return true if the instruction is commutative:
///
///   Commutative operators satisfy: (x op y) === (y op x)
///
/// In LLVM, these are the associative operators, plus SetEQ and SetNE, when
/// applied to any type.
///
bool Instruction::isCommutative(unsigned op) {
  switch (op) {
  case Add:
  case FAdd:
  case Mul:
  case FMul:
  case And:
  case Or:
  case Xor:
    return true;
  default:
    return false;
  }
}

bool Instruction::isSafeToSpeculativelyExecute() const {
  for (unsigned i = 0, e = getNumOperands(); i != e; ++i)
    if (Constant *C = dyn_cast<Constant>(getOperand(i)))
      if (C->canTrap())
        return false;

  switch (getOpcode()) {
  default:
    return true;
  case UDiv:
  case URem: {
    // x / y is undefined if y == 0, but calcuations like x / 3 are safe.
    ConstantInt *Op = dyn_cast<ConstantInt>(getOperand(1));
    return Op && !Op->isNullValue();
  }
  case SDiv:
  case SRem: {
    // x / y is undefined if y == 0, and might be undefined if y == -1,
    // but calcuations like x / 3 are safe.
    ConstantInt *Op = dyn_cast<ConstantInt>(getOperand(1));
    return Op && !Op->isNullValue() && !Op->isAllOnesValue();
  }
  case Load: {
    const LoadInst *LI = cast<LoadInst>(this);
    if (LI->isVolatile())
      return false;
    return LI->getPointerOperand()->isDereferenceablePointer();
  }
  case Call:
    if (const IntrinsicInst *II = dyn_cast<IntrinsicInst>(this)) {
      switch (II->getIntrinsicID()) {
      case Intrinsic::gla_abs:
      case Intrinsic::gla_addCarry:
      case Intrinsic::gla_all:
      case Intrinsic::gla_any:
      case Intrinsic::gla_bitCount:
      case Intrinsic::gla_bitFieldInsert:
      case Intrinsic::gla_bitReverse:
      // case Intrinsic::gla_discard:
      // case Intrinsic::gla_discardConditional:
      case Intrinsic::gla_fAbs:
      case Intrinsic::gla_fAcos:
      case Intrinsic::gla_fAcosh:
      case Intrinsic::gla_fAsin:
      case Intrinsic::gla_fAsinh:
      case Intrinsic::gla_fAtan:
      case Intrinsic::gla_fAtan2:
      case Intrinsic::gla_fAtanh:
      case Intrinsic::gla_fCeiling:
      case Intrinsic::gla_fClamp:
      case Intrinsic::gla_fCos:
      case Intrinsic::gla_fCosh:
      case Intrinsic::gla_fCross:
      case Intrinsic::gla_fDFdx:
      case Intrinsic::gla_fDFdy:
      case Intrinsic::gla_fDegrees:
      case Intrinsic::gla_fDistance:
      case Intrinsic::gla_fDot2:
      case Intrinsic::gla_fDot3:
      case Intrinsic::gla_fDot4:
      case Intrinsic::gla_fExp:
      case Intrinsic::gla_fExp10:
      case Intrinsic::gla_fExp2:
      case Intrinsic::gla_fFaceForward:
      case Intrinsic::gla_fFilterWidth:
      case Intrinsic::gla_fFixedTransform:
      case Intrinsic::gla_fFloatBitsToInt:
      case Intrinsic::gla_fFloor:
      case Intrinsic::gla_fFma:
      case Intrinsic::gla_fFraction:
      case Intrinsic::gla_fFrexp:
      case Intrinsic::gla_fIntBitsTofloat:
      case Intrinsic::gla_fInverseSqrt:
      case Intrinsic::gla_fIsInf:
      case Intrinsic::gla_fIsNan:
      case Intrinsic::gla_fLdexp:
      case Intrinsic::gla_fLength:
      case Intrinsic::gla_fLit:
      case Intrinsic::gla_fLog:
      case Intrinsic::gla_fLog10:
      case Intrinsic::gla_fLog2:
      case Intrinsic::gla_fMax:
      case Intrinsic::gla_fMin:
      case Intrinsic::gla_fMix:
      case Intrinsic::gla_fModF:
      case Intrinsic::gla_fMultiInsert:
      case Intrinsic::gla_fNormalize:
      case Intrinsic::gla_fNormalize3D:
      case Intrinsic::gla_fPackDouble2x32:
      case Intrinsic::gla_fPackSnorm4x8:
      case Intrinsic::gla_fPackUnorm2x16:
      case Intrinsic::gla_fPackUnorm4x8:
      case Intrinsic::gla_fPow:
      case Intrinsic::gla_fPowi:
      case Intrinsic::gla_fQueryTextureLod:
      case Intrinsic::gla_fRTextureSample1:
      case Intrinsic::gla_fRTextureSample2:
      case Intrinsic::gla_fRTextureSample3:
      case Intrinsic::gla_fRTextureSample4:
      case Intrinsic::gla_fRTextureSampleLodRefZ1:
      case Intrinsic::gla_fRTextureSampleLodRefZ2:
      case Intrinsic::gla_fRTextureSampleLodRefZ3:
      case Intrinsic::gla_fRTextureSampleLodRefZ4:
      case Intrinsic::gla_fRTextureSampleLodRefZOffset1:
      case Intrinsic::gla_fRTextureSampleLodRefZOffset2:
      case Intrinsic::gla_fRTextureSampleLodRefZOffset3:
      case Intrinsic::gla_fRTextureSampleLodRefZOffset4:
      case Intrinsic::gla_fRTextureSampleLodRefZOffsetGrad1:
      case Intrinsic::gla_fRTextureSampleLodRefZOffsetGrad2:
      case Intrinsic::gla_fRTextureSampleLodRefZOffsetGrad3:
      case Intrinsic::gla_fRTextureSampleLodRefZOffsetGrad4:
      case Intrinsic::gla_fRadians:
      case Intrinsic::gla_fReadData:
      case Intrinsic::gla_fReadInterpolant:
      case Intrinsic::gla_fReadInterpolantOffset:
      case Intrinsic::gla_fReflect:
      case Intrinsic::gla_fRefract:
      case Intrinsic::gla_fRoundEven:
      case Intrinsic::gla_fRoundFast:
      case Intrinsic::gla_fRoundZero:
      case Intrinsic::gla_fSign:
      case Intrinsic::gla_fSin:
      case Intrinsic::gla_fSinh:
      case Intrinsic::gla_fSmoothStep:
      case Intrinsic::gla_fSqrt:
      case Intrinsic::gla_fStep:
      case Intrinsic::gla_fSwizzle:
      case Intrinsic::gla_fTan:
      case Intrinsic::gla_fTanh:
      case Intrinsic::gla_fTexelFetchOffset:
      case Intrinsic::gla_fTexelGather:
      case Intrinsic::gla_fTexelGatherOffset:
      case Intrinsic::gla_fTexelGatherOffsets:
      case Intrinsic::gla_fTextureSample:
      case Intrinsic::gla_fTextureSampleLodRefZ:
      case Intrinsic::gla_fTextureSampleLodRefZOffset:
      case Intrinsic::gla_fTextureSampleLodRefZOffsetGrad:
      case Intrinsic::gla_fUnpackDouble2x32:
      case Intrinsic::gla_fUnpackSnorm4x8:
      case Intrinsic::gla_fUnpackUnorm2x16:
      case Intrinsic::gla_fUnpackUnorm4x8:
      // case Intrinsic::gla_fWriteData:
      // case Intrinsic::gla_fWriteInterpolant:
      case Intrinsic::gla_findLSB:
      case Intrinsic::gla_getInterpolant:
      case Intrinsic::gla_multiInsert:
      case Intrinsic::gla_not:
      case Intrinsic::gla_queryTextureSize:
      case Intrinsic::gla_rTextureSample1:
      case Intrinsic::gla_rTextureSample2:
      case Intrinsic::gla_rTextureSample3:
      case Intrinsic::gla_rTextureSample4:
      case Intrinsic::gla_rTextureSampleLodRefZ1:
      case Intrinsic::gla_rTextureSampleLodRefZ2:
      case Intrinsic::gla_rTextureSampleLodRefZ3:
      case Intrinsic::gla_rTextureSampleLodRefZ4:
      case Intrinsic::gla_rTextureSampleLodRefZOffset1:
      case Intrinsic::gla_rTextureSampleLodRefZOffset2:
      case Intrinsic::gla_rTextureSampleLodRefZOffset3:
      case Intrinsic::gla_rTextureSampleLodRefZOffset4:
      case Intrinsic::gla_rTextureSampleLodRefZOffsetGrad1:
      case Intrinsic::gla_rTextureSampleLodRefZOffsetGrad2:
      case Intrinsic::gla_rTextureSampleLodRefZOffsetGrad3:
      case Intrinsic::gla_rTextureSampleLodRefZOffsetGrad4:
      case Intrinsic::gla_readData:
      case Intrinsic::gla_sBitFieldExtract:
      case Intrinsic::gla_sClamp:
      case Intrinsic::gla_sFindMSB:
      case Intrinsic::gla_sFma:
      case Intrinsic::gla_sMax:
      case Intrinsic::gla_sMin:
      case Intrinsic::gla_smulExtended:
      case Intrinsic::gla_subBorrow:
      case Intrinsic::gla_swizzle:
      case Intrinsic::gla_texelFetchOffset:
      case Intrinsic::gla_texelGather:
      case Intrinsic::gla_texelGatherOffset:
      case Intrinsic::gla_texelGatherOffsets:
      case Intrinsic::gla_textureSample:
      case Intrinsic::gla_textureSampleLodRefZ:
      case Intrinsic::gla_textureSampleLodRefZOffset:
      case Intrinsic::gla_textureSampleLodRefZOffsetGrad:
      case Intrinsic::gla_uBitFieldExtract:
      case Intrinsic::gla_uClamp:
      case Intrinsic::gla_uFindMSB:
      case Intrinsic::gla_uFma:
      case Intrinsic::gla_uMax:
      case Intrinsic::gla_uMin:
      case Intrinsic::gla_umulExtended:
      // case Intrinsic::gla_writeData:
          return true;
      default:
          break;
      }
    }


    return false; // The called function could have undefined behavior or
                  // side-effects.
                  // FIXME: We should special-case some intrinsics (bswap,
                  // overflow-checking arithmetic, etc.)
  case VAArg:
  case Alloca:
  case Invoke:
  case PHI:
  case Store:
  case Ret:
  case Br:
  case IndirectBr:
  case Switch:
  case Unwind:
  case Unreachable:
    return false; // Misc instructions which have effects
  }
}

Instruction *Instruction::clone() const {
  Instruction *New = clone_impl();
  New->SubclassOptionalData = SubclassOptionalData;
  if (!hasMetadata())
    return New;

  // Otherwise, enumerate and copy over metadata from the old instruction to the
  // new one.
  SmallVector<std::pair<unsigned, MDNode*>, 4> TheMDs;
  getAllMetadata(TheMDs);
  for (unsigned i = 0, e = TheMDs.size(); i != e; ++i)
    New->setMetadata(TheMDs[i].first, TheMDs[i].second);
  return New;
}
