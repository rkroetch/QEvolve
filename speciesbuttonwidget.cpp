#include "speciesbuttonwidget.h"

SpeciesButtonWidget::SpeciesButtonWidget(const QString & name, const QColor & color, QWidget *parent) :
    QPushButton(name, parent), mPixmap(16, 16)
{
    setIconSize(QSize(16,16));
    setSpeciesColor(color);
}

void SpeciesButtonWidget::setSpeciesColor(const QColor &color)
{
    mPixmap.fill(color);
    setIcon(QIcon(mPixmap));
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
