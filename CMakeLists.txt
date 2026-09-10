cmake_minimum_required(VERSION 3.16)
project(QEvolve VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Charts Concurrent Gui OpenGL OpenGLWidgets Widgets)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

qt_standard_project_setup()

qt_add_executable(QEvolve WIN32 MACOSX_BUNDLE
    animal.cpp animal.h
    common.h
    delay.h
    editspeciesdialog.cpp editspeciesdialog.h editspeciesdialog.ui
    laboratory.cpp laboratory.h laboratory.ui
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
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
if(ipo_supported)
    set_property(TARGET QEvolve PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
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