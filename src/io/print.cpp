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

#include <dyn/io/print.h>

#include <dyn/ref.h>


using namespace dyn::io;

PrintState::PrintState(FILE *fout)
  : out_(fout)
{
}

PrintState::~PrintState()
{
}

void PrintState::tab() {
  for (uint32_t i=0; i<current_depth_; ++i)
    fprintf(out_, "  ");
}

bool PrintState::more_depth() {
  return (current_depth_ < print_depth_);
}

bool PrintState::incr_depth() {
  if (current_depth_ < print_depth_) {
    current_depth_++;
    return true;
  } else {
    return false;
  }
}

void PrintState::decr_depth() {
  if (current_depth_ > 0)
    current_depth_--;
}

/**
 Print any Ref.
 */
void dyn::Print(RefArg p)
{
  dyn::io::PrintState state(stdout);
  p.Print(state);
  fprintf(state.out_, "\n");
}

