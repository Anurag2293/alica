cmake_minimum_required(VERSION 3.10)
project(alica_sim VERSION 0.1.0)
include(CTest)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}/cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake_flags/cflags.cmake)
alica_compile_flags()

find_package(yaml-cpp REQUIRED)
find_package(alica_solver_interface REQUIRED)
find_package(alica_engine REQUIRED)
find_package(alica_dynamic_loading REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)

find_package(Git QUIET)
set(ALICA_SIM_BUILD_ID "unknown")
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE ALICA_SIM_GIT_HEAD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(ALICA_SIM_GIT_HEAD)
    set(ALICA_SIM_BUILD_ID "${ALICA_SIM_GIT_HEAD}")
  endif()
endif()

add_library(${PROJECT_NAME} SHARED
  src/Agent.cpp
  src/JsonWriter.cpp
  src/Match.cpp
  src/MatchConfig.cpp
  src/MessageBus.cpp
  src/Numeric.cpp
  src/PlanFiles.cpp
  src/Runner.cpp
  src/Scheduler.cpp
  src/Sha256.cpp
  src/SimLogger.cpp
)

set_source_files_properties(src/Numeric.cpp PROPERTIES COMPILE_OPTIONS -fno-finite-math-only)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  ALICA_SIM_VERSION="${PROJECT_VERSION}"
  ALICA_SIM_BUILD_ID="${ALICA_SIM_BUILD_ID}"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  alica_engine
  alica_dynamic_loading
  ${YAML_CPP_LIBRARIES}
  ${CMAKE_DL_LIBS}
)

add_executable(alica_sim_run src/main.cpp)
target_link_libraries(alica_sim_run PRIVATE ${PROJECT_NAME})
if(APPLE)
  set(ALICA_TOOL_RPATH "@loader_path/../lib")
else()
  set(ALICA_TOOL_RPATH "$ORIGIN/../lib")
endif()
set_target_properties(alica_sim_run PROPERTIES INSTALL_RPATH "${ALICA_TOOL_RPATH}" INSTALL_RPATH_USE_LINK_PATH TRUE)

if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  include(GoogleTest)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)

  add_library(alica_sim_test_world SHARED test/world/TestWorld.cpp)
  target_link_libraries(alica_sim_test_world PRIVATE ${PROJECT_NAME})

  add_library(alica_sim_test_team SHARED test/team/TestTeam.cpp)
  target_link_libraries(alica_sim_test_team PRIVATE alica_engine)
  target_include_directories(alica_sim_test_team PRIVATE ${Boost_INCLUDE_DIRS})

  add_executable(${PROJECT_NAME}-test
    test/test_match.cpp
    test/test_message_bus.cpp
    test/test_plan_files.cpp
    test/test_runner.cpp
    test/test_scheduler.cpp
    test/test_util.cpp
  )
  target_compile_definitions(${PROJECT_NAME}-test PRIVATE
    ALICA_SIM_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test"
    ALICA_SIM_TEST_WORLD_DIR="$<TARGET_FILE_DIR:alica_sim_test_world>"
    ALICA_SIM_TEST_TEAM_DIR="$<TARGET_FILE_DIR:alica_sim_test_team>"
    ALICA_SIM_RUN="$<TARGET_FILE:alica_sim_run>"
  )
  target_link_libraries(${PROJECT_NAME}-test PRIVATE ${PROJECT_NAME} GTest::gtest GTest::gtest_main Threads::Threads)
  add_dependencies(${PROJECT_NAME}-test alica_sim_test_world alica_sim_test_team alica_sim_run)
  gtest_discover_tests(${PROJECT_NAME}-test)

  add_executable(${PROJECT_NAME}-fast-math-test test/test_fast_math.cpp src/JsonWriter.cpp src/MatchConfig.cpp src/Numeric.cpp)
  target_include_directories(${PROJECT_NAME}-fast-math-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_options(${PROJECT_NAME}-fast-math-test PRIVATE -O2 -ffast-math)
  target_link_libraries(${PROJECT_NAME}-fast-math-test PRIVATE alica_engine ${YAML_CPP_LIBRARIES} GTest::gtest GTest::gtest_main Threads::Threads)
  gtest_discover_tests(${PROJECT_NAME}-fast-math-test)

  add_test(NAME alica_sim_run_test
    COMMAND alica_sim_run ${CMAKE_CURRENT_SOURCE_DIR}/test/match.yaml
      --plugin-path $<TARGET_FILE_DIR:alica_sim_test_world>
      --out ${CMAKE_CURRENT_BINARY_DIR}/alica_sim_run_test.jsonl
  )
  set_tests_properties(alica_sim_run_test PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:alica_sim_test_team>"
    PASS_REGULAR_EXPRESSION "\\{\"result\":.*\"ticks\":150\\}"
  )
endif()

install(TARGETS ${PROJECT_NAME}
  EXPORT ${CMAKE_PROJECT_NAME}Targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(TARGETS alica_sim_run
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include/
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

export(EXPORT ${CMAKE_PROJECT_NAME}Targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Targets.cmake
)

install(EXPORT ${CMAKE_PROJECT_NAME}Targets
  FILE ${CMAKE_PROJECT_NAME}Targets.cmake
  DESTINATION ${ConfigPackageLocation}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
  ${CMAKE_PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${ConfigPackageLocation}
  PATH_VARS INCLUDE_INSTALL_DIR
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${ConfigPackageLocation}
)
