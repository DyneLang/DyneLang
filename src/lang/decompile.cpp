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

#include <dyn/lang/decompile.h>
#include <dyn/objects.h>

#include <stdio.h>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace dyn;

/*
 We have 58 bytecode commands:

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

typedef struct {
  int cmd; int val; size_t ip;
} AC;

AC ac_empty { 0, 0, 0 };
class Deco {
public:
  Index ip { 0 };
  AC &ac { ac_empty };
};

typedef int (*VcHandler)(Deco&);

static std::vector<int> label_list { };
static std::vector<AC> altcode { };
static std::vector<std::string> stack;
static std::string src { };

enum {
  kVCEOF,             kVCPop,             kVCDup,             kVCReturn,
  kVCPushSelf,        kVCSetLexScope,     kVCIterNext,        kVCIterDone,
  kVCPopHandlers,     kVCPush,            kVCPushConst,       kVCCall,
  kVCInvoke,          kVCSend,            kVCSendIfDefined,   kVCResend,
  kVCResendIfDefined, kVCBranch,          kVCBranchIfTrue,    kVCBranchIfFalse,
  kVCFindVar,         kVCGetVar,          kVCMakeFrame,       kVCMakeArray,
  kVCFillArray,       kVCGetPath,         kVCGetPathCheck,    kVCSetPath,
  kVCSetPathVal,      kVCSetVar,          kVCFindAndSetVar,   kVCIncrVar,
  kVCBranchLoop,      kVCAdd,             kVCSubtract,        kVCARef,
  kVCSetARef,         kVCEquals,          kVCNot,             kVCNotEquals,
  kVCMultiply,        kVCDivide,          kVCDiv,             kVCLessThan,
  kVCGreaterThan,     kVCGreaterOrEqual,  kVCLessOrEqual,     kVCBitAnd,
  kVCBitOr,           kVCBitNot,          kVCNewIter,         kVCLength,
  kVCClone,           kVCSetClass,        kVCAddArraySlot,    kVCStringer,
  kVCHasPath,         kVCClassOf,         kVCNewHandler,      kVCLabel
};


int DoUnknown(Deco &deco) {
  stack.push_back("ERROR<" + std::to_string(deco.ac.cmd) + ", "
                  + std::to_string(deco.ac.val) + ", "
                  + std::to_string(deco.ac.ip) + ">");
  return 0;
}

int DoEOF(Deco &deco) {
  (void)deco;
  return 1;
}

int DoPushConst(Deco &deco) {
  stack.push_back("ref" + std::to_string(deco.ac.val));
  return 0;
}

int DoAdd(Deco &deco) {
  (void)deco;
  std::string e1 = stack.back(); stack.pop_back();
  std::string e2 = stack.back(); stack.pop_back();
  stack.push_back("(" + e2 + "+" + e1 + ")");
  return 0;
}

int DoReturn(Deco &deco) {
  (void)deco;
  std::string e1 = stack.back(); stack.pop_back();
  stack.push_back("return " + e1);
  return 0;
}

int DoEquals(Deco &deco) {
  (void)deco;
  std::string e1 = stack.back(); stack.pop_back();
  std::string e2 = stack.back(); stack.pop_back();
  stack.push_back("(" + e2 + "=" + e1 + ")");
  return 0;
}

int DoBranchIfFalse(Deco &deco) {
  (void)deco;
  std::string e1 = stack.back(); stack.pop_back();
  stack.push_back("if not " + e1);
  return 0;
}

int DoXXX(Deco &deco) {
  (void)deco;
  return 0;
}

VcHandler handler[] = {
  &DoEOF,              DoUnknown,          DoUnknown,          DoReturn,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoPushConst,        DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoBranchIfFalse,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoAdd,              DoUnknown,          DoUnknown,
  DoUnknown,          DoEquals,           DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
};


static int expand(dyn::RefArg func)
{
  dyn::Ref inst_ref = dyn::GetFrameSlot(func, dyn::Sym("instructions"));
  if (!inst_ref.IsBinary()) {
    std::cout << "ERROR: Expand: No 'instructions array in function." << std::endl;
    return -1;
  }
  dyn::BinaryObject *inst_obj = static_cast<dyn::BinaryObject*>(inst_ref.GetObject());
  size_t n_inst = inst_obj->size();
  uint8_t *inst = static_cast<uint8_t*>(inst_obj->Data());

  // Find all jump targets so we can generate pseudo labels.
  for (size_t i=0; i<n_inst; i++) {
    uint8_t cmd = inst[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) {
      b = inst[i+1]<<8 | inst[i+2]; i += 2;
    }
    if (   (a==11) // branch
        || (a==12) // brach-if-true
        || (a==13) // brach-if-false
        || (a==23) // branch-if-loop-not-done
        // TODO: new-handlers (A = 25) sym1 pc1 sym2 pc2 ... symN pcN --
        // The B field contains the number of exception
        // names matched by the handler context
        ) {
      label_list.push_back(b);
    }
  }

  // Generate the Bison byte code.
  for (size_t i=0; i<n_inst; i++) {
    if (std::binary_search(label_list.begin(), label_list.end(), i))
      altcode.push_back({kVCLabel, (int)i, i});
    uint8_t cmd = inst[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) {
      b = inst[i+1]<<8 | inst[i+2]; i += 2;
    }
    switch (a) {
      case 0:
        switch (b) {
          case 0: altcode.push_back({kVCPop, 0, i}); break;
          case 1: altcode.push_back({kVCDup, 0, i}); break;
          case 2: altcode.push_back({kVCReturn, 0, i}); break;
          case 3: altcode.push_back({kVCPushSelf, 0, i}); break;
          case 4: altcode.push_back({kVCSetLexScope, 0, i}); break;
          case 5: altcode.push_back({kVCIterNext, 0, i}); break;
          case 6: altcode.push_back({kVCIterDone, 0, i}); break;
          case 7: altcode.push_back({kVCPopHandlers, 0, i}); break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)i << "." << std::endl;
            break;
        }
        break;
      case 3: altcode.push_back({kVCPush, (int16_t)b, i}); break;
      case 4: altcode.push_back({kVCPushConst, (int16_t)b, i}); break;
        //      case 5: f << "call " << b; break;
        //      case 6: f << "invoke " << b; break;
        //      case 7: f << "send " << b; break;
        //      case 8: f << "send_if_defined " << b; break;
        //      case 9: f << "resend " << b; break;
        //      case 10: f << "resend_if_defined " << b; break;
        //      case 11: f << "branch " << (int16_t)b; break;
        //      case 12: f << "branch_if_true " << (int16_t)b; break;
      case 13: altcode.push_back({kVCBranchIfFalse, (int16_t)b, i}); break;
        //      case 14: f << "find_var " << b; break;
        //      case 15: f << "get_var " << b; break;
        //      case 16: f << "make_frame " << b; break;
        //      case 17: f << "make_array " << b; break;
        //      case 18: f << "get_path " << b; break;
        //      case 19: f << "set_path " << b; break;
        //      case 20: f << "set_var " << b; break;
        //      case 21: f << "find_and_set_var " << b; break;
        //      case 22: f << "incr_var " << b; break;
        //      case 23: f << "branch_if_loop_not_done " << b; break;
      case 24:
        switch (b) {
          case 0: altcode.push_back({kVCAdd, 0, i}); break;
            //          case 1: f << "subtract"; break;
            //          case 2: f << "aref"; break;
            //          case 3: f << "set_aref"; break;
          case 4: altcode.push_back({kVCEquals, 0, i}); break;
            //          case 5: f << "not"; break;
            //          case 6: f << "not_equals"; break;
            //          case 7: f << "multiply"; break;
            //          case 8: f << "divide"; break;
            //          case 9: f << "div"; break;
            //          case 10: f << "less_than"; break;
            //          case 11: f << "greater_than"; break;
            //          case 12: f << "greater_or_equal"; break;
            //          case 13: f << "less_or_equal"; break;
            //          case 14: f << "bit_and"; break;
            //          case 15: f << "bit_or"; break;
            //          case 16: f << "bit_not"; break;
            //          case 17: f << "new_iterator"; break;
            //          case 18: f << "length"; break;
            //          case 19: f << "clone"; break;
            //          case 20: f << "set_class"; break;
            //          case 21: f << "add_array_slot"; break;
            //          case 22: f << "stringer"; break;
            //          case 23: f << "has_path"; break;
            //          case 24: f << "class_of"; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)i << "." << std::endl;
            break;
        }
        break;
      case 25: altcode.push_back({kVCNewHandler, 0, i}); break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)i << "." << std::endl;
        break;
    }
  }
  altcode.push_back({kVCEOF, 0, n_inst});
  return 0;
}

bool decode() {
  Deco deco;
  for (size_t i=0; i<altcode.size();++i) {
    deco.ip = i;
    deco.ac = altcode[i];
    handler[deco.ac.cmd](deco);
  }
  return true;
}


Ref dyn::lang::decompile(RefArg func)
{
  if (expand(func)!=0)
    return RefNIL;
  stack.push_back("<<stack underflow>>");
  decode();
  printf("--- remaining stack:\n");
  while (stack.size()) {
    std::cout << stack.back() << std::endl;
    stack.pop_back();
  }

//  std::cout << stack.back() << std::endl; stack.pop_back();

  return RefNIL;
}
