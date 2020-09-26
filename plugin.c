/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Yonatan Goldschmidt
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gcc-plugin.h>
#include <tree.h>
#include <print-tree.h>
#include <tree-iterator.h>
#include <c-family/c-common.h>
#include <c-tree.h>
#include <toplev.h>
#include <stringpool.h>
#include <plugin-version.h>


#define PLUGIN_NAME "builtin_lookup_name"

int plugin_is_GPL_compatible; // must be defined for the plugin to run

static tree builtin_lookup_name = NULL_TREE;

// taken from my assert introspect plugin, https://github.com/Jongy/gcc_assert_introspect
// but with gcc_assert changed to "return NULL_TREE"
static tree get_string_cst_arg(tree function, int n) {
    tree arg = CALL_EXPR_ARG(function, n);
    if (CONVERT_EXPR_P(arg)) {
        arg = TREE_OPERAND(arg, 0);
    }
    if (TREE_CODE(arg) != ADDR_EXPR) {
        return NULL_TREE;
    }

    arg = TREE_OPERAND(arg, 0);
    if (TREE_CODE(arg) != STRING_CST) {
        return NULL_TREE;
    }

    // TREE_STRING_LENGTH should include the null terminator
    gcc_assert(TREE_STRING_POINTER(arg)[TREE_STRING_LENGTH(arg) - 1] == '\0');

    return arg;
}

static void process_maybe_builtin_lookup_name(tree *stmtp) {
    if (TREE_CODE(*stmtp) != CALL_EXPR) {
        return;
    }

    tree addr = CALL_EXPR_FN(*stmtp);
    if (TREE_CODE(addr) != ADDR_EXPR) {
        return;
    }

    tree fn = TREE_OPERAND(addr, 0);
    if (fn != builtin_lookup_name) {
        return;
    }

    tree name = get_string_cst_arg(*stmtp, 0);
    if (name == NULL_TREE) {
        error_at(EXPR_LOCATION(*stmtp), PLUGIN_NAME ": expected name string as first argument\n");
        return;
    }

    tree id = get_identifier(TREE_STRING_POINTER(name));
    // TODO: PLUGIN_FINISH_PARSE_FUNCTION does not execute in function context - so lookup does not include names
    // private to the function.
    // not that I think it matters much...
    tree target = lookup_name(id);

    tree replacement;
    tree default_ = CALL_EXPR_ARG(*stmtp, 1);
    if (target == NULL_TREE) {
        // replace with the "default" value, the second argument.
        replacement = default_;
    } else {
        // replace with the found target
        replacement = target;

        // for consistency, make sure the default value is compatible with the found value.
        // TODO doesn't work for e. integral types - because the default value is promotoed due to
        // the parameter being defined as uintptr_t.
        if (0 && 1 != comptypes(TREE_TYPE(target), TREE_TYPE(default_))) {
            // TODO: doesn't work with functions - for example, for (printf, NULL). probably because
            // the type of "printf" can't be assigned with "NULL".
            // __builtin_types_compatible_p(typeof(printf), typeof(NULL)) returns 0 indeed.
            error_at(EXPR_LOCATION(*stmtp), PLUGIN_NAME ": incompatible default type");
            return;
        }
    }

    *stmtp = replacement;
}

static void iterate_tree(tree *stmtp) {
    tree stmt = *stmtp;
    if (stmt == NULL_TREE) {
        return;
    }

    for (int i = 0; i < TREE_OPERAND_LENGTH(stmt); i++) {
        iterate_tree(&TREE_OPERAND(stmt, i));
    }

    // in `int x = my_func(5);`, the CALL_EXPR of my_func is inside DECL_INITIAL, which is
    // not a TREE_OPERAND of stmt.
    if (DECL_P(stmt) && DECL_INITIAL(stmt)) {
        iterate_tree(&DECL_INITIAL(stmt));
    }

    process_maybe_builtin_lookup_name(stmtp);
}

// taken from my assert introspect plugin, https://github.com/Jongy/gcc_assert_introspect
static void iterate_function_body(tree expr) {
    tree body;

    if (TREE_CODE(expr) == BIND_EXPR) {
        body = BIND_EXPR_BODY(expr);
    } else {
        gcc_assert(TREE_CODE(expr) == STATEMENT_LIST);
        body = expr;
    }

    if (TREE_CODE(body) == STATEMENT_LIST) {
        for (tree_stmt_iterator i = tsi_start(body); !tsi_end_p(i); tsi_next(&i)) {
            tree stmt = tsi_stmt(i);

            if (TREE_CODE(stmt) == BIND_EXPR) {
                iterate_function_body(stmt);
            } else {
                iterate_tree(&stmt);
            }
        }
    } else {
        iterate_tree(&body);
    }
}

static void plugin_start_parse_function(void *event_data, void *data)
{
    // using this hook to declare __builtin_lookup_name() once for this translation unit.

    if (builtin_lookup_name == NULL_TREE) {
        // roughly based on implicitly_declare().

        // not using default_function_type because it returns int, which results in -Wint-to-pointer-cast
        // warnings. instead, using uintptr_t.
        // this is `uintptr_t fn(const char *name, uintptr_t default_)`
        tree fntype = build_function_type_list(uintptr_type_node, const_string_type_node, uintptr_type_node, NULL_TREE);

        tree decl = build_decl(BUILTINS_LOCATION, FUNCTION_DECL, get_identifier("__builtin_lookup_name"), fntype);
        tree olddecl = pushdecl(decl); // curent lexical scope is still global (i.e not function)
        gcc_assert(olddecl == decl); // shoudln't have existed before.
        rest_of_decl_compilation(decl, 0, 0);

        builtin_lookup_name = decl;
    }
}

static void plugin_finish_parse_function(void *event_data, void *data)
{
    gcc_assert(builtin_lookup_name != NULL_TREE); // should've been called before
    tree fn = (tree)event_data;
    iterate_function_body(DECL_SAVED_TREE(fn));
}

int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
    printf(PLUGIN_NAME " loaded, compiled for GCC %s\n", gcc_version.basever);

    if (!plugin_default_version_check(version, &gcc_version)) {
        error("incompatible gcc/plugin versions");
        return 1;
    }

    register_callback(plugin_info->base_name, PLUGIN_START_PARSE_FUNCTION, plugin_start_parse_function, NULL);
    register_callback(plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION, plugin_finish_parse_function, NULL);

    return 0;
}
