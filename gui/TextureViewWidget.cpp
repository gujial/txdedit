#include "TextureViewWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QBrush>
#include <QColor>
#include <QResizeEvent>
#include <QTimer>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QApplication>

TextureViewWidget::TextureViewWidget(QWidget *parent)
    : QWidget(parent), currentZoom(1.0), isPanning(false), hasBeenShown(false) {
    setupUI();
}

void TextureViewWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Graphics view
    scene = new QGraphicsScene(this);
    graphicsView = new QGraphicsView(scene, this);
    graphicsView->setDragMode(QGraphicsView::NoDrag); // We handle panning manually
    graphicsView->setRenderHint(QPainter::Antialiasing);
    graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    graphicsView->setBackgroundBrush(QBrush(QColor(26, 26, 26)));
    graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    graphicsView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    graphicsView->setMouseTracking(true); // Enable mouse tracking for panning
    
    // Enable keyboard focus for zoom shortcuts
    setFocusPolicy(Qt::StrongFocus);
    graphicsView->setFocusPolicy(Qt::StrongFocus);
    
    pixmapItem = new QGraphicsPixmapItem();
    scene->addItem(pixmapItem);
    
    // Install event filter on viewport to handle panning
    graphicsView->viewport()->installEventFilter(this);
    graphicsView->viewport()->setMouseTracking(true);
    
    mainLayout->addWidget(graphicsView);
    
    // Floating controls in bottom right
    floatingControls = new QWidget(this);
    floatingControls->setStyleSheet(
        "QWidget { background-color: rgba(30, 30, 30, 200); }"
    );
    floatingControls->setAttribute(Qt::WA_TranslucentBackground);
    
    QHBoxLayout* controlsLayout = new QHBoxLayout(floatingControls);
    controlsLayout->setContentsMargins(8, 8, 8, 8);
    controlsLayout->setSpacing(5);
    
    // Helper function to get icon path
    auto getIconPath = [](const QString& iconName) -> QString {
        // Use Qt resource system
        QString resourcePath = ":/icons/" + iconName;
        if (QFile::exists(resourcePath)) {
            return resourcePath;
        }
        return QString();
    };
    
    zoomInBtn = new QToolButton(floatingControls);
    zoomInBtn->setIcon(QIcon(getIconPath("zoom-in.png")));
    zoomInBtn->setIconSize(QSize(14, 14));
    zoomInBtn->setToolTip("Zoom in");
    zoomInBtn->setFixedSize(32, 32);
    zoomInBtn->setStyleSheet("QToolButton { background-color: rgba(42, 42, 42, 220); border: 1px solid #4a4a4a; color: #e0e0e0;  } QToolButton:hover { background-color: rgba(58, 58, 58, 240); border: 1px solid #ff8800; }");
    connect(zoomInBtn, &QToolButton::clicked, this, &TextureViewWidget::zoomIn);
    controlsLayout->addWidget(zoomInBtn);
    
    zoomOutBtn = new QToolButton(floatingControls);
    zoomOutBtn->setIcon(QIcon(getIconPath("zoom-out.png")));
    zoomOutBtn->setIconSize(QSize(14, 14));
    zoomOutBtn->setToolTip("Zoom out");
    zoomOutBtn->setFixedSize(32, 32);
    zoomOutBtn->setStyleSheet("QToolButton { background-color: rgba(42, 42, 42, 220); border: 1px solid #4a4a4a; color: #e0e0e0;  } QToolButton:hover { background-color: rgba(58, 58, 58, 240); border: 1px solid #ff8800; }");
    connect(zoomOutBtn, &QToolButton::clicked, this, &TextureViewWidget::zoomOut);
    controlsLayout->addWidget(zoomOutBtn);
    
    controlsLayout->addSpacing(5);
    
    zoomFitBtn = new QToolButton(floatingControls);
    zoomFitBtn->setIcon(QIcon(getIconPath("fit.png")));
    zoomFitBtn->setIconSize(QSize(14, 14));
    zoomFitBtn->setToolTip("Fit to window");
    zoomFitBtn->setFixedSize(32, 32);
    zoomFitBtn->setStyleSheet("QToolButton { background-color: rgba(42, 42, 42, 220); border: 1px solid #4a4a4a; color: #e0e0e0;  } QToolButton:hover { background-color: rgba(58, 58, 58, 240); border: 1px solid #ff8800; }");
    connect(zoomFitBtn, &QToolButton::clicked, this, &TextureViewWidget::zoomFit);
    controlsLayout->addWidget(zoomFitBtn);
    
    resetBtn = new QToolButton(floatingControls);
    resetBtn->setIcon(QIcon(getIconPath("reset.png")));
    resetBtn->setIconSize(QSize(14, 14));
    resetBtn->setToolTip("Reset view");
    resetBtn->setFixedSize(32, 32);
    resetBtn->setStyleSheet("QToolButton { background-color: rgba(42, 42, 42, 220); border: 1px solid #4a4a4a; color: #e0e0e0;  } QToolButton:hover { background-color: rgba(58, 58, 58, 240); border: 1px solid #ff8800; }");
    connect(resetBtn, &QToolButton::clicked, this, &TextureViewWidget::resetView);
    controlsLayout->addWidget(resetBtn);
    
    controlsLayout->addSpacing(5);
    
    zoomLabel = new QLabel("100%", floatingControls);
    zoomLabel->setMinimumWidth(50);
    zoomLabel->setAlignment(Qt::AlignCenter);
    zoomLabel->setStyleSheet("QLabel { color: #ff8800; font-weight: bold; background-color: transparent; }");
    controlsLayout->addWidget(zoomLabel);
    
    floatingControls->setLayout(controlsLayout);
    floatingControls->hide(); // Hide initially, show when texture is set
    // Keep as child of this widget, not viewport, so it stays fixed when panning
    floatingControls->raise();
}

void TextureViewWidget::setPixmap(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        clear();
        return;
    }
    
    pixmapItem->setPixmap(pixmap);
    scene->setSceneRect(pixmapItem->boundingRect());
    
    // Don't auto-zoom here - let the tab change handler or explicit zoom100() handle it
    // This ensures proper sizing when widget is first shown
    
    // Show floating controls
    floatingControls->show();
    updateFloatingControlsPosition();
    
    // Set focus to enable keyboard shortcuts
    setFocus();
}

void TextureViewWidget::clear() {
    pixmapItem->setPixmap(QPixmap());
    scene->setSceneRect(0, 0, 0, 0);
    resetView();
    floatingControls->hide();
    hasBeenShown = false;
}

void TextureViewWidget::zoomIn() {
    setZoomFactor(currentZoom * 1.2);
    updateFloatingControlsPosition();
}

void TextureViewWidget::zoomOut() {
    setZoomFactor(currentZoom / 1.2);
    updateFloatingControlsPosition();
}

void TextureViewWidget::zoomFit() {
    if (pixmapItem->pixmap().isNull()) {
        return;
    }
    
    QRectF sceneRect = scene->itemsBoundingRect();
    if (sceneRect.isEmpty()) {
        return;
    }
    
    graphicsView->fitInView(sceneRect, Qt::KeepAspectRatio);
    QTransform transform = graphicsView->transform();
    currentZoom = transform.m11();
    updateZoomLabel();
    updateFloatingControlsPosition();
}

void TextureViewWidget::zoom100() {
    setZoomFactor(1.0);
    updateFloatingControlsPosition();
}

void TextureViewWidget::resetView() {
    setZoomFactor(1.0);
    graphicsView->resetTransform();
    graphicsView->centerOn(0, 0);
    updateFloatingControlsPosition();
}

void TextureViewWidget::setZoomFactor(double factor) {
    if (factor < 0.1) factor = 0.1;
    if (factor > 5.0) factor = 5.0;
    
    currentZoom = factor;
    QTransform transform;
    transform.scale(factor, factor);
    graphicsView->setTransform(transform);
    
    updateZoomLabel();
    emit zoomChanged(factor);
}

void TextureViewWidget::updateFloatingControlsPosition() {
    if (!floatingControls || !graphicsView) {
        return;
    }
    
    floatingControls->adjustSize();
    QSize controlsSize = floatingControls->size();
    
    // Position relative to this widget, not the viewport, so it stays fixed when panning
    int margin = 15;
    int x = width() - controlsSize.width() - margin;
    int y = height() - controlsSize.height() - margin;
    
    floatingControls->move(x, y);
    floatingControls->raise();
}

void TextureViewWidget::updateZoomLabel() {
    zoomLabel->setText(QString("%1%").arg(static_cast<int>(currentZoom * 100)));
}

void TextureViewWidget::wheelEvent(QWheelEvent* event) {
    // Handle Ctrl+wheel zoom
    if (event->modifiers() & Qt::ControlModifier) {
        double scaleFactor = 1.15;
        if (event->angleDelta().y() > 0) {
            setZoomFactor(currentZoom * scaleFactor);
        } else {
            setZoomFactor(currentZoom / scaleFactor);
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void TextureViewWidget::mousePressEvent(QMouseEvent* event) {
    // Let the event filter handle panning on the viewport
    QWidget::mousePressEvent(event);
}

void TextureViewWidget::mouseMoveEvent(QMouseEvent* event) {
    // Let the event filter handle panning on the viewport
    QWidget::mouseMoveEvent(event);
}

void TextureViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    // Let the event filter handle panning on the viewport
    QWidget::mouseReleaseEvent(event);
}

void TextureViewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateFloatingControlsPosition();
}

void TextureViewWidget::keyPressEvent(QKeyEvent* event) {
    // Handle Ctrl/Cmd + Plus/Minus for zooming
    bool isZoomIn = (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal);
    bool isZoomOut = (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore);
    
    // Check for Ctrl (Windows/Linux) or Cmd (Mac)
    bool modifierPressed = (event->modifiers() & Qt::ControlModifier) || 
                           (event->modifiers() & Qt::MetaModifier);
    
    if (modifierPressed && isZoomIn) {
        zoomIn();
        event->accept();
        return;
    } else if (modifierPressed && isZoomOut) {
        zoomOut();
        event->accept();
        return;
    }
    
    QWidget::keyPressEvent(event);
}


bool TextureViewWidget::eventFilter(QObject* obj, QEvent* event) {
    // Handle mouse events on the graphics view viewport for panning
    if (obj == graphicsView->viewport()) {
        QMouseEvent* mouseEvent = nullptr;
        
        if (event->type() == QEvent::MouseButtonPress) {
            mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton) {
                isPanning = true;
                lastPanPoint = mouseEvent->pos();
                graphicsView->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            mouseEvent = static_cast<QMouseEvent*>(event);
            if (isPanning) {
                QPoint delta = mouseEvent->pos() - lastPanPoint;
                graphicsView->horizontalScrollBar()->setValue(
                    graphicsView->horizontalScrollBar()->value() - delta.x());
                graphicsView->verticalScrollBar()->setValue(
                    graphicsView->verticalScrollBar()->value() - delta.y());
                lastPanPoint = mouseEvent->pos();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            mouseEvent = static_cast<QMouseEvent*>(event);
            if (isPanning && (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton)) {
                isPanning = false;
                graphicsView->setCursor(Qt::ArrowCursor);
                event->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

