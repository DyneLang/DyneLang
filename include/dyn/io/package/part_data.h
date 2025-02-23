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

#ifndef DYN_IO_PACKAGE_PART_DATA_H
#define DYN_IO_PACKAGE_PART_DATA_H

#include <dyn/ref.h>

#include <ios>
#include <cstdlib>
#include <vector>
#include <map>
#include <memory>

namespace dyn::io {

class PartEntry;
class PackageBytes;

class PartData {
protected:
  PartEntry &part_entry_;
public:
  PartData(PartEntry &part_entry) : part_entry_(part_entry) { }
  virtual ~PartData() = default;
  virtual int load(PackageBytes &p) = 0;
  virtual int writeAsm(std::ofstream &f) = 0;
  virtual int compare(PartData &other);
  virtual dyn::Ref toNOS() { return dyn::RefNIL; }
  int index();
};

class PartDataGeneric : public PartData {
  std::vector<uint8_t> data_;
public:
  PartDataGeneric(PartEntry &part_entry) : PartData(part_entry) { }
  ~PartDataGeneric() override = default;
  int load(PackageBytes &p) override;
  int writeAsm(std::ofstream &f) override;
};

class PartDataNOS;

class Object {
protected:
  std::string label_;
  uint32_t offset_{ 0 };
  uint32_t type_ { 0 };
  uint32_t flags_ { 0 };
  uint32_t size_ { 0 };
  uint32_t ref_cnt_ { 0 };
  uint32_t class_{ 0 };
  bool mark_ { false };
  dyn::Object *nos_object_ { nullptr };
public: // TODO: hack
  std::vector<uint8_t> padding_;
public:
  static std::shared_ptr<Object> peek(PackageBytes &p, uint32_t offset);
  Object(uint32_t offset) : offset_(offset) { }
  virtual ~Object() = default;
  virtual int load(PackageBytes &p);
  void loadPadding(PackageBytes &p, uint32_t start, uint32_t align);
  virtual int writeAsm(std::ofstream &f, PartDataNOS &p);
  virtual void makeAsmLabel(PartDataNOS &p);
  virtual int compare(Object &other_obj) = 0;
  virtual dyn::Ref toNOS(PartDataNOS &p) = 0;
  int compareBase(Object &other);
  std::string &label() { return label_; }
  uint32_t type() const { return type_; }
  uint32_t offset() const { return offset_; }
  uint32_t size() const { return size_; }
  void mark(bool v) { mark_ = v; }
  bool marked() { return mark_; }
};

class ObjectBinary : public Object {
  std::vector<uint8_t> data_;
public:
  ObjectBinary(uint32_t offset) : Object(offset) { }
  int load(PackageBytes &p) override;
  int writeAsm(std::ofstream &f, PartDataNOS &p) override;
  int compare(Object &other_obj) override;
  dyn::Ref toNOS(PartDataNOS &p) override;
};

class ObjectSymbol : public Object {
  uint32_t hash_{ 0 };
  std::string symbol_;
public:
  ObjectSymbol(uint32_t offset) : Object(offset) { }
  int load(PackageBytes &p) override;
  int writeAsm(std::ofstream &f, PartDataNOS &p) override;
  void makeAsmLabel(PartDataNOS &p) override;
  int compare(Object &other_obj) override;
  std::string symbol() { return symbol_; }
  dyn::Ref toNOS(PartDataNOS &p) override;
};

class ObjectSlotted : public Object {
protected:
  std::vector<uint32_t> ref_list_;
public:
  ObjectSlotted(uint32_t offset) : Object(offset) { }
  int load(PackageBytes &p) override;
  int writeAsm(std::ofstream &f, PartDataNOS &p) override;
  int compare(Object &other_obj) override;
  uint32_t slot(int i) { return ref_list_[i]; }
  dyn::Ref toNOS(PartDataNOS &p) override;
};

class ObjectMap : public ObjectSlotted {
public:
  ObjectMap(uint32_t offset) : ObjectSlotted(offset) { }
  uint32_t symbol_at(int index);
  int writeAsm(std::ofstream &f, PartDataNOS &p) override;
  dyn::Ref toNOS(PartDataNOS &p) override;
};

class PartDataNOS : public PartData {
  std::map<uint32_t, std::shared_ptr<Object>> object_list_;
  std::map<std::string, ObjectSymbol*> label_list_;
  uint32_t align_{ 8 };
  uint32_t align_fill_{ 0xadbadbad };
public:
  PartDataNOS(PartEntry &part_entry) : PartData(part_entry) { }
  ~PartDataNOS() override = default;
  int load(PackageBytes &p) override;
  int writeAsm(std::ofstream &f) override;
  std::string asmRef(uint32_t ref);
  std::string getSymbol(uint32_t ref);
  bool addLabel(std::string label, ObjectSymbol *symbol);
  int compare(PartData &other_part) override;
  Object *object_at(uint32_t offset);
  dyn::Ref toNOS() override;
  dyn::Ref refToNOS(uint32_t ref);
};

} // namespace dyn::io

#endif // DYN_IO_PACKAGE_PART_DATA_H
