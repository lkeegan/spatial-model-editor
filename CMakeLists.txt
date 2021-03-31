cmake_minimum_required(VERSION 3.13...3.20)
message(STATUS "CMake version ${CMAKE_VERSION}")

# version number here is embedded in compiled executable
project(
  SpatialModelEditor
  VERSION 1.0.8
  DESCRIPTION "Spatial Model Editor"
  LANGUAGES CXX)

list(
  APPEND
  CMAKE_MODULE_PATH
  "${PROJECT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

# find required Qt5 modules
find_package(
  Qt5
  COMPONENTS Core
             Gui
             Widgets
  REQUIRED)
get_target_property(
  Qt5Core_location
  Qt5::Core
  LOCATION)
message(STATUS "Found Qt5Core: ${Qt5Core_location}")

# enable Qt5 utils: moc, uic, rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(SME_LOG_LEVEL
    "INFO"
    CACHE
      STRING
      "Logging level. Available levels in ascending order of verbosity: OFF, CRITICAL, ERROR, WARN, INFO, DEBUG, TRACE"
)
message(STATUS "SME_LOG_LEVEL: ${SME_LOG_LEVEL}")

set(SME_EXTRA_EXE_LIBS
    ""
    CACHE STRING "Optional additional exe libs")
message(STATUS "SME_EXTRA_EXE_LIBS: ${SME_EXTRA_EXE_LIBS}")

set(SME_EXTRA_CORE_DEFS
    ""
    CACHE STRING "Optional additional defines for core")
message(STATUS "SME_EXTRA_CORE_DEFS: ${SME_EXTRA_CORE_DEFS}")

set(SME_EXTERNAL_SMECORE
    no
    CACHE
      BOOL
      "Use an externally provided sme::core instead of building it from source")
if(SME_EXTERNAL_SMECORE)
  find_package(
    sme
    REQUIRED
    CONFIG)
endif()

set(BUILD_BENCHMARKS
    yes
    CACHE BOOL "Build benchmarks")
if(BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Catch2 REQUIRED)
  include(Catch)
  add_subdirectory(test)
endif()

set(SME_WITH_TBB
    no
    CACHE BOOL "Build with TBB support (multithreading)")
set(SME_WITH_OPENMP
    no
    CACHE BOOL "Build with OpenMP support (multithreading)")
if(SME_WITH_TBB AND SME_WITH_OPENMP)
  message(
    FATAL_ERROR
      "Please choose either TBB (-DSME_WITH_TBB=ON) or OpenMP (-DSME_WITH_OPENMP=ON) for multithreading. Using both together is not supported."
  )
endif()

set(BUILD_GUI
    yes
    CACHE BOOL "Build GUI version")
add_subdirectory(src)

# compile spatial cli
set(BUILD_CLI
    yes
    CACHE BOOL "Build command-line version")
if(BUILD_CLI)
  add_subdirectory(ext/CLI11)
  add_subdirectory(cli)
endif()

# compile python library
set(BUILD_PYTHON_LIBRARY
    yes
    CACHE BOOL "Build python library")
if(BUILD_PYTHON_LIBRARY)
  add_subdirectory(ext/pybind11)
  add_subdirectory(sme)
endif()
