#ifndef CHECKBOX_H
#define CHECKBOX_H

#include <QCheckBox>
#include <QPainter>

class CheckBox : public QCheckBox {
public:
    explicit CheckBox(const QString& text = "", QWidget* parent = nullptr)
        : QCheckBox(text, parent) {}
    
protected:
    void paintEvent(QPaintEvent* event) override {
        QCheckBox::paintEvent(event);
        
        if (isChecked()) {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            
            // Get indicator rect
            QStyleOptionButton option;
            initStyleOption(&option);
            QRect indicatorRect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, this);
            
            // Draw checkmark
            painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPointF p1(indicatorRect.left() + 4, indicatorRect.center().y());
            QPointF p2(indicatorRect.center().x(), indicatorRect.bottom() - 4);
            QPointF p3(indicatorRect.right() - 4, indicatorRect.top() + 4);
            
            painter.drawLine(p1, p2);
            painter.drawLine(p2, p3);
        }
    }
};

#endif // CHECKBOX_H

