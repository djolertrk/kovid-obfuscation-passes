/*
 * Dummy Code Insertion GCC Plugin (Skip "a" Hack)
 * -----------------------------------------------
 *
 * This plugin injects dummy code into each function that has at least
 * two statements, EXCEPT it specifically checks if the function
 * is named "a" and skips it (to avoid a known ICE).
 *
 * Author: djolertrk
 * License: Apache v2.0 with LLVM-exception
 */

#include <cstdio>
#include <cstring>

// GCC plugin headers
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "context.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-pretty-print.h"
#include "basic-block.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "print-tree.h"
#include "opts.h"

int plugin_is_GPL_compatible;
extern struct gcc_options global_options;

// Basic pass data for a GIMPLE pass
static const pass_data my_pass_data = {
    GIMPLE_PASS,            // type
    "dummy_code_insertion", // name
    OPTGROUP_NONE,          // optinfo_flags
    TV_NONE,                // tv_id
    0,                      // properties_required
    0,                      // properties_provided
    0,                      // properties_destroyed
    0,                      // todo_flags_start
    0                       // todo_flags_finish
};

namespace {

struct dummy_code_insertion_plugin : gimple_opt_pass {
  dummy_code_insertion_plugin(gcc::context *ctx)
      : gimple_opt_pass(my_pass_data, ctx) {}

  unsigned int execute(function *) override {
    // If it's an external decl, skip
    if (DECL_EXTERNAL(cfun->decl))
      return 0;

    // Get the cgraph node for this function
    cgraph_node *node = cgraph_node::get(cfun->decl);
    if (!node)
      return 0;

    fprintf(stderr, "Crrent Function: ");
    print_generic_expr(stderr, cfun->decl, TDF_NONE);
    fprintf(stderr, "...\n");

    // We'll count all real statements
    size_t total_stmts = 0;
    // We'll also store the first block that actually has statements
    basic_block first_real_bb = nullptr;

    // The artificial blocks typically have indexes < NUM_FIXED_BLOCKS.
    int last_bb_index = last_basic_block_for_fn(cfun);
    for (int i = NUM_FIXED_BLOCKS; i < last_bb_index; ++i) {
      basic_block bb = BASIC_BLOCK_FOR_FN(cfun, i);
      if (!bb)
        continue;

      for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi);
           gsi_next(&gsi)) {
        total_stmts++;
        if (!first_real_bb)
          first_real_bb = bb;
      }
    }

    // If fewer than 2 statements, skip to avoid ICE in trivial funcs
    if (total_stmts < 2)
      return 0;

    // Insert dummy code at the start of the block that had the first stmt.
    if (!first_real_bb)
      return 0;

    // Build a volatile int type
    tree volatile_int_type =
        build_qualified_type(integer_type_node, TYPE_QUAL_VOLATILE);

    // Create local var "dummy"
    tree dummy_var = create_tmp_var(volatile_int_type, "dummy");
    // Force memory-based storage
    TREE_ADDRESSABLE(dummy_var) = 1;

    // Insert at the front
    gimple_stmt_iterator gsi = gsi_start_bb(first_real_bb);

    // 1) dummy = 0
    gimple *set0 =
        gimple_build_assign(dummy_var, build_int_cst(volatile_int_type, 0));
    gsi_insert_before(&gsi, set0, GSI_SAME_STMT);

    // 2) dummy = dummy + 1
    {
      tree plus_expr = build2(PLUS_EXPR, volatile_int_type, dummy_var,
                              build_int_cst(volatile_int_type, 1));
      gimple *add1 = gimple_build_assign(dummy_var, plus_expr);
      gsi_insert_before(&gsi, add1, GSI_SAME_STMT);
    }

    // 3) dummy = dummy - 1
    {
      tree minus_expr = build2(MINUS_EXPR, volatile_int_type, dummy_var,
                               build_int_cst(volatile_int_type, 1));
      gimple *sub1 = gimple_build_assign(dummy_var, minus_expr);
      gsi_insert_before(&gsi, sub1, GSI_SAME_STMT);
    }

    return 0; // no analysis preserved
  }
};

} // end anonymous namespace

// --------------------------------------------------------------------------
// plugin_init: register the pass and the plugin info
// --------------------------------------------------------------------------
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr, "Dummy Code Insertion: Incompatible GCC version\n");
    return 1;
  }

  fprintf(stderr, "KoviD Dummy Code Insertion Plugin loaded.\n");

  int optLevel = global_options.x_optimize;
  fprintf(stderr,
          "KoviD Dummy Code Insertion: Detected optimization level: -O%d\n",
          optLevel);

  if (optLevel == 0) {
    fprintf(stderr, "NOTE: Use it with -O1 and higher.\n");
    return 0;
  }

  static struct plugin_info my_plugin_info = {
      .version = "1.0", .help = "Inserts dummy code in non-trivial functions."};
  register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr,
                    &my_plugin_info);

  dummy_code_insertion_plugin *dummy_pass = new dummy_code_insertion_plugin(g);

  struct register_pass_info pass_info;
  pass_info.pass = dummy_pass;
  pass_info.reference_pass_name = "cfg";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr,
                    &pass_info);

  return 0;
}
