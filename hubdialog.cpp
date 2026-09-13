#include "hubdialog.h"

#include <QComboBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "species.h"

namespace {

// Maps the three biome/plant-pattern catalog ids registered by
// MetaProgression::registerDefaultCatalog() to their PlantPattern value.
// Kept local to the dialog rather than in MetaProgression since it's a
// UI-side concern (choosing what a "biome" combo entry means); the ids
// themselves are the contract shared between the two.
bool biomeIdToPattern(const QString & id, PlantPattern & outPattern)
{
    if (id == QLatin1String("biome.oneGroup")) { outPattern = plantPatternOneGroup; return true; }
    if (id == QLatin1String("biome.twoGroups")) { outPattern = plantPatternTwoGroups; return true; }
    if (id == QLatin1String("biome.random")) { outPattern = plantPatternRandom; return true; }
    return false;
}

QString categoryLabel(UnlockCategory category)
{
    switch (category)
    {
    case UnlockCategory::StartingSpecies: return QStringLiteral("Species");
    case UnlockCategory::StatBonus: return QStringLiteral("Stat Bonus");
    case UnlockCategory::Biome: return QStringLiteral("Biome");
    case UnlockCategory::Cosmetic: return QStringLiteral("Cosmetic");
    case UnlockCategory::Charge: return QStringLiteral("Charge");
    }
    return QString();
}

} // namespace

HubDialog::HubDialog(MetaProgression * meta, QWidget * parent) :
    QDialog(parent),
    mMeta(meta)
{
    Q_ASSERT(mMeta != nullptr);

    setWindowTitle(tr("Evolution Hub"));
    buildUi();

    connect(mMeta, &MetaProgression::evolutionPointsChanged, this, &HubDialog::refresh);
    connect(mMeta, &MetaProgression::unlockPurchased, this, [this](const QString &) { refresh(); });
    connect(mMeta, &MetaProgression::chargesChanged, this, [this](const QString &, int) { refresh(); });

    refresh();
}

void HubDialog::buildUi()
{
    auto * rootLayout = new QVBoxLayout(this);

    mEpLabel = new QLabel(this);
    QFont epFont = mEpLabel->font();
    epFont.setBold(true);
    epFont.setPointSize(epFont.pointSize() + 2);
    mEpLabel->setFont(epFont);
    rootLayout->addWidget(mEpLabel);

    // --- Unlock catalog / shop -----------------------------------------
    auto * shopBox = new QGroupBox(tr("Unlocks"), this);
    auto * shopLayout = new QVBoxLayout(shopBox);

    mCatalogList = new QListWidget(shopBox);
    shopLayout->addWidget(mCatalogList);

    mCatalogDescriptionLabel = new QLabel(shopBox);
    mCatalogDescriptionLabel->setWordWrap(true);
    shopLayout->addWidget(mCatalogDescriptionLabel);

    mPurchaseButton = new QPushButton(tr("Purchase"), shopBox);
    shopLayout->addWidget(mPurchaseButton);

    rootLayout->addWidget(shopBox);

    // --- Run setup --------------------------------------------------------
    auto * setupBox = new QGroupBox(tr("Run Setup"), this);
    auto * setupLayout = new QVBoxLayout(setupBox);

    auto * speciesRow = new QHBoxLayout();
    speciesRow->addWidget(new QLabel(tr("Starting Species:"), setupBox));
    mSpeciesCombo = new QComboBox(setupBox);
    speciesRow->addWidget(mSpeciesCombo, 1);
    setupLayout->addLayout(speciesRow);

    auto * biomeRow = new QHBoxLayout();
    biomeRow->addWidget(new QLabel(tr("Biome:"), setupBox));
    mBiomeCombo = new QComboBox(setupBox);
    biomeRow->addWidget(mBiomeCombo, 1);
    setupLayout->addLayout(biomeRow);

    auto * epochsRow = new QHBoxLayout();
    epochsRow->addWidget(new QLabel(tr("Target Epochs:"), setupBox));
    mTargetEpochsSpin = new QSpinBox(setupBox);
    mTargetEpochsSpin->setRange(1, 100);
    mTargetEpochsSpin->setValue(10);
    epochsRow->addWidget(mTargetEpochsSpin, 1);
    setupLayout->addLayout(epochsRow);

    rootLayout->addWidget(setupBox);

    mStartButton = new QPushButton(tr("Start Run"), this);
    rootLayout->addWidget(mStartButton);

    connect(mCatalogList, &QListWidget::currentItemChanged, this, &HubDialog::onCatalogSelectionChanged);
    connect(mPurchaseButton, &QPushButton::clicked, this, &HubDialog::onPurchaseClicked);
    connect(mSpeciesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HubDialog::onSpeciesComboChanged);
    connect(mBiomeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HubDialog::onBiomeComboChanged);
    connect(mTargetEpochsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &HubDialog::setTargetEpochs);
    connect(mStartButton, &QPushButton::clicked, this, &HubDialog::onStartRunClicked);
}

MetaProgression * HubDialog::metaProgression() const
{
    return mMeta;
}

void HubDialog::setAvailableSpecies(const QList<Species *> & species)
{
    mAvailableSpecies = species;
    populateSpeciesCombo();
    updateStartButtonState();
}

QList<Species *> HubDialog::availableSpecies() const
{
    return mAvailableSpecies;
}

Species * HubDialog::selectedSpecies() const
{
    return mSelectedSpecies;
}

PlantPattern HubDialog::selectedBiome() const
{
    return mSelectedBiome;
}

qint64 HubDialog::ticksPerEpoch() const
{
    return mTicksPerEpoch;
}

int HubDialog::targetEpochs() const
{
    return mTargetEpochsSpin != nullptr ? mTargetEpochsSpin->value() : 10;
}

void HubDialog::setTicksPerEpoch(qint64 ticks)
{
    mTicksPerEpoch = ticks;
}

void HubDialog::setTargetEpochs(int epochs)
{
    if (mTargetEpochsSpin != nullptr && mTargetEpochsSpin->value() != epochs)
    {
        mTargetEpochsSpin->setValue(epochs);
    }
}

void HubDialog::applyRunResult(const RunResult & result)
{
    mMeta->applyRunResult(result);
    refresh(); // Also covers the case where applyRunResult() earned 0 EP.
}

void HubDialog::refresh()
{
    updateEpLabel();
    populateCatalogList();
    populateSpeciesCombo();
    populateBiomeCombo();
    updatePurchaseButtonState();
    updateStartButtonState();
}

void HubDialog::updateEpLabel()
{
    mEpLabel->setText(tr("Evolution Points: %1").arg(mMeta->evolutionPoints()));
}

void HubDialog::populateCatalogList()
{
    const QString previouslySelectedId = mCatalogList->currentItem() != nullptr
        ? mCatalogList->currentItem()->data(Qt::UserRole).toString()
        : QString();

    mCatalogList->clear();

    for (const UnlockDefinition & def : mMeta->catalog())
    {
        QString label = QStringLiteral("[%1] %2 - %3 EP").arg(categoryLabel(def.category), def.displayName).arg(def.cost);

        if (def.category == UnlockCategory::Charge)
        {
            label += QStringLiteral(" (owned: %1/%2)").arg(mMeta->chargesRemaining(def.id)).arg(def.maxCharges);
        }
        else if (mMeta->isUnlocked(def.id))
        {
            label += QStringLiteral(" (owned)");
        }

        auto * item = new QListWidgetItem(label, mCatalogList);
        item->setData(Qt::UserRole, def.id);
        if (!mMeta->canUnlock(def.id))
        {
            item->setForeground(mCatalogList->palette().mid());
        }

        if (def.id == previouslySelectedId)
        {
            mCatalogList->setCurrentItem(item);
        }
    }
}

void HubDialog::populateSpeciesCombo()
{
    const QString previousName = mSpeciesCombo->count() > 0 ? mSpeciesCombo->currentData().toString() : QString();

    mSpeciesCombo->clear();
    mSelectableSpecies.clear();

    for (Species * species : mAvailableSpecies)
    {
        if (species == nullptr)
        {
            continue;
        }
        const QString unlockId = QStringLiteral("species.") + species->name();
        if (!mMeta->isUnlocked(unlockId))
        {
            continue;
        }
        mSpeciesCombo->addItem(species->name(), species->name());
        mSelectableSpecies.append(species);
    }

    int indexToSelect = 0;
    if (!previousName.isEmpty())
    {
        const int found = mSpeciesCombo->findData(previousName);
        if (found >= 0)
        {
            indexToSelect = found;
        }
    }

    if (mSpeciesCombo->count() > 0)
    {
        mSpeciesCombo->setCurrentIndex(indexToSelect);
        onSpeciesComboChanged(indexToSelect);
    }
    else
    {
        mSelectedSpecies = nullptr;
    }
}

void HubDialog::populateBiomeCombo()
{
    const PlantPattern previousPattern = mSelectedBiome;

    mBiomeCombo->clear();

    for (const UnlockDefinition & def : mMeta->catalog())
    {
        if (def.category != UnlockCategory::Biome || !mMeta->isUnlocked(def.id))
        {
            continue;
        }
        PlantPattern pattern;
        if (!biomeIdToPattern(def.id, pattern))
        {
            continue;
        }
        mBiomeCombo->addItem(def.displayName, static_cast<int>(pattern));
    }

    int indexToSelect = 0;
    for (int i = 0; i < mBiomeCombo->count(); ++i)
    {
        if (static_cast<PlantPattern>(mBiomeCombo->itemData(i).toInt()) == previousPattern)
        {
            indexToSelect = i;
            break;
        }
    }

    if (mBiomeCombo->count() > 0)
    {
        mBiomeCombo->setCurrentIndex(indexToSelect);
        onBiomeComboChanged(indexToSelect);
    }
}

void HubDialog::updatePurchaseButtonState()
{
    QListWidgetItem * current = mCatalogList->currentItem();
    if (current == nullptr)
    {
        mPurchaseButton->setEnabled(false);
        mCatalogDescriptionLabel->clear();
        return;
    }

    const QString id = current->data(Qt::UserRole).toString();
    const UnlockDefinition * def = mMeta->findDefinition(id);
    mPurchaseButton->setEnabled(def != nullptr && mMeta->canUnlock(id));

    if (def != nullptr)
    {
        mCatalogDescriptionLabel->setText(def->description);
    }
}

void HubDialog::updateStartButtonState()
{
    mStartButton->setEnabled(mSelectedSpecies != nullptr);
}

void HubDialog::onCatalogSelectionChanged()
{
    updatePurchaseButtonState();
}

void HubDialog::onPurchaseClicked()
{
    QListWidgetItem * current = mCatalogList->currentItem();
    if (current == nullptr)
    {
        return;
    }
    const QString id = current->data(Qt::UserRole).toString();
    mMeta->purchaseUnlock(id); // refresh() runs via the unlockPurchased/evolutionPointsChanged connections.
}

void HubDialog::onSpeciesComboChanged(int index)
{
    mSelectedSpecies = (index >= 0 && index < mSelectableSpecies.size()) ? mSelectableSpecies.at(index) : nullptr;
    updateStartButtonState();
}

void HubDialog::onBiomeComboChanged(int index)
{
    if (index < 0)
    {
        return;
    }
    mSelectedBiome = static_cast<PlantPattern>(mBiomeCombo->itemData(index).toInt());
}

void HubDialog::onStartRunClicked()
{
    if (mSelectedSpecies == nullptr)
    {
        return;
    }

    RunConfig config;
    config.playerSpecies = mSelectedSpecies;
    config.ticksPerEpoch = mTicksPerEpoch;
    config.targetEpochs = targetEpochs();

    mRunRequested = true;
    emit startRunRequested(config);
    accept();
}

void HubDialog::reject()
{
    if (!mRunRequested)
    {
        emit closedWithoutStartingRun();
    }
    QDialog::reject();
}
