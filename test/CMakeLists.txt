# Google Test unit tests for the simulation core (Movements/Animal/Species)
# and the CycleConcurrent threading backend.
#
# Build (from repo root, in an already-configured build dir):
#   cmake -S . -B build -DQEVOLVE_BUILD_TESTS=ON
#   cmake --build build --config Debug --target QEvolveTests
#   ctest --test-dir build -C Debug
# or run the produced QEvolveTests(.exe) directly for gtest's own filtering
# (--gtest_filter=..., --gtest_list_tests, etc).

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(QEvolveTests
    ${CMAKE_SOURCE_DIR}/animal.cpp ${CMAKE_SOURCE_DIR}/animal.h
    ${CMAKE_SOURCE_DIR}/common.h
    ${CMAKE_SOURCE_DIR}/deatheffects.cpp ${CMAKE_SOURCE_DIR}/deatheffects.h
    ${CMAKE_SOURCE_DIR}/species.cpp ${CMAKE_SOURCE_DIR}/species.h
    ${CMAKE_SOURCE_DIR}/cycleconcurrent.h
    ${CMAKE_SOURCE_DIR}/runstate.h
    ${CMAKE_SOURCE_DIR}/evolutionpoints.cpp ${CMAKE_SOURCE_DIR}/evolutionpoints.h
    ${CMAKE_SOURCE_DIR}/metaprogression.cpp ${CMAKE_SOURCE_DIR}/metaprogression.h
    common_test.cpp
    animal_test.cpp
    species_test.cpp
    cycleconcurrent_test.cpp
    evolutionpoints_test.cpp
    metaprogression_test.cpp
)
set_target_properties(QEvolveTests PROPERTIES AUTOMOC ON)
target_include_directories(QEvolveTests PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(QEvolveTests PRIVATE
    NOMINMAX
    _USE_MATH_DEFINES
)
target_link_libraries(QEvolveTests PRIVATE
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(QEvolveTests)
