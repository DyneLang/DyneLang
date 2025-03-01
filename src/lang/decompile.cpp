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
#include <iomanip>

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

 12    kTokenAssign
 11    kTokenAnd kTokenOr
 10    kTokenNot
  9    '<' kTokenLEQ '>' kTokenGEQ kTokenEQL kTokenNEQ
  %nonassoc  kTokenExists
  7    '&' kTokenAmperAmper
  6    '+' '-'
  5    '*' '/' kTokenDiv kTokenMod
  4    kTokenLShift kTokenRShift
  3    kTokenUMinus

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

class State {
public:
  int type;
  int arg;
  std::string text;
};

static std::vector<int> label_list { };
static std::vector<AC> altcode { };
static std::vector<State> stack { };

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

enum {
  kSTEndOfSTack, kSTUnknown, kSTError, kSTExpr, kSTStatement, kSTCondition
};

void PrintStack() {
  for (auto &state: stack) {
    switch (state.type) {
      case kSTEndOfSTack: std::cout << "---------: "; break;
      case kSTUnknown:    std::cout << "  unknown: "; break;
      case kSTError:      std::cout << "    ERROR: "; break;
      case kSTExpr:       std::cout << "     expr: "; break;
      case kSTStatement:  std::cout << "statement: "; break;
      case kSTCondition:  std::cout << "condition: "; break;
      default:            std::cout << "  default: "; break;
    }
    std::cout << std::setw(4) << state.arg << ": " << state.text << std::endl;
  }
}

int DoUnknown(Deco &deco) {
  stack.push_back( {
    kSTUnknown, 0,
    "ERROR: Unknown altcode " + std::to_string(deco.ac.cmd) + ", " + std::to_string(deco.ac.val)
    + " at " + std::to_string(deco.ac.ip) } );
  return 0;
}

int DoEOF(Deco &deco) {
  (void)deco;
  return 1;
}

int DoPushConst(Deco &deco) {
  stack.push_back( { kSTExpr, 0, "imm_" + std::to_string(deco.ac.val) } );
  return 0;
}

int DoInfixOperator(Deco &deco, const std::string &op, int precedence) {
  (void)deco;
  State s1 = stack.back(); stack.pop_back();
  if (s1.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": '" << op << "': expected Expression as first argument!\n" << std::endl;
    return -1;
  }
  State s2 = stack.back(); stack.pop_back();
  if (s2.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": '" << op << "': expected Expression as second argument!\n" << std::endl;
    return -1;
  }
  stack.push_back( { kSTExpr, precedence,
    (precedence < s2.arg ? "(" : "") + s2.text
    + (precedence < s2.arg ? ")" : "")
    + " " + op + " "
    + (precedence < s1.arg ? "(" : "") + s1.text
    + (precedence < s1.arg ? ")" : "") } );
  return 0;
}

int DoEquals(Deco &deco) { return DoInfixOperator(deco, "=", 9); }
int DoNotEquals(Deco &deco) { return DoInfixOperator(deco, "<>", 9); }
int DoGreaterOrEqual(Deco &deco) { return DoInfixOperator(deco, ">=", 9); }
int DoGreaterThan(Deco &deco) { return DoInfixOperator(deco, ">", 9); }
int DoLessOrEqual(Deco &deco) { return DoInfixOperator(deco, "<=", 9); }
int DoLessThan(Deco &deco) { return DoInfixOperator(deco, "<", 9); }
int DoAdd(Deco &deco) { return DoInfixOperator(deco, "+", 6); }
int DoSubtract(Deco &deco) { return DoInfixOperator(deco, "-", 6); }
int DoMultiply(Deco &deco) { return DoInfixOperator(deco, "*", 5); }
int DoDivide(Deco &deco) { return DoInfixOperator(deco, "/", 5); }

int DoReturn(Deco &deco) {
  State s = stack.back();
  // Handle the 'return' command at the end of the function.
  if (altcode[deco.ip+1].cmd == kVCEOF) {
    // The compiler added a 'return' even though it was not needed. Don't output anything
    if (s.type != kSTExpr) {
      return 0;
    }
    // The compiler added a 'return NIL'. This is the default if there is no
    // return command, so don't write anything.
    if ((deco.ip > 0) && (altcode[deco.ip-1].cmd == kVCPushConst) && (altcode[deco.ip-1].val == 2)) {
      stack.pop_back();
      return 0;
    }
  } else {
    // We are not at the end of the function, so check if we can generate the 'return'.
    if (s.type != kSTExpr) {
      std::cout << "ERROR: " << deco.ip << ": 'return': expected Expression as first argument!\n" << std::endl;
      return -1;
    }
  }
  stack.pop_back();
  stack.push_back( { kSTStatement, 0, "return " + s.text } );
  return 0;
}

int DoBranchIfFalse(Deco &deco) {
  (void)deco;
  State s = stack.back(); stack.pop_back();
  if (s.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": 'branch_if_false': expected Expression as first argument!\n" << std::endl;
    return -1;
  }
  stack.push_back( { kSTCondition, 0, "if not " + s.text + " begin"} );
  return 0;
}

int DoLabel(Deco &deco) {
  (void)deco;
  std::cout << "---- label " << deco.ac.val << " ----\n";
  PrintStack();
  std::cout << "---- end label " << deco.ac.val << " ----\n";

//  if (altcode[deco.ip-1].cmd == kVCBranch) {
//    This is probably an Else statment
//  }

//  Search up to find all branches that end here

  //  if ... then ...
  //    expr
  //    branch_if_false n
  //    statement(s)
  //    label n

  //  if ... then ... else ...
  //    expr
  //    branch_if_false n
  //    statement(s), expr
  //    branch m
  //  label n
  //    statement(s), expr
  //  label m

  //  while ... do ...
  //    branch n
  //  label m
  //    statement(s)
  //  label n
  //    expr
  //    branch_if_true m

  //  while ... do ... break ...
  //    branch n
  //  label m
  //    statement(s)
  //    branch o
  //    statement(s)
  //  label n
  //    expr
  //    branch_if_true m
  //  label o

  //  repeat ... until ...
  //  label m
  //    statement(s)
  //    expr
  //    branch_if_false n

  //  repeat ... break ... until ...
  //  label m
  //    statement(s)
  //    branch n
  //    expr
  //    branch_if_false n
  //  label n

  //  loop ... break ...
  //  label m
  //    statement(s)
  //    branch n
  //    statement(s)
  //    branch m
  //  label n

  // TODO: for loops, try catch


//  Search down to find all branches that end here



  // TODO: write this!
//  State &s = stack.back();
//  if (s.type != kSTExpr) {
//    std::cout << "ERROR: " << deco.ip << ": 'branch_if_false': expected Expression as first argument!\n" << std::endl;
//    return -1;
//  }
//  s.type = kSTStatement;
  stack.push_back( { kSTUnknown, 0, "end"} );

  return 0;
}

int DoPop(Deco &deco) {
  State &s = stack.back();
  if (s.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": 'pop': expected expression on stack!\n" << std::endl;
    return -1;
  }
  s.type = kSTStatement;
  return 0;
}

int DoArgList(Deco &deco, std::string &args, int num_args) {
  for (int i=0; i<num_args; ++i) {
    State &arg = stack.back();
    if (arg.type != kSTExpr) {
      std::cout << "ERROR: " << deco.ip << ": expected argument " << i << " expr on stack!\n" << std::endl;
      return -1;
    }
    if (i==0)
      args = arg.text;
    else
      args = arg.text + ", " + args;
    stack.pop_back();
  }
  return 0;
}

int DoCallOrInvoke(Deco &deco, const std::string &call, int which) {
  State s = stack.back();
  if (s.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": '" << call << "': expected function name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  std::string args { };
  if (DoArgList(deco, args, deco.ac.val) == -1)
    return -1;
  if (which == 0) // call
    stack.push_back( { kSTExpr, 0, s.text + "(" + args + ")" } );
  else // invoke
    stack.push_back( { kSTExpr, 0, "call " + s.text + " with (" + args + ")" } );
  return 0;
}

int DoCall(Deco &deco) { return DoCallOrInvoke(deco, "call", 0); }
int DoInvoke(Deco &deco) { return DoCallOrInvoke(deco, "invoke", 1); }

int DoSend(Deco &deco, const std::string &op, const std::string &call, bool is_resend) {
  State name = stack.back();
  if (name.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": '" << call << "': expected message name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  State rcvr { };
  if (!is_resend) {
    rcvr = stack.back();
    if (rcvr.type != kSTExpr) {
      std::cout << "ERROR: " << deco.ip << ": '" << call << "': expected receiver on stack!\n" << std::endl;
      return -1;
    }
    stack.pop_back();
  }
  std::string args { };
  if (DoArgList(deco, args, deco.ac.val) == -1)
    return -1;
  if (is_resend)
    stack.push_back( { kSTExpr, 0, "inherited" + op + name.text + "(" + args + ")" } );
  else
    stack.push_back( { kSTExpr, 0, rcvr.text + op + name.text + "(" + args + ")" } );
  return 0;
}

int DoSend(Deco &deco) { return DoSend(deco, ":", "send", false); }
int DoSendIfDefined(Deco &deco) { return DoSend(deco, ":?", "send_if_defined", false); }
int DoResend(Deco &deco) { return DoSend(deco, ":", "resend", true); }
int DoResendIfDefined(Deco &deco) { return DoSend(deco, ":?", "resend_if_defined", true); }

int DoPush(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "lit_" + std::to_string(deco.ac.val)} );
  return 0;
}

int DoFindVar(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "lit_" + std::to_string(deco.ac.val)} );
  return 0;
}

int DoPushSelf(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "self"} );
  return 0;
}

int DoFindAndSetVar(Deco &deco) {
  State value = stack.back();
  if (value.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": 'find_and_set_var': expected expression on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  stack.push_back( { kSTStatement, 0, "lit_" + std::to_string(deco.ac.val) + " := " + value.text} );
  return 0;
}

int DoXXX(Deco &deco) {
  (void)deco;
  return 0;
}

VcHandler handler[] = {
//kVCEOF,             kVCPop,             kVCDup,             kVCReturn,
  DoEOF,              DoPop,              DoUnknown,          DoReturn,
//kVCPushSelf,        kVCSetLexScope,     kVCIterNext,        kVCIterDone,
  DoPushSelf,         DoUnknown,          DoUnknown,          DoUnknown,
//kVCPopHandlers,     kVCPush,            kVCPushConst,       kVCCall,
  DoUnknown,          DoPush,             DoPushConst,        DoCall,
//kVCInvoke,          kVCSend,            kVCSendIfDefined,   kVCResend,
  DoInvoke,           DoSend,             DoSendIfDefined,    DoResend,
//kVCResendIfDefined, kVCBranch,          kVCBranchIfTrue,    kVCBranchIfFalse,
  DoResendIfDefined,  DoUnknown,          DoUnknown,          DoBranchIfFalse,
//kVCFindVar,         kVCGetVar,          kVCMakeFrame,       kVCMakeArray,
  DoFindVar,          DoUnknown,          DoUnknown,          DoUnknown,
//kVCFillArray,       kVCGetPath,         kVCGetPathCheck,    kVCSetPath,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kVCSetPathVal,      kVCSetVar,          kVCFindAndSetVar,   kVCIncrVar,
  DoUnknown,          DoUnknown,          DoFindAndSetVar,    DoUnknown,
//kVCBranchLoop,      kVCAdd,             kVCSubtract,        kVCARef,
  DoUnknown,          DoAdd,              DoSubtract,         DoUnknown,
//kVCSetARef,         kVCEquals,          kVCNot,             kVCNotEquals,
  DoUnknown,          DoEquals,           DoUnknown,          DoNotEquals,
//kVCMultiply,        kVCDivide,          kVCDiv,             kVCLessThan,
  DoMultiply,         DoDivide,           DoUnknown,          DoLessThan,
//kVCGreaterThan,     kVCGreaterOrEqual,  kVCLessOrEqual,     kVCBitAnd,
  DoGreaterThan,      DoGreaterOrEqual,   DoLessOrEqual,      DoUnknown,
//kVCBitOr,           kVCBitNot,          kVCNewIter,         kVCLength,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kVCClone,           kVCSetClass,        kVCAddArraySlot,    kVCStringer,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kVCHasPath,         kVCClassOf,         kVCNewHandler,      kVCLabel
  DoUnknown,          DoUnknown,          DoUnknown,          DoLabel,
};

static void print_altcode(int ip, AC &ac) {
  std::cout << std::setw(4) << ip << std::setw(4) << ac.ip << ": ";
  switch (ac.cmd) {
    case kVCEOF:              std::cout << "    EOF" << std::endl; break;
    case kVCPop:              std::cout << "    pop" << std::endl; break;
    case kVCDup:              std::cout << "    dup" << std::endl; break;
    case kVCReturn:           std::cout << "    return" << std::endl; break;
    case kVCPushSelf:         std::cout << "    push_self" << std::endl; break;
    case kVCSetLexScope:      std::cout << "    set_lex_scope" << std::endl; break;
    case kVCIterNext:         std::cout << "    iter_next" << std::endl; break;
    case kVCIterDone:         std::cout << "    iter_done" << std::endl; break;
    case kVCPopHandlers:      std::cout << "    pop_handlers" << std::endl; break;
    case kVCPush:             std::cout << "    push lit_" << ac.val << std::endl; break;
    case kVCPushConst:        std::cout << "    push_const imm_" << ac.val << std::endl; break;
    case kVCCall:             std::cout << "    call #args_" << ac.val << std::endl; break;
    case kVCInvoke:           std::cout << "    invoke #args_" << ac.val << std::endl; break;
    case kVCSend:             std::cout << "    send #args_" << ac.val << std::endl; break;
    case kVCSendIfDefined:    std::cout << "    send_if_defined #args_" << ac.val << std::endl; break;
    case kVCResend:           std::cout << "    resend #args_" << ac.val << std::endl; break;
    case kVCResendIfDefined:  std::cout << "    resend_if_defined #args_" << ac.val << std::endl; break;
    case kVCBranch:           std::cout << "    branch pc=" << ac.val << std::endl; break;
    case kVCBranchIfTrue:     std::cout << "    branch_if_true pc=" << ac.val << std::endl; break;
    case kVCBranchIfFalse:    std::cout << "    branch_if_false pc=" << ac.val << std::endl; break;
    case kVCFindVar:          std::cout << "    find_var lit_" << ac.val << std::endl; break;
    case kVCGetVar:           std::cout << "    get_var local_" << ac.val << std::endl; break;
    case kVCMakeFrame:        std::cout << "    make_frame #slots_" << ac.val << std::endl; break;
    case kVCMakeArray:        std::cout << "    make_array #slots_" << ac.val << std::endl; break;
    case kVCFillArray:        std::cout << "    fill_array" << std::endl; break;
    case kVCGetPath:          std::cout << "    get_path" << std::endl; break;
    case kVCGetPathCheck:     std::cout << "    get_path_check" << std::endl; break;
    case kVCSetPath:          std::cout << "    set_path" << std::endl; break;
    case kVCSetPathVal:       std::cout << "    set_path_val" << std::endl; break;
    case kVCSetVar:           std::cout << "    set_var local_" << ac.val << std::endl; break;
    case kVCFindAndSetVar:    std::cout << "    find_and_set_var lit_" << ac.val << std::endl; break;
    case kVCIncrVar:          std::cout << "    incr_var loc_" << ac.val << std::endl; break;
    case kVCBranchLoop:       std::cout << "    branch_loop pc=" << ac.val << std::endl; break;
    case kVCAdd:              std::cout << "    add" << std::endl; break;
    case kVCSubtract:         std::cout << "    subtract" << std::endl; break;
    case kVCARef:             std::cout << "    aref" << std::endl; break;
    case kVCSetARef:          std::cout << "    set_aref" << std::endl; break;
    case kVCEquals:           std::cout << "    equals" << std::endl; break;
    case kVCNot:              std::cout << "    not" << std::endl; break;
    case kVCNotEquals:        std::cout << "    not_equals" << std::endl; break;
    case kVCMultiply:         std::cout << "    multiply" << std::endl; break;
    case kVCDivide:           std::cout << "    divide" << std::endl; break;
    case kVCDiv:              std::cout << "    div" << std::endl; break;
    case kVCLessThan:         std::cout << "    less_than" << std::endl; break;
    case kVCGreaterThan:      std::cout << "    greater_than" << std::endl; break;
    case kVCGreaterOrEqual:   std::cout << "    greater_or_equal" << std::endl; break;
    case kVCLessOrEqual:      std::cout << "    less_or_equal" << std::endl; break;
    case kVCBitAnd:           std::cout << "    bit_and" << std::endl; break;
    case kVCBitOr:            std::cout << "    bit_or" << std::endl; break;
    case kVCBitNot:           std::cout << "    bit_not" << std::endl; break;
    case kVCNewIter:          std::cout << "    new_iter" << std::endl; break;
    case kVCLength:           std::cout << "    length" << std::endl; break;
    case kVCClone:            std::cout << "    clone" << std::endl; break;
    case kVCSetClass:         std::cout << "    set_class" << std::endl; break;
    case kVCAddArraySlot:     std::cout << "    add_array_slot" << std::endl; break;
    case kVCStringer:         std::cout << "    stringer" << std::endl; break;
    case kVCHasPath:          std::cout << "    has_path" << std::endl; break;
    case kVCClassOf:          std::cout << "    class_of" << std::endl; break;
    case kVCNewHandler:       std::cout << "    new_handler #exc_" << ac.val << std::endl; break;
    case kVCLabel:            std::cout << "label" << ac.val << ":" << std::endl; break;
    default:
      std::cout << "ERROR: unknown altcode: a=" << ac.cmd << ", b=" << ac.val << "." << std::endl; break;
  }
}

static void print_expanded() {
  int i = 0;
  for (auto &ac: altcode) {
    print_altcode(i++, ac);
  }
}

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
    auto ip = i;
    if (std::binary_search(label_list.begin(), label_list.end(), i)) {
      altcode.push_back({kVCLabel, (int)ip, i});
    }
    uint8_t cmd = inst[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) {
      b = inst[i+1]<<8 | inst[i+2]; i += 2;
    }
    switch (a) {
      case 0:
        switch (b) {
          case 0: altcode.push_back({kVCPop, 0, ip}); break;
          case 1: altcode.push_back({kVCDup, 0, ip}); break;
          case 2: altcode.push_back({kVCReturn, 0, ip}); break;
          case 3: altcode.push_back({kVCPushSelf, 0, ip}); break;
          case 4: altcode.push_back({kVCSetLexScope, 0, ip}); break;
          case 5: altcode.push_back({kVCIterNext, 0, ip}); break;
          case 6: altcode.push_back({kVCIterDone, 0, ip}); break;
          case 7: altcode.push_back({kVCPopHandlers, 0, ip}); break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 3: altcode.push_back({kVCPush, (int16_t)b, ip}); break;
      case 4: altcode.push_back({kVCPushConst, (int16_t)b, ip}); break;
      case 5: altcode.push_back({kVCCall, (int16_t)b, ip}); break;
      case 6: altcode.push_back({kVCInvoke, (int16_t)b, ip}); break;
      case 7: altcode.push_back({kVCSend, (int16_t)b, ip}); break;
      case 8: altcode.push_back({kVCSendIfDefined, (int16_t)b, ip}); break;
      case 9: altcode.push_back({kVCResend, (int16_t)b, ip}); break;
      case 10: altcode.push_back({kVCResendIfDefined, (int16_t)b, ip}); break;
      case 11: altcode.push_back({kVCBranch, (int16_t)b, ip}); break;
      case 12: altcode.push_back({kVCBranchIfTrue, (int16_t)b, ip}); break;
      case 13: altcode.push_back({kVCBranchIfFalse, (int16_t)b, ip}); break;
      case 14: altcode.push_back({kVCFindVar, (int16_t)b, ip}); break;
      case 15: altcode.push_back({kVCGetVar, (int16_t)b, ip}); break;
      case 16: altcode.push_back({kVCMakeFrame, (int16_t)b, ip}); break;
      case 17: altcode.push_back({kVCMakeArray, (int16_t)b, ip}); break;
      case 18: altcode.push_back({kVCGetPath, (int16_t)b, ip}); break;
      case 19: altcode.push_back({kVCSetPath, (int16_t)b, ip}); break;
      case 20: altcode.push_back({kVCSetVar, (int16_t)b, ip}); break;
      case 21: altcode.push_back({kVCFindAndSetVar, (int16_t)b, ip}); break;
      case 22: altcode.push_back({kVCIncrVar, (int16_t)b, ip}); break;
      case 23: altcode.push_back({kVCBranchLoop, (int16_t)b, ip}); break;
      case 24:
        switch (b) {
          case 0: altcode.push_back({kVCAdd, 0, ip}); break;
          case 1: altcode.push_back({kVCSubtract, 0, ip}); break;
          case 2: altcode.push_back({kVCARef, 0, ip}); break;
          case 3: altcode.push_back({kVCSetARef, 0, ip}); break;
          case 4: altcode.push_back({kVCEquals, 0, ip}); break;
          case 5: altcode.push_back({kVCNot, 0, ip}); break;
          case 6: altcode.push_back({kVCNotEquals, 0, ip}); break;
          case 7: altcode.push_back({kVCMultiply, 0, ip}); break;
          case 8: altcode.push_back({kVCDivide, 0, ip}); break;
          case 9: altcode.push_back({kVCDiv, 0, ip}); break;
          case 10: altcode.push_back({kVCLessThan, 0, ip}); break;
          case 11: altcode.push_back({kVCGreaterThan, 0, ip}); break;
          case 12: altcode.push_back({kVCGreaterOrEqual, 0, ip}); break;
          case 13: altcode.push_back({kVCLessOrEqual, 0, ip}); break;
          case 14: altcode.push_back({kVCBitAnd, 0, ip}); break;
          case 15: altcode.push_back({kVCBitOr, 0, ip}); break;
          case 16: altcode.push_back({kVCBitNot, 0, ip}); break;
          case 17: altcode.push_back({kVCNewIter, 0, ip}); break;
          case 18: altcode.push_back({kVCLength, 0, ip}); break;
          case 19: altcode.push_back({kVCClone, 0, ip}); break;
          case 20: altcode.push_back({kVCSetClass, 0, ip}); break;
          case 21: altcode.push_back({kVCAddArraySlot, 0, ip}); break;
          case 22: altcode.push_back({kVCStringer, 0, ip}); break;
          case 23: altcode.push_back({kVCHasPath, 0, ip}); break;
          case 24: altcode.push_back({kVCClassOf, 0, ip}); break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 25: altcode.push_back({kVCNewHandler, (int16_t)b, ip}); break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
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
  printf("--- expanded byte code\n");
  print_expanded();
  stack.push_back( { kSTEndOfSTack, 0, "... stack bottom ..." } );
  printf("--- decode\n");
  decode();
  printf("--- remaining stack:\n");
  PrintStack();

//  std::cout << stack.back() << std::endl; stack.pop_back();

  return RefNIL;
}
