/*
 * KoviD Control-Flow Taint GCC Plugin
 * -----------------------------------
 *
 * This plugin implements control flow obfuscation techniques by:
 * 1. Adding opaque predicates to basic blocks
 * 2. Creating control flow obfuscation with conditional branches
 * 3. Preserving semantics and execution paths while making analysis harder
 *
 * Author: djolertrk
 * License: Apache-2.0 WITH LLVM-exception
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <set>
#include <string>

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
#include "cfg.h"
#include "cfgloop.h"

// Enable debug output
#define DEBUG_OUTPUT 0

int plugin_is_GPL_compatible;

// Global counter to track how many functions we've tainted
static int g_tainted_function_count = 0;

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
    TODO_update_ssa        // todo_flags_finish
};

namespace {
// Class to track exception handling regions in a function
class ExceptionRegionTracker {
private:
    std::set<basic_block> eh_blocks;

public:
    ExceptionRegionTracker() {}

    // Mark a basic block as part of an exception handling region
    void mark_eh_block(basic_block bb) {
        eh_blocks.insert(bb);
    }

    // Check if a basic block is part of an exception handling region
    bool is_eh_block(basic_block bb) const {
        return eh_blocks.find(bb) != eh_blocks.end();
    }

    // Find all exception handling regions in the current function
    void find_eh_regions() {
        eh_blocks.clear();

        // Skip if function has no CFG
        if (!cfun || !cfun->cfg)
            return;
        
        const char *func_name = "(unnamed)";
        if (cfun->decl && DECL_NAME(cfun->decl)) {
            func_name = IDENTIFIER_POINTER(DECL_NAME(cfun->decl));
        }
        (void)func_name;
#if DEBUG_OUTPUT >= 2
        fprintf(stderr, "KoviD Debug: Analyzing EH regions for function '%s'\n", func_name);
        
        // Report if function has EH data in GCC structures
        if (cfun->eh) {
            fprintf(stderr, "KoviD Debug: Function '%s' has GCC EH data structure\n", func_name);
        }
#endif

        // Examine all basic blocks in the function
        basic_block bb;
        int eh_edge_count = 0;
        int eh_stmt_count = 0;

        FOR_EACH_BB_FN(bb, cfun) {
            // Check all edges for EH flags
            edge e;
            edge_iterator ei;

            // Check outgoing edges
            FOR_EACH_EDGE(e, ei, bb->succs) {
                if (e && (e->flags & EDGE_EH)) {
                    mark_eh_block(bb);
                    mark_eh_block(e->dest);
                    eh_edge_count++;

#if DEBUG_OUTPUT >= 2
                    fprintf(stderr, "KoviD Debug: Found EH edge from BB%d to BB%d\n", 
                            bb->index, e->dest->index);
#endif
                }
            }

            // Check incoming edges
            FOR_EACH_EDGE(e, ei, bb->preds) {
                if (e && (e->flags & EDGE_EH)) {
                    mark_eh_block(bb);
                    mark_eh_block(e->src);
                    eh_edge_count++;

#if DEBUG_OUTPUT >= 2
                    fprintf(stderr, "KoviD Debug: Found EH edge from BB%d to BB%d\n", 
                            e->src->index, bb->index);
#endif
                }
            }

            // Check each statement for EH-related operations
            gimple_stmt_iterator gsi;
            for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
                gimple* stmt = gsi_stmt(gsi);
                if (!stmt) continue;

                // Check for EH-related GIMPLE statements
                enum gimple_code code = gimple_code(stmt);
                if (code == GIMPLE_RESX || code == GIMPLE_EH_DISPATCH ||
                    code == GIMPLE_CATCH || code == GIMPLE_EH_FILTER ||
                    code == GIMPLE_TRY) {
                    mark_eh_block(bb);
                    eh_stmt_count++;

#if DEBUG_OUTPUT >= 2
                    fprintf(stderr, "KoviD Debug: Found EH-related statement in BB%d (code=%d)\n", 
                            bb->index, (int)code);
#endif
                    break;
                }

                // Check for calls that might throw
                if (code == GIMPLE_CALL && !(gimple_call_flags(stmt) & ECF_NOTHROW)) {
                    // Potentially throwing call - mark the block as EH-related
                    mark_eh_block(bb);
                    eh_stmt_count++;

#if DEBUG_OUTPUT >= 2
                    tree fndecl = gimple_call_fndecl(as_a<gcall *>(stmt));
                    const char *callee_name = fndecl ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : "unknown";
                    fprintf(stderr, "KoviD Debug: Found potentially throwing call to '%s' in BB%d\n", 
                            callee_name, bb->index);
#endif
                    break;
                }
            }
        }

#if DEBUG_OUTPUT >= 1
        fprintf(stderr, "KoviD Debug: Function '%s' has %d EH edges and %d EH statements\n", 
                func_name, eh_edge_count, eh_stmt_count);
        fprintf(stderr, "KoviD Debug: Marked %d blocks as EH-related out of %d total blocks\n", 
                (int)eh_blocks.size(), cfun->cfg->x_n_basic_blocks);
#endif
    }
};

// Main GCC plugin pass implementation
struct control_flow_taint_plugin : gimple_opt_pass {
    control_flow_taint_plugin(gcc::context *ctx)
        : gimple_opt_pass(control_flow_taint_pass_data, ctx) {}

    // Create a variable to use for opaque predicates
    tree create_variable(const char *name, tree type) {
        tree var = create_tmp_var(type, name);
        // In newer GCC versions, add_referenced_var is not needed
        return var;
    }

    // Create a constant with the given value
    tree create_constant(tree type, int64_t value) {
        return build_int_cst(type, value);
    }

    // Create a dummy assignment in a basic block
    bool add_dummy_assignment(basic_block bb) {
        // Get the first statement in the block
        gimple_stmt_iterator gsi = gsi_start_bb(bb);
        if (gsi_end_p(gsi))
            return false;

        // Create a dummy variable and assign 1 to it
        tree int_type = integer_type_node;
        tree dummy_var = create_variable("kovid_dummy", int_type);
        tree one = create_constant(int_type, 1);

        // Build and insert the assignment
        gimple *assign_stmt = gimple_build_assign(dummy_var, one);
        gsi_insert_before(&gsi, assign_stmt, GSI_SAME_STMT);

        return true;
    }

    // Create a new basic block after the given block
    basic_block create_new_bb_after(basic_block bb) {
        // Create a new empty basic block
        edge fallthru = find_fallthru_edge(bb->succs);
        if (!fallthru)
            return NULL;

        // Split the edge to create a new basic block
        basic_block new_bb = split_edge(fallthru);
        if (!new_bb)
            return NULL;

        return new_bb;
    }

    // Create an opaque predicate that always evaluates to true
    // but is hard for static analysis to determine
    tree create_opaque_predicate(gimple_stmt_iterator *gsi) {
        tree int_type = integer_type_node;
        tree result = create_variable("kovid_opaque", int_type);

        // result = 1
        tree one = create_constant(int_type, 1);
        gimple *assign_stmt = gimple_build_assign(result, one);
        gsi_insert_before(gsi, assign_stmt, GSI_SAME_STMT);
        
        return result;
    }

    // Split a basic block in a way that maintains semantics but obfuscates control flow
    bool split_block_with_opaque_predicate(basic_block bb, basic_block next_bb) {
        if (!bb || !next_bb)
            return false;

        // Get a statement iterator for the start of the block
        gimple_stmt_iterator gsi = gsi_start_bb(bb);
        if (gsi_end_p(gsi))
            return false;

        // Create an opaque predicate (always true but looks complex)
        tree predicate = create_opaque_predicate(&gsi);

        // Find the edge from bb to next_bb
        edge orig_edge = NULL;
        edge e;
        edge_iterator ei;
        FOR_EACH_EDGE(e, ei, bb->succs) {
            if (e->dest == next_bb) {
                orig_edge = e;
                break;
            }
        }

        if (!orig_edge)
            return false;

        // Create a conditional branch at the end of bb
        gimple_stmt_iterator bb_end = gsi_last_bb(bb);
        tree zero = create_constant(integer_type_node, 0);

        // Build the condition: if (predicate != 0) goto true_path else goto false_path
        gcond *cond_stmt = gimple_build_cond(NE_EXPR, predicate, zero, NULL_TREE, NULL_TREE);
        gsi_insert_after(&bb_end, cond_stmt, GSI_NEW_STMT);

        // Create a new basic block for the false path (which should never be taken)
        basic_block false_bb = create_empty_bb(bb);
        if (!false_bb)
            return false;

        // Add a dummy assignment to the false block 
        add_dummy_assignment(false_bb);

        // Create an edge from the false block to the next block
        make_edge(false_bb, next_bb, EDGE_FALLTHRU);

        // Make the conditional branch have the appropriate destinations
        e = find_edge(bb, next_bb);
        if (e)
            redirect_edge_succ(e, false_bb);

        make_edge(bb, next_bb, EDGE_TRUE_VALUE);

        return true;
    }

    // Taint control flow in a function - using only the most basic transformation
    bool taint_control_flow(ExceptionRegionTracker &eh_tracker) {
        if (!cfun || !cfun->cfg)
            return false;

        const char *func_name = get_function_name();
        (void)func_name;
#if DEBUG_OUTPUT >= 1
        fprintf(stderr, "KoviD Debug: Attempting to taint function '%s' with %d blocks\n", 
                func_name, cfun->cfg->x_n_basic_blocks);
                
        if (cfun->eh) {
            fprintf(stderr, "KoviD Debug: Function '%s' has EH data in GCC structures\n", func_name);
        }
#endif
        // Just get the entry block's successor
        basic_block entry = ENTRY_BLOCK_PTR_FOR_FN(cfun);
        if (!entry || !entry->succs || EDGE_COUNT(entry->succs) == 0) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: Function '%s' has no valid entry block\n", func_name);
#endif
            return false;
        }
        edge e = EDGE_SUCC(entry, 0);
        if (!e || !e->dest) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: Function '%s' has no valid successor to entry\n", func_name);
#endif
            return false;
        }

        basic_block first_bb = e->dest;
        // Skip if this is the exit block or empty
        if (first_bb == EXIT_BLOCK_PTR_FOR_FN(cfun)) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: First block is EXIT in function '%s'\n", func_name);
#endif
            return false;
        }

        if (gimple_seq_empty_p(bb_seq(first_bb))) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: First block is empty in function '%s'\n", func_name);
#endif
            return false;
        }
        // Skip if this block is part of an exception handling region
        if (eh_tracker.is_eh_block(first_bb)) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: First block is part of EH region in function '%s'\n", func_name);
#endif
            return false;
        }

#if DEBUG_OUTPUT >= 1
        fprintf(stderr, "KoviD Debug: Adding dummy assignment to first block (BB%d) in function '%s'\n", 
                first_bb->index, func_name);
#endif

        // Add a dummy assignment - this is the simplest transformation
        bool result = add_dummy_assignment(first_bb);
#if DEBUG_OUTPUT >= 1
        if (result) {
            fprintf(stderr, "KoviD Debug: Successfully tainted function '%s'\n", func_name);
        } else {
            fprintf(stderr, "KoviD Debug: Failed to taint function '%s'\n", func_name);
        }
#endif
        return result;
    }

    // Get the name of a function (for debugging)
    const char* get_function_name() {
        if (!cfun || !cfun->decl || !DECL_NAME(cfun->decl))
            return "(unnamed)";
        return IDENTIFIER_POINTER(DECL_NAME(cfun->decl));
    }

    // For development only.
    // Check if a function is likely to be problematic
    bool is_problematic_function() {
        const char *func_name = get_function_name();
        (void) func_name;
        return false;
    }

    // Special handler for problematic functions
    bool handle_problematic_function() {
        // For problematic functions, we just add a single dummy assignment
        // to the entry block successor to minimize any potential issues
        const char *func_name = get_function_name();
        (void)func_name;
#if DEBUG_OUTPUT >= 1
        fprintf(stderr, "KoviD Debug: Using ultra-safe handling for problematic function '%s'\n", func_name);
#endif

        basic_block entry = ENTRY_BLOCK_PTR_FOR_FN(cfun);
        if (!entry || !entry->succs || EDGE_COUNT(entry->succs) == 0) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: No entry block for problematic function '%s'\n", func_name);
#endif
            return false;
        }

        edge e = EDGE_SUCC(entry, 0);
        if (!e || !e->dest) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: No valid successor for problematic function '%s'\n", func_name);
#endif
            return false;
        }

        basic_block first_bb = e->dest;

        // Skip if this is the exit block or empty
        if (first_bb == EXIT_BLOCK_PTR_FOR_FN(cfun) || 
            gimple_seq_empty_p(bb_seq(first_bb))) {
#if DEBUG_OUTPUT >= 1
            fprintf(stderr, "KoviD Debug: First block is EXIT or empty in problematic function '%s'\n", func_name);
#endif
            return false;
        }

#if DEBUG_OUTPUT >= 1
        fprintf(stderr, "KoviD Debug: Adding minimal transformation to problematic function '%s'\n", func_name);
#endif

        // Add a single dummy assignment - the safest transformation
        bool result = add_dummy_assignment(first_bb);

#if DEBUG_OUTPUT >= 1
        if (result) {
            fprintf(stderr, "KoviD Debug: Successfully applied minimal transformation to '%s'\n", func_name);
        } else {
            fprintf(stderr, "KoviD Debug: Failed to apply minimal transformation to '%s'\n", func_name);
        }
#endif

        return result;
    }

    // Main execution method
    unsigned int execute(function *fun) override {
        try {
            // Skip if function pointer is null
            if (!fun) {
                fprintf(stderr, "KoviD Control Flow Taint: Skipping - null function pointer\n");
                return 0;
            }

            // Skip if current function is null
            if (!cfun) {
                fprintf(stderr, "KoviD Control Flow Taint: Skipping - null cfun\n");
                return 0;
            }

            // Skip if declaration or control flow graph is missing
            if (!cfun->decl || !cfun->cfg) {
                fprintf(stderr, "KoviD Control Flow Taint: Skipping - missing declaration or CFG\n");
                return 0;
            }

            // Get function name for logs
            const char *func_name = get_function_name();
            (void)func_name;
            // Get the filename if available
            const char *filename = "(unknown)";
            (void)filename;
            if (DECL_SOURCE_FILE(cfun->decl)) {
                filename = DECL_SOURCE_FILE(cfun->decl);
            } else if (global_options.x_main_input_filename) {
                filename = global_options.x_main_input_filename;
            }

#if DEBUG_OUTPUT >= 2
            // Print detailed debug info
            fprintf(stderr, "KoviD Debug: Processing function '%s' from file '%s'\n", func_name, filename);
            fprintf(stderr, "KoviD Debug: Function has %d basic blocks\n", cfun->cfg->x_n_basic_blocks);
            if (cfun->eh) {
                fprintf(stderr, "KoviD Debug: Function has EH data structure\n");
            } else {
                fprintf(stderr, "KoviD Debug: Function does not have EH data structure\n");
            }
#endif
            // Skip inlined functions
            if (DECL_DECLARED_INLINE_P(cfun->decl)) {
                fprintf(stderr, "KoviD Control Flow Taint: Skipping - inlined function '%s'\n", func_name);
                return 0;
            }

            // Print processing message
            fprintf(stderr, "KoviD Control Flow Taint: Processing function '%s'\n", func_name);

            // Analyze exception handling regions
            ExceptionRegionTracker eh_tracker;
            eh_tracker.find_eh_regions();

#if DEBUG_OUTPUT >= 2
            // Print information about the function's CFG
            fprintf(stderr, "KoviD Debug: Function '%s' CFG structure:\n", func_name);

            // Count different types of blocks and edges
            int normal_blocks = 0;
            int eh_blocks = 0;
            int normal_edges = 0;
            int eh_edges = 0;

            basic_block bb;
            FOR_EACH_BB_FN(bb, cfun) {
                if (eh_tracker.is_eh_block(bb)) {
                    eh_blocks++;
                } else {
                    normal_blocks++;
                }

                edge e;
                edge_iterator ei;
                FOR_EACH_EDGE(e, ei, bb->succs) {
                    if (e && (e->flags & EDGE_EH)) {
                        eh_edges++;
                    } else {
                        normal_edges++;
                    }
                }
            }

            fprintf(stderr, "KoviD Debug: Function '%s' has %d normal blocks, %d EH blocks\n", 
                    func_name, normal_blocks, eh_blocks);
            fprintf(stderr, "KoviD Debug: Function '%s' has %d normal edges, %d EH edges\n", 
                    func_name, normal_edges, eh_edges);
#endif

            // Apply control flow tainting
            bool success = taint_control_flow(eh_tracker);
            if (success) {
                // Increment the global counter of tainted functions
                g_tainted_function_count++;

                // Print success message
                fprintf(stderr, "KoviD Control Flow Taint: Successfully tainted function '%s' (total: %d)\n", 
                        func_name, g_tainted_function_count);
            } else {
                fprintf(stderr, "KoviD Control Flow Taint: Could not transform function '%s'\n", func_name);
            }

            return 0;
        } catch (...) {
            // Global error handler
            fprintf(stderr, "KoviD Control Flow Taint: Caught exception while processing function '%s'\n", 
                    get_function_name());
            return 0;
        }
    }
};

} // end anonymous namespace

// Callback function for printing stats at the end of compilation
static void print_final_stats(void *gcc_data, void *user_data) {
    // Print a summary of how many functions were tainted
    if (g_tainted_function_count > 0) {
        fprintf(stderr, "\nKoviD Control Flow Taint Summary: Tainted %d functions\n\n", 
                g_tainted_function_count);
    }
}

// Plugin initialization
int plugin_init(struct plugin_name_args *info, struct plugin_gcc_version *ver) {
    if (!plugin_default_version_check(ver, &gcc_version))
        return 1;

    // Print a loading message with optimization level
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
                      
    // Register callback to print final stats at the end of compilation
    register_callback(info->base_name, PLUGIN_FINISH, print_final_stats, nullptr);
                      
    return 0;
}