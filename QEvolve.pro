#-------------------------------------------------
#
# Project created by QtCreator 2010-09-16T22:32:33
#
#-------------------------------------------------

QT       += core opengl widgets concurrent openglwidgets charts
CONFIG += c++20

TARGET = QEvolve
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    laboratory.cpp \
    animal.cpp \
    species.cpp \
    editspeciesdialog.cpp \
    speciesbuttonlayoutwidget.cpp \
    speciesbuttonwidget.cpp

HEADERS  += mainwindow.h \
    delay.h \
    laboratory.h \
    animal.h \
    species.h \
    common.h \
    editspeciesdialog.h \
    speciesbuttonlayoutwidget.h \
    speciesbuttonwidget.h
#    glext.h

FORMS    += mainwindow.ui \
    laboratory.ui \
    editspeciesdialog.ui \
    speciesbuttonlayoutwidget.ui

LIBS += opengl32.lib
LIBS += glu32.lib

DEFINES += _USE_MATH_DEFINES

#QMAKE_CXXFLAGS += -pg
#QMAKE_LFLAGS += -pg

RESOURCES += \
    resources.qrc
