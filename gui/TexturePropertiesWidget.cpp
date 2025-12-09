#include "TexturePropertiesWidget.h"
#include "libtxd/txd_converter.h"
#include "TXDModel.h"
#include <QFormLayout>
#include <QLabel>
#include <QIntValidator>
#include <QFontMetrics>
#include <cstring>

TexturePropertiesWidget::TexturePropertiesWidget(QWidget *parent)
    : QWidget(parent), currentEntry(nullptr) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Scroll area for properties
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    contentWidget = new QWidget;
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(10, 10, 10, 10);
    contentLayout->setSpacing(8);
    contentLayout->setAlignment(Qt::AlignTop);
    
    // Basic properties
    basicGroup = new QGroupBox("Basic properties", contentWidget);
    basicGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFormLayout* basicLayout = new QFormLayout(basicGroup);
    basicLayout->setSpacing(8);
    basicLayout->setLabelAlignment(Qt::AlignRight);
    basicLayout->setContentsMargins(10, 15, 10, 10);
    basicLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    nameEdit = new QLineEdit(contentWidget);
    connect(nameEdit, &QLineEdit::textChanged, this, &TexturePropertiesWidget::onNameChanged);
    basicLayout->addRow("Diffuse name:", nameEdit);
    
    alphaNameEdit = new QLineEdit(contentWidget);
    connect(alphaNameEdit, &QLineEdit::textChanged, this, &TexturePropertiesWidget::onAlphaNameChanged);
    basicLayout->addRow("Alpha name:", alphaNameEdit);
    
    // Width label (read-only: dimensions come from texture data)
    widthLabel = new QLabel("256", contentWidget);
    basicLayout->addRow("Width:", widthLabel);
    
    // Height label (read-only: dimensions come from texture data)
    heightLabel = new QLabel("256", contentWidget);
    basicLayout->addRow("Height:", heightLabel);
    
    // Mipmap input field with number validation
    mipmapEdit = new QLineEdit(contentWidget);
    mipmapEdit->setValidator(new QIntValidator(1, 16, mipmapEdit));
    mipmapEdit->setText("1");
    connect(mipmapEdit, &QLineEdit::editingFinished, 
            this, &TexturePropertiesWidget::onMipmapCountChanged);
    basicLayout->addRow("Mipmaps:", mipmapEdit);
    
    alphaCheck = new CheckBox("", contentWidget);
    connect(alphaCheck, &QCheckBox::toggled, this, &TexturePropertiesWidget::onAlphaChannelToggled);
    basicLayout->addRow("Has alpha:", alphaCheck);
    
    contentLayout->addWidget(basicGroup);
    
    // Format properties
    formatGroup = new QGroupBox("Format", contentWidget);
    formatGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFormLayout* formatLayout = new QFormLayout(formatGroup);
    formatLayout->setSpacing(8);
    formatLayout->setLabelAlignment(Qt::AlignRight);
    formatLayout->setContentsMargins(10, 15, 10, 10);
    formatLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    // Format label for existing textures (read-only)
    formatLabel = new QLabel("", contentWidget);
    formatLayout->addRow("Raster format:", formatLabel);
    
    // Format combo for new textures (editable, though auto-set based on alpha)
    formatCombo = new QComboBox(contentWidget);
    QListView* formatView = new QListView();
    formatView->setSpacing(0);
    formatView->setUniformItemSizes(true);
    formatCombo->setView(formatView);
    formatCombo->setEditable(false);
    // Note: libtxd uses B8G8R8A8, not R8G8B8A8
    formatCombo->addItem("B8G8R8A8", static_cast<uint32_t>(LibTXD::RasterFormat::B8G8R8A8));
    formatCombo->addItem("B8G8R8", static_cast<uint32_t>(LibTXD::RasterFormat::B8G8R8));
    formatCombo->addItem("R5G6B5", static_cast<uint32_t>(LibTXD::RasterFormat::R5G6B5));
    formatCombo->addItem("A1R5G5B5", static_cast<uint32_t>(LibTXD::RasterFormat::A1R5G5B5));
    formatCombo->addItem("R4G4B4A4", static_cast<uint32_t>(LibTXD::RasterFormat::R4G4B4A4));
    formatCombo->addItem("LUM8", static_cast<uint32_t>(LibTXD::RasterFormat::LUM8));
    // Calculate width based on longest item text
    QFontMetrics fm(formatCombo->font());
    int maxWidth = 0;
    for (int i = 0; i < formatCombo->count(); i++) {
        int width = fm.horizontalAdvance(formatCombo->itemText(i));
        if (width > maxWidth) maxWidth = width;
    }
    formatCombo->view()->setMinimumWidth(maxWidth + 40); // Add padding for borders and scrollbar
    formatCombo->hide(); // Hidden by default, shown only for new textures
    
    compressionCheck = new CheckBox("", contentWidget);
    formatLayout->addRow("Compression:", compressionCheck);
    
    // Connect format/compression changes
    // Note: Format changes are only allowed for new textures (added by user)
    // Existing textures keep their original format
    connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (!this || !currentEntry) {
            return; // Widget or entry is being destroyed
        }
        if (currentEntry->isNew) {
            // Format changes for new textures could be implemented here if needed
            // For now, format is set automatically based on alpha when texture is added
            // Don't emit propertyChanged for format changes
        }
    });
    
    connect(compressionCheck, &QCheckBox::toggled, this, &TexturePropertiesWidget::onCompressionToggled);
    
    contentLayout->addWidget(formatGroup);
    
    // Flags
    flagsGroup = new QGroupBox("Flags", contentWidget);
    flagsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFormLayout* flagsLayout = new QFormLayout(flagsGroup);
    flagsLayout->setSpacing(8);
    flagsLayout->setLabelAlignment(Qt::AlignRight);
    flagsLayout->setContentsMargins(10, 15, 10, 10);
    flagsLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    
    filterCombo = new QComboBox(contentWidget);
    QListView* filterView = new QListView();
    filterView->setSpacing(0);
    filterView->setUniformItemSizes(true);
    filterCombo->setView(filterView);
    filterCombo->setEditable(false);
    // Note: libtxd doesn't have separate filter flags enum, using raw values
    filterCombo->addItem("None", 0);
    filterCombo->addItem("Nearest", 1);
    filterCombo->addItem("Linear", 2);
    filterCombo->addItem("Mip Nearest", 3);
    filterCombo->addItem("Mip Linear", 4);
    filterCombo->addItem("Linear Mip Nearest", 5);
    filterCombo->addItem("Linear Mip Linear", 6);
    // Calculate width based on longest item text
    QFontMetrics fmFilter(filterCombo->font());
    int maxWidthFilter = 0;
    for (int i = 0; i < filterCombo->count(); i++) {
        int width = fmFilter.horizontalAdvance(filterCombo->itemText(i));
        if (width > maxWidthFilter) maxWidthFilter = width;
    }
    filterCombo->view()->setMinimumWidth(maxWidthFilter + 40);
    flagsLayout->addRow("Filter:", filterCombo);
    
    uWrapCombo = new QComboBox(contentWidget);
    QListView* uWrapView = new QListView();
    uWrapView->setSpacing(0);
    uWrapView->setUniformItemSizes(true);
    uWrapCombo->setView(uWrapView);
    uWrapCombo->setEditable(false);
    // Note: libtxd doesn't have separate wrap flags enum, using raw values
    uWrapCombo->addItem("None", 0);
    uWrapCombo->addItem("Wrap", 1);
    uWrapCombo->addItem("Mirror", 2);
    uWrapCombo->addItem("Clamp", 3);
    // Calculate width based on longest item text
    QFontMetrics fmUWrap(uWrapCombo->font());
    int maxWidthUWrap = 0;
    for (int i = 0; i < uWrapCombo->count(); i++) {
        int width = fmUWrap.horizontalAdvance(uWrapCombo->itemText(i));
        if (width > maxWidthUWrap) maxWidthUWrap = width;
    }
    uWrapCombo->view()->setMinimumWidth(maxWidthUWrap + 40);
    flagsLayout->addRow("U wrap:", uWrapCombo);
    
    vWrapCombo = new QComboBox(contentWidget);
    QListView* vWrapView = new QListView();
    vWrapView->setSpacing(0);
    vWrapView->setUniformItemSizes(true);
    vWrapCombo->setView(vWrapView);
    vWrapCombo->setEditable(false);
    // Note: libtxd doesn't have separate wrap flags enum, using raw values
    vWrapCombo->addItem("None", 0);
    vWrapCombo->addItem("Wrap", 1);
    vWrapCombo->addItem("Mirror", 2);
    vWrapCombo->addItem("Clamp", 3);
    // Calculate width based on longest item text
    QFontMetrics fmVWrap(vWrapCombo->font());
    int maxWidthVWrap = 0;
    for (int i = 0; i < vWrapCombo->count(); i++) {
        int width = fmVWrap.horizontalAdvance(vWrapCombo->itemText(i));
        if (width > maxWidthVWrap) maxWidthVWrap = width;
    }
    vWrapCombo->view()->setMinimumWidth(maxWidthVWrap + 40);
    flagsLayout->addRow("V wrap:", vWrapCombo);
    
    // Connect flag changes
    // Note: libtxd only has filterFlags as a single uint32_t, not separate wrap flags
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentEntry) {
            currentEntry->filterFlags = filterCombo->currentData().toUInt();
            // Don't emit propertyChanged for filter changes
            // Filter is metadata for rendering, not for display
        }
    });
    
    // Note: libtxd doesn't support separate U/V wrap flags yet
    // These combos are kept for UI consistency but won't modify the texture
    connect(uWrapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        // Not implemented in libtxd yet
    });
    
    connect(vWrapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        // Not implemented in libtxd yet
    });
    
    contentLayout->addWidget(flagsGroup);
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    clear();
}

void TexturePropertiesWidget::setTexture(TXDFileEntry* entry) {
    currentEntry = entry;
    if (entry) {
        // Show all groups when texture is set
        basicGroup->show();
        formatGroup->show();
        flagsGroup->show();
    }
    updateUI();
}

void TexturePropertiesWidget::clear() {
    currentEntry = nullptr;
    blockSignals(true);
    
    nameEdit->clear();
    alphaNameEdit->clear();
    widthLabel->setText("256");
    heightLabel->setText("256");
    mipmapEdit->setText("1");
    alphaCheck->setChecked(false);
    formatLabel->setText("");
    // Block signals before setting index to avoid triggering handlers during destruction
    formatCombo->blockSignals(true);
    formatCombo->setCurrentIndex(0);
    formatCombo->blockSignals(false);
    formatCombo->hide();
    formatLabel->hide();
    compressionCheck->setChecked(false);
    filterCombo->setCurrentIndex(0);
    uWrapCombo->setCurrentIndex(0);
    vWrapCombo->setCurrentIndex(0);
    
    basicGroup->setEnabled(false);
    formatGroup->setEnabled(false);
    flagsGroup->setEnabled(false);
    
    // Hide all groups when no texture is selected
    basicGroup->hide();
    formatGroup->hide();
    flagsGroup->hide();
    
    blockSignals(false);
}

void TexturePropertiesWidget::updateUI() {
    if (!currentEntry) {
        clear();
        return;
    }
    
    blockSignals(true);
    
    // Ensure groups are visible when updating
    basicGroup->show();
    formatGroup->show();
    flagsGroup->show();
    
    basicGroup->setEnabled(true);
    formatGroup->setEnabled(true);
    flagsGroup->setEnabled(true);
    
    nameEdit->setText(currentEntry->name);
    alphaNameEdit->setText(currentEntry->maskName);
    
    // Get dimensions from entry (always read-only, display as label)
    widthLabel->setText(QString::number(currentEntry->width));
    heightLabel->setText(QString::number(currentEntry->height));
    
    mipmapEdit->setText(QString::number(currentEntry->mipmapCount));
    alphaCheck->setChecked(currentEntry->hasAlpha);
    
    // Set format display
    LibTXD::RasterFormat format = currentEntry->rasterFormat;
    QString formatText;
    // Convert format enum to text - mask out flags to get base format
    uint32_t formatValue = static_cast<uint32_t>(format);
    uint32_t baseFormat = formatValue & static_cast<uint32_t>(LibTXD::RasterFormat::MASK);
    
    if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::B8G8R8A8)) {
        formatText = "B8G8R8A8";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::B8G8R8)) {
        formatText = "B8G8R8";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::R5G6B5)) {
        formatText = "R5G6B5";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::A1R5G5B5)) {
        formatText = "A1R5G5B5";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::R4G4B4A4)) {
        formatText = "R4G4B4A4";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::LUM8)) {
        formatText = "LUM8";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::R5G5B5)) {
        formatText = "R5G5B5";
    } else if (baseFormat == static_cast<uint32_t>(LibTXD::RasterFormat::DEFAULT)) {
        formatText = "Default";
    } else {
        formatText = QString("Unknown (0x%1)").arg(baseFormat, 4, 16, QChar('0')).toUpper();
    }
    
    // Format is read-only for both new and existing textures (auto-set for new, preserved for existing)
    // Always show as label (plain text)
    formatCombo->hide();
    formatLabel->show();
    formatLabel->setText(formatText);
    
    // Set compression checkbox
    compressionCheck->setChecked(currentEntry->compressionEnabled);
    
    // Set filter
    uint32_t filter = currentEntry->filterFlags;
    for (int i = 0; i < filterCombo->count(); i++) {
        if (filterCombo->itemData(i).toUInt() == filter) {
            filterCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // Note: libtxd doesn't support separate U/V wrap flags yet
    // Keep combos at default values
    uWrapCombo->setCurrentIndex(0);
    vWrapCombo->setCurrentIndex(0);
    
    blockSignals(false);
}

void TexturePropertiesWidget::blockSignals(bool block) {
    nameEdit->blockSignals(block);
    alphaNameEdit->blockSignals(block);
    mipmapEdit->blockSignals(block);
    alphaCheck->blockSignals(block);
    formatCombo->blockSignals(block);
    compressionCheck->blockSignals(block);
    filterCombo->blockSignals(block);
    uWrapCombo->blockSignals(block);
    vWrapCombo->blockSignals(block);
}

void TexturePropertiesWidget::onNameChanged() {
    if (currentEntry) {
        currentEntry->name = nameEdit->text();
        emit propertyChanged();
    }
}

void TexturePropertiesWidget::onAlphaNameChanged() {
    if (currentEntry) {
        currentEntry->maskName = alphaNameEdit->text();
        emit propertyChanged();
    }
}

void TexturePropertiesWidget::onMipmapCountChanged() {
    if (currentEntry) {
        bool ok;
        int value = mipmapEdit->text().toInt(&ok);
        if (ok && value >= 1 && value <= 16) {
            // Mipmap count is read-only - revert to current value
            mipmapEdit->setText(QString::number(currentEntry->mipmapCount));
        } else {
            // Revert to current value
            mipmapEdit->setText(QString::number(currentEntry->mipmapCount));
        }
    }
}

void TexturePropertiesWidget::onAlphaChannelToggled(bool enabled) {
    if (!currentEntry) {
        return;
    }
    
    // Update alpha channel in RGBA data
    if (!currentEntry->diffuse.empty() && currentEntry->diffuse.size() == currentEntry->width * currentEntry->height * 4) {
        if (enabled) {
            // Alpha enabled - just set alpha to 255 (fully opaque)
            for (size_t i = 3; i < currentEntry->diffuse.size(); i += 4) {
                currentEntry->diffuse[i] = 255;
            }
        } else {
            // Alpha disabled - composite RGB onto black background
            // Formula: result = source * (alpha/255) + background * (1 - alpha/255)
            // Background is black (0, 0, 0)
            for (size_t i = 0; i < currentEntry->diffuse.size(); i += 4) {
                float alpha = currentEntry->diffuse[i + 3] / 255.0f;
                // Composite RGB onto black
                currentEntry->diffuse[i] = static_cast<uint8_t>(currentEntry->diffuse[i] * alpha);     // R
                currentEntry->diffuse[i + 1] = static_cast<uint8_t>(currentEntry->diffuse[i + 1] * alpha); // G
                currentEntry->diffuse[i + 2] = static_cast<uint8_t>(currentEntry->diffuse[i + 2] * alpha); // B
                currentEntry->diffuse[i + 3] = 255; // Fully opaque after compositing
            }
        }
    }
    
    // Update alpha flag
    currentEntry->hasAlpha = enabled;
    
    emit propertyChanged();
}

void TexturePropertiesWidget::onCompressionToggled(bool enabled) {
    if (!currentEntry) {
        return;
    }
    
    // Just update the flag - compression happens on save
    currentEntry->compressionEnabled = enabled;
    
    emit propertyChanged();
}

