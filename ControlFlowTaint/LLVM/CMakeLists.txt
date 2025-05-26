add_library(ControlFlowTaintPass MODULE ControlFlowTaint.cpp)

target_include_directories(ControlFlowTaintPass PRIVATE "${LLVM_INCLUDE_DIRS}")
target_compile_features(ControlFlowTaintPass PRIVATE cxx_range_for cxx_auto_type)

# Enable c++17
target_compile_features(ControlFlowTaintPass PRIVATE cxx_std_17)

# Get proper shared-library behavior when linking with undefined symbols (OS X)
if(APPLE)
    set_target_properties(ControlFlowTaintPass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)