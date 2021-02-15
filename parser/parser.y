%{
/*
 * Description: Parser of the Chaos Programming Language's source
 *
 * Copyright (c) 2019-2020 Chaos Language Development Authority <info@chaos-lang.org>
 *
 * License: GNU General Public License v3.0
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>
 *
 * Authors: M. Mert Yildiran <me@mertyildiran.com>
 *          Melih Sahin <melihsahin24@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>

#include "parser/parser.h"

extern int yylex();
extern int yyparse();
extern int yylex_destroy();
extern FILE* yyin;

extern int yylineno;
extern char *yytext;

#ifndef CHAOS_COMPILER
extern bool is_interactive;
#endif

bool inject_mode = false;

extern char *main_interpreted_module;
extern unsigned long long loops_inside_function_counter;
extern bool is_complex_parsing;
%}

%union {
    bool bval;
    long long ival;
    long double fval;
    char *sval;
    unsigned long long lluval;
    ASTNode* ast_node_type;
}

%token START_PROGRAM START_PREPARSE START_JSON_PARSE
%token<bval> T_TRUE T_FALSE
%token<ival> T_INT T_TIMES_DO_INT
%token<fval> T_FLOAT
%token<sval> T_STRING T_VAR
%token<lluval> T_UNSIGNED_LONG_LONG_INT
%token T_PLUS T_MINUS T_MULTIPLY T_DIVIDE T_LEFT T_RIGHT T_EQUAL
%token T_LEFT_BRACKET T_RIGHT_BRACKET T_LEFT_CURLY_BRACKET T_RIGHT_CURLY_BRACKET T_COMMA T_DOT T_COLON
%token T_NEWLINE T_QUIT
%token T_PRINT T_ECHO T_PRETTY
%token T_VAR_BOOL T_VAR_NUMBER T_VAR_STRING T_VAR_LIST T_VAR_DICT T_VAR_ANY T_NULL
%token T_DEL T_RETURN T_VOID T_DEFAULT T_BREAK T_CONTINUE
%token T_SYMBOL_TABLE T_FUNCTION_TABLE
%token T_TIMES_DO T_FOREACH T_AS T_END T_FUNCTION T_IMPORT T_FROM T_BACKSLASH T_INFINITE
%token T_REL_EQUAL T_REL_NOT_EQUAL T_REL_GREAT T_REL_SMALL T_REL_GREAT_EQUAL T_REL_SMALL_EQUAL
%token T_LOGIC_AND T_LOGIC_OR T_LOGIC_NOT
%token T_BITWISE_AND T_BITWISE_OR T_BITWISE_XOR T_BITWISE_NOT T_BITWISE_LEFT_SHIFT T_BITWISE_RIGHT_SHIFT
%token T_INCREMENT T_DECREMENT
%left T_PLUS T_MINUS T_VAR
%left T_MULTIPLY T_DIVIDE

%destructor {
    free($$);
} <sval>

%start meta_start

%%

meta_start:
    | START_PROGRAM parser              { }
;

parser:
    | parser line   { }
;

line: {}

%%

#ifndef CHAOS_COMPILER
int main(int argc, char** argv) {
    initParser(argc, argv);
}
#endif
