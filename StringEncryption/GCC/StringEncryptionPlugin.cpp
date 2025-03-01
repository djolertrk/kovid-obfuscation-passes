/*
 * In-Place XOR of STRING_CST data via a GIMPLE Pass (GCC 12-compatible)
 *
 * This plugin finds global variables with string initializers
 * and XORs the data in place **early** in the pipeline,
 * ensuring the emitted object file has the XORed data.
 *
 * Author: djolertrk
 */

#include <cstdio>
#include <cstring>
#include <string>

// GCC plugin headers:
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree.h"
#include "cp/cp-tree.h"
#include "context.h"
#include "tree-pass.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "config.h"
#include "print-tree.h"
#include "tree-pretty-print.h"

int plugin_is_GPL_compatible;

// For demonstration, define an XOR key unless overridden at compile time:
//   -DSTR_GCC_CRYPTO_KEY=\"464e40d1dce5c98d\"
#ifndef STR_GCC_CRYPTO_KEY
#define STR_GCC_CRYPTO_KEY "default_key"
#endif

// --------------------------------------------------------------------------
// A simple XOR loop.
// --------------------------------------------------------------------------
static void xor_inplace(char *data, int length, const char *key,
                        size_t keylen) {
  for (int i = 0; i < length; i++)
    data[i] = data[i] ^ key[i % keylen];
}

// --------------------------------------------------------------------------
// Mutate the STRING_CST contents in place by XORing them with
// STR_GCC_CRYPTO_KEY.
// --------------------------------------------------------------------------
static void mutate_string_cst(tree cst_node) {
  if (!cst_node || TREE_CODE(cst_node) != STRING_CST)
    return;

  const int length = TREE_STRING_LENGTH(cst_node);
  if (length <= 0)
    return;

  // Access the array from the embedded union inside STRING_CST.
  char *array_ptr = &STRING_CST_CHECK(cst_node)->string.str[0];

  const char *key = STR_GCC_CRYPTO_KEY;
  fprintf(stderr, "    Using key: %s\n", key);

  // XOR in place:
  xor_inplace(array_ptr, length, STR_GCC_CRYPTO_KEY,
              strlen(STR_GCC_CRYPTO_KEY));
}

// --------------------------------------------------------------------------
// Recursively walk an initializer, XORing any STRING_CST found.
// --------------------------------------------------------------------------
static void scan_initializer(tree init) {
  if (!init)
    return;

  switch (TREE_CODE(init)) {
  case STRING_CST:
    fprintf(stderr, "  Found STRING_CST:\n");
    fprintf(stderr, "    Before XOR: ");
    print_generic_expr(stderr, init, TDF_NONE);
    fprintf(stderr, "\n");

    mutate_string_cst(init);

    fprintf(stderr, "    After XOR:  ");
    print_generic_expr(stderr, init, TDF_NONE);
    fprintf(stderr, "\n");
    break;

  case CONSTRUCTOR: {
    // Recursively scan each element of a constructor.
    unsigned n = CONSTRUCTOR_NELTS(init);
    for (unsigned i = 0; i < n; i++) {
      constructor_elt *elt = CONSTRUCTOR_ELT(init, i);
      if (elt)
        scan_initializer(elt->value);
    }
    break;
  }

  // If the initializer is something like &"some string", or a cast,
  // we can scan its operand as well.
  case ADDR_EXPR:
  case NOP_EXPR:
  case BIT_CAST_EXPR:
  case CONVERT_EXPR: {
    tree op = TREE_OPERAND(init, 0);
    scan_initializer(op);
    break;
  }

  default:
    // Not handling other node codes
    break;
  }
}

// --------------------------------------------------------------------------
// A GIMPLE pass that scans **global** variables once, early in the pass
// pipeline.
// --------------------------------------------------------------------------
namespace {

static const pass_data my_pass_data = {.type = GIMPLE_PASS,
                                       .name = "string_encryption_plugin",
                                       .optinfo_flags =
                                           OPTGROUP_NONE, // Instead of '0'
                                       .tv_id = TV_NONE,  // Instead of '0'
                                       .properties_required = 0,
                                       .properties_provided = 0,
                                       .properties_destroyed = 0,
                                       .todo_flags_start = 0,
                                       .todo_flags_finish = 0};

struct string_encryption_plugin : gimple_opt_pass {
  // We use a static boolean so we only scan once, even though
  // the pass might be invoked for multiple functions.
  static bool done_global_scan;

  string_encryption_plugin(gcc::context *ctx)
      : gimple_opt_pass(my_pass_data, ctx) {}

  unsigned int execute(function *) override {
    // Only do this once to handle top-level (global) initializers.
    if (!done_global_scan) {
      done_global_scan = true;

      fprintf(stderr,
              "\n[string_encryption_plugin] Scanning global variables...\n");

      varpool_node *vnode;
      FOR_EACH_VARIABLE(vnode) {
        tree decl = vnode->decl;
        if (!decl)
          continue;

        tree init = DECL_INITIAL(decl);
        if (!init)
          continue;

        const char *name =
            (DECL_NAME(decl) ? IDENTIFIER_POINTER(DECL_NAME(decl))
                             : "<unknown>");
        fprintf(stderr, "  XORing strings in global: %s\n", name);

        scan_initializer(init);
      }
    }
    return 0; // no function-body modifications
  }
};

bool string_encryption_plugin::done_global_scan = false;

} // end anonymous namespace

// --------------------------------------------------------------------------
// plugin_init: register the pass and the plugin info
// --------------------------------------------------------------------------
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  // Basic GCC version compatibility check:
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr, "In-Place String XOR Plugin: Incompatible GCC version\n");
    return 1;
  }

  // For '-fplugin-info' diagnostic
  static struct plugin_info my_plugin_info = {
      .version = "1.0",
      .help = "XORs global STRING_CST data in place (GIMPLE pass)."};
  register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, &my_plugin_info);

  // Create and register our pass. We do NOT directly access 'pass_manager' in
  // GCC 12+; we use 'PLUGIN_PASS_MANAGER_SETUP' with a register_pass_info
  // struct.
  string_encryption_plugin *xor_pass = new string_encryption_plugin(g);

  struct register_pass_info pass_info;
  pass_info.pass = xor_pass;
  // Insert after the "cfg" pass as an example; you could choose another
  // reference pass.
  pass_info.reference_pass_name = "cfg";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
                    /* callback = */ NULL,
                    /* user_data = */ &pass_info);

  fprintf(stderr, "KoviD String XOR Plugin (early pass) loaded.\n");
  return 0;
}
