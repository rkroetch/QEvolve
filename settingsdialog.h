#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;

// Small app-preferences dialog, opened via the gear-icon overlay button on
// the laboratory view (see MainWindow). Deliberately holds no Laboratory
// reference: the caller seeds the initial checkbox state via
// setCrtShaderEnabled() and listens for crtShaderEnabledChanged() to apply
// it live and persist it - same "dumb UI, caller owns the actual setting"
// split used by HubDialog/RunResultDialog elsewhere in this codebase.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget * parent = nullptr);

    bool crtShaderEnabled() const;
    void setCrtShaderEnabled(bool enabled);

signals:
    // Emitted immediately on toggle (not just on accept/close) so the
    // effect can be previewed live while the dialog is still open.
    void crtShaderEnabledChanged(bool enabled);

private:
    QCheckBox * mCrtShaderCheckBox = nullptr;
};

#endif // SETTINGSDIALOG_H
