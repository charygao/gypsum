// Copyright 2014-2015 Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#ifndef function_h
#define function_h

#include <iostream>
#include <new>
#include <vector>
#include "array.h"
#include "block.h"
#include "type.h"
#include "utils.h"

namespace codeswitch {
namespace internal {

class Bitmap;
class Package;
class StackPointerMap;
class String;
class TypeParameter;

class Function: public Block {
 public:
  static const BlockType kBlockType = FUNCTION_BLOCK_TYPE;

  void* operator new(size_t, Heap* heap, length_t instructionsSize);
  Function(String* name,
           u32 flags,
           TaggedArray<TypeParameter>* typeParameters,
           BlockArray<Type>* types,
           word_t localsSize,
           const std::vector<u8>& instructions,
           LengthArray* blockOffsets,
           Package* package,
           StackPointerMap* stackPointerMap);
  static Local<Function> create(Heap* heap,
                                const Handle<String>& name,
                                u32 flags,
                                const Handle<TaggedArray<TypeParameter>>& typeParameters,
                                const Handle<BlockArray<Type>>& types,
                                word_t localsSize,
                                const std::vector<u8>& instructions,
                                const Handle<LengthArray>& blockOffsets,
                                const Handle<Package>& package);

  static word_t sizeForFunction(length_t instructionsSize);

  String* name() const { return name_.get(); }
  u32 flags() const { return flags_; }

  BuiltinId builtinId() const {
    ASSERT(hasBuiltinId());
    return builtinId_;
  }
  void setBuiltinId(BuiltinId id) { builtinId_ = id; }
  bool hasBuiltinId() const { return builtinId_ != 0; }

  TaggedArray<TypeParameter>* typeParameters() const { return typeParameters_.get(); }
  TypeParameter* typeParameter(length_t index) const;
  length_t typeParameterCount() const { return typeParameters()->length(); }

  BlockArray<Type>* types() const { return types_.get(); }
  Type* returnType() const { return types()->get(0); }
  length_t parameterCount() const { return types()->length() - 1; }
  word_t parametersSize() const;
  ptrdiff_t parameterOffset(length_t index) const;
  Type* parameterType(length_t index) const { return types()->get(index + 1); }

  word_t localsSize() const { return localsSize_; }

  length_t instructionsSize() const { return instructionsSize_; }
  u8* instructionsStart() const;

  LengthArray* blockOffsets() const { return blockOffsets_.get(); }
  length_t blockOffset(length_t index) const { return blockOffsets()->get(index); }

  Package* package() const { return package_.get(); }

  StackPointerMap* stackPointerMap() const { return stackPointerMap_.get(); }
  void setStackPointerMap(StackPointerMap* newStackPointerMap) {
    stackPointerMap_.set(this, newStackPointerMap);
  }
  bool hasPointerMapAtPcOffset(length_t pcOffset) const;

  static bool isCompatibleWith(const Handle<Function>& a, const Handle<Function>& b);

 private:
  DECLARE_POINTER_MAP()
  Ptr<String> name_;
  u32 flags_;
  BuiltinId builtinId_;
  Ptr<TaggedArray<TypeParameter>> typeParameters_;
  Ptr<BlockArray<Type>> types_;
  word_t localsSize_;
  length_t instructionsSize_;
  Ptr<LengthArray> blockOffsets_;
  Ptr<Package> package_;
  Ptr<StackPointerMap> stackPointerMap_;
  // Update FUNCTION_POINTER_LIST if pointer members change.
};

std::ostream& operator << (std::ostream& os, const Function* fn);


class StackPointerMap: public WordArray {
  // NOTE: We shouldn't say StackPointerMap "is a" WordArray. It is implemented using a
  // WordArray, so private inheritance might be more appropriate. However, it definitely
  // "is a" block. Multiple inheritance will likely mess up everything here, but at least
  // there aren't any virtual methods.
 public:
  static Local<StackPointerMap> buildFrom(Heap* heap, const Local<Function>& function);

  Bitmap bitmap();
  void getParametersRegion(word_t* paramOffset, word_t* paramCount);
  void getLocalsRegion(length_t pc, word_t* localsOffset, word_t* localsCount);
  word_t searchLocalsRegion(length_t pc);
  bool hasLocalsRegion(length_t pc) { return searchLocalsRegion(pc) != kNotSet; }

  DEFINE_INL_INDEX_ACCESSORS(word_t, bitmapLength, setBitmapLength, kBitmapLengthIndex)
  DEFINE_INL_INDEX_ACCESSORS(word_t, entryCount, setEntryCount, kEntryCountIndex)
  DEFINE_INL_ENTRY_ACCESSORS(word_t, pcOffset, setPcOffset,
                             kHeaderLength, kEntryLength, kPcOffsetEntryIndex)
  DEFINE_INL_ENTRY_ACCESSORS(word_t, mapOffset, setMapOffset,
                             kHeaderLength, kEntryLength, kMapOffsetEntryIndex)
  DEFINE_INL_ENTRY_ACCESSORS(word_t, mapCount, setMapCount,
                             kHeaderLength, kEntryLength, kMapCountEntryIndex)

  static const int kBitmapLengthIndex = 0;
  static const int kEntryCountIndex = kBitmapLengthIndex + 1;
  static const int kHeaderLength = kEntryCountIndex + 1;

  static const int kPcOffsetEntryIndex = 0;
  static const int kMapOffsetEntryIndex = kPcOffsetEntryIndex + 1;
  static const int kMapCountEntryIndex = kMapOffsetEntryIndex + 1;
  static const int kEntryLength = kMapCountEntryIndex + kWordSize;
};

}
}

#endif
