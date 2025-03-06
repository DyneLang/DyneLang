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

// Managinging the Abstract Syntax Tree of the decompiler.

#include "ast.h"
#include <dyn/objects.h>
#include <dyn/io/print.h>

#include <stdio.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace dyn;

using namespace dyn::lang;

dyn::lang::Node::Node(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info)
: type(a_type), pc_first(a_pc_first), pc_last(a_pc_last), arg(a_arg), text(a_text), info(a_info)
{
}

dyn::lang::Node::~Node()
{
}

std::string dyn::lang::Node::ToString() {
  return text;
}

int dyn::lang::Node::print(dyn::io::PrintState &ps)
{
  fprintf(ps.out_, "%s", ToString().c_str());
  return 0;
}


dyn::lang::NodeImmediate::NodeImmediate(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info, RefArg a_ref)
: Node(a_type, a_pc_first, a_pc_last, a_arg, a_text, a_info), imm(a_ref)
{
}

std::string dyn::lang::NodeImmediate::ToString()
{
  return imm.ToString();
}

dyn::lang::NodeControlFlow::NodeControlFlow(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info)
: Node(a_type, a_pc_first, a_pc_last, a_arg, a_text, a_info)
{
}

void dyn::lang::NodeControlFlow::set_condition(std::shared_ptr<Node> condition)
{
  condition_ = condition;
}

void dyn::lang::NodeControlFlow::add_statement_a(std::shared_ptr<Node> stmt)
{
  statements_a_.push_back(stmt);
}

void dyn::lang::NodeControlFlow::add_statement_b(std::shared_ptr<Node> stmt)
{
  statements_b_.push_back(stmt);
}

std::string dyn::lang::NodeControlFlow::ToString()
{
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

void dyn::lang::PrintStack(const std::vector<std::shared_ptr<Node>> &stack) {
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

/*
 if         if          while       while       repeat      repeat      loop        for       foreach     try
 then       then        do          break       until       break       break       to        in          onexception
            else                    do                      until                   by        do          onexception
                                                                                    do

 expr       expr        b_fwd a     b_fwd a     a:          a:          a:          set_l 1   new_iter    new_handler a, b
 b_f_fwd a  b_f_fwd a   b:          b:          stmts       stmts       stmts       set_l 2   b_fwd a     stmts
 stmts      stmts       stmts       stmts       expr        expr        expr        set_l 3   b:          pop_handlers
 a:         b_fwd b     a:          expr        b_f_bck a   b_fwd b     b_fwd b     b_fwd a   stmts       b_fwd c
            a:          expr        b_fwd c                 stmts       stmts       b:        iter_next   a:
            stmts       b_t_bck b   a:                      expr        b_back a    stmts     a:          stmts
            b:                      expr                    b_f_bck a   b:          get_l     iter_done   b_fwd d
                                    b_t_bck b               expr                    incr_l    b_f_bwd a   stmts
                                    expr                    b:                      a:                    b_fwd d
                                    c:                                              get_l                 d:
                                    b:                                              b_loop b              pop_handlers
                                                                                                          c:

*/
