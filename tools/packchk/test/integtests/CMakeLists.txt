SET(SOURCE_FILES PackChkIntegTestEnv.cpp PackChkIntegTests.cpp)
SET(HEADER_FILES PackChkIntegTestEnv.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND src/)

add_executable(PackChkIntegTests ${SOURCE_FILES} ${HEADER_FILES})

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DBUILD_FOLDER="${CMAKE_BINARY_DIR}/")

set_property(TARGET PackChkIntegTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(TARGET PackChkIntegTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(PackChkIntegTests PUBLIC RteFsUtils packchklib gtest_main)
target_include_directories(PackChkIntegTests PUBLIC ../include ./src)

add_test(NAME PackChkIntegTests
         COMMAND PackChkIntegTests --gtest_output=xml:test_reports/packchkintegtest-${SYSTEM}${ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
