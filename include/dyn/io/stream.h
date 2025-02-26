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

#ifndef DYN_IO_STREAM_H
#define DYN_IO_STREAM_H

#include <dyn/io/package/package_bytes.h>

#include <string>

namespace dyn {

class Ref;

namespace io {

class StreamReader
{
  std::shared_ptr<PackageBytes> bytes_ { nullptr };
  dyn::Ref read_next_();
  std::vector<dyn::Ref> precedent_;
public:
  StreamReader();
  ~StreamReader();
  int open(const std::string &filename);
  dyn::Ref read();
  void close();
  static dyn::Ref read(const std::string &filename);
};

} // namespace io

} // namespace dyn

#endif // DYN_IO_STREAM_H


