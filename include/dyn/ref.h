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

class Ref
{
private:
  union {
    uintptr_t r_;
    Object*   o_;
  };

  static constexpr uint8_t kTagPointer    = 0x00;
  static constexpr uint8_t kTagInteger    = 0x01;
  static constexpr uint8_t kTagImmed      = 0x02;
  static constexpr uint8_t kTagMagicPtr   = 0x03;
  static constexpr uint8_t kTagMask       = 0x03;
  static constexpr uint8_t kTagBits       = 2;
  static constexpr uint8_t kTagShift      = 2;
  static constexpr uint8_t kTagValueBits  = sizeof(uintptr_t)*4-kTagShift;

  static constexpr uint8_t kImmedSpecial  = 0x00;
  static constexpr uint8_t kImmedChar     = 0x04;
  static constexpr uint8_t kImmedBoolean  = 0x08;
  static constexpr uint8_t kImmedReserved = 0x0c;
  static constexpr uint8_t kImmedMask     = 0x0c;
  static constexpr uint8_t kImmedBits     = 2;
  static constexpr uint8_t kImmedShift    = 4;
  static constexpr uint8_t kImmedValueBits = sizeof(uintptr_t)*4-kImmedShift;

  uint8_t   tag_() const          { return r_ & kTagMask; }
  intptr_t  tag_value_() const    { return static_cast<intptr_t>(r_) >> kTagShift; }
  uint8_t   immed_() const        { return r_ & kImmedMask; }
  uintptr_t immed_value_() const  { return static_cast<uintptr_t>(r_) >> kImmedShift; }

public:

  using Verbatim_ = uintptr_t;
  constexpr Ref(Verbatim_ r)        : r_{ r } { };
  constexpr Ref()                   : r_ { 0x000FFFF2 } { };
  constexpr Ref(Integer i)          : r_{ (((uintptr_t)i)<<kTagShift)|kTagInteger } {}
  constexpr Ref(int i)              : r_{ (((uintptr_t)i)<<kTagShift)|kTagInteger } {}
  constexpr Ref(UniChar u)          : r_{ (((uintptr_t)u)<<kImmedShift)|kTagImmed|kImmedChar } {}
  constexpr Ref(int table, int ix)  : r_{ (((uintptr_t)table)<<14) | (((uintptr_t)ix)<<kTagShift)|kTagMagicPtr } {}
  constexpr Ref(Boolean i)          : r_{ (uintptr_t)(i ? 0x1a : 0x02) } {}
  constexpr Ref(const Object &obj)  : o_{ const_cast<Object*>(&obj) } { }
  constexpr Ref(Object *obj)        : o_{ obj } { }

  // --- move new style w/information hiding here
  constexpr bool operator==(const Ref &other) const { return r_ == other.r_; }
  constexpr bool IsPtr() const        { return (r_&0x03)==0x00; }
  constexpr bool IsInt() const        { return (r_&0x03)==0x01; }
  constexpr bool IsBoolean() const    { return IsTrue() || IsFalse(); }
  constexpr bool IsTrue() const       { return r_==0x1a; }
  constexpr bool IsFalse() const      { return IsNIL(); }
  constexpr bool IsNIL() const        { return r_==0x02; }
  constexpr bool IsNotNIL() const     { return !IsNIL(); }
  constexpr bool IsChar() const       { return (r_&0x0f)==0x06; }
  constexpr bool IsMagicPtr() const   { return (r_&0x03)==0x03; }

  bool IsBinary() const;
  bool IsArray() const;
  bool IsFrame() const;
  bool IsSymbol() const;
  bool IsReadOnly() const;

  Object *GetObject() const { return IsPtr() ? o_ : nullptr; }

  int Print(dyn::io::PrintState &ps) const;


  //extern  bool    IsRealPtr(Ref r);
  
  static const Ref kWeakArrayClass;
  static const Ref kFaultBlockClass;
  static const Ref kFuncClass;
  static const Ref kBadPackageRef;
  static const Ref kUnstreamableObject;
  static const Ref kSymbolClass;
  static const Ref kPlainFuncClass;
  static const Ref kPlainCFunctionClass;
  static const Ref kBinCFunctionClass;
};

constexpr Ref RefNIL          { (Ref::Verbatim_)0x00000002 };
constexpr Ref RefTRUE         { (Ref::Verbatim_)0x0000001a };
constexpr Ref RefSymbolClass  { (Ref::Verbatim_)0x00055552 };
constexpr Ref RefUNREF        { (Ref::Verbatim_)0x000FFFF2 };


constexpr bool IsPtr(Ref r) { return r.IsPtr(); }
inline bool IsBinary(Ref r) { return r.IsBinary(); }
inline bool IsArray(Ref r) { return r.IsArray(); }
inline bool IsFrame(Ref r) { return r.IsFrame(); }

//inline Ref MakeInt(int i) { return




//int  _RPTRError(Ref r) { ThrowBadTypeWithFrameData(kNSErrNotAPointer, r); return 0; }
//int  _RINTError(Ref r) { ThrowBadTypeWithFrameData(kNSErrNotAnInteger, r); return 0; }
//int  _RCHARError(Ref r) { ThrowBadTypeWithFrameData(kNSErrNotACharacter, r); return 0; }
//
//Ref    AddressToRef(void * inAddr) { return (Ref) inAddr & kRefValueMask; }
//void *  RefToAddress(Ref r) { return (void *)(ISINT(r) ? r & kRefValueMask : _RINTError(r)); }
//int    RefToInt(Ref r) { return RINT(r); }
//UniChar  RefToUniChar(Ref r) { return RCHAR(r); }
//inline long  RINT(Ref r)    { return ISINT(r) ? RVALUE(r) : _RINTError(r); }
//inline UniChar  RCHAR(Ref r)  { return ISCHAR(r) ? (UniChar) RIMMEDVALUE(r) : _RCHARError(r); }


//______________________________________________________________________________
// Macros as Functions

//extern  Ref     MakeInt(int i);
//extern  Ref     MakeChar(unsigned char c);
//extern  Ref    MakeBoolean(int val);
//
//extern  bool    IsInt(Ref r);
//extern  bool    IsChar(Ref r);
//extern  bool    IsPtr(Ref r);
//extern  bool    IsMagicPtr(Ref r);
//extern  bool    IsRealPtr(Ref r);
//
//extern  Ref    AddressToRef(void *);
//extern  void *  RefToAddress(Ref r);
//extern  int    RefToInt(Ref r);
//extern  UniChar  RefToUniChar(Ref r);


} // namespace dyn

#endif // DYN_REF_H

