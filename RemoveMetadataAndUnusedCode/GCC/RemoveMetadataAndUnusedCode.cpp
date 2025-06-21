/*
 * Remove Metadata & Unused Code GCC Plugin
 * ----------------------------------------
 *
 * 1) Disables debug info (no DWARF).
 * 2) Clears statement locations in each function (strips line info).
 * 3) Removes local unused functions (no callers, not address-taken, etc.).
 *
 * Author: djolertrk
 * License: Apache v2.0 with LLVM-exception
 */

#include <cstdio>
#include <cstring>
#include <vector>

// Disable implicit inlining for this plugin
// #pragma GCC optimize ("no-inline")

// GCC plugin headers
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "context.h"
#include "opts.h"
#include "tree-pass.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-pretty-print.h"
#include "basic-block.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "print-tree.h"
#include "symtab.h"
#include "function.h"

int plugin_is_GPL_compatible;

// -----------------------------------------------------------------------------
// 1) A GIMPLE pass that clears statement locations in each function
// -----------------------------------------------------------------------------

static const pass_data dbg_removal_pass_data = {
    GIMPLE_PASS,          // type
    "rm_dbg_info_plugin", // name
    OPTGROUP_NONE,        TV_NONE, 0, 0, 0, 0, 0};

namespace {

struct rm_dbg_info_pass : gimple_opt_pass {
  rm_dbg_info_pass(gcc::context *ctx)
      : gimple_opt_pass(dbg_removal_pass_data, ctx) {}

  unsigned int execute(function *) override {
    if (!cfun)
      return 0;

    // For each statement in each basic block, set location to UNKNOWN_LOCATION
    basic_block bb;
    FOR_ALL_BB_FN(bb, cfun) {
      for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi);
           gsi_next(&gsi)) {
        gimple *stmt = gsi_stmt(gsi);
        gimple_set_location(stmt, UNKNOWN_LOCATION);
      }
    }

    // Also clear the function's DECL source location
    if (cfun->decl)
      DECL_SOURCE_LOCATION(cfun->decl) = BUILTINS_LOCATION;

    return 0;
  }
};

} // end anonymous namespace

// -----------------------------------------------------------------------------
// 2) Immediately disable debug info generation
// -----------------------------------------------------------------------------

static void disable_global_debug_info() {
  debug_info_level = DINFO_LEVEL_NONE; // Turn off all debug info
  write_symbols = NO_DEBUG;
}

// -----------------------------------------------------------------------------
// 3) Callback at PLUGIN_FINISH_UNIT: remove unused local functions
//    i.e., if they have no callers, aren't address-taken, etc.
// -----------------------------------------------------------------------------

static void remove_unused_local_functions(void *, void *) {
#ifdef DEBUG_OUTPUT
  fprintf(
      stderr,
      "[RemoveMetadataUnusedCode] Checking for unused local functions...\n");
#endif

  // Use a fixed-size array instead of std::vector to avoid ABI issues
  const int MAX_REMOVABLE_FUNCTIONS = 1024;
  struct RemovePair {
    cgraph_node *cnode;
  };
  RemovePair to_remove[MAX_REMOVABLE_FUNCTIONS];
  int to_remove_count = 0;

  // Protect the whole function with try-catch to prevent crashes
  try {
    // First pass: collect nodes that need to be removed
    // Use cgraph_node_for_each_function to safely iterate
    cgraph_node *node;
    FOR_EACH_FUNCTION(node) {
      // Skip invalid nodes
      if (!node || !node->decl)
        continue;

      // Only consider local function definitions that can be discarded
      // (i.e., not externally visible or required).
      if (!node->definition || !node->can_be_discarded_p())
        continue;

      // "No callers" => node->callers == nullptr
      bool no_callers = (node->callers == nullptr);

      // "Not address-taken"
      bool not_address_taken = !node->address_taken;

      // Additional safety checks
      if (DECL_EXTERNAL(node->decl) || TREE_PUBLIC(node->decl))
        continue;

      // If both conditions hold, mark it for removal
      if (no_callers && not_address_taken && to_remove_count < MAX_REMOVABLE_FUNCTIONS) {
        to_remove[to_remove_count].cnode = node;
        to_remove_count++;
      }
    }

    // Actually remove them (only from the call graph)
    for (int i = 0; i < to_remove_count; i++) {
      cgraph_node *node = to_remove[i].cnode;
      
      // Skip invalid nodes
      if (!node)
        continue;

      // Print out function name in debug mode
#ifdef DEBUG_OUTPUT
      const char *name = nullptr;
      if (node->decl)
        name = get_name(node->decl);
      fprintf(stderr, "  Removing unused function: %s\n",
              name ? name : "(unknown)");
#endif

      try {
        // Only remove from the call graph
        node->remove();
      } catch (...) {
#ifdef DEBUG_OUTPUT
        fprintf(stderr, "  Error removing function\n");
#endif
      }
    }
  } catch (...) {
    // Catch any exceptions to prevent plugin crashes
#ifdef DEBUG_OUTPUT
    fprintf(stderr, "Exception caught in remove_unused_local_functions\n");
#endif
  }
}

// -----------------------------------------------------------------------------
// plugin_init
// -----------------------------------------------------------------------------

int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  // 0) Basic version check
  if (!plugin_default_version_check(version, &gcc_version)) {
  #ifdef DEBUG_OUTPUT
  fprintf(stderr, "RemoveMetadataUnusedCode: Incompatible GCC version\n");
#endif
    return 1;
  }

  // Provide plugin info
  static struct plugin_info my_plugin_info = {
      .version = "1.0", .help = "Removes debug info & unused local functions"};
  register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr,
                    &my_plugin_info);

  // 1) Immediately disable debug info
  disable_global_debug_info();

  // 2) Register the pass that clears statement locations in each function
  rm_dbg_info_pass *pass_obj = new rm_dbg_info_pass(g);
  struct register_pass_info pass_info;
  pass_info.pass = pass_obj;
  pass_info.reference_pass_name = "cfg";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr,
                    &pass_info);

  // 3) At the end of compilation, remove unused local functions
  register_callback(plugin_info->base_name, PLUGIN_FINISH_UNIT,
                    remove_unused_local_functions, nullptr);

#ifdef DEBUG_OUTPUT
  fprintf(stderr, "KoviD RemoveMetadataUnusedCode Plugin loaded.\n");
#endif
  return 0;
}
