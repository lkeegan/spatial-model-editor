configure_file("version.cpp.in"
               "${PROJECT_BINARY_DIR}/src/core/common/src/version.cpp")
target_sources(
  core
  PRIVATE logger.cpp
          svg.cpp
          symbolic.cpp
          tiff.cpp
          utils.cpp
          ${PROJECT_BINARY_DIR}/src/core/common/src/version.cpp)
if(BUILD_TESTING)
  target_sources(
    core_tests
    PUBLIC logger_t.cpp
           svg_t.cpp
           symbolic_t.cpp
           tiff_t.cpp
           utils_t.cpp)
endif()
