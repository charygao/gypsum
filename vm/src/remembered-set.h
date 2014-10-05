// Copyright 2014 Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#ifndef remembered_set_h
#define remembered_set_h

#include <iterator>
#include <memory>
#include "utils.h"

namespace codeswitch {
namespace internal {

class Block;

/** RememberedSet is used to track incoming pointers for a Chunk. It is a simple data structure,
 *  stored on the C++ heap (which is necessary since it may grow after a chunk is already full).
 *  Each element of the set is a slot: a location of a pointer to an object on the chunk.
 *  Note that since pointers can be rewritten, the set may contain pointers that no longer
 *  point to the associated chunk. The elements of the set are unique.
 *
 *  The set is designed for fast common-case insertion. Elements are stored in two partitions:
 *  a clean array, which is sorted and unique, and a dirty array, which is neither. New slots
 *  are added to the dirty array. When the dirty array is full, it is sorted and merged with
 *  the clean array in a new, larger clean array. Duplicates are eliminating during merging.
 */
class RememberedSet {
 public:
  typedef Block** Slot;

  RememberedSet();

  void add(Slot slot);
  length_t length();
  void clear();

  class iterator: public std::iterator<std::input_iterator_tag, Slot> {
   public:
    iterator(RememberedSet* set, length_t index)
        : set_(set), index_(index) { }
    Slot operator * ();
    bool operator == (const iterator& other) const;
    bool operator != (const iterator& other) const {
      return !(*this == other);
    }
    iterator& operator ++ ();
   private:
    RememberedSet* set_;
    length_t index_;
  };
  iterator begin();
  iterator end();

 private:
  void addFast(Slot slot);
  void addSlow(Slot slot);
  void sortAndMerge();

  static const length_t kMinimumSetSize = 16;

  std::unique_ptr<Slot[]> clean_;
  std::unique_ptr<Slot[]> dirty_;
  length_t cleanCount_;
  length_t dirtyCount_;
  length_t setSize_;
};

}
}

#endif
