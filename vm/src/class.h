// Copyright 2014-2015 Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#ifndef class_h
#define class_h

#include <iostream>
#include "block.h"
#include "utils.h"

namespace codeswitch {
namespace internal {

template <class T>
class BlockArray;
class Field;
class Function;
class I32Array;
typedef I32Array LengthArray;
class Package;
class PackageIdArray;
class Name;
template <class T>
class TaggedArray;
class Type;
class TypeParameter;

class Class: public Block {
 public:
  static const BlockType kBlockType = CLASS_BLOCK_TYPE;

  void* operator new(size_t, Heap* heap);
  Class(Name* name,
        u32 flags,
        BlockArray<TypeParameter>* typeParameters,
        Type* supertype,
        BlockArray<Field>* fields,
        BlockArray<Function>* constructors,
        BlockArray<Function>* methods,
        Package* package,
        Meta* instanceMeta = nullptr,
        Type* elementType = nullptr,
        length_t lengthFieldIndex = kIndexNotSet);
  static Local<Class> create(Heap* heap);
  static Local<Class> create(Heap* heap,
                             const Handle<Name>& name,
                             u32 flags,
                             const Handle<BlockArray<TypeParameter>>& typeParameters,
                             const Handle<Type>& supertype,
                             const Handle<BlockArray<Field>>& fields,
                             const Handle<BlockArray<Function>>& constructors,
                             const Handle<BlockArray<Function>>& methods,
                             const Handle<Package>& package,
                             const Handle<Meta>& instanceMeta = Local<Meta>(),
                             const Handle<Type>& elementType = Local<Type>(),
                             length_t lengthFieldIndex = kIndexNotSet);

  // Most members can be set after construction, even though we would like to consider Class
  // as immutable. This is necessary since Class and Type have a cyclic relationship. We may
  // need to allocate empty Class objects early, then fill them after other objects which
  // refer to them have been allocated.

  Name* name() const { return name_.get(); }
  void setName(Name* name) { name_.set(this, name); }

  u32 flags() const { return flags_; }
  void setFlags(u32 flags) { flags_ = flags; }

  BlockArray<TypeParameter>* typeParameters() const { return typeParameters_.get(); }
  void setTypeParameters(BlockArray<TypeParameter>* newTypeParameters) {
    typeParameters_.set(this, newTypeParameters);
  }
  TypeParameter* typeParameter(length_t index) const;
  length_t typeParameterCount() const;

  Type* supertype() const { return supertype_.get(); }
  void setSupertype(Type* newSupertype) { supertype_.set(this, newSupertype); }

  BlockArray<Field>* fields() const { return fields_.get(); }
  void setFields(BlockArray<Field>* newFields) { fields_.set(this, newFields); }
  length_t findFieldIndex(word_t offset) const;
  word_t findFieldOffset(length_t index) const;
  Class* findFieldClass(length_t index);

  BlockArray<Function>* constructors() const { return constructors_.get(); }
  void setConstructors(BlockArray<Function>* newConstructors) {
    constructors_.set(this, newConstructors);
  }

  BlockArray<Function>* methods() const { return methods_.get(); }
  void setMethods(BlockArray<Function>* newMethods) { methods_.set(this, newMethods); }

  Package* package() const { return package_.get(); }
  void setPackage(Package* newPackage) { package_.set(this, newPackage); }
  Meta* instanceMeta() const { return instanceMeta_.get(); }
  void setInstanceMeta(Meta* newInstanceMeta) { instanceMeta_.set(this, newInstanceMeta); }

  Type* elementType() const { return elementType_.get(); }
  void setElementType(Type* newElementType) { elementType_.set(this, newElementType); }

  length_t lengthFieldIndex() const { return lengthFieldIndex_; }

  /** Constructs a new instance Meta whether one already exists or not. Does not use handles
   *  or invoke the garbage collector. This is used by Roots, since GC is not available there.
   *  `ensureAndGetInstaceMeta` should be called normally.
   */
  Meta* buildInstanceMeta();
  static Local<Meta> ensureAndGetInstanceMeta(const Handle<Class>& clas);
  static void ensureInstanceMeta(const Handle<Class>& clas);

  bool isSubclassOf(Class* other) const;

 private:
  void computeSizeAndPointerMap(u32* size, bool* hasPointers, BitSet* pointerMap) const;
  void computeSizeAndPointerMapForType(Type* type, u32* size,
                                       bool* hasPointers, BitSet* pointerMap) const;

  DECLARE_POINTER_MAP()
  Ptr<Name> name_;
  u32 flags_;
  Ptr<BlockArray<TypeParameter>> typeParameters_;
  Ptr<Type> supertype_;
  Ptr<BlockArray<Field>> fields_;
  Ptr<BlockArray<Function>> constructors_;
  Ptr<BlockArray<Function>> methods_;
  Ptr<Package> package_;
  Ptr<Meta> instanceMeta_;
  Ptr<Type> elementType_;
  length_t lengthFieldIndex_;
  // Update CLASS_POINTER_LIST if pointer members change.
};

std::ostream& operator << (std::ostream& os, const Class* clas);

}
}

#endif
