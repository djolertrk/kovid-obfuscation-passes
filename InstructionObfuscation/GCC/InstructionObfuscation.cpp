/*
 * Instruction Obfuscation GCC Plugin
 * -----------------------------------
 *
 * This plugin locates GIMPLE statements of the form:
 *     x = a + b
 * and replaces them with a more "obfuscated" sequence:
 *
 *   dummy = 42 + 0
 *   temp  = dummy - 42
 *   left  = a + temp
 *   x     = left + b
 *
 * We collect all the original ADD statements first, to avoid re-transforming
 * the newly inserted statements and causing an infinite loop.
 *
 * Author: djolertrk
 * License: Apache License v2.0 with LLVM Exceptions
 */

#include <cstdio>
#include <cstring>
#include <vector>

// GCC plugin headers
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "context.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-pretty-print.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "print-tree.h"

int plugin_is_GPL_compatible;

// Basic pass data for a GIMPLE pass
static const pass_data my_pass_data = {
    GIMPLE_PASS,                // type
    "instruction_obfuscation",  // name
    OPTGROUP_NONE,              // optinfo_flags
    TV_NONE,                    // tv_id
    0,                          // properties_required
    0,                          // properties_provided
    0,                          // properties_destroyed
    0,                          // todo_flags_start
    0                           // todo_flags_finish
};

namespace {

struct instruction_obfuscation_plugin : gimple_opt_pass {
  instruction_obfuscation_plugin(gcc::context *ctx)
      : gimple_opt_pass(my_pass_data, ctx) {}

  unsigned int execute(function *) override {
    fprintf(stderr,
            "\n[instruction_obfuscation_plugin] Scanning function: %s\n",
            current_function_name());

    // We'll gather all add statements in a vector so we don't transform
    // newly inserted statements again.
    std::vector<gimple_stmt_iterator> add_stmts;

    // 1) Collect all statements of the form: X = op0 + op1
    basic_block bb;
    FOR_EACH_BB_FN(bb, cfun) {
      for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi);
           gsi_next(&gsi)) {
        gimple *stmt = gsi_stmt(gsi);
        if (gimple_code(stmt) == GIMPLE_ASSIGN &&
            gimple_assign_rhs_code(stmt) == PLUS_EXPR) {
          add_stmts.push_back(gsi);
        }
      }
    }

    // 2) Transform them (outside the main loop).
    for (gimple_stmt_iterator gsi : add_stmts) {
      // If the statement was removed or replaced in the meantime,
      // skip if it's no longer valid. (We can check gsi_end_p.)
      if (gsi_end_p(gsi))
        continue;

      gimple *stmt = gsi_stmt(gsi);
      if (!stmt)
        continue;

      // Double-check it's still an add statement:
      if (gimple_code(stmt) != GIMPLE_ASSIGN ||
          gimple_assign_rhs_code(stmt) != PLUS_EXPR) {
        continue;
      }

      tree lhs = gimple_assign_lhs(stmt);
      tree op0 = gimple_assign_rhs1(stmt);
      tree op1 = gimple_assign_rhs2(stmt);

      tree type = TREE_TYPE(lhs);
      if (!INTEGRAL_TYPE_P(type))
        continue;

      fprintf(stderr, "  Obfuscating statement: ");
      print_gimple_stmt(stderr, stmt, 0, TDF_SLIM);
      fprintf(stderr, "\n");

      // The sequence:
      //   1) dummy = 42 + 0
      //   2) temp  = dummy - 42
      //   3) left  = op0 + temp
      //   4) lhs   = left + op1

      // Step 1) dummy = 42 + 0
      tree dummy_var = create_tmp_var(type, "dummy");
      gimple *dummy_stmt = gimple_build_assign(
          dummy_var, build2(PLUS_EXPR, type, build_int_cst(type, 42),
                            build_int_cst(type, 0)));
      gsi_insert_before(&gsi, dummy_stmt, GSI_SAME_STMT);

      // Step 2) temp = dummy - 42
      tree temp_var = create_tmp_var(type, "temp");
      gimple *temp_stmt =
          gimple_build_assign(temp_var, build2(MINUS_EXPR, type, dummy_var,
                                               build_int_cst(type, 42)));
      gsi_insert_before(&gsi, temp_stmt, GSI_SAME_STMT);

      // Step 3) left = op0 + temp
      tree left_var = create_tmp_var(type, "left");
      gimple *left_stmt =
          gimple_build_assign(left_var, build2(PLUS_EXPR, type, op0, temp_var));
      gsi_insert_before(&gsi, left_stmt, GSI_SAME_STMT);

      // Step 4) final = left + op1
      gimple *final_stmt =
          gimple_build_assign(lhs, build2(PLUS_EXPR, type, left_var, op1));

      // Replace the original statement
      gsi_replace(&gsi, final_stmt, true);
    }

    // We transformed statements, no preservation
    return 0;
  }
};

} // end anonymous namespace

// --------------------------------------------------------------------------
// plugin_init: register the pass and the plugin info
// --------------------------------------------------------------------------
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  // Check GCC version compatibility
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr,
            "Instruction Obfuscation Plugin: Incompatible GCC version\n");
    return 1;
  }

  // For '-fplugin-info' diagnostic
  static struct plugin_info my_plugin_info = {
      .version = "1.0",
      .help = "Obfuscates ADD instructions into multi-step arithmetic."};
  register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr,
                    &my_plugin_info);

  // Create and register our pass
  instruction_obfuscation_plugin *io_pass =
      new instruction_obfuscation_plugin(g);

  struct register_pass_info pass_info;
  pass_info.pass = io_pass;
  pass_info.reference_pass_name = "cfg"; // Insert after 'cfg' pass
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr,
                    &pass_info);

  fprintf(stderr, "KoviD Instruction Obfuscation Plugin loaded.\n");
  return 0;
}
