#ifndef CUSTOMTABWIDGET_H
#define CUSTOMTABWIDGET_H
// CustomTabWidget.h
#include <QTabWidget>
#include "CustomTabBar.h"

class CustomTabWidget : public QTabWidget {
public:
    CustomTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {
        setTabBar(new CustomTabBar());
        setTabPosition(QTabWidget::West);
    }
};
#endif // CUSTOMTABWIDGET_H
