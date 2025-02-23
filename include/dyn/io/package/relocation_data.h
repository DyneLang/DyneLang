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

#ifndef DYN_IO_PACKAGE_PACKAGE_DATA_H
#define DYN_IO_PACKAGE_PACKAGE_DATA_H

#include <iostream>
#include <fstream>
#include <ios>
#include <cstdlib>
#include <vector>
#include <memory>

namespace dyn::io {

class PackageBytes;

class RelocationSet {
  uint16_t page_number_{ 0 };
  uint16_t offset_count_{ 0 };
  std::vector<uint8_t> offset_list_;
  std::vector<uint8_t> padding_;
public:
  RelocationSet() = default;
  int load(PackageBytes &p);
  int writeAsm(std::ofstream &f);
};

class RelocationData {
  uint32_t reserved_ {0};
  uint32_t size_ {0};
  uint32_t page_size_ {0};
  uint32_t num_entries_ {0};
  uint32_t base_address_ {0};
  std::vector<RelocationSet> relocation_set_list_;
  std::vector<uint8_t> padding_;
public:
  RelocationData() = default;
  int load(PackageBytes &p);
  int writeAsm(std::ofstream &f);
};

} // namespace dyn::io

#endif // DYN_IO_PACKAGE_PACKAGE_DATA_H

