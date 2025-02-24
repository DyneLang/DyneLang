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

#include <dyn/io/package.h>
#include <dyn/tools/tools.h>
#include <dyn/objects.h>

#include <iostream>
#include <fstream>
#include <ios>
#include <cassert>
#include <memory>

using namespace dyn::io;


/** \class pkg::PartData
 Base class for holding the data in a Newton Package Part
 */


/**
 Index of this Part in the list of Parts in the Package.
 \return 0-based index
 */
int PartData::index() {
  return part_entry_.index();
}


/**
 Compare this part with the other part.
 \param[in] other the other part
 \return 0 if they are the same.
 */
int PartData::compare(PartData &other)
{
  std::cout << "WARNING: Part type not supported;" << std::endl;
  (void)other;
  return -1;
}



/** \class pkg::PartDataGeneric
 Holds the uninterpreted data of a Part with raw data or unknown type.
 */


/**
 Read the Part of the Package as raw data.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int PartDataGeneric::load(PackageBytes &p) {
  data_ = p.get_data(part_entry_.size());
  return 0;
}


/**
 Write raw Package Part data in ARM32 assembler format.
 \param[in] f output stream
 \return number of bytes written
 */
int PartDataGeneric::writeAsm(std::ofstream &f) {
  f << "@ ===== Part " << part_entry_.index() << " Data Generic" << std::endl;
  f << "part_" << part_entry_.index() << ":" << std::endl;
  write_data(f, data_);
  f << "\t.balign\t4" << std::endl << std::endl;
  f << "part_" << part_entry_.index() << "_end:" << std::endl;
  f << "@ ===== Part " << part_entry_.index() << " End" << std::endl << std::endl;
  return part_entry_.size();
}


// MARK: -


/** \class pkg::Object
 Any kind of Object from the Dyne Object System.
 */


/**
 Create a new Object by peeking at the next three words in the package data.
 \param[in] p package data stream
 \return a new instantiation of a class derived from Object
 */
std::shared_ptr<Object> Object::peek(PackageBytes &p, uint32_t offset)
{
  int pos = p.tell();
  uint32_t header_ = p.get_uint();
  p.get_uint();
  uint32_t class_ = p.get_uint();
  p.seek_set(pos);
  switch (header_ & 0x00000003) {
    case 0:
      // TODO: use the symbol to get information and find Reals and ByteCode
      // There are also machine code block, bitmaps, sounds etc. .
      if (class_ == 0x00055552)
        return std::make_shared<ObjectSymbol>(offset); // Symbol
      else
        return std::make_shared<ObjectBinary>(offset); // Binary
    case 1:
      // If the class is an integer, the array is used to store a map
      // for a Frame. Check what flags are set (sorted(1), _proto(4)),
      // and if any map has a supermap.
      if ((class_ & 0x00000003) == 0)
        return std::make_shared<ObjectMap>(offset); // Map
      else
        return std::make_shared<ObjectSlotted>(offset); // Array
      // TODO: what other special class values are there?
    default:
    case 2: return std::make_shared<ObjectBinary>(offset); // Unknown
    case 3: return std::make_shared<ObjectSlotted>(offset); // Frame
  }
}


/**
 Load the head of an Object; derived class must load the remaining data.
 \parm[in] p reference to the biary data
 \return 0 if successful
 */
int Object::load(PackageBytes &p)
{
  uint32_t header = p.get_uint();
  type_ = (header & 0x00000003);
  if (type_ == 2) {
    std::cout << "ERROR: 0x"
      << std::setw(8) << std::setfill('0') << std::hex << p.tell() << std::dec
      << ": NS Object type unknown." << std::endl;
    size_ = 0;
  }
  //  kObjFree      = 0x04,
  //  kObjMarked    = 0x08,
  //  kObjLocked    = 0x10,
  //  kObjForward   = 0x20,
  //  kObjReadOnly  = 0x40,
  //  kObjDirty     = 0x80 can be set in some cases
  flags_ = (header & 0x000000fc);
  if (flags_!=64) // Read Only
    std::cout << "WARNING: NS Object flags should be 0x40, but it's 0x"
    << std::setw(2) << std::setfill('0') << std::hex << (header & 0x000000fc) << std::dec
    << "." << std::endl;
  size_  = ((header >> 8) - 8);
  if (size_ < 0) {
    std::cout << "ERROR: NS Object size <0 found." << std::endl;
    size_ = 0;
  }
  ref_cnt_ = p.get_uint();
  class_ = p.get_ref();
  return 0;
}


/**
 If the data is unaligned, load the filler bytes.
 \param[in] p raw package data stream
 \param[in] start position of first byte of the current part in the package data
 \param[in] align align to 4 bytes (NewtonOS 2.x) or 8 bytes (NewtonOS 1.x)
 */
void Object::loadPadding(PackageBytes &p, uint32_t start, uint32_t align) {
  align--;
  uint32_t fpos = p.tell() - start;
  uint32_t apos = (fpos + align) & ~align;
  uint32_t n = apos - fpos;
  if (n) padding_ = p.get_data(n);
}


/**
 Write the common header data of an NOS object.
 The caption and the actual data is written by the derived class.
 \param[in] f assembler file output stream
 \param[in] p back reference to part data
 \return number of bytes written
 */
int Object::writeAsm(std::ofstream &f, PartDataNOS &p)
{
  (void)p;
  f << label() << ":" << std::endl;
#if 0
  f << "\t.int\t(" << size_ << "+8)<<8 | " << flags_ << " | " << type_;
#else
  f << "\t.int\t(1f-.)<<8 | " << flags_ << " | " << type_;
#endif
  f << ", " << ref_cnt_ << std::endl;
  return 8;
}


/**
 Generate a simple assembler label using the part index and the offset in the package file.
 \param[in] p Part data reference.
 */
void Object::makeAsmLabel(PartDataNOS &p) {
  char buf[32];
  ::snprintf(buf, 31, "obj_%d_%d", p.index(), offset_);
  label_ = buf;
}


int Object::compareBase(Object &other)
{
  int ret = 0;
  if (type() != other.type()) {
    std::cout << "WARNING: Object at " << offset() << ", types differ!" << std::endl;
    return -1;
  }
  if (size() != other.size()) {
    std::cout << "WARNING: Object at " << offset() << ", sizes differ!" << std::endl;
    return -1;
  }
  if (flags_ != other.flags_) {
    std::cout << "WARNING: Object at " << offset() << ", flags differ!" << std::endl;
    ret = -1;
  }
  if (class_ != other.class_) {
    std::cout << "WARNING: Object at " << offset() << ", classes differ!" << std::endl;
    ret = -1;
  }
  return ret;
}


// MARK: -


/** \class pkg::Object
 A Binary Object from the Newton Object System.
 */


/**
 Read a binary object form the Package stream.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int ObjectBinary::load(PackageBytes &p)
{
  Object::load(p);
  data_ = p.get_data(size_-4);
  return 0;
}


/**
 Write a binary object in assembler code.
 We could look at the Class entry of the object to find the actual type and
 write the data in a better understandable format. For example, this is
 used for storing floating point data (class = 'real), so we could instead
 just print one FP value.
 \param[in] f assembler file output stream
 \param[in] p back reference to part data
 \return number of bytes written
 */
int ObjectBinary::writeAsm(std::ofstream &f, PartDataNOS &p)
{
  f << "@ ----- " << offset_ << " Binary Object (" << size_-4 << " bytes)" << std::endl;
  std::string klass = p.getSymbol(class_);
  Object::writeAsm(f, p);
  f << "\t" << p.asmRef(class_) << "\t@ class" << std::endl;
  if (dyn::symcmp(klass.c_str(), "instructions")==0) {
    f << std::setfill(' ');
    int n = (int)data_.size();
    for (int i=0; i<n; ) {
      uint8_t cmd = data_[i++];
      uint8_t a = (cmd & 0xf8) >> 3;
      uint16_t b = (cmd & 0x07);
      if (b==7) {
        b = data_[i]<<8 | data_[i+1]; i += 2;
        f << "\tnscmd3\t" << std::setw(2) << (int)a << ", " << std::setw(5) << (int)b << "\t@ ";
      } else {
        f << "\tnscmd1\t" << std::setw(2) << (int)a << ", " << std::setw(5) << (int)b << "\t@ ";
      }
      switch (a) {
        case 0:
          switch (b) {
            case 0: f << "pop"; break;
            case 1: f << "dup"; break;
            case 2: f << "return"; break;
            case 3: f << "push_self"; break;
            case 4: f << "set_lex_scope"; break;
            case 5: f << "iter_next"; break;
            case 6: f << "iter_done"; break;
            case 7: f << "pop_handlers"; break;
            default:
              std::cout << "WARNING: unknown byte code a:" << a << ", b:" << b << "." << std::endl;
              break;
          }
          break;
        case 3: f << "push " << b; break;
        case 4: f << "push_const " << (int16_t)b; break;
        case 5: f << "call " << b; break;
        case 6: f << "invoke " << b; break;
        case 7: f << "send " << b; break;
        case 8: f << "send_if_defined " << b; break;
        case 9: f << "resend " << b; break;
        case 10: f << "resend_if_defined " << b; break;
        case 11: f << "branch " << (int16_t)b; break;
        case 12: f << "branch_if_true " << (int16_t)b; break;
        case 13: f << "branch_if_false " << (int16_t)b; break;
        case 14: f << "find_var " << b; break;
        case 15: f << "get_var " << b; break;
        case 16: f << "make_frame " << b; break;
        case 17: f << "make_array " << b; break;
        case 18: f << "get_path " << b; break;
        case 19: f << "set_path " << b; break;
        case 20: f << "set_var " << b; break;
        case 21: f << "find_and_set_var " << b; break;
        case 22: f << "incr_var " << b; break;
        case 23: f << "branch_if_loop_not_done " << b; break;
        case 24:
          switch (b) {
            case 0: f << "add"; break;
            case 1: f << "subtract"; break;
            case 2: f << "aref"; break;
            case 3: f << "set_aref"; break;
            case 4: f << "equals"; break;
            case 5: f << "not"; break;
            case 6: f << "not_equals"; break;
            case 7: f << "multiply"; break;
            case 8: f << "divide"; break;
            case 9: f << "div"; break;
            case 10: f << "less_than"; break;
            case 11: f << "greater_than"; break;
            case 12: f << "greater_or_equal"; break;
            case 13: f << "less_or_equal"; break;
            case 14: f << "bit_and"; break;
            case 15: f << "bit_or"; break;
            case 16: f << "bit_not"; break;
            case 17: f << "new_iterator"; break;
            case 18: f << "length"; break;
            case 19: f << "clone"; break;
            case 20: f << "set_class"; break;
            case 21: f << "add_array_slot"; break;
            case 22: f << "stringer"; break;
            case 23: f << "has_path"; break;
            case 24: f << "class_of"; break;
            default:
              std::cout << "WARNING: unknown byte code a:" << a << ", b:" << b << "." << std::endl;
              break;
          }
          break;
        case 25: f << "new_handlers " << b; break;
        default:
          std::cout << "WARNING: unknown byte code a:" << a << ", b:" << b << "." << std::endl;
          break;
      }
      f << std::endl;
    }
    f << std::setfill('0');
  } else if (dyn::symcmp(klass.c_str(), "real")==0) {
    union { uint64_t x; double d; } v;
    ::memcpy(&v.x, &data_[0], 8);
    v.x = htonll(v.x);
    f << "\t@.double\t" << v.d << std::endl;
    write_data(f, data_);
  } else {
    write_data(f, data_);
  }
  return size_;
}


/**
 Compare objects.
 \param[in] other_obj the other object
 \return 0 if they are the same.
 */
int ObjectBinary::compare(Object &other_obj)
{
  int ret = compareBase(other_obj);
  if (ret != 0) return ret;
  ObjectBinary &other = static_cast<ObjectBinary&>(other_obj);
  if (data_ != other.data_) {
    std::cout << "WARNING: Object at " << offset() << ", binary data differs!" << std::endl;
    ret = -1;
  }
  return ret;
}


dyn::Ref ObjectBinary::toNOS(PartDataNOS &p) {
  if (nos_object_)
    return dyn::Ref(nos_object_);
  assert(!marked()); // discover recursion
  mark(true);
  dyn::Ref ret = dyn::RefNIL;
  p.refToNOS(class_); // mark the object as used
  std::string klass = p.getSymbol(class_);
  if (dyn::symcmp(klass.c_str(), "real")==0) {
    union { uint64_t x; double d; } v;
    ::memcpy(&v.x, &data_[0], 8);
    v.x = htonll(v.x);
    ret = dyn::MakeReal(v.d);
  } else if (dyn::symcmp(klass.c_str(), "string")==0) {
    std::u16string s;
    int n = (int)data_.size();
    for (int i=0; i<n; i+=2) {
      uint16_t c = ((data_[i]<<8)|data_[i+1]);
      if (c==0) break;
      s += c;
    }
    ret = dyn::MakeString(utf16_to_utf8(s));
  } else {
    uint32_t class_ref = class_;
    uint32_t data_size = (uint32_t)data_.size();
    dyn::Ref bin = dyn::AllocateBinary(p.refToNOS(class_ref), data_size);
    void *dst = dyn::BinaryData(bin);
    ::memcpy(dst, data_.data(), data_size);
    ret = bin;
  }
  nos_object_ = ret.GetObject();
  return ret;
}


// MARK: -


/** \class pkg::ObjectSymbol
 A Symbol from the Newton Object System.
 */


/**
 Read a symbol form the Package stream.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int ObjectSymbol::load(PackageBytes &p)
{
  Object::load(p);
  hash_ = p.get_uint();
  symbol_ = p.get_cstring(size_-8-1);
#if 0
  uint32_t fpos = p.tell();
  uint32_t apos = (fpos + 7) & ~7;
  uint32_t n = apos - fpos;
  if (n) {
    printf("align at 0x%08x, %d: ", fpos, n);
    for (int i=0; i<n; i++) {
      printf("%02x", p.get_ubyte());
    }
    printf("\n");
  }
  p.seek_set(fpos);
#endif
  return 0;
}


/**
 Write a Symbol in assembler code.
 \param[in] f assembler file output stream
 \param[in] p back reference to part data
 \return number of bytes written
 */
int ObjectSymbol::writeAsm(std::ofstream &f, PartDataNOS &p)
{
  static char hex[] = "0123456789ABCDEF";
  f << "@ ----- " << offset_ << " Symbol (" << size_-9 << " chars)" << std::endl;
  Object::writeAsm(f, p);
  f << "\t.int\t0x"
  << std::setw(8) << std::setfill('0') << std::hex << class_ << ", 0x" << hash_ << std::dec
  << "\t@ hash" << std::endl;

  std::string ascii_symbol;
  for (auto c: symbol_) {
    if (::isprint(c)) {
      ascii_symbol += c;
    } else {
      uint8_t h = (uint8_t)c;
      ascii_symbol += "\\x";
      ascii_symbol += hex[h>>4];
      ascii_symbol += hex[h&0x0f];
    }
  }

  f << "\t.asciz\t\"" << ascii_symbol << "\"" << std::endl;
  return size_;
}


/**
 Create an assembler label for this symbol.

 Symbols can contain characters that are not legal for labels, so we replace
 those with their hex value. We then check the generated label, and if there is
 already the same label in the file, generate a new label by adding and then
 incrementing a counter at the end of the label.

 The label is then stored with the symbol.

 \param[in] p back reference to part data
 */
void ObjectSymbol::makeAsmLabel(PartDataNOS &p) {
  static char hex[] = "0123456789ABCDEF";
  char buf[128];
//  ::snprintf(buf, 31, "sym_%d_%s", p.index(), label_.c_str());
  ::snprintf(buf, sizeof(buf)-1, "sym_%d_", p.index());
  label_ = buf;
  std::string ascii_label;
  for (auto c: symbol_) {
    if ( ::isalnum(c) || (c=='_') ) {
      label_ += c;
    } else {
      uint8_t h = (uint8_t)c;
      label_ += hex[h>>4];
      label_ += hex[h&0x0f];
    }
  }
  if (p.addLabel(label_, this)==false) {
    strncpy(buf, label_.c_str(), sizeof(buf)-7);
    int ins = (int)strlen(buf);
    for (int i=2; ; i++) {
      snprintf(buf+ins, 6, "_%d", i);
      label_ = buf;
      if (p.addLabel(label_, this)) break;
    }
  }
}


/**
 Compare objects.
 \param[in] other_obj the other object
 \return 0 if they are the same.
 */
int ObjectSymbol::compare(Object &other_obj)
{
  int ret = compareBase(other_obj);
  if (ret != 0) return ret;
  ObjectSymbol &other = static_cast<ObjectSymbol&>(other_obj);
  if (hash_ != other.hash_) {
    std::cout << "WARNING: Object at " << offset() << ", symbol hashes differ!" << std::endl;
    ret = -1;
  }
  if (symbol_ != other.symbol_) {
    std::cout << "WARNING: Object at " << offset() << ", symbols differ!" << std::endl;
    ret = -1;
  }
  return ret;
}


dyn::Ref ObjectSymbol::toNOS(PartDataNOS &) {
  if (nos_object_)
    return dyn::Ref(nos_object_);
  assert(!marked()); // discover recursion
  mark(true);
  dyn::Ref ret = dyn::Sym(symbol());
  nos_object_ = ret.GetObject();
  return ret;
}


// MARK: -


/** \class pkg::ObjectSlotted
 A Slotted Object (Frame or Array) from the Newton Object System.
 */


/**
 Read a slotted object (Frame or Array) form the Package stream.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int ObjectSlotted::load(PackageBytes &p)
{
  Object::load(p);
  int i = 0, n = size_/4-1;
  for ( ; i<n; ++i) {
    ref_list_.push_back(p.get_ref());
  }
  return 0;
}


/**
 Write a slotted object (a Form or an Array) in assembler code.
 \param[in] f assembler file output stream
 \param[in] p back reference to part data
 \return number of bytes written
 */
int ObjectSlotted::writeAsm(std::ofstream &f, PartDataNOS &p)
{
  if (type_ == 1) {
    f << "@ ----- " << offset_ << " Array (" << (size_/4)-1 << " entries)" << std::endl;
    Object::writeAsm(f, p);
    f << "\t" << p.asmRef(class_) << "\t@ class" << std::endl;
  } else {
    f << "@ ----- " << offset_ << " Frame (" << (size_/4)-1 << " entries)" << std::endl;
    Object::writeAsm(f, p);
    f << "\t" << p.asmRef(class_) << "\t@ map" << std::endl;
  }
  for (auto &ref: ref_list_) {
    f << "\t" << p.asmRef(ref) << "\t@ ref" << std::endl;
  }
  return size_;
}


/**
 Compare objects.
 \param[in] other_obj the other object
 \return 0 if they are the same.
 */
int ObjectSlotted::compare(Object &other_obj)
{
  int ret = compareBase(other_obj);
  if (ret != 0) return ret;
  ObjectSlotted &other = static_cast<ObjectSlotted&>(other_obj);
  if (ref_list_ != other.ref_list_) {
    std::cout << "WARNING: Object at " << offset() << ", list of Refs differ!" << std::endl;
    ret = -1;
  }
  return ret;
}


dyn::Ref ObjectSlotted::toNOS(PartDataNOS &p) {
  if (nos_object_)
    return dyn::Ref(nos_object_);
  assert(!marked()); // discover recursion
  mark(true);
  dyn::Ref ret = dyn::RefNIL;
  if (type_ == 1) {
    dyn::Ref class_ref = p.refToNOS(class_);
    dyn::Ref array = dyn::AllocateArray(class_ref, 0);
    int i, n = (int)ref_list_.size();
    for (i=0; i<n; ++i) {
      dyn::Ref value = p.refToNOS(ref_list_[i]);
      dyn::AddArraySlot(array, value);
    }
    ret = array;
  } else if (type_ == 3) {
    dyn::Ref frame = dyn::AllocateFrame();
    // TODO: check if class_ is really a map
    p.refToNOS(class_); // mark as created, it will be linked later if is actually used
    ObjectMap *map = static_cast<ObjectMap*>(p.object_at(class_));
    map->mark(true);
    int i, n = (int)ref_list_.size();
    for (i=0; i<n; ++i) {
      dyn::Ref tag = p.refToNOS(map->symbol_at(i));
      dyn::Ref value = p.refToNOS(ref_list_[i]);
      dyn::SetFrameSlot(frame, tag, value);
    }
    ret = frame;
  } else {
    std::cout << "ERROR: Slotted Object has unknown type!" << std::endl;
    ret = dyn::RefNIL;
  }
  nos_object_ = ret.GetObject();
  return ret;
}


// MARK: -


/** \class pkg::ObjectMap
 An Array of Symbols, as used by the Frame Object to create named indexing.
 */


/**
 Write a Frame lookup map object in assembler code.
 \param[in] f assembler file output stream
 \param[in] p back reference to part data
 \return number of bytes written
 */
int ObjectMap::writeAsm(std::ofstream &f, PartDataNOS &p)
{
  f << "@ ----- " << offset_ << " Map (" << (size_/4)-2 << " entries)" << std::endl;
  Object::writeAsm(f, p);
  f << "\t" << p.asmRef(class_) << "\t@ flags" << std::endl;
  // Flags can be 1 (kMapSorted), 2(kMapShared), 4 (kMapProto)
  if (((class_>>2) & ~(1+2+4)) != 0)
    std::cout << "WARNING: Unknown map flag set: " << (class_>>2) << std::endl;
  if (ref_list_.size() > 0) {
    f << "\t" << p.asmRef(ref_list_[0]) << "\t@ supermap";
#if 1
    // Yes, we use supermaps in Packages which will make life slightly harder
    if (ref_list_[0] != 0x00000002) {
      std::cout << "WARNING: map references a supermap!" << std::endl;
      f << " to SUPERMAP";
    }
#endif
    f << std::endl;
    int i, n = (int)ref_list_.size();
    for (i=1; i<n; ++i) {
      f << "\t" << p.asmRef(ref_list_[i]) << "\t@ ref" << std::endl;
    }
  }
  return size_;
}


// TODO: supermaps!
uint32_t ObjectMap::symbol_at(int index)
{
  if (ref_list_[0]!=2) {
    std::cout << "ERROR: supermap not supported" << std::endl;
  }
  return ref_list_[index+1];
}


dyn::Ref ObjectMap::toNOS(PartDataNOS &p) {
  return ObjectSlotted::toNOS(p);
}


// MARK: -


/** \class pkg::PartDataNOS
 All the data in a NOS Part of the Package.
 */


/**
 Read the NOS Part of the Package as a list of Objects.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int PartDataNOS::load(PackageBytes &p) {
  int start = p.tell();
  int n = start + part_entry_.size();
  
  p.get_uint();
  uint32_t align_bit = p.get_uint();
  if (align_bit & 0x00000001) {
    align_ = 4;
    align_fill_ = 0xbfbfbfbf;
  }
  p.seek_set(start);

  while (p.tell() < n) {
    uint32_t offset = p.tell();
    auto o = Object::peek(p, offset);
    o->load(p);
    object_list_[offset] = o;
    o->loadPadding(p, start, align_);
  }

  for (auto &obj: object_list_) {
    obj.second->makeAsmLabel(*this);
  }

  return 0;
}


/**
 Write NOS Package Part data in ARM32 assembler format.
 \param[in] f output stream
 \return number of bytes written
 */
int PartDataNOS::writeAsm(std::ofstream &f) {
  f << "@ ===== Part " << part_entry_.index() << " Data NOS" << std::endl;
  f << "part_" << part_entry_.index() << ":" << std::endl;
  f << std::endl;
  for (auto &obj: object_list_) {
    obj.second->writeAsm(f, *this);
    f << "1:" << std::endl;
#if 0
    write_data(f, obj.second->padding_);
#else
    if (align_==4) {
      f << "\t.balign\t4, 0xbf\n";
    } else {
      int n_fill = (int)obj.second->padding_.size();
      if (n_fill > 0)
        f << "\t.space\t" << n_fill << ", 0xbf\n";
    }
#endif
  }
  f << "\t.balign\t4" << std::endl << std::endl;
  f << "part_" << part_entry_.index() << "_end:" << std::endl;
  f << "@ ===== Part " << part_entry_.index() << " End" << std::endl << std::endl;
  return part_entry_.size();
}


/**
 Return the start of an assembler line that will produce the given Ref.
 \param[in] ref a valid Ref
 \return[in] a temporary string with the assembler code
 */
std::string PartDataNOS::asmRef(uint32_t ref)
{
  static char buf[80];
  switch (ref & 3) {
    case 0: // integer
      ::snprintf(buf, 79, "ref_integer\t%d", ref/4);
      break;
    case 1: // pointer
      if (object_list_.find(ref&~3) != object_list_.end()) {
        ::snprintf(buf, 79, "ref_pointer\t%s", object_list_[ref&~3]->label().c_str());
      } else {
        std::cout << "WARNING: Invalid reference to offset " << (ref&~3) << "." << std::endl;
        ::snprintf(buf, 79, "ref_pointer_invalid\t0x%08x", ref);
      }
      break;
    case 2: // special
      if (ref == 2) {
        ::snprintf(buf, 79, "ref_nil");
      } else if (ref == 0x1a) {
        ::snprintf(buf, 79, "ref_true");
      } else if ((ref & 15) == 10) {
        uint32_t c = ref >> 4;
        if (c>=32 && c<127)
          ::snprintf(buf, 79, "ref_unichar '%c'", (char)c);
        else
          ::snprintf(buf, 79, "ref_unichar %d", c);
      } else {
        ::snprintf(buf, 79, ".int\t0x%08x", ref);
      }
      break;
    case 3: // magic
      ::snprintf(buf, 79, "ref_magic\t%d", ref/4);
      break;
  }
  return std::string(buf);
}


/**
 Return a symbol from a reference.
 If the Ref is not a symbol, we return an empty string.
 \param[in] ref a valid Ref
 \return[in] a string containing the mixed case symbol text, or empty if ref
      does not reference a symbol
 */
std::string PartDataNOS::getSymbol(uint32_t ref)
{
  if ( (ref&3)==1 ) {
    auto obj_it = object_list_.find(ref&~3);
    if ( obj_it != object_list_.end() ) {
      auto up_obj = obj_it->second;
      Object *obj = &(*obj_it->second);
      ObjectSymbol *sym = dynamic_cast<ObjectSymbol*>(obj);
      if (sym) {
        return sym->symbol();
      }
    }
  }
  return std::string("");
}


/**
 Try to add a label to the list of unique labels.

 This is used to make labels unique within the part. The method is called
 with a suggestion for a label text. If the label is taken, it will return
 false, giving the caller the opportunity to try again with a modified label,
 for example by adding a number that increments until the label is unique.
 If the label is not yet taken, it is added to the list and true is retuned.

 \note The label text and the symbol text may be different to match requirements
 of the assembler and to make the unique if the same symbol exists
 more than once.

 \param[in] label ASCII string
 \param[in] symbol if the label is unique, it will reference this symbol
 */
bool PartDataNOS::addLabel(std::string label, ObjectSymbol *symbol) {
  if (label_list_.find(label) == label_list_.end()) {
    // not found, add the label
    label_list_[label] = symbol;
    return true;
  } else {
    // already in list, caller may try again with another label
    return false;
  }
}


/**
 Compare this NOS part with the other NOS part.
 \param[in] other_part the other part which must be NOS as well
 \return 0 if they are the same.
 */
int PartDataNOS::compare(PartData &other_part)
{
  int ret = 0;
  PartDataNOS &other = static_cast<PartDataNOS&>(other_part);
  if (object_list_.size() != other.object_list_.size()) {
    std::cout << "WARNING: Part " << part_entry_.index() << ", object list sizes differ!" << std::endl;
    return -1;
  }
  auto other_obj = other.object_list_.begin();
  for (auto obj = object_list_.begin(); obj != object_list_.end(); ++obj, ++other_obj) {
    if (obj->second->compare(*other_obj->second) !=0)
      ret = -1;
  }
  return ret;
}


Object *PartDataNOS::object_at(uint32_t offset)
{
  return object_list_[offset&~3].get();
}


/**
 Convert this part of the package into a Newton OS object tree.
 \return the object tree or an error code as an integer
 */
dyn::Ref PartDataNOS::toNOS()
{
  // Mark all objects as not yet written.
  for (auto &obj: object_list_)
    obj.second->mark(false);

  // The first object must be an array with one element that is the root of 
  // the tree.
  auto root_it = object_list_.begin();
  ObjectSlotted *root_obj = static_cast<ObjectSlotted*>(root_it->second.get());
  root_obj->mark(true);
  uint32_t data_ref = root_obj->slot(0);
  Object *data_obj = object_at(data_ref);
  dyn::Ref nos_form = data_obj->toNOS(*this);

#if 0
  // Count the objects that were not written.
  int unmarked = 0;
  for (auto &obj: object_list_) {
    if (!obj.second->marked()) {
      unmarked++;
      printf("Unmarked object at %d, %s\n", obj.first, obj.second->label().c_str());
    }
  }
  if (unmarked > 0)
    std::cout << "WARNING: " << unmarked << " objects not converted!" << std::endl;
#endif
  
  return nos_form;
}


dyn::Ref PartDataNOS::refToNOS(uint32_t ref) {
//  static char buf[80];
  switch (ref & 3) {
    case 0: { // integer
      int32_t s = static_cast<int32_t>(ref);
      dyn::Integer v = (dyn::Integer(s))/4;
      return dyn::Ref(v); }
    case 1: { // pointer
      Object *obj = object_at(ref);
      if (obj)
        return object_at(ref)->toNOS(*this);
      else
        std::cout << "WARNING: Invalid reference to offset " << (ref&~3) << "." << std::endl;
      return dyn::RefUNREF; }
    case 2: // special
      switch (ref & 0x0c) {
        case 0x00: // special
          if (ref == 2) {
            return dyn::RefNIL;
          } else if (ref == 0x0032) {
            return dyn::Ref::kPlainFuncClass;
          } else if (ref == 0x0132) {
            return dyn::Ref::kPlainCFunctionClass;
          } else if (ref == 0x0232) {
            return dyn::Ref::kBinCFunctionClass;
          } else {
            std::cout << "WARNING: Unknown special Ref " << ref
            << ", 0x" << std::setw(8) << std::setfill('0') << std::hex << ref << std::dec
            << "." << std::endl;
            return dyn::Ref(dyn::Ref::Verbatim_(ref));
          }
        case 0x04: // char
          return dyn::Ref(static_cast<dyn::UniChar>(ref>>4));
        case 0x08: // boolean
          if (ref == 0x1a) {
            return dyn::RefTRUE;
          } else {
            std::cout << "WARNING: Unsupported boolean reference " << ref
            << ", 0x" << std::setw(8) << std::setfill('0') << std::hex << ref << std::dec
            << "." << std::endl;
            return dyn::Ref(dyn::Ref::Verbatim_(ref));
          }
        case 0x0c: // reserved
          std::cout << "WARNING: Resereved reference " << ref
          << ", 0x" << std::setw(8) << std::setfill('0') << std::hex << ref << std::dec
          << "." << std::endl;
          return dyn::Ref(dyn::Ref::Verbatim_(ref));
      }
    case 3: // Make Magic Pointer
      return dyn::Ref(ref>>14, (ref>>4)&0xfff);
  }
  return dyn::RefUNREF;
}
