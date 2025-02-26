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

#include <dyn/ref.h>
#include <dyn/objects.h>
#include <dyn/io/package.h>
#include <dyn/tools/tools.h>
#include <dyn/lang/decompile.h>

#include <gtest/gtest.h>


int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// Using Clang on ARM64, these are all indeed compile time constants!
//constexpr dyn::Symbol gSymObjArray { "array" };
//constexpr dyn::Ref gSymArray { gSymObjArray };
//constexpr dyn::Ref r32 { 32 };

// Creating a read-only Array
//constexpr dyn::Ref aa[] = { 3, 4, 5 };
//constexpr dyn::Array a { gSymArray, 3, aa };

// Creating a read-only Frame
//constexpr dyn::Symbol gSymObjTop { "top" };
//constexpr dyn::Ref gSymTop { gSymObjTop };
//constexpr dyn::Symbol gSymObjLeft { "left" };
//constexpr dyn::Ref gSymLeft { gSymObjLeft };
//constexpr dyn::Ref f_map_symbols[] = { dyn::RefNIL, gSymTop, gSymLeft };
//constexpr dyn::Array f_map { 0, 3, f_map_symbols };
//constexpr dyn::Object v205 { 20.5 };
//constexpr dyn::Ref f_values[] = { 10, v205 };
//constexpr dyn::Frame f { f_map, 2, f_values };

// Create a read-only array
//constexpr dyn::Array gArray { gSymArray, 2, f_values };
//constexpr dyn::Ref gRefArray { gArray };

//constexpr dyn::Object gObjHello { "Hello world!" };
//constexpr dyn::Ref gRefHello { gObjHello };

//constexpr dyn::Object gObjPi { 3.141592654 };
//constexpr dyn::Ref gRefPi { gObjPi };


TEST(DyneRefs, BasicTypes) {
  // -- check integers
  ASSERT_FALSE( dyn::Ref(1234).IsPtr() );
  ASSERT_FALSE( dyn::Ref(1234).IsBoolean() );
  ASSERT_TRUE ( dyn::Ref(1234).IsInt() );
  ASSERT_TRUE ( dyn::Ref(0).IsNotNIL() );
  // -- check unicode characters
  ASSERT_FALSE( dyn::Ref(U'ß').IsPtr() );
  ASSERT_FALSE( dyn::Ref(U'ß').IsInt() );
  ASSERT_TRUE ( dyn::Ref(U'ß').IsChar() );
  // -- check NIL and TRUE
  ASSERT_TRUE ( dyn::RefTRUE.IsTrue() );
  ASSERT_FALSE( dyn::RefTRUE.IsFalse() );
  ASSERT_TRUE ( dyn::RefTRUE.IsBoolean() );
  ASSERT_FALSE( dyn::RefNIL.IsTrue() );
  ASSERT_TRUE ( dyn::RefNIL.IsFalse() );
  ASSERT_TRUE ( dyn::RefNIL.IsNIL() );
  ASSERT_TRUE ( dyn::RefNIL.IsBoolean() );
  ASSERT_TRUE ( dyn::Ref(true).IsBoolean() );
  ASSERT_TRUE ( dyn::Ref(false).IsBoolean() );
  // -- check magic pointers
  ASSERT_TRUE ( dyn::Ref(0, 42).IsMagicPtr() );
  ASSERT_FALSE( dyn::Ref(0, 42).IsInt() );
  ASSERT_FALSE( dyn::Ref(0, 42).IsPtr() );
  // -- check if Ref points to an object
  static constexpr dyn::Symbol array_sym { "array" };
  static constexpr dyn::Ref array_sym_ref { array_sym };
  ASSERT_TRUE( array_sym_ref.IsPtr() );
  ASSERT_FALSE( array_sym_ref.IsInt() );
}

TEST(DyneRefs, GetSet) {
}

