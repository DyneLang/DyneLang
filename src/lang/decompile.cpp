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

#include "bytecode.h"

#include <stdio.h>


extern int yyparse();


int data[] = {
//  T_FLOAT,
//  T_NEWLINE,
//  T_NEWLINE,
//  0,

  T_INT,
  T_PLUS,
  T_INT,
  T_NEWLINE,
  0
};
int dix = 0;


int yylex() {
  if (data[dix]) {
//    yylval.fval = 3.1415;
    yylval.ival = 16;
    return data[dix++];
  }
  return 0;
}

void yyerror(const char* s) {
  fprintf(stderr, "Parse error: %s\n", s);
}


using namespace dyn;


Ref dyn::lang::decompile(RefArg func)
{
  (void)func;
  yydebug = 1;

  // we can put this in a loop while we still have data available
  yyparse();

  return RefNIL;
}

