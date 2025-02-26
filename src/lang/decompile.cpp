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
 30x freq-func:
 0 add |+|                      num1 num2 -- result
 1 subtract |-|                 num1 num2 -- result
 2 aref                         object index -- element
 3 set-aref                     object index element -- element
 4 equals |=|                   obj1 obj2 -- result
 5 not |not|                    value -- result
 6 not-equals |<>|              obj1 obj2 -- result
 7 multiply |*|                 num1 num2 -- result
 8 divide |/|                   num1 num2 -- result
 9 div |div|                    int1 int2 -- result
 10 less-than |<|               obj1 obj2 -- result
 11 greater-than |>|            obj1 obj2 -- result
 12 greater-or-equal |>=|       obj1 obj2 -- result
 13 less-or-equal |<=|          obj1 obj2 -- result
 14 bit-and BAnd                int1 int2 -- result
 15 bit-or BOr                  int1 int2 -- result
 16 bit-not BNot                int -- result
 17 new-iterator                object deeply -- iterator
 18 length Length               object -- length
 19 clone Clone                 object -- clone
 20 set-class SetClass          object class -- object
 21 add-array-slot              array object -- object
 22 stringer Stringer           array -- string
 23 has-path none               object pathExpr -- result
 24 class-of ClassOf            object -- class
 31x new-handlers               sym1 pc1 sym2 pc2 ... symN pcN --
 */

/*
 NewtonScript Grammar:

 Character : $ { nonEscapeCharacter |\ { \ | n | t |hexDigit hexDigit
             | u hexDigit hexDigit hexDigit hexDigit } }
 nonEscapeCharacter : 32-126, not '\'
 hexDigit : 0-9 a-f A-F
 Boolean: true | nil
 Integer : [ - ] {[ digit ]+ | 0x [hexDigit ]+}
 Real : [-] [digit ]+.[ digit ]* [ { e | E } [ - ] [ digit ]+
 Symbol : { { alpha | _ } [ { alpha | digit | _ } ]*
          | '|' [ { symbolChar| \ { '|' | \ } ]* '|'}
 symbolChar : 32-126, not '|', not '\'
 String : " [ { stringChar | escSeq } ]* [ truncEscape ] ] "
 stringChar : 32-127, not '"', not '\'
 escSeq : \" | \n | \t | \\ | \u[hexDigit hexDigit hexDigit hexDigit]*\u
 Array : ‘[’ [ symbol : ] [ object [ , object]* [ , ] ] ‘]’
 Array Accessor : arrayExpression ‘[’ indexEpression ‘]’
 Frame : ‘{’ [ symbol : value [ , symbol : value ]* [ , ] ] ‘}’
 Frame Accessor : frameExpr. {symbol | ( pathExpr )}
 Path Expression : symbol[.symbol]+
 Expressions :
 Local :local [typeIdentifier]varSymbol1 [:= expression ]
        [, varSymbol2 [:= expression] ]*
 Constant : constant constSymbol1 := expression [, constSymbol2 := expression ]*
 Quoted Constant : 'object
 Assignment Operator : lvalue:= expression
 Arithmetic Operators : { + | - | * | / | div | mod | << | >> }
 Equality and Relational Operators : { = | <> | < | > | <= | >=}
 Boolean Operators : { and | or }
 Unary Operators : { - | not }
 String Operators : string1 { & | && } string2
 Exists : lValue exists
 Compound Expressions : begin expression1; expressionN[;] end
 If…Then…Else : if testExpression then expression [;] [else alternateExpression]
 For : for counter := initialValue to finalValue [by incrValue] do expression
 Foreach : foreach [slot,] value [deeply] in {frame | array}
           {collect | do} expression
 loop : loop expression
 while : while condition do expression
 Repeat : repeat expression1; expressionN[;] until condition
 Break : break [expression ];
 try : try expression1; expressionN; [onexception exceptionSymbol do statement]*
 try : try begin expression1; expressionN; [onexception exceptionSymbol do statement]* end
 throw : Throw('|evt.ex.foo|, -12345);
 rethrow : Rethrow()
 Function Constructor : func [native](paramList) expression
 Return : return [returnValue]
 Message-Send : [{inherited|frame}] {:|:?} message(paramList)
 Call With : call function with (paramList)
 Global Function :{ global|func } functionName (paramList) expression
 Global Function Invocation : functionName (paramList)
 self : self  _proto  _parent  class
 */


typedef struct {
  int a; int b; size_t ip;
} AC;

static std::vector<int> label_list { };
static std::vector<AC> altcode { };
//static size_t altcode_ip = 0;

std::string src { };

enum {
  AC_EOF = 0,
  AC_POP,
  AC_RETURN,
  AC_PUSH_CONST,
  AC_PLUS,
  AC_EQUALS,
  AC_BRANCH_F,
  AC_LABEL
};


static void recode(dyn::RefArg func)
{
  (void)func;
#if 0
  dyn::Ref inst_ref = dyn::GetFrameSlot(func, dyn::Sym("instructions"));
  if (!inst_ref.IsBinary()) {
    std::cout << "ERROR: not 'instructions array in function." << std::endl;
    return;
  }
  dyn::BinaryObject *inst_obj = static_cast<dyn::BinaryObject*>(inst_ref.GetObject());
  size_t n_inst = inst_obj->size();
  uint8_t *inst = static_cast<uint8_t*>(inst_obj->Data());
#elif 0
  //  begin end
  //  0: 22       push-constant nil
  //  1: 02       return
  uint8_t inst[] = { 0x22, 0x02, };
  size_t n_inst = sizeof(inst);
#elif 0
  //  0: 24       push-constant 1
  //  1: 24       push-constant 1
  //  2: C0       freq-func 0 [+/2]
  //  3: 02       return
  //  func()
  //  begin
  //  1 + 1;
  //  end
  uint8_t inst[] = { 0x24, 0x24, 0xc0, 0x02 };
  size_t n_inst = sizeof(inst);
#elif 1
//  if 1+2=3 then return test(2) else return 3
//  0: 24       push-constant 1
//  1: 27 0008  push-constant 2
//  4: C0       freq-func 0 [+/2]
//  5: 27 000C  push-constant 3
//  8: C4       freq-func 4 [=/2]
//  9: 6F 0015  branch-f 21
//  12: 27 0008  push-constant 2
//  15: 18       push 'test
//  16: 29       call 1
//  17: 02       return
//  18: 5F 0019  branch 25
//  21: 27 000C  push-constant 3
//  24: 02       return
//  25: 02       return
  uint8_t inst[] = {
    0x24, 0x27, 0x00, 0x08, 0xC0, 0x27, 0x00, 0x0C, 0xC4, 0x6F, 0x00, 0x15,
    0x27, 0x00, 0x08, 0x18, 0x29, 0x02, 0x5F, 0x00, 0x19, 0x27, 0x00, 0x0C,
    0x02, 0x02 };
  size_t n_inst = sizeof(inst);
#endif

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
      altcode.push_back({AC_LABEL, (int)i, i});
    uint8_t cmd = inst[i];
    uint8_t a = (cmd & 0xf8) >> 3;
    uint16_t b = (cmd & 0x07);
    if (b==7) {
      b = inst[i+1]<<8 | inst[i+2]; i += 2;
    }
    switch (a) {
      case 0:
        switch (b) {
          case 0: altcode.push_back({AC_POP, 0, i}); break;
            //          case 1: f << "dup"; break;
          case 2: altcode.push_back({AC_RETURN, 0, i}); break;
            //          case 3: f << "push_self"; break;
            //          case 4: f << "set_lex_scope"; break;
            //          case 5: f << "iter_next"; break;
            //          case 6: f << "iter_done"; break;
            //          case 7: f << "pop_handlers"; break;
          default:
            std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)i << "." << std::endl;
            break;
        }
        break;
        //      case 3: f << "push " << b; break;
      case 4: altcode.push_back({AC_PUSH_CONST, (int16_t)b, i}); break;
        //      case 5: f << "call " << b; break;
        //      case 6: f << "invoke " << b; break;
        //      case 7: f << "send " << b; break;
        //      case 8: f << "send_if_defined " << b; break;
        //      case 9: f << "resend " << b; break;
        //      case 10: f << "resend_if_defined " << b; break;
        //      case 11: f << "branch " << (int16_t)b; break;
        //      case 12: f << "branch_if_true " << (int16_t)b; break;
        //      case 13: f << "branch_if_false " << (int16_t)b; break;
      case 13: altcode.push_back({AC_BRANCH_F, (int16_t)b, i}); break;
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
          case 0: altcode.push_back({AC_PLUS, 0, i}); break;
            //          case 1: f << "subtract"; break;
            //          case 2: f << "aref"; break;
            //          case 3: f << "set_aref"; break;
          case 4: altcode.push_back({AC_EQUALS, 0, i}); break;
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
        //      case 25: f << "new_handlers " << b; break;
      default:
        std::cout << "WARNING: unknown byte code a:" << (int)a << ", b:" << (int)b << ", ip:" << (int)i << "." << std::endl;
        break;
    }
  }
  altcode.push_back({AC_EOF, 0, n_inst});
}


std::vector<std::string> stack;

bool decode(int ip) {
  AC &ac = altcode[ip];
  switch (ac.a) {
    case AC_EOF: return true;
    case AC_PUSH_CONST:
      stack.push_back("ref" + std::to_string(ac.b)); break;
    case AC_PLUS: {
      std::string e1 = stack.back(); stack.pop_back();
      std::string e2 = stack.back(); stack.pop_back();
      stack.push_back("(" + e2 + "+" + e1 + ")"); break; }
    case AC_RETURN: {
      std::string e1 = stack.back(); stack.pop_back();
      stack.push_back("return " + e1); break; }
    case AC_EQUALS: {
      std::string e1 = stack.back(); stack.pop_back();
      std::string e2 = stack.back(); stack.pop_back();
      stack.push_back("(" + e2 + "=" + e1 + ")"); break; }
    case AC_BRANCH_F: {
      std::string e1 = stack.back(); stack.pop_back();
      stack.push_back("if not " + e1); break; }
    default:
      stack.push_back("ERROR<" + std::to_string(ac.a) + ", " + std::to_string(ac.b) + ", " + std::to_string(ac.ip) + ">"); break;
  }
  return decode(ip+1);
}


Ref dyn::lang::decompile(RefArg func)
{
  recode(func);

  stack.push_back("<<stack underflow>>");
  decode(0);
  while (stack.size()) {
    std::cout << stack.back() << std::endl;
    stack.pop_back();
  }

//  std::cout << stack.back() << std::endl; stack.pop_back();

  return RefNIL;
}
