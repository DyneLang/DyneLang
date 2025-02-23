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

#include <iostream>
#include <fstream>
#include <ios>
#include <cstdlib>
#include <locale>
#include <codecvt>


const std::string gnu_as { "/opt/homebrew/bin/arm-none-eabi-as" };
const std::string gnu_objcopy { "/opt/homebrew/bin/arm-none-eabi-objcopy" };

//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/Mines/Mines.pkg" };
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/SuperNewtris2.0/SNewtris.pkg" };
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/DeepGreen1.0b3/deepgreen10b3.pkg" }; // contains relocation data
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/GoldTeeAtBighorn/Goldtee.pkg" }; // Contains Floatin Point values
std::string input_pkg_name { "/Users/matt/Azureus/unna/games/NewTiles1.2/newtiles-1_2.pkg" }; // 'package1'

/**
 Call the GNU assembler to create an object file form the assembler file.
 \param[in] assembler_file_name
 \param[in] object_file_name
 \return 0 if successful
 */
int asmToObj(std::string assembler_file_name, std::string object_file_name)
{
  std::string cmd = gnu_as + " -march=armv4 -mbig-endian "
                           + "\"" + assembler_file_name + "\" "
                           + "-o \"" + object_file_name + "\"";
  if (std::system(cmd.c_str()) != 0) {
    std::cout << "asmToObj: Unable to generate object file:" << std::endl
              << "  " << cmd << std::endl;
    return -1;
  }
  
//  std::cout << "asmToObj: Wrote \"" << object_file_name << "\"." << std::endl;
  return 0;
}

/**
 Call GNU objcopy to extract the binary .data segment, creating a new Package.
 \param[in] object_file_name
 \param[in] new_package_name file path and name
 \return 0 if successful
 */
int objToBin(std::string object_file_name, std::string new_package_name)
{
  std::string cmd = gnu_objcopy + " -O binary -j .data "
                                + "\"" + object_file_name + "\" "
                                + "\"" + new_package_name + "\"";
  if (std::system(cmd.c_str()) != 0) {
    std::cout << "objToBin: Unable to generate binary file:" << std::endl
    << "  " << cmd << std::endl;
    return -1;
  }

//  std::cout << "objToBin: Wrote \"" << new_package_name << "\"." << std::endl;
  return 0;
}

/**
 Run our application with hardcoded file names for now.
 \param[in] argc, argv
 */
int main(int argc, const char * argv[])
{
  if (argc==2) {
    input_pkg_name = argv[1];
  }

  std::cout << "Testing package \"" << input_pkg_name << "\"." << std::endl;

  dyn::io::Package my_pkg;

  if (my_pkg.load(input_pkg_name) < 0) {
    std::cout << "ERROR reading package file." << std::endl;
    return 0;
  }
#if 0
  if (my_pkg.writeAsm("/Users/matt/dev/newtfmt.git/mines.s") < 0) {
    std::cout << "ERROR writing assembler file." << std::endl;
    return 0;
  }
  if (asmToObj("/Users/matt/dev/newtfmt.git/mines.s", "/Users/matt/dev/newtfmt.git/mines.o") < 0) {
    std::cout << "ERROR calling assembler and creating object file." << std::endl;
    return 0;
  }
  if (objToBin("/Users/matt/dev/newtfmt.git/mines.o", "/Users/matt/dev/newtfmt.git/mines.pkg") < 0) {
    std::cout << "ERROR extracting binary data from object file." << std::endl;
    return 0;
  }
  if (my_pkg.compareContents("/Users/matt/dev/newtfmt.git/mines.pkg") < 0) {
    std::cout << "ERROR comparing the original package and the new package contents." << std::endl;
    return 0;
  }
#endif
#if 0
  if (my_pkg.compareFile("/Users/matt/dev/newtfmt.git/mines.pkg") < 0) {
    std::cout << "ERROR comparing the binaries of the original package and the new package." << std::endl;
    return 0;
  }
#endif
  dyn::Ref nos_pkg = my_pkg.toNOS();
  dyn::Print(nos_pkg);
  std::cout << "OK." << std::endl;
  return 0;
}

#if 0

// Some Globals:
//  prettyPrint: true,
//  printDepth: 3,
//  printLength: nil,

// Using Clang on ARM64, these are all indeed compile time constants!
constexpr dyn::Symbol gSymObjArray { "array" };
constexpr dyn::Ref gSymArray { gSymObjArray };
constexpr dyn::Ref r32 { 32 };

// Creating a read-only Array
constexpr dyn::Ref aa[] = { 3, 4, 5 };
constexpr dyn::Array a { gSymArray, 3, aa };

// Creating a read-only Frame
constexpr dyn::Symbol gSymObjTop { "top" };
constexpr dyn::Ref gSymTop { gSymObjTop };
constexpr dyn::Symbol gSymObjLeft { "left" };
constexpr dyn::Ref gSymLeft { gSymObjLeft };
constexpr dyn::Ref f_map_symbols[] = { dyn::RefNIL, gSymTop, gSymLeft };
constexpr dyn::Array f_map { 0, 3, f_map_symbols };
constexpr dyn::Object v205 { 20.5 };
constexpr dyn::Ref f_values[] = { 10, v205 };
constexpr dyn::Frame f { f_map, 2, f_values };

// Create a read-only array
constexpr dyn::Array gArray { gSymArray, 2, f_values };
constexpr dyn::Ref gRefArray { gArray };

constexpr dyn::Object gObjHello { "Hello world!" };
constexpr dyn::Ref gRefHello { gObjHello };

constexpr dyn::Object gObjPi { 3.141592654 };
constexpr dyn::Ref gRefPi { gObjPi };

int main(int argc, const char * argv[])
{
  (void)argc; (void)argv;

  dyn::Print(gSymArray);  // compile time symbol
  dyn::Print(a);          // test arrays
  dyn::Print(f);          // test arrays
  dyn::Print(r32);        // compile time integer
  dyn::Print(U'ü');       // support for lower 8 bit unicode characters
  dyn::Print(U'😀');      // support for full sized unicode characters
  dyn::Print(gRefHello);  // static strings
  dyn::Print(gRefPi);     // floating point values
  dyn::Print(gArray);     // array
  dyn::Print(gRefArray);  // array

  return 0;
}

#endif
