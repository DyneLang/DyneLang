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

#ifndef DYN_OBJECTS_H
#define DYN_OBJECTS_H

#include <dyn/ref.h>

#include <string>
#include <vector>
#include <stdexcept>

namespace dyn {

class Object;
class SlottedObject;
class Array;
class Frame;
class Map;
class Symbol;

namespace io {

class PrintState;

} // namespace io

class alignas(uintptr_t) Object
{
  friend class Ref;

protected:
  enum class Tag: uint8_t {
    binary, array, large_binary, frame,
    real, symbol, native_ptr, reserved
  };

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  typedef struct {
    uint8_t type0_:1;
    uint8_t type1_:1;
    uint8_t type2_:1;
    uint8_t marked_:1;
    uint8_t locked_:1;
    uint8_t forward_:1;
    uint8_t read_only_:1;
    uint8_t dirty_:1;
  } TagFlags;

  typedef struct {
    Tag tag_:3;
    uint8_t flags_:5;
  } Tags;
#else
#error "Sorry, not implemented"
#endif

  typedef struct {
    Ref    class_;
    char   *data_;
  } Binary_;

  typedef struct {
    Ref    class_;
//    const  IndirectBinaryProcs * procs;
    char   *data_;
  } LargeBinary_;

  typedef struct {
    Ref    class_;
    Ref    *slot_;
    uint32_t reserve_;
  } Array_;

  typedef struct {
    Map    *map_;
    Ref    *slot_;
    uint32_t reserve_;
  } Frame_;

  typedef struct {
    Ref    class_;
    Real   value_;
  } Real_;

  typedef struct {
    Ref    class_;
    char   *string_;
    uint32_t hash_;
  } Symbol_;

  typedef struct {
    Ref    class_;
    void  *value_;
  } NativePtr_;

  union {
    TagFlags f;
    Tags t;
    uint8_t all_flags_;
  };
  uint32_t size_:24;
  uint32_t gc_ { 0 };
  union {
    Binary_ binary;
    LargeBinary_ lbo;
    Array_ array;
    Frame_ frame;
    Real_ real;
    Symbol_ symbol;
    NativePtr_ ptr;
  };
//  Object(const Object&) = delete;
//  Object(Object&&) = delete;
//  Object &operator=(const Object&) = delete;
//  Object &operator=(Object&&) = delete;
  // a little trick to get a constexpr strlen()
  static constexpr uint32_t _strlen(const char* str) {
    return *str ? 1 + _strlen(str + 1) : 0;
  }
  static constexpr uint32_t _hash1(const char* str) {
    uint8_t c = (uint8_t)str[0];
    if (c>='a' && c<='z') c -= 32;
    return *str ? c + _hash1(str+1) : 0;
  }
  static constexpr uint32_t _hash(const char* str) {
    return _hash1(str) * 0x9E3779B9;
  }

  constexpr Object(const Binary_ a, uint32_t size)
  : t { Tag::binary, 0x10 },
  size_ { size },
  binary { a }
  { }

  constexpr Object(const Array_ a, uint32_t num_slots)
  : t { Tag::array, 0x10 },
    size_ { static_cast<uint32_t>(num_slots*sizeof(Ref)) },
    array { a }
  { }

  constexpr Object(const Frame_ f, uint32_t num_slots)
  : t { Tag::frame, 0x10 },
    size_ { static_cast<uint32_t>(num_slots*sizeof(Ref)) },
    frame { f }
  { }

  Object(bool, Frame_ f, uint32_t num_slots)
  : t { Tag::frame, 0x00 },
  size_ { static_cast<uint32_t>(num_slots*sizeof(Ref)) },
  frame { f }
  { }

  constexpr Object(const Symbol_ sym_arg)
  : t { Tag::symbol, 0x10 },
    size_ { static_cast<uint32_t>(_strlen(sym_arg.string_)+1) },
    symbol { sym_arg }
  { }

public:

  constexpr Object(const char *str);
  Object(const std::string &str);
  constexpr Object(Real value);

  Index size() const { return size_; }
  uint32_t gc() const { return gc_; }

  constexpr bool IsBinary() const { return (t.tag_ == Tag::binary); }
  constexpr bool IsArray() const { return (t.tag_ == Tag::array); }
  constexpr bool IsFrame() const { return (t.tag_ == Tag::frame); }
  constexpr bool IsSymbol() const { return (t.tag_ == Tag::symbol); }
  constexpr bool IsReadOnly() const { return (f.read_only_ == 1); }

  int SymbolCompare(const Object *other) const;

  int Print(dyn::io::PrintState &ps) const;

};

class BinaryObject: public Object
{
public:
  BinaryObject(RefArg theClass, Index size, void *data)
  : Object( Binary_{ theClass, (char*)data }, (uint32_t)size ) { }
  void *Data() { return (void*)binary.data_; }
};

class SlottedObject: public Object
{
public:
  constexpr SlottedObject(const Array_ a, uint32_t num_slots)
  : Object { a, num_slots } { }
  constexpr SlottedObject(const Frame_ f, uint32_t num_slots)
  : Object { f, num_slots } { }
  Index Length() const;
  void SetLength(Index new_length);
  Ref GetSlot(Index i) const;
  void SetSlot(Index ix, RefArg value);
};

class Array: public SlottedObject
{
public:
  constexpr Array(Ref obj_class, uint32_t num_slots, const Ref *values)
  : SlottedObject( Array_{ obj_class, const_cast<Ref*>(values), 0 }, num_slots) { }
  Array(RefArg theClass);
  Array(RefArg theClass, Index length);
  int Print(dyn::io::PrintState &ps) const;
  Index AddSlot(RefArg value);
};

class Map: public Array
{
public:
  constexpr Map(Ref obj_class, uint32_t num_slots, const Ref *values)
  : Array{obj_class, num_slots, values } { }
  Map(RefArg theClass);
  Map(RefArg theClass, Index length);
};

class Frame: public SlottedObject
{
public:
  constexpr Frame(Map *map, uint32_t num_slots, const Ref *values)
  : SlottedObject( Frame_{ map, const_cast<Ref*>(values), 0 }, num_slots) { }
  Frame();
  int Print(dyn::io::PrintState &ps) const;
  void SetSlot(RefArg tag, RefArg value);
  Ref GetSlot(Index i) const { return SlottedObject::GetSlot(i); }
  Ref GetSlot(RefArg tag) const;
  Index AddSlot(RefArg tag);
};

class Symbol: public Object
{
public:
  constexpr Symbol(const char *symbol)
  : Object( Symbol_{ RefSymbolClass, const_cast<char*>(symbol), _hash(symbol) } ) { }
  int Print(dyn::io::PrintState &ps) const;
};


int symcmp(const char *a, const char *b);
int SymbolCompare(Ref sym1, Ref sym2);

constexpr Symbol gSymObjString { "string" };
constexpr Ref gSymString { gSymObjString };

constexpr Symbol gSymObjInstructions { "instructions" };
constexpr Ref gSymInstructions { gSymObjInstructions };

constexpr Symbol gSymObjReal { "real" };
constexpr Ref gSymReal { gSymObjReal };

constexpr Object::Object(const char *str)
: t { Tag::binary, 0x10 }, size_{ _strlen(str)+1 }, binary{ gSymString, const_cast<char*>(str) }
{ }

constexpr Object::Object(Real value)
: t { Tag::real, 0x10 }, size_{ 0 }, real { gSymReal, value }
{ }


Ref AllocateFrame();
void SetFrameSlot(RefArg obj, RefArg slot, RefArg value);
Ref GetFrameSlot(RefArg obj, RefArg slot);
Ref AllocateArray(RefArg obj_class, Index length);
Ref AllocateArray(Index length);
Index FindOffset(Ref map, Ref tag);
Index AddArraySlot(RefArg array_ref, RefArg value);
bool IsReadOnly(RefArg ref);
Ref GetArraySlot(RefArg array_obj, Index slot);
void SetArraySlot(RefArg array, Index slot, RefArg value);
Ref MakeString(const char *str);
inline Ref MakeString(const std::string &str) { return MakeString(str.c_str()); }
Ref Sym(const char *name);
inline Ref Sym(const std::string &name) { return Sym(name.c_str()); }

Ref AllocateBinary(RefArg theClass, Index length);
Ptr BinaryData(Ref r);

Ref MakeReal(double d);

//#define  MAKECHAR(c)        MAKEIMMED(kImmedChar, (unsigned) c)
//#define  MAKEMAGICPTR(index)  ((Ref) (((long) (index)) << kRefTagBits) | kTagMagicPtr)
//extern  Ref    AddressToRef(void *);
//extern  void *  RefToAddress(Ref r);
//extern  int    RefToInt(Ref r);
//extern  UniChar  RefToUniChar(Ref r);




constexpr Symbol kSymArray { "array" };
constexpr Ref kRefArray { kSymArray };


// TODO: move these into their own header

class RuntimeError : public std::runtime_error
{
protected:
  DyneErr err_;
public:
  RuntimeError(DyneErr err, const std::string& msg = "")
  : std::runtime_error(msg), err_(err) {}
};

class BadTypeWithFrameData : public RuntimeError
{
public:
  BadTypeWithFrameData(DyneErr err, const std::string& msg = "") : RuntimeError(err, msg) {}
};

class FramesWithBadValue : public RuntimeError
{
public:
  FramesWithBadValue(DyneErr err, const std::string& msg = "") : RuntimeError(err, msg) {}
};



} // namespace dyn

#endif // DYN_OBJECTS_H

