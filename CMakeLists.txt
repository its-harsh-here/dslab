# DSLab: one CMake project, built two ways (docs/architecture.md, section 4).
#   Native (g++), for tests:    npm run test:cpp     -> build/native
#   WebAssembly (Emscripten):   npm run build:wasm   -> build/wasm
cmake_minimum_required(VERSION 3.20)
project(dslab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DSLAB_WARNINGS -Wall -Wextra -Wpedantic -Werror)

if(EMSCRIPTEN)
  # Wasm exception handling, at compile and link time, so the protocol layer can
  # catch anything unexpected instead of aborting the module.
  add_compile_options(-fwasm-exceptions)
  add_link_options(-fwasm-exceptions)
endif()

# The engine: plain C++17 that compiles natively and to WebAssembly.
add_library(dslab_engine STATIC
  engine/src/BinaryTree.cpp
  engine/src/Engine.cpp
  engine/src/History.cpp
  engine/src/Queue.cpp
  engine/src/SinglyLinkedList.cpp
  engine/src/Stack.cpp
  engine/src/Trace.cpp
  engine/src/json/Protocol.cpp)
target_include_directories(dslab_engine PUBLIC engine/include)
# Third-party headers are SYSTEM so their warnings do not fail our build.
target_include_directories(dslab_engine SYSTEM PRIVATE engine/third_party)
target_compile_options(dslab_engine PRIVATE ${DSLAB_WARNINGS})

if(EMSCRIPTEN)
  # dslab_engine.mjs (ES module factory) + dslab_engine.wasm + dslab_engine.d.ts
  add_executable(dslab_wasm engine/bindings/wasm_bindings.cpp)
  target_link_libraries(dslab_wasm PRIVATE dslab_engine)
  target_compile_options(dslab_wasm PRIVATE ${DSLAB_WARNINGS})
  set_target_properties(dslab_wasm PROPERTIES OUTPUT_NAME dslab_engine SUFFIX ".mjs")
  target_link_options(dslab_wasm PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createDslabEngine
    -sENVIRONMENT=web
    -sSTACK_SIZE=1048576
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=134217728
    "SHELL:--emit-tsd ${CMAKE_CURRENT_BINARY_DIR}/dslab_engine.d.ts")
else()
  enable_testing()
  add_executable(dslab_tests
    engine/tests/test_main.cpp
    engine/tests/test_binary_tree.cpp
    engine/tests/test_list.cpp
    engine/tests/test_protocol.cpp
    engine/tests/test_queue.cpp
    engine/tests/test_stack.cpp
    engine/tests/test_support_classes.cpp
    engine/tests/support/Checkers.cpp
    engine/tests/support/Run.cpp)
  target_link_libraries(dslab_tests PRIVATE dslab_engine)
  target_include_directories(dslab_tests PRIVATE engine/tests)
  target_include_directories(dslab_tests SYSTEM PRIVATE engine/third_party)
  target_compile_options(dslab_tests PRIVATE ${DSLAB_WARNINGS})
  target_compile_definitions(dslab_tests PRIVATE
    DSLAB_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/engine/tests/golden")
  add_test(NAME dslab_tests COMMAND dslab_tests)
endif()
