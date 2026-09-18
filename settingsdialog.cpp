#include "settingsdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget * parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    auto * rootLayout = new QVBoxLayout(this);

    auto * displayLabel = new QLabel(tr("Display"), this);
    QFont sectionFont = displayLabel->font();
    sectionFont.setBold(true);
    displayLabel->setFont(sectionFont);
    rootLayout->addWidget(displayLabel);

    mCrtShaderCheckBox = new QCheckBox(tr("CRT shader"), this);
    mCrtShaderCheckBox->setToolTip(tr("Overlays scanlines, a vignette, and mild screen curvature on the laboratory view."));
    connect(mCrtShaderCheckBox, &QCheckBox::toggled, this, &SettingsDialog::crtShaderEnabledChanged);
    rootLayout->addWidget(mCrtShaderCheckBox);

    rootLayout->addStretch();

    auto * buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

bool SettingsDialog::crtShaderEnabled() const
{
    return mCrtShaderCheckBox->isChecked();
}

void SettingsDialog::setCrtShaderEnabled(bool enabled)
{
    mCrtShaderCheckBox->setChecked(enabled);
}
