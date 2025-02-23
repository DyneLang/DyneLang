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

#ifndef DYN_IO_PACKAGE_PART_ENTRY_H
#define DYN_IO_PACKAGE_PART_ENTRY_H

#include <dyn/ref.h>

#include <iostream>
#include <fstream>
#include <ios>
#include <cstdlib>
#include <memory>

namespace dyn::io {

class PartData;
class PackageBytes;

class PartEntry {
  int index_;
  uint32_t offset_ {0};
  uint32_t size_ {0};
  uint32_t size2_ {0};
  std::string type_;
  uint32_t reserved_ {0};
  uint32_t flags_ {0};
  uint16_t info_offset_ {0};
  uint16_t info_length_ {0};
  uint16_t compressor_offset_ {0};
  uint16_t compressor_length_ {0};
  std::string info_;
  std::shared_ptr<PartData> part_data_;
public:
  PartEntry(int ix);
  int size();
  int index();
  int load(PackageBytes &p);
  int loadInfo(PackageBytes &p);
  int loadPartData(PackageBytes &p);
  int writeAsm(std::ofstream &f);
  int writeAsmInfo(std::ofstream &f);
  int writeAsmPartData(std::ofstream &f);
  int compare(PartEntry &other);
  dyn::Ref toNOS();
};

} // namespace dyn::io

#endif // DYN_IO_PACKAGE_PART_ENTRY_H

