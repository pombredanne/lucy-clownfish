/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include "CFCCClass.h"
#include "CFCClass.h"
#include "CFCDocuComment.h"
#include "CFCFunction.h"
#include "CFCMethod.h"
#include "CFCParamList.h"
#include "CFCType.h"
#include "CFCUtil.h"

static char*
S_man_create_synopsis(CFCClass *klass);

static char*
S_man_create_functions(CFCClass *klass);

static char*
S_man_create_methods(CFCClass *klass);

static char*
S_man_create_func(CFCClass *klass, CFCFunction *func, const char *short_sym,
                  const char *full_sym);

static char*
S_man_create_inheritance(CFCClass *klass);

static char*
S_man_escape_content(const char *content);

// Declare dummy host callbacks.
char*
CFCCClass_callback_decs(CFCClass *klass) {
    CFCMethod **fresh_methods = CFCClass_fresh_methods(klass);
    char       *cb_decs       = CFCUtil_strdup("");

    for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
        CFCMethod *method = fresh_methods[meth_num];

        // Define callback to NULL.
        if (CFCMethod_novel(method) && !CFCMethod_final(method)) {
            const char *override_sym = CFCMethod_full_override_sym(method);
            cb_decs = CFCUtil_cat(cb_decs, "#define ", override_sym, " NULL\n",
                                  NULL);
        }
    }

    FREEMEM(fresh_methods);

    return cb_decs;
}

char*
CFCCClass_create_man_page(CFCClass *klass) {
    const char *class_name = CFCClass_get_class_name(klass);

    CFCDocuComment *docucom = CFCClass_get_docucomment(klass);
    if (!docucom) { return NULL; }

    // Get the class's brief description.
    const char *raw_brief = CFCDocuComment_get_brief(docucom);
    char *brief = S_man_escape_content(raw_brief);

    // Get the class's long description.
    const char *raw_description = CFCDocuComment_get_long(docucom);
    char *description = S_man_escape_content(raw_description);

    // Create SYNOPSIS.
    char *synopsis = S_man_create_synopsis(klass);

    // Create CONSTRUCTORS.
    char *functions_man = S_man_create_functions(klass);

    // Create METHODS, possibly including an ABSTRACT METHODS section.
    char *methods_man = S_man_create_methods(klass);

    // Build an INHERITANCE section describing class ancestry.
    char *inheritance = S_man_create_inheritance(klass);

    // Put it all together.
    const char pattern[] =
    ".\\\" Licensed to the Apache Software Foundation (ASF) under one or more\n"
    ".\\\" contributor license agreements.  See the NOTICE file distributed with\n"
    ".\\\" this work for additional information regarding copyright ownership.\n"
    ".\\\" The ASF licenses this file to You under the Apache License, Version 2.0\n"
    ".\\\" (the \"License\"); you may not use this file except in compliance with\n"
    ".\\\" the License.  You may obtain a copy of the License at\n"
    ".\\\"\n"
    ".\\\"     http://www.apache.org/licenses/LICENSE-2.0\n"
    ".\\\"\n"
    ".\\\" Unless required by applicable law or agreed to in writing, software\n"
    ".\\\" distributed under the License is distributed on an \"AS IS\" BASIS,\n"
    ".\\\" WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
    ".\\\" See the License for the specific language governing permissions and\n"
    ".\\\" limitations under the License.\n"
    ".TH %s 3\n"
    ".SH NAME\n"
    "%s - %s\n"
    "%s"
    ".SH DESCRIPTION\n"
    "%s\n"
    "%s"
    "%s"
    "%s";
    char *man_page
        = CFCUtil_sprintf(pattern, class_name, class_name, brief, synopsis,
                          description, functions_man, methods_man,
                          inheritance);

    FREEMEM(brief);
    FREEMEM(synopsis);
    FREEMEM(description);
    FREEMEM(functions_man);
    FREEMEM(methods_man);
    FREEMEM(inheritance);

    return man_page;
}

static char*
S_man_create_synopsis(CFCClass *klass) {
    return CFCUtil_strdup("");
}

static char*
S_man_create_functions(CFCClass *klass) {
    CFCFunction **functions = CFCClass_functions(klass);
    const char  *class_name = CFCClass_get_class_name(klass);
    char        *result     = CFCUtil_strdup(".SH FUNCTIONS\n");

    for (int func_num = 0; functions[func_num] != NULL; func_num++) {
        CFCFunction *func = functions[func_num];
        if (!CFCFunction_public(func)) { continue; }

        const char *micro_sym     = CFCFunction_micro_sym(func);
        const char *full_func_sym = CFCFunction_full_func_sym(func);

        char *redman = S_man_create_func(klass, func, micro_sym,
                                         full_func_sym);
        result = CFCUtil_cat(result, redman, NULL);
        FREEMEM(redman);
    }

    return result;
}

static char*
S_man_create_methods(CFCClass *klass) {
    CFCMethod  **fresh_methods = CFCClass_fresh_methods(klass);
    const char  *class_name    = CFCClass_get_class_name(klass);
    char        *result        = CFCUtil_strdup(".SH METHODS\n");

    for (int meth_num = 0; fresh_methods[meth_num] != NULL; meth_num++) {
        CFCMethod *method = fresh_methods[meth_num];
        if (!CFCMethod_public(method)) { continue; }

        const char *macro_sym = CFCMethod_get_macro_sym(method);
        char *full_method_sym = CFCMethod_full_method_sym(method, NULL);

        char *method_man = S_man_create_func(klass, (CFCFunction*)method,
                                             macro_sym, full_method_sym);
        result = CFCUtil_cat(result, method_man, NULL);

        FREEMEM(method_man);
        FREEMEM(full_method_sym);
    }

    FREEMEM(fresh_methods);

    return result;
}

static char*
S_man_create_func(CFCClass *klass, CFCFunction *func, const char *short_sym,
                  const char *full_sym) {
    CFCType *return_type
        = CFCFunction_get_return_type(func);
    const char *return_type_c = CFCType_to_c(return_type);

    CFCParamList *param_list
        = CFCFunction_get_param_list(func);
    const char *param_list_c = CFCParamList_to_c(param_list);

    const char *pattern =
        ".TP\n"
        ".B %s\n"
        ".na\n"
        "%s\n"
        ".br\n"
        ".BR \"%s\" \"(%s);\"\n"
        ".ad\n";
    char *result = CFCUtil_sprintf(pattern, short_sym, return_type_c, full_sym,
                                   param_list_c);

    // Get documentation, which may be inherited.
    CFCDocuComment *docucomment = CFCFunction_get_docucomment(func);
    if (!docucomment) {
        const char *micro_sym = CFCFunction_micro_sym(func);
        CFCClass *parent = klass;
        while (NULL != (parent = CFCClass_get_parent(parent))) {
            CFCFunction *parent_func
                = (CFCFunction*)CFCClass_method(parent, micro_sym);
            if (!parent_func) { break; }
            docucomment = CFCFunction_get_docucomment(parent_func);
            if (docucomment) { break; }
        }
    }

    if (docucomment) {
        // Description
        const char *raw_desc = CFCDocuComment_get_description(docucomment);
        char *desc = S_man_escape_content(raw_desc);
        result = CFCUtil_cat(result, ".IP\n", desc, "\n", NULL);
        FREEMEM(desc);

        // Params
        const char **param_names
            = CFCDocuComment_get_param_names(docucomment);
        const char **param_docs
            = CFCDocuComment_get_param_docs(docucomment);
        if (param_names[0]) {
            result = CFCUtil_cat(result, ".RS\n", NULL);
            for (size_t i = 0; param_names[i] != NULL; i++) {
                char *doc = S_man_escape_content(param_docs[i]);
                result = CFCUtil_cat(result, ".TP\n.I ", param_names[i],
                                     "\n", doc, "\n", NULL);
                FREEMEM(doc);
            }
            result = CFCUtil_cat(result, ".RE\n", NULL);
        }

        // Return value
        const char *retval_doc = CFCDocuComment_get_retval(docucomment);
        if (retval_doc && strlen(retval_doc)) {
            char *doc = S_man_escape_content(retval_doc);
            result = CFCUtil_cat(result, ".IP\n.B Returns:\n", doc, "\n",
                                 NULL);
            FREEMEM(doc);
        }
    }

    return result;
}

static char*
S_man_create_inheritance(CFCClass *klass) {
    CFCClass *ancestor = CFCClass_get_parent(klass);
    char     *result   = CFCUtil_strdup("");

    if (!ancestor) { return result; }

    const char *class_name = CFCClass_get_class_name(klass);
    result = CFCUtil_cat(result, ".SH INHERITANCE\n", class_name, NULL);
    while (ancestor) {
        const char *ancestor_name = CFCClass_get_class_name(ancestor);
        result = CFCUtil_cat(result, " is a ", ancestor_name, NULL);
        ancestor = CFCClass_get_parent(ancestor);
    }
    result = CFCUtil_cat(result, ".\n", NULL);

    return result;
}

static char*
S_man_escape_content(const char *content) {
    size_t  result_len = 0;
    size_t  result_cap = strlen(content) + 20;
    char   *result     = (char*)MALLOCATE(result_cap + 1);
    int     fill       = 1;

    for (size_t i = 0; content[i]; i++) {
        char c[8];
        int  num_chars = 1;

        c[0] = content[i];

        switch (c[0]) {
            case '\\':
                c[1] = 'e';
                num_chars = 2;
                break;
            case '-':
                c[0] = '\\';
                c[1] = '-';
                num_chars = 2;
                break;
            case '\n':
                if (content[i+1] == '.') {
                    memcpy(c + num_chars, "\\&", 2);
                    num_chars += 2;
                }
                break;
            default:
                break;
        }

        if (result_len + num_chars > result_cap) {
            result_cap += 50;
            result = (char*)REALLOCATE(result, result_cap + 1);
        }

        memcpy(result + result_len, c, num_chars);
        result_len += num_chars;
    }

    result[result_len] = '\0';

    return result;
}
