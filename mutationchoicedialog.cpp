#include "mutationchoicedialog.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "movementicons.h"

MutationChoiceDialog::MutationChoiceDialog(const QVector<MutationChoice> & choices, QWidget * parent) :
    QDialog(parent)
{
    setWindowTitle(tr("A New Mutation"));
    setModal(true);
    buildUi(choices);
}

void MutationChoiceDialog::buildUi(const QVector<MutationChoice> & choices)
{
    auto * rootLayout = new QVBoxLayout(this);

    auto * headline = new QLabel(tr("Your bloodline can mutate. Choose one path:"), this);
    QFont headlineFont = headline->font();
    headlineFont.setBold(true);
    headline->setFont(headlineFont);
    rootLayout->addWidget(headline);

    for (int i = 0; i < choices.size(); ++i)
    {
        const MutationChoice & choice = choices.at(i);

        auto * box = new QGroupBox(choice.title, this);
        auto * boxLayout = new QHBoxLayout(box);

        if (choice.kind == MutationChoiceKind::MovementTweak)
        {
            auto * icon = new QLabel(box);
            icon->setPixmap(movementIcon(choice.moveDirection));
            icon->setToolTip(movementToolTip(choice.moveDirection));
            boxLayout->addWidget(icon);
        }

        auto * description = new QLabel(choice.description, box);
        description->setWordWrap(true);
        boxLayout->addWidget(description, 1);

        auto * chooseButton = new QPushButton(tr("Choose"), box);
        connect(chooseButton, &QPushButton::clicked, this, [this, i]() { choose(i); });
        boxLayout->addWidget(chooseButton);

        rootLayout->addWidget(box);
    }

    auto * skipButton = new QPushButton(tr("Skip"), this);
    connect(skipButton, &QPushButton::clicked, this, &QDialog::reject);
    rootLayout->addWidget(skipButton);
}

void MutationChoiceDialog::choose(int index)
{
    mSelectedIndex = index;
    accept();
}
