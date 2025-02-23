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

#ifndef DYN_IO_PACKAGE_PACKAGE_BYTES_H
#define DYN_IO_PACKAGE_PACKAGE_BYTES_H

#include <ios>
#include <cstdlib>
#include <vector>

namespace dyn::io {

class PackageBytes : public std::vector<uint8_t>
{
  PackageBytes::iterator it_;

public:
  PackageBytes() = default;
  ~PackageBytes() = default;
  PackageBytes(PackageBytes const& rhs) = delete;
  PackageBytes(PackageBytes const&& rhs) = delete;
  PackageBytes& operator=(PackageBytes const& rhs) = delete;
  PackageBytes& operator=(PackageBytes const&& rhs) = delete;

  void rewind();
  void seek_set(int ix);
  int tell();
  bool eof();
  uint8_t get_ubyte();
  uint16_t get_ushort();
  uint32_t get_uint();
  uint32_t get_ref();
  std::string get_cstring(int n, bool trailing_nul=true);
  std::string get_ustring(int n, bool trailing_nul=true);
  std::vector<uint8_t> get_data(int n);
  void align(int n);
};

}; // namespace dyn::io

#endif // DYN_IO_PACKAGE_PACKAGE_BYTES_H

