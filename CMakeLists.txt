cmake_minimum_required(VERSION 3.16)
project(QEvolve VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Charts Concurrent Gui OpenGL OpenGLWidgets Widgets)

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
    _USE_MATH_DEFINES
    NOMINMAX
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

get_filename_component(QT_WINDEPLOYQT_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_WINDEPLOYQT_DIR}" REQUIRED)
add_custom_command(TARGET QEvolve POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --verbose 0 --no-translations "$<TARGET_FILE:QEvolve>"
    COMMENT "Deploying Qt runtime next to QEvolve"
)


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

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
