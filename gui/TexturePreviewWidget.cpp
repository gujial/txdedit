#include "TexturePreviewWidget.h"
#include "TextureViewWidget.h"
#include "../core/TXDConverter.h"
#include <QPixmap>
#include <QImage>
#include <QString>
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QTimer>
#include <QSizePolicy>
#include <QTabBar>

TexturePreviewWidget::TexturePreviewWidget(QWidget *parent)
    : QWidget(parent), alphaTabIndex(-1), mixedTabIndex(-1), alphaTabsVisible(false) {
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(0);
    
    // Create placeholder widget (shown when no texture)
    placeholderWidget = new QWidget(this);
    placeholderWidget->setStyleSheet("QWidget { background-color: #1a1a1a; }");
    mainLayout->addWidget(placeholderWidget);
    
    // Create tab widget (will be shown when texture is set)
    tabWidget = nullptr; // Create on demand
    imageView = nullptr;
    alphaView = nullptr;
    mixedView = nullptr;
    
    // Set dark background for the widget itself
    setStyleSheet("TexturePreviewWidget { background-color: #1a1a1a; }");
}

void TexturePreviewWidget::setTexture(const TXDTextureHeader* header, const uint8_t* data, int originalWidth, int originalHeight) {
    if (!header || !data) {
        clear();
        return;
    }
    
    // Create tab widget if it doesn't exist
    if (!tabWidget) {
        tabWidget = new QTabWidget(this);
        
        // Image tab (always visible)
        imageView = new TextureViewWidget(this);
        tabWidget->addTab(imageView, "Image");
        
        // Alpha and Mixed tabs will be added/removed dynamically based on alpha channel
        alphaView = new TextureViewWidget(this);
        mixedView = new TextureViewWidget(this);
        
        // Connect to tab change signal to reset views when first shown
        connect(tabWidget, &QTabWidget::currentChanged, this, &TexturePreviewWidget::onTabChanged);
        
        mainLayout->addWidget(tabWidget);
    }
    
    // Show tab widget when texture is set
    placeholderWidget->hide();
    tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tabWidget->show();
    
    // Check if texture has alpha channel
    bool hasAlpha = header->isAlphaChannelUsed();
    
    // Add or remove alpha/mixed tabs based on alpha channel
    if (hasAlpha && !alphaTabsVisible) {
        // Add alpha and mixed tabs
        alphaTabIndex = tabWidget->addTab(alphaView, "Alpha / mask");
        mixedTabIndex = tabWidget->addTab(mixedView, "Combined");
        alphaTabsVisible = true;
    } else if (!hasAlpha && alphaTabsVisible) {
        // Remove alpha and mixed tabs
        tabWidget->removeTab(mixedTabIndex);
        tabWidget->removeTab(alphaTabIndex);
        alphaTabIndex = -1;
        mixedTabIndex = -1;
        alphaTabsVisible = false;
    }
    
    // Reset hasBeenShown flags so each view resets to 100% when first shown
    imageView->resetHasBeenShown();
    if (hasAlpha) {
        alphaView->resetHasBeenShown();
        mixedView->resetHasBeenShown();
    }
    
    // Use original dimensions if provided, otherwise use header dimensions
    int origW = (originalWidth > 0) ? originalWidth : header->getWidth();
    int origH = (originalHeight > 0) ? originalHeight : header->getHeight();
    
    updateImageTab(header, data, origW, origH);
    if (hasAlpha) {
        updateAlphaTab(header, data, origW, origH);
        updateMixedTab(header, data, origW, origH);
    }
    
    // Reset current tab to 100% immediately
    int currentTab = tabWidget->currentIndex();
    if (currentTab == 0) {
        imageView->zoom100();
    } else if (hasAlpha && currentTab == alphaTabIndex) {
        alphaView->zoom100();
    } else if (hasAlpha && currentTab == mixedTabIndex) {
        mixedView->zoom100();
    }
}

void TexturePreviewWidget::updateImageTab(const TXDTextureHeader* header, const uint8_t* data, int originalWidth, int originalHeight) {
    QPixmap pixmap = createImagePixmap(header, data, false, false, originalWidth, originalHeight);
    imageView->setPixmap(pixmap);
}

void TexturePreviewWidget::updateAlphaTab(const TXDTextureHeader* header, const uint8_t* data, int originalWidth, int originalHeight) {
    QPixmap pixmap = createImagePixmap(header, data, true, false, originalWidth, originalHeight);
    alphaView->setPixmap(pixmap);
}

void TexturePreviewWidget::updateMixedTab(const TXDTextureHeader* header, const uint8_t* data, int originalWidth, int originalHeight) {
    QPixmap pixmap = createImagePixmap(header, data, false, true, originalWidth, originalHeight);
    mixedView->setPixmap(pixmap);
}

QPixmap TexturePreviewWidget::createImagePixmap(const TXDTextureHeader* header, const uint8_t* data, bool showAlpha, bool mixed, int targetWidth, int targetHeight) {
    if (!header || !data) {
        return QPixmap();
    }
    
    // Convert texture to RGBA8 using original dimensions from data
    // We need to create a temporary header with original dimensions for conversion
    TXDTextureHeader tempHeader(*header);
    if (targetWidth > 0 && targetHeight > 0) {
        // Use original dimensions for data conversion
        // The targetWidth/targetHeight are the original data dimensions
        tempHeader.setRasterSize(targetWidth, targetHeight);
    }
    
    auto rgbaData = TXDConverter::convertToRGBA8(&tempHeader, data, 0);
    if (!rgbaData) {
        return QPixmap();
    }
    
    // Use original dimensions for image creation
    int dataWidth = (targetWidth > 0) ? targetWidth : header->getWidth();
    int dataHeight = (targetHeight > 0) ? targetHeight : header->getHeight();
    
    // Create QImage from RGBA data (using original data dimensions)
    QImage image(rgbaData.get(), dataWidth, dataHeight, QImage::Format_RGBA8888);
    QImage imageCopy = image.copy();
    
    // Scale to target dimensions if they differ from data dimensions
    int targetW = header->getWidth();
    int targetH = header->getHeight();
    if (targetW != dataWidth || targetH != dataHeight) {
        imageCopy = imageCopy.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    
    // Get final dimensions after scaling
    int finalWidth = imageCopy.width();
    int finalHeight = imageCopy.height();
    
    if (showAlpha) {
        // Show only alpha channel as grayscale
        for (int y = 0; y < finalHeight; y++) {
            for (int x = 0; x < finalWidth; x++) {
                QRgb pixel = imageCopy.pixel(x, y);
                uint8_t alpha = qAlpha(pixel);
                imageCopy.setPixel(x, y, qRgb(alpha, alpha, alpha));
            }
        }
    } else if (mixed) {
        // Show RGB with alpha as checkerboard pattern
        // Create checkerboard pattern
        QPixmap checkerPattern(16, 16);
        checkerPattern.fill(Qt::lightGray);
        QPainter checkerPainter(&checkerPattern);
        checkerPainter.fillRect(0, 0, 8, 8, Qt::white);
        checkerPainter.fillRect(8, 8, 8, 8, Qt::white);
        checkerPainter.fillRect(0, 8, 8, 8, Qt::lightGray);
        checkerPainter.fillRect(8, 0, 8, 8, Qt::lightGray);
        checkerPainter.end();
        
        // Create background image with checkerboard
        QImage mixedImage(finalWidth, finalHeight, QImage::Format_ARGB32);
        QPainter bgPainter(&mixedImage);
        QBrush checkerBrush(checkerPattern);
        bgPainter.fillRect(0, 0, finalWidth, finalHeight, checkerBrush);
        bgPainter.end();
        
        // Composite the texture over the checkerboard
        QPainter painter(&mixedImage);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(0, 0, imageCopy);
        painter.end();
        
        imageCopy = mixedImage;
    }
    
    // Create pixmap
    QPixmap pixmap = QPixmap::fromImage(imageCopy);
    
    // Scale down if too large
    if (pixmap.width() > 512 || pixmap.height() > 512) {
        pixmap = pixmap.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    return pixmap;
}

void TexturePreviewWidget::clear() {
    if (imageView) imageView->clear();
    if (alphaView) alphaView->clear();
    if (mixedView) mixedView->clear();
    
    // Remove alpha/mixed tabs if they exist
    if (tabWidget && alphaTabsVisible) {
        tabWidget->removeTab(mixedTabIndex);
        tabWidget->removeTab(alphaTabIndex);
        alphaTabIndex = -1;
        mixedTabIndex = -1;
        alphaTabsVisible = false;
    }
    
    // Hide tab widget when no texture
    if (tabWidget) {
        tabWidget->hide();
        // Remove from layout to prevent it from taking up space
        mainLayout->removeWidget(tabWidget);
    }
    // Show placeholder
    placeholderWidget->show();
}

void TexturePreviewWidget::onTabChanged(int index) {
    // Reset the current view to 100% when its tab is first shown
    TextureViewWidget* currentView = nullptr;
    if (index == 0) {
        currentView = imageView;
    } else if (alphaTabsVisible && index == alphaTabIndex) {
        currentView = alphaView;
    } else if (alphaTabsVisible && index == mixedTabIndex) {
        currentView = mixedView;
    }
    
    if (currentView && !currentView->hasBeenShownOnce()) {
        // Use QTimer to ensure widget is properly sized before zooming
        QTimer::singleShot(0, [currentView]() {
            currentView->zoom100();
            currentView->setHasBeenShownOnce(true);
        });
    }
}
