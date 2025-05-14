# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile

import lit.formats
import lit.util

# Try to import LLVM specific modules, but don't fail if not available
try:
    from lit.llvm import llvm_config
    from lit.llvm.subst import ToolSubst
    from lit.llvm.subst import FindTool
    llvm_config_available = True
except ImportError:
    lit_config.warning("Could not import lit.llvm modules. Using fallbacks.")
    llvm_config_available = False
    # Define minimal fallbacks for what we need
    class DummyLLVMConfig:
        def __init__(self):
            self.use_lit_shell = False
        
        def with_system_environment(self, vars):
            pass
            
        def use_default_substitutions(self):
            pass
            
    llvm_config = DummyLLVMConfig()
    
    def FindTool(name):
        return name
        
    class ToolSubst:
        def __init__(self, cmd, unresolved='error', post='', extra_args=None):
            self.cmd = cmd
            self.unresolved = unresolved

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = "KOVID-OBFUSCATION"

# Set test format
# The 'use_lit_shell' option affects script execution
use_lit_shell = getattr(llvm_config, 'use_lit_shell', True)
config.test_format = lit.formats.ShTest(use_lit_shell)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = [".c", ".test", ".ll"]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
if hasattr(config, 'kovid_obj_root'):
    config.test_exec_root = os.path.join(config.kovid_obj_root, "test")
else:
    config.test_exec_root = os.path.dirname(os.path.abspath(__file__))

# Make sure we have the required config variables
if not hasattr(config, 'llvm_tools_dir'):
    lit_config.warning('llvm_tools_dir is not defined, some substitutions may not work')
    config.llvm_tools_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'bin')

# Attempt to load the local config if it exists
try:
    local_config_path = os.path.join(config.test_source_root, "lit.local.cfg")
    if os.path.exists(local_config_path):
        lit_config.note(f"Loading local configuration from {local_config_path}")
        lit_config.load_config(config, local_config_path)
except Exception as e:
    lit_config.warning(f'Failed to load lit.local.cfg: {e}')

# Add environment variables
if llvm_config_available and llvm_config:
    llvm_config.with_system_environment(["HOME", "INCLUDE", "LIB", "TMP", "TEMP", "PATH"])

# excludes: A list of directories to exclude from the testsuite.
config.excludes = ["Inputs", "Examples", "CMakeLists.txt", "README.md", "lit.cfg.py", "lit.site.cfg.py.in", "lit.local.cfg"]

# Get the paths from the site configuration
filecheck_path = getattr(config, 'filecheck_path', 'FileCheck')
not_path = getattr(config, 'not_path', 'not')

# Add basic substitutions
config.substitutions.append(('%FileCheck', filecheck_path))
config.substitutions.append(('%not', not_path))

# Add project-specific directory substitutions
if hasattr(config, 'kovid_obj_root'):
    config.substitutions.append(("%kovid_testdir", config.kovid_obj_root))
else:
    config.substitutions.append(("%kovid_testdir", os.path.dirname(os.path.abspath(__file__))))

# Configure clang and other LLVM tools
if llvm_config_available:
    # Check if the llvm_config object is not None and has the method
    if llvm_config and hasattr(llvm_config, 'use_default_substitutions'):
        try:
            llvm_config.use_default_substitutions()
        except Exception as e:
            lit_config.warning(f"Couldn't set up default substitutions: {e}")
    else:
        lit_config.warning("LLVM config doesn't support default substitutions")

# Define simple tool substitutions if LLVM config not available or didn't setup substitutions
if not llvm_config_available or '%clang' not in [s[0] for s in config.substitutions]:
    # Define basic tool substitutions with full paths when possible
    # First check if tools_dir is set
    tools_dir = getattr(config, 'llvm_tools_dir', '')
    
    # Detect system clang if not specified
    if 'clang' not in [s[0] for s in config.substitutions]:
        clang_path = os.path.join(tools_dir, 'clang') if tools_dir else '/usr/bin/env clang'
        config.substitutions.append(('%clang', clang_path))
    
    # Detect system opt if not specified  
    if 'opt' not in [s[0] for s in config.substitutions]:
        opt_path = os.path.join(tools_dir, 'opt') if tools_dir else '/usr/bin/env opt'
        config.substitutions.append(('%opt', opt_path))
        
    # Other basic tools
    if 'llc' not in [s[0] for s in config.substitutions]:
        llc_path = os.path.join(tools_dir, 'llc') if tools_dir else '/usr/bin/env llc'
        config.substitutions.append(('%llc', llc_path))
        
    if 'llvm-link' not in [s[0] for s in config.substitutions]:
        link_path = os.path.join(tools_dir, 'llvm-link') if tools_dir else '/usr/bin/env llvm-link'
        config.substitutions.append(('%llvm-link', link_path))

# Display LLVM version if available
if hasattr(config, 'llvm_version_major') and config.llvm_version_major:
    lit_config.note("Using LLVM version: {}.{}".format(config.llvm_version_major, 
        getattr(config, 'llvm_version_minor', '')))

# Add plugin-specific substitutions with proper shared library extension based on platform
shared_lib_ext = '.so'
if platform.system() == 'Darwin':
    shared_lib_ext = '.dylib'
elif platform.system() == 'Windows':
    shared_lib_ext = '.dll'

# Add plugin paths with appropriate extension
plugin_lib_dir = os.path.join(getattr(config, 'kovid_obj_root', '.'), 'lib')
config.substitutions.append(('%rename_plugin', os.path.join(plugin_lib_dir, 'libKoviDRenameCodeLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%string_encryption_plugin', os.path.join(plugin_lib_dir, 'libKoviDStringEncryptionLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%dummy_code_plugin', os.path.join(plugin_lib_dir, 'libKoviDDummyCodeInsertionLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%break_cfg_plugin', os.path.join(plugin_lib_dir, 'libKoviDBreakCFGLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%cf_flattening_plugin', os.path.join(plugin_lib_dir, 'libKoviDCFFlatteningLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%inst_obf_plugin', os.path.join(plugin_lib_dir, 'libKoviDInstructionObfuscationPassLLVMPlugin' + shared_lib_ext)))
config.substitutions.append(('%metadata_plugin', os.path.join(plugin_lib_dir, 'libKoviDRemoveMetadataAndUnusedCodeLLVMPlugin' + shared_lib_ext)))

# Add features based on environment
if getattr(config, 'native_tests', '1') == '1':
    config.available_features.add('NATIVE_TESTS')

# Add shell feature
config.available_features.add('shell')

# Print configuration details in debug mode
if lit_config.debug:
    lit_config.note("Using LLVM tool dir: {}".format(getattr(config, 'llvm_tools_dir', 'Not defined')))
    lit_config.note("Using KOVID object root: {}".format(getattr(config, 'kovid_obj_root', 'Not defined')))
    for substitution in config.substitutions:
        if len(substitution) > 1:
            lit_config.note("Substitution: {} -> {}".format(substitution[0], substitution[1]))
