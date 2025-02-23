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

#ifndef DYN_IO_PRINT_H
#define DYN_IO_PRINT_H

#include <cstdio>
#include <cstdint>

namespace dyn::io {

class PrintState {
public:
//  prettyPrint: true,
//  printDepth: 3,
//  printLength: nil,
//  uint32_t print_length_{ std::numeric_limits<uint32_t>::max() };
  uint32_t print_depth_{ 8 };
  uint32_t current_depth_{ 0 };
  std::FILE *out_{ nullptr };
  bool sym_next_{ false };
public:
  PrintState(std::FILE *fout);
  ~PrintState();
  void tab();
  bool more_depth(); // TODO: bad naming
  bool incr_depth(); // TODO: return value not used
  void decr_depth();
  void expect_symbol(bool s) { sym_next_ = s; }
  bool symbol_expected() { return sym_next_; }
};

} // namespace dyn::io

#endif // DYN_IO_PRINT_H

