#include "compiler.h"

struct stat dir_stat = {0};

unsigned short indent_length = 4;

void compile(char *module, enum Phase phase_arg, char *bin_file) {
    printf("Starting compiling...\n");
    ASTNode* ast_node = ast_root_node;

    if (stat(__KAOS_BUILD_DIRECTORY__, &dir_stat) == -1) {
        printf("Creating build directory...\n");
        #if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
            _mkdir(__KAOS_BUILD_DIRECTORY__);
        #else
            mkdir(__KAOS_BUILD_DIRECTORY__, 0700);
        #endif
    }

    char c_file_path[PATH_MAX];
    char h_file_path[PATH_MAX];
    sprintf(c_file_path, "%s%smain.c", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
    sprintf(h_file_path, "%s%smain.h", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);

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
        string_uppercase(bin_file);
        fprintf(h_fp, "#ifndef %s_H\n", bin_file);
        fprintf(h_fp, "#define %s_H\n\n", bin_file);
    } else {
        fprintf(h_fp, "#ifndef MAIN_H\n");
        fprintf(h_fp, "#define MAIN_H\n\n");
    }

    const char *c_file_base =
        "#include \"main.h\"\n\n"
        "int main() {\n";

    const char *h_file_base =
        "#include <stdio.h>\n"
        "#include <stdbool.h>\n\n"
        "bool is_interactive = false;\n"
        "#include \"interpreter/symbol.h\"\n\n";

    unsigned short indent = indent_length;

    fprintf(c_fp, "%s", c_file_base);
    fprintf(h_fp, "%s", h_file_base);

    fprintf(c_fp, "%*cinitMainFunction();\n", indent, ' ');

    transpile_node(ast_node, module, c_fp, indent);

    fprintf(c_fp, "}\n");
    fprintf(h_fp, "#endif\n");

    fclose(c_fp);
    fclose(h_fp);

    printf("Compiling the C code into machine code...\n");

    char bin_file_path[PATH_MAX];
    if (bin_file != NULL) {
        sprintf(bin_file_path, "%s%s%s", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__, bin_file);
    } else {
        sprintf(bin_file_path, "%s%smain", __KAOS_BUILD_DIRECTORY__, __KAOS_PATH_SEPARATOR__);
    }

    char c_compiler_path[PATH_MAX];
    #if defined(__clang__)
        sprintf(c_compiler_path, "clang");
    #elif defined(__GNUC__) || defined(__GNUG__)
        sprintf(c_compiler_path, "gcc");
    #endif

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    STARTUPINFO info={sizeof(info)};
    PROCESS_INFORMATION processInfo;
    DWORD status;

    char cmd[PATH_MAX];
    sprintf(cmd, "/c %s -o %s %s", c_compiler_path, bin_file_path, c_file_path);
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

    if ((pid = fork()) == -1)
        perror("fork error");
    else if (pid == 0)
        execlp(
            c_compiler_path,
            c_compiler_path,
            "-DCHAOS_COMPILER",
            "-o",
            bin_file_path,
            c_file_path,
            "/usr/local/include/chaos/utilities/helpers.c",
            "/usr/local/include/chaos/ast/ast.c",
            "/usr/local/include/chaos/interpreter/errors.c",
            "/usr/local/include/chaos/interpreter/extension.c",
            "/usr/local/include/chaos/interpreter/function.c",
            "/usr/local/include/chaos/interpreter/module.c",
            "/usr/local/include/chaos/interpreter/symbol.c",
            "/usr/local/include/chaos/compiler/compiler.c",
            "/usr/local/include/chaos/compiler/lib/alternative.c",
            "/usr/local/include/chaos/Chaos.c",
            "-lreadline",
            "-L/usr/local/opt/readline/lib",
            "-ldl",
            "-I/usr/local/include/chaos/",
            NULL
        );

    int status;
    pid_t wait_result;

    while ((wait_result = wait(&status)) != -1)
    {
        // printf("Process %lu returned result: %d\n", (unsigned long) wait_result, status);
        if (status != 0) {
            printf("Compilation of %s is failed!\n", c_file_path);
            exit(status);
        }
    }
#endif

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

ASTNode* transpile_node(ASTNode* ast_node, char *module, FILE *c_fp, unsigned short indent) {
    if (ast_node == NULL) {
        return ast_node;
    }

    if (ast_node->depend != NULL) {
        transpile_node(ast_node->depend, module, c_fp, indent);
    }

    if (ast_node->right != NULL) {
        transpile_node(ast_node->right, module, c_fp, indent);
    }

    if (ast_node->left != NULL) {
        transpile_node(ast_node->left, module, c_fp, indent);
    }

    fprintf(c_fp, "%*c", indent, ' ');
    switch (ast_node->node_type)
    {
        case AST_VAR_CREATE_BOOL:
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolBool(NULL, %s);", ast_node->right->value.b ? "true" : "false");
            } else {
                fprintf(c_fp, "addSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
            }
            break;
        case AST_VAR_CREATE_BOOL_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_BOOL, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_BOOL, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_BOOL, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_BOOL, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
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
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_NUMBER, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_NUMBER, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_STRING:
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolString(NULL, \"%s\");", escape_string_literal_for_transpiler(ast_node->value.s));
            } else {
                fprintf(c_fp, "addSymbolString(\"%s\", \"%s\");", ast_node->strings[0], escape_string_literal_for_transpiler(ast_node->value.s));
            }
            break;
        case AST_VAR_CREATE_STRING_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_STRING, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_STRING, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_STRING, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_STRING, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
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
            if (ast_node->strings[0] == NULL) {
                fprintf(c_fp, "addSymbolAnyString(NULL, \"%s\");", escape_string_literal_for_transpiler(ast_node->value.s));
            } else {
                fprintf(c_fp, "addSymbolAnyString(\"%s\", \"%s\");", ast_node->strings[0], escape_string_literal_for_transpiler(ast_node->value.s));
            }
            break;
        case AST_VAR_CREATE_ANY_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_ANY, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_ANY_VAR_EL:
            fprintf(c_fp, "createCloneFromComplexElement(\"%s\", K_ANY, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_ANY_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_ANY, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_ANY, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_LIST_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_ANY);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_ANY);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_DICT_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", NULL, K_ANY);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", \"%s\", K_ANY);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_BOOL_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_BOOL_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_BOOL);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_LIST_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", NULL, K_BOOL);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", \"%s\", K_BOOL);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_BOOL_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_BOOL);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_BOOL_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_BOOL);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_BOOL_DICT_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", NULL, K_BOOL);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", \"%s\", K_BOOL);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_NUMBER_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_NUMBER_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_NUMBER);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_LIST_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", NULL, K_NUMBER);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", \"%s\", K_NUMBER);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_NUMBER_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_NUMBER);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_NUMBER_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_NUMBER);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_NUMBER_DICT_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", NULL, K_NUMBER);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", \"%s\", K_NUMBER);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_STRING_LIST:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_STRING_LIST_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_LIST, \"%s\", K_STRING);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_LIST_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", NULL, K_STRING);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_LIST, \"%s\", \"%s\", K_STRING);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_CREATE_STRING_DICT:
            fprintf(c_fp, "reverseComplexMode(); finishComplexMode(\"%s\", K_STRING);", ast_node->strings[0]);
            break;
        case AST_VAR_CREATE_STRING_DICT_VAR:
            fprintf(c_fp, "createCloneFromSymbolByName(\"%s\", K_DICT, \"%s\", K_STRING);", ast_node->strings[0], ast_node->strings[1]);
            break;
        case AST_VAR_CREATE_STRING_DICT_FUNC_RETURN:
            switch (ast_node->strings_size)
            {
                case 2:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", NULL); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", NULL, K_STRING);",
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[1]
                    );
                    break;
                case 3:
                    fprintf(
                        c_fp,
                        "callFunction(\"%s\", \"%s\"); createCloneFromFunctionReturn(\"%s\", K_DICT, \"%s\", \"%s\", K_STRING);",
                        ast_node->strings[2],
                        ast_node->strings[1],
                        ast_node->strings[0],
                        ast_node->strings[2],
                        ast_node->strings[1]
                    );
                    break;
                default:
                    break;
            }
            break;
        case AST_VAR_UPDATE_BOOL:
            fprintf(c_fp, "updateSymbolBool(\"%s\", %s);", ast_node->strings[0], ast_node->right->value.b ? "true" : "false");
            break;
        case AST_PRINT_VAR:
            fprintf(c_fp, "printSymbolValueEndWithNewLine(getSymbol(\"%s\"), false, true);", ast_node->strings[0]);
            break;
        case AST_PRINT_EXPRESSION:
            fprintf(c_fp, "printf(\"%lld\\n\");", ast_node->right->value.i);
            break;
        case AST_PRINT_MIXED_EXPRESSION:
            fprintf(c_fp, "printf(\"%Lg\\n\");", ast_node->right->value.f);
            break;
        case AST_PRINT_STRING:
            fprintf(c_fp, "printf(\"%s\\n\");", escape_string_literal_for_transpiler(ast_node->value.s));
            break;
        case AST_EXPRESSION_PLUS:
            if (!transpile_common_operator_ii(ast_node, "+"))
                ast_node->value.i = ast_node->left->value.i + ast_node->right->value.i;
            break;
        case AST_EXPRESSION_MINUS:
            if (!transpile_common_operator_ii(ast_node, "-"))
                ast_node->value.i = ast_node->left->value.i - ast_node->right->value.i;
            break;
        case AST_EXPRESSION_MULTIPLY:
            if (!transpile_common_operator_ii(ast_node, "*"))
                ast_node->value.i = ast_node->left->value.i * ast_node->right->value.i;
            break;
        case AST_EXPRESSION_BITWISE_AND:
            if (!transpile_common_operator_ii(ast_node, "&"))
                ast_node->value.i = ast_node->left->value.i & ast_node->right->value.i;
            break;
        case AST_EXPRESSION_BITWISE_OR:
            if (!transpile_common_operator_ii(ast_node, "|"))
                ast_node->value.i = ast_node->left->value.i | ast_node->right->value.i;
            break;
        case AST_EXPRESSION_BITWISE_XOR:
            if (!transpile_common_operator_ii(ast_node, "^"))
                ast_node->value.i = ast_node->left->value.i ^ ast_node->right->value.i;
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
            if (!transpile_common_operator_ii(ast_node, "<<"))
                ast_node->value.i = ast_node->left->value.i << ast_node->right->value.i;
            break;
        case AST_EXPRESSION_BITWISE_RIGHT_SHIFT:
            if (!transpile_common_operator_ii(ast_node, ">>"))
                ast_node->value.i = ast_node->left->value.i >> ast_node->right->value.i;
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
        default:
            break;
    }
    fprintf(c_fp, "\n");

    return transpile_node(ast_node->next, module, c_fp, indent);
}

bool transpile_common_operator_ii(ASTNode* ast_node, char *operator) {
    ast_node->value_type = V_INT;
    if (ast_node->left->is_transpiled || ast_node->right->is_transpiled) {
        if (ast_node->left->is_transpiled) {
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->left->transpiled);
        } else {
            ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->left->value.i);
        }
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        ast_node->transpiled = strcat_ext(ast_node->transpiled, operator);
        ast_node->transpiled = strcat_ext(ast_node->transpiled, " ");
        if (ast_node->right->is_transpiled) {
            ast_node->transpiled = snprintf_concat_string(ast_node->transpiled, "%s", ast_node->right->transpiled);
        } else {
            ast_node->transpiled = snprintf_concat_int(ast_node->transpiled, "%lld", ast_node->right->value.i);
        }
        ast_node->is_transpiled = true;
        return true;
    }
    return false;
}
