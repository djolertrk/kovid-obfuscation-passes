// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

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

// Ensure GPL compatibility
int plugin_is_GPL_compatible;

#ifndef CRYPTO_KEY
#define CRYPTO_KEY "default_key"
#endif

// ----------------------------------------------------------------------
// Encryption helper: XOR each character with the key (repeating as needed)
// and then convert the result to a hex string.
static std::string encryptFunctionName(const std::string &name,
                                       const std::string &key) {
  std::string xored;
  xored.reserve(name.size());
  for (size_t i = 0; i < name.size(); ++i)
    xored.push_back(name[i] ^ key[i % key.size()]);

  std::ostringstream oss;
  oss << std::hex << std::setw(2) << std::setfill('0');
  for (unsigned char c : xored)
    oss << std::setw(2) << static_cast<int>(c);
  return oss.str();
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
    tree fndecl = fun->decl;

    // Skip if there is no function body.
    if (!gimple_has_body_p(fndecl))
      return 0;

    // TODO: Handle those.
    if (TREE_PUBLIC(fndecl))
      return 0;
    if (DECL_EXTERNAL(fndecl))
      return 0;

    // Only rename functions with local linkage (typically static functions).
    if (!TREE_STATIC(fndecl))
      return 0;

    // Skip `inline` functions for now.
    if (DECL_DECLARED_INLINE_P(fndecl))
      return 0;

    // Get the original function name.
    const char *origNameC = IDENTIFIER_POINTER(DECL_NAME(fndecl));
    if (!origNameC)
      return 0;
    std::string originalName(origNameC);

    // Debug output.
    fprintf(stderr, "KoviD Rename: Original function name: %s\n",
            originalName.c_str());
    fprintf(stderr, "KoviD Rename: Using crypto key: %s\n", CryptoKey.c_str());

    // Encrypt the name.
    std::string encryptedName = encryptFunctionName(originalName, CryptoKey);
    fprintf(stderr, "KoviD Rename: Encrypted name: %s\n\n",
            encryptedName.c_str());

    // Prepend an underscore to the encrypted name.
    std::string newName = "_" + encryptedName;

    // Set the new name as the function's identifier.
    DECL_NAME(fndecl) = get_identifier(newName.c_str());
    SET_DECL_ASSEMBLER_NAME(fndecl, get_identifier(newName.c_str()));

    // Also update the cgraph node if available.
    if (cgraph_node *node = cgraph_node::get(fndecl))
      node->decl = fndecl;

    return 0;
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
// Plugin initialization function.
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
  if (!plugin_default_version_check(version, &gcc_version)) {
    fprintf(stderr, "KoviD Rename plugin: Incompatible GCC version\n");
    return 1;
  }

  // Register plugin information.
  register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, plugin_info);

  // Register our pass.
  register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                    &kovid_rename_pass_info);

  fprintf(stderr, "KoviD Rename Code GCC Plugin loaded successfully\n");
  return 0;
}
