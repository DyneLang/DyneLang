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

#ifndef DYN_REF_H
#define DYN_REF_H

#include <dyn/types.h>

namespace dyn {

namespace io {

class PrintState;

} // namespace io

constexpr int kRefTagBits = 2;
constexpr int kRefValueBits = sizeof(uintptr_t) * 8 - kRefTagBits;
constexpr uintptr_t kRefValueMask = (~(uintptr_t)0) << kRefTagBits;
constexpr uintptr_t kRefTagMask = ~kRefValueMask;
constexpr int kRefImmedBits = 2;
constexpr uintptr_t kRefImmedMask = (~(uintptr_t)0) << kRefImmedBits;

constexpr uint32_t kTagPointer  = 0;
constexpr uint32_t kTagInteger  = 1;
constexpr uint32_t kTagImmed    = 2;
constexpr uint32_t kTagMagicPtr = 3;

constexpr int nBits = 30;

#ifndef __BYTE_ORDER__
#error byte order not defined
#endif

class Ref
{
public:
  enum class Tag: uint8_t {
    pointer, integer, immed, magic
  };

  enum class Type: uint8_t {
    special, unichar, boolean, reserved
  };

private:
  typedef struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    Tag tag_:kRefTagBits;
    Integer value_:kRefValueBits;
#else
    Integer value_:kRefValueBits;
    Tag tag_:kRefTagBits;
#endif
  } Value;

  typedef struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    Tag tag_:kRefTagBits;
    Type type_:kRefImmedBits;
    uintptr_t value_:kRefValueBits-kRefImmedBits;
#else
    uintptr_t value_:kRefValueBits-kRefImmedBits;
    Type type_:kRefImmedBits;
    Tag tag_:kRefTagBits;
#endif
  } Immed;

  union {
    uintptr_t t;
    Value v;
    Immed i;
//  Magic table:18, index:12, Tag:2
    Object *o;
  };

public:

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  constexpr Ref() : i { Tag::immed, Type::special, 0 } { };
  constexpr Ref(Type type, Integer ival) : i { Tag::immed, type, (unsigned long)ival } { };
  constexpr Ref(Integer i) : v { Tag::integer, i } {}
  constexpr Ref(int i) : v { Tag::integer, (Integer)i } {}
  constexpr Ref(UniChar u) : i { Tag::immed, Type::unichar, u } {}
#else
  constexpr Ref() : i { 0, Type::special, Tag::immed } { };
  constexpr Ref(Type type, Integer ival) : i { (unsigned long)ival, type, Tag::immed } { };
  constexpr Ref(Integer i) : v { (unsigned long)i, Tag::integer } {}
  constexpr Ref(int i) : v { (unsigned long)i, Tag::integer } {}
  constexpr Ref(UniChar u) : i { u, Type::unichar, Tag::immed } {}
#endif
  constexpr Ref(const Object &obj): o(const_cast<Object*>(&obj)) { }
  constexpr Ref(Object *obj): o(obj) { }

//  constexpr Ref(const Ref &other) { i = other.i; }
//  Ref(Ref &&other);
//  Ref &operator=(const Ref &other);
//  Ref &operator=(Ref &&other);
//  ~Ref() = default;
//  void Assign(Integer i);
//  Boolean IsInteger();
//
//  Ref(Object *o): v { ((unsigned long)o)>>kRefTagBits, Tag::pointer } { }
//  void Assign(ObjectPtr o);
//  Boolean IsObject();
//
//  constexpr Ref(Object &o): ref_(&o) { o.read_only_ = false; o.incr_ref_count(); }
//
//  void AddArraySlot(RefArg value) const;
  constexpr bool operator==(const Ref &other) const { return t == other.t; }

  constexpr bool IsPtr() const { return (v.tag_ == Tag::pointer); }

  bool IsBinary() const;
  bool IsArray() const;
  bool IsFrame() const;
  bool IsSymbol() const;
  bool IsReadOnly() const;

  Object *GetObject() const { return IsPtr() ? o : nullptr; }

  int Print(dyn::io::PrintState &ps) const;
};

constexpr Ref RefNIL;
constexpr Ref RefTRUE { Ref::Type::boolean, 1 };
constexpr Ref RefSMILE { U'😀' };
constexpr Ref RefPYTHON { 42 };
constexpr Ref RefSymbolClass { Ref::Type::special, 0x5555 };

constexpr bool IsPtr(Ref r) { return r.IsPtr(); }
inline bool IsBinary(Ref r) { return r.IsBinary(); }
inline bool IsArray(Ref r) { return r.IsArray(); }
inline bool IsFrame(Ref r) { return r.IsFrame(); }



} // namespace dyn

#endif // DYN_REF_H

