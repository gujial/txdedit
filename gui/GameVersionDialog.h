#ifndef GAMEVERSIONDIALOG_H
#define GAMEVERSIONDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../core/TXDTypes.h"

class GameVersionDialog : public QDialog {
    Q_OBJECT

public:
    explicit GameVersionDialog(QWidget *parent = nullptr);
    GTAGameVersion getSelectedVersion() const { return selectedVersion; }

private slots:
    void onGTA3Selected();
    void onGTAVCSelected();
    void onGTASASelected();

private:
    GTAGameVersion selectedVersion = GTAGameVersion::UNKNOWN;
    QPushButton* gta3Button;
    QPushButton* gtavcButton;
    QPushButton* gtasaButton;
    QLabel* instructionLabel;
    
    void setupUI();
    QString getLogoPath(const QString& logoName) const;
};

#endif // GAMEVERSIONDIALOG_H

