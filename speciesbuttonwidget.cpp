#include "speciesbuttonwidget.h"

#include <QStyle>
#include <QStyleOptionButton>
#include <QStylePainter>

namespace {
// Base button font is scaled up 2x to read well against the species-color
// background, then trimmed by ~15% (2.0 * 0.85 = 1.7) so a full species
// roster fits more comfortably in the sidebar.
constexpr double kButtonFontScale = 1.7;
} // namespace

SpeciesButtonWidget::SpeciesButtonWidget(const QString & name, const QColor & color, QWidget *parent) :
    QPushButton(name, parent)
{
    QFont buttonFont = font();
    if (buttonFont.pointSizeF() > 0)
    {
        buttonFont.setPointSizeF(buttonFont.pointSizeF() * kButtonFontScale);
    }
    else
    {
        buttonFont.setPixelSize(qRound(buttonFont.pixelSize() * kButtonFontScale));
    }
    setFont(buttonFont);

    // Fill the full width of whatever sidebar/layout row this button sits
    // in, rather than shrinking to fit each species' name.
    setSizePolicy(QSizePolicy::Expanding, sizePolicy().verticalPolicy());

    setSpeciesColor(color);
}

void SpeciesButtonWidget::setSpeciesColor(const QColor &color)
{
    mColor = color;
    setStyleSheet(QString("QPushButton { background-color: %1; }").arg(color.name()));
    update();
}

void SpeciesButtonWidget::setSpeciesName(const QString &text)
{
    setText(text);
}

QSize SpeciesButtonWidget::minimumSizeHint() const
{
    // QPushButton's own minimumSizeHint() already accounts for the current
    // (enlarged) font's text width, so only enforce a floor here - a flat
    // override would either clip long species names (e.g. "cruiser_2") or
    // pointlessly widen short ones.
    auto sh = QPushButton::minimumSizeHint();
    sh.setWidth(qMax(sh.width(), 150));
    return sh;
}

void SpeciesButtonWidget::paintEvent(QPaintEvent *)
{
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.icon = QIcon();

    // Pick black or white text depending on the background's perceived brightness
    // so the species name stays readable against any species color.
    const double luminance = 0.299 * mColor.redF() + 0.587 * mColor.greenF() + 0.114 * mColor.blueF();
    opt.palette.setColor(QPalette::ButtonText, luminance > 0.5 ? Qt::black : Qt::white);

    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButton, opt);
}
