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

#ifndef DYN_IO_PACKAGE_H
#define DYN_IO_PACKAGE_H

#include <dyn/ref.h>

#include <dyn/io/package/package_bytes.h>
#include <dyn/io/package/part_data.h>
#include <dyn/io/package/part_entry.h>
#include <dyn/io/package/relocation_data.h>


#include <iostream>
#include <fstream>
#include <ios>
#include <cstdlib>
#include <memory>

namespace dyn::io {

class PartEntry;
class PackageBytes;

class Package {
  std::string signature_ { };
  std::string type_ { };
  uint32_t flags_ {0};
  uint32_t version_ {0};
  uint16_t copyright_start_ {0};
  uint16_t copyright_length_ {0};
  uint16_t name_start_ {0};
  uint16_t name_length_ {0};
  uint32_t size_ {0};
  uint32_t date_ {0};
  uint32_t reserved2_ {0};
  uint32_t reserved3_ {0};
  uint32_t directory_size_ {0};
  uint32_t num_parts_ {0};
  uint32_t vdata_start_ {0};
//  uint32_t info_start_ {0};
  uint32_t info_length_ {0};
  std::vector<std::shared_ptr<PartEntry>> part_ { };
  std::string copyright_ { };
  std::string name_ { };
  std::vector<uint8_t> info_ { };
  RelocationData relocation_data_;

  std::string file_name_ { };
  std::shared_ptr<PackageBytes> pkg_bytes_ { nullptr };

  int load();
  int writeAsm(std::ofstream &f);
  int compare(Package &other);

public:
  Package() = default;
  ~Package() = default;
  Package(Package const& rhs) = delete;
  Package(Package const&& rhs) = delete;
  Package& operator=(Package const& rhs) = delete;
  Package& operator=(Package const&& rhs) = delete;

  int load(const std::string &package_file_name);
  int writeAsm(const std::string &assembler_file_name);
  int compareFile(const std::string &other_package_file);
  int compareContents(const std::string &other_package_file);
  dyn::Ref toNOS();
};


} // namespace dyn::io

#endif // DYN_IO_PACKAGE_H


