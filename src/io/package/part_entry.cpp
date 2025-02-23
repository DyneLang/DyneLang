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
#include <dyn/objects.h>

#include <cassert>
#include <iomanip>
#include <ios>

using namespace dyn::io;

/** \class pkg::PartEntry
 The directory entry for each part in the package file.
 */

/**
 Hold all the Part attributes, but not the Part Data.
 \param[in] ix index within the Package Part list
 */
PartEntry::PartEntry(int ix)
  : index_(ix) { }

/**
 Size of the Part Data block.
 \return bytes in the Part Data
 */
int PartEntry::size()
{
  return size_;
}

/**
 Index within the Package Part list.
 \return 0-based index
 */
int PartEntry::index()
{
  return index_;
}

/**
 Read the Part attributes of the Package
 \param[in] p package data stream
 \return 0 if succeeded
 */
int PartEntry::load(PackageBytes &p) {
  offset_ = p.get_uint();
  size_ = p.get_uint();
  size2_ = p.get_uint();
  // As opposed to the documentation, size and size2 occasionally differ
  //if (size_ != size2_)
  //  std::cout << "WARNING: Part Entry " << index_ << ": Size and size2 differ.\n";
  type_ = p.get_cstring(4, false);
  // 'form' 'book' 'dict' 'auto' 'comm'
  // Foud: "auto", "", "soup", "book", "cdhl", "prnt"
//  if (type_=="book" || type_=="dict" || type_=="comm")
//    std::cout << "WARNING: Part Entry " << index_ << ": unsupported type \"" << type_ << "\"\n";
//  if (type_!="form" && type_!="book" && type_!="dict" && type_!="comm")
//    std::cout << "WARNING: Part Entry " << index_ << ": unknown type \"" << type_ << "\"\n";
  reserved_ = p.get_uint();
  flags_ = p.get_uint();
  if (flags_ & 0xfffffe0c)
    std::cout << "WARNING: Part Entry " << index_ << ": unknown flag: "
    << std::setw(8) << std::setfill('0') << std::hex
    << (flags_ & 0xfffffe0c) << std::dec << std::endl;
  info_offset_ = p.get_ushort();
  info_length_ = p.get_ushort();
  compressor_offset_ = p.get_ushort();
  compressor_length_ = p.get_ushort();

  switch (flags_ & 3) {
    case 0: // kProtocolPart (found in Apple packages)
      std::cout << "WARNING: Protocol Parts not yet understood." << std::endl;
      part_data_ = std::make_shared<PartDataGeneric>(*this);
      break;
    case 1: // kNOSPart
      part_data_ = std::make_shared<PartDataNOS>(*this);
      break;
    case 2: // kRawPart
      std::cout << "WARNING: Raw Parts not yet understood." << std::endl;
      part_data_ = std::make_shared<PartDataGeneric>(*this);
      break;
    case 3: // kPackagePart
      std::cout << "WARNING: Package Parts in Packages not yet understood." << std::endl;
      part_data_ = std::make_shared<PartDataGeneric>(*this);
      break;
  }

  return 0;
}

/**
 Read the optional Info field.
 If the Package was created with NTK, this defaults to
 "A Newton Toolkit application".
 \param[in] p package data stream
 \return 0 if succeeded
 */
int PartEntry::loadInfo(PackageBytes &p) {
  if (info_length_ > 0)
    info_ = p.get_cstring(info_length_, false);
  return 0;
}

/**
 Read the part data using an interpreter for the format as set in the flags.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int PartEntry::loadPartData(PackageBytes &p) {
  return part_data_->load(p);
}

/**
 Write the Package Part attributes in ARM32 assembler format.
 \param[in] f output stream
 \return number of bytes written
 */
int PartEntry::writeAsm(std::ofstream &f) {
  f << "@ ===== Part Entry " << index_ << std::endl;
  f << "\t.int\t" << offset_ << "\t@ offset" << std::endl;
#if 0
  f << "\t.int\t" << size_ << "\t@ size" << std::endl;
  f << "\t.int\t" << size2_ << "\t@ size2" << std::endl;
#else
  f << "\t.int\tpart_" << index() << "_end-part_" << index() << "\t@ size" << std::endl;
  f << "\t.int\tpart_" << index() << "_end-part_" << index() << "\t@ size2" << std::endl;
#endif
  f << "\t.ascii\t\"" << type_ << "\"\t@ type\n";
  f << "\t.int\t" << reserved_ << "\t@ reserved" << std::endl;
  f << "\t.int\t0x" << std::setw(8) << std::setfill('0') << std::hex << flags_ << std::dec << "\t@ flags\n";
  static const std::string lut[] = { "kProtocolPart", "kNOSPart", "kRawPart", "UNKNOWN"};
  f << "\t\t@ " << lut[flags_ & 3] << std::endl;
  if (flags_ & 0x000001f0) {
    f << "\t\t@";
    if (flags_ & 0x00000010) f << " kAutoLoadPartFlag";
    if (flags_ & 0x00000020) f << " kAutoRemovePartFlag";
    if (flags_ & 0x00000040) f << " kCompressedFlag";
    if (flags_ & 0x00000080) f << " kNotifyFlag";
    if (flags_ & 0x00000100) f << " kAutoCopyFlag";
    f << std::endl;
  }
  if (flags_ & 0xfffffe0c)
    f << "\t@ WARNING unknown flag: "
    << std::setw(8) << std::setfill('0') << std::hex
    << (flags_ & 0xfffffe0c) << std::dec << std::endl;
#if 0
  f << "\t.short\t" << info_offset_ << ", " << info_length_ << "\t@ info" << std::endl;
#else
  f << "\t.short\tpart" << index_ << "info_start, part" << index_ << "info_end-part" << index_ << "info_start\t@ info" << std::endl;
#endif
  f << "\t.short\t" << compressor_offset_ << ", " << compressor_length_ << "\t@ compressor" << std::endl;
  f << std::endl;
  return 32;
}

/**
 Write the optional Info filed in ARM32 assembler format.
 \param[in] f output stream
 \return number of bytes written
 */
int PartEntry::writeAsmInfo(std::ofstream &f) {
  f << "@ ----- Part " << index_ << " Info" << std::endl;
  f << "part" << index_ << "info_start:" << std::endl;
  if (info_length_)
    f << "\t.ascii\t\"" << info_ << "\"\t@ info" << std::endl;
  f << "part" << index_ << "info_end:" << std::endl << std::endl;
  return info_length_;
}

/**
 Write the Part Data.
 \param[in] f output stream
 \return number of bytes written
 */
int PartEntry::writeAsmPartData(std::ofstream &f) {
  return part_data_->writeAsm(f);
}

/**
 Compare this part entry with the other part entry.
 \param[in] other the other part entry
 \return 0 if they are the same.
 */
int PartEntry::compare(PartEntry &other)
{
  int ret = 0;
  if (size_ != other.size_) {
    std::cout << "WARNING: Part " << index_ << ", sizes differ!" << std::endl;
    return -1;
  }
  if (type_ != other.type_) {
    std::cout << "WARNING: Part " << index_ << ", types differ!" << std::endl;
    return -1;
  }
  if (flags_ != other.flags_) {
    std::cout << "WARNING: Part " << index_ << ", flags differ!" << std::endl;
    ret = -1;
  }
  if (info_ != other.info_) {
    std::cout << "WARNING: Part " << index_ << ", info texts differ!" << std::endl;
    ret = -1;
  }
  if (ret != 0)
    return ret;

  return part_data_->compare(*other.part_data_);
}

/**
 Convert this part of the package into a Dyne object tree.
 \return the object tree or an error code as an integer
 */
dyn::Ref PartEntry::toNOS() {
  auto part = dyn::AllocateFrame();
  dyn::SetFrameSlot(part, dyn::Sym("type"), dyn::MakeString(type_));
  dyn::SetFrameSlot(part, dyn::Sym("flags"), (int)flags_);
  dyn::SetFrameSlot(part, dyn::Sym("info"), dyn::MakeString(info_));
  // TODO: compressor?
  // TODO: check part type!
  switch (flags_ & 3) {
    case 0: // kProtocolPart
      dyn::SetFrameSlot(part, dyn::Sym("warning"),
                        dyn::MakeString("WARNING: Protocol Parts not yet understood."));
      break;
    case 1: // kNOSPart
      dyn::SetFrameSlot(part, dyn::Sym("data"), part_data_->toNOS());
      break;
    case 2: // kRawPart
      dyn::SetFrameSlot(part, dyn::Sym("warning"),
                        dyn::MakeString("WARNING: Raw Parts not yet understood."));
      break;
    case 3: // kPackagePart
      dyn::SetFrameSlot(part, dyn::Sym("warning"),
                        dyn::MakeString("WARNING: Package Parts in Packages not yet understood."));
      break;
  }
  return part;
}
