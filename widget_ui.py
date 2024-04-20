# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'widget.ui'
##
## Created by: Qt User Interface Compiler version 6.7.0
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QFrame, QHBoxLayout, QPlainTextEdit,
    QProgressBar, QPushButton, QSizePolicy, QSplitter,
    QTextEdit, QVBoxLayout, QWidget)

from utils.doubleqprogressbar import DoubleQProgressBar

class Ui_Widget(object):
    def setupUi(self, Widget):
        if not Widget.objectName():
            Widget.setObjectName(u"Widget")
        Widget.resize(328, 648)
        self.verticalLayout_2 = QVBoxLayout(Widget)
        self.verticalLayout_2.setSpacing(1)
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.verticalLayout_2.setContentsMargins(1, 1, 1, 1)
        self.splitter = QSplitter(Widget)
        self.splitter.setObjectName(u"splitter")
        self.splitter.setOrientation(Qt.Vertical)
        self.splitter.setChildrenCollapsible(True)
        self.layoutWidget = QWidget(self.splitter)
        self.layoutWidget.setObjectName(u"layoutWidget")
        self.verticalLayout = QVBoxLayout(self.layoutWidget)
        self.verticalLayout.setSpacing(3)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout.setContentsMargins(3, 3, 3, 3)
        self.horizontalLayout_4 = QHBoxLayout()
        self.horizontalLayout_4.setSpacing(0)
        self.horizontalLayout_4.setObjectName(u"horizontalLayout_4")
        self.load = QPushButton(self.layoutWidget)
        self.load.setObjectName(u"load")
        self.load.setMinimumSize(QSize(0, 50))
        self.load.setMaximumSize(QSize(16777215, 50))
        font = QFont()
        font.setFamilies([u"Arial"])
        font.setPointSize(12)
        self.load.setFont(font)

        self.horizontalLayout_4.addWidget(self.load)

        self.date = QPushButton(self.layoutWidget)
        self.date.setObjectName(u"date")
        self.date.setEnabled(False)
        self.date.setMinimumSize(QSize(0, 50))
        self.date.setMaximumSize(QSize(16777215, 50))
        self.date.setFont(font)

        self.horizontalLayout_4.addWidget(self.date)

        self.set = QPushButton(self.layoutWidget)
        self.set.setObjectName(u"set")
        self.set.setEnabled(False)
        self.set.setMinimumSize(QSize(50, 50))
        self.set.setMaximumSize(QSize(50, 50))
        self.set.setFont(font)

        self.horizontalLayout_4.addWidget(self.set)

        self.reset = QPushButton(self.layoutWidget)
        self.reset.setObjectName(u"reset")
        self.reset.setEnabled(False)
        self.reset.setMinimumSize(QSize(50, 50))
        self.reset.setMaximumSize(QSize(50, 50))

        self.horizontalLayout_4.addWidget(self.reset)


        self.verticalLayout.addLayout(self.horizontalLayout_4)

        self.output = QTextEdit(self.layoutWidget)
        self.output.setObjectName(u"output")
        self.output.setEnabled(True)
        self.output.setMinimumSize(QSize(320, 170))
        self.output.setMaximumSize(QSize(16777215, 16777215))
        font1 = QFont()
        font1.setFamilies([u"\u9ed1\u4f53"])
        font1.setPointSize(12)
        self.output.setFont(font1)
        self.output.setLineWidth(0)

        self.verticalLayout.addWidget(self.output)

        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setSpacing(0)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.input = QTextEdit(self.layoutWidget)
        self.input.setObjectName(u"input")
        self.input.setMinimumSize(QSize(0, 80))
        self.input.setMaximumSize(QSize(16777215, 80))
        font2 = QFont()
        font2.setPointSize(12)
        self.input.setFont(font2)
        self.input.setLineWidth(0)

        self.horizontalLayout.addWidget(self.input)

        self.send = QPushButton(self.layoutWidget)
        self.send.setObjectName(u"send")
        self.send.setEnabled(False)
        self.send.setMinimumSize(QSize(50, 80))
        self.send.setMaximumSize(QSize(50, 80))
        self.send.setFont(font)

        self.horizontalLayout.addWidget(self.send)


        self.verticalLayout.addLayout(self.horizontalLayout)

        self.splitter.addWidget(self.layoutWidget)
        self.layoutWidget1 = QWidget(self.splitter)
        self.layoutWidget1.setObjectName(u"layoutWidget1")
        self.horizontalLayout_5 = QHBoxLayout(self.layoutWidget1)
        self.horizontalLayout_5.setSpacing(0)
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.horizontalLayout_5.setContentsMargins(0, 0, 0, 0)
        self.frame_2 = QFrame(self.layoutWidget1)
        self.frame_2.setObjectName(u"frame_2")
        self.frame_2.setMinimumSize(QSize(80, 170))
        self.frame_2.setMaximumSize(QSize(80, 16777215))
        self.frame_2.setFrameShape(QFrame.Box)
        self.frame_2.setFrameShadow(QFrame.Sunken)
        self.frame_2.setLineWidth(1)
        self.verticalLayout_3 = QVBoxLayout(self.frame_2)
        self.verticalLayout_3.setSpacing(6)
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.verticalLayout_3.setContentsMargins(3, 3, 3, 3)
        self.kv_bar = DoubleQProgressBar(self.frame_2)
        self.kv_bar.setObjectName(u"kv_bar")
        font3 = QFont()
        font3.setFamilies([u"\u5fae\u8f6f\u96c5\u9ed1"])
        font3.setPointSize(9)
        self.kv_bar.setFont(font3)
        self.kv_bar.setLayoutDirection(Qt.LeftToRight)
        self.kv_bar.setValue(0)
        self.kv_bar.setAlignment(Qt.AlignCenter)
        self.kv_bar.setTextVisible(True)
        self.kv_bar.setOrientation(Qt.Horizontal)
        self.kv_bar.setTextDirection(QProgressBar.BottomToTop)

        self.verticalLayout_3.addWidget(self.kv_bar)

        self.cpu_bar = DoubleQProgressBar(self.frame_2)
        self.cpu_bar.setObjectName(u"cpu_bar")
        self.cpu_bar.setFont(font3)
        self.cpu_bar.setValue(0)
        self.cpu_bar.setAlignment(Qt.AlignCenter)

        self.verticalLayout_3.addWidget(self.cpu_bar)

        self.mem_bar = DoubleQProgressBar(self.frame_2)
        self.mem_bar.setObjectName(u"mem_bar")
        self.mem_bar.setFont(font3)
        self.mem_bar.setValue(0)
        self.mem_bar.setAlignment(Qt.AlignCenter)
        self.mem_bar.setTextVisible(True)
        self.mem_bar.setOrientation(Qt.Horizontal)
        self.mem_bar.setInvertedAppearance(False)
        self.mem_bar.setTextDirection(QProgressBar.TopToBottom)

        self.verticalLayout_3.addWidget(self.mem_bar)

        self.vcore_bar = DoubleQProgressBar(self.frame_2)
        self.vcore_bar.setObjectName(u"vcore_bar")
        self.vcore_bar.setFont(font3)
        self.vcore_bar.setValue(0)
        self.vcore_bar.setAlignment(Qt.AlignCenter)
        self.vcore_bar.setTextVisible(True)
        self.vcore_bar.setOrientation(Qt.Horizontal)
        self.vcore_bar.setInvertedAppearance(False)
        self.vcore_bar.setTextDirection(QProgressBar.TopToBottom)

        self.verticalLayout_3.addWidget(self.vcore_bar)

        self.vram_bar = DoubleQProgressBar(self.frame_2)
        self.vram_bar.setObjectName(u"vram_bar")
        self.vram_bar.setFont(font3)
        self.vram_bar.setValue(0)
        self.vram_bar.setAlignment(Qt.AlignCenter)
        self.vram_bar.setTextVisible(True)
        self.vram_bar.setOrientation(Qt.Horizontal)
        self.vram_bar.setInvertedAppearance(False)
        self.vram_bar.setTextDirection(QProgressBar.TopToBottom)

        self.verticalLayout_3.addWidget(self.vram_bar)


        self.horizontalLayout_5.addWidget(self.frame_2)

        self.state = QPlainTextEdit(self.layoutWidget1)
        self.state.setObjectName(u"state")
        self.state.setMinimumSize(QSize(0, 170))
        self.state.setMaximumSize(QSize(16777215, 16777215))
        font4 = QFont()
        font4.setPointSize(9)
        self.state.setFont(font4)
        self.state.setFrameShape(QFrame.NoFrame)
        self.state.setLineWidth(0)
        self.state.setReadOnly(True)

        self.horizontalLayout_5.addWidget(self.state)

        self.splitter.addWidget(self.layoutWidget1)

        self.verticalLayout_2.addWidget(self.splitter)


        self.retranslateUi(Widget)

        QMetaObject.connectSlotsByName(Widget)
    # setupUi

    def retranslateUi(self, Widget):
        Widget.setWindowTitle(QCoreApplication.translate("Widget", u"\u673a\u4f53", None))
        self.load.setText(QCoreApplication.translate("Widget", u"\u88c5\u8f7d", None))
        self.date.setText(QCoreApplication.translate("Widget", u"\u7ea6\u5b9a", None))
#if QT_CONFIG(tooltip)
        self.set.setToolTip("")
#endif // QT_CONFIG(tooltip)
        self.set.setText("")
#if QT_CONFIG(tooltip)
        self.reset.setToolTip("")
#endif // QT_CONFIG(tooltip)
        self.reset.setText("")
#if QT_CONFIG(tooltip)
        self.send.setToolTip("")
#endif // QT_CONFIG(tooltip)
        self.send.setText(QCoreApplication.translate("Widget", u"\u53d1\u9001", None))
#if QT_CONFIG(tooltip)
        self.kv_bar.setToolTip(QCoreApplication.translate("Widget", u"\u4e0a\u4e0b\u6587\u7f13\u5b58\u91cf 0 token", None))
#endif // QT_CONFIG(tooltip)
        self.kv_bar.setFormat(QCoreApplication.translate("Widget", u"\u8bb0\u5fc6%p%", None))
        self.cpu_bar.setFormat(QCoreApplication.translate("Widget", u"cpu%p%", None))
        self.mem_bar.setFormat(QCoreApplication.translate("Widget", u"\u5185\u5b58%p%", None))
        self.vcore_bar.setFormat(QCoreApplication.translate("Widget", u"gpu%p%", None))
        self.vram_bar.setFormat(QCoreApplication.translate("Widget", u"\u663e\u5b58%p%", None))
        self.state.setPlainText("")
        self.state.setPlaceholderText("")
    # retranslateUi

