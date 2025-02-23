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

using namespace dyn::io;

/** \class pkg::RelocationSet
 One array of up to 256 location where a word of data must be fixed to be
 relative to the relocation base.
 */

/**
 Load a single package and word-align the input stream.
 \param[in] p Reference to the package data stream.
 \return 0
 */
int RelocationSet::load(PackageBytes &p)
{
  page_number_ = p.get_ushort();
  offset_count_ = p.get_ushort();
  offset_list_ = p.get_data(offset_count_);

  uint32_t fpos = p.tell();
  uint32_t apos = (fpos + 3) & ~3;
  uint32_t n = apos - fpos;
  padding_ = p.get_data(n);
  return 0;
}

/**
 Write the relocation set to the assembler file.

 \note This does not use labels, so the result may be false if data changes its
 position (bytes are inserted or deleted).

 \param[in] f write to this text stream
 \return number of bytes converted to assembler
 */
int RelocationSet::writeAsm(std::ofstream &f)
{
  f << "@ ----- Relocation Set" << std::endl;
  f << "\t.short\t" << (int)page_number_ << "\t@ page_number" << std::endl;
  f << "\t.short\t" << (int)offset_count_ << "\t@ offset_count_" << std::endl;
  for (auto o: offset_list_) {
    int offset_in_part_data = o*4 + page_number_*1024;
    f << "\t.byte\t" << (int)o << "\t@ relocate word at " << offset_in_part_data << std::endl;
  }
  write_data(f, padding_);
  f << std::endl;
  return (int)(4 + offset_list_.size() + padding_.size());
}

/** \class pkg:RelocationData
 Header data set for all relocation data.
 */

/**
 Read relocation data from the package.
 \todo We are not interpreting the Relocation Sets yet and just save the binary
 data. It will be interesting to see how many Packages use this feature.
 \param[in] p package data stream
 \return 0 if succeeded
 */
int RelocationData::load(PackageBytes &p) {
  int start = p.tell();
  reserved_ = p.get_uint();
  size_ = p.get_uint();
  page_size_ = p.get_uint();
  num_entries_ = p.get_uint();
  base_address_ = p.get_uint();
  for (int i=0; i<(int)num_entries_; ++i) {
    relocation_set_list_.push_back(RelocationSet());
    int result = relocation_set_list_[i].load(p);
    if (result != 0)
      return -1;
  }
  int pading_size_ = start + size_ - p.tell();
  if (pading_size_ > 0) {
    padding_ = p.get_data(pading_size_);
  } else if (pading_size_ < 0) {
    std::cout << "ERROR: Relocation Data padding is negative." << std::endl;
    return -1;
  }
  return 0;
}

/**
 Write relocation data in ARM32 assembler format.
 \todo The output is not yet symbolic. Absolute values are used
 \param[in] f output stream
 \return number of bytes written
 */
int RelocationData::writeAsm(std::ofstream &f) {
  f << "@ ===== Relocation Data" << std::endl;
  f << "\t.int\t" << reserved_ << "\t@ reserved" << std::endl;
  f << "\t.int\t" << size_ << "\t@ size" << std::endl;
  f << "\t.int\t" << page_size_ << "\t@ page_size" << std::endl;
  f << "\t.int\t" << num_entries_ << "\t@ num_entries" << std::endl;
  f << "\t.int\t" << base_address_ << "\t@ base_address" << std::endl;
  for (auto &set: relocation_set_list_) {
    set.writeAsm(f);
  }
  write_data(f, padding_);
  f << std::endl;
  return size_;
}


