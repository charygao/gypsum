// Copyright 2015 Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#include "test.h"

#include "hash-table.h"
#include "string.h"

using namespace std;
using namespace codeswitch::internal;

typedef BlockHashMap<String, String> StringHashMap;

TEST(HashMapBasics) {
  TEST_PROLOGUE

  auto map = StringHashMap::create(heap);
  ASSERT_TRUE(map->isEmpty());
  ASSERT_EQ(0, map->length());

  auto foo = STR("foo");
  auto bar = STR("bar");
  auto baz = STR("baz");
  ASSERT_FALSE(map->contains(*foo));

  StringHashMap::add(heap, map, foo, foo);
  ASSERT_EQ(1, map->length());
  ASSERT_FALSE(map->isEmpty());
  ASSERT_TRUE(map->contains(*foo));
  ASSERT_EQ(*foo, map->get(*foo));

  StringHashMap::add(heap, map, bar, bar);
  StringHashMap::add(heap, map, baz, baz);
  ASSERT_EQ(3, map->length());
  ASSERT_TRUE(map->contains(*bar));
  ASSERT_TRUE(map->contains(*baz));

  StringHashMap::remove(heap, map, foo);
  ASSERT_EQ(2, map->length());
  ASSERT_FALSE(map->contains(*foo));

  StringHashMap::remove(heap, map, bar);
  StringHashMap::remove(heap, map, baz);
  ASSERT_TRUE(map->isEmpty());
}


TEST(HashMapStress) {
  TEST_PROLOGUE

  length_t repetitions = 2000;
  auto map = StringHashMap::create(heap);
  for (auto i = 0; i < 2; i++) {
    for (length_t j = 0; j < repetitions; j++) {
      u32 ch = static_cast<u32>(j);
      auto s = String::create(heap, 1, &ch);
      StringHashMap::add(heap, map, s, s);
      ASSERT_EQ(j + 1, map->length());
    }
    for (length_t j = 0; j < repetitions; j++) {
      u32 ch = static_cast<u32>(j);
      auto s = String::create(heap, 1, &ch);
      ASSERT_TRUE(map->contains(*s));
    }
    for (length_t j = 0; j < repetitions; j++) {
      u32 ch = static_cast<u32>(j);
      auto s = String::create(heap, 1, &ch);
      StringHashMap::remove(heap, map, s);
      ASSERT_EQ(repetitions - j - 1, map->length());
    }
  }
}
