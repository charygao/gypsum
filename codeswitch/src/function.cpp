// Copyright Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#include "function.h"

#include <algorithm>
#include <string>
#include <vector>
#include "array.h"
#include "block.h"
#include "bytecode.h"
#include "class.h"
#include "field.h"
#include "global.h"
#include "name.h"
#include "object-type-defn.h"
#include "package.h"
#include "roots.h"
#include "string.h"
#include "type.h"
#include "type-parameter.h"
#include "utils.h"

using namespace std;

namespace codeswitch {
namespace internal {

#define FUNCTION_POINTER_LIST(F) \
  F(Function, name_)             \
  F(Function, sourceName_)       \
  F(Function, typeParameters_)   \
  F(Function, returnType_)       \
  F(Function, parameterTypes_)   \
  F(Function, definingClass_)    \
  F(Function, blockOffsets_)     \
  F(Function, package_)          \
  F(Function, overrides_)        \
  F(Function, instTypes_)        \
  F(Function, stackPointerMap_)  \


DEFINE_POINTER_MAP(Function, FUNCTION_POINTER_LIST)

#undef FUNCTION_POINTER_LIST


void* Function::operator new(size_t, Heap* heap, length_t instructionsSize) {
  ASSERT(instructionsSize <= kMaxLength);
  word_t size = sizeForFunction(instructionsSize);
  Function* function = reinterpret_cast<Function*>(heap->allocate(size));
  function->instructionsSize_ = instructionsSize;
  return function;
}


Function::Function(DefnId id,
                   Name* name,
                   String* sourceName,
                   u32 flags,
                   BlockArray<TypeParameter>* typeParameters,
                   Type* returnType,
                   BlockArray<Type>* parameterTypes,
                   ObjectTypeDefn* definingClass,
                   word_t localsSize,
                   const vector<u8>& instructions,
                   LengthArray* blockOffsets,
                   Package* package,
                   BlockArray<Function>* overrides,
                   BlockArray<Type>* instTypes,
                   StackPointerMap* stackPointerMap,
                   NativeFunction nativeFunction)
    : Block(FUNCTION_BLOCK_TYPE),
      id_(id),
      name_(this, name),
      sourceName_(this, sourceName),
      flags_(flags),
      builtinId_(0),
      typeParameters_(this, typeParameters),
      returnType_(this, returnType),
      parameterTypes_(this, parameterTypes),
      definingClass_(this, definingClass),
      localsSize_(localsSize),
      instructionsSize_(instructions.size()),
      blockOffsets_(this, blockOffsets),
      package_(this, package),
      overrides_(this, overrides),
      instTypes_(this, instTypes),
      stackPointerMap_(this, stackPointerMap),
      nativeFunction_(nullptr) {
  ASSERT(instructionsSize_ <= kMaxLength);
  copy_n(instructions.begin(), instructions.size(), instructionsStart());
}


Local<Function> Function::create(Heap* heap, DefnId id) {
  RETRY_WITH_GC(heap, return Local<Function>(new(heap, 0) Function(
      id, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
      0, vector<u8>{}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)));
}


Local<Function> Function::create(Heap* heap,
                                 DefnId id,
                                 const Handle<Name>& name,
                                 const Handle<String>& sourceName,
                                 u32 flags,
                                 const Handle<BlockArray<TypeParameter>>& typeParameters,
                                 const Handle<Type>& returnType,
                                 const Handle<BlockArray<Type>>& parameterTypes,
                                 const Handle<ObjectTypeDefn>& definingClass,
                                 word_t localsSize,
                                 const vector<u8>& instructions,
                                 const Handle<LengthArray>& blockOffsets,
                                 const Handle<Package>& package,
                                 const Handle<BlockArray<Function>>& overrides,
                                 const Handle<BlockArray<Type>>& instTypes,
                                 NativeFunction nativeFunction) {
  RETRY_WITH_GC(heap, return Local<Function>(new(heap, instructions.size()) Function(
      id, *name, sourceName.getOrNull(), flags, *typeParameters, *returnType, *parameterTypes,
      definingClass.getOrNull(), localsSize, instructions, blockOffsets.getOrNull(),
      package.getOrNull(), overrides.getOrNull(), instTypes.getOrNull(),
      nullptr, nativeFunction)));
}


word_t Function::sizeForFunction(length_t instructionsSize) {
  ASSERT(instructionsSize <= kMaxLength);
  return elementsOffset(sizeof(Function), 1) + instructionsSize;
}


word_t Function::parametersSize() const {
  word_t size = 0;
  for (length_t i = 0, n = parameterTypes()->length(); i < n; i++) {
    size += align(parameterTypes()->get(i)->typeSize(), kWordSize);
  }
  return size;
}


ptrdiff_t Function::parameterOffset(length_t index) const {
  ptrdiff_t offset = 0;
  // Use i32 instead of length_t to avoid underflow.
  for (auto i = static_cast<i32>(parameterTypes()->length()) - 1;
       i > static_cast<i32>(index); i--) {
    offset += align(parameterTypes()->get(i)->typeSize(), kWordSize);
  }
  return offset;
}


u8* Function::instructionsStart() const {
  auto start = align(address() + sizeof(Function), kWordSize);
  return reinterpret_cast<u8*>(start);
}


bool Function::hasPointerMapAtPcOffset(length_t pcOffset) const {
  auto map = stackPointerMap();
  if (map == nullptr)
    return false;
  return map->hasLocalsRegion(pcOffset);
}


bool Function::isNative() const {
  return (NATIVE_FLAG & flags_) != 0;
}


void Function::ensureNativeFunction() {
  if (nativeFunction_ != nullptr)
    return;
  nativeFunction_ = package()->loadNativeFunction(name());
}


NativeFunction Function::ensureAndGetNativeFunction() {
  ensureNativeFunction();
  return nativeFunction();
}


DefnId Function::findOverriddenMethodId() const {
  auto override = this;
  while (override->overrides() != nullptr) {
    override = override->overrides()->get(0);
  }
  return override->id();
}


unordered_set<DefnId> Function::findOverriddenMethodIds() const {
  if (overrides() == nullptr) {
    return unordered_set<DefnId>{id()};
  } else {
    unordered_set<DefnId> allOverrideIds;
    for (length_t i = 0; i < overrides()->length(); i++) {
      auto overrideIds = overrides()->get(i)->findOverriddenMethodIds();
      allOverrideIds.insert(overrideIds.begin(), overrideIds.end());
    }
    return allOverrideIds;
  }
}


ostream& operator << (ostream& os, const Function* fn) {
  os << brief(fn)
     << "\n  id: " << fn->id();
  if (fn->hasBuiltinId())
    os << "\n  builtin id: " << fn->builtinId();
  os << "\n  name: " << brief(fn->name())
     << "\n  sourceName: " << brief(fn->sourceName())
     << "\n  type parameters: " << brief(fn->typeParameters())
     << "\n  returnType: " << brief(fn->returnType())
     << "\n  parameterTypes: " << brief(fn->parameterTypes())
     << "\n  locals size: " << fn->localsSize()
     << "\n  instructions size: " << fn->instructionsSize()
     << "\n  block offsets: " << brief(fn->blockOffsets())
     << "\n  package: " << brief(fn->package())
     << "\n  overrides: " << brief(fn->overrides())
     << "\n  inst types: " << brief(fn->instTypes())
     << "\n  stack pointer map: " << brief(fn->stackPointerMap());
  return os;
}


Bitmap StackPointerMap::bitmap() {
  word_t* base = reinterpret_cast<word_t*>(
      elements() + kHeaderLength + entryCount() * kEntryLength);
  return Bitmap(base, bitmapLength());
}


struct FrameState {
  FrameState(word_t localsSlots, const Local<Type>& defaultType)
      : pcOffset(-1) {
    typeMap.insert(typeMap.begin(), localsSlots, defaultType);
  }

  void push(const Local<Type>& type) {
    typeMap.push_back(type);
  }

  Local<Type> pop() {
    auto type = typeMap.back();
    typeMap.pop_back();
    return type;
  }

  void pop(length_t count) {
    ASSERT(count <= typeMap.size());
    typeMap.erase(typeMap.end() - count, typeMap.end());
  }

  Local<Type>& top() { return typeMap.back(); }

  void setLocal(i64 slot, const Local<Type>& type) {
    ASSERT(slot < 0);
    word_t index = -slot - 1;
    ASSERT(index < typeMap.size());
    typeMap[index] = type;
  }

  void pushTypeArg(const Local<Type>& type) {
    ASSERT(type->isObject());
    typeArgs.push_back(type);
  }

  Local<Type> popTypeArg() {
    ASSERT(typeArgs.size() >= 1);
    auto arg = typeArgs.back();
    typeArgs.pop_back();
    return arg;
  }

  void popTypeArgs() {
    typeArgs.clear();
  }

  void popTypeArgs(length_t count, vector<Local<Type>>* poppedArgs) {
    ASSERT(count <= typeArgs.size());
    if (poppedArgs != nullptr)
      poppedArgs->assign(typeArgs.end() - count, typeArgs.end());
    typeArgs.erase(typeArgs.end() - count, typeArgs.end());
  }

  Local<Type> substituteReturnType(const Local<Function>& callee) {
    ASSERT(typeArgs.size() == callee->typeParameters()->length());
    vector<pair<Local<TypeParameter>, Local<Type>>> typeBindings;
    typeBindings.reserve(typeArgs.size());
    for (word_t i = 0; i < typeArgs.size(); i++) {
      pair<Local<TypeParameter>, Local<Type>> binding(
          Local<TypeParameter>(callee->typeParameter(i)),
          typeArgs[i]);
      typeBindings.push_back(binding);
    }
    auto retTy = Type::substitute(handle(callee->returnType()), typeBindings);
    return retTy;
  }

  size_t size() { return typeMap.size(); }

  bool operator < (const FrameState& other) const {
    return pcOffset < other.pcOffset;
  }

  vector<Local<Type>> typeMap;
  vector<Local<Type>> typeArgs;
  length_t pcOffset;
};


Local<StackPointerMap> StackPointerMap::buildFrom(Heap* heap, const Local<Function>& function) {
  ASSERT(function->instructionsSize() > 0);

  auto roots = heap->vm()->roots();
  HandleScope handleScope(heap->vm());
  Local<Package> package(function->package());

  // Constrct a pointer map for the parameters.
  vector<Local<Type>> parametersMap;
  for (length_t i = 0, n = function->parameterTypes()->length(); i < n; i++) {
    parametersMap.push_back(handle(function->parameterTypes()->get(i)));
  }

  // Construct a pointer map for each point in the function where the garbage collector may be
  // invoked: every allocation and every function call. We do this by constructing a frame
  // state for the beginning of the function, and simulating the effect each instruction has
  // on the pointers in the frame. We save frame states at points where they're needed. The
  // blocks of the function are traversed in depth first order.
  vector<FrameState> maps;
  BitSet visitedBlockOffsets;
  vector<FrameState> blocksToVisit;
  blocksToVisit.push_back(FrameState(function->localsSize() / kWordSize,
                                     handle(Type::unitType(roots))));
  blocksToVisit.front().pcOffset = 0;
  auto bytecode = function->instructionsStart();

  while (!blocksToVisit.empty()) {
    FrameState currentMap = blocksToVisit.back();
    blocksToVisit.pop_back();
    if (visitedBlockOffsets.contains(currentMap.pcOffset))
      continue;
    auto pcOffset = currentMap.pcOffset;
    visitedBlockOffsets.add(pcOffset);

    bool blockDone = false;
    while (!blockDone) {
      auto opc = static_cast<Opcode>(bytecode[pcOffset++]);
      switch (opc) {
        case NOP:
          break;

        case RET:
          currentMap.pop();
          blockDone = true;
          break;

        case BRANCH: {
          i64 blockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(blockIndex);
          blocksToVisit.push_back(move(currentMap));
          blockDone = true;
          break;
        }

        case BRANCHIF: {
          currentMap.pop();
          i64 trueBlockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(trueBlockIndex);
          blocksToVisit.push_back(currentMap);
          i64 falseBlockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(falseBlockIndex);
          blocksToVisit.push_back(move(currentMap));
          blockDone = true;
          break;
        }

        case LABEL: {
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(roots->labelType()));
          break;
        }

        case BRANCHL: {
          currentMap.pop();
          auto count = toLength(readVbn(bytecode, &pcOffset));
          for (length_t i = 0; i < count - 1; i++) {
            auto succBlockIndex = toLength(readVbn(bytecode, &pcOffset));
            currentMap.pcOffset = function->blockOffset(succBlockIndex);
            blocksToVisit.push_back(currentMap);
          }
          auto succBlockIndex = toLength(readVbn(bytecode, &pcOffset));
          currentMap.pcOffset = function->blockOffset(succBlockIndex);
          blocksToVisit.emplace_back(move(currentMap));
          blockDone = true;
          break;
        }

        case PUSHTRY: {
          i64 tryBlockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(tryBlockIndex);
          blocksToVisit.push_back(currentMap);
          i64 catchBlockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(catchBlockIndex);
          currentMap.push(handle(roots->getBuiltinType(BUILTIN_EXCEPTION_CLASS_ID)));
          maps.push_back(currentMap);
          blocksToVisit.push_back(move(currentMap));
          blockDone = true;
          break;
        }

        case POPTRY: {
          i64 doneBlockIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = function->blockOffset(doneBlockIndex);
          blocksToVisit.push_back(move(currentMap));
          blockDone = true;
          break;
        }

        case THROW:
          currentMap.pop();
          blockDone = true;
          break;

        case DROP:
          currentMap.pop();
          break;

        case DROPI: {
          i64 count = readVbn(bytecode, &pcOffset);
          for (i64 i = 0; i < count; i++) {
            currentMap.pop();
          }
          break;
        }

        case DUP:
          currentMap.push(currentMap.top());
          break;

        case DUPI: {
          auto slot = readVbn(bytecode, &pcOffset);
          auto index = currentMap.size() - slot - 1;
          auto type = currentMap.typeMap[index];
          currentMap.typeMap.push_back(type);
          break;
        }

        case SWAP: {
          auto index = currentMap.size() - 2;
          auto top = currentMap.top();
          auto other = currentMap.typeMap[index];
          currentMap.typeMap.back() = other;
          currentMap.typeMap[index] = top;
          break;
        }

        case SWAP2: {
          auto index = currentMap.size() - 3;
          auto top = currentMap.top();
          auto other = currentMap.typeMap[index];
          currentMap.typeMap.back() = other;
          currentMap.typeMap[index] = top;
          break;
          break;
        }

        case UNIT:
          currentMap.push(handle(Type::unitType(roots)));
          break;

        case TRUE:
          currentMap.push(handle(Type::booleanType(roots)));
          break;

        case FALSE:
          currentMap.push(handle(Type::booleanType(roots)));
          break;

        case NUL:
          currentMap.push(handle(Type::nullType(roots)));
          break;

        case UNINITIALIZED:
          currentMap.push(handle(Type::nullType(roots)));
          break;

        case I8:
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(Type::i8Type(roots)));
          break;

        case I16:
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(Type::i16Type(roots)));
          break;

        case I32:
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(Type::i32Type(roots)));
          break;

        case I64:
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(Type::i64Type(roots)));
          break;

        case F32:
          pcOffset += 4;
          currentMap.push(handle(Type::f32Type(roots)));
          break;

        case F64:
          pcOffset += 8;
          currentMap.push(handle(Type::f64Type(roots)));
          break;

        case STRING: {
          readVbn(bytecode, &pcOffset);
          currentMap.push(handle(roots->getBuiltinType(BUILTIN_STRING_CLASS_ID)));
          break;
        }

        case LDLOCAL: {
          auto slot = readVbn(bytecode, &pcOffset);
          auto type = slot >= 0
              ? parametersMap[slot]
              : currentMap.typeMap[-slot - 1];
          currentMap.push(type);
          break;
        }

        case STLOCAL: {
          auto slot = readVbn(bytecode, &pcOffset);
          auto type = currentMap.pop();
          if (slot < 0)
            currentMap.typeMap[-slot - 1] = type;
          break;
        }

        case LDG: {
          auto index = readVbn(bytecode, &pcOffset);
          auto type = handle(package->getGlobal(index)->type());
          currentMap.push(type);
          break;
        }

        case LDGF: {
          auto depIndex = readVbn(bytecode, &pcOffset);
          auto externIndex = readVbn(bytecode, &pcOffset);
          auto type = handle(package->dependencies()->get(depIndex)
              ->linkedGlobals()->get(externIndex)->type());
          currentMap.push(type);
          break;
        }

        case STG: {
          readVbn(bytecode, &pcOffset);
          currentMap.pop();
          break;
        }

        case STGF: {
          readVbn(bytecode, &pcOffset);
          readVbn(bytecode, &pcOffset);
          currentMap.pop();
        }

        case LDF: {
          auto classId = readVbn(bytecode, &pcOffset);
          auto nameIndex = readVbn(bytecode, &pcOffset);
          Local<Class> fieldClass = handle(isBuiltinId(classId)
              ? roots->getBuiltinClass(static_cast<BuiltinId>(classId))
              : package->getClass(classId));
          auto name = handle(package->getName(nameIndex));
          auto fieldType = handle(fieldClass->findField(*name)->type());
          auto receiverType = currentMap.pop();
          auto receiverClass = handle(receiverType->effectiveClass());
          if (fieldType->isObject()) {
            fieldType = Type::substituteForInheritance(fieldType, receiverClass, fieldClass);
            fieldType = Type::substitute(fieldType, receiverType->getTypeArgumentBindings());
          }
          currentMap.push(fieldType);
          break;
        }

        case LDFF: {
          auto depIndex = readVbn(bytecode, &pcOffset);
          auto externIndex = readVbn(bytecode, &pcOffset);
          auto nameIndex = readVbn(bytecode, &pcOffset);
          auto fieldClass = handle(package->dependencies()->get(depIndex)
              ->linkedClasses()->get(externIndex));
          auto name = handle(package->getName(nameIndex));
          auto fieldType = handle(fieldClass->findField(*name)->type());
          auto receiverType = currentMap.pop();
          auto receiverClass = handle(receiverType->effectiveClass());
          if (fieldType->isObject()) {
            fieldType = Type::substituteForInheritance(fieldType, receiverClass, fieldClass);
            fieldType = Type::substitute(fieldType, receiverType->getTypeArgumentBindings());
          }
          currentMap.push(fieldType);
          break;
        }

        case STF:
          readVbn(bytecode, &pcOffset);
          readVbn(bytecode, &pcOffset);
          currentMap.pop();
          currentMap.pop();
          break;

        case STFF:
          readVbn(bytecode, &pcOffset);
          readVbn(bytecode, &pcOffset);
          readVbn(bytecode, &pcOffset);
          currentMap.pop();
          currentMap.pop();
          break;

        case LDE: {
          auto receiverType = currentMap.pop();
          currentMap.pop();  // index (i32)
          auto elementType = handle(receiverType->effectiveClass()->elementType());
          currentMap.push(elementType);
          break;
        }

        case STE: {
          currentMap.pop();
          currentMap.pop();
          currentMap.pop();
          break;
        }

        case ALLOCARR:
          currentMap.pop();
          // fall through.

        case ALLOCOBJ: {
          i64 classId = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = pcOffset;
          maps.push_back(currentMap);
          Local<Type> type;
          if (isBuiltinId(classId)) {
            type = handle(roots->getBuiltinType(classId));
          } else {
            Local<Class> clas(package->getClass(classId));
            vector<Local<Type>> typeArgs;
            currentMap.popTypeArgs(clas->typeParameterCount(), &typeArgs);
            type = Type::create(heap, clas, typeArgs);
          }
          currentMap.push(type);
          break;
        }

        case ALLOCARRF:
          currentMap.pop();
          // fall through.

        case ALLOCOBJF: {
          auto depIndex = readVbn(bytecode, &pcOffset);
          auto externIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = pcOffset;
          maps.push_back(currentMap);
          auto clas = handle(package->dependencies()->get(depIndex)
              ->linkedClasses()->get(externIndex));
          vector<Local<Type>> typeArgs;
          currentMap.popTypeArgs(clas->typeParameterCount(), &typeArgs);
          auto type = Type::create(heap, clas, typeArgs);
          currentMap.push(type);
          break;
        }

        case TYS: {
          i64 index = readVbn(bytecode, &pcOffset);
          auto type = handle(function->instTypes()->get(index));
          currentMap.pushTypeArg(type);
          break;
        }

        case TYD: {
          i64 index = readVbn(bytecode, &pcOffset);
          auto type = handle(function->instTypes()->get(index));
          currentMap.pushTypeArg(type);
          auto valueType = handle(roots->getBuiltinType(BUILTIN_TYPE_CLASS_ID));
          currentMap.push(valueType);
          break;
        }

        case CAST: {
          auto type = currentMap.popTypeArg();
          currentMap.pop();
          currentMap.push(type);
          break;
        }

        case CASTC: {
          auto type = currentMap.popTypeArg();
          currentMap.pop();
          currentMap.pop();
          currentMap.push(type);
          break;
        }

        case CASTCBR: {
          i64 trueBlockIndex = readVbn(bytecode, &pcOffset);
          i64 falseBlockIndex = readVbn(bytecode, &pcOffset);
          auto type = currentMap.popTypeArg();
          currentMap.pop();
          currentMap.pcOffset = function->blockOffset(falseBlockIndex);
          blocksToVisit.push_back(currentMap);
          currentMap.pcOffset = function->blockOffset(trueBlockIndex);
          currentMap.pop();
          currentMap.push(type);
          blocksToVisit.push_back(move(currentMap));
          blockDone = true;
          break;
        }

        case CALLG:
        case CALLV: {
          i64 functionId = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = pcOffset;
          maps.push_back(currentMap);
          Local<Function> callee;
          if (isBuiltinId(functionId)) {
            callee = handle(roots->getBuiltinFunction(static_cast<BuiltinId>(functionId)));
          } else {
            callee = handle(package->getFunction(functionId));
          }
          for (length_t i = 0; i < callee->parameterTypes()->length(); i++)
            currentMap.pop();
          auto returnType = currentMap.substituteReturnType(callee);
          currentMap.popTypeArgs();
          currentMap.push(returnType);
          break;
        }

        case CALLGF:
        case CALLVF: {
          auto depIndex = readVbn(bytecode, &pcOffset);
          auto externIndex = readVbn(bytecode, &pcOffset);
          currentMap.pcOffset = pcOffset;
          maps.push_back(currentMap);
          auto callee = handle(package->dependencies()->get(depIndex)
              ->linkedFunctions()->get(externIndex));
          for (length_t i = 0; i < callee->parameterTypes()->length(); i++)
            currentMap.pop();
          auto returnType = currentMap.substituteReturnType(callee);
          currentMap.popTypeArgs();
          currentMap.push(returnType);
          break;
        }

        case PKG: {
          readVbn(bytecode, &pcOffset);
          auto packageClass = handle(roots->getBuiltinClass(BUILTIN_PACKAGE_CLASS_ID));
          auto type = Type::create(heap, packageClass);
          currentMap.push(type);
          break;
        }

        case ADDI8:
        case SUBI8:
        case MULI8:
        case DIVI8:
        case MODI8:
        case LSLI8:
        case LSRI8:
        case ASRI8:
        case ANDI8:
        case ORI8:
        case XORI8:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::i8Type(roots)));
          break;

        case ADDI16:
        case SUBI16:
        case MULI16:
        case DIVI16:
        case MODI16:
        case LSLI16:
        case LSRI16:
        case ASRI16:
        case ANDI16:
        case ORI16:
        case XORI16:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::i16Type(roots)));
          break;

        case ADDI32:
        case SUBI32:
        case MULI32:
        case DIVI32:
        case MODI32:
        case LSLI32:
        case LSRI32:
        case ASRI32:
        case ANDI32:
        case ORI32:
        case XORI32:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::i32Type(roots)));
          break;

        case ADDI64:
        case SUBI64:
        case MULI64:
        case DIVI64:
        case MODI64:
        case LSLI64:
        case LSRI64:
        case ASRI64:
        case ANDI64:
        case ORI64:
        case XORI64:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::i64Type(roots)));
          break;

        case ADDF32:
        case SUBF32:
        case MULF32:
        case DIVF32:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::f32Type(roots)));
          break;

        case ADDF64:
        case SUBF64:
        case MULF64:
        case DIVF64:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::f64Type(roots)));
          break;

        case EQI8:
        case EQI16:
        case EQI32:
        case EQI64:
        case EQF32:
        case EQF64:
        case EQP:
        case NEI8:
        case NEI16:
        case NEI32:
        case NEI64:
        case NEF32:
        case NEF64:
        case NEP:
        case LTI8:
        case LTI16:
        case LTI32:
        case LTI64:
        case LTF32:
        case LTF64:
        case LEI8:
        case LEI16:
        case LEI32:
        case LEI64:
        case LEF32:
        case LEF64:
        case GTI8:
        case GTI16:
        case GTI32:
        case GTI64:
        case GTF32:
        case GTF64:
        case GEI8:
        case GEI16:
        case GEI32:
        case GEI64:
        case GEF32:
        case GEF64:
          currentMap.pop();
          currentMap.pop();
          currentMap.push(handle(Type::booleanType(roots)));
          break;

        case NEGI8:
        case INVI8:
          currentMap.pop();
          currentMap.push(handle(Type::i8Type(roots)));
          break;

        case NEGI16:
        case INVI16:
          currentMap.pop();
          currentMap.push(handle(Type::i16Type(roots)));
          break;

        case NEGI32:
        case INVI32:
          currentMap.pop();
          currentMap.push(handle(Type::i32Type(roots)));
          break;

        case NEGI64:
        case INVI64:
          currentMap.pop();
          currentMap.push(handle(Type::i64Type(roots)));
          break;

        case NEGF32:
          currentMap.pop();
          currentMap.push(handle(Type::f32Type(roots)));
          break;

        case NEGF64:
          currentMap.pop();
          currentMap.push(handle(Type::f64Type(roots)));
          break;

        case NOTB:
          currentMap.pop();
          currentMap.push(handle(Type::booleanType(roots)));
          break;

        case TRUNCI8:
          currentMap.pop();
          currentMap.push(handle(Type::i8Type(roots)));
          break;

        case TRUNCI16:
        case SEXTI16_8:
        case ZEXTI16:
          currentMap.pop();
          currentMap.push(handle(Type::i16Type(roots)));
          break;

        case TRUNCI32:
        case SEXTI32_8:
        case SEXTI32_16:
        case ZEXTI32:
        case FCVTI32:
        case FTOI32:
          currentMap.pop();
          currentMap.push(handle(Type::i32Type(roots)));
          break;

        case SEXTI64_8:
        case SEXTI64_16:
        case SEXTI64_32:
        case ZEXTI64:
        case FCVTI64:
        case FTOI64:
          currentMap.pop();
          currentMap.push(handle(Type::i64Type(roots)));
          break;

        case TRUNCF32:
        case ICVTF32:
        case ITOF32:
          currentMap.pop();
          currentMap.push(handle(Type::f32Type(roots)));
          break;

        case EXTF64:
        case ICVTF64:
        case ITOF64:
          currentMap.pop();
          currentMap.push(handle(Type::f64Type(roots)));
          break;

        default:
          UNIMPLEMENTED();
      }
    }
  }

  // Sort the pointer maps by pc offset.
  sort(maps.begin(), maps.end());

  // Determine how big the final bitmap will be.
  word_t bitmapLength = parametersMap.size();
  for (FrameState state : maps) {
    bitmapLength += state.typeMap.size();
  }

  // Allocate and build the final data structure.
  length_t arrayLength = StackPointerMap::kHeaderLength +
      maps.size() * StackPointerMap::kEntryLength +
      align(bitmapLength, kBitsInWord) / kWordSize;
  auto array = WordArray::create(heap, arrayLength);

  auto stackPointerMap = handle(static_cast<StackPointerMap*>(*array));
  stackPointerMap->setEntryCount(maps.size());
  length_t mapOffset = parametersMap.size();
  for (length_t i = 0, n = maps.size(); i < n; i++) {
    stackPointerMap->setPcOffset(i, maps[i].pcOffset);
    stackPointerMap->setMapOffset(i, mapOffset);
    stackPointerMap->setMapCount(i, maps[i].typeMap.size());
    mapOffset += maps[i].typeMap.size();
  }
  stackPointerMap->setBitmapLength(bitmapLength);
  Bitmap bitmap = stackPointerMap->bitmap();
  word_t bitOffset = 0;
  for (auto& type : parametersMap) {
    bitmap.set(bitOffset++, type->isObject());
  }
  for (auto& state : maps) {
    for (auto& type : state.typeMap) {
      bitmap.set(bitOffset++, type->isObject());
    }
  }

  return handleScope.escape(*stackPointerMap);
}


void StackPointerMap::getParametersRegion(word_t* paramOffset, word_t* paramCount) {
  *paramOffset = 0;
  // The parameter region is first in the bitmap. We determine its size by checking the offset
  // of the first locals region. If there are no other regions, then it is the size of the
  // whole bitmap.
  if (entryCount() == 0) {
    *paramCount = bitmapLength();
  } else {
    *paramCount = mapOffset(0);
  }
}


void StackPointerMap::getLocalsRegion(length_t pc, word_t* localsOffset, word_t* localsCount) {
  word_t begin = 0;
  word_t end = entryCount();
  word_t middle = begin + (end - begin) / 2;
  ASSERT(end != 0);
  while (pc != pcOffset(middle)) {
    ASSERT(middle < end);
    if (pc < pcOffset(middle))
      end = middle;
    else
      begin = middle + 1;
    middle = begin + (end - begin) / 2;
  }
  *localsOffset = mapOffset(middle);
  *localsCount = mapCount(middle);
}


word_t StackPointerMap::searchLocalsRegion(length_t pc) {
  word_t begin = 0;
  word_t end = entryCount();
  word_t middle = begin + (end - begin) / 2;
  if (end == 0)
    return kNotSet;
  while (pc != pcOffset(middle) && middle < end) {
    if (pc < pcOffset(middle))
      end = middle;
    else
      begin = middle + 1;
    middle = begin + (end - begin) / 2;
  }
  if (middle == entryCount() || pcOffset(middle) != pc)
    return kNotSet;
  return middle;
}

}
}
