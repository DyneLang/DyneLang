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
#include "transcode.h"
#include <dyn/objects.h>

#include <stdio.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace dyn;

using namespace dyn::lang;

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

 TODO: use an enum class for the precedence level
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

// TODO: bytecode here meand Dyne bytecode. Move all Newton bytecode references to 'transcode.cpp'
// TODO: have a function that converts bytecode index into altcode index
// TODO: find literals that will build a 'for' loop, so we can preprocess that
// TODO: 'foreach': new_iter, iter_next

using BCIP = size_t;

Bytecode dc_empty { BC::EndOfFile, 0, 0, 0 };
// TODO: rename this, it's the current state of the Decompiler
// TODO: add the global variables to it and make sure the destructor works
class State {
public:
  PC pc { 0 };
  Bytecode &bytecode { dc_empty };
};

typedef int (*VcHandler)(State&);

enum class ND {
  EndOfStack, Unknown, Error, Expr, Statement, Condition,
  BranchFwd, BranchBack, BranchTrueFwd, BranchTrueBack,
  BranchFalseFwd, BranchFalseBack
};

/**
 A node in the syntax stack.
 */
class Node {
public:
  ND type;
  int arg;
  std::string text;
  PC pc { 0 };
  int info { 0 }; // additional information field depends on Node type
  // expr 1 = push_const nil
  // expr 2 = push_const true
  // - add a string vector option here if we have a bunch of statements that need indenting later
  // - add the original AltCode here?
};

static std::vector<Bytecode> altcode { };

/*
 A stack holding the syntax nodes as we decompile.
 We may have to use a tree instead of a stack if things get
 too complicated.
 */
static std::vector<Node> stack { };


void PrintStack() {
  for (auto &state: stack) {
    switch (state.type) {
      case ND::EndOfStack:       std::cout << "------------: "; break;
      case ND::Unknown:          std::cout << "     unknown: "; break;
      case ND::Error:            std::cout << "       ERROR: "; break;
      case ND::Expr:             std::cout << "        expr: "; break;
      case ND::Statement:        std::cout << "   statement: "; break;
      case ND::Condition:        std::cout << "   condition: "; break;
      case ND::BranchFwd:        std::cout << "       b_fwd: "; break;
      case ND::BranchBack:       std::cout << "      b_back: "; break;
      case ND::BranchTrueFwd:    std::cout << "  b_true_fwd: "; break;
      case ND::BranchTrueBack:   std::cout << " b_true_back: "; break;
      case ND::BranchFalseFwd:   std::cout << " b_false_fwd: "; break;
      case ND::BranchFalseBack:  std::cout << "b_false_back: "; break;
      default:                  std::cout << "         ???: "; break;
    }
    std::cout << std::setw(4) << state.arg << ": " << state.text << std::endl;
  }
}

int DoUnknown(State &state) {
  stack.push_back( {
    ND::Unknown, 0,
    "ERROR: Unknown altcode " + std::to_string((int)state.bytecode.bytecode) + ", " + std::to_string(state.bytecode.arg)
    + " at " + std::to_string(state.bytecode.pc) } );
  return 1;
}

int DoEOF(State &state) {
  (void)state;
  return -1;
}

int DoPushConst(State &state) {
  int info = 0;
  if (state.bytecode.arg ==  2) info = 1; // push_const nil
  if (state.bytecode.arg == 26) info = 2; // push_const true
  Node nd { ND::Expr, 0, "imm_" + std::to_string(state.bytecode.arg) };
  nd.info = info;
  nd.pc = state.bytecode.pc;
  stack.push_back( nd );
  return 1;
}

int DoInfixOperator(State &state, const std::string &op, int precedence) {
  (void)state;
  Node s1 = stack.back(); stack.pop_back();
  if (s1.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << op << "': expected Expression as first argument!\n" << std::endl;
    return -1;
  }
  Node s2 = stack.back(); stack.pop_back();
  if (s2.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << op << "': expected Expression as second argument!\n" << std::endl;
    return -1;
  }
  stack.push_back( { ND::Expr, precedence,
      (precedence < s2.arg ? "(" : "") + s2.text
    + (precedence < s2.arg ? ")" : "")
    + " " + op + " "
    + (precedence < s1.arg ? "(" : "") + s1.text
    + (precedence < s1.arg ? ")" : "") } );
  return 1;
}

// equality and relational operators
int DoEquals(State &state) { return DoInfixOperator(state, "=", 9); }
int DoNotEquals(State &state) { return DoInfixOperator(state, "<>", 9); }
int DoGreaterOrEqual(State &state) { return DoInfixOperator(state, ">=", 9); }
int DoGreaterThan(State &state) { return DoInfixOperator(state, ">", 9); }
int DoLessOrEqual(State &state) { return DoInfixOperator(state, "<=", 9); }
int DoLessThan(State &state) { return DoInfixOperator(state, "<", 9); }
// arithmetic operators
int DoAdd(State &state) { return DoInfixOperator(state, "+", 6); }
int DoSubtract(State &state) { return DoInfixOperator(state, "-", 6); }
int DoMultiply(State &state) { return DoInfixOperator(state, "*", 5); }
int DoDivide(State &state) { return DoInfixOperator(state, "/", 5); }
// TODO: div mod << >>
//   a div b
//   mod(a, b)  // expr; expr; lit; call #2
//   `<<(a, b)  // expr; expr; lit; call #2
// unary operators
// TODO: `-` not
//    '-' -> negate(a);
//    'not' -> expr; not;
// string operators
// TODO: & &&
//    & -> expr; expr; lit:'array; make_array #2; stringer
//    & &-> expr; lit " "; expr; lit 'array; make_array #3; stringer
// `exists` operator
//    -> HasVar(a);


int DoReturn(State &state) {
  Node s = stack.back();
  // Handle the 'return' command at the end of the function.
  if (altcode[state.pc+1].bytecode == BC::EndOfFile) {
    // The compiler added a 'return' even though it was not needed. Don't output anything
    if (s.type != ND::Expr) {
      return 1;
    }
    // The compiler added a 'return NIL'. This is the default if there is no
    // return command, so don't write anything.
    if ((state.pc > 0) && (altcode[state.pc-1].bytecode == BC::PushConst) && (altcode[state.pc-1].arg == 2)) {
      stack.pop_back();
      return 1;
    }
  } else {
    // We are not at the end of the function, so check if we can generate the 'return'.
    if (s.type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": 'return': expected Expression as first argument!\n" << std::endl;
      return -1;
    }
  }
  stack.pop_back();
  stack.push_back( { ND::Statement, 0, "return " + s.text } );
  return 1;
}

/**
 Called by DoLabel, check if this is the end of an 'and' operation.
 \param[inout] state the current state of the decompiler
 \return 1 if this was an 'and' operation, 0 if not
 */
int CheckLogicAnd(State &state) {
  if (stack.size()<6) // not really needed, but makes the code faster
    return 0;
  size_t n = stack.size();
  if (   ((stack[n-1].type == ND::Expr) && (stack[n-1].info == 1)) // 'push_const nil'
      && ((stack[n-2].type == ND::BranchFwd) && ((PC)stack[n-2].arg == state.pc))
      && (stack[n-3].type == ND::Expr)
      && ((stack[n-4].type == ND::BranchFalseFwd) && ((PC)stack[n-4].arg == stack[n-1].pc))
      && (stack[n-5].type == ND::Expr) )
  {
    int precedence = 11;
    std::string text =
        (precedence < stack[n-5].arg ? "(" : "") + stack[n-5].text
      + (precedence < stack[n-5].arg ? ")" : "")
      + " and "
      + (precedence < stack[n-3].arg ? "(" : "") + stack[n-3].text
      + (precedence < stack[n-3].arg ? ")" : "");
    for (int i=5; i>0; --i) stack.pop_back();
    stack.push_back( { ND::Expr, 11, text} );
    return 1;
  } else {
    return 0;
  }
}

/**
 Called by DoLabel, check if this is the end of an 'or' operation.
 \param[inout] state the current state of the decompiler
 \return 1 if this was an 'and' operation, 0 if not
 */
int CheckLogicOr(State &state) {
  if (stack.size()<6) // not really needed, but makes the code faster
    return 0;
  size_t n = stack.size();
  if (   ((stack[n-1].type == ND::Expr) && (stack[n-1].info == 2)) // 'push_const true'
      && ((stack[n-2].type == ND::BranchFwd) && ((PC)stack[n-2].arg == state.pc))
      && (stack[n-3].type == ND::Expr)
      && ((stack[n-4].type == ND::BranchTrueFwd) && ((PC)stack[n-4].arg == stack[n-1].pc))
      && (stack[n-5].type == ND::Expr) )
  {
    int precedence = 11;
    std::string text =
        (precedence < stack[n-5].arg ? "(" : "") + stack[n-5].text
      + (precedence < stack[n-5].arg ? ")" : "")
      + " or "
      + (precedence < stack[n-3].arg ? "(" : "") + stack[n-3].text
      + (precedence < stack[n-3].arg ? ")" : "");
    for (int i=5; i>0; --i) stack.pop_back();
    stack.push_back( { ND::Expr, 11, text} );
    return 1;
  } else {
    return 0;
  }
}

/**
 \code
    expr
    branch_if_false_fwd a
    [statement]*
    expr
    branch_fwd b
 label a
    push_const nil
 label b
 \endcode
 \note if..then is an expression and returns a value!
 */
//int CheckIfThen(State &state) {
//  (void)state;
//  return 0;
//}

/**
 \code
    expr
    branch_if_false_fwd a
    [statement]*
    expr
    branch_fwd b
 label a
    [statement]*
    expr
 label b
 \endcode
 \note if..then is an expression and returns a value!
 */
int CheckIfThenElse(State &state) {
  (void)state;
//  PC label_ // it's not the PC but the index in the stack :-/
//  PC c = n-1;
//  // We always create an expression last:
//  if (stack[c].type != ND::Expr) return 0;
//  // Skip all statements:
//  do c--; while (stack[c].type == ND::Statement);
//  // Now this should be a label:
//  PC label_a = c;
#if 0
  size_t n = stack.size();
  if (   ((stack[n-1].type == ND::Expr) && (stack[n-1].info == 2)) // 'push_const true'
      && ((stack[n-2].type == ND::BranchFwd) && ((PC)stack[n-2].arg == state.pc))
      && (stack[n-3].type == ND::Expr)
      && ((stack[n-4].type == ND::BranchTrueFwd) && ((PC)stack[n-4].arg == stack[n-1].pc))
      && (stack[n-5].type == ND::Expr) )
  {
    int precedence = 11;
    std::string text =
    (precedence < stack[n-5].arg ? "(" : "") + stack[n-5].text
    + (precedence < stack[n-5].arg ? ")" : "")
    + " or "
    + (precedence < stack[n-3].arg ? "(" : "") + stack[n-3].text
    + (precedence < stack[n-3].arg ? ")" : "");
    for (int i=5; i>0; --i) stack.pop_back();
    stack.push_back( { ND::Expr, 11, text} );
    return 1;
  } else {
    return 0;
  }
#endif
  return 0;
}

/**
 \code
    branch_fwd a
 label b:
    [statements]*
 label a:
    expr
    branch_if_true_back b
    push_const nil  <--- note: `while-do` is an expr!
 \endcode
 \note The code changes substantially if there is a 'break' statement
 */
int CheckWhileDo(State &state) {
  (void)state;
  return 0;
}

/**
 \code
    branch_fwd a
 label b:
    [statements]*
    push_const nil; branch fwd c
    [statements]*
 label a:
    expr
    branch_if_true_back b
    push_const nil;   <--- note: `while-do` is an expr!
 label c:
 \endcode
 \note The code changes substantially if there is a 'break' statement
 */
int CheckWhileBreakDo(State &state) {
  (void)state;
  return 0;
}

/**
 \code
 label a
    [statement]*
    expr
    branch_if_false_back a
 \endcode
 */
int CheckRepeatUntil(State &state) {
  (void)state;
  return 0;
}

/**
 \code
 label a
    [statement]*
    push_const nil; branch fwd b
    (pop)               <--- note: the 'break' would be inside an 'if' statement which itself is an expr
    [statement]*
    expr
    branch_if_false_back a
    push_cont nil       <--- note: `while-do` is an expr!
 label b
 \endcode
 */
int CheckRepeatBreakUntil(State &state) {
  (void)state;
  return 0;
}

/**
 \code
 label a
    [statement]*
    push_const nil; branch fwd b
    (pop)               <--- note: the 'break' would be inside an 'if' statement which itself is an expr
    [statement]*
    branch_back a
 label b
 \endcode
 */
int CheckEndlessLoop(State &state) {
  (void)state;
  return 0;
}

/**
 \code
    expr
    set_var local_3
    expr
    set_var local_4
    expr
    set_var local_5
    get_var local_5
    get_var local_3
    branch_fwd a
 label b
    [statment]*
    get_var local_5
    incr_var loc_3
 label a
    get_var local_4
    branch_loop b
 \endcode
 */
int CheckForLoop(State &state) {
  (void)state;
  return 0;
}

/**
 \code
 foreach a in b do ...
  0:     find_var lit_0
  1:     push_const imm_2
  2:     new_iter
  3:     set_var local_4
  4:     branch pc=15
  5: label[1]:
  5:     get_var local_4
  6:     push_const imm_4
  7:     aref
  8:     set_var local_3
  *:     [statment]*
 13:     get_var local_4
 14:     iter_next
 15: label[1]:
 15:     get_var local_4
 16:     iter_done
 17:     branch_if_false pc=5
 18:     push_const imm_2
 19:     push_const imm_2
 20:     set_var local_4
 \endcode
 \note foreach [slot,] value [deeply] in {frame | array} {collect | do} expression
 */
int CheckForeach(State &state) {
  (void)state;
  return 0;
}

/**
 \code
    [push_lit symbol, push_const a[n]]*n
    new_handler n
    [statement]*
    expr
    pop_handlers
    branch b
 [label a0
    [statement]*
    expr
    branch_fwd c]
 label a0
    [statement]*
    expr
 label c
    pop_handlers
 label b

 \endcode
 */
int CheckTryOnExpression(State &state) {
  (void)state;
  return 0;
}

int DoLabel(State &state) {
  // Repeat finding actions related to this label until all found.
  for (;;) {
    if (CheckLogicAnd(state) == 1) continue;
    if (CheckLogicOr(state) == 1) continue;
    if (CheckIfThenElse(state) == 1) continue;
    // check "while ... break ... do ... "
    // check "if ... then" and "if ... then ... else"
    break;
  }
  //  if ... then ...
  //  if ... then ... else ...
  //  repeat ... until ...
  //  repeat ... break ... until ...
  //  loop ... break ...
  //  for counter := initialValue to finalValue [by incrValue] do expression
  //  foreach [slot,] value [deeply] in {frame | array} {collect | do} expression
  //  try [begin] expression[s] [onException symbol do statement]* [end]
  return 1;
}

int DoBranch(State &state) {
  (void)state;
  if ((PC)state.bytecode.arg > state.pc)
    stack.push_back( { ND::BranchFwd, state.bytecode.arg, "ND::BranchFwd"} );
  else
    stack.push_back( { ND::BranchBack, state.bytecode.arg, "ND::BranchBack"} );
  return 1;
}

int DoBranchIfTrue(State &state) {
  (void)state;
  if ((PC)state.bytecode.arg > state.pc)
    stack.push_back( { ND::BranchTrueFwd, state.bytecode.arg, "ND::BranchTrueFwd"} );
  else
    // check "while ... do ... "
    stack.push_back( { ND::BranchTrueBack, state.bytecode.arg, "ND::BranchTrueBack"} );
  return 1;
}

int DoBranchIfFalse(State &state) {
  (void)state;
  if ((PC)state.bytecode.arg > state.pc)
    stack.push_back( { ND::BranchFalseFwd, state.bytecode.arg, "ND::BranchFalseFwd"} );
  else
    //  State s = stack.back(); stack.pop_back();
    //  if (s.type != ND::Expr) {
    //    std::cout << "ERROR: " << state.ip << ": 'branch_if_false': expected Expression as first argument!\n" << std::endl;
    //    return -1;
    //  }
    //  stack.push_back( { ND::Condition, 0, "if not " + s.text + " begin"} );
    stack.push_back( { ND::BranchFalseBack, state.bytecode.arg, "ND::BranchFalseBack"} );
  return 1;
}

/**
 \note check if 'pop' cancel out an expr
 */
int DoPop(State &state) {
  Node &s = stack.back();
  if (s.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": 'pop': expected expression on stack!\n" << std::endl;
    return -1;
  }
  s.type = ND::Statement;
  return 1;
}

int DoArgList(State &state, std::string &args, int num_args) {
  for (int i=0; i<num_args; ++i) {
    Node &arg = stack.back();
    if (arg.type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": expected argument " << i << " expr on stack!\n" << std::endl;
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

int DoCallOrInvoke(State &state, const std::string &call, int which) {
  Node s = stack.back();
  if (s.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << call << "': expected function name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  std::string args { };
  if (DoArgList(state, args, state.bytecode.arg) == -1)
    return -1;
  if (which == 0) // call
    stack.push_back( { ND::Expr, 0, s.text + "(" + args + ")" } );
  else // invoke
    stack.push_back( { ND::Expr, 0, "call " + s.text + " with (" + args + ")" } );
  return 1;
}

int DoCall(State &state) { return DoCallOrInvoke(state, "call", 0); }
int DoInvoke(State &state) { return DoCallOrInvoke(state, "invoke", 1); }

int DoSend(State &state, const std::string &op, const std::string &call, bool is_resend) {
  Node name = stack.back();
  if (name.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << call << "': expected message name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  Node rcvr { };
  if (!is_resend) {
    rcvr = stack.back();
    if (rcvr.type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": '" << call << "': expected receiver on stack!\n" << std::endl;
      return -1;
    }
    stack.pop_back();
  }
  std::string args { };
  if (DoArgList(state, args, state.bytecode.arg) == -1)
    return -1;
  if (is_resend)
    stack.push_back( { ND::Expr, 0, "inherited" + op + name.text + "(" + args + ")" } );
  else
    stack.push_back( { ND::Expr, 0, rcvr.text + op + name.text + "(" + args + ")" } );
  return 1;
}

int DoSend(State &state) { return DoSend(state, ":", "send", false); }
int DoSendIfDefined(State &state) { return DoSend(state, ":?", "send_if_defined", false); }
int DoResend(State &state) { return DoSend(state, ":", "resend", true); }
int DoResendIfDefined(State &state) { return DoSend(state, ":?", "resend_if_defined", true); }

int DoPush(State &state) {
  (void)state;
  stack.push_back( { ND::Expr, 0, "lit_" + std::to_string(state.bytecode.arg)} );
  return 1;
}

int DoFindVar(State &state) {
  (void)state;
  stack.push_back( { ND::Expr, 0, "lit_" + std::to_string(state.bytecode.arg)} );
  return 1;
}

int DoPushSelf(State &state) {
  (void)state;
  stack.push_back( { ND::Expr, 0, "self"} );
  return 1;
}

/**
 Take an expression from the stack and put it into the slot named literal[arg].
 This does not leave a value on the stack, so it's a statement, not an expression.
 \param[inout] state the state of the decompiler
 \return -1 for end of func, <-1 for error, or the number of bytes consumed
 */
int DoFindAndSetVar(State &state) {
  Node value = stack.back();
  if (value.type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": 'find_and_set_var': expected expression on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  stack.push_back( { ND::Statement, 0, "lit_" + std::to_string(state.bytecode.arg) + " := " + value.text} );
  return 1;
}

/**
 Prototype.
 \param[inout] state the state of the decompiler
 \return -1 for end of func, <-1 for error, or the number of bytes consumed
 */
int DoXXX(State &state) {
  (void)state;
  return 1;
}

VcHandler handler[] = {
//DC::EOF,             DC::Pop,             DC::Dup,             DC::Return,
  DoEOF,              DoPop,              DoUnknown,          DoReturn,
//DC::PushSelf,        DC::SetLexScope,     DC::IterNext,        DC::IterDone,
  DoPushSelf,         DoUnknown,          DoUnknown,          DoUnknown,
//DC::PopHandlers,     DC::Push,            DC::PushConst,       DC::Call,
  DoUnknown,          DoPush,             DoPushConst,        DoCall,
//DC::Invoke,          DC::Send,            DC::SendIfDefined,   DC::Resend,
  DoInvoke,           DoSend,             DoSendIfDefined,    DoResend,
//DC::ResendIfDefined, DC::Branch,          DC::BranchIfTrue,    DC::BranchIfFalse,
  DoResendIfDefined,  DoBranch,           DoBranchIfTrue,     DoBranchIfFalse,
//DC::FindVar,         DC::GetVar,          DC::MakeFrame,       DC::MakeArray,
  DoFindVar,          DoUnknown,          DoUnknown,          DoUnknown,
//DC::FillArray,       DC::GetPath,         DC::GetPathCheck,    DC::SetPath,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//DC::SetPathVal,      DC::SetVar,          DC::FindAndSetVar,   DC::IncrVar,
  DoUnknown,          DoUnknown,          DoFindAndSetVar,    DoUnknown,
//DC::BranchLoop,      DC::Add,             DC::Subtract,        DC::ARef,
  DoUnknown,          DoAdd,              DoSubtract,         DoUnknown,
//DC::SetARef,         DC::Equals,          DC::Not,             DC::NotEquals,
  DoUnknown,          DoEquals,           DoUnknown,          DoNotEquals,
//DC::Multiply,        DC::Divide,          DC::Div,             DC::LessThan,
  DoMultiply,         DoDivide,           DoUnknown,          DoLessThan,
//DC::GreaterThan,     DC::GreaterOrEqual,  DC::LessOrEqual,     DC::BitAnd,
  DoGreaterThan,      DoGreaterOrEqual,   DoLessOrEqual,      DoUnknown,
//DC::BitOr,           DC::BitNot,          DC::NewIter,         DC::Length,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//DC::Clone,           DC::SetClass,        DC::AddArraySlot,    DC::Stringer,
  DoUnknown,          DoUnknown,          DoUnknown,          DoUnknown,
//DC::HasPath,         DC::ClassOf,         DC::NewHandler,      DC::Label
  DoUnknown,          DoUnknown,          DoUnknown,          DoLabel,
};

static void print_altcode(int ip, Bytecode &ac) {
  if (ac.references)
    std::cout << std::setw(4) << ip << ": label[" << ac.references << "]:" << std::endl;
  std::cout << std::setw(4) << ip << ": ";
  switch (ac.bytecode) {
    case BC::EndOfFile:        std::cout << "    EOF" << std::endl; break;
    case BC::Pop:              std::cout << "    pop" << std::endl; break;
    case BC::Dup:              std::cout << "    dup" << std::endl; break;
    case BC::Return:           std::cout << "    return" << std::endl; break;
    case BC::PushSelf:         std::cout << "    push_self" << std::endl; break;
    case BC::SetLexScope:      std::cout << "    set_lex_scope" << std::endl; break;
    case BC::IterNext:         std::cout << "    iter_next" << std::endl; break;
    case BC::IterDone:         std::cout << "    iter_done" << std::endl; break;
    case BC::PopHandlers:      std::cout << "    pop_handlers" << std::endl; break;
    case BC::Push:             std::cout << "    push lit_" << ac.arg << std::endl; break;
    case BC::PushConst:        std::cout << "    push_const imm_" << ac.arg << std::endl; break;
    case BC::Call:             std::cout << "    call #args_" << ac.arg << std::endl; break;
    case BC::Invoke:           std::cout << "    invoke #args_" << ac.arg << std::endl; break;
    case BC::Send:             std::cout << "    send #args_" << ac.arg << std::endl; break;
    case BC::SendIfDefined:    std::cout << "    send_if_defined #args_" << ac.arg << std::endl; break;
    case BC::Resend:           std::cout << "    resend #args_" << ac.arg << std::endl; break;
    case BC::ResendIfDefined:  std::cout << "    resend_if_defined #args_" << ac.arg << std::endl; break;
    case BC::Branch:           std::cout << "    branch pc=" << ac.arg << std::endl; break;
    case BC::BranchIfTrue:     std::cout << "    branch_if_true pc=" << ac.arg << std::endl; break;
    case BC::BranchIfFalse:    std::cout << "    branch_if_false pc=" << ac.arg << std::endl; break;
    case BC::FindVar:          std::cout << "    find_var lit_" << ac.arg << std::endl; break;
    case BC::GetVar:           std::cout << "    get_var local_" << ac.arg << std::endl; break;
    case BC::MakeFrame:        std::cout << "    make_frame #slots_" << ac.arg << std::endl; break;
    case BC::MakeArray:        std::cout << "    make_array #slots_" << ac.arg << std::endl; break;
    case BC::FillArray:        std::cout << "    fill_array" << std::endl; break;
    case BC::GetPath:          std::cout << "    get_path" << std::endl; break;
    case BC::GetPathCheck:     std::cout << "    get_path_check" << std::endl; break;
    case BC::SetPath:          std::cout << "    set_path" << std::endl; break;
    case BC::SetPathVal:       std::cout << "    set_path_val" << std::endl; break;
    case BC::SetVar:           std::cout << "    set_var local_" << ac.arg << std::endl; break;
    case BC::FindAndSetVar:    std::cout << "    find_and_set_var lit_" << ac.arg << std::endl; break;
    case BC::IncrVar:          std::cout << "    incr_var loc_" << ac.arg << std::endl; break;
    case BC::BranchLoop:       std::cout << "    branch_loop pc=" << ac.arg << std::endl; break;
    case BC::Add:              std::cout << "    add" << std::endl; break;
    case BC::Subtract:         std::cout << "    subtract" << std::endl; break;
    case BC::ARef:             std::cout << "    aref" << std::endl; break;
    case BC::SetARef:          std::cout << "    set_aref" << std::endl; break;
    case BC::Equals:           std::cout << "    equals" << std::endl; break;
    case BC::Not:              std::cout << "    not" << std::endl; break;
    case BC::NotEquals:        std::cout << "    not_equals" << std::endl; break;
    case BC::Multiply:         std::cout << "    multiply" << std::endl; break;
    case BC::Divide:           std::cout << "    divide" << std::endl; break;
    case BC::Div:              std::cout << "    div" << std::endl; break;
    case BC::LessThan:         std::cout << "    less_than" << std::endl; break;
    case BC::GreaterThan:      std::cout << "    greater_than" << std::endl; break;
    case BC::GreaterOrEqual:   std::cout << "    greater_or_equal" << std::endl; break;
    case BC::LessOrEqual:      std::cout << "    less_or_equal" << std::endl; break;
    case BC::BitAnd:           std::cout << "    bit_and" << std::endl; break;
    case BC::BitOr:            std::cout << "    bit_or" << std::endl; break;
    case BC::BitNot:           std::cout << "    bit_not" << std::endl; break;
    case BC::NewIter:          std::cout << "    new_iter" << std::endl; break;
    case BC::Length:           std::cout << "    length" << std::endl; break;
    case BC::Clone:            std::cout << "    clone" << std::endl; break;
    case BC::SetClass:         std::cout << "    set_class" << std::endl; break;
    case BC::AddArraySlot:     std::cout << "    add_array_slot" << std::endl; break;
    case BC::Stringer:         std::cout << "    stringer" << std::endl; break;
    case BC::HasPath:          std::cout << "    has_path" << std::endl; break;
    case BC::ClassOf:          std::cout << "    class_of" << std::endl; break;
    case BC::NewHandler:       std::cout << "    new_handler #exc_" << ac.arg << std::endl; break;
    default:
      std::cout << "ERROR: unknown altcode: a=" << (int)ac.bytecode << ", b=" << ac.arg << "." << std::endl; break;
  }
}

void dyn::lang::print_bytecode(std::vector<Bytecode> &func) {
  int i = 0;
  for (auto &bc: func) {
    print_altcode(i++, bc);
  }
}

bool decode() {
  State state;
  int num_altcodes_handled = 0;
  for (size_t i=0; i<altcode.size(); ) {
    if (altcode[i].references) {
      state.pc = i;
      state.bytecode = altcode[i];
      DoLabel(state);
    }
    state.pc = i;
    state.bytecode = altcode[i];
    num_altcodes_handled = handler[(int)state.bytecode.bytecode](state);
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
  altcode = transcode_from_ns(func);
  if (altcode.empty())
    return RefNIL;
  printf("--- expanded byte code\n");
  print_bytecode(altcode);
  stack.push_back( { ND::EndOfStack, 0, "... stack bottom ..." } );
  printf("--- decode\n");
  decode();
  printf("--- remaining stack:\n");
  PrintStack();

//  std::cout << stack.back() << std::endl; stack.pop_back();

  return RefNIL;
}
