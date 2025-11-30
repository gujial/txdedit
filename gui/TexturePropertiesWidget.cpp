#include "TexturePropertiesWidget.h"
#include <QFormLayout>
#include <QLabel>
#include <QIntValidator>
#include <QFontMetrics>

TexturePropertiesWidget::TexturePropertiesWidget(QWidget *parent)
    : QWidget(parent), currentHeader(nullptr) {
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
    
    // Width input field with number validation
    widthEdit = new QLineEdit(contentWidget);
    widthEdit->setValidator(new QIntValidator(1, 4096, widthEdit));
    widthEdit->setText("256");
    connect(widthEdit, &QLineEdit::editingFinished, 
            this, &TexturePropertiesWidget::onWidthChanged);
    basicLayout->addRow("Width:", widthEdit);
    
    // Height input field with number validation
    heightEdit = new QLineEdit(contentWidget);
    heightEdit->setValidator(new QIntValidator(1, 4096, heightEdit));
    heightEdit->setText("256");
    connect(heightEdit, &QLineEdit::editingFinished, 
            this, &TexturePropertiesWidget::onHeightChanged);
    basicLayout->addRow("Height:", heightEdit);
    
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
    
    formatCombo = new QComboBox(contentWidget);
    QListView* formatView = new QListView();
    formatView->setSpacing(0);
    formatView->setUniformItemSizes(true);
    formatCombo->setView(formatView);
    formatCombo->setEditable(false);
    formatCombo->addItem("R8G8B8A8", RasterFormatR8G8B8A8);
    formatCombo->addItem("B8G8R8A8", RasterFormatB8G8R8A8);
    formatCombo->addItem("B8G8R8", RasterFormatB8G8R8);
    formatCombo->addItem("R5G6B5", RasterFormatR5G6B5);
    formatCombo->addItem("A1R5G5B5", RasterFormatA1R5G5B5);
    formatCombo->addItem("R4G4B4A4", RasterFormatR4G4B4A4);
    formatCombo->addItem("LUM8", RasterFormatLUM8);
    // Calculate width based on longest item text
    QFontMetrics fm(formatCombo->font());
    int maxWidth = 0;
    for (int i = 0; i < formatCombo->count(); i++) {
        int width = fm.horizontalAdvance(formatCombo->itemText(i));
        if (width > maxWidth) maxWidth = width;
    }
    formatCombo->view()->setMinimumWidth(maxWidth + 40); // Add padding for borders and scrollbar
    formatLayout->addRow("Raster format:", formatCombo);
    
    compressionCombo = new QComboBox(contentWidget);
    QListView* compressionView = new QListView();
    compressionCombo->setView(compressionView);
    compressionCombo->setEditable(false);
    compressionCombo->addItem("None", static_cast<int>(TXDCompression::NONE));
    compressionCombo->addItem("DXT1", static_cast<int>(TXDCompression::DXT1));
    compressionCombo->addItem("DXT3", static_cast<int>(TXDCompression::DXT3));
    // Calculate width based on longest item text
    QFontMetrics fmComp(compressionCombo->font());
    int maxWidthComp = 0;
    for (int i = 0; i < compressionCombo->count(); i++) {
        int width = fmComp.horizontalAdvance(compressionCombo->itemText(i));
        if (width > maxWidthComp) maxWidthComp = width;
    }
    compressionCombo->view()->setMinimumWidth(maxWidthComp + 40);
    formatLayout->addRow("Compression:", compressionCombo);
    
    // Connect format/compression changes
    connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentHeader) {
            uint32_t format = formatCombo->currentData().toUInt();
            TXDCompression comp = static_cast<TXDCompression>(compressionCombo->currentData().toInt());
            try {
                currentHeader->setRasterFormat(format, comp);
                // Don't emit propertyChanged for format changes
                // Raster format is metadata for saving, not for display
                // The preview should not be updated when format changes
            } catch (const std::exception&) {
                updateUI(); // Revert on error
            }
        }
    });
    
    connect(compressionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentHeader) {
            uint32_t format = formatCombo->currentData().toUInt();
            TXDCompression comp = static_cast<TXDCompression>(compressionCombo->currentData().toInt());
            try {
                currentHeader->setRasterFormat(format, comp);
                // Don't emit propertyChanged for compression-only changes
                // Compression is metadata for saving, not for display
                // The preview should not be updated when only compression changes
            } catch (const std::exception&) {
                updateUI(); // Revert on error
            }
        }
    });
    
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
    filterCombo->addItem("None", FilterNone);
    filterCombo->addItem("Nearest", FilterNearest);
    filterCombo->addItem("Linear", FilterLinear);
    filterCombo->addItem("Mip Nearest", FilterMipNearest);
    filterCombo->addItem("Mip Linear", FilterMipLinear);
    filterCombo->addItem("Linear Mip Nearest", FilterLinearMipNearest);
    filterCombo->addItem("Linear Mip Linear", FilterLinearMipLinear);
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
    uWrapCombo->addItem("None", WrapNone);
    uWrapCombo->addItem("Wrap", WrapWrap);
    uWrapCombo->addItem("Mirror", WrapMirror);
    uWrapCombo->addItem("Clamp", WrapClamp);
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
    vWrapCombo->addItem("None", WrapNone);
    vWrapCombo->addItem("Wrap", WrapWrap);
    vWrapCombo->addItem("Mirror", WrapMirror);
    vWrapCombo->addItem("Clamp", WrapClamp);
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
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentHeader) {
            currentHeader->setFilterFlags(filterCombo->currentData().toUInt());
            // Don't emit propertyChanged for filter changes
            // Filter is metadata for rendering, not for display
            // The preview should not be updated when filter changes
        }
    });
    
    connect(uWrapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentHeader) {
            currentHeader->setWrappingFlags(
                uWrapCombo->currentData().toUInt(),
                currentHeader->getVWrapFlags()
            );
            // Don't emit propertyChanged for wrap changes
            // Wrap is metadata for rendering, not for display
            // The preview should not be updated when wrap changes
        }
    });
    
    connect(vWrapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        if (currentHeader) {
            currentHeader->setWrappingFlags(
                currentHeader->getUWrapFlags(),
                vWrapCombo->currentData().toUInt()
            );
            // Don't emit propertyChanged for wrap changes
            // Wrap is metadata for rendering, not for display
            // The preview should not be updated when wrap changes
        }
    });
    
    contentLayout->addWidget(flagsGroup);
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    clear();
}

void TexturePropertiesWidget::setTexture(TXDTextureHeader* header) {
    currentHeader = header;
    if (header) {
        // Show all groups when texture is set
        basicGroup->show();
        formatGroup->show();
        flagsGroup->show();
    }
    updateUI();
}

void TexturePropertiesWidget::clear() {
    currentHeader = nullptr;
    blockSignals(true);
    
    nameEdit->clear();
    alphaNameEdit->clear();
    widthEdit->setText("256");
    heightEdit->setText("256");
    mipmapEdit->setText("1");
    alphaCheck->setChecked(false);
    formatCombo->setCurrentIndex(0);
    compressionCombo->setCurrentIndex(0);
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
    if (!currentHeader) {
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
    
    nameEdit->setText(QString::fromStdString(currentHeader->getDiffuseName()));
    alphaNameEdit->setText(QString::fromStdString(currentHeader->getAlphaName()));
    int width = currentHeader->getWidth();
    int height = currentHeader->getHeight();
    widthEdit->setText(QString::number(width));
    heightEdit->setText(QString::number(height));
    mipmapEdit->setText(QString::number(currentHeader->getMipmapCount()));
    alphaCheck->setChecked(currentHeader->isAlphaChannelUsed());
    
    // Set format combo
    uint32_t format = currentHeader->getRasterFormat();
    for (int i = 0; i < formatCombo->count(); i++) {
        if (formatCombo->itemData(i).toUInt() == format) {
            formatCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // Set compression combo
    TXDCompression comp = currentHeader->getCompression();
    for (int i = 0; i < compressionCombo->count(); i++) {
        if (compressionCombo->itemData(i).toInt() == static_cast<int>(comp)) {
            compressionCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // Set filter
    uint16_t filter = currentHeader->getFilterFlags();
    for (int i = 0; i < filterCombo->count(); i++) {
        if (filterCombo->itemData(i).toUInt() == filter) {
            filterCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // Set wrap modes
    uint8_t uWrap = currentHeader->getUWrapFlags();
    for (int i = 0; i < uWrapCombo->count(); i++) {
        if (uWrapCombo->itemData(i).toUInt() == uWrap) {
            uWrapCombo->setCurrentIndex(i);
            break;
        }
    }
    
    uint8_t vWrap = currentHeader->getVWrapFlags();
    for (int i = 0; i < vWrapCombo->count(); i++) {
        if (vWrapCombo->itemData(i).toUInt() == vWrap) {
            vWrapCombo->setCurrentIndex(i);
            break;
        }
    }
    
    blockSignals(false);
}

void TexturePropertiesWidget::blockSignals(bool block) {
    nameEdit->blockSignals(block);
    alphaNameEdit->blockSignals(block);
    widthEdit->blockSignals(block);
    heightEdit->blockSignals(block);
    mipmapEdit->blockSignals(block);
    alphaCheck->blockSignals(block);
    formatCombo->blockSignals(block);
    compressionCombo->blockSignals(block);
    filterCombo->blockSignals(block);
    uWrapCombo->blockSignals(block);
    vWrapCombo->blockSignals(block);
}

void TexturePropertiesWidget::onNameChanged() {
    if (currentHeader) {
        try {
            currentHeader->setDiffuseName(nameEdit->text().toStdString());
            emit propertyChanged();
        } catch (const std::exception& e) {
            // Revert on error
            nameEdit->setText(QString::fromStdString(currentHeader->getDiffuseName()));
        }
    }
}

void TexturePropertiesWidget::onAlphaNameChanged() {
    if (currentHeader) {
        try {
            currentHeader->setAlphaName(alphaNameEdit->text().toStdString());
            emit propertyChanged();
        } catch (const std::exception& e) {
            alphaNameEdit->setText(QString::fromStdString(currentHeader->getAlphaName()));
        }
    }
}

void TexturePropertiesWidget::onWidthChanged() {
    if (currentHeader) {
        bool ok;
        int value = widthEdit->text().toInt(&ok);
        if (ok && value >= 1 && value <= 4096) {
            currentHeader->setRasterSize(value, currentHeader->getHeight());
            emit propertyChanged();
        } else {
            // Revert to current value
            widthEdit->setText(QString::number(currentHeader->getWidth()));
        }
    }
}

void TexturePropertiesWidget::onHeightChanged() {
    if (currentHeader) {
        bool ok;
        int value = heightEdit->text().toInt(&ok);
        if (ok && value >= 1 && value <= 4096) {
            currentHeader->setRasterSize(currentHeader->getWidth(), value);
            emit propertyChanged();
        } else {
            // Revert to current value
            heightEdit->setText(QString::number(currentHeader->getHeight()));
        }
    }
}

void TexturePropertiesWidget::onMipmapCountChanged() {
    if (currentHeader) {
        bool ok;
        int value = mipmapEdit->text().toInt(&ok);
        if (ok && value >= 1 && value <= 16) {
            currentHeader->setMipmapCount(value);
            emit propertyChanged();
        } else {
            // Revert to current value
            mipmapEdit->setText(QString::number(currentHeader->getMipmapCount()));
        }
    }
}

void TexturePropertiesWidget::onAlphaChannelToggled(bool enabled) {
    if (currentHeader) {
        currentHeader->setAlphaChannelUsed(enabled);
        emit propertyChanged();
    }
}

