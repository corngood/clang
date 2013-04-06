//===--- AMP.h - C++AMP enums --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines some C++AMP-specific enums.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_AMP_H
#define LLVM_CLANG_BASIC_AMP_H

namespace clang {

enum AMPFunctionTarget {
  AFT_CPU = 1 << 0,
  AFT_AMP = 1 << 1
};

}

#endif
