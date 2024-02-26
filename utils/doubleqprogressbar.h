#ifndef DOUBLEQPROGRESSBAR_H
#define DOUBLEQPROGRESSBAR_H

#include <QProgressBar>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QGraphicsDropShadowEffect>

class DoubleQProgressBar : public QProgressBar
{
public:
    DoubleQProgressBar(QWidget *parent = nullptr);
    void paintEvent(QPaintEvent *event) override;
    int m_secondValue=0;
    void setSecondValue(int value);
    QString message;
};

#endif // DOUBLEQPROGRESSBAR_H
