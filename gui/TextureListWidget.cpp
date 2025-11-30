#include "TextureListWidget.h"
#include "../core/TXDConverter.h"
#include <QPixmap>
#include <QImage>
#include <QString>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QPalette>
#include <QStyle>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>

TextureListWidget::TextureListWidget(QWidget *parent)
    : QListWidget(parent) {
    setViewMode(QListWidget::ListMode);
    setIconSize(QSize(32, 32));
    setSpacing(2);
    setItemDelegate(new TextureListItemDelegate(this));
}

QString TextureListWidget::formatTextureInfo(const TXDTextureHeader* header) const {
    QString info = QString("Name: %1\nSize: %2x%3px\nHas alpha: %4\nCompression: %5")
        .arg(QString::fromStdString(header->getDiffuseName()))
        .arg(header->getWidth())
        .arg(header->getHeight())
        .arg(header->isAlphaChannelUsed() ? "Y" : "N");
    
    QString compressionStr;
    switch (header->getCompression()) {
        case TXDCompression::NONE:
            compressionStr = "None";
            break;
        case TXDCompression::DXT1:
            compressionStr = "DXT1";
            break;
        case TXDCompression::DXT3:
            compressionStr = "DXT3";
            break;
        default:
            compressionStr = "Unknown";
            break;
    }
    
    info = QString("Name: %1\nSize: %2x%3px\nHas alpha: %4\nCompression: %5")
        .arg(QString::fromStdString(header->getDiffuseName()))
        .arg(header->getWidth())
        .arg(header->getHeight())
        .arg(header->isAlphaChannelUsed() ? "Y" : "N")
        .arg(compressionStr);
    
    return info;
}

QPixmap TextureListWidget::createThumbnail(const TXDTextureHeader* header, const uint8_t* data) const {
    if (!header || !data) {
        return QPixmap();
    }
    
    // Convert to RGBA8
    auto rgbaData = TXDConverter::convertToRGBA8(header, data, 0);
    if (!rgbaData) {
        return QPixmap();
    }
    
    // Create QImage
    int width = header->getWidth();
    int height = header->getHeight();
    QImage image(rgbaData.get(), width, height, QImage::Format_RGBA8888);
    QImage imageCopy = image.copy();
    
    // Create thumbnail (32x32 max)
    QPixmap pixmap = QPixmap::fromImage(imageCopy);
    if (pixmap.width() > 32 || pixmap.height() > 32) {
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    return pixmap;
}

void TextureListWidget::addTexture(const TXDTextureHeader* header, const uint8_t* data, int index) {
    if (!header) {
        return;
    }
    
    QString info = formatTextureInfo(header);
    
    QListWidgetItem* item = new QListWidgetItem(info, this);
    
    // Create thumbnail
    QPixmap thumbnail = createThumbnail(header, data);
    if (!thumbnail.isNull()) {
        item->setIcon(QIcon(thumbnail));
    }
    
    // Store index as data
    item->setData(Qt::UserRole, index);
    
    // Set item height to accommodate multi-line text (reduced padding)
    item->setSizeHint(QSize(item->sizeHint().width(), 80));
    
    addItem(item);
}

void TextureListWidget::updateTexture(const TXDTextureHeader* header, const uint8_t* data, int index) {
    QListWidgetItem* item = itemAt(0, 0); // This is a simplified approach
    // In a real implementation, we'd find the item by index
    for (int i = 0; i < count(); i++) {
        QListWidgetItem* it = this->item(i);
        if (it && it->data(Qt::UserRole).toInt() == index) {
            item = it;
            break;
        }
    }
    
    if (item && header) {
        QString name = QString::fromStdString(header->getDiffuseName());
        if (name.isEmpty()) {
            name = QString("Texture %1").arg(index);
        }
        
        QString info = formatTextureInfo(header);
        QString displayText = QString("%1\n%2").arg(name).arg(info);
        
        item->setText(displayText);
        
        QPixmap thumbnail = createThumbnail(header, data);
        if (!thumbnail.isNull()) {
            item->setIcon(QIcon(thumbnail));
        }
    }
}

void TextureListWidget::clearTextures() {
    clear();
}

void TextureListWidget::contextMenuEvent(QContextMenuEvent* event) {
    QListWidgetItem* item = itemAt(event->pos());
    if (!item) {
        return; // No item under cursor
    }
    
    int index = item->data(Qt::UserRole).toInt();
    
    QMenu menu(this);
    
    QAction* exportAction = menu.addAction("Export...");
    QAction* importAction = menu.addAction("Import...");
    menu.addSeparator();
    QAction* replaceDiffuseAction = menu.addAction("Replace diffuse...");
    QAction* replaceAlphaAction = menu.addAction("Replace alpha...");
    menu.addSeparator();
    QAction* removeAction = menu.addAction("Remove");
    
    QAction* selectedAction = menu.exec(event->globalPos());
    
    if (selectedAction == exportAction) {
        emit exportRequested(index);
    } else if (selectedAction == importAction) {
        emit importRequested(index);
    } else if (selectedAction == replaceDiffuseAction) {
        emit replaceDiffuseRequested(index);
    } else if (selectedAction == replaceAlphaAction) {
        emit replaceAlphaRequested(index);
    } else if (selectedAction == removeAction) {
        emit removeRequested(index);
    }
}

