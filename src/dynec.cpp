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


// TODO: move the code below into some functional validator file and offer
// to add (and default to) stress tests at compile time.

const std::string gnu_as { "/opt/homebrew/bin/arm-none-eabi-as" };
const std::string gnu_objcopy { "/opt/homebrew/bin/arm-none-eabi-objcopy" };

//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/Mines/Mines.pkg" };
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/SuperNewtris2.0/SNewtris.pkg" };
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/DeepGreen1.0b3/deepgreen10b3.pkg" }; // contains relocation data
std::string input_pkg_name { "/Users/matt/Azureus/unna/games/GoldTeeAtBighorn/Goldtee.pkg" }; // pkg0: Contains Floatin Point values
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/NewTiles1.2/newtiles-1_2.pkg" }; // pkg1: 'package1'
//std::string input_pkg_name { "/Users/matt/Azureus/unna/games/ScrabbleScorer/ScrabbleScorer.pkg" }; // pkg1: unreferenced objects


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
 Run our application with default or user supplied file names.
 \param[in] argc, argv
 \note This version of main stress tests our package reading and writing
      capabilities. It reads a pkg file, writes it back as ARM32 assembler,
      and converts that back into a package. It then compares both packages
      and alerts us of differences. It also warns us about abnormalities when
      reading the original package.
 \note Optionally, it reads the generated package as well, and compares the
      data structures in memory. This test is less pedantic in that it does not
      verify filler bytes, but alos does not catch all errors that.
 \note Optionally, it creates a Dyna Object Tree from the package data and
      prints it to the terminal. The output has not been verified yet.
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
#if 1
  if (my_pkg.writeAsm("/Users/matt/dev/DyneLang/mines.s") < 0) {
    std::cout << "ERROR writing assembler file." << std::endl;
    return 0;
  }
  if (asmToObj("/Users/matt/dev/DyneLang/mines.s", "/Users/matt/dev/DyneLang/mines.o") < 0) {
    std::cout << "ERROR calling assembler and creating object file." << std::endl;
    return 0;
  }
  if (objToBin("/Users/matt/dev/DyneLang/mines.o", "/Users/matt/dev/DyneLang/mines.pkg") < 0) {
    std::cout << "ERROR extracting binary data from object file." << std::endl;
    return 0;
  }
  if (my_pkg.compareContents("/Users/matt/dev/DyneLang/mines.pkg") < 0) {
    std::cout << "ERROR comparing the original package and the new package contents." << std::endl;
    return 0;
  }
#endif
#if 0
  if (my_pkg.compareFile("/Users/matt/dev/DyneLang/mines.pkg") < 0) {
    std::cout << "ERROR comparing the binaries of the original package and the new package." << std::endl;
    return 0;
  }
#endif
#if 1
  dyn::Ref nos_pkg = my_pkg.toNOS();
//  (void)nos_pkg;
  dyn::Print(nos_pkg);
#endif
  std::cout << "OK." << std::endl;
  return 0;
}

/**
 Read a Dyne Stream file that conatains a function and decompile it.
 \param[in] argc, argv
 \note This version of main stress tests the decompiler. We start with the
      source code of a function, let an external tool compile the function
      into a Ref with bytecode, read the Ref back, and decompile it back
      into source code. This is successful when the original source code and
      the generated code are functionally identical.
 \note In later incarnations, we want to decompiel all available functions
      from known packages, recompile them, and ideally generate the exact same
      bytecode. Functional equality is sufficient though.
 */
int main_002(int argc, const char * argv[])
{
  // Enter some source code here or read a file
  // Call the Newton Framework to generate a Newton Stream File
  // Read the stream file
  // Output the binary representation
  // Decompile the function
  // Output the source code and compare the result to the input
  return 0;
}

// Test me: dyn::lang::decompile(dyn::RefNIL);
