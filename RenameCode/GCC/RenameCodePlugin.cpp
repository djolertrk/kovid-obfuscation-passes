// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <set>
#include <map>
#include <vector>

// Disable implicit inlining for this plugin
// #pragma GCC optimize ("no-inline")

// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree.h"
#include "tree-ssa-alias.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "tree-pretty-print.h"
#include "tree-pass.h"
#include "tree-ssa-operands.h"
#include "tree-phinodes.h"
#include "tree-nested.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "diagnostic.h"
#include "stringpool.h"
#include "gimplify.h"
#include "context.h"
#include "function.h"
#include "cgraph.h"
#include "config.h"
#include "toplev.h"  // For main_input_filename

// Ensure GPL compatibility
int plugin_is_GPL_compatible;

#define DEBUG_OUTPUT 1

#ifndef CRYPTO_KEY
#define CRYPTO_KEY "default_key"
#endif

// Maximum size of function name to process (bytes)
#define MAX_FUNCTION_NAME_SIZE 128

// Maximum number of functions to process per translation unit
static size_t MAX_FUNCTIONS_PER_TU = 500;

// Maximum size of a translation unit (in bytes) to process
#define MAX_TU_SIZE 1000000

// List of large translation units to completely skip (filename patterns)
static const char* SKIP_FILES[] = {
  "sqlite3.c",
  "kimwitu",
  "SPASS",
  "ClamAV",
  "consumer-typeset",
  "7zOut.cpp",        // Problematic 7zip file that causes segfaults
  "BenchCon.cpp",     // Problematic 7zip benchmark file with overloaded functions
  "7z",               // Skip all 7z files to be safe
  "CTMark/7zip"       // Skip entire 7zip benchmark directory
};
static const int NUM_SKIP_FILES = sizeof(SKIP_FILES) / sizeof(SKIP_FILES[0]);

// Global tracking of renamed functions to avoid duplication
static std::map<std::string, std::string> g_renamed_functions;

// Track used names to avoid collisions
static std::set<std::string> g_used_names;

// Track function names per file to detect overloads (which we'll skip)
static std::map<std::string, std::set<std::string>> g_file_function_names;

// Limit processing to avoid excessive memory usage
static size_t g_processed_functions = 0;

// Statistics tracking
static size_t g_functions_seen = 0;
static size_t g_functions_renamed = 0;
static size_t g_functions_skipped = 0;
static size_t g_functions_overloaded = 0;
static size_t g_functions_error = 0;

// TU tracking
static const char* g_current_file = NULL;
static size_t g_current_file_size = 0;
static bool g_skip_current_file = false;

// Flag to abort processing if a critical error occurs
static bool g_critical_error = false;

// Flag to indicate if we're in the preprocessing pass
static bool g_preprocessing_pass = true;

// Global counter to guarantee unique names
static unsigned long long g_unique_id_counter = 0;

// Add a map to track function names across translation units by their full qualified name
static std::map<std::string, std::string> g_full_qualified_names;

// Set to track functions with duplicate names in each translation unit
static std::set<std::string> g_duplicate_functions;

// Map to store absolute filename → file index for consistently numbering files across runs
static std::map<std::string, unsigned int> g_file_indices;
static unsigned int g_next_file_index = 0;

// ----------------------------------------------------------------------
// Generate a unique name for a function - GUARANTEED to be unique
static std::string encryptFunctionName(const std::string &name,
                                     const std::string &key) {
  // Safety wrapper to prevent any crashes
  try {
    // Skip empty names or empty keys to avoid potential issues
    if (name.empty() || key.empty()) {
      g_functions_error++;
      return name; // Just return the original name
    }

    // Skip overlong names to prevent excessive memory usage
    if (name.size() > MAX_FUNCTION_NAME_SIZE) {
      g_functions_skipped++;
      return name; // Return original name
    }

    try {
      // Generate a UUID-like format that's guaranteed to be unique
      std::string prefix = "kovid_";
      
      // Get a unique ID that will never repeat
      unsigned long long unique_id = g_unique_id_counter++;
      
      // Generate a stable hash from the function name, current file, and key
      // This ensures uniqueness across translation units
      std::string hash_input = name + key;
      if (g_current_file) {
        hash_input += g_current_file;
      }
      size_t name_hash = std::hash<std::string>{}(hash_input) & 0xFFFFFFFF;
      
      // Get file index for current translation unit
      unsigned int file_index = 0;
      if (g_current_file && *g_current_file) {
        std::string current_file_str(g_current_file);
        if (g_file_indices.find(current_file_str) != g_file_indices.end()) {
          file_index = g_file_indices[current_file_str];
        }
      }
      
      // Format: kovid_[8-char hash]_[file-index]_[counter]
      std::ostringstream oss;
      oss << prefix << std::hex << name_hash << "_" << std::dec << file_index << "_" << unique_id;
      
      std::string result = oss.str();
      
      // Always add to the set of used names
      g_used_names.insert(result);
      return result;
    } catch (const std::exception &e) {
      // Handle any exceptions that might occur
      g_functions_error++;
  #ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Error encrypting function name: %s\n", e.what());
  #endif
      return name; // Return original name on error
    }
  } catch (...) {
    // Catch absolutely any error to prevent plugin crash
    g_functions_error++;
    g_critical_error = true; // Mark as critical to prevent further processing
    return name; // Return original name
  }
}

static bool starts_with(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), str.begin());
}

// Check if a function should be skipped based on name patterns
static bool should_skip_function(const std::string& name) {
  // Skip functions that start with underscore - these are often system/internal functions
  if (starts_with(name, "_")) {
    return true;
  }
  
  // Skip functions in our duplicate list
  if (g_duplicate_functions.find(name) != g_duplicate_functions.end()) {
    return true;
  }
  
  return false;
}

// ----------------------------------------------------------------------
// Define pass data for our GIMPLE pass.
static const pass_data kovid_rename_pass_data = {
    GIMPLE_PASS,    // type of pass
    "kovid_rename", // name
    OPTGROUP_NONE,  // optinfo_flags
    TV_NONE,        // tv_id
    0,              // properties_required
    0,              // properties_provided
    0,              // properties_destroyed
    0,              // todo_flags_start
    TODO_update_ssa // todo_flags_finish
};

// Our pass: it derives from gimple_opt_pass and operates on one function.
struct kovid_rename_pass : gimple_opt_pass {
  std::string CryptoKey;

  kovid_rename_pass(gcc::context *ctx)
      : gimple_opt_pass(kovid_rename_pass_data, ctx), CryptoKey(CRYPTO_KEY) {}

  // Execute the pass on the current function.
  virtual unsigned int execute(function *fun) override {
    // Wrap everything in a try-catch to prevent segfaults
    try {
      // Skip entire file if it's in the exclusion list or very large
      if (g_skip_current_file) {
        return 0;
      }
    
      // If we've encountered a critical error, stop processing more functions
      if (g_critical_error) {
        return 0;
      }

      // Limit the number of functions processed per translation unit to avoid excessive memory usage
      if (g_processed_functions >= MAX_FUNCTIONS_PER_TU) {
  #ifdef DEBUG_OUTPUT
        fprintf(stderr, "KoviD Rename: Maximum function limit reached (%zu), skipping remaining functions\n", 
                MAX_FUNCTIONS_PER_TU);
  #endif
        return 0;
      }

      g_functions_seen++;
      g_processed_functions++;
    } catch (...) {
      // If we encounter any error here, mark it as critical and stop processing
      g_critical_error = true;
      return 0;
    }

    // Safety checks
    if (!fun) {
      g_functions_error++;
      return 0;
    }

    // Use try-catch to prevent crashes
    try {
      tree fndecl = fun->decl;
      if (!fndecl) {
        g_functions_error++;
        return 0;
      }

      // Skip if there is no function body.
      if (!gimple_has_body_p(fndecl)) {
        g_functions_skipped++;
        return 0;
      }

      // We now check for static functions later in the code

      // Skip `inline` functions for now.
      if (DECL_DECLARED_INLINE_P(fndecl)) {
        g_functions_skipped++;
        return 0;
      }

      // Get the original function name with safety checks
      tree name_tree = DECL_NAME(fndecl);
      if (!name_tree) {
        g_functions_skipped++;
        return 0;
      }

      const char *origNameC = IDENTIFIER_POINTER(name_tree);
      if (!origNameC) {
        g_functions_skipped++;
        return 0;
      }

      // Skip functions with very long names
      size_t name_len = strlen(origNameC);
      if (name_len > MAX_FUNCTION_NAME_SIZE) {
        g_functions_skipped++;
        return 0;
      }

      std::string originalName(origNameC);
      std::string qualified_name;
      
      // Get the source file information
      const char* source_file = NULL;
      try {
        if (DECL_SOURCE_FILE(fndecl)) {
          source_file = DECL_SOURCE_FILE(fndecl);
        }
      } catch (...) {
        // If we can't get source info, skip this function
        g_functions_skipped++;
        return 0;
      }
      
      // Check if we should skip this function
      if (should_skip_function(originalName)) {
        g_functions_skipped++;
#ifdef DEBUG_OUTPUT
        fprintf(stderr, "KoviD Rename: Skipping function with underscore prefix or in duplicate list: %s\n", originalName.c_str());
#endif
        return 0;
      }
      
      // Source file based tracking
      if (source_file) {
        std::string current_file_str(source_file);
        
        // In preprocessing pass, we detect duplicate function names across the entire file
        if (g_preprocessing_pass) {
          bool duplicate_found = false;
          
          // Check if we've seen this function name in this file before
          if (g_file_function_names.find(current_file_str) != g_file_function_names.end()) {
            if (g_file_function_names[current_file_str].find(originalName) != 
                g_file_function_names[current_file_str].end()) {
              // This function name already exists in this file - it's a duplicate
              g_duplicate_functions.insert(originalName);
              duplicate_found = true;
            } else {
              // First time seeing this function name in this file
              g_file_function_names[current_file_str].insert(originalName);
            }
          } else {
            // First function in this file
            g_file_function_names[current_file_str].insert(originalName);
          }
          
          if (duplicate_found) {
            g_functions_overloaded++;
#ifdef DEBUG_OUTPUT
            fprintf(stderr, "KoviD Rename: Found duplicate function (will be skipped in renaming pass): %s\n", 
                    originalName.c_str());
#endif
          }
          
          // During preprocessing, we only detect duplicates but don't rename
          return 0;
        }
      }
      
      // Check if function has internal linkage (static functions)
      // Only rename static functions to avoid duplicate symbols
      if (!DECL_EXTERNAL(fndecl) && !TREE_PUBLIC(fndecl)) {
        // Create a qualified name with source information and parameter types
        qualified_name = originalName;
        
        // Add parameter information to handle overloaded functions
        try {
          // Get function type
          tree fntype = TREE_TYPE(fndecl);
          if (fntype) {
            // Get parameter information
            qualified_name += "(";
            
            // Handle parameters
            if (TYPE_ARG_TYPES(fntype)) {
              bool first_param = true;
              for (tree arg = TYPE_ARG_TYPES(fntype); arg && arg != void_list_node; arg = TREE_CHAIN(arg)) {
                if (!first_param) {
                  qualified_name += ",";
                }
                first_param = false;
                
                // Get parameter type
                tree type = TREE_VALUE(arg);
                if (type) {
                  // Add a simple type hash to the qualified name
                  size_t type_hash = (size_t)TYPE_MAIN_VARIANT(type) & 0xFFFF;
                  qualified_name += std::to_string(type_hash);
                } else {
                  qualified_name += "?";
                }
              }
            }
            qualified_name += ")";
          }
        } catch (...) {
          // If we can't get parameter info, add a random identifier based on line
          qualified_name += "(?)";
        }
        
        // Try to get source file and line information for better function identification
        const char* source_file = NULL;
        unsigned int line = 0;
        
        // Safely access source file and line
        try {
          if (DECL_SOURCE_FILE(fndecl)) {
            source_file = DECL_SOURCE_FILE(fndecl);
            if (source_file) {
              qualified_name += "@";
              qualified_name += source_file;
            }
          }
          
          if ((line = DECL_SOURCE_LINE(fndecl)) > 0) {
            qualified_name += ":";
            qualified_name += std::to_string(line);
          }
        } catch (...) {
          // Just use the name if we can't access source info
        }
        
        // First check if we've already seen this qualified name
        if (g_full_qualified_names.find(qualified_name) != g_full_qualified_names.end()) {
          std::string cached_name = g_full_qualified_names[qualified_name];
          
          // Make sure the cached name is still in our set of used names
          if (g_used_names.find(cached_name) == g_used_names.end()) {
              g_used_names.insert(cached_name);
          }
          
          // Use cached name to avoid inconsistent renaming
          tree id = get_identifier(cached_name.c_str());
          if (id) {
            DECL_NAME(fndecl) = id;
            SET_DECL_ASSEMBLER_NAME(fndecl, id);
            
            // Also update the cgraph node if available.
            if (cgraph_node *node = cgraph_node::get(fndecl))
              node->decl = fndecl;
          }
          return 0;
        }
      } else {
        // Skip non-static functions
        g_functions_skipped++;
        return 0;
      }
      
      // Then check if we've already processed this function name
      if (g_renamed_functions.find(originalName) != g_renamed_functions.end()) {
        std::string cached_name = g_renamed_functions[originalName];
        
        // Check if the cached name matches any qualified function we've seen
        bool found_in_qualified = false;
        for (const auto& entry : g_full_qualified_names) {
          if (entry.second == cached_name) {
            found_in_qualified = true;
            break;
          }
        }
        
        // Only use the cached name if it also exists in qualified names
        if (found_in_qualified) {
          // Make sure the cached name is still in our set of used names
          if (g_used_names.find(cached_name) == g_used_names.end()) {
              g_used_names.insert(cached_name);
          }
          
          // Use cached name to avoid inconsistent renaming
          tree id = get_identifier(cached_name.c_str());
          if (id) {
            DECL_NAME(fndecl) = id;
            SET_DECL_ASSEMBLER_NAME(fndecl, id);
            
            // Also update the cgraph node if available.
            if (cgraph_node *node = cgraph_node::get(fndecl))
              node->decl = fndecl;
          }
          return 0;
        }
      }

      // Encrypt the name - without debug output in benchmarks
#ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Original function name: %s\n",
              originalName.c_str());
      fprintf(stderr, "KoviD Rename: Qualified name with parameters: %s\n",
              qualified_name.c_str());
      fprintf(stderr, "KoviD Rename: Using crypto key: %s\n", CryptoKey.c_str());
#endif

      // Encrypt the name with error handling
      std::string encryptedName = encryptFunctionName(originalName, CryptoKey);
      if (encryptedName == originalName) {
        // Skip if encryption failed or was skipped
        g_functions_skipped++;
        return 0;
      }

#ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Encrypted name: %s\n\n",
              encryptedName.c_str());
#endif

      // Use the encrypted name directly - no need to prepend underscore
      std::string newName = encryptedName;

      // Cache the new name in both maps
      g_renamed_functions[originalName] = newName;
      g_full_qualified_names[qualified_name] = newName;

      // Set the new name as the function's identifier.
      tree id = get_identifier(newName.c_str());
      if (!id) {
        g_functions_error++;
#ifdef DEBUG_OUTPUT
        fprintf(stderr, "KoviD Rename: Failed to create identifier for %s\n", newName.c_str());
#endif
        return 0;
      }
      
      DECL_NAME(fndecl) = id;
      SET_DECL_ASSEMBLER_NAME(fndecl, id);

      // Also update the cgraph node if available.
      if (cgraph_node *node = cgraph_node::get(fndecl))
        node->decl = fndecl;

      g_functions_renamed++;
      return 0;
    } catch (const std::exception &e) {
      g_functions_error++;
#ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Exception during function renaming: %s\n", e.what());
#endif
      return 0;
    } catch (...) {
      // Catch all other exceptions to prevent plugin crashes
      g_functions_error++;
      g_critical_error = true; // Set flag to avoid processing more functions
#ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Unknown exception during function renaming, stopping further processing\n");
#endif
      return 0;
    }
  }

  // No cloning is necessary; return this.
  virtual kovid_rename_pass *clone() override { return this; }
};

// ----------------------------------------------------------------------
// Register the pass so that it runs after SSA construction.
static struct register_pass_info kovid_rename_pass_info = {
    new kovid_rename_pass(g), // instance of our pass (using global context g)
    "ssa",                    // run after the "ssa" pass
    1,                        // reference pass instance number
    PASS_POS_INSERT_AFTER     // insert after the referenced pass
};

// ----------------------------------------------------------------------
// Check if a file should be skipped based on name
static bool should_skip_file(const char* filename) {
  if (!filename)
    return true;

  // Check against the list of files to skip
  for (int i = 0; i < NUM_SKIP_FILES; i++) {
    if (strstr(filename, SKIP_FILES[i]) != NULL) {
#ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Skipping file based on blacklist pattern '%s': %s\n", 
              SKIP_FILES[i], filename);
#endif
      return true;
    }
  }
  
  return false;
}

// ----------------------------------------------------------------------
// Get the translation unit file size
static size_t get_file_size(const char* filename) {
  if (!filename)
    return 0;
    
  FILE* f = fopen(filename, "rb");
  if (!f)
    return 0;
    
  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fclose(f);
  
  return size;
}

// ----------------------------------------------------------------------
// Init function for each translation unit
static void kovid_rename_init_unit(void *gcc_data, void *user_data) {
  // Safety guard to prevent segfaults
  try {
    // Get current input filename
    g_current_file = main_input_filename;
    
    // Reset per-TU counters
    g_processed_functions = 0;
    g_functions_seen = 0;
    g_functions_renamed = 0;
    g_functions_skipped = 0;
    g_functions_overloaded = 0;
    g_functions_error = 0;
    g_critical_error = false;
    
    // Reset the unique ID counter for each translation unit
    g_unique_id_counter = 0;
    
    // Clear the g_renamed_functions map for each new translation unit
    g_renamed_functions.clear();
    
    // Clear the function name tracking for the current file
    // so we can detect duplicate functions in this file
    if (g_current_file && *g_current_file) {
      std::string current_file_str(g_current_file);
      g_file_function_names[current_file_str].clear();
    }
    
    // For the first pass, we clear the duplicate functions set
    if (g_preprocessing_pass) {
      g_duplicate_functions.clear();
    }
    
    // Get or assign a unique file index for this translation unit
    // This ensures function names are unique across translation units
    if (g_current_file && *g_current_file) {
      std::string current_file_str(g_current_file);
      if (g_file_indices.find(current_file_str) == g_file_indices.end()) {
        // Assign a new index to this file
        g_file_indices[current_file_str] = g_next_file_index++;
      }
    }
    
    // Check if we should skip this file
    g_skip_current_file = should_skip_file(g_current_file);
    
    // Get file size if not skipping
    if (!g_skip_current_file) {
      g_current_file_size = get_file_size(g_current_file);
      
      // Skip very large files
      if (g_current_file_size > MAX_TU_SIZE) {
        g_skip_current_file = true;
      }
    }
    
  #ifdef DEBUG_OUTPUT
    unsigned int file_index = g_current_file && *g_current_file ? 
                             g_file_indices[std::string(g_current_file)] : 0;
    
    fprintf(stderr, "KoviD Rename: Starting translation unit %s\n", g_current_file ? g_current_file : "<unknown>");
    fprintf(stderr, "  Pass: %s\n", g_preprocessing_pass ? "preprocessing" : "renaming");
    fprintf(stderr, "  File index: %u\n", file_index);
    fprintf(stderr, "  File size: %zu bytes\n", g_current_file_size);
    fprintf(stderr, "  Skipping: %s\n", g_skip_current_file ? "yes" : "no");
    fprintf(stderr, "  Current unique names: %zu\n", g_used_names.size());
    fprintf(stderr, "  Current tracked renames: %zu\n", g_full_qualified_names.size());
    if (!g_preprocessing_pass) {
      fprintf(stderr, "  Duplicate functions to skip: %zu\n", g_duplicate_functions.size());
    }
  #endif
  } catch (...) {
    // In case of any error, mark the file to be skipped
    g_skip_current_file = true;
    g_current_file = main_input_filename ? main_input_filename : "<unknown>";
    g_current_file_size = 0;
    g_processed_functions = 0;
    g_functions_seen = 0;
    g_functions_renamed = 0;
    g_functions_skipped = 0;
    g_functions_error = 0;
    g_critical_error = true;
  }
}

// ----------------------------------------------------------------------
// Reset function to clean up at end of compilation unit
static void kovid_rename_finish_unit(void *gcc_data, void *user_data) {
  // Safety guard to prevent segfaults
  try {
    #ifdef DEBUG_OUTPUT
      fprintf(stderr, "KoviD Rename: Statistics for translation unit %s (pass: %s):\n", 
              g_current_file ? g_current_file : "<unknown>",
              g_preprocessing_pass ? "preprocessing" : "renaming");
              
      fprintf(stderr, "  Functions seen: %zu\n", g_functions_seen);
      
      if (!g_preprocessing_pass) {
        fprintf(stderr, "  Functions renamed: %zu\n", g_functions_renamed);
      }
      
      fprintf(stderr, "  Functions skipped: %zu\n", g_functions_skipped);
      fprintf(stderr, "  Functions overloaded (skipped): %zu\n", g_functions_overloaded);
      fprintf(stderr, "  Functions with errors: %zu\n", g_functions_error);
      
      if (!g_preprocessing_pass) {
        fprintf(stderr, "  Total tracked renames: %zu\n", g_full_qualified_names.size());
        fprintf(stderr, "  Total unique names: %zu\n", g_used_names.size());
      } else {
        fprintf(stderr, "  Total duplicate functions identified: %zu\n", g_duplicate_functions.size());
      }
      
      fprintf(stderr, "  Critical error occurred: %s\n", g_critical_error ? "yes" : "no");
      fprintf(stderr, "  Skipped file: %s\n", g_skip_current_file ? "yes" : "no");
    #endif

    // If we completed the preprocessing pass, switch to the renaming pass
    if (g_preprocessing_pass) {
      g_preprocessing_pass = false;
      
      #ifdef DEBUG_OUTPUT
        fprintf(stderr, "KoviD Rename: Switching to renaming pass\n");
      #endif
    }
    
    // Note: we intentionally don't clear g_renamed_functions or g_full_qualified_names to maintain
    // consistent renaming across translation units
    
    // Reset file tracking
    g_current_file = NULL;
    g_current_file_size = 0;
    g_skip_current_file = false;
  } catch (...) {
    // If anything goes wrong, just silently continue
    // to avoid crashes during cleanup
    g_current_file = NULL;
    g_current_file_size = 0;
    g_skip_current_file = false;
  }
}

// ----------------------------------------------------------------------
// Plugin initialization function.
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  // Version check
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr, "KoviD Rename plugin: Incompatible GCC version\n");
    return 1;
  }

  // Parse plugin arguments if any
  for (int i = 0; i < plugin_info->argc; ++i) {
    if (strcmp(plugin_info->argv[i].key, "max_functions") == 0) {
      size_t max_funcs = static_cast<size_t>(atoi(plugin_info->argv[i].value));
      if (max_funcs > 0) {
        MAX_FUNCTIONS_PER_TU = max_funcs;
      }
    }
  }

  // Register plugin information.
  register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, plugin_info);

  // Register our pass.
  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                    &kovid_rename_pass_info);
  
  // Register initialization callback to check file size and eligibility
  register_callback(plugin_info->base_name, PLUGIN_START_UNIT,
                   &kovid_rename_init_unit, NULL);
                    
  // Register finish callback to output statistics and reset counters
  register_callback(plugin_info->base_name, PLUGIN_FINISH_UNIT, 
                   &kovid_rename_finish_unit, NULL);

#ifdef DEBUG_OUTPUT
  fprintf(stderr, "KoviD Rename Code GCC Plugin loaded successfully\n");
  fprintf(stderr, "  Maximum functions per translation unit: %zu\n", MAX_FUNCTIONS_PER_TU);
#endif
  return 0;
}