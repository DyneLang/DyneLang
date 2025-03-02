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
#include <map>
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
       [] array element
       : :? message send, conditional sen
       . slot access
 */

// TODO: we have bytecode, altcode, and VC for virtual code. Please agree on naming!
// TODO: we *must* use different names and/or types for bytecode vs. altcode and bc/ac instruction indices
// TODO: have a function that converts bytecode index into altcode index
// TODO: find literals that will build a 'for' loop, so we can preprocess that
// TODO: 'foreach': new_iter, iter_next

using BCIP = size_t;
using ACIP = size_t;

enum {
  kACEOF,             kACPop,             kACDup,             kACReturn,
  kACPushSelf,        kACSetLexScope,     kACIterNext,        kACIterDone,
  kACPopHandlers,     kACPush,            kACPushConst,       kACCall,
  kACInvoke,          kACSend,            kACSendIfDefined,   kACResend,
  kACResendIfDefined, kACBranch,          kACBranchIfTrue,    kACBranchIfFalse,
  kACFindVar,         kACGetVar,          kACMakeFrame,       kACMakeArray,
  kACFillArray,       kACGetPath,         kACGetPathCheck,    kACSetPath,
  kACSetPathVal,      kACSetVar,          kACFindAndSetVar,   kACIncrVar,
  kACBranchLoop,      kACAdd,             kACSubtract,        kACARef,
  kACSetARef,         kACEquals,          kACNot,             kACNotEquals,
  kACMultiply,        kACDivide,          kACDiv,             kACLessThan,
  kACGreaterThan,     kACGreaterOrEqual,  kACLessOrEqual,     kACBitAnd,
  kACBitOr,           kACBitNot,          kACNewIter,         kACLength,
  kACClone,           kACSetClass,        kACAddArraySlot,    kACStringer,
  kACHasPath,         kACClassOf,         kACNewHandler,      kACLabel
};

typedef struct {
  int cmd; int val; size_t ip;
} AltCode;

AltCode ac_empty { 0, 0, 0 };
// TODO: rename this, it's the current state of the Decompiler
// TODO: add the gloabl variables to it and make sure the destructor works
class Deco {
public:
  Index ip { 0 };
  AltCode &ac { ac_empty };
};

typedef int (*VcHandler)(Deco&);

enum {
  kSTEndOfSTack, kSTUnknown, kSTError, kSTExpr, kSTStatement, kSTCondition,
  kSTBranchFwd, kSTBranchBack, kSTBranchTrueFwd, kSTBranchTrueBack,
  kSTBranchFalseFwd, kSTBranchFalseBack
};

// TODO: rename this, it's an item on the stack (Node?)
class State {
public:
  int type;
  int arg;
  std::string text;
  // add a string vector option here if we have a bunch of statements that need indenting later
  // add the original AltCode here?
};

static std::map<int, int> label_map { };
static std::vector<AltCode> altcode { };
static std::vector<State> stack { };


void PrintStack() {
  for (auto &state: stack) {
    switch (state.type) {
      case kSTEndOfSTack:       std::cout << "------------: "; break;
      case kSTUnknown:          std::cout << "     unknown: "; break;
      case kSTError:            std::cout << "       ERROR: "; break;
      case kSTExpr:             std::cout << "        expr: "; break;
      case kSTStatement:        std::cout << "   statement: "; break;
      case kSTCondition:        std::cout << "   condition: "; break;
      case kSTBranchFwd:        std::cout << "       b_fwd: "; break;
      case kSTBranchBack:       std::cout << "      b_back: "; break;
      case kSTBranchTrueFwd:    std::cout << "  b_true_fwd: "; break;
      case kSTBranchTrueBack:   std::cout << " b_true_back: "; break;
      case kSTBranchFalseFwd:   std::cout << " b_false_fwd: "; break;
      case kSTBranchFalseBack:  std::cout << "b_false_back: "; break;
      default:                  std::cout << "         ???: "; break;
    }
    std::cout << std::setw(4) << state.arg << ": " << state.text << std::endl;
  }
}

int DoUnknown(Deco &deco) {
  stack.push_back( {
    kSTUnknown, 0,
    "ERROR: Unknown altcode " + std::to_string(deco.ac.cmd) + ", " + std::to_string(deco.ac.val)
    + " at " + std::to_string(deco.ac.ip) } );
  return 1;
}

int DoEOF(Deco &deco) {
  (void)deco;
  return -1;
}

int DoPushConst(Deco &deco) {
  stack.push_back( { kSTExpr, 0, "imm_" + std::to_string(deco.ac.val) } );
  return 1;
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
  return 1;
}

// equality and relational operators
int DoEquals(Deco &deco) { return DoInfixOperator(deco, "=", 9); }
int DoNotEquals(Deco &deco) { return DoInfixOperator(deco, "<>", 9); }
int DoGreaterOrEqual(Deco &deco) { return DoInfixOperator(deco, ">=", 9); }
int DoGreaterThan(Deco &deco) { return DoInfixOperator(deco, ">", 9); }
int DoLessOrEqual(Deco &deco) { return DoInfixOperator(deco, "<=", 9); }
int DoLessThan(Deco &deco) { return DoInfixOperator(deco, "<", 9); }
// arithmetic operators
int DoAdd(Deco &deco) { return DoInfixOperator(deco, "+", 6); }
int DoSubtract(Deco &deco) { return DoInfixOperator(deco, "-", 6); }
int DoMultiply(Deco &deco) { return DoInfixOperator(deco, "*", 5); }
int DoDivide(Deco &deco) { return DoInfixOperator(deco, "/", 5); }
// TODO: div mod << >>
// boolean operators
// TODO: and or
// unary operators
// TODO: `-` not
// string operators
// TODO: & &&
// `exists` operator


int DoReturn(Deco &deco) {
  State s = stack.back();
  // Handle the 'return' command at the end of the function.
  if (altcode[deco.ip+1].cmd == kACEOF) {
    // The compiler added a 'return' even though it was not needed. Don't output anything
    if (s.type != kSTExpr) {
      return 1;
    }
    // The compiler added a 'return NIL'. This is the default if there is no
    // return command, so don't write anything.
    if ((deco.ip > 0) && (altcode[deco.ip-1].cmd == kACPushConst) && (altcode[deco.ip-1].val == 2)) {
      stack.pop_back();
      return 1;
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
  return 1;
}

int DoBranch(Deco &deco) {
  (void)deco;
  if (deco.ac.val > deco.ip)
    stack.push_back( { kSTBranchFwd, deco.ac.val, "kSTBranchFwd"} );
  else
    stack.push_back( { kSTBranchBack, deco.ac.val, "kSTBranchBack"} );
  return 1;
}

int DoBranchIfTrue(Deco &deco) {
  (void)deco;
  if (deco.ac.val > deco.ip)
    stack.push_back( { kSTBranchTrueFwd, deco.ac.val, "kSTBranchTrueFwd"} );
  else
    stack.push_back( { kSTBranchTrueBack, deco.ac.val, "kSTBranchTrueBack"} );
  return 1;
}

int DoBranchIfFalse(Deco &deco) {
  (void)deco;
//  State s = stack.back(); stack.pop_back();
//  if (s.type != kSTExpr) {
//    std::cout << "ERROR: " << deco.ip << ": 'branch_if_false': expected Expression as first argument!\n" << std::endl;
//    return -1;
//  }
//  stack.push_back( { kSTCondition, 0, "if not " + s.text + " begin"} );
  if (deco.ac.val > deco.ip)
    stack.push_back( { kSTBranchFalseFwd, deco.ac.val, "kSTBranchFalseFwd"} );
  else
    stack.push_back( { kSTBranchFalseBack, deco.ac.val, "kSTBranchFalseBack"} );
  return 1;
}

int DoLogicalAnd(Deco &deco) {
  (void)deco;
  // TODO: if this is the final node (last before return?), the compiler emits different code!
  if (stack.size()<5)
    return 0;
//  std::cout << "---- DoLogicalAnd " << deco.ac.val << " ----\n";
//  PrintStack();
//  std::cout << "---- DoLogicalAnd mid " << deco.ac.val << " ----\n";
  size_t n = stack.size();
  if (   (stack[n-1].type == kSTExpr) /* TODO: it's exactly 'push_const NIL' */
      && (stack[n-2].type == kSTBranchFwd) /* TODO: and correct address*/
      && (stack[n-3].type == kSTExpr)
      && (stack[n-4].type == kSTBranchFalseFwd) /* TODO: and correct address*/
      && (stack[n-5].type == kSTExpr) )
  {
    // TODO: add brackets around arguments if needed
    std::string logic_and = stack[n-4].text + " and " + stack[n-2].text;
    stack.pop_back();
    stack.pop_back();
    stack.pop_back();
    stack.pop_back();
    stack.pop_back();
    stack.push_back( { kSTExpr, 11, logic_and} );
//    PrintStack();
//    std::cout << "---- DoLogicalAnd END " << deco.ac.val << " ----\n";
    return 1;
  } else {
//    std::cout << "---- not a LogicalAnd ----\n";
    return 0;
  }
}

int DoLabel(Deco &deco) {
  (void)deco;
  if (DoLogicalAnd(deco) == 0)
    return 1;

  std::cout << "---- label " << deco.ac.val << " ----\n";
  PrintStack();
  std::cout << "---- end label " << deco.ac.val << " ----\n";

//  if (altcode[deco.ip-1].cmd == kACBranch) {
//    This is probably an Else statment
//  }

//  Search up to find all branches that end here
  // TODO: `and`, `or` are implemented using `if` statements!

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

  // for ... to ... by ... do ...
  //    set_var local start:3, end:4, incr:5
  //    get_var incr
  //    get_var start
  //    branch m
  // label n:
  //    [statement(s)]
  //    get_var incr
  //    incr_var start
  // label m:
  //    get_var end
  //    branch_loop n


  // TODO: any of these can have a `break` anywhere, there is no `continue`
  // if testExpression then expression [;] [else alternateExpression]
  // for counter := initialValue to finalValue [by incrValue] do expression
  // foreach [slot,] value [deeply] in {frame | array} {collect | do} expression
  // loop expression
  // while condition do expression
  // repeat expression[s] until condition

  // TODO: exceptions
  // try [begin] expression[s] [onException symbol do statement]* [end]
  // (also: throw, rethrow )

//  return func() begin
//  try
//  x := 3;
//  x := 3;
//  onException |ev.xy| do
//    x := 4;
//  onException |ev.zx| do
//    x := 5;
//  end

//  0   0:     push lit_0
//  1   1:     push_const imm_96
//  2   4:     push lit_1
//  3   5:     push_const imm_128
//  4   8:     new_handler #exc_2
//  5   9:     push_const imm_12
//  6  12:     find_and_set_var lit_2
//  7  13:     push_const imm_12
//  8  16:     find_and_set_var lit_2
//  9  17:     find_var lit_2
//  10  18:     pop_handlers
//  11  21:     branch pc=40
//  12  24:     push_const imm_16
//  13  27:     find_and_set_var lit_2
//  14  28:     find_var lit_2
//  15  29:     branch pc=37
//  16  32:     push_const imm_20
//  17  35:     find_and_set_var lit_2
//  18  36:     find_var lit_2
//  19  37:     pop_handlers
//  20  40:     return



//  Search down to find all branches that end here



  // TODO: write this!
//  State &s = stack.back();
//  if (s.type != kSTExpr) {
//    std::cout << "ERROR: " << deco.ip << ": 'branch_if_false': expected Expression as first argument!\n" << std::endl;
//    return -1;
//  }
//  s.type = kSTStatement;
//  stack.push_back( { kSTUnknown, 0, "end"} );

//  return func() begin
//  a := 2 and 4;
//  end
// ----
//  0   0:     push_const 2
//  1   3:     branch_if_false pc=12
//  2   6:     push_const 4
//  3   9:     branch pc=13
//  4  12: label12:
//  5  12:     push_const nil
//  6  13: label13:
//  7  13:     find_and_set_var lit_0
//  8  14:     find_var lit_0
//  9  15:     return
//  10  16:     EOF
//  expr branch_f(a) expr branch(b) a: nil b:   -> expr

//  return func() begin
//  a := 2 or 4;
//  end
// ----
//  0   0:     push_const 2
//  1   3:     branch_if_true pc=12
//  2   6:     push_const 4
//  3   9:     branch pc=15
//  4  12: label12:
//  5  12:     push_const true
//  6  15: label15:
//  7  15:     find_and_set_var lit_0
//  8  16:     find_var lit_0
//  9  17:     return
//  10  18:     EOF
// expr branch_t(a) expr branch(b) a: true b:   -> expr

  return 1;
}

int DoPop(Deco &deco) {
  State &s = stack.back();
  if (s.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": 'pop': expected expression on stack!\n" << std::endl;
    return -1;
  }
  s.type = kSTStatement;
  return 1;
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
  return 1;
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
  return 1;
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
  return 1;
}

int DoSend(Deco &deco) { return DoSend(deco, ":", "send", false); }
int DoSendIfDefined(Deco &deco) { return DoSend(deco, ":?", "send_if_defined", false); }
int DoResend(Deco &deco) { return DoSend(deco, ":", "resend", true); }
int DoResendIfDefined(Deco &deco) { return DoSend(deco, ":?", "resend_if_defined", true); }

int DoPush(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "lit_" + std::to_string(deco.ac.val)} );
  return 1;
}

int DoFindVar(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "lit_" + std::to_string(deco.ac.val)} );
  return 1;
}

int DoPushSelf(Deco &deco) {
  (void)deco;
  stack.push_back( { kSTExpr, 0, "self"} );
  return 1;
}

int DoFindAndSetVar(Deco &deco) {
  State value = stack.back();
  if (value.type != kSTExpr) {
    std::cout << "ERROR: " << deco.ip << ": 'find_and_set_var': expected expression on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  stack.push_back( { kSTStatement, 0, "lit_" + std::to_string(deco.ac.val) + " := " + value.text} );
  return 1;
}

int DoXXX(Deco &deco) {
  (void)deco;
  return 1;
}

VcHandler handler[] = {
//kACEOF,             kACPop,             kACDup,             kACReturn,
  DoEOF,              DoPop,              DoUnknown,          DoReturn,
//kACPushSelf,        kACSetLexScope,     kACIterNext,        kACIterDone,
  DoPushSelf,         DoUnknown,          DoUnknown,          DoUnknown,
//kACPopHandlers,     kACPush,            kACPushConst,       kACCall,
  DoUnknown,          DoPush,             DoPushConst,        DoCall,
//kACInvoke,          kACSend,            kACSendIfDefined,   kACResend,
  DoInvoke,           DoSend,             DoSendIfDefined,    DoResend,
//kACResendIfDefined, kACBranch,          kACBranchIfTrue,    kACBranchIfFalse,
  DoResendIfDefined,  DoBranch,           DoBranchIfTrue,     DoBranchIfFalse,
//kACFindVar,         kACGetVar,          kACMakeFrame,       kACMakeArray,
  DoFindVar,          DoUnknown,          DoUnknown,          DoUnknown,
//kACFillArray,       kACGetPath,         kACGetPathCheck,    kACSetPath,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kACSetPathVal,      kACSetVar,          kACFindAndSetVar,   kACIncrVar,
  DoUnknown,          DoUnknown,          DoFindAndSetVar,    DoUnknown,
//kACBranchLoop,      kACAdd,             kACSubtract,        kACARef,
  DoUnknown,          DoAdd,              DoSubtract,         DoUnknown,
//kACSetARef,         kACEquals,          kACNot,             kACNotEquals,
  DoUnknown,          DoEquals,           DoUnknown,          DoNotEquals,
//kACMultiply,        kACDivide,          kACDiv,             kACLessThan,
  DoMultiply,         DoDivide,           DoUnknown,          DoLessThan,
//kACGreaterThan,     kACGreaterOrEqual,  kACLessOrEqual,     kACBitAnd,
  DoGreaterThan,      DoGreaterOrEqual,   DoLessOrEqual,      DoUnknown,
//kACBitOr,           kACBitNot,          kACNewIter,         kACLength,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kACClone,           kACSetClass,        kACAddArraySlot,    kACStringer,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//kACHasPath,         kACClassOf,         kACNewHandler,      kACLabel
  DoUnknown,          DoUnknown,          DoUnknown,          DoLabel,
};

static void print_altcode(int ip, AltCode &ac) {
  std::cout << std::setw(4) << ip << std::setw(4) << ac.ip << ": ";
  switch (ac.cmd) {
    case kACEOF:              std::cout << "    EOF" << std::endl; break;
    case kACPop:              std::cout << "    pop" << std::endl; break;
    case kACDup:              std::cout << "    dup" << std::endl; break;
    case kACReturn:           std::cout << "    return" << std::endl; break;
    case kACPushSelf:         std::cout << "    push_self" << std::endl; break;
    case kACSetLexScope:      std::cout << "    set_lex_scope" << std::endl; break;
    case kACIterNext:         std::cout << "    iter_next" << std::endl; break;
    case kACIterDone:         std::cout << "    iter_done" << std::endl; break;
    case kACPopHandlers:      std::cout << "    pop_handlers" << std::endl; break;
    case kACPush:             std::cout << "    push lit_" << ac.val << std::endl; break;
    case kACPushConst:        std::cout << "    push_const imm_" << ac.val << std::endl; break;
    case kACCall:             std::cout << "    call #args_" << ac.val << std::endl; break;
    case kACInvoke:           std::cout << "    invoke #args_" << ac.val << std::endl; break;
    case kACSend:             std::cout << "    send #args_" << ac.val << std::endl; break;
    case kACSendIfDefined:    std::cout << "    send_if_defined #args_" << ac.val << std::endl; break;
    case kACResend:           std::cout << "    resend #args_" << ac.val << std::endl; break;
    case kACResendIfDefined:  std::cout << "    resend_if_defined #args_" << ac.val << std::endl; break;
    case kACBranch:           std::cout << "    branch pc=" << ac.val << std::endl; break;
    case kACBranchIfTrue:     std::cout << "    branch_if_true pc=" << ac.val << std::endl; break;
    case kACBranchIfFalse:    std::cout << "    branch_if_false pc=" << ac.val << std::endl; break;
    case kACFindVar:          std::cout << "    find_var lit_" << ac.val << std::endl; break;
    case kACGetVar:           std::cout << "    get_var local_" << ac.val << std::endl; break;
    case kACMakeFrame:        std::cout << "    make_frame #slots_" << ac.val << std::endl; break;
    case kACMakeArray:        std::cout << "    make_array #slots_" << ac.val << std::endl; break;
    case kACFillArray:        std::cout << "    fill_array" << std::endl; break;
    case kACGetPath:          std::cout << "    get_path" << std::endl; break;
    case kACGetPathCheck:     std::cout << "    get_path_check" << std::endl; break;
    case kACSetPath:          std::cout << "    set_path" << std::endl; break;
    case kACSetPathVal:       std::cout << "    set_path_val" << std::endl; break;
    case kACSetVar:           std::cout << "    set_var local_" << ac.val << std::endl; break;
    case kACFindAndSetVar:    std::cout << "    find_and_set_var lit_" << ac.val << std::endl; break;
    case kACIncrVar:          std::cout << "    incr_var loc_" << ac.val << std::endl; break;
    case kACBranchLoop:       std::cout << "    branch_loop pc=" << ac.val << std::endl; break;
    case kACAdd:              std::cout << "    add" << std::endl; break;
    case kACSubtract:         std::cout << "    subtract" << std::endl; break;
    case kACARef:             std::cout << "    aref" << std::endl; break;
    case kACSetARef:          std::cout << "    set_aref" << std::endl; break;
    case kACEquals:           std::cout << "    equals" << std::endl; break;
    case kACNot:              std::cout << "    not" << std::endl; break;
    case kACNotEquals:        std::cout << "    not_equals" << std::endl; break;
    case kACMultiply:         std::cout << "    multiply" << std::endl; break;
    case kACDivide:           std::cout << "    divide" << std::endl; break;
    case kACDiv:              std::cout << "    div" << std::endl; break;
    case kACLessThan:         std::cout << "    less_than" << std::endl; break;
    case kACGreaterThan:      std::cout << "    greater_than" << std::endl; break;
    case kACGreaterOrEqual:   std::cout << "    greater_or_equal" << std::endl; break;
    case kACLessOrEqual:      std::cout << "    less_or_equal" << std::endl; break;
    case kACBitAnd:           std::cout << "    bit_and" << std::endl; break;
    case kACBitOr:            std::cout << "    bit_or" << std::endl; break;
    case kACBitNot:           std::cout << "    bit_not" << std::endl; break;
    case kACNewIter:          std::cout << "    new_iter" << std::endl; break;
    case kACLength:           std::cout << "    length" << std::endl; break;
    case kACClone:            std::cout << "    clone" << std::endl; break;
    case kACSetClass:         std::cout << "    set_class" << std::endl; break;
    case kACAddArraySlot:     std::cout << "    add_array_slot" << std::endl; break;
    case kACStringer:         std::cout << "    stringer" << std::endl; break;
    case kACHasPath:          std::cout << "    has_path" << std::endl; break;
    case kACClassOf:          std::cout << "    class_of" << std::endl; break;
    case kACNewHandler:       std::cout << "    new_handler #exc_" << ac.val << std::endl; break;
    case kACLabel:            std::cout << "label" << ac.val << ":" << std::endl; break;
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
      label_map[b] = -1;
    }
  }

  // Generate an uncompressed byte code with some extras.
  for (size_t i=0; i<n_inst; i++) {
    auto ip = i;
    if (label_map.find((int)i) != label_map.end()) {
      label_map[(int)i] = (int)altcode.size();
      altcode.push_back({kACLabel, (int)altcode.size(), ip});
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
          case 0: altcode.push_back({kACPop, 0, ip}); break;
          case 1: altcode.push_back({kACDup, 0, ip}); break;
          case 2: altcode.push_back({kACReturn, 0, ip}); break;
          case 3: altcode.push_back({kACPushSelf, 0, ip}); break;
          case 4: altcode.push_back({kACSetLexScope, 0, ip}); break;
          case 5: altcode.push_back({kACIterNext, 0, ip}); break;
          case 6: altcode.push_back({kACIterDone, 0, ip}); break;
          case 7: altcode.push_back({kACPopHandlers, 0, ip}); break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 3: altcode.push_back({kACPush, (int16_t)b, ip}); break;
      case 4: altcode.push_back({kACPushConst, (int16_t)b, ip}); break;
      case 5: altcode.push_back({kACCall, (int16_t)b, ip}); break;
      case 6: altcode.push_back({kACInvoke, (int16_t)b, ip}); break;
      case 7: altcode.push_back({kACSend, (int16_t)b, ip}); break;
      case 8: altcode.push_back({kACSendIfDefined, (int16_t)b, ip}); break;
      case 9: altcode.push_back({kACResend, (int16_t)b, ip}); break;
      case 10: altcode.push_back({kACResendIfDefined, (int16_t)b, ip}); break;
      case 11: altcode.push_back({kACBranch, (int16_t)b, ip}); break;
      case 12: altcode.push_back({kACBranchIfTrue, (int16_t)b, ip}); break;
      case 13: altcode.push_back({kACBranchIfFalse, (int16_t)b, ip}); break;
      case 14: altcode.push_back({kACFindVar, (int16_t)b, ip}); break;
      case 15: altcode.push_back({kACGetVar, (int16_t)b, ip}); break;
      case 16: altcode.push_back({kACMakeFrame, (int16_t)b, ip}); break;
      case 17: altcode.push_back({kACMakeArray, (int16_t)b, ip}); break;
      case 18: altcode.push_back({kACGetPath, (int16_t)b, ip}); break;
      case 19: altcode.push_back({kACSetPath, (int16_t)b, ip}); break;
      case 20: altcode.push_back({kACSetVar, (int16_t)b, ip}); break;
      case 21: altcode.push_back({kACFindAndSetVar, (int16_t)b, ip}); break;
      case 22: altcode.push_back({kACIncrVar, (int16_t)b, ip}); break;
      case 23: altcode.push_back({kACBranchLoop, (int16_t)b, ip}); break;
      case 24:
        switch (b) {
          case 0: altcode.push_back({kACAdd, 0, ip}); break;
          case 1: altcode.push_back({kACSubtract, 0, ip}); break;
          case 2: altcode.push_back({kACARef, 0, ip}); break;
          case 3: altcode.push_back({kACSetARef, 0, ip}); break;
          case 4: altcode.push_back({kACEquals, 0, ip}); break;
          case 5: altcode.push_back({kACNot, 0, ip}); break;
          case 6: altcode.push_back({kACNotEquals, 0, ip}); break;
          case 7: altcode.push_back({kACMultiply, 0, ip}); break;
          case 8: altcode.push_back({kACDivide, 0, ip}); break;
          case 9: altcode.push_back({kACDiv, 0, ip}); break;
          case 10: altcode.push_back({kACLessThan, 0, ip}); break;
          case 11: altcode.push_back({kACGreaterThan, 0, ip}); break;
          case 12: altcode.push_back({kACGreaterOrEqual, 0, ip}); break;
          case 13: altcode.push_back({kACLessOrEqual, 0, ip}); break;
          case 14: altcode.push_back({kACBitAnd, 0, ip}); break;
          case 15: altcode.push_back({kACBitOr, 0, ip}); break;
          case 16: altcode.push_back({kACBitNot, 0, ip}); break;
          case 17: altcode.push_back({kACNewIter, 0, ip}); break;
          case 18: altcode.push_back({kACLength, 0, ip}); break;
          case 19: altcode.push_back({kACClone, 0, ip}); break;
          case 20: altcode.push_back({kACSetClass, 0, ip}); break;
          case 21: altcode.push_back({kACAddArraySlot, 0, ip}); break;
          case 22: altcode.push_back({kACStringer, 0, ip}); break;
          case 23: altcode.push_back({kACHasPath, 0, ip}); break;
          case 24: altcode.push_back({kACClassOf, 0, ip}); break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
            break;
        }
        break;
      case 25: altcode.push_back({kACNewHandler, (int16_t)b, ip}); break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)ip << "." << std::endl;
        break;
    }
  }
  altcode.push_back({kACEOF, 0, n_inst});
  return 0;
}

bool decode() {
  Deco deco;
  int num_altcodes_handled = 0;
  for (size_t i=0; i<altcode.size(); ) {
    deco.ip = i;
    deco.ac = altcode[i];
    num_altcodes_handled = handler[deco.ac.cmd](deco);
    if (num_altcodes_handled <= 0) break;
    i += num_altcodes_handled;
  }
  if (num_altcodes_handled < -1) {
    std::cout << "ERROR: can't decode altcodes." << std::endl;
    return false;
  }
  if (num_altcodes_handled == 0) {
    std::cout << "ERROR: unknown altcode found." << std::endl;
    return false;
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
