#ifndef TEXTUREPROPERTIESWIDGET_H
#define TEXTUREPROPERTIESWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QListView>
#include <QIntValidator>
#include "CheckBox.h"
#include "../core/TXDTextureHeader.h"

class TexturePropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit TexturePropertiesWidget(QWidget *parent = nullptr);
    void setTexture(TXDTextureHeader* header);
    void clear();

signals:
    void propertyChanged();

private slots:
    void onNameChanged();
    void onAlphaNameChanged();
    void onWidthChanged();
    void onHeightChanged();
    void onMipmapCountChanged();
    void onAlphaChannelToggled(bool enabled);

private:
    void updateUI();
    void blockSignals(bool block);
    
    TXDTextureHeader* currentHeader;
    
    QScrollArea* scrollArea;
    QWidget* contentWidget;
    
    QGroupBox* basicGroup;
    QLineEdit* nameEdit;
    QLineEdit* alphaNameEdit;
    QLineEdit* widthEdit;
    QLineEdit* heightEdit;
    QLineEdit* mipmapEdit;
    CheckBox* alphaCheck;
    
    QGroupBox* formatGroup;
    QComboBox* formatCombo;
    QComboBox* compressionCombo;
    
    QGroupBox* flagsGroup;
    QComboBox* filterCombo;
    QComboBox* uWrapCombo;
    QComboBox* vWrapCombo;
};

#endif // TEXTUREPROPERTIESWIDGET_H

