// Copyright 2016 Jay Conrod. All rights reserved.

// This file is part of CodeSwitch. Use of this source code is governed by
// the 3-clause BSD license that can be found in the LICENSE.txt file.


#include "platform.h"

extern "C" uint64_t codeswitch_glue_callNativeFunctionRawForInt(
    codeswitch::VM* vm,
    codeswitch::internal::NativeFunction function,
    uint64_t intArgCount,
    uint64_t* intArgs,
    uint64_t floatArgCount,
    uint64_t* floatArgs,
    uint64_t stackArgCount,
    uint64_t* stackArgs);

extern "C" double codeswitch_glue_callNativeFunctionRawForFloat(
    codeswitch::VM* vm,
    codeswitch::internal::NativeFunction function,
    uint64_t intArgCount,
    uint64_t* intArgs,
    uint64_t floatArgCount,
    uint64_t* floatArgs,
    uint64_t stackArgCount,
    uint64_t* stackArgs);

namespace codeswitch {
namespace internal {

int64_t callNativeFunctionRaw(
    codeswitch::VM* vm,
    NativeFunction function,
    word_t argCount,
    uint64_t* rawArgs,
    bool* argsAreInt,
    bool resultIsFloat) {
  const int kMaxIntArgs = 5;
  const int kMaxFloatArgs = 8;
  uint64_t intArgs[kMaxIntArgs];
  uint64_t floatArgs[kMaxFloatArgs];
  uint64_t stackArgs[argCount];
  uint64_t i, ii, fi, si;
  for (i = 0, ii = 0, fi = 0, si = 0; i < argCount; i++) {
    if (argsAreInt[i]) {
      if (ii < kMaxIntArgs) {
        intArgs[ii++] = rawArgs[i];
      } else {
        stackArgs[argCount - si++ - 1] = rawArgs[i];
      }
    } else {
      if (fi < kMaxFloatArgs) {
        floatArgs[fi++] = rawArgs[i];
      } else {
        stackArgs[argCount - si++ - 1] = rawArgs[i];
      }
    }
  }
  if (resultIsFloat) {
    auto result = codeswitch_glue_callNativeFunctionRawForFloat(
        vm, function, ii, intArgs, fi, floatArgs, si, stackArgs + (argCount - si));
    return f64ToBits(result);
  } else {
    return codeswitch_glue_callNativeFunctionRawForInt(
        vm, function, ii, intArgs, fi, floatArgs, si, stackArgs + (argCount - si));
  }
}

}
}
