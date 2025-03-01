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
  fprintf(
      stderr,
      "[RemoveMetadataUnusedCode] Checking for unused local functions...\n");

  // We'll gather the cgraph_node + symtab_node pairs to remove
  std::vector<std::pair<cgraph_node *, symtab_node *>> to_remove;

  // Iterate over all symtab nodes
  for (symtab_node *snode = symtab->nodes; snode; snode = snode->next) {
    // 1) Check if 'decl' is a function (FUNCTION_DECL)
    if (!snode->decl || TREE_CODE(snode->decl) != FUNCTION_DECL)
      continue;

    // 2) Get the corresponding cgraph_node from this decl
    cgraph_node *cnode = cgraph_node::get(snode->decl);
    if (!cnode)
      continue;

    // Only consider local function definitions that can be discarded
    // (i.e., not externally visible or required).
    if (!cnode->definition || !cnode->can_be_discarded_p())
      continue;

    // "No callers" => cnode->callers == nullptr
    bool no_callers = (cnode->callers == nullptr);

    // "Not address-taken"
    bool not_address_taken = !cnode->address_taken;

    // If both conditions hold, mark it for removal
    if (no_callers && not_address_taken)
      to_remove.emplace_back(cnode, snode);
  }

  // Actually remove them
  for (auto &pair : to_remove) {
    cgraph_node *cnode = pair.first;
    symtab_node *snode = pair.second;

    // Print out function name
    const char *name = nullptr;
    if (cnode->decl)
      name = get_name(cnode->decl);

    fprintf(stderr, "  Removing unused function: %s\n",
            name ? name : "(unknown)");

    // 1) Remove from the call graph
    cnode->remove();

    // 2) Remove from the global symbol table
    snode->remove();
  }
}

// -----------------------------------------------------------------------------
// plugin_init
// -----------------------------------------------------------------------------

int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  // 0) Basic version check
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr, "RemoveMetadataUnusedCode: Incompatible GCC version\n");
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

  fprintf(stderr, "KoviD RemoveMetadataUnusedCode Plugin loaded.\n");
  return 0;
}
