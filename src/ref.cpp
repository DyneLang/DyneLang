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

#include <dyn/ref.h>
#include <dyn/objects.h>
#include <dyn/tools/tools.h>
#include <dyn/io/print.h>

#include <iostream>
#include <cstring>

using namespace dyn;

// TODO: static_assert(sizeof(Ref)==sizeof(uintptr_t), "'Ref' has unexpected size.");

bool Ref::IsBinary() const {
  return IsPtr() && o_->IsBinary();
}

bool Ref::IsArray() const {
  return IsPtr() && o_->IsArray();
}

bool Ref::IsFrame() const {
  return IsPtr() && o_->IsFrame();
}

bool Ref::IsSymbol() const {
  return IsPtr() && o_->IsSymbol();
}

bool Ref::IsReadOnly() const {
  auto obj = GetObject();
  if (obj)
    return obj->IsReadOnly();
  else
    return false;
}

int Ref::Print(dyn::io::PrintState &ps) const
{
  switch (tag_()) {
    case kTagPointer:
      o_->Print(ps);
      break;
    case kTagInteger:
      fprintf(ps.out_, "%ld", tag_value_());
      break;
    case kTagImmed:
      switch (immed_()) {
        case kImmedChar:
          std::fprintf(ps.out_, "$%s", unicode_to_utf8((UniChar)immed_value_()).c_str());
          break;
        case kImmedSpecial:
          if (*this == RefNIL) {
            std::fprintf(ps.out_, "NIL");
          } else if (*this == Ref::kPlainFuncClass) {
            if (!ps.symbol_expected()) std::fputc('\'', ps.out_);
            std::fprintf(ps.out_, "__PlainFuncClass");
          } else if (*this == Ref::kPlainCFunctionClass) {
            if (!ps.symbol_expected()) std::fputc('\'', ps.out_);
            std::fprintf(ps.out_, "__PlainCFunctionClass");
          } else if (*this == Ref::kBinCFunctionClass) {
            if (!ps.symbol_expected()) std::fputc('\'', ps.out_);
            std::fprintf(ps.out_, "__BinCFunctionClass");
          } else {
            std::fprintf(ps.out_, "[ERROR: undefined special: %ld]", immed_value_());
          }
          break;
        case kImmedBoolean:
          if (immed_value_() == 1) {
            std::fprintf(ps.out_, "TRUE");
          } else {
            std::fprintf(ps.out_, "[ERROR: undefined boolean: %ld]", immed_value_());
          }
          break;
        case kImmedReserved:
          if (sizeof(r_)==4)
            std::fprintf(ps.out_, "[ERROR: reserved: 0x%08lx]", r_);
          else
            std::fprintf(ps.out_, "[ERROR: reserved: 0x%016lx]", r_);
          break;
      }
      break;
    case kTagMagicPtr: {
      int table = static_cast<int>(r_ >> 14);
      int index = static_cast<int>((r_ >> 4) & 0x00000fff);
      if (table) {
        std::fprintf(ps.out_, "@%d.%d", table, index);
      } else {
        std::fprintf(ps.out_, "@%d", index);
      }
      break; }
  }

  return 0;
}

const Ref Ref::kWeakArrayClass      { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(1<<kImmedShift)) };
const Ref Ref::kFaultBlockClass     { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(2<<kImmedShift)) };
const Ref Ref::kPlainFuncClass      { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(0x03<<kImmedShift)) };
const Ref Ref::kPlainCFunctionClass { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(0x13<<kImmedShift)) };
const Ref Ref::kBinCFunctionClass   { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(0x23<<kImmedShift)) };
const Ref Ref::kBadPackageRef       { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(4<<kImmedShift)) };
const Ref Ref::kUnstreamableObject  { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(5<kImmedShift)) };
const Ref Ref::kSymbolClass         { (Ref::Verbatim_)(kTagImmed|kImmedSpecial|(0x5555<<kImmedShift)) };
