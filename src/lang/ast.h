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

#ifndef DYN_LANG_AST_H
#define DYN_LANG_AST_H

#include <dyn/lang/decompile.h>
#include <dyn/ref.h>

namespace dyn {

namespace lang {

enum class ND {
  EndOfStack, Unknown, Error, Expr, Statement, Condition,
  BranchFwd, BranchBack, BranchTrueFwd, BranchTrueBack,
  BranchFalseFwd, BranchFalseBack
};

class Node {
public:
  ND type { ND::Unknown };
  PC pc_first { kInvalidPC };
  PC pc_last { kInvalidPC };
  int arg { 0 };
  std::string text { };
  int info { 0 };
  Node() { }
  Node(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info=0);
  virtual ~Node();
  virtual std::string ToString();
};

class NodeImmediate : public Node {
  dyn::Ref imm { RefNIL };
public:
  NodeImmediate(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info, RefArg a_ref);
  std::string ToString() override;
};

class NodeControlFlow : public Node {
  std::shared_ptr<Node> condition_ { };
  std::vector<std::shared_ptr<Node>> statements_a_ { };
  std::vector<std::shared_ptr<Node>> statements_b_ { };
public:
  NodeControlFlow(ND a_type, PC a_pc_first, PC a_pc_last, int a_arg, const std::string &a_text, int a_info);
  void set_condition(std::shared_ptr<Node> condition);
  void add_statement_a(std::shared_ptr<Node> stmt);
  void add_statement_b(std::shared_ptr<Node> stmt);
  std::string ToString() override;
};

void PrintStack(const std::vector<std::shared_ptr<Node>> &stack);

} // namespce lang

} // namespace dn

#endif // DYN_LANG_AST_H




