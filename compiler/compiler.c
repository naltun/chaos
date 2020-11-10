/*
 * Description: Compiler module of the Chaos Programming Language's source
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
 */

#include "compiler.h"

extern bool disable_complex_mode;

struct stat dir_stat = {0};

unsigned short indent_length = 4;
unsigned long long compiler_loop_counter = 0;
unsigned long long compiler_function_counter = 0;
unsigned long long extension_counter = 0;

char *type_strings[] = {
    "K_BOOL",
    "K_NUMBER",
    "K_STRING",
    "K_ANY",
    "K_LIST",
    "K_DICT",
    "K_VOID"
};

char* relational_operators[] = {"==", "!=", ">", "<", ">=", "<="};
char* relational_operators_size[] = {">", "<", ">=", "<="};
char* logical_operators[] = {"&&", "||", "!"};
char* bitwise_operators[] = {"&", "|", "^", "~", "<<", ">>"};
char* unary_operators[] = {"++", "--"};

void compile(char *module, enum Phase phase_arg, char *bin_file, char *extra_flags, bool keep) {
    printf("Starting compiling...\n");
    char *module_orig = malloc(strlen(module) + 1);
    strcpy(module_orig, module);
    compiler_escape_module(module);
    ASTNode* ast_node = ast_root_node;

    if (stat(__KAOS_BUILD_DIRECTORY__, &dir_stat) == -1) {
        printf("Creating %s directory...\n", __KAOS_BUILD_DIRECTORY__);
        #if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
            _mkdir(__KAOS_BUILD_DIRECTORY__);
        #else
            mkdir(__KAOS_BUILD_DIRECTORY__, 0700);
        #endif
    }

    char c_file_path[PATH_MAX];
    char h_file_path[PATH_MAX];
    if (bin_file != NULL) {
        sprintf(c_file_path, "%s%s%s.c", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__, bin_file);
        sprintf(h_file_path, "%s%s%s.h", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__, bin_file);
    } else {
        sprintf(c_file_path, "%s%smain.c", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
        sprintf(h_file_path, "%s%smain.h", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
    }

    printf("Compiling Chaos code into %s\n", c_file_path);

    FILE *c_fp = fopen(c_file_path, "w");
    if (c_fp == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    FILE *h_fp = fopen(h_file_path, "w");
    if (h_fp == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    if (bin_file != NULL) {
        char *bin_file_upper = malloc(1 + strlen(bin_file));
        strcpy(bin_file_upper, bin_file);
        string_uppercase(bin_file_upper);
        fprintf(h_fp, "#ifndef %s_H\n", bin_file_upper);
        fprintf(h_fp, "#define %s_H\n\n", bin_file_upper);
        free(bin_file_upper);
    } else {
        fprintf(h_fp, "#ifndef MAIN_H\n");
        fprintf(h_fp, "#define MAIN_H\n\n");
    }

    if (bin_file != NULL) {
        fprintf(c_fp, "#include \"%s.h\"\n\n", bin_file);
    } else {
        fprintf(c_fp, "#include \"main.h\"\n\n");
    }

    unsigned short indent = indent_length;

    const char *c_file_base =
        "extern bool disable_complex_mode;\n\n"
        "bool is_interactive = false;\n"
        "unsigned long long nested_loop_counter = 0;\n\n";

    const char *h_file_base =
        "#include <stdio.h>\n"
        "#include <stdbool.h>\n\n"
        "#include \"interpreter/function.h\"\n"
        "#include \"interpreter/symbol.h\"\n\n"
        "unsigned long long nested_loop_counter;\n"
        "jmp_buf LoopBreak;\n"
        "jmp_buf LoopContinue;\n\n";

    fprintf(c_fp, "%s", c_file_base);
    fprintf(h_fp, "%s", h_file_base);

    register_functions(ast_node, module_orig);
    transpile_functions(ast_node, module, c_fp, indent, h_fp);
    free_transpiled_functions();
    free_transpiled_decisions();

    fprintf(c_fp, "int main(int argc, char** argv) {\n");

    char *fixed_module_orig = fix_bs(module_orig);
    free(module_orig);
    fprintf(
        c_fp,
        "%*cprogram_file_path = malloc(strlen(\"%s\") + 1);\n"
        "%*cstrcpy(program_file_path, \"%s\");\n",
        indent,
        ' ',
        fixed_module_orig,
        indent,
        ' ',
        fixed_module_orig
    );
    free(fixed_module_orig);

    fprintf(
        c_fp,
        "char *argv0 = malloc(1 + strlen(argv[0]));\n"
        "strcpy(argv0, argv[0]);\n"
    );

    fprintf(c_fp, "%*cinitMainFunction();\n", indent, ' ');
    fprintf(c_fp, "%*cSymbol* symbol;\n", indent, ' ');
    fprintf(c_fp, "%*clong long exit_code;\n", indent, ' ');

    fprintf(c_fp, "phase = PREPARSE;\n");
    compiler_register_functions(ast_node, module, c_fp, indent);
    fprintf(c_fp, "phase = PROGRAM;\n");
    transpile_node(ast_node, module, c_fp, indent);

    fprintf(
        c_fp,
        "free(argv0);\n"
        "freeEverything(argv0);\n"
        "return 0;\n"
    );

    fprintf(c_fp, "}\n");
    fprintf(h_fp, "\n#endif\n");

    fclose(c_fp);
    fclose(h_fp);

    printf("Compiling the C code into machine code...\n");

    char bin_file_path[PATH_MAX];
    if (bin_file != NULL) {
        #if defined(__clang__) && (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            sprintf(bin_file_path, "%s%s%s.exe", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__, bin_file);
        #else
            sprintf(bin_file_path, "%s%s%s", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__, bin_file);
        #endif
    } else {
        #if defined(__clang__) && (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            sprintf(bin_file_path, "%s%smain.exe", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
        #else
            sprintf(bin_file_path, "%s%smain", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
        #endif
    }

    char c_compiler_path[PATH_MAX];
    #if defined(__clang__)
        sprintf(c_compiler_path, "clang");
    #elif defined(__GNUC__) || defined(__GNUG__)
        sprintf(c_compiler_path, "gcc");
    #endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    char include_path[PATH_MAX];
    #if defined(__clang__)
        sprintf(include_path, "%%programfiles%%/LLVM/lib/clang/%d.%d.%d/include/chaos", __clang_major__, __clang_minor__, __clang_patchlevel__);
    #elif defined(__GNUC__) || defined(__GNUG__)
        sprintf(include_path, "%%programdata%%/Chocolatey/lib/mingw/tools/install/mingw64/lib/gcc/x86_64-w64-mingw32/%d.%d.%d/include/chaos", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    #endif

    char include_path_helpers[PATH_MAX];
    sprintf(include_path_helpers, "\"%s/utilities/helpers.c\"", include_path);
    char include_path_ast[PATH_MAX];
    sprintf(include_path_ast, "\"%s/ast/ast.c\"", include_path);
    char include_path_errors[PATH_MAX];
    sprintf(include_path_errors, "\"%s/interpreter/errors.c\"", include_path);
    char include_path_extension[PATH_MAX];
    sprintf(include_path_extension, "\"%s/interpreter/extension.c\"", include_path);
    char include_path_function[PATH_MAX];
    sprintf(include_path_function, "\"%s/interpreter/function.c\"", include_path);
    char include_path_module[PATH_MAX];
    sprintf(include_path_module, "\"%s/interpreter/module.c\"", include_path);
    char include_path_symbol[PATH_MAX];
    sprintf(include_path_symbol, "\"%s/interpreter/symbol.c\"", include_path);
    char include_path_alternative[PATH_MAX];
    sprintf(include_path_alternative, "\"%s/compiler/lib/alternative.c\"", include_path);
    char include_path_chaos[PATH_MAX];
    sprintf(include_path_chaos, "\"%s/Chaos.c\"", include_path);
    char include_path_include[PATH_MAX];
    sprintf(include_path_include, "\"-I%s\"", include_path);

    STARTUPINFO info={sizeof(info)};
    PROCESS_INFORMATION processInfo;
    DWORD status;

    char cmd[2048];
    sprintf(
        cmd,
#if !defined(__clang__)
        "/c %s %s %s %s -o %s %s %s %s %s %s %s %s %s %s %s %s",
#else
        "/c %s %s %s -o %s %s %s %s %s %s %s %s %s %s %s %s",
#endif
        c_compiler_path,
#if !defined(__clang__)
        "-fcompare-debug-second",
        "-D__USE_MINGW_ANSI_STDIO",
#else
        "-Wno-everything",
#endif
        "-DCHAOS_COMPILER",
        bin_file_path,
        c_file_path,
        include_path_helpers,
        include_path_ast,
        include_path_errors,
        include_path_extension,
        include_path_function,
        include_path_module,
        include_path_symbol,
        include_path_alternative,
        include_path_chaos,
        include_path_include
    );

    if (extra_flags != NULL)
        sprintf(cmd, "%s %s", cmd, extra_flags);

    if (CreateProcess("C:\\WINDOWS\\system32\\cmd.exe", cmd, NULL, NULL, TRUE, 0, NULL, NULL, &info, &processInfo)) {

        WaitForSingleObject(processInfo.hProcess, INFINITE);

        GetExitCodeProcess(processInfo.hProcess, &status);

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        if (status != 0) {
            printf("Compilation of %s is failed!\n", c_file_path);
            exit(status);
        }
    } else {
        printf("CreateProcess() failed!");
    }
#else
    pid_t pid;

    string_array extra_flags_arr;
    extra_flags_arr.size = 0;
    if (extra_flags != NULL)
        extra_flags_arr = str_split(extra_flags, ' ');
    unsigned extra_flags_count = extra_flags_arr.size;
    if (extra_flags_count > 0)
        extra_flags_count--;

    unsigned arg_count = 20 + extra_flags_count;
#if !defined(__clang__)
    arg_count++;
#endif

    char *c_compiler_args[arg_count];
    c_compiler_args[0] = c_compiler_path;
    c_compiler_args[1] = "-fcommon";
    c_compiler_args[2] = "-DCHAOS_COMPILER";
    c_compiler_args[3] = "-o";
    c_compiler_args[4] = bin_file_path;
    c_compiler_args[5] = c_file_path;
    c_compiler_args[6] = "/usr/local/include/chaos/utilities/helpers.c";
    c_compiler_args[7] = "/usr/local/include/chaos/ast/ast.c";
    c_compiler_args[8] = "/usr/local/include/chaos/interpreter/errors.c";
    c_compiler_args[9] = "/usr/local/include/chaos/interpreter/extension.c";
    c_compiler_args[10] = "/usr/local/include/chaos/interpreter/function.c";
    c_compiler_args[11] = "/usr/local/include/chaos/interpreter/module.c";
    c_compiler_args[12] = "/usr/local/include/chaos/interpreter/symbol.c";
    c_compiler_args[13] = "/usr/local/include/chaos/compiler/lib/alternative.c";
    c_compiler_args[14] = "/usr/local/include/chaos/Chaos.c";
    c_compiler_args[15] = "-lreadline";
    c_compiler_args[16] = "-L/usr/local/opt/readline/lib";
    c_compiler_args[17] = "-ldl";
    c_compiler_args[18] = "-I/usr/local/include/chaos/";

    for (unsigned i = 0; i < (extra_flags_count); i++) {
        c_compiler_args[19 + i] = extra_flags_arr.arr[i];
    }

#if !defined(__clang__)
    c_compiler_args[19 + extra_flags_count] = "-fcompare-debug-second",
#endif

    c_compiler_args[arg_count - 1] = NULL;

    if ((pid = fork()) == -1)
        perror("fork error");
    else if (pid == 0)
        execvp(c_compiler_path, c_compiler_args);

    int status;
    pid_t wait_result;

    while ((wait_result = wait(&status)) != -1)
    {
        // printf("Process %lu returned result: %d\n", (unsigned long) wait_result, status);
        if (status != 0) {
            printf("Compilation of %s is failed!\n", c_file_path);
            exit(1);
        }
    }
#endif

    if (!keep) {
        printf("Cleaning up the temporary files...\n\n");
        remove(c_file_path);
        remove(h_file_path);
    }

    printf("Finished compiling.\n\n");

    char bin_file_path_final[PATH_MAX + 4];
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    if (!string_ends_with(bin_file_path, __KAOS_WINDOWS_EXE_EXT__)) {
        sprintf(bin_file_path_final, "%s%s", bin_file_path, __KAOS_WINDOWS_EXE_EXT__);
    } else {
        sprintf(bin_file_path_final, "%s", bin_file_path);
    }
#else
    sprintf(bin_file_path_final, "%s", bin_file_path);
#endif

    printf("Binary is ready on: %s\n", bin_file_path_final);
}

char* last_function_name;

ASTNode* transpile_functions(ASTNode* ast_node, char *module, FILE *c_fp, unsigned short indent, FILE *h_fp) {
    if (ast_node == NULL) {
        return ast_node;
    }
    char *ast_node_module = malloc(1 + strlen(ast_node->module));
    strcpy(ast_node_module, ast_node->module);
    compiler_escape_module(ast_node_module);

    if (strcmp(ast_node_module, module) != 0) {
        free(ast_node_module);
        return transpile_functions(ast_node->next, module, c_fp, indent, h_fp);
    }
    free(ast_node_module);

    if (is_node_function_related(ast_node)) {
        if (ast_node->depend != NULL) {
            transpile_functions(ast_node->depend, module, c_fp, indent, h_fp);
        }

        if (ast_node->right != NULL) {
            transpile_functions(ast_node->right, module, c_fp, indent, h_fp);
        }

        if (ast_node->left != NULL) {
            transpile_functions(ast_node->left, module, c_fp, indent, h_fp);
        }
    }

    if (debug_enabled)
        printf(
            "(TranspileF)\tASTNode: {id: %llu, node_type: %s, module: %s, string_size: %lu}\n",
            ast_node->id,
            getAstNodeTypeName(ast_node->node_type),
            ast_node->module,
            ast_node->strings_size
        );

    if (ast_node->node_type >= AST_DEFINE_FUNCTION_BOOL && ast_node->node_type <= AST_DEFINE_FUNCTION_VOID) {
        char *function_name = NULL;
        function_name = snprintf_concat_string(function_name, "kaos_function_%s", module);
        function_name = snprintf_concat_string(function_name, "_%s", ast_node->strings[0]);
        if (!ast_node->dont_transpile && !is_in_array(&transpiled_functions, function_name)) {
            append_to_array(&transpiled_functions, function_name);
            fprintf(h_fp, "void %s();\n", function_name);
            fprintf(c_fp, "void %s() {\n", function_name);
            transpile_node(ast_node->child, module, c_fp, indent);
            fprintf(c_fp, "}\n\n");
        }
        free(function_name);
        last_function_name = ast_node->strings[0];
    }

    char *decision_name;
    switch (ast_node->node_type)
    {
        case AST_ADD_FUNCTION_NAME:
            addFunctionNameToFunctionNamesBuffer(ast_node->strings[0]);
            break;
        case AST_APPEND_MODULE:
            appendModuleToModuleBuffer(ast_node->strings[0]);
            break;
        case AST_PREPEND_MODULE:
            prependModuleToModuleBuffer(ast_node->strings[0]);
            break;
        case AST_MODULE_IMPORT:
            compiler_handleModuleImport(NULL, false, c_fp, indent, h_fp);
            break;
        case AST_MODULE_IMPORT_AS:
            compiler_handleModuleImport(ast_node->strings[0], false, c_fp, indent, h_fp);
            break;
        case AST_MODULE_IMPORT_PARTIAL:
            compiler_handleModuleImport(NULL, true, c_fp, indent, h_fp);
            break;
        case AST_DECISION_DEFINE:
            decision_name = NULL;
            decision_name = snprintf_concat_string(decision_name, "kaos_decision_%s", module);
            decision_name = snprintf_concat_string(decision_name, "_%s", last_function_name);
            if (!ast_node->dont_transpile && !is_in_array(&transpiled_decisions, decision_name)) {
                append_to_array(&transpiled_decisions, decision_name);
                fprintf(h_fp, "void %s();\n", decision_name);
                fprintf(c_fp, "void %s() {\n", decision_name);
                transpile_decisions(ast_node->right, module, c_fp, indent);
                fprintf(c_fp, "}\n\n");
            }
            free(decision_name);
            break;
        default:
            break;
    }

    return transpile_functions(ast_node->next, module, c_fp, indent, h_fp);
}

ASTNode* transpile_decisions(ASTNode* ast_node, char *module, FILE *c_fp, unsigned short indent) {
    if (ast_node == NULL) {
        return ast_node;
    }
    char *ast_node_module = malloc(1 + strlen(ast_node->module));
    strcpy(ast_node_module, ast_node->module);
    compiler_escape_module(ast_node_module);

    if (strcmp(ast_node_module, module) != 0) {
        free(ast_node_module);
        return transpile_decisions(ast_node->next, module, c_fp, indent);
    }
    free(ast_node_module);

    if (is_node_function_related(ast_node)) {
        if (ast_node->depend != NULL) {
            transpile_decisions(ast_node->depend, module, c_fp, indent);
        }

        if (ast_node->right != NULL) {
            transpile_decisions(ast_node->right, module, c_fp, indent);
        }

        if (ast_node->left != NULL) {
            transpile_decisions(ast_node->left, module, c_fp, indent);
        }
    }

    if (ast_node->node_type >= AST_DECISION_MAKE_BOOLEAN && ast_node->node_type <= AST_DECISION_MAKE_BOOLEAN_CONTINUE) {
        transpile_node(ast_node->right, module, c_fp, indent);
    }

    if (debug_enabled)
        printf(
            "(TranspileD)\tASTNode: {id: %llu, node_type: %s, module: %s, string_size: %lu}\n",
            ast_node->id,
            getAstNodeTypeName(ast_node->node_type),
            ast_node->module,
            ast_node->strings_size
        );

    switch (ast_node->node_type)
    {
        case AST_DECISION_MAKE_BOOLEAN:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "if (%s) {", ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "if (%s) {", ast_node->right->value.b ? "true" : "false");
            }
            fprintf(
                c_fp,
                "    _Function* function_%llu = callFunction(\"%s\", function_call_stack.arr[function_call_stack.size - 1]->module);",
                compiler_function_counter,
                ast_node->strings[0]
            );
            transpile_function_call_decision(c_fp, ast_node->module, module, ast_node->strings[0]);
            fprintf(c_fp, "return;\n}");
            break;
        case AST_DECISION_MAKE_BOOLEAN_BREAK:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "if (nested_loop_counter > 0 && %s) {", ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "if (nested_loop_counter > 0 && %s) {", ast_node->right->value.b ? "true" : "false");
            }
            fprintf(
                c_fp,
                "    decisionBreakLoop();"
                "}"
            );
            break;
        case AST_DECISION_MAKE_BOOLEAN_CONTINUE:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "if (nested_loop_counter > 0 && %s) {", ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "if (nested_loop_counter > 0 && %s) {", ast_node->right->value.b ? "true" : "false");
            }
            fprintf(
                c_fp,
                "    decisionContinueLoop();"
                "}"
            );
            break;
        case AST_DECISION_MAKE_DEFAULT:
            fprintf(
                c_fp,
                "if (function_call_stack.arr[function_call_stack.size - 1] != NULL) {"
                "    _Function* function_%llu = callFunction(\"%s\", function_call_stack.arr[function_call_stack.size - 1]->module);",
                compiler_function_counter,
                ast_node->strings[0]
            );
            transpile_function_call_decision(c_fp, ast_node->module, module, ast_node->strings[0]);
            fprintf(c_fp, "}");
            break;
        case AST_DECISION_MAKE_DEFAULT_BREAK:
            fprintf(
                c_fp,
                "if (nested_loop_counter > 0 && function_call_stack.arr[function_call_stack.size - 1] != NULL) {"
                "    decisionBreakLoop();"
                "}"
            );
            break;
        case AST_DECISION_MAKE_DEFAULT_CONTINUE:
            fprintf(
                c_fp,
                "if (nested_loop_counter > 0 && function_call_stack.arr[function_call_stack.size - 1] != NULL) {"
                "    decisionContinueLoop();"
                "}"
            );
            break;
        default:
            break;
    }

    return transpile_decisions(ast_node->next, module, c_fp, indent);
}

ASTNode* compiler_register_functions(ASTNode* ast_node, char *module, FILE *c_fp, unsigned short indent) {
    if (ast_node == NULL) {
        return ast_node;
    }
    char *ast_node_module = malloc(1 + strlen(ast_node->module));
    strcpy(ast_node_module, ast_node->module);
    compiler_escape_module(ast_node_module);

    if (strcmp(ast_node_module, module) != 0) {
        free(ast_node_module);
        return compiler_register_functions(ast_node->next, module, c_fp, indent);
    }
    free(ast_node_module);

    if (is_node_function_related(ast_node)) {
        if (ast_node->depend != NULL) {
            transpile_node(ast_node->depend, module, c_fp, indent);
            compiler_register_functions(ast_node->depend, module, c_fp, indent);
        }

        if (ast_node->right != NULL) {
            transpile_node(ast_node->right, module, c_fp, indent);
            compiler_register_functions(ast_node->right, module, c_fp, indent);
        }

        if (ast_node->left != NULL) {
            transpile_node(ast_node->left, module, c_fp, indent);
            compiler_register_functions(ast_node->left, module, c_fp, indent);
        }
    }

    if (debug_enabled)
        printf(
            "(TranspileR)\tASTNode: {id: %llu, node_type: %s, module: %s, string_size: %lu}\n",
            ast_node->id,
            getAstNodeTypeName(ast_node->node_type),
            ast_node->module,
            ast_node->strings_size
        );

    fprintf(c_fp, "%*c", indent, ' ');

    char* compiler_current_context;
    char* compiler_current_module_context;
    char* compiler_current_module;
    if (ast_node->node_type >= AST_DEFINE_FUNCTION_BOOL && ast_node->node_type <= AST_DEFINE_FUNCTION_VOID) {
        compiler_current_context = compiler_getCurrentContext();
        compiler_current_module_context = compiler_getCurrentModuleContext();
        compiler_current_module = compiler_getCurrentModule();
    }

    switch (ast_node->node_type)
    {
        case AST_DEFINE_FUNCTION_BOOL:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_BOOL, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_NUMBER:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_NUMBER, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_STRING:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_STRING, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_ANY:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_ANY, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_LIST:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_LIST, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_DICT:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_DICT, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_BOOL_LIST:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_LIST, K_BOOL, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_BOOL_DICT:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_DICT, K_BOOL, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_NUMBER_LIST:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_LIST, K_NUMBER, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_NUMBER_DICT:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_DICT, K_NUMBER, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_STRING_LIST:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_LIST, K_STRING, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_STRING_DICT:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_DICT, K_STRING, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_DEFINE_FUNCTION_VOID:
            fprintf(
                c_fp,
                "startFunction(\"%s\", K_VOID, K_ANY, \"%s\", \"%s\", \"%s\", false);\n",
                ast_node->strings[0],
                compiler_current_context,
                compiler_current_module_context,
                compiler_current_module
            );
            break;
        case AST_FUNCTION_PARAMETERS_START:
            fprintf(c_fp, "if (function_parameters_mode == NULL) startFunctionParameters();\n");
            break;
        case AST_FUNCTION_PARAMETER_BOOL:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_BOOL, K_ANY);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_NUMBER:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_NUMBER, K_ANY);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_STRING:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_STRING, K_ANY);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_LIST:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_LIST, K_ANY);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_BOOL_LIST:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_LIST, K_BOOL);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_NUMBER_LIST:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_LIST, K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_STRING_LIST:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_LIST, K_STRING);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_DICT:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_DICT, K_ANY);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_BOOL_DICT:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_DICT, K_BOOL);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_NUMBER_DICT:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_DICT, K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_FUNCTION_PARAMETER_STRING_DICT:
            fprintf(c_fp, "addFunctionParameter(\"%s\", K_DICT, K_STRING);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_BOOL:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "addFunctionOptionalParameterBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "addFunctionOptionalParameterBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
            }
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_NUMBER:
            if (ast_node->right->is_transpiled) {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "addFunctionOptionalParameterInt(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "addFunctionOptionalParameterFloat(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                }
            } else {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "addFunctionOptionalParameterInt(\"%s\", %lld);", ast_node->strings[0], ast_node->right->value.i);
                } else {
                    fprintf(c_fp, "addFunctionOptionalParameterFloat(\"%s\", %Lg);", ast_node->strings[0], ast_node->right->value.f);
                }
            }
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_STRING:
            fprintf(c_fp, "addFunctionOptionalParameterString(\"%s\", \"%s\");", ast_node->strings[0], ast_node->value.s);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_LIST:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_BOOL_LIST:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_NUMBER_LIST:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_STRING_LIST:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_DICT:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_BOOL_DICT:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_NUMBER_DICT:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_OPTIONAL_FUNCTION_PARAMETER_STRING_DICT:
            fprintf(c_fp, "reverseComplexMode(); addFunctionOptionalParameterComplex(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_ADD_FUNCTION_NAME:
            addFunctionNameToFunctionNamesBuffer(ast_node->strings[0]);
            break;
        case AST_APPEND_MODULE:
            appendModuleToModuleBuffer(ast_node->strings[0]);
            break;
        case AST_PREPEND_MODULE:
            prependModuleToModuleBuffer(ast_node->strings[0]);
            break;
        case AST_MODULE_IMPORT:
            compiler_handleModuleImportRegister(NULL, false, c_fp, indent);
            break;
        case AST_MODULE_IMPORT_AS:
            compiler_handleModuleImportRegister(ast_node->strings[0], false, c_fp, indent);
            break;
        case AST_MODULE_IMPORT_PARTIAL:
            compiler_handleModuleImportRegister(NULL, true, c_fp, indent);
            break;
        case AST_DECISION_DEFINE:
            // TODO: The code below cannot be compiled.
            // decision_mode->decision_node = ast_node->right;
            // decision_mode = NULL;
            break;
        default:
            break;
    }

    if (ast_node->node_type >= AST_DEFINE_FUNCTION_BOOL && ast_node->node_type <= AST_DEFINE_FUNCTION_VOID) {
        free(compiler_current_context);
        free(compiler_current_module_context);
        free(compiler_current_module);
    }

    return compiler_register_functions(ast_node->next, module, c_fp, indent);
}

ASTNode* transpile_node(ASTNode* ast_node, char *module, FILE *c_fp, unsigned short indent) {
    if (ast_node == NULL) {
        return ast_node;
    }
    char *ast_node_module = malloc(1 + strlen(ast_node->module));
    strcpy(ast_node_module, ast_node->module);
    compiler_escape_module(ast_node_module);

    if (strcmp(ast_node_module, module) != 0) {
        free(ast_node_module);
        return transpile_node(ast_node->next, module, c_fp, indent);
    }
    free(ast_node_module);

    if (ast_node->node_type != AST_FUNCTION_STEP)
        if (is_node_function_related(ast_node)) return transpile_node(ast_node->next, module, c_fp, indent);

    if (ast_node->depend != NULL) {
        transpile_node(ast_node->depend, module, c_fp, indent);
    }

    if (ast_node->right != NULL) {
        transpile_node(ast_node->right, module, c_fp, indent);
    }

    if (ast_node->left != NULL) {
        transpile_node(ast_node->left, module, c_fp, indent);
    }

    if (ast_node->node_type == AST_END) {
        return ast_node;
    }

    if (debug_enabled)
        printf(
            "(Transpile)\tASTNode: {id: %llu, node_type: %s, module: %s, string_size: %lu}\n",
            ast_node->id,
            getAstNodeTypeName(ast_node->node_type),
            ast_node->module,
            ast_node->strings_size
        );

    fprintf(c_fp, "%*c", indent, ' ');

    unsigned long long current_loop_counter = 0;
    switch (ast_node->node_type)
    {
        case AST_START_TIMES_DO:
            compiler_loop_counter++;
            fprintf(c_fp, "nested_loop_counter++;");
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "for (int i = 0; i < %s; i++) {\n", ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "for (int i = 0; i < %lld; i++) {\n", ast_node->right->value.i);
            }
            indent += indent_length;
            fprintf(c_fp, "if (setjmp(LoopBreak)) break;");
            fprintf(c_fp, "if (setjmp(LoopContinue)) continue;");
            ast_node = transpile_node(ast_node->next, module, c_fp, indent);
            indent -= indent_length;
            fprintf(c_fp, "%*c}\n", indent, ' ');
            fprintf(c_fp, "nested_loop_counter--;");
            break;
        case AST_START_TIMES_DO_INFINITE:
            compiler_loop_counter++;
            fprintf(c_fp, "nested_loop_counter++;");
            fprintf(c_fp, "while (true) {");
            indent += indent_length;
            fprintf(c_fp, "if (setjmp(LoopBreak)) break;");
            fprintf(c_fp, "if (setjmp(LoopContinue)) continue;");
            ast_node = transpile_node(ast_node->next, module, c_fp, indent);
            indent -= indent_length;
            fprintf(c_fp, "%*c}\n", indent, ' ');
            fprintf(c_fp, "nested_loop_counter--;");
            break;
        case AST_START_TIMES_DO_VAR:
            compiler_loop_counter++;
            fprintf(c_fp, "nested_loop_counter++;");
            fprintf(c_fp, "for (int i = 0; i < (unsigned) getSymbolValueInt(\"%s\"); i++) {\n", ast_node->strings[0]);
            indent += indent_length;
            fprintf(c_fp, "if (setjmp(LoopBreak)) break;");
            fprintf(c_fp, "if (setjmp(LoopContinue)) continue;");
            ast_node = transpile_node(ast_node->next, module, c_fp, indent);
            indent -= indent_length;
            fprintf(c_fp, "%*c}\n", indent, ' ');
            fprintf(c_fp, "nested_loop_counter--;");
            break;
        case AST_START_FOREACH:
            compiler_loop_counter++;
            current_loop_counter = compiler_loop_counter;
            fprintf(c_fp, "nested_loop_counter++;");
            fprintf(
                c_fp,
                "Symbol* loop_%llu_list = getSymbol(\"%s\");\n"
                "%*cif (loop_%llu_list->type != K_LIST) throw_error(E_NOT_A_LIST, \"%s\");\n"
                "%*cfor (unsigned long i = 0; i < loop_%llu_list->children_count; i++) {\n"
                "%*cSymbol* loop_%llu_child = loop_%llu_list->children[i];\n"
                "%*cSymbol* loop_%llu_clone_symbol = createCloneFromSymbol(\"%s\", loop_%llu_child->type, loop_%llu_child, loop_%llu_child->secondary_type);\n",
                compiler_loop_counter,
                ast_node->strings[0],
                indent,
                ' ',
                compiler_loop_counter,
                ast_node->strings[0],
                indent,
                ' ',
                compiler_loop_counter,
                indent * 2,
                ' ',
                compiler_loop_counter,
                compiler_loop_counter,
                indent * 2,
                ' ',
                compiler_loop_counter,
                ast_node->strings[1],
                compiler_loop_counter,
                compiler_loop_counter,
                compiler_loop_counter
            );
            indent += indent_length;
            fprintf(
                c_fp,
                "if (setjmp(LoopBreak)) {"
                "   removeSymbol(loop_%llu_clone_symbol);"
                "   break;"
                "}",
                compiler_loop_counter
            );
            fprintf(
                c_fp,
                "if (setjmp(LoopContinue)) {"
                "   removeSymbol(loop_%llu_clone_symbol);"
                "   continue;"
                "}",
                compiler_loop_counter
            );
            ast_node = transpile_node(ast_node->next, module, c_fp, indent);
            indent -= indent_length;
            fprintf(c_fp, "%*cremoveSymbol(loop_%llu_clone_symbol);\n", indent * 2, ' ', current_loop_counter);
            fprintf(c_fp, "%*c}\n", indent, ' ');
            fprintf(c_fp, "nested_loop_counter--;");
            break;
        case AST_START_FOREACH_DICT:
            compiler_loop_counter++;
            current_loop_counter = compiler_loop_counter;
            fprintf(c_fp, "nested_loop_counter++;");
            fprintf(
                c_fp,
                "Symbol* loop_%llu_dict = getSymbol(\"%s\");\n"
                "%*cif (loop_%llu_dict->type != K_DICT) throw_error(E_NOT_A_DICT, \"%s\");\n"
                "%*cfor (unsigned long i = 0; i < loop_%llu_dict->children_count; i++) {\n"
                "%*cSymbol* loop_%llu_child = loop_%llu_dict->children[i];\n"
                "%*caddSymbolString(\"%s\", loop_%llu_child->key);\n"
                "%*cSymbol* loop_%llu_clone_symbol = createCloneFromSymbol(\"%s\", loop_%llu_child->type, loop_%llu_child, loop_%llu_child->secondary_type);\n",
                compiler_loop_counter,
                ast_node->strings[0],
                indent,
                ' ',
                compiler_loop_counter,
                ast_node->strings[0],
                indent,
                ' ',
                compiler_loop_counter,
                indent * 2,
                ' ',
                compiler_loop_counter,
                compiler_loop_counter,
                indent * 2,
                ' ',
                ast_node->strings[1],
                compiler_loop_counter,
                indent * 2,
                ' ',
                compiler_loop_counter,
                ast_node->strings[2],
                compiler_loop_counter,
                compiler_loop_counter,
                compiler_loop_counter
            );
            indent += indent_length;
            fprintf(
                c_fp,
                "if (setjmp(LoopBreak)) {"
                "   removeSymbol(loop_%llu_clone_symbol);"
                "   removeSymbolByName(\"%s\");"
                "   break;"
                "}",
                compiler_loop_counter,
                ast_node->strings[1]
            );
            fprintf(
                c_fp,
                "if (setjmp(LoopContinue)) {"
                "   removeSymbol(loop_%llu_clone_symbol);"
                "   removeSymbolByName(\"%s\");"
                "   continue;"
                "}",
                compiler_loop_counter,
                ast_node->strings[1]
            );
            ASTNode* next_node = transpile_node(ast_node->next, module, c_fp, indent);
            indent -= indent_length;
            fprintf(c_fp, "%*cremoveSymbol(loop_%llu_clone_symbol);\n", indent * 2, ' ', current_loop_counter);
            fprintf(c_fp, "%*cremoveSymbolByName(\"%s\");\n", indent * 2, ' ', ast_node->strings[1]);
            fprintf(c_fp, "%*c}\n", indent, ' ');
            fprintf(c_fp, "nested_loop_counter--;");
            ast_node = next_node;
            break;
        default:
            break;
    }

    long double l_value;
    long double r_value;
    char *_module = NULL;
    char *value_s = NULL;
    switch (ast_node->node_type)
    {
        case AST_VAR_CREATE_BOOL:
            if (ast_node->strings[0] == NULL) {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "addSymbolBool(NULL, %s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "addSymbolBool(NULL, %s);", ast_node->right->value.b ? "true" : "false");
                }
            } else {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "addSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "addSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
                }
            }
            break;
        case AST_VAR_CREATE_BOOL_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_BOOL, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_BOOL, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_BOOL, K_ANY);
            break;
        case AST_VAR_CREATE_NUMBER:
            if (ast_node->right->is_transpiled) {
                if (ast_node->right->value_type == V_INT) {
                    if (ast_node->strings[0] == NULL) {
                        fprintf(c_fp, "addSymbolInt(NULL, %s);", ast_node->right->transpiled);
                    } else {
                        fprintf(c_fp, "addSymbolInt(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                    }
                } else {
                    if (ast_node->strings[0] == NULL) {
                        fprintf(c_fp, "addSymbolFloat(NULL, %s);", ast_node->right->transpiled);
                    } else {
                        fprintf(c_fp, "addSymbolFloat(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                    }
                }
            } else {
                if (ast_node->right->value_type == V_INT) {
                    if (ast_node->strings[0] == NULL) {
                        fprintf(c_fp, "addSymbolInt(NULL, %lld);", ast_node->right->value.i);
                    } else {
                        fprintf(c_fp, "addSymbolInt(\"%s\", %lld);", ast_node->strings[0], ast_node->right->value.i);
                    }
                } else {
                    if (ast_node->strings[0] == NULL) {
                        fprintf(c_fp, "addSymbolFloat(NULL, %Lg);", ast_node->right->value.f);
                    } else {
                        fprintf(c_fp, "addSymbolFloat(\"%s\", %Lg);", ast_node->strings[0], ast_node->right->value.f);
                    }
                }
            }
            break;
        case AST_VAR_CREATE_NUMBER_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_NUMBER, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_NUMBER, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_NUMBER, K_ANY);
            break;
        case AST_VAR_CREATE_STRING:
            value_s = escape_string_literal_for_transpiler(ast_node->value.s);
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolString(NULL, \"%s\");", value_s);
            } else {
                fprintf(c_fp, "addSymbolString(\"%s\", \"%s\");", ast_node->strings[0], value_s);
            }
            free(value_s);
            break;
        case AST_VAR_CREATE_STRING_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_STRING, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_STRING, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_STRING, K_ANY);
            break;
        case AST_VAR_CREATE_ANY_BOOL:
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolAnyBool(NULL, %s);", ast_node->right->value.b ? "true" : "false");
            } else {
                fprintf(c_fp, "addSymbolAnyBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
            }
            break;
        case AST_VAR_CREATE_ANY_NUMBER:
            if (ast_node->right->value_type == V_INT) {
                if (ast_node->strings[0] == NULL) {
                    fprintf(c_fp, "addSymbolAnyInt(NULL, %lld);", ast_node->right->value.i);
                } else {
                    fprintf(c_fp, "addSymbolAnyInt(\"%s\", %lld);", ast_node->strings[0], ast_node->right->value.i);
                }
            } else {
                if (ast_node->strings[0] == NULL) {
                    fprintf(c_fp, "addSymbolAnyFloat(NULL, %Lg);", ast_node->right->value.f);
                } else {
                    fprintf(c_fp, "addSymbolAnyFloat(\"%s\", %Lg);", ast_node->strings[0], ast_node->right->value.f);
                }
            }
            break;
        case AST_VAR_CREATE_ANY_STRING:
            value_s = escape_string_literal_for_transpiler(ast_node->value.s);
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolAnyString(NULL, \"%s\");", value_s);
            } else {
                fprintf(c_fp, "addSymbolAnyString(\"%s\", \"%s\");", ast_node->strings[0], value_s);
            }
            free(value_s);
            break;
        case AST_VAR_CREATE_ANY_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_ANY, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_ANY_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_ANY, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_ANY_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_ANY, K_ANY);
            break;
        case AST_VAR_CREATE_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_LIST_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_LIST, K_ANY);
            break;
        case AST_VAR_CREATE_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_DICT_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_DICT, K_ANY);
            break;
        case AST_VAR_CREATE_BOOL_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_BOOL_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_BOOL);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_LIST_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_LIST, K_BOOL);
            break;
        case AST_VAR_CREATE_BOOL_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_BOOL_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_BOOL);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_DICT_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_DICT, K_BOOL);
            break;
        case AST_VAR_CREATE_NUMBER_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_NUMBER_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_NUMBER);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_LIST_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_LIST, K_NUMBER);
            break;
        case AST_VAR_CREATE_NUMBER_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_NUMBER_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_NUMBER);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_DICT_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_DICT, K_NUMBER);
            break;
        case AST_VAR_CREATE_STRING_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_STRING_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_STRING);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_LIST_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_LIST, K_STRING);
            break;
        case AST_VAR_CREATE_STRING_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_STRING_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_STRING);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_DICT_FUNC_RETURN:
            transpile_function_call_create_var(c_fp, ast_node, module, K_DICT, K_STRING);
            break;
        case AST_VAR_UPDATE_BOOL:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "updateSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "updateSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
            }
            break;
        case AST_VAR_UPDATE_NUMBER:
            if (ast_node->right->value_type == V_INT) {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "updateSymbolInt(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "updateSymbolInt(\"%s\", %lld);", ast_node->strings[0], ast_node->right->value.i);
                }
            } else {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "updateSymbolFloat(\"%s\", %s);", ast_node->strings[0], ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "updateSymbolFloat(\"%s\", %Lg);", ast_node->strings[0], ast_node->right->value.f);
                }
            }
            break;
        case AST_VAR_UPDATE_STRING:
            value_s = escape_string_literal_for_transpiler(ast_node->value.s);
            fprintf(c_fp, "updateSymbolString(\"%s\", \"%s\");", ast_node->strings[0], value_s);
            free(value_s);
            break;
        case AST_VAR_UPDATE_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexModeWithUpdate(\"%s\");", ast_node->strings[0]);
            break;
        case AST_VAR_UPDATE_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexModeWithUpdate(\"%s\");", ast_node->strings[0]);
            break;
        case AST_VAR_UPDATE_VAR:
            fprintf(c_fp, "updateSymbolByClonningName(\"%s\", \"%s\");", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_UPDATE_VAR_EL:
            fprintf(c_fp, "updateSymbolByClonningComplexElement(\"%s\", \"%s\");", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_UPDATE_FUNC_RETURN:
            compiler_function_counter++;
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "_Function* function_%llu = callFunction(\"%s\", NULL);",
                        compiler_function_counter,
                        ast_node->strings[1]
                    );
                    transpile_function_call(c_fp, NULL, ast_node->strings[1]);
                    fprintf(
                        c_fp,
                        "updateSymbolByClonningFunctionReturn(\"%s\", \"%s\", NULL);",
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "_Function* function_%llu = callFunction(\"%s\", \"%s\");",
                        compiler_function_counter,
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    transpile_function_call(c_fp, ast_node->strings[1], ast_node->strings[2]);
                    fprintf(
                        c_fp,
                        "updateSymbolByClonningFunctionReturn(\"%s\", \"%s\", \"%s\");",
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_RETURN_VAR:
            fprintf(c_fp, "returnSymbol(\"%s\");", ast_node->strings[0]);
            break;
        case AST_PRINT_COMPLEX_EL:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getComplexElementBySymbolId(variable_complex_element, variable_complex_element_symbol_id), false, false);");
            break;
        case AST_COMPLEX_EL_UPDATE_BOOL:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "updateComplexElementBool(%s);", ast_node->right->transpiled);
            } else {
                fprintf(c_fp, "updateComplexElementBool(%s);", ast_node->right->value.b ? "true" : "false");
            }
            break;
        case AST_COMPLEX_EL_UPDATE_NUMBER:
            if (ast_node->right->value_type == V_INT) {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "updateComplexElementInt(%s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "updateComplexElementInt(%lld);", ast_node->right->value.i);
                }
            } else {
                if (ast_node->right->is_transpiled) {
                    fprintf(c_fp, "updateComplexElementFloat(%s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "updateComplexElementFloat(%Lg);", ast_node->right->value.f);
                }
            }
            break;
        case AST_COMPLEX_EL_UPDATE_STRING:
            if (ast_node->right->is_transpiled) {
                fprintf(c_fp, "updateComplexElementString(%s);", ast_node->right->transpiled);
            } else {
                value_s = escape_string_literal_for_transpiler(ast_node->value.s);
                fprintf(c_fp, "updateComplexElementString(\"%s\");", value_s);
                free(value_s);
            }
            break;
        case AST_COMPLEX_EL_UPDATE_LIST:
            fprintf(c_fp, "reverseComplexMode(); updateComplexElementComplex();");
            break;
        case AST_COMPLEX_EL_UPDATE_DICT:
            fprintf(c_fp, "reverseComplexMode(); updateComplexElementComplex();");
            break;
        case AST_COMPLEX_EL_UPDATE_VAR:
            fprintf(c_fp, "updateComplexElementSymbol(getSymbol(\"%s\"));", ast_node->strings[0]);
            break;
        case AST_COMPLEX_EL_UPDATE_VAR_EL:
            fprintf(c_fp, "updateComplexElementSymbol(getComplexElementThroughLeftRightBracketStack(\"%s\", 0));", ast_node->strings[0]);
            break;
        case AST_COMPLEX_EL_UPDATE_FUNC_RETURN:
            compiler_function_counter++;
            switch (ast_node->strings_size)
            {
                case 1:
                    fprintf(
                        c_fp,
                        "_Function* function_%llu = callFunction(\"%s\", NULL);",
                        compiler_function_counter,
                        ast_node->strings[0]
                    );
                    transpile_function_call(c_fp, NULL, ast_node->strings[0]);
                    fprintf(
                        c_fp,
                        "updateComplexSymbolByClonningFunctionReturn(\"%s\", NULL);",
                        ast_node->strings[0]
                    );
                    break;
                case 2:
                    fprintf(
                        c_fp,
                        "_Function* function_%llu = callFunction(\"%s\", \"%s\");",
                        compiler_function_counter,
                        ast_node->strings[1],
                        ast_node->strings[0]
                    );
                    transpile_function_call(c_fp, ast_node->strings[0], ast_node->strings[1]);
                    fprintf(
                        c_fp,
                        "updateComplexSymbolByClonningFunctionReturn(\"%s\", \"%s\");",
                        ast_node->strings[1],
                        ast_node->strings[0]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_PRINT_VAR:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getSymbol(\"%s\"), false, true);", ast_node->strings[0]);
            break;
        case AST_PRINT_VAR_EL:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getComplexElementThroughLeftRightBracketStack(\"%s\", 0), false, true);", ast_node->strings[0]);
            break;
        case AST_PRINT_EXPRESSION:
            fprintf(c_fp, "printf(\"%lld\\n\");", ast_node->right->value.i);
            break;
        case AST_PRINT_MIXED_EXPRESSION:
            fprintf(c_fp, "printf(\"%Lg\\n\");", ast_node->right->value.f);
            break;
        case AST_PRINT_STRING:
            value_s = escape_string_literal_for_transpiler(ast_node->value.s);
            fprintf(c_fp, "printf(\"%s\\n\");", value_s);
            free(value_s);
            break;
        case AST_ECHO_VAR:
            fprintf(c_fp, "printSymbolValueEndWith(getSymbol(\"%s\"), \"\", false, true);", ast_node->strings[0]);
            break;
        case AST_ECHO_VAR_EL:
            fprintf(c_fp, "printSymbolValueEndWith(getComplexElementThroughLeftRightBracketStack(\"%s\", 0), \"\", false, true);", ast_node->strings[0]);
            break;
        case AST_ECHO_EXPRESSION:
            fprintf(c_fp, "printf(\"%lld\");", ast_node->right->value.i);
            break;
        case AST_ECHO_MIXED_EXPRESSION:
            fprintf(c_fp, "printf(\"%Lg\");", ast_node->right->value.f);
            break;
        case AST_ECHO_STRING:
            value_s = escape_string_literal_for_transpiler(ast_node->value.s);
            fprintf(c_fp, "printf(\"%s\");", value_s);
            free(value_s);
            break;
        case AST_PRETTY_PRINT_VAR:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getSymbol(\"%s\"), true, true);", ast_node->strings[0]);
            break;
        case AST_PRETTY_PRINT_VAR_EL:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getComplexElementThroughLeftRightBracketStack(\"%s\", 0), true, true);", ast_node->strings[0]);
            break;
        case AST_PRETTY_ECHO_VAR:
            fprintf(c_fp, "printSymbolValueEndWith(getSymbol(\"%s\"), \"\", true, true);", ast_node->strings[0]);
            break;
        case AST_PRETTY_ECHO_VAR_EL:
            fprintf(c_fp, "printSymbolValueEndWith(getComplexElementThroughLeftRightBracketStack(\"%s\", 0), \"\", true, true);", ast_node->strings[0]);
            break;
        case AST_PARENTHESIS:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "(%s)", ast_node->right->transpiled);
            } else {
                switch (ast_node->right->value_type)
                {
                    case V_BOOL:
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->value.b ? "true" : "false");
                        break;
                    case V_INT:
                        ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->right->value.i);
                        break;
                    case V_FLOAT:
                        ast_node->transpiled = snprintf_concat_float(ast_node->transpiled, "%Lg", ast_node->right->value.f);
                        break;
                    default:
                        break;
                }
            }
            ast_node->is_transpiled = true;
            break;
        case AST_EXPRESSION_PLUS:
            if (!transpile_common_operator(ast_node, "+", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i + ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_MINUS:
            if (!transpile_common_operator(ast_node, "-", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i - ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_MULTIPLY:
            if (!transpile_common_operator(ast_node, "*", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i * ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_AND:
            if (!transpile_common_operator(ast_node, "&", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i & ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_OR:
            if (!transpile_common_operator(ast_node, "|", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i | ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_XOR:
            if (!transpile_common_operator(ast_node, "^", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i ^ ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_NOT:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "~ %s", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.i = ~ ast_node->right->value.i;
            }
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_LEFT_SHIFT:
            if (!transpile_common_operator(ast_node, "<<", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i << ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_EXPRESSION_BITWISE_RIGHT_SHIFT:
            if (!transpile_common_operator(ast_node, ">>", V_INT, V_INT))
                ast_node->value.i = ast_node->left->value.i >> ast_node->right->value.i;
            ast_node->value_type = V_INT;
            break;
        case AST_VAR_EXPRESSION_VALUE:
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "getSymbolValueInt(\"%s\")", ast_node->strings[0]);
            ast_node->is_transpiled = true;
            ast_node->value_type = V_INT;
            break;
        case AST_VAR_EXPRESSION_INCREMENT:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s + 1", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.i = ast_node->right->value.i + 1;
            }
            ast_node->value_type = V_INT;
            break;
        case AST_VAR_EXPRESSION_DECREMENT:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s - 1", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.i = ast_node->right->value.i - 1;
            }
            ast_node->value_type = V_INT;
            break;
        case AST_VAR_EXPRESSION_INCREMENT_ASSIGN:
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "incrementThenAssign(\"%s\"", ast_node->strings[0]);
            ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, ", %lld)", ast_node->value.i);
            ast_node->is_transpiled = true;
            ast_node->value_type = V_INT;
            break;
        case AST_VAR_EXPRESSION_ASSIGN_INCREMENT:
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "assignThenIncrement(\"%s\"", ast_node->strings[0]);
            ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, ", %lld)", ast_node->value.i);
            ast_node->is_transpiled = true;
            ast_node->value_type = V_INT;
            break;
        case AST_MIXED_EXPRESSION_PLUS:
            if (!transpile_common_mixed_operator(ast_node, "+")) {
                if (ast_node->left->value_type == V_INT) {
                    l_value = (long double) ast_node->left->value.i;
                } else {
                    l_value = ast_node->left->value.f;
                }
                if (ast_node->right->value_type == V_INT) {
                    r_value = (long double) ast_node->right->value.i;
                } else {
                    r_value = ast_node->right->value.f;
                }
                ast_node->value.f = l_value + r_value;
            }
            ast_node->value_type = V_FLOAT;
            break;
        case AST_MIXED_EXPRESSION_MINUS:
            if (!transpile_common_mixed_operator(ast_node, "-")) {
                if (ast_node->left->value_type == V_INT) {
                    l_value = (long double) ast_node->left->value.i;
                } else {
                    l_value = ast_node->left->value.f;
                }
                if (ast_node->right->value_type == V_INT) {
                    r_value = (long double) ast_node->right->value.i;
                } else {
                    r_value = ast_node->right->value.f;
                }
                ast_node->value.f = l_value - r_value;
            }
            ast_node->value_type = V_FLOAT;
            break;
        case AST_MIXED_EXPRESSION_MULTIPLY:
            if (!transpile_common_mixed_operator(ast_node, "*")) {
                if (ast_node->left->value_type == V_INT) {
                    l_value = (long double) ast_node->left->value.i;
                } else {
                    l_value = ast_node->left->value.f;
                }
                if (ast_node->right->value_type == V_INT) {
                    r_value = (long double) ast_node->right->value.i;
                } else {
                    r_value = ast_node->right->value.f;
                }
                ast_node->value.f = l_value * r_value;
            }
            ast_node->value_type = V_FLOAT;
            break;
        case AST_MIXED_EXPRESSION_DIVIDE:
            if (!transpile_common_mixed_operator(ast_node, "/")) {
                if (ast_node->left->value_type == V_INT) {
                    l_value = (long double) ast_node->left->value.i;
                } else {
                    l_value = ast_node->left->value.f;
                }
                if (ast_node->right->value_type == V_INT) {
                    r_value = (long double) ast_node->right->value.i;
                } else {
                    r_value = ast_node->right->value.f;
                }
                ast_node->value.f = l_value / r_value;
            }
            ast_node->value_type = V_FLOAT;
            break;
        case AST_VAR_MIXED_EXPRESSION_VALUE:
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "getSymbolValueFloat(\"%s\")", ast_node->strings[0]);
            ast_node->is_transpiled = true;
            ast_node->value_type = V_FLOAT;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL:
            if (!transpile_common_operator(ast_node, "==", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b == ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL:
            if (!transpile_common_operator(ast_node, "!=", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b != ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT:
            if (!transpile_common_operator(ast_node, ">", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b > ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL:
            if (!transpile_common_operator(ast_node, "<", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b < ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL:
            if (!transpile_common_operator(ast_node, ">=", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b >= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL:
            if (!transpile_common_operator(ast_node, "<=", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b <= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND:
            if (!transpile_common_operator(ast_node, "&&", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b && ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR:
            if (!transpile_common_operator(ast_node, "||", V_BOOL, V_BOOL))
                ast_node->value.b = ast_node->left->value.b || ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_NOT:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "! %s", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.b = ! ast_node->right->value.b;
            }
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_MIXED:
            if (!transpile_common_operator(ast_node, "==", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f == ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_MIXED:
            if (!transpile_common_operator(ast_node, "!=", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f != ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_MIXED:
            if (!transpile_common_operator(ast_node, ">", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f > ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_MIXED:
            if (!transpile_common_operator(ast_node, "<", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f < ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_MIXED:
            if (!transpile_common_operator(ast_node, ">=", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f >= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_MIXED:
            if (!transpile_common_operator(ast_node, "<=", V_FLOAT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.f <= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_MIXED:
            if (!transpile_common_operator(ast_node, "&&", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f && ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_MIXED:
            if (!transpile_common_operator(ast_node, "||", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f || ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_NOT_MIXED:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "! %s", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.b = ! ast_node->right->value.f;
            }
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, "!=", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f != ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, ">", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f > ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, "<", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f < ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, ">=", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f >= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, "<=", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f <= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, "&&", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f && ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_MIXED_BOOLEAN:
            if (!transpile_common_operator(ast_node, "||", V_FLOAT, V_BOOL))
                ast_node->value.b = ast_node->left->value.f || ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "==", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b == ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "!=", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b != ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, ">", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b > ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "<", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b < ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, ">=", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b >= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "<=", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b <= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "&&", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b && ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_BOOLEAN_MIXED:
            if (!transpile_common_operator(ast_node, "||", V_BOOL, V_FLOAT))
                ast_node->value.b = ast_node->left->value.b || ast_node->right->value.f;
            break;
        case AST_VAR_BOOLEAN_EXPRESSION_VALUE:
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "getSymbolValueBool(\"%s\")", ast_node->strings[0]);
            ast_node->is_transpiled = true;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_EXP:
            if (!transpile_common_operator(ast_node, "==", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i == ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_EXP:
            if (!transpile_common_operator(ast_node, "!=", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i != ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EXP:
            if (!transpile_common_operator(ast_node, ">", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i > ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EXP:
            if (!transpile_common_operator(ast_node, "<", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i < ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_EXP:
            if (!transpile_common_operator(ast_node, ">=", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i >= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_EXP:
            if (!transpile_common_operator(ast_node, "<=", V_INT, V_INT))
                ast_node->value.b = ast_node->left->value.i <= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_EXP:
            if (!transpile_common_operator(ast_node, "&&", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i && ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_EXP:
            if (!transpile_common_operator(ast_node, "||", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i || ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_NOT_EXP:
            if (ast_node->right->is_transpiled) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "! %s", ast_node->right->transpiled);
                ast_node->is_transpiled = true;
            } else {
                ast_node->value.b = ! ast_node->right->value.i;
            }
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "==", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i == ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "!=", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i != ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, ">", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i > ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "<", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i < ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, ">=", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i >= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "<=", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i <= ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "&&", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i && ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_EXP_BOOLEAN:
            if (!transpile_common_operator(ast_node, "||", V_INT, V_BOOL))
                ast_node->value.b = ast_node->left->value.i || ast_node->right->value.b;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "==", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b == ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "!=", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b != ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, ">", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b > ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "<", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b < ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, ">=", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b >= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "<=", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b <= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "&&", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b && ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_BOOLEAN_EXP:
            if (!transpile_common_operator(ast_node, "||", V_BOOL, V_INT))
                ast_node->value.b = ast_node->left->value.b || ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "==", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f == ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "!=", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f != ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_MIXED_EXP:
            if (!transpile_common_operator(ast_node, ">", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f > ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "<", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f < ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_MIXED_EXP:
            if (!transpile_common_operator(ast_node, ">=", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f >= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "<=", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f <= ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "&&", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f && ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_MIXED_EXP:
            if (!transpile_common_operator(ast_node, "||", V_FLOAT, V_INT))
                ast_node->value.b = ast_node->left->value.f || ast_node->right->value.i;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_EQUAL_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "==", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i == ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_NOT_EQUAL_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "!=", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i != ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EXP_MIXED:
            if (!transpile_common_operator(ast_node, ">", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i > ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "<", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i < ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_GREAT_EQUAL_EXP_MIXED:
            if (!transpile_common_operator(ast_node, ">=", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i >= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_REL_SMALL_EQUAL_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "<=", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i <= ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_AND_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "&&", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i && ast_node->right->value.f;
            break;
        case AST_BOOLEAN_EXPRESSION_LOGIC_OR_EXP_MIXED:
            if (!transpile_common_operator(ast_node, "||", V_INT, V_FLOAT))
                ast_node->value.b = ast_node->left->value.i || ast_node->right->value.f;
            break;
        case AST_DELETE_VAR:
            fprintf(c_fp, "removeSymbolByName(\"%s\");", ast_node->strings[0]);
            break;
        case AST_DELETE_VAR_EL:
            fprintf(c_fp, "removeComplexElementByLeftRightBracketStack(\"%s\");", ast_node->strings[0]);
            break;
        case AST_PRINT_SYMBOL_TABLE:
            fprintf(c_fp, "printSymbolTable();");
            break;
        case AST_LIST_START:
            fprintf(c_fp, "addSymbolList(NULL);");
            break;
        case AST_LIST_ADD_VAR:
            fprintf(c_fp, "cloneSymbolToComplex(\"%s\", NULL);", ast_node->strings[0]);
            break;
        case AST_LIST_ADD_VAR_EL:
            fprintf(c_fp, "buildVariableComplexElement(\"%s\", NULL);", ast_node->strings[0]);
            break;
        case AST_LIST_NESTED_FINISH:
            fprintf(c_fp, "if (isNestedComplexMode()) { pushNestedComplexModeStack(getComplexMode()); reverseComplexMode(); finishComplexMode(NULL, K_ANY); }");
            break;
        case AST_DICT_START:
            fprintf(c_fp, "addSymbolDict(NULL);");
            break;
        case AST_DICT_ADD_VAR:
            fprintf(c_fp, "cloneSymbolToComplex(\"%s\", \"%s\");", ast_node->strings[1], ast_node->strings[0]);
            break;
        case AST_DICT_ADD_VAR_EL:
            fprintf(c_fp, "buildVariableComplexElement(\"%s\", \"%s\");", ast_node->strings[1], ast_node->strings[0]);
            break;
        case AST_DICT_NESTED_FINISH:
            fprintf(c_fp, "if (isNestedComplexMode()) { pushNestedComplexModeStack(getComplexMode()); reverseComplexMode(); finishComplexMode(NULL, K_ANY); }");
            break;
        case AST_POP_NESTED_COMPLEX_STACK:
            fprintf(c_fp, "popNestedComplexModeStack(\"%s\");", ast_node->strings[0]);
            break;
        case AST_LEFT_RIGHT_BRACKET_EXPRESSION:
            fprintf(c_fp, "disable_complex_mode = true;");
            if (ast_node->right->is_transpiled) {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "symbol = addSymbolInt(NULL, %s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "symbol = addSymbolFloat(NULL, %s);", ast_node->right->transpiled);
                }
            } else {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "symbol = addSymbolInt(NULL, %lld);", ast_node->right->value.i);
                } else {
                    fprintf(c_fp, "symbol = addSymbolFloat(NULL, %Lg);", ast_node->right->value.f);
                }
            }
            fprintf(c_fp, "symbol->sign = 1;");
            fprintf(c_fp, "pushLeftRightBracketStack(symbol->id);");
            fprintf(c_fp, "disable_complex_mode = false;");
            break;
        case AST_LEFT_RIGHT_BRACKET_MINUS_EXPRESSION:
            fprintf(c_fp, "disable_complex_mode = true;");
            if (ast_node->right->is_transpiled) {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "symbol = addSymbolInt(NULL, - %s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "symbol = addSymbolFloat(NULL, - %s);", ast_node->right->transpiled);
                }
            } else {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "symbol = addSymbolInt(NULL, - %lld);", ast_node->right->value.i);
                } else {
                    fprintf(c_fp, "symbol = addSymbolFloat(NULL, - %Lg);", ast_node->right->value.f);
                }
            }
            fprintf(
                c_fp,
                "symbol->sign = 1;"
                "pushLeftRightBracketStack(symbol->id);"
                "disable_complex_mode = false;"
            );
            break;
        case AST_LEFT_RIGHT_BRACKET_STRING:
            fprintf(c_fp, "disable_complex_mode = true;");
            fprintf(c_fp, "symbol = addSymbolString(NULL, \"%s\");", ast_node->value.s);
            fprintf(
                c_fp,
                "symbol->sign = 1;"
                "pushLeftRightBracketStack(symbol->id);"
                "disable_complex_mode = false;"
            );
            break;
        case AST_LEFT_RIGHT_BRACKET_VAR:
            fprintf(c_fp, "disable_complex_mode = true;");
            fprintf(c_fp, "symbol = createCloneFromSymbolByName(NULL, K_ANY, \"%s\", K_ANY);", ast_node->strings[0]);
            fprintf(
                c_fp,
                "symbol->sign = 1;"
                "pushLeftRightBracketStack(symbol->id);"
                "disable_complex_mode = false;"
            );
            break;
        case AST_LEFT_RIGHT_BRACKET_VAR_MINUS:
            fprintf(c_fp, "disable_complex_mode = true;");
            fprintf(c_fp, "symbol = createCloneFromSymbolByName(NULL, K_ANY, \"%s\", K_ANY);", ast_node->strings[0]);
            fprintf(
                c_fp,
                "symbol->sign = -1;"
                "pushLeftRightBracketStack(symbol->id);"
                "disable_complex_mode = false;"
            );
            break;
        case AST_BUILD_COMPLEX_VARIABLE:
            fprintf(c_fp, "disable_complex_mode = true;");
            fprintf(c_fp, "buildVariableComplexElement(\"%s\", NULL);", ast_node->strings[0]);
            fprintf(c_fp, "disable_complex_mode = false;");
            break;
        case AST_EXIT_SUCCESS:
            fprintf(
                c_fp,
                "freeEverything();"
                "exit(E_SUCCESS);"
            );
            break;
        case AST_EXIT_EXPRESSION:
            if (ast_node->right->is_transpiled) {
                fprintf(
                    c_fp,
                    "exit_code = %s;"
                    "freeEverything();"
                    "exit(exit_code);",
                    ast_node->right->transpiled
                );
            } else {
                fprintf(
                    c_fp,
                    "exit_code = %lld;"
                    "freeEverything();"
                    "exit(exit_code);",
                    ast_node->right->value.i
                );
            }
            break;
        case AST_EXIT_VAR:
            fprintf(
                c_fp,
                "exit_code = getSymbolValueInt(\"%s\");"
                "freeEverything();"
                "exit(exit_code);",
                ast_node->strings[0]
            );
            break;
        case AST_PRINT_FUNCTION_TABLE:
            fprintf(c_fp, "printFunctionTable();");
            break;
        case AST_FUNCTION_CALL_PARAMETERS_START:
            break;
        case AST_FUNCTION_CALL_PARAMETER_BOOL:
            fprintf(c_fp, "addFunctionCallParameterBool(%s);", ast_node->right->value.b ? "true" : "false");
            break;
        case AST_FUNCTION_CALL_PARAMETER_NUMBER:
            if (ast_node->right->is_transpiled) {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "addFunctionCallParameterInt(%s);", ast_node->right->transpiled);
                } else {
                    fprintf(c_fp, "addFunctionCallParameterFloat(%s);", ast_node->right->transpiled);
                }
            } else {
                if (ast_node->right->value_type == V_INT) {
                    fprintf(c_fp, "addFunctionCallParameterInt(%lld);", ast_node->right->value.i);
                } else {
                    fprintf(c_fp, "addFunctionCallParameterFloat(%Lg);", ast_node->right->value.f);
                }
            }
            break;
        case AST_FUNCTION_CALL_PARAMETER_STRING:
            fprintf(c_fp, "addFunctionCallParameterString(\"%s\");", ast_node->value.s);
            break;
        case AST_FUNCTION_CALL_PARAMETER_VAR:
            fprintf(c_fp, "addFunctionCallParameterSymbol(\"%s\");", ast_node->strings[0]);
            break;
        case AST_FUNCTION_CALL_PARAMETER_LIST:
            fprintf(
                c_fp,
                "reverseComplexMode();"
                "addFunctionCallParameterList(K_ANY);"
            );
            break;
        case AST_FUNCTION_CALL_PARAMETER_DICT:
            fprintf(
                c_fp,
                "reverseComplexMode();"
                "addFunctionCallParameterList(K_ANY);"
            );
            break;
        case AST_PRINT_FUNCTION_RETURN:
            if (ast_node->strings_size > 1) {
                _module = ast_node->strings[1];
            }
            compiler_function_counter++;
            if (_module == NULL) {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", NULL);", compiler_function_counter, ast_node->strings[0]);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", NULL, \"\\n\", false, true);", ast_node->strings[0]);
            } else {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", \"%s\");", compiler_function_counter, ast_node->strings[0], _module);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", \"%s\", \"\\n\", false, true);", ast_node->strings[0], _module);
            }
            break;
        case AST_ECHO_FUNCTION_RETURN:
            if (ast_node->strings_size > 1) {
                _module = ast_node->strings[1];
            }
            compiler_function_counter++;
            if (_module == NULL) {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", NULL);", compiler_function_counter, ast_node->strings[0]);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", NULL, \"\", false, true);", ast_node->strings[0]);
            } else {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", \"%s\");", compiler_function_counter, ast_node->strings[0], _module);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", \"%s\", \"\", false, true);", ast_node->strings[0], _module);
            }
            break;
        case AST_PRETTY_PRINT_FUNCTION_RETURN:
            if (ast_node->strings_size > 1) {
                _module = ast_node->strings[1];
            }
            compiler_function_counter++;
            if (_module == NULL) {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", NULL);", compiler_function_counter, ast_node->strings[0]);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", NULL, \"\\n\", true, true);", ast_node->strings[0]);
            } else {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", \"%s\");", compiler_function_counter, ast_node->strings[0], _module);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", \"%s\", \"\\n\", true, true);", ast_node->strings[0], _module);
            }
            break;
        case AST_PRETTY_ECHO_FUNCTION_RETURN:
            if (ast_node->strings_size > 1) {
                _module = ast_node->strings[1];
            }
            compiler_function_counter++;
            if (_module == NULL) {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", NULL);", compiler_function_counter, ast_node->strings[0]);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", NULL, \"\", true, true);", ast_node->strings[0]);
            } else {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", \"%s\");", compiler_function_counter, ast_node->strings[0], _module);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "printFunctionReturn(\"%s\", \"%s\", \"\", true, true);", ast_node->strings[0], _module);
            }
            break;
        case AST_FUNCTION_RETURN:
            if (ast_node->strings_size > 1) {
                _module = ast_node->strings[1];
            }
            compiler_function_counter++;
            if (_module == NULL) {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", NULL);", compiler_function_counter, ast_node->strings[0]);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "freeFunctionReturn(\"%s\", NULL);", ast_node->strings[0]);
            } else {
                fprintf(c_fp, "_Function* function_%llu = callFunction(\"%s\", \"%s\");", compiler_function_counter, ast_node->strings[0], _module);
                transpile_function_call(c_fp, _module, ast_node->strings[0]);
                fprintf(c_fp, "freeFunctionReturn(\"%s\", \"%s\");", ast_node->strings[0], _module);
            }
            break;
        case AST_NESTED_COMPLEX_TRANSITION:
            fprintf(c_fp, "reverseComplexMode();");
            break;
        case AST_JSON_PARSER:
            fprintf(
                c_fp,
                "reverseComplexMode();"
                "symbol = finishComplexMode(NULL, K_ANY);"
                "returnVariable(symbol);"
            );
            break;
        default:
            break;
    }
    fprintf(c_fp, "\n");

    return transpile_node(ast_node->next, module, c_fp, indent);
}

bool transpile_common_operator(ASTNode* ast_node, char *operator, enum ValueType left_value_type, enum ValueType right_value_type) {
    if ((ast_node->left != NULL && ast_node->left->is_transpiled) || (ast_node->right != NULL && ast_node->right->is_transpiled)) {
        if (ast_node->left != NULL && ast_node->left->is_transpiled) {
            if (in(operator, relational_operators_size)) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "(long double) %s", ast_node->left->transpiled);
            } else {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->transpiled);
            }
        } else {
            switch (left_value_type)
            {
                case V_BOOL:
                    if (in(operator, relational_operators_size)) {
                        ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->left->value.b ? 1 : 0);
                    } else {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->value.b ? "true" : "false");
                    }
                    break;
                case V_INT:
                    if (in(operator, logical_operators)) {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->value.i > 0 ? "true" : "false");
                    } else {
                        ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->left->value.i);
                    }
                    break;
                case V_FLOAT:
                    if (in(operator, logical_operators)) {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->value.f > 0.0 ? "true" : "false");
                    } else {
                        ast_node->transpiled = snprintf_concat_float(ast_node->transpiled, "%Lg", ast_node->left->value.f);
                    }
                    break;
                default:
                    break;
            }
        }
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        ast_node->transpiled = strcat_ext(ast_node->transpiled, operator);
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        if (ast_node->right != NULL && ast_node->right->is_transpiled) {
            if (in(operator, relational_operators_size)) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "(long double) %s", ast_node->right->transpiled);
            } else {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->transpiled);
            }
        } else {
            switch (right_value_type)
            {
                case V_BOOL:
                    if (in(operator, relational_operators_size)) {
                        ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->right->value.b ? 1 : 0);
                    } else {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->value.b ? "true" : "false");
                    }
                    break;
                case V_INT:
                    if (in(operator, logical_operators)) {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->value.i ? "true" : "false");
                    } else {
                        ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->right->value.i);
                    }
                    break;
                case V_FLOAT:
                    if (in(operator, logical_operators)) {
                        ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->value.f ? "true" : "false");
                    } else {
                        ast_node->transpiled = snprintf_concat_float(ast_node->transpiled, "%Lg", ast_node->right->value.f);
                    }
                    break;
                default:
                    break;
            }
        }
        ast_node->is_transpiled = true;
        return true;
    }
    return false;
}

bool transpile_common_mixed_operator(ASTNode* ast_node, char *operator) {
    if ((ast_node->left != NULL && ast_node->left->is_transpiled) || (ast_node->right != NULL && ast_node->right->is_transpiled)) {
        if (ast_node->left != NULL && ast_node->left->is_transpiled) {
            if (ast_node->left->value_type == V_INT) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "(long double) %s", ast_node->left->transpiled);
            } else {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->transpiled);
            }
        } else {
            if (ast_node->left->value_type == V_INT) {
                ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", (long double) ast_node->left->value.i);
            } else {
                ast_node->transpiled = snprintf_concat_float(ast_node->transpiled, "%Lg", ast_node->left->value.f);
            }
        }
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        ast_node->transpiled = strcat_ext(ast_node->transpiled, operator);
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        if (ast_node->right != NULL && ast_node->right->is_transpiled) {
            if (ast_node->right->value_type == V_INT) {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "(long double) %s", ast_node->right->transpiled);
            } else {
                ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->transpiled);
            }
        } else {
            if (ast_node->right->value_type == V_INT) {
                ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", (long double) ast_node->right->value.i);
            } else {
                ast_node->transpiled = snprintf_concat_float(ast_node->transpiled, "%Lg", ast_node->right->value.f);
            }
        }
        ast_node->is_transpiled = true;
        return true;
    }
    return false;
}

void transpile_function_call(FILE *c_fp, char *module, char *name) {
    char *module_context = compiler_getFunctionModuleContext(name, module);
    if (!isFunctionFromDynamicLibrary(name, module))
        fprintf(c_fp, "kaos_function_%s_%s();", module_context, name);
    _Function* function = getFunction(name, module);
    if (function->decision_node != NULL) {
        fprintf(c_fp, "kaos_decision_%s_%s();", module_context, name);
    }
    free(module_context);
    fprintf(
        c_fp,
        "callFunctionCleanUp(function_%llu, \"%s\", %s);",
        compiler_function_counter,
        name,
        function->decision_node != NULL ? "true" : "false"
    );
}

void transpile_function_call_decision(FILE *c_fp, char *module_context, char* module, char *name) {
    if (!isFunctionFromDynamicLibraryByModuleContext(name, module_context))
        fprintf(c_fp, "kaos_function_%s_%s();", module, name);
    _Function* function = getFunctionByModuleContext(name, module_context);
    if (function->decision_node != NULL) {
        fprintf(c_fp, "kaos_decision_%s_%s();", module, name);
    }
    fprintf(
        c_fp,
        "callFunctionCleanUp(function_%llu, \"%s\", %s);",
        compiler_function_counter,
        name,
        function->decision_node != NULL ? "true" : "false"
    );
}

void transpile_function_call_create_var(FILE *c_fp, ASTNode* ast_node, char *module, enum Type type1, enum Type type2) {
    compiler_function_counter++;
    switch (ast_node->strings_size)
    {
        case 2:
            fprintf(
                c_fp,
                "_Function* function_%llu = callFunction(\"%s\", NULL);",
                compiler_function_counter,
                ast_node->strings[1]
            );
            transpile_function_call(c_fp, NULL, ast_node->strings[1]);
            fprintf(
                c_fp,
                "createCloneFromFunctionReturn(\"%s\", %s, \"%s\", NULL, %s);",
                ast_node->strings[0],
                type_strings[type1],
                ast_node->strings[1],
                type_strings[type2]
            );
            break;
        case 3:
            fprintf(
                c_fp,
                "_Function* function_%llu = callFunction(\"%s\", \"%s\");",
                compiler_function_counter,
                ast_node->strings[2],
                ast_node->strings[1]
            );
            transpile_function_call(c_fp, ast_node->strings[1], ast_node->strings[2]);
            fprintf(
                c_fp,
                "createCloneFromFunctionReturn(\"%s\", %s, \"%s\", \"%s\", %s);",
                ast_node->strings[0],
                type_strings[type1],
                ast_node->strings[2],
                ast_node->strings[1],
                type_strings[type2]
            );
            break;
        default:
            break;
    }
}

void compiler_handleModuleImport(char *module_name, bool directly_import, FILE *c_fp, unsigned short indent, FILE *h_fp) {
    char *module_path = resolveModulePath(module_name, directly_import);

    char *compiled_module = malloc(1 + strlen(module_path_stack.arr[module_path_stack.size - 1]));
    strcpy(compiled_module, module_path_stack.arr[module_path_stack.size - 1]);
    compiler_escape_module(compiled_module);
    ASTNode* ast_node = ast_root_node;
    transpile_functions(ast_node, compiled_module, c_fp, indent, h_fp);
    free(compiled_module);

    moduleImportCleanUp(module_path);
}

void compiler_handleModuleImportRegister(char *module_name, bool directly_import, FILE *c_fp, unsigned short indent) {
    char *module_path = resolveModulePath(module_name, directly_import);
    if (
        strcmp(
            get_filename_ext(module_path),
            __KAOS_DYNAMIC_LIBRARY_EXTENSION__
        ) == 0
    ) {
        char *extension_name = module_stack.arr[module_stack.size - 1];

        char *spells_dir_path = malloc(1);
        strcpy(spells_dir_path, "");
        spells_dir_path = snprintf_concat_string(spells_dir_path, "%s", __KAOS_BUILD_DIRECTORY__);
        spells_dir_path = snprintf_concat_string(spells_dir_path, "%s", __KAOS_PATH_SEPARATOR__);
        spells_dir_path = snprintf_concat_string(spells_dir_path, "%s", __KAOS_SPELLS__);
        if (stat(spells_dir_path, &dir_stat) == -1) {
            printf("Creating %s directory...\n", spells_dir_path);
            #if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
                _mkdir(spells_dir_path);
            #else
                mkdir(spells_dir_path, 0700);
            #endif
        }

        char *extension_dir_path = spells_dir_path;
        extension_dir_path = snprintf_concat_string(extension_dir_path, "%s", __KAOS_PATH_SEPARATOR__);
        extension_dir_path = snprintf_concat_string(extension_dir_path, "%s", extension_name);
        if (stat(extension_dir_path, &dir_stat) == -1) {
            printf("Creating %s directory...\n", extension_dir_path);
            #if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
                _mkdir(extension_dir_path);
            #else
                mkdir(extension_dir_path, 0700);
            #endif
        }

        char *new_module_path = extension_dir_path;
        new_module_path = snprintf_concat_string(new_module_path, "%s", __KAOS_PATH_SEPARATOR__);
        new_module_path = snprintf_concat_string(new_module_path, "%s", extension_name);
        new_module_path = snprintf_concat_string(new_module_path, "%s", ".");
        new_module_path = snprintf_concat_string(new_module_path, "%s", (char*) get_filename_ext(module_path));
        printf("Copying %s to %s\n", module_path, new_module_path);
        copy_binary_file(module_path, new_module_path);

        extension_counter++;
        fprintf(
            c_fp,
            "char *extension_path_%llu = strcat_ext(getParentDir(argv0), \"%s%s%s%s%s%s.%s\");\n",
            extension_counter,
            __KAOS_PATH_SEPARATOR__,
            __KAOS_SPELLS__,
            __KAOS_PATH_SEPARATOR__,
            extension_name,
            __KAOS_PATH_SEPARATOR__,
            extension_name,
            get_filename_ext(module_path)
        );
        fprintf(c_fp, "pushModuleStack(extension_path_%llu, \"%s\");\n", extension_counter, extension_name);
        fprintf(c_fp, "callRegisterInDynamicLibrary(extension_path_%llu);\n", extension_counter);
        fprintf(c_fp, "popModuleStack();\n");
    } else {
        char *compiled_module = malloc(1 + strlen(module_path_stack.arr[module_path_stack.size - 1]));
        strcpy(compiled_module, module_path_stack.arr[module_path_stack.size - 1]);
        compiler_escape_module(compiled_module);
        ASTNode* ast_node = ast_root_node;
        compiler_register_functions(ast_node, compiled_module, c_fp, indent);
        free(compiled_module);
    }

    moduleImportCleanUp(module_path);
}

char* compiler_getCurrentContext() {
    unsigned short parent_context = 1;
    if (module_path_stack.size > 1) parent_context = 2;
    return fix_bs(module_path_stack.arr[module_path_stack.size - parent_context]);
}

char* compiler_getCurrentModuleContext() {
    return fix_bs(module_path_stack.arr[module_path_stack.size - 1]);
}

char* compiler_getCurrentModule() {
    return fix_bs(module_stack.arr[module_stack.size - 1]);
}

char* compiler_getFunctionModuleContext(char *name, char *module) {
    _Function* function = getFunction(name, module);
    char *module_context = malloc(strlen(function->module_context) + 1);
    strcpy(module_context, function->module_context);
    compiler_escape_module(module_context);
    return module_context;
}

bool isFunctionFromDynamicLibrary(char *name, char *module) {
    _Function* function = getFunction(name, module);
    return strcmp(
        get_filename_ext(function->module_context),
        __KAOS_DYNAMIC_LIBRARY_EXTENSION__
    ) == 0;
}

bool isFunctionFromDynamicLibraryByModuleContext(char *name, char *module) {
    _Function* function = getFunctionByModuleContext(name, module);
    return strcmp(
        get_filename_ext(function->module_context),
        __KAOS_DYNAMIC_LIBRARY_EXTENSION__
    ) == 0;
}

char* fix_bs(char* str) {
    char* new_str = malloc(1 + strlen(str));
    strcpy(new_str, str);
    str_replace(new_str, "\\", "\\\\");
    return new_str;
}

void compiler_escape_module(char* module) {
    module = replace_char(module, '.', '_');
    module = replace_char(module, __KAOS_PATH_SEPARATOR_ASCII__, '_');
    module = replace_char(module, '-', '_');
    module = replace_char(module, ':', '_');
    module = replace_char(module, ' ', '_');
}

void free_transpiled_functions() {
    for (unsigned i = 0; i < transpiled_functions.size; i++) {
        free(transpiled_functions.arr[i]);
    }
    if (transpiled_functions.size > 0) free(transpiled_functions.arr);
    transpiled_functions.capacity = 0;
    transpiled_functions.size = 0;
}

void free_transpiled_decisions() {
    for (unsigned i = 0; i < transpiled_decisions.size; i++) {
        free(transpiled_decisions.arr[i]);
    }
    if (transpiled_decisions.size > 0) free(transpiled_decisions.arr);
    transpiled_decisions.capacity = 0;
    transpiled_decisions.size = 0;
}
