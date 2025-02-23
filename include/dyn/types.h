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

#ifndef DYN_TYPES_H
#define DYN_TYPES_H

#include <cstdint>

namespace dyn {

using Boolean = bool;
using Integer = long;
using Index = long;
using Size = long;
using Real = double;
using UniChar = char32_t;
using Magic = int;

using Ptr = void*;
using NativePtr = void*;
using ObjectPtr = class Object*;

using RefArg = const class Ref;

using DyneErr = int;

using Coord = int;

struct Rect
{
  Coord  top;
  Coord  left;
  Coord  bottom;
  Coord  right;
};
typedef struct Rect Rect;

Ref MakeInt(Integer i);
Ref MakeChar(UniChar c);
Ref MakeBool(Boolean b);
Ref MakeMagic(Index i);

Boolean IsNil(RefArg r);
Boolean NotNil(RefArg r);
Boolean IsFalse(RefArg r);
Boolean IsTrue(RefArg r);
Boolean IsBinary(RefArg ref);
Boolean IsArray(RefArg ref);
Boolean IsFrame(RefArg ref);
Boolean IsNumber(Ref ref);
Boolean IsPathExpr(RefArg ref);

Ref AllocateArray(RefArg theClass, Size length);

void Print(RefArg);

} // namespace dyn

#endif // DYN_TYPES_H

