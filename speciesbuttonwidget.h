#ifndef SPECIESBUTTONWIDGET_H
#define SPECIESBUTTONWIDGET_H

#include <QPushButton>
#include <QColor>

class SpeciesButtonWidget : public QPushButton
{
    Q_OBJECT
public:
    explicit SpeciesButtonWidget(const QString & name, const QColor & color, QWidget *parent = 0);
    ~SpeciesButtonWidget() = default;

signals:

public slots:
    void setSpeciesColor(const QColor & color);
    void setSpeciesName(const QString &text);

protected:
    QSize minimumSizeHint() const override;
    void paintEvent(QPaintEvent *event) override;

private:
    QColor mColor;

};

#endif // SPECIESBUTTONWIDGET_H
