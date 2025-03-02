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


#include "transcode.h"
#include <dyn/lang/decompile.h>
#include <dyn/objects.h>
#include <map>
#include <iostream>

using namespace dyn;

using namespace dyn::lang;

/**
 Transcode instructions from NewtonScript bytecode to an easier to decompile list.

 Newton bytecode is a series of a/b pairs. a is the command, and depending on
 a, b can either be a more specific command, or an argument of any type. Pairs
 are three bytes, but may be compressed into a single byte if b is less than 7.

 \param[in] ns_func a function that uses NewtonScript bytecode
 \return an array of verbose Dyne bytecode
 */
std::vector<Bytecode> dyn::lang::transcode_from_ns(dyn::RefArg ns_func)
{
  (void)ns_func;
  std::vector<Bytecode> func;
  std::map<size_t, PC> pc_map { };  // map Newton instructing indices to fn

  // Check if we have a binary object 'instructions in the function.
  if (!ns_func.IsFrame()) {
    std::cout << "ERROR: Expand: Not a function." << std::endl;
    return func;
  }
  dyn::Ref inst_ref = dyn::GetFrameSlot(ns_func, dyn::Sym("instructions"));
  if (!inst_ref.IsBinary()) {
    std::cout << "ERROR: Expand: No 'instructions array in function." << std::endl;
    return func;
  }

  // Set up access to the 'instructions binary data
  dyn::BinaryObject *inst_obj = static_cast<dyn::BinaryObject*>(inst_ref.GetObject());
  size_t n_inst = inst_obj->size();
  uint8_t *inst = static_cast<uint8_t*>(inst_obj->Data());

  // Map all bytecode positions to an index in the fn vector
  for (size_t i=0; i<n_inst; i++) {
    PC pc = func.size();
    pc_map[i] = pc;
    uint8_t b = (inst[i] & 0x07);
    if (b == 7) i += 2;
    func.push_back( { BC::Unknown, 0, pc, 0 } );
  }
  func.push_back( { BC::EndOfFile, 0, func.size(), 0 } );

  // Generate an uncompressed byte code with some extras.
  // TODO: remove (int16_t) if argument is unsigned!
  // TODO: find everything that generates code reference
  size_t ip = 0;
  for (PC pc=0; pc<func.size()-1; ++pc) {
    uint8_t cmd = inst[ip++];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) { b = inst[ip]<<8 | inst[ip+1]; ip += 2; }
    Bytecode &bc = func[pc];
    switch (a) {
      case 0:
        switch (b) {
          case 0: bc.bytecode = BC::Pop; break;
          case 1: bc.bytecode = BC::Dup; break;
          case 2: bc.bytecode = BC::Return; break;
          case 3: bc.bytecode = BC::PushSelf; break;
          case 4: bc.bytecode = BC::SetLexScope; break;
          case 5: bc.bytecode = BC::IterNext; break;
          case 6: bc.bytecode = BC::IterDone; break;
          case 7: bc.bytecode = BC::PopHandlers; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 3: bc.bytecode = BC::Push; bc.arg = (int16_t)b; break;
      case 4: bc.bytecode = BC::PushConst; bc.arg = (int16_t)b; break;
      case 5: bc.bytecode = BC::Call; bc.arg = (int16_t)b; break;
      case 6: bc.bytecode = BC::Invoke; bc.arg = (int16_t)b; break;
      case 7: bc.bytecode = BC::Send; bc.arg = (int16_t)b; break;
      case 8: bc.bytecode = BC::SendIfDefined; bc.arg = (int16_t)b; break;
      case 9: bc.bytecode = BC::Resend; bc.arg = (int16_t)b; break;
      case 10: bc.bytecode = BC::ResendIfDefined; bc.arg = (int16_t)b; break;
      case 11: bc.bytecode = BC::Branch; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 12: bc.bytecode = BC::BranchIfTrue; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 13: bc.bytecode = BC::BranchIfFalse; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 14: bc.bytecode = BC::FindVar; bc.arg = (int16_t)b; break;
      case 15: bc.bytecode = BC::GetVar; bc.arg = (int16_t)b; break;
      case 16: bc.bytecode = BC::MakeFrame; bc.arg = (int16_t)b; break;
      case 17: bc.bytecode = BC::MakeArray; bc.arg = (int16_t)b; break;
      case 18: bc.bytecode = BC::GetPath; bc.arg = (int16_t)b; break;
      case 19: bc.bytecode = BC::SetPath; bc.arg = (int16_t)b; break;
      case 20: bc.bytecode = BC::SetVar; bc.arg = (int16_t)b; break;
      case 21: bc.bytecode = BC::FindAndSetVar; bc.arg = (int16_t)b; break;
      case 22: bc.bytecode = BC::IncrVar; bc.arg = (int16_t)b; break;
      case 23: bc.bytecode = BC::BranchLoop; bc.arg = b; break;
        // TODO: BranchLoop pushes jump destinations onto the stack
      case 24:
        switch (b) {
          case 0: bc.bytecode = BC::Add; break;
          case 1: bc.bytecode = BC::Subtract; break;
          case 2: bc.bytecode = BC::ARef; break;
          case 3: bc.bytecode = BC::SetARef; break;
          case 4: bc.bytecode = BC::Equals; break;
          case 5: bc.bytecode = BC::Not; break;
          case 6: bc.bytecode = BC::NotEquals; break;
          case 7: bc.bytecode = BC::Multiply; break;
          case 8: bc.bytecode = BC::Divide; break;
          case 9: bc.bytecode = BC::Div; break;
          case 10: bc.bytecode = BC::LessThan; break;
          case 11: bc.bytecode = BC::GreaterThan; break;
          case 12: bc.bytecode = BC::GreaterOrEqual; break;
          case 13: bc.bytecode = BC::LessOrEqual; break;
          case 14: bc.bytecode = BC::BitAnd; break;
          case 15: bc.bytecode = BC::BitOr; break;
          case 16: bc.bytecode = BC::BitNot; break;
          case 17: bc.bytecode = BC::NewIter; break;
          case 18: bc.bytecode = BC::Length; break;
          case 19: bc.bytecode = BC::Clone; break;
          case 20: bc.bytecode = BC::SetClass; break;
          case 21: bc.bytecode = BC::AddArraySlot; break;
          case 22: bc.bytecode = BC::Stringer; break;
          case 23: bc.bytecode = BC::HasPath; break;
          case 24: bc.bytecode = BC::ClassOf; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 25: bc.bytecode = BC::NewHandler; bc.arg = (int16_t)b; break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
        break;
    }
  }
  return func;
}

