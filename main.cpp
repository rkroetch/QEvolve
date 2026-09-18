#include <QApplication>
#include <QFile>
#include <QFont>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Laboratory (laboratory.cpp) renders via modern core-profile OpenGL -
    // VAOs/VBOs and GLSL 330 shaders only, no fixed-function calls. The
    // default QSurfaceFormat doesn't request a profile at all (leaving it
    // to whatever the driver defaults to, typically compatibility), so this
    // must be set before the QApplication - and therefore before any
    // QOpenGLWidget/context - is created.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);

    // --- Retro pixel-art presentation pass (DB16 palette + chunky bevels;
    // see resources/theme.qss) -----------------------------------------
    // Windows' native style (windows11/windowsvista) draws QMenuBar/
    // QToolBar/QScrollBar chrome itself and ignores most QSS background/
    // border rules for them, which would leave the menu/tool bars stuck
    // looking like stock Windows UI no matter what theme.qss says. Fusion
    // is a full software style that actually respects the stylesheet
    // everywhere, so it's forced here rather than left at the platform
    // default.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    // TODO(font): no bitmap/pixel font asset is bundled (no internet/
    // image-gen access when this theme was written). Consolas is the
    // nearest already-available monospace fallback in this project (see
    // mainwindow.ui's textBrowser, which already used it) - swap this for a
    // real pixel TTF (e.g. "Press Start 2P") dropped into resources/ and
    // loaded via QFontDatabase::addApplicationFont() for a more authentic
    // look.
    QFont pixelFont(QStringLiteral("Consolas"));
    pixelFont.setStyleHint(QFont::Monospace);
    pixelFont.setPointSize(pixelFont.pointSize() > 0 ? pixelFont.pointSize() + 1 : 10);
    pixelFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    a.setFont(pixelFont);

    // qt_add_resources() (see CMakeLists.txt) keeps the FILES path under the
    // PREFIX verbatim - same as how movementicons.cpp reaches its icons via
    // ":actions/resources/actions/..." rather than ":actions/....png" - so
    // this is ":/resources/theme.qss", not ":/theme.qss".
    QFile themeFile(QStringLiteral(":/resources/theme.qss"));
    if (themeFile.open(QFile::ReadOnly | QFile::Text))
    {
        a.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
