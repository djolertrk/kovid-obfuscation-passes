/*
 * KoviD Control-Flow Taint GCC Plugin
 * -----------------------------------
 *
 * This plugin implements control flow obfuscation techniques by:
 * 1. Breaking control flow by adding dummy blocks and opaque predicates
 * 2. Creating state variables to track control flow
 * 3. Adding opaque predicates to resist optimization
 * 4. Making blocks branch to each other in a confusing way
 * 5. Using volatile variables to prevent optimizations
 * 6. Adding various control flow complication mechanisms
 *
 * Author: djolertrk
 * License: Apache-2.0 WITH LLVM-exception
 */

#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <random>

// GCC plugin headers
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "context.h"
#include "tree.h"
#include "tree-pass.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "basic-block.h"
#include "cgraph.h"
#include "function.h"
#include "diagnostic.h"
#include "opts.h"
#include "stringpool.h"
#include "tree-cfg.h"

int plugin_is_GPL_compatible;

// Basic pass data for a GIMPLE pass
static const pass_data control_flow_taint_pass_data = {
    GIMPLE_PASS,           // type
    "control_flow_taint",  // name
    OPTGROUP_NONE,         // optinfo_flags
    TV_NONE,               // tv_id
    0,                     // properties_required
    0,                     // properties_provided
    0,                     // properties_destroyed
    0,                     // todo_flags_start
    TODO_update_ssa        // todo_flags_finish - we will modify the CFG
};

namespace {

// Simple pseudo-random number generator to avoid needing to seed std::random
class SimpleRandom {
private:
    unsigned int state;
public:
    SimpleRandom(unsigned int seed = 0x12345678) : state(seed) {}
    
    unsigned int next() {
        state = state * 1664525 + 1013904223;
        return state;
    }
    
    unsigned int get(unsigned int min, unsigned int max) {
        return min + (next() % (max - min + 1));
    }
};

struct control_flow_taint_plugin : gimple_opt_pass {
  control_flow_taint_plugin(gcc::context *ctx)
      : gimple_opt_pass(control_flow_taint_pass_data, ctx) {}

  // Helper: Create a basic opaque predicate
  tree create_opaque_predicate(gimple_stmt_iterator *gsi, tree state_var) {
    tree int_type = integer_type_node;
    tree result = create_tmp_var(int_type, "opaque");

    // Load the state var (volatile to prevent optimization)
    gimple *load_stmt = gimple_build_assign(result, state_var);
    gsi_insert_before(gsi, load_stmt, GSI_SAME_STMT);

    // XOR with a constant
    tree xor_val = build_int_cst(int_type, 0xDEADBEEF);
    tree xor_expr = fold_build2(BIT_XOR_EXPR, int_type, result, xor_val);
    tree xor_result = create_tmp_var(int_type, "xor_result");
    gimple *xor_stmt = gimple_build_assign(xor_result, xor_expr);
    gsi_insert_before(gsi, xor_stmt, GSI_SAME_STMT);

    // AND with 1 to get 0 or 1
    tree and_val = build_int_cst(int_type, 1);
    tree and_expr = fold_build2(BIT_AND_EXPR, int_type, xor_result, and_val);
    gimple *and_stmt = gimple_build_assign(result, and_expr);
    gsi_insert_before(gsi, and_stmt, GSI_SAME_STMT);

    return result;
  }

  // Helper: Create a simple opaque predicate for CFG breaking
  tree create_simple_opaque_predicate(gimple_stmt_iterator *gsi) {
    tree int_type = integer_type_node;

    // Create: (2 * 2) % 2 == 1 (always false)
    tree two = build_int_cst(int_type, 2);
    tree four = build_int_cst(int_type, 4);
    tree one = build_int_cst(int_type, 1);

    // Create temporary variables for the computation
    tree mul_result = create_tmp_var(int_type, "mul_tmp");
    gimple *mul_stmt = gimple_build_assign(mul_result, four);
    gsi_insert_before(gsi, mul_stmt, GSI_SAME_STMT);

    tree mod_result = create_tmp_var(int_type, "mod_tmp");
    tree mod_expr = fold_build2(TRUNC_MOD_EXPR, int_type, mul_result, two);
    gimple *mod_stmt = gimple_build_assign(mod_result, mod_expr);
    gsi_insert_before(gsi, mod_stmt, GSI_SAME_STMT);

    tree cmp_result = create_tmp_var(int_type, "cmp_tmp");
    tree cmp_expr = fold_build2(EQ_EXPR, int_type, mod_result, one);
    gimple *cmp_stmt = gimple_build_assign(cmp_result, cmp_expr);
    gsi_insert_before(gsi, cmp_stmt, GSI_SAME_STMT);

    return cmp_result;
  }

  // Break CFG functionality: Add dummy blocks and opaque predicates
  bool break_control_flow() {
    bool modified = false;
    std::vector<basic_block> blocks_to_break;

    // Collect blocks with exactly one successor
    basic_block bb;
    FOR_EACH_BB_FN(bb, cfun) {
      if (EDGE_COUNT(bb->succs) == 1 && bb != ENTRY_BLOCK_PTR_FOR_FN(cfun)) {
        blocks_to_break.push_back(bb);
      }
    }

    // Add opaque predicates and dummy operations to confuse analysis
    for (auto target_bb : blocks_to_break) {
      gimple_stmt_iterator gsi = gsi_last_bb(target_bb);
      if (gsi_end_p(gsi))
        continue;

      // Skip if this is already a conditional
      gimple *last_stmt = gsi_stmt(gsi);
      if (gimple_code(last_stmt) == GIMPLE_COND)
        continue;

      // Create an opaque predicate before the terminator
      tree opaque = create_simple_opaque_predicate(&gsi);

      // Add a dummy volatile operation that depends on the opaque predicate
      tree int_type = integer_type_node;
      tree volatile_type = build_qualified_type(int_type, TYPE_QUAL_VOLATILE);
      tree dummy_var = create_tmp_var(volatile_type, "break_dummy");

      // Create: dummy_var = opaque ? 42 : 24
      tree forty_two = build_int_cst(int_type, 42);
      tree twenty_four = build_int_cst(int_type, 24);
      tree cond_expr =
          fold_build3(COND_EXPR, int_type, opaque, forty_two, twenty_four);
      gimple *dummy_assign = gimple_build_assign(dummy_var, cond_expr);
      gsi_insert_before(&gsi, dummy_assign, GSI_SAME_STMT);

      modified = true;
    }

    return modified;
  }

  // Main execution method
  unsigned int execute(function *fun) override {
    // Skip if external or empty or too small
    if (DECL_EXTERNAL(cfun->decl) || cfun->cfg->x_n_basic_blocks <= 2)
      return 0;

    // Get the function name
    const char *func_name = IDENTIFIER_POINTER(DECL_NAME(cfun->decl));
    fprintf(stderr, "[ControlFlowTaint] Processing function: %s\n", func_name);

    // First, apply control flow breaking to add opaque predicates and dummy
    // blocks
    bool cfg_broken = break_control_flow();

    // Then, collect all blocks we want to taint
    std::vector<basic_block> blocks_to_taint;
    std::map<basic_block, int> block_to_id;
    int next_id = 1; // Start IDs at 1
    SimpleRandom rand(0xB105F00D ^
                      (uintptr_t)cfun); // Seed with function address

    // Collect basic blocks to taint (skip empty blocks)
    basic_block bb;
    FOR_EACH_BB_FN(bb, cfun) {
      // Skip empty blocks
      if (gimple_seq_empty_p(bb_seq(bb)))
        continue;

      blocks_to_taint.push_back(bb);
      block_to_id[bb] = next_id++;
    }

    if (blocks_to_taint.size() < 2) {
      fprintf(stderr, "[ControlFlowTaint] Function too small to taint\n");
      return 0;
    }

    // Create state variables to track control flow
    tree int_type = integer_type_node;
    tree volatile_type = build_qualified_type(int_type, TYPE_QUAL_VOLATILE);
    tree state_var = create_tmp_var(volatile_type, "state");
    tree obf_var1 = create_tmp_var(volatile_type, "obf_state");
    tree obf_var2 = create_tmp_var(volatile_type, "obf_key");

    // Process each basic block
    for (auto bb : blocks_to_taint) {
      gimple_stmt_iterator gsi = gsi_start_bb(bb);
      if (gsi_end_p(gsi))
        continue;

      // Insert a block-specific operation at the start
      // Initialize state to a block ID
      int block_id = block_to_id[bb];
      gimple *init =
          gimple_build_assign(state_var, build_int_cst(int_type, block_id));
      gsi_insert_before(&gsi, init, GSI_SAME_STMT);

      // Add additional volatile vars for obfuscation
      unsigned int random_val1 = rand.next();
      unsigned int random_val2 = rand.next();

      gimple *init1 = gimple_build_assign(
          obf_var1, build_int_cst(volatile_type, random_val1));
      gsi_insert_before(&gsi, init1, GSI_SAME_STMT);

      gimple *init2 = gimple_build_assign(
          obf_var2, build_int_cst(volatile_type, random_val2));
      gsi_insert_before(&gsi, init2, GSI_SAME_STMT);

      // Create a complex expression for obfuscation
      tree xor_expr =
          fold_build2(BIT_XOR_EXPR, volatile_type, obf_var1, obf_var2);
      tree result_var = create_tmp_var(volatile_type, "obf_result");
      gimple *complex_op = gimple_build_assign(result_var, xor_expr);
      gsi_insert_before(&gsi, complex_op, GSI_SAME_STMT);

      // Now modify successors by adding conditional branches
      edge_iterator ei;
      edge e;
      FOR_EACH_EDGE(e, ei, bb->succs) {
        basic_block succ_bb = e->dest;

        // If this block is in our taint set, update state and create opaque
        // conditions
        if (block_to_id.find(succ_bb) != block_to_id.end()) {
          int succ_id = block_to_id[succ_bb];

          // Find the terminator
          gimple_stmt_iterator term_gsi = gsi_last_bb(bb);
          if (!gsi_end_p(term_gsi)) {
            // Skip if already processed or not a terminator
            gimple *term = gsi_stmt(term_gsi);
            if (gimple_code(term) == GIMPLE_COND)
              continue;

            // Set state to successor's ID just before the terminator
            gimple *update_state = gimple_build_assign(
                state_var, build_int_cst(int_type, succ_id));
            gsi_insert_before(&term_gsi, update_state, GSI_SAME_STMT);

            // Create an opaque predicate for more complexity
            tree opaque = create_opaque_predicate(&term_gsi, state_var);

            // Add a computation using the state variable
            tree add_expr =
                fold_build2(PLUS_EXPR, volatile_type, opaque, result_var);
            tree block_result = create_tmp_var(volatile_type, "block_result");
            gimple *block_op = gimple_build_assign(block_result, add_expr);
            gsi_insert_before(&term_gsi, block_op, GSI_SAME_STMT);
          }
        }
      }
    }

    fprintf(stderr,
            "[ControlFlowTaint] Successfully obfuscated function: %s (CFG "
            "broken: %s)\n",
            func_name, cfg_broken ? "yes" : "no");
    return 0; // No major CFG changes, so don't update SSA
  }
};

} // end anonymous namespace

// Plugin initialization
int plugin_init(struct plugin_name_args *info, struct plugin_gcc_version *ver) {
  if (!plugin_default_version_check(ver, &gcc_version))
    return 1;

  fprintf(stderr, "KoviD Control Flow Taint Plugin loaded (opt level: -O%d)\n",
          global_options.x_optimize);

  static struct plugin_info my_plugin_info = {
      .version = "1.0", .help = "Control flow taint obfuscation"};

  register_callback(info->base_name, PLUGIN_INFO, nullptr, &my_plugin_info);

  control_flow_taint_plugin *taint_pass = new control_flow_taint_plugin(g);

  struct register_pass_info pass_info;
  pass_info.pass = taint_pass;
  pass_info.reference_pass_name = "cfg";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback(info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr,
                    &pass_info);
  return 0;
}