#cmake_minimum_required(VERSION 3.22...3.30)
cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(libbitcoin-node
  VERSION 4.0.0
  DESCRIPTION "Bitcoin Cross-Platform C++ Development Toolkit - base library"
  LANGUAGES C CXX
)

#set(Boost_VERBOSE true)

if (MSVC)
  set( CMAKE_STATIC_LIBRARY_PREFIX "lib" )
  set( CMAKE_SHARED_LIBRARY_PREFIX "lib" )
endif ()

add_library( bitcoin-node )
add_library( bitcoin::node ALIAS bitcoin-node )

# ──────────────────────────────────────────────────────────────
#   Options
# ─────────────────────────────────────────────────────────────

#option(BUILD_SHARED_LIBS "Build shared library instead of static" OFF)
option(with-tests "Build unit tests" ON)

set(LIBBITCOIN_CXX_STANDARD 20 CACHE STRING "Require C++ 20 standard.")

# ──────────────────────────────────────────────────────────────
#   Dependencies
# ──────────────────────────────────────────────────────────────

find_package(Boost 1.86.0 REQUIRED
  COMPONENTS
    unit_test_framework
)

find_package( bitcoin-database 4.0.0 REQUIRED )
find_package( bitcoin-network 4.0.0 REQUIRED )

# ──────────────────────────────────────────────────────────────
#   Library definition
# ──────────────────────────────────────────────────────────────

file(GLOB_RECURSE bitcoin_node_HEADERS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/bitcoin/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include/bitcoin/*.ipp"
)

file( GLOB_RECURSE bitcoin_node_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.c"
)

target_sources( bitcoin-node PUBLIC
  FILE_SET HEADERS
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
  FILES ${bitcoin_node_HEADERS}
)

target_sources( bitcoin-node PRIVATE
  ${bitcoin_node_SOURCES}
)

target_include_directories( bitcoin-node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(bitcoin-node PUBLIC cxx_std_${LIBBITCOIN_CXX_STANDARD})

target_compile_definitions(bitcoin-node
  PUBLIC
    $<$<BOOL:${with-ssl}>:-DWITH_SSL -DWOLFSSL_USER_SETTINGS>
  PRIVATE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:BC_DLL>
    # Add other internal defines if needed, e.g. BC_USE_ICU
)

target_link_libraries(bitcoin-node
  PUBLIC
    bitcoin::database
    bitcoin::network
)

# Very common flags for libbitcoin projects
target_compile_options(bitcoin-node
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive- /W3>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

set_target_properties(bitcoin-node PROPERTIES
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME node
)

# ──────────────────────────────────────────────────────────────
#   Tests
# ──────────────────────────────────────────────────────────────

if(with-tests)
  enable_testing()

  file(GLOB_RECURSE bitcoin_node_test_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.c"
  )

  add_executable( bitcoin-node-test )

  target_sources( bitcoin-node-test PRIVATE
    ${bitcoin_node_test_SOURCES} )

  target_link_libraries( bitcoin-node-test PRIVATE
    bitcoin::node
    Boost::unit_test_framework
  )

  target_compile_options(bitcoin-node-test
    PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:/permissive- /W3>
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
  )

  add_test( NAME bitcoin-node-test COMMAND bitcoin-node-test
    --run_test=*
    --log_level=warning
    --show_progress=no
    --detect_memory_leak=0
    --report_level=no
    --build_info=yes
  )

endif()

# ──────────────────────────────────────────────────────────────
#   Installation & CMake package config
# ──────────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS bitcoin-node
  EXPORT bitcoin-node-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILE_SET HEADERS
)

install(EXPORT bitcoin-node-targets
  NAMESPACE bitcoin::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitcoin-node
  FILE bitcoin-node-targets.cmake
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/bitcoin-node-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/bitcoin-node-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitcoin-node
)

write_basic_package_version_file(
  bitcoin-node-config-version.cmake
  VERSION ${PACKAGE_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/bitcoin-node-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/bitcoin-node-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bitcoin-node
)
