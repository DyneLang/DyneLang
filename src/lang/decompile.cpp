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
#include <cassert>

using namespace dyn;

using namespace dyn::lang;

/*
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
  ND type { ND::Unknown };
  PC pc_first { kInvalidPC };
  PC pc_last { kInvalidPC };
  int arg { 0 };
  std::string text { };
  int info { 0 }; // additional information field depends on Node type
  // expr 1 = push_const nil
  // expr 2 = push_const true
  // - add a string vector option here if we have a bunch of statements that need indenting later
  // - add the original AltCode here?
  Node() { }
  Node(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info=0)
  : type(a_type), pc_first(a_pc_first), pc_last(a_pc_last), arg(a_arg), text(a_text), info(a_info)
  {
  }
  virtual ~Node() { }
  virtual std::string ToString() { return text; }
};

class NodeImmediate : public Node {
  dyn::Ref imm { RefNIL };
public:
  NodeImmediate(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info, RefArg a_ref)
  : Node(a_type, a_pc_first, a_pc_last, a_arg, a_text, a_info), imm(a_ref)
  { }
  std::string ToString() override { return imm.ToString(); }
};

class NodeControlFlow : public Node {
  std::shared_ptr<Node> condition_ { };
  std::vector<std::shared_ptr<Node>> statements_a_ { };
  std::vector<std::shared_ptr<Node>> statements_b_ { };
public:
  NodeControlFlow(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info)
  : Node(a_type, a_pc_first, a_pc_last, a_arg, a_text, a_info) { }
  void set_condition(std::shared_ptr<Node> condition) { condition_ = condition; }
  void add_statement_a(std::shared_ptr<Node> stmt) { statements_a_.push_back(stmt); }
  void add_statement_b(std::shared_ptr<Node> stmt) { statements_b_.push_back(stmt); }
  std::string ToString() override {
    // Write the expression
    std::string out { };
    switch (info) {
      case 0: out = "if " + condition_->ToString() + " then"; break;
      case 1: out = "while " + condition_->ToString() + " do"; break;
      case 2: out = "repeat"; break; // TODO: not yet implemented
    }
    bool add_begin_end = (statements_a_.size()>1) || (statements_b_.size()>1);
    // Write the statements in block a
    std::string stat_a { };
    if (add_begin_end) stat_a += " begin";
    stat_a += "\n";
    for (size_t i=0; i<statements_a_.size(); ++i)
      stat_a += "  " + statements_a_[i]->ToString() + ";\n";
    if (add_begin_end) stat_a += "end ";
    // Write the statements in block b
    if (statements_b_.empty()) {
      return out + stat_a;
    } else {
      std::string stat_b { };
      if (add_begin_end) stat_b += " begin";
      stat_b += "\n";
      for (size_t i=0; i<statements_b_.size(); ++i)
        stat_b += "  " + statements_b_[i]->ToString() + ";\n";
      if (add_begin_end) stat_b += "end ";
      // Write and if...then...else... statement
      return out + stat_a + "else" + stat_b;
    }
  }
};

void PrintStack(const std::vector<std::shared_ptr<Node>> &stack) {
  for (auto &node: stack) {
    switch (node->type) {
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
      default:                   std::cout << "         ???: "; break;
    }
    std::cout << std::setw(4) << node->arg << ": " << node->ToString() << std::endl;
  }
}

// TODO: this is now part of the class Decompiler. Define it there or remove it.
Bytecode dc_empty { BC::EndOfFile, 0, 0, 0 };
class State {
public:
  PC pc { 0 };
  Bytecode &bytecode { dc_empty };
};

class Decompiler;
typedef int (Decompiler::*BytecodeHandler)();

class Decompiler {
private:
  // Control Flow helpers:
  int CheckTryOnExpression();           // todo
  int CheckForeach();                   // todo
  int CheckForLoop();                   // todo
  int CheckEndlessLoop();               // todo
  int CheckRepeatBreakUntil();          // todo
  int CheckRepeatUntil();               // todo
  int CheckWhileBreakDo();              // todo
  int CheckWhileDo();
  int CheckLogicAnd();
  int CheckLogicOr();
  int CheckIfThen();
  int CheckIfThenElse();
  int CheckIfThenElseExpr();
  int DoLabel();
  // Bytecode helpers:
  int DoInfixOperator(const std::string &op, int precedence);
  int DoSend(const std::string &op, const std::string &call, bool is_resend);
  int DoCallOrInvoke(const std::string &call, int which);
  PC DoArgList(std::string &args, int num_args);
  // handle all known bytecodes:
  int DoEOF();
  int DoPop();
  int DoDup() { return -2; }            // Compiler does not emit this code
  int DoReturn();
  int DoPushSelf();
  int DoSetLexScope() { return -2; }    // when calling a function that is declared inside this function
  int DoIterNext() { return -2; }       // 'foreach'
  int DoIterDone() { return -2; }       // 'foreach'
  int DoPopHandlers() { return -2; };   // end of 'try' and 'onException'
  int DoPush();
  int DoPushConst();
  int DoCall();
  int DoInvoke();
  int DoSend();
  int DoSendIfDefined();
  int DoResend();
  int DoResendIfDefined();
  int DoBranch();
  int DoBranchIfTrue();
  int DoBranchIfFalse();
  int DoFindVar();
  int DoGetVar() { return -2; };        // read from a slot
  int DoMakeFrame() { return -2; };     // build a frame
  int DoMakeArray() { return -2; };     // build an array
  int DoFillArray() { return -2; };     // fill an array with values
  int DoGetPath() { return -2; };       // access path
  int DoGetPathCheck() { return -2; };  // access path
  int DoSetPath() { return -2; };       // access path
  int DoSetPathVal() { return -2; };    // access path
  int DoSetVar() { return -2; };        // write to a slot
  int DoFindAndSetVar();
  int DoIncrVar() { return -2; };       // 'for' loops
  int DoBranchLoop() { return -2; };    // 'for' loops
  int DoAdd();
  int DoSubtract();
  int DoARef() { return -2; };          // array read access
  int DoSetARef() { return -2; };       // array write access
  int DoEquals();
  int DoNot() { return -2; };           // unary operator
  int DoNotEquals();
  int DoMultiply();
  int DoDivide();
  int DoDiv() { return -2; };           // binary operator 'div'
  int DoLessThan();
  int DoGreaterThan();
  int DoGreaterOrEqual();
  int DoLessOrEqual();
  int DoBitAnd() { return -2; };        // global function call 'bAnd(ref, ref)'
  int DoBitOr() { return -2; };         // global function call 'bOr(ref, ref)'
  int DoBitNot() { return -2; };        // global function call 'bNot(ref)'
  int DoNewIter() { return -2; };       // 'foreach'
  int DoLength() { return -2; };        // global function call 'length(ref)', probably 'foreach'
  int DoClone() { return -2; };         // global function call 'clone(ref)'
  int DoSetClass() { return -2; };      // global function call 'SetClass(obj, symbol)'
  int DoAddArraySlot() { return -2; };  // global function call 'AddArraySlot(array, value)'
  int DoStringer() { return -2; };      // binary operators '&' and '&&'
  int DoHasPath() { return -2; };       // access path
  int DoClassOf() { return -2; };       // global function call 'ClassOf(obj)'
  int DoNewHandler() { return -2; };    // exception handling start: 'try'
  int DoUnknown() { return -2; };
public:
  std::vector<Bytecode> instructions { };
  std::vector<std::shared_ptr<Node>> stack { };
  State state;
  Ref ns_function;
  Ref ns_literals;
public:
  static const BytecodeHandler handler[];
  Decompiler(RefArg func);
  int Do(BC bc) {
    assert ((bc >= BC::EndOfFile) || (bc <= BC::Unknown));
    return (*this.*handler[(size_t)bc])(); }
  bool decode();
};

const BytecodeHandler Decompiler::handler[] = {
  &Decompiler::DoEOF, &Decompiler::DoPop, &Decompiler::DoDup, &Decompiler::DoReturn,
  &Decompiler::DoPushSelf, &Decompiler::DoSetLexScope, &Decompiler::DoIterNext, &Decompiler::DoIterDone,
  &Decompiler::DoPopHandlers, &Decompiler::DoPush, &Decompiler::DoPushConst, &Decompiler::DoCall,
  &Decompiler::DoInvoke, &Decompiler::DoSend, &Decompiler::DoSendIfDefined, &Decompiler::DoResend,
  &Decompiler::DoResendIfDefined, &Decompiler::DoBranch, &Decompiler::DoBranchIfTrue, &Decompiler::DoBranchIfFalse,
  &Decompiler::DoFindVar, &Decompiler::DoGetVar, &Decompiler::DoMakeFrame, &Decompiler::DoMakeArray,
  &Decompiler::DoFillArray, &Decompiler::DoGetPath, &Decompiler::DoGetPathCheck, &Decompiler::DoSetPath,
  &Decompiler::DoSetPathVal, &Decompiler::DoSetVar, &Decompiler::DoFindAndSetVar, &Decompiler::DoIncrVar,
  &Decompiler::DoBranchLoop, &Decompiler::DoAdd, &Decompiler::DoSubtract, &Decompiler::DoARef,
  &Decompiler::DoSetARef, &Decompiler::DoEquals, &Decompiler::DoNot, &Decompiler::DoNotEquals,
  &Decompiler::DoMultiply, &Decompiler::DoDivide, &Decompiler::DoDiv, &Decompiler::DoLessThan,
  &Decompiler::DoGreaterThan, &Decompiler::DoGreaterOrEqual, &Decompiler::DoLessOrEqual, &Decompiler::DoBitAnd,
  &Decompiler::DoBitOr, &Decompiler::DoBitNot, &Decompiler::DoNewIter, &Decompiler::DoLength,
  &Decompiler::DoClone, &Decompiler::DoSetClass, &Decompiler::DoAddArraySlot, &Decompiler::DoStringer,
  &Decompiler::DoHasPath, &Decompiler::DoClassOf, &Decompiler::DoNewHandler, &Decompiler::DoUnknown
};

/**
 Handle the end of function marker.
 \return -1 to tell the analyser that we reached the end of the function
 */
int Decompiler::DoEOF() {
  return -1;
}

/**
 Push a reference to an immediate onto the stack.
 \return number of bytecodes consumed
 */
int Decompiler::DoPushConst() {
  int info = 0;
  if (state.bytecode.arg ==  2) info = 1; // push_const nil
  if (state.bytecode.arg == 26) info = 2; // push_const true
  Ref r = Ref::NSRef(state.bytecode.arg);
  stack.push_back( std::make_shared<NodeImmediate>(ND::Expr, state.pc, state.pc, 0, "imm_" + std::to_string(state.bytecode.arg), info, r ) );
  return 1;
}

/**
 Handle infix operators like '+', '*', '<=', and more.
 \param[in] op Operator as it will appear in the source code.
 \param[in] precedence Level of precedence of this operation so we can determine
    if the left or right expression need to be bracketed.
 \return number of bytecodes consumed
 */
int Decompiler::DoInfixOperator(const std::string &op, int precedence) {
  Node *s1 = stack[stack.size()-1].get();
  if (s1->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << op << "': expected Expression as first argument!\n" << std::endl;
    return -1;
  }
  Node *s2 = stack[stack.size()-2].get();
  if (s2->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << op << "': expected Expression as second argument!\n" << std::endl;
    return -1;
  }
  PC pcf = s2->pc_first;
  std::string text {
    (precedence < s2->arg ? "(" : "") + s2->ToString()
    + (precedence < s2->arg ? ")" : "")
    + " " + op + " "
    + (precedence < s1->arg ? "(" : "") + s1->ToString()
    + (precedence < s1->arg ? ")" : "") };
  stack.pop_back();
  stack.pop_back();
  stack.push_back( std::make_shared<Node>( ND::Expr, pcf, state.pc, precedence, text) );
  return 1;
}

// equality and relational operators
int Decompiler::DoEquals() { return DoInfixOperator("=", 9); }
int Decompiler::DoNotEquals() { return DoInfixOperator("<>", 9); }
int Decompiler::DoGreaterOrEqual() { return DoInfixOperator(">=", 9); }
int Decompiler::DoGreaterThan() { return DoInfixOperator(">", 9); }
int Decompiler::DoLessOrEqual() { return DoInfixOperator("<=", 9); }
int Decompiler::DoLessThan() { return DoInfixOperator("<", 9); }
// arithmetic operators
int Decompiler::DoAdd() { return DoInfixOperator("+", 6); }
int Decompiler::DoSubtract() { return DoInfixOperator("-", 6); }
int Decompiler::DoMultiply() { return DoInfixOperator("*", 5); }
int Decompiler::DoDivide() { return DoInfixOperator("/", 5); }
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


int Decompiler::DoReturn() {
  auto expr = stack.back();
  // Handle the 'return' command at the end of the function.
  if (instructions[state.pc+1].bc == BC::EndOfFile) {
    // The compiler added a 'return' even though it was not needed. Don't output anything
    if (expr->type != ND::Expr) {
      return 1;
    }
    // The compiler added a 'return NIL'. This is the default if there is no
    // return command, so don't write anything.
    if ((state.pc > 0) && (instructions[state.pc-1].bc == BC::PushConst) && (instructions[state.pc-1].arg == 2)) {
      stack.pop_back();
      return 1;
    }
  } else {
    // We are not at the end of the function, so check if we can generate the 'return'.
    if (expr->type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": 'return': expected Expression as first argument!\n" << std::endl;
      return -1;
    }
  }
  PC pcf = expr->pc_first;
  stack.pop_back();
  stack.push_back( std::make_shared<Node>(ND::Statement, pcf, state.pc, 0, "return " + expr->ToString()) );
  return 1;
}

/**
 Called by DoLabel, check if this is the end of an 'and' operation.
 \param[inout] state the current state of the decompiler
 \return 1 if this was an 'and' operation, 0 if not
 */
int Decompiler::CheckLogicAnd() {
  if (stack.size()<6) // not really needed, but makes the code faster
    return 0;
  size_t n = stack.size();
  if (   ((stack[n-1]->type == ND::Expr) && (stack[n-1]->info == 1)) // 'push_const nil'
      && ((stack[n-2]->type == ND::BranchFwd) && ((PC)stack[n-2]->arg == state.pc))
      && (stack[n-3]->type == ND::Expr)
      && ((stack[n-4]->type == ND::BranchFalseFwd) && ((PC)stack[n-4]->arg == stack[n-1]->pc_first))
      && (stack[n-5]->type == ND::Expr) )
  {
    int precedence = 11;
    std::string text =
        (precedence < stack[n-5]->arg ? "(" : "") + stack[n-5]->ToString()
      + (precedence < stack[n-5]->arg ? ")" : "")
      + " and "
      + (precedence < stack[n-3]->arg ? "(" : "") + stack[n-3]->ToString()
      + (precedence < stack[n-3]->arg ? ")" : "");
    PC pcf = stack[n-5]->pc_first;
    for (int i=5; i>0; --i) stack.pop_back();
    stack.push_back( std::make_shared<Node>(ND::Expr, pcf, state.pc, 11, text) );
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
int Decompiler::CheckLogicOr() {
  if (stack.size()<6) // not really needed, but makes the code faster
    return 0;
  size_t n = stack.size();
  if (   ((stack[n-1]->type == ND::Expr) && (stack[n-1]->info == 2)) // 'push_const true'
      && ((stack[n-2]->type == ND::BranchFwd) && ((PC)stack[n-2]->arg == state.pc))
      && (stack[n-3]->type == ND::Expr)
      && ((stack[n-4]->type == ND::BranchTrueFwd) && ((PC)stack[n-4]->arg == stack[n-1]->pc_first))
      && (stack[n-5]->type == ND::Expr) )
  {
    int precedence = 11;
    std::string text =
        (precedence < stack[n-5]->arg ? "(" : "") + stack[n-5]->ToString()
      + (precedence < stack[n-5]->arg ? ")" : "")
      + " or "
      + (precedence < stack[n-3]->arg ? "(" : "") + stack[n-3]->ToString()
      + (precedence < stack[n-3]->arg ? ")" : "");
    PC pcf = stack[n-5]->pc_first;
    for (int i=5; i>0; --i) stack.pop_back();
    stack.push_back( std::make_shared<Node>(ND::Expr, pcf, state.pc, 11, text) );
    return 1;
  } else {
    return 0;
  }
}

/**
 Test if the nodes on the stack form an 'if...then...' flow statement.

 An 'if...then...' flow control statement has the footprint:
 \code
    expr
    branch_if_false_fwd a
    [statement]*
 label a
 \endcode

 This method is called when a label is encountered.
 */
int Decompiler::CheckIfThen()
{
  // -- Check if this is an 'if...then...' statement:
  size_t si = stack.size()-1; // current stack index
  PC label_a = state.pc;
  // The first entry must be a statement.
  if (stack[si]->type != ND::Statement) return 0;
  PC pcl = stack[si]->pc_last;
  PC stat_first = si;
  --si;
  // Skip zero or more statements.
  while (stack[si]->type == ND::Statement) --si;
  // So this is where label a is in the bytecode array.
  PC stat_last = si+1;
  // The initial 'if' statement must be a conditional jump to label a:
  if ((stack[si]->type != ND::BranchFalseFwd) || ((PC)stack[si]->arg != label_a)) return 0;
  --si;
  // And we need an expression to feed the conditional jump
  if (stack[si]->type != ND::Expr) return 0;
  PC condition_pc = si;
  PC pcf = stack[si]->pc_first;

  // -- Create a node on the stack, using all the information from above:
  auto node = std::make_shared<NodeControlFlow>(ND::Statement, pcf, pcl, 0, "if ... then ...; ", 0);
  node->set_condition(stack[condition_pc]);
  for (PC i=stat_last; i<=stat_first; ++i)
    node->add_statement_a(stack[i]);
  // Replace nodes on the stack that make up the new if...then...else... node
  while (stack.size()>condition_pc) stack.pop_back();
  stack.push_back(node);
  return 1;
}

/**
 Test if the nodes on the stack form an 'if...then...else...' flow statement.

 An 'if...then...else...' flow control statement has the footprint:
 \code
    expr
    branch_if_false_fwd a
    [statement]*
    branch_fwd b
 label a
    [statement]*
 label b
 \endcode

 This method is called when a label is encountered, but can only be successful
 if called from label b.

 \note CheckIfThenElse and CheckIfThenElseExpr are very similar. Try to bake
    this into a single method.
 */
int Decompiler::CheckIfThenElse()
{
  // -- Check if this is an 'if...then...else...' statement:
  size_t si = stack.size()-1; // current stack index
  PC label_b = state.pc;
  // The first entry must be a statement.
  if (stack[si]->type != ND::Statement) return 0;
  PC pcl = stack[si]->pc_last;
  PC stat_b_first = si;
  --si;
  // Skip zero or more statements.
  while (stack[si]->type == ND::Statement) --si;
  // So this is where label a is in the bytecode array.
  PC label_a = stack[si+1]->pc_first;
  PC stat_b_last = si+1;
  // The next command must be a forward jump to the current pc:
  if ((stack[si]->type != ND::BranchFwd) || ((PC)stack[si]->arg != label_b)) return 0;
  --si;
  // The first half of the 'if' code must again be a statement:
  if (stack[si]->type != ND::Statement) return 0;
  PC stat_a_first = si;
  --si;
  // again, skip zero or more statements.
  while (stack[si]->type == ND::Statement) --si;
  PC stat_a_last = si+1;
  // The initial 'if' statement must be a conditional jump to label a:
  if ((stack[si]->type != ND::BranchFalseFwd) || ((PC)stack[si]->arg != label_a)) return 0;
  --si;
  // And we need an expression to feed the conditional jump
  if (stack[si]->type != ND::Expr) return 0;
  PC condition_pc = si;
  PC pcf = stack[si]->pc_first;

  // -- Create a node on the stack, using all the information from above:
  auto node = std::make_shared<NodeControlFlow>(ND::Statement, pcf, pcl, 0, "if ... then ... else ...; ", 0);
  node->set_condition(stack[condition_pc]);
  for (PC i=stat_a_last; i<=stat_a_first; ++i)
    node->add_statement_a(stack[i]);
  for (PC i=stat_b_last; i<=stat_b_first; ++i)
    node->add_statement_b(stack[i]);
  // Replace nodes on the stack that make up the new if...then...else... node
  while (stack.size()>condition_pc) stack.pop_back();
  stack.push_back(node);
  return 1;
}

/**
 Test if the nodes on the stack form an 'if...then...else...' flow and return a value.

 An 'if' statement can stand by itself as a flow control statement, or it
 can be used as an expression, similar to the C++ "?:" operator.

 If it is used as an expressions, the entire 'if...then...else...' will
 resolve into a single value on the stack and the 'else' branch is always
 implemented. If none was defined, the bytecode generates one that pushes 'nil'
 onto the stack.
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

 This method is called when a label is encountered, but can only be successful
 if called from label b.
 */
int Decompiler::CheckIfThenElseExpr()
{
  // -- Check if this is an 'if...then...else...expr':
  size_t si = stack.size()-1; // current stack index
  PC label_b = state.pc;
  // The first entry must be an expression.
  if (stack[si]->type != ND::Expr) return 0;
  PC pcl = stack[si]->pc_last;
  PC stat_b_first = si;
  --si;
  // Skip zero or more statements.
  while (stack[si]->type == ND::Statement) --si;
  // So this is where label a is in the bytecode array.
  PC label_a = stack[si+1]->pc_first;
  PC stat_b_last = si+1;
  // The next command must be a forward jump to the current pc:
  if ((stack[si]->type != ND::BranchFwd) || ((PC)stack[si]->arg != label_b)) return 0;
  --si;
  // The first half of the 'if' code must generate a value:
  if (stack[si]->type != ND::Expr) return 0;
  PC stat_a_first = si;
  --si;
  // again, skip zero or more statements.
  while (stack[si]->type == ND::Statement) --si;
  PC stat_a_last = si+1;
  // The initial 'if' statement must be a conditional jump to label a:
  if ((stack[si]->type != ND::BranchFalseFwd) || ((PC)stack[si]->arg != label_a)) return 0;
  --si;
  // And we need an expression to feed the conditional jump
  if (stack[si]->type != ND::Expr) return 0;
  PC condition_pc = si;
  PC pcf = stack[si]->pc_first;

  // -- Create a node on the stack, using all the information from above:
  auto node = std::make_shared<NodeControlFlow>(ND::Expr, pcf, pcl, 0, "if ... then ... else ...; ", 0);
  node->set_condition(stack[condition_pc]);
  for (PC i=stat_a_last; i<=stat_a_first; ++i)
    node->add_statement_a(stack[i]);
  // TODO: the last statement and the expr in the block may correlate, e.g.: "lit_4:=4; lit_4;"
  // TODO: the second statement here should not appear in the source code!
  for (PC i=stat_b_last; i<=stat_b_first; ++i)
    node->add_statement_b(stack[i]);
  // Replace nodes on the stack that make up the new if...then...else... node
  while (stack.size()>condition_pc) stack.pop_back();
  stack.push_back(node);
  return 1;
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
 TODO: Like with 'if', the code generated differs if the result of the 'while' loop is used.
 'while' pushes 'NIL' on the stack if it is used as an expression. If 'break'
 is called, the argument to 'break' is pushed.
 */
int Decompiler::CheckWhileDo() {
  // This is called with branch_true_back pending before added to the stack
  size_t si = stack.size()-1; // current stack index
  // The first entry must be an expression.
  if (stack[si]->type != ND::Expr) return 0;
  PC condition_pc = si;
  PC label_a = stack[si]->pc_first;
  --si;
  // Check for one expression
  // TODO: example code has nonsensical "find_var;pop;" here.
  if (stack[si]->type != ND::Statement) return 0;
  PC stat_first = si;
  --si;
  // Check for more expressions
  while (stack[si]->type == ND::Statement) --si;
  PC stat_last = si+1;
  // Now check if our current branch_true_back jumps to label b
  PC label_b = stack[si+1]->pc_first;
  if ((PC)instructions[state.pc].arg != label_b) return 0;
  // Finally, check for the branch forward to label a:
  if (stack[si]->type != ND::BranchFwd) return 0;
  if ((PC)stack[si]->arg != label_a) return 0;
  PC crop_pc = si;
  PC pcf = stack[si]->pc_first;

  // -- Create a node on the stack, using all the information from above:
  auto node = std::make_shared<NodeControlFlow>(ND::Statement, pcf, state.pc+1, 0, "while ... do ...; ", 1);
  node->set_condition(stack[condition_pc]);
  for (PC i=stat_last; i<=stat_first; ++i)
    node->add_statement_a(stack[i]);
  while (stack.size()>crop_pc) stack.pop_back();
  stack.push_back(node);
  return 1;
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
int Decompiler::CheckWhileBreakDo() {
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
int Decompiler::CheckRepeatUntil() {
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
int Decompiler::CheckRepeatBreakUntil() {
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
int Decompiler::CheckEndlessLoop() {
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
int Decompiler::CheckForLoop() {
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
int Decompiler::CheckForeach() {
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
int Decompiler::CheckTryOnExpression() {
  (void)state;
  return 0;
}

int Decompiler::DoLabel() {
  // Repeat finding actions related to this label until all found.
  int ret = 0;
  for (;;) {
    if (CheckLogicAnd() == 1) { ret = 1; continue; }
    if (CheckLogicOr() == 1) { ret = 1; continue; }
    if (CheckIfThenElseExpr() == 1) { ret = 1; continue; }
    if (CheckIfThenElse() == 1) { ret = 1; continue; }
    if (CheckIfThen() == 1) { ret = 1; continue; }
    // check "while ... break ... do ... "
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
  return ret;
}

/**
 Handle a `branch` bytecode instruction.

 A branch changes the the program counter to a new address. Branches can jump
 forward or back. Unconditional branches are always used in combination with
 conditional branches or within exception handling. A forward branch is used
 in 'try', 'onException', 'else', 'repeat', and 'break'. They are always
 followed by a label, so we just mark them with a node on the stack.

 A backward branch is created by the 'loop' statement. The loop itself will
 have one or more 'break' statements.
 */
int Decompiler::DoBranch() {
  (void)state;
  if ((PC)state.bytecode.arg > state.pc)
    stack.push_back( std::make_shared<Node>(ND::BranchFwd, state.pc, state.pc, state.bytecode.arg, "ND::BranchFwd") );
  else
    stack.push_back( std::make_shared<Node>(ND::BranchBack, state.pc, state.pc, state.bytecode.arg, "ND::BranchBack") );
  return 1;
}

int Decompiler::DoBranchIfTrue() {
  int ret = 0;
  if ((PC)state.bytecode.arg > state.pc) {
    stack.push_back( std::make_shared<Node>(ND::BranchTrueFwd, state.pc, state.pc, state.bytecode.arg, "ND::BranchTrueFwd") );
    ret = 1;
  } else {
    for (;;) {
      if (CheckWhileDo() == 1) { ret = 1; continue; }
      break;
    }
    if (ret == 0) {
      stack.push_back( std::make_shared<Node>(ND::BranchTrueBack, state.pc, state.pc, state.bytecode.arg, "ND::BranchTrueBack") );
    }
  }
  return ret;
}

int Decompiler::DoBranchIfFalse() {
  (void)state;
  if ((PC)state.bytecode.arg > state.pc)
    stack.push_back( std::make_shared<Node>(ND::BranchFalseFwd, state.pc, state.pc, state.bytecode.arg, "ND::BranchFalseFwd") );
  else
    stack.push_back( std::make_shared<Node>(ND::BranchFalseBack, state.pc, state.pc, state.bytecode.arg, "ND::BranchFalseBack") );
  return 1;
}

/**
 \note check if 'pop' cancel out an expr
 */
int Decompiler::DoPop() {
  auto s = stack.back();
  if (s->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": 'pop': expected expression on stack!\n" << std::endl;
    return -1;
  }
  s->type = ND::Statement;
  return 1;
}

/**
 \return the starting PC of the top arg, or -1 if an error occurred.
 */
PC Decompiler::DoArgList(std::string &args, int num_args) {
  PC ret = kInvalidPC;
  for (int i=0; i<num_args; ++i) {
    auto arg = stack.back();
    if (arg->type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": expected argument " << i << " expr on stack!\n" << std::endl;
      return kInvalidPC;
    }
    if (i==0)
      args = arg->ToString();
    else
      args = arg->ToString() + ", " + args;
    ret = arg->pc_first;
    stack.pop_back();
  }
  return ret;
}

int Decompiler::DoCallOrInvoke(const std::string &call, int which) {
  auto s = stack.back();
  if (s->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << call << "': expected function name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  std::string args { };
  PC first_pc = DoArgList(args, state.bytecode.arg);
  if (first_pc == kInvalidPC)
    return -1;
  if (which == 0) // call
    stack.push_back( std::make_shared<Node>(ND::Expr, first_pc, state.pc, 0, s->ToString() + "(" + args + ")") );
  else // invoke
    stack.push_back( std::make_shared<Node>(ND::Expr, first_pc, state.pc, 0, "call " + s->ToString() + " with (" + args + ")") );
  return 1;
}

int Decompiler::DoCall() { return DoCallOrInvoke("call", 0); }
int Decompiler::DoInvoke() { return DoCallOrInvoke("invoke", 1); }

int Decompiler::DoSend(const std::string &op, const std::string &call, bool is_resend) {
  auto name = stack.back();
  if (name->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": '" << call << "': expected message name on stack!\n" << std::endl;
    return -1;
  }
  stack.pop_back();
  auto rcvr = std::make_shared<Node>();
  if (!is_resend) {
    rcvr = stack.back();
    if (rcvr->type != ND::Expr) {
      std::cout << "ERROR: " << state.pc << ": '" << call << "': expected receiver on stack!\n" << std::endl;
      return -1;
    }
    stack.pop_back();
  }
  std::string args { };
  PC pcf = DoArgList(args, state.bytecode.arg);
  if (pcf == kInvalidPC)
    return -1;
  if (is_resend)
    stack.push_back( std::make_shared<Node>(ND::Expr, pcf, state.pc, 0, "inherited" + op + name->ToString() + "(" + args + ")") );
  else
    stack.push_back( std::make_shared<Node>(ND::Expr, pcf, state.pc, 0, rcvr->ToString() + op + name->ToString() + "(" + args + ")") );
  return 1;
}

int Decompiler::DoSend() { return DoSend(":", "send", false); }
int Decompiler::DoSendIfDefined() { return DoSend(":?", "send_if_defined", false); }
int Decompiler::DoResend() { return DoSend(":", "resend", true); }
int Decompiler::DoResendIfDefined() { return DoSend(":?", "resend_if_defined", true); }

int Decompiler::DoPush() {
#if 0
  stack.push_back( std::make_shared<Node>(ND::Expr, state.pc, state.pc, 0, "lit_" + std::to_string(state.bytecode.arg)) );
#else
  // TODO: check if literal is NIL or TRUE and set 'info' accordingly
  Ref r = GetArraySlot(ns_literals, state.bytecode.arg);
  stack.push_back( std::make_shared<NodeImmediate>(ND::Expr, state.pc, state.pc, 0, "lit_" + std::to_string(state.bytecode.arg), 0, r) );
#endif
  return 1;
}

int Decompiler::DoFindVar() {
  stack.push_back( std::make_shared<Node>(ND::Expr, state.pc, state.pc, 0, "lit_" + std::to_string(state.bytecode.arg)) );
  return 1;
}

int Decompiler::DoPushSelf() {
  (void)state;
  stack.push_back( std::make_shared<Node>(ND::Expr, state.pc, state.pc, 0, "self") );
  return 1;
}

/**
 Take an expression from the stack and put it into the slot named literal[arg].
 This does not leave a value on the stack, so it's a statement, not an expression.
 \param[inout] state the state of the decompiler
 \return -1 for end of func, <-1 for error, or the number of bytes consumed
 */
int Decompiler::DoFindAndSetVar() {
  auto node = stack.back();
  if (node->type != ND::Expr) {
    std::cout << "ERROR: " << state.pc << ": 'find_and_set_var': expected expression on stack!\n" << std::endl;
    return -1;
  }
  PC pcf = node->pc_first;
  stack.pop_back();
  stack.push_back( std::make_shared<Node>(ND::Statement, pcf, state.pc, 0, "lit_" + std::to_string(state.bytecode.arg) + " := " + node->ToString()) );
  return 1;
}

static void print_altcode(int ip, Bytecode &ac) {
  if (ac.references)
    std::cout << std::setw(4) << ip << ": label[refs=" << ac.references << "]:" << std::endl;
  std::cout << std::setw(4) << ip << ": ";
  switch (ac.bc) {
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
      std::cout << "ERROR: unknown altcode: a=" << (int)ac.bc << ", b=" << ac.arg << "." << std::endl; break;
  }
}

void dyn::lang::print_bytecode(std::vector<Bytecode> &func) {
  int i = 0;
  for (auto &bc: func) {
    print_altcode(i++, bc);
  }
}

bool Decompiler::decode() {
  int num_altcodes_handled = 0;
  size_t i = 0;
  for ( ; i<instructions.size(); ) {
    if (instructions[i].references) {
      state.pc = i;
      state.bytecode = instructions[i];
      DoLabel();
    }
    state.pc = i;
    state.bytecode = instructions[i];
    num_altcodes_handled = Do(state.bytecode.bc);
    if (num_altcodes_handled <= 0) break;
    i += num_altcodes_handled;
  }
  if (num_altcodes_handled < -1) {
    std::cout << "ERROR: can't decode bytecode at pc " << i << "." << std::endl;
    return false;
  }
  if (num_altcodes_handled == 0) {
    std::cout << "ERROR: unknown bytecode found." << std::endl;
    return false;
  }
  return true;
}

Decompiler::Decompiler(RefArg func)
: ns_function(func)
{
  ns_literals = GetFrameSlot(func, Sym("literals"));
}

Ref dyn::lang::decompile(RefArg func)
{
  if (!IsFrame(func)) return RefNIL;
  Decompiler decompiler(func);
  decompiler.instructions = transcode_from_ns(func);
  if (decompiler.instructions.empty())
    return RefNIL;
  printf("--- expanded byte code\n");
  print_bytecode(decompiler.instructions);
  decompiler.stack.push_back( std::make_shared<Node>(ND::EndOfStack, kInvalidPC, kInvalidPC, 0, "... stack bottom ...") );
  printf("--- decode\n");
  decompiler.decode();
  printf("--- remaining stack:\n");
  PrintStack(decompiler.stack);

//  std::cout << stack.back() << std::endl; stack.pop_back();

  return RefNIL;
}
