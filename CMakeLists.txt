cmake_minimum_required(VERSION 3.16)
project(QEvolve VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Charts Concurrent Gui OpenGL OpenGLWidgets Widgets)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lets the Release configuration build without optimizations, e.g. to get
# usable breakpoints/stepping while still exercising Release-only code paths
# (NDEBUG, Qt release libs). See the windows-msvc-x64-unoptimized-release
# preset in CMakePresets.json.
option(QEVOLVE_UNOPTIMIZED_RELEASE
    "Build the Release configuration without compiler optimizations"
    OFF)
if(QEVOLVE_UNOPTIMIZED_RELEASE)
    if(MSVC)
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Od")
    else()
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O0")
    endif()
endif()

qt_standard_project_setup()

qt_add_executable(QEvolve WIN32 MACOSX_BUNDLE
    animal.cpp animal.h
    animalinfodialog.cpp animalinfodialog.h animalinfodialog.ui
    common.h
    deatheffects.cpp deatheffects.h
    delay.h
    difficultycurve.cpp difficultycurve.h
    editspeciesdialog.cpp editspeciesdialog.h editspeciesdialog.ui
    evolutionpoints.cpp evolutionpoints.h
    hubdialog.cpp hubdialog.h
    laboratory.cpp laboratory.h laboratory.ui
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    metaprogression.cpp metaprogression.h
    movementicons.cpp movementicons.h
    runstate.h
    species.cpp species.h
    speciesbuttonlayoutwidget.cpp speciesbuttonlayoutwidget.h speciesbuttonlayoutwidget.ui
    speciesbuttonwidget.cpp speciesbuttonwidget.h
)
target_compile_definitions(QEvolve PRIVATE
    NOMINMAX
    _USE_MATH_DEFINES
)

target_link_libraries(QEvolve PRIVATE
    Qt::Charts
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    Qt::OpenGL
    Qt::OpenGLWidgets
    Qt::Widgets
    glu32.lib
    opengl32.lib
)


include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported AND NOT QEVOLVE_UNOPTIMIZED_RELEASE)
    set_property(TARGET QEvolve PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()


# Micro-benchmarks for the simulation core (Species/Animal). On by default,
# so QEvolveBenchmarks builds as part of ALL_BUILD like every other target;
# set to OFF to skip fetching/building Google Benchmark.
option(QEVOLVE_BUILD_BENCHMARKS "Build the Google Benchmark micro-benchmarks for Species/Animal" ON)
if(QEVOLVE_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(QEvolveBenchmarks
        animal.cpp animal.h
        common.h
        deatheffects.cpp deatheffects.h
        species.cpp species.h
        benchmarks/species_benchmark.cpp
    )
    set_target_properties(QEvolveBenchmarks PROPERTIES AUTOMOC ON)
    target_include_directories(QEvolveBenchmarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(QEvolveBenchmarks PRIVATE
        NOMINMAX
        _USE_MATH_DEFINES
    )
    target_link_libraries(QEvolveBenchmarks PRIVATE
        Qt::Concurrent
        Qt::Core
        Qt::Gui
        benchmark::benchmark
    )
endif()


# Google Test unit tests for the simulation core (Movements/Animal/Species)
# and the CycleConcurrent threading backend. On by default, so QEvolveTests
# builds as part of ALL_BUILD like every other target; set to OFF to skip
# fetching/building Google Test.
option(QEVOLVE_BUILD_TESTS "Build the Google Test unit tests for QEvolve" ON)
if(QEVOLVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()


# Resources:
set(resources_resource_files
    "resources/icons/color_picker.png"
    "resources/icons/dna.png"
    "resources/icons/save.png"
)

qt_add_resources(QEvolve "resources"
    PREFIX
        "/icons"
    FILES
        ${resources_resource_files}
)
set(resources1_resource_files
    "resources/actions/arrow_branch.png"
    "resources/actions/arrow_divide.png"
    "resources/actions/arrow_down.png"
    "resources/actions/arrow_in.png"
    "resources/actions/arrow_inout.png"
    "resources/actions/arrow_join.png"
    "resources/actions/arrow_left.png"
    "resources/actions/arrow_left_down.png"
    "resources/actions/arrow_left_up.png"
    "resources/actions/arrow_merge.png"
    "resources/actions/arrow_out.png"
    "resources/actions/arrow_redo.png"
    "resources/actions/arrow_refresh.png"
    "resources/actions/arrow_refresh_small.png"
    "resources/actions/arrow_right.png"
    "resources/actions/arrow_right_down.png"
    "resources/actions/arrow_right_up.png"
    "resources/actions/arrow_rotate_anticlockwise.png"
    "resources/actions/arrow_rotate_clockwise.png"
    "resources/actions/arrow_switch.png"
    "resources/actions/arrow_turn_left.png"
    "resources/actions/arrow_turn_right.png"
    "resources/actions/arrow_undo.png"
    "resources/actions/arrow_up.png"
    "resources/actions/stop.png"
)

qt_add_resources(QEvolve "resources1"
    PREFIX
        "/actions"
    FILES
        ${resources1_resource_files}
)

add_custom_command(TARGET QEvolve POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/species"
        "$<TARGET_FILE_DIR:QEvolve>/species"
    COMMENT "Copying species/ to output directory"
)

install(DIRECTORY species/ DESTINATION ${CMAKE_INSTALL_BINDIR}/species)

install(TARGETS QEvolve
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET QEvolve
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})