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

/*
 NewtonScript has 58 bytecode commands:

 000 pop                        x --
 001 dup                        x -- x x
 002 return                     -- (returns the value in stack[sp] to the caller)
 003 push-self                  -- RCVR
 004 set-lex-scope              func -- closure
 005 iter-next                  iterator --
 006 iter-done                  iterator -- done?
 007 000 001 pop-handlers       --
 03x push                       -- literal
 04x (B signed) push-constant   -- value
 05x call                       arg1 arg2 ... argN name -- result
 06x invoke                     arg1 arg2 ... argN func -- result
 07x send                       arg1 arg2 ... argN name receiver -- result
 10x send-if-defined            arg1 arg2 ... argN name receiver -- result
 11x resend                     arg1 arg2 ... argN name -- result
 12x resend-if-defined          arg1 arg2 ... argN name -- result
 13x branch                     --
 14x branch-if-true             value --
 15x branch-if-false            value --
 16x find-var                   -- value
 17x get-var                    -- value
 20x make-frame                 val1 val2 ... valN map -- frame
 21x make-array b=0xffff        size class -- array
 21x fill_array                 val1 val2 ... valN class -- array
 220 get-path                   object pathExpr -- value
 221 get-path-check             object pathExpr -- value
 230 set-path                   object pathExpr value --
 231 set-path-ret               object pathExpr value -- value
 24x set-var                    value --
 25x find-and-set-var           value --
 26x incr-var                   addend -- addend value
 27x branch-if-loop-not-done    incr index limit --
 30:0 add |+|                   num1 num2 -- result  (30x freq-func:)
 30:1 subtract |-|              num1 num2 -- result
 30:2 aref                      object index -- element
 30:3 set-aref                  object index element -- element
 30:4 equals |=|                obj1 obj2 -- result
 30:5 not |not|                 value -- result
 30:6 not-equals |<>|           obj1 obj2 -- result
 30:7 multiply |*|              num1 num2 -- result
 30:8 divide |/|                num1 num2 -- result
 30:9 div |div|                 int1 int2 -- result
 30:10 less-than |<|            obj1 obj2 -- result
 30:11 greater-than |>|         obj1 obj2 -- result
 30:12 greater-or-equal |>=|    obj1 obj2 -- result
 30:13 less-or-equal |<=|       obj1 obj2 -- result
 30:14 bit-and BAnd             int1 int2 -- result
 30:15 bit-or BOr               int1 int2 -- result
 30:16 bit-not BNot             int -- result
 30:17 new-iterator             object deeply -- iterator
 30:18 length Length            object -- length
 30:19 clone Clone              object -- clone
 30:20 set-class SetClass       object class -- object
 30:21 add-array-slot           array object -- object
 30:22 stringer Stringer        array -- string
 30:23 has-path none            object pathExpr -- result
 30:24 class-of ClassOf         object -- class
 31x new-handlers               sym1 pc1 sym2 pc2 ... symN pcN --
*/

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
          case 0: bc.bc = BC::Pop; break;
          case 1: bc.bc = BC::Dup; break;
          case 2: bc.bc = BC::Return; break;
          case 3: bc.bc = BC::PushSelf; break;
          case 4: bc.bc = BC::SetLexScope; break;
          case 5: bc.bc = BC::IterNext; break;
          case 6: bc.bc = BC::IterDone; break;
          case 7: bc.bc = BC::PopHandlers; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 3: bc.bc = BC::Push; bc.arg = (int16_t)b; break;
      case 4: bc.bc = BC::PushConst; bc.arg = (int16_t)b; break;
      case 5: bc.bc = BC::Call; bc.arg = (int16_t)b; break;
      case 6: bc.bc = BC::Invoke; bc.arg = (int16_t)b; break;
      case 7: bc.bc = BC::Send; bc.arg = (int16_t)b; break;
      case 8: bc.bc = BC::SendIfDefined; bc.arg = (int16_t)b; break;
      case 9: bc.bc = BC::Resend; bc.arg = (int16_t)b; break;
      case 10: bc.bc = BC::ResendIfDefined; bc.arg = (int16_t)b; break;
      case 11: bc.bc = BC::Branch; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 12: bc.bc = BC::BranchIfTrue; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 13: bc.bc = BC::BranchIfFalse; bc.arg = (int)pc_map[b]; func[bc.arg].references++; break;
      case 14: bc.bc = BC::FindVar; bc.arg = (int16_t)b; break;
      case 15: bc.bc = BC::GetVar; bc.arg = (int16_t)b; break;
      case 16: bc.bc = BC::MakeFrame; bc.arg = (int16_t)b; break;
      case 17: bc.bc = BC::MakeArray; bc.arg = (int16_t)b; break;
      case 18: bc.bc = BC::GetPath; bc.arg = (int16_t)b; break;
      case 19: bc.bc = BC::SetPath; bc.arg = (int16_t)b; break;
      case 20: bc.bc = BC::SetVar; bc.arg = (int16_t)b; break;
      case 21: bc.bc = BC::FindAndSetVar; bc.arg = (int16_t)b; break;
      case 22: bc.bc = BC::IncrVar; bc.arg = (int16_t)b; break;
      case 23: bc.bc = BC::BranchLoop; bc.arg = b; break;
        // TODO: BranchLoop pushes jump destinations onto the stack
      case 24:
        switch (b) {
          case 0: bc.bc = BC::Add; break;
          case 1: bc.bc = BC::Subtract; break;
          case 2: bc.bc = BC::ARef; break;
          case 3: bc.bc = BC::SetARef; break;
          case 4: bc.bc = BC::Equals; break;
          case 5: bc.bc = BC::Not; break;
          case 6: bc.bc = BC::NotEquals; break;
          case 7: bc.bc = BC::Multiply; break;
          case 8: bc.bc = BC::Divide; break;
          case 9: bc.bc = BC::Div; break;
          case 10: bc.bc = BC::LessThan; break;
          case 11: bc.bc = BC::GreaterThan; break;
          case 12: bc.bc = BC::GreaterOrEqual; break;
          case 13: bc.bc = BC::LessOrEqual; break;
          case 14: bc.bc = BC::BitAnd; break;
          case 15: bc.bc = BC::BitOr; break;
          case 16: bc.bc = BC::BitNot; break;
          case 17: bc.bc = BC::NewIter; break;
          case 18: bc.bc = BC::Length; break;
          case 19: bc.bc = BC::Clone; break;
          case 20: bc.bc = BC::SetClass; break;
          case 21: bc.bc = BC::AddArraySlot; break;
          case 22: bc.bc = BC::Stringer; break;
          case 23: bc.bc = BC::HasPath; break;
          case 24: bc.bc = BC::ClassOf; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 25: bc.bc = BC::NewHandler; bc.arg = (int16_t)b; break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
        break;
    }
  }
  return func;
}

