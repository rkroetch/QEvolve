#include "speciesbuttonwidget.h"

#include <QStyle>
#include <QStyleOptionButton>
#include <QStylePainter>

SpeciesButtonWidget::SpeciesButtonWidget(const QString & name, const QColor & color, QWidget *parent) :
    QPushButton(name, parent)
{
    QFont buttonFont = font();
    if (buttonFont.pointSizeF() > 0)
    {
        buttonFont.setPointSizeF(buttonFont.pointSizeF() * 2.0);
    }
    else
    {
        buttonFont.setPixelSize(buttonFont.pixelSize() * 2);
    }
    setFont(buttonFont);

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
    auto sh = QPushButton::minimumSizeHint();
    sh.setWidth(150);
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
