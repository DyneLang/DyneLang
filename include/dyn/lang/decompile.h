/*
 * MIT License
 *
 * Copyright (c) 2025 The Dyne Language Team
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DYN_LANG_DECOMPILE_H
#define DYN_LANG_DECOMPILE_H

#include <dyn/ref.h>
#include <vector>

namespace dyn {

namespace lang {

/**
 Dyne bytecode program counter.
 This is the index of the bytecode within the instructions array.
 */
using PC = size_t;
constexpr PC kInvalidPC = (PC)-1;

/**
 Enum of all available Dyne bytecodes.
 */
enum class BC {
  EndOfFile,       Pop,             Dup,             Return,
  PushSelf,        SetLexScope,     IterNext,        IterDone,
  PopHandlers,     Push,            PushConst,       Call,
  Invoke,          Send,            SendIfDefined,   Resend,
  ResendIfDefined, Branch,          BranchIfTrue,    BranchIfFalse,
  FindVar,         GetVar,          MakeFrame,       MakeArray,
  FillArray,       GetPath,         GetPathCheck,    SetPath,
  SetPathVal,      SetVar,          FindAndSetVar,   IncrVar,
  BranchLoop,      Add,             Subtract,        ARef,
  SetARef,         Equals,          Not,             NotEquals,
  Multiply,        Divide,          Div,             LessThan,
  GreaterThan,     GreaterOrEqual,  LessOrEqual,     BitAnd,
  BitOr,           BitNot,          NewIter,         Length,
  Clone,           SetClass,        AddArraySlot,    Stringer,
  HasPath,         ClassOf,         NewHandler,      Unknown
};

/**
 Verbose version of a Dyne bytecode instruction including labels.
 */
typedef struct {
  BC bc;
  int arg;
  PC pc;
  int references;
} Bytecode;

void print_bytecode(std::vector<Bytecode> &func);
Ref decompile(RefArg func);

} // lang

} // dyn

#endif // DYN_LANG_DECOMPILE_H


