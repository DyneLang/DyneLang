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
  return IsPtr() && o->IsBinary();
}

bool Ref::IsArray() const {
  return IsPtr() && o->IsArray();
}

bool Ref::IsFrame() const {
  return IsPtr() && o->IsFrame();
}

bool Ref::IsSymbol() const {
  return IsPtr() && o->IsSymbol();
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
  switch (v.tag_) {
    case Tag::pointer:
      o->Print(ps);
      break;
    case Tag::integer:
      fprintf(ps.out_, "%ld", (Integer)v.value_);
      break;
    case Tag::immed:
      switch (i.type_) {
        case Type::unichar:
          std::fprintf(ps.out_, "$%s", unicode_to_utf8((UniChar)i.value_).c_str());
          break;
        case Type::special:
          if (i.value_==0) {
            std::fprintf(ps.out_, "NIL");
          } else {
            std::fprintf(ps.out_, "[undefined special: %ld]", i.value_);
          }
          break;
        case Type::boolean:
          if (i.value_==1) {
            std::fprintf(ps.out_, "TRUE");
          } else {
            std::fprintf(ps.out_, "[undefined boolean: %ld]", i.value_);
          }
          break;
        case Type::reserved:
          if (sizeof(t)==4)
            std::fprintf(ps.out_, "[reserved: 0x%08lx]", t);
          else
            std::fprintf(ps.out_, "[reserved: 0x%086lx]", t);
          break;
      }
      break;
    case Tag::magic:
      std::fprintf(ps.out_, "@%ld", v.value_);
      break;
  }

  return 0;
}


