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

 #include <dyn/lang/ast.h>
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
 