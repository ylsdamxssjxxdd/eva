# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'expend.ui'
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
from PySide6.QtWidgets import (QApplication, QComboBox, QDoubleSpinBox, QFrame,
    QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QPlainTextEdit, QPushButton,
    QRadioButton, QSizePolicy, QSpacerItem, QSpinBox,
    QTabWidget, QTableWidget, QTableWidgetItem, QTextEdit,
    QVBoxLayout, QWidget)

class Ui_Expend(object):
    def setupUi(self, Expend):
        if not Expend.objectName():
            Expend.setObjectName(u"Expend")
        Expend.resize(820, 650)
        self.horizontalLayout_46 = QHBoxLayout(Expend)
        self.horizontalLayout_46.setSpacing(0)
        self.horizontalLayout_46.setObjectName(u"horizontalLayout_46")
        self.horizontalLayout_46.setContentsMargins(0, 0, 0, 0)
        self.tabWidget = QTabWidget(Expend)
        self.tabWidget.setObjectName(u"tabWidget")
        font = QFont()
        font.setFamilies([u"Arial"])
        font.setPointSize(10)
        self.tabWidget.setFont(font)
        self.tab_1 = QWidget()
        self.tab_1.setObjectName(u"tab_1")
        self.verticalLayout_4 = QVBoxLayout(self.tab_1)
        self.verticalLayout_4.setSpacing(0)
        self.verticalLayout_4.setObjectName(u"verticalLayout_4")
        self.verticalLayout_4.setContentsMargins(0, 0, 0, 0)
        self.info_card = QTextEdit(self.tab_1)
        self.info_card.setObjectName(u"info_card")

        self.verticalLayout_4.addWidget(self.info_card)

        self.tabWidget.addTab(self.tab_1, "")
        self.tab_2 = QWidget()
        self.tab_2.setObjectName(u"tab_2")
        self.verticalLayout_3 = QVBoxLayout(self.tab_2)
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.verticalLayout_3.setContentsMargins(0, 0, 0, 0)
        self.vocab_card = QPlainTextEdit(self.tab_2)
        self.vocab_card.setObjectName(u"vocab_card")
        self.vocab_card.setFont(font)

        self.verticalLayout_3.addWidget(self.vocab_card)

        self.tabWidget.addTab(self.tab_2, "")
        self.tab_3 = QWidget()
        self.tab_3.setObjectName(u"tab_3")
        self.horizontalLayout = QHBoxLayout(self.tab_3)
        self.horizontalLayout.setSpacing(0)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.horizontalLayout.setContentsMargins(0, 0, 0, 0)
        self.modellog_card = QPlainTextEdit(self.tab_3)
        self.modellog_card.setObjectName(u"modellog_card")
        self.modellog_card.setFont(font)

        self.horizontalLayout.addWidget(self.modellog_card)

        self.tabWidget.addTab(self.tab_3, "")
        self.tab_4 = QWidget()
        self.tab_4.setObjectName(u"tab_4")
        self.verticalLayout = QVBoxLayout(self.tab_4)
        self.verticalLayout.setSpacing(0)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_frame1 = QFrame(self.tab_4)
        self.model_quantize_frame1.setObjectName(u"model_quantize_frame1")
        self.model_quantize_frame1.setMinimumSize(QSize(0, 31))
        self.model_quantize_frame1.setMaximumSize(QSize(16777215, 31))
        self.model_quantize_frame1.setFrameShape(QFrame.StyledPanel)
        self.model_quantize_frame1.setFrameShadow(QFrame.Raised)
        self.model_quantize_frame1.setLineWidth(0)
        self.horizontalLayout_12 = QHBoxLayout(self.model_quantize_frame1)
        self.horizontalLayout_12.setSpacing(0)
        self.horizontalLayout_12.setObjectName(u"horizontalLayout_12")
        self.horizontalLayout_12.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_label = QLabel(self.model_quantize_frame1)
        self.model_quantize_label.setObjectName(u"model_quantize_label")
        self.model_quantize_label.setMinimumSize(QSize(100, 30))
        self.model_quantize_label.setMaximumSize(QSize(100, 30))
        self.model_quantize_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_12.addWidget(self.model_quantize_label)

        self.model_quantize_row_modelpath_lineedit = QLineEdit(self.model_quantize_frame1)
        self.model_quantize_row_modelpath_lineedit.setObjectName(u"model_quantize_row_modelpath_lineedit")
        self.model_quantize_row_modelpath_lineedit.setMinimumSize(QSize(0, 30))
        self.model_quantize_row_modelpath_lineedit.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_12.addWidget(self.model_quantize_row_modelpath_lineedit)

        self.model_quantize_row_modelpath_pushButton = QPushButton(self.model_quantize_frame1)
        self.model_quantize_row_modelpath_pushButton.setObjectName(u"model_quantize_row_modelpath_pushButton")
        self.model_quantize_row_modelpath_pushButton.setMinimumSize(QSize(80, 30))
        self.model_quantize_row_modelpath_pushButton.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_12.addWidget(self.model_quantize_row_modelpath_pushButton)


        self.verticalLayout.addWidget(self.model_quantize_frame1)

        self.model_quantize_frame2 = QFrame(self.tab_4)
        self.model_quantize_frame2.setObjectName(u"model_quantize_frame2")
        self.model_quantize_frame2.setMinimumSize(QSize(0, 31))
        self.model_quantize_frame2.setMaximumSize(QSize(16777215, 31))
        self.model_quantize_frame2.setFrameShape(QFrame.StyledPanel)
        self.model_quantize_frame2.setFrameShadow(QFrame.Raised)
        self.model_quantize_frame2.setLineWidth(0)
        self.horizontalLayout_13 = QHBoxLayout(self.model_quantize_frame2)
        self.horizontalLayout_13.setSpacing(0)
        self.horizontalLayout_13.setObjectName(u"horizontalLayout_13")
        self.horizontalLayout_13.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_label_2 = QLabel(self.model_quantize_frame2)
        self.model_quantize_label_2.setObjectName(u"model_quantize_label_2")
        self.model_quantize_label_2.setMinimumSize(QSize(100, 30))
        self.model_quantize_label_2.setMaximumSize(QSize(100, 30))
        self.model_quantize_label_2.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_13.addWidget(self.model_quantize_label_2)

        self.model_quantize_important_datapath_lineedit = QLineEdit(self.model_quantize_frame2)
        self.model_quantize_important_datapath_lineedit.setObjectName(u"model_quantize_important_datapath_lineedit")
        self.model_quantize_important_datapath_lineedit.setMinimumSize(QSize(0, 30))
        self.model_quantize_important_datapath_lineedit.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_13.addWidget(self.model_quantize_important_datapath_lineedit)

        self.model_quantize_important_datapath_pushButton = QPushButton(self.model_quantize_frame2)
        self.model_quantize_important_datapath_pushButton.setObjectName(u"model_quantize_important_datapath_pushButton")
        self.model_quantize_important_datapath_pushButton.setMinimumSize(QSize(80, 30))
        self.model_quantize_important_datapath_pushButton.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_13.addWidget(self.model_quantize_important_datapath_pushButton)


        self.verticalLayout.addWidget(self.model_quantize_frame2)

        self.model_quantize_frame3 = QFrame(self.tab_4)
        self.model_quantize_frame3.setObjectName(u"model_quantize_frame3")
        self.model_quantize_frame3.setMinimumSize(QSize(0, 31))
        self.model_quantize_frame3.setMaximumSize(QSize(16777215, 31))
        self.model_quantize_frame3.setFrameShape(QFrame.StyledPanel)
        self.model_quantize_frame3.setFrameShadow(QFrame.Raised)
        self.model_quantize_frame3.setLineWidth(0)
        self.horizontalLayout_14 = QHBoxLayout(self.model_quantize_frame3)
        self.horizontalLayout_14.setSpacing(0)
        self.horizontalLayout_14.setObjectName(u"horizontalLayout_14")
        self.horizontalLayout_14.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_label_3 = QLabel(self.model_quantize_frame3)
        self.model_quantize_label_3.setObjectName(u"model_quantize_label_3")
        self.model_quantize_label_3.setMinimumSize(QSize(100, 30))
        self.model_quantize_label_3.setMaximumSize(QSize(100, 30))
        self.model_quantize_label_3.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_14.addWidget(self.model_quantize_label_3)

        self.model_quantize_output_modelpath_lineedit = QLineEdit(self.model_quantize_frame3)
        self.model_quantize_output_modelpath_lineedit.setObjectName(u"model_quantize_output_modelpath_lineedit")
        self.model_quantize_output_modelpath_lineedit.setMinimumSize(QSize(0, 30))
        self.model_quantize_output_modelpath_lineedit.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_14.addWidget(self.model_quantize_output_modelpath_lineedit)


        self.verticalLayout.addWidget(self.model_quantize_frame3)

        self.quantize_info_groupBox = QGroupBox(self.tab_4)
        self.quantize_info_groupBox.setObjectName(u"quantize_info_groupBox")
        self.horizontalLayout_15 = QHBoxLayout(self.quantize_info_groupBox)
        self.horizontalLayout_15.setSpacing(0)
        self.horizontalLayout_15.setObjectName(u"horizontalLayout_15")
        self.horizontalLayout_15.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_info = QTableWidget(self.quantize_info_groupBox)
        self.model_quantize_info.setObjectName(u"model_quantize_info")

        self.horizontalLayout_15.addWidget(self.model_quantize_info)


        self.verticalLayout.addWidget(self.quantize_info_groupBox)

        self.model_quantize_frame4 = QFrame(self.tab_4)
        self.model_quantize_frame4.setObjectName(u"model_quantize_frame4")
        self.model_quantize_frame4.setMinimumSize(QSize(0, 31))
        self.model_quantize_frame4.setMaximumSize(QSize(16777215, 31))
        self.model_quantize_frame4.setFrameShape(QFrame.StyledPanel)
        self.model_quantize_frame4.setFrameShadow(QFrame.Raised)
        self.model_quantize_frame4.setLineWidth(0)
        self.horizontalLayout_16 = QHBoxLayout(self.model_quantize_frame4)
        self.horizontalLayout_16.setSpacing(0)
        self.horizontalLayout_16.setObjectName(u"horizontalLayout_16")
        self.horizontalLayout_16.setContentsMargins(0, 0, 0, 0)
        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_16.addItem(self.horizontalSpacer)

        self.model_quantize_type_label = QLabel(self.model_quantize_frame4)
        self.model_quantize_type_label.setObjectName(u"model_quantize_type_label")
        self.model_quantize_type_label.setMinimumSize(QSize(100, 30))
        self.model_quantize_type_label.setMaximumSize(QSize(100, 30))
        self.model_quantize_type_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_16.addWidget(self.model_quantize_type_label)

        self.model_quantize_type = QComboBox(self.model_quantize_frame4)
        self.model_quantize_type.setObjectName(u"model_quantize_type")
        self.model_quantize_type.setMinimumSize(QSize(80, 30))
        self.model_quantize_type.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_16.addWidget(self.model_quantize_type)

        self.model_quantize_execute = QPushButton(self.model_quantize_frame4)
        self.model_quantize_execute.setObjectName(u"model_quantize_execute")
        self.model_quantize_execute.setMinimumSize(QSize(80, 30))
        self.model_quantize_execute.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_16.addWidget(self.model_quantize_execute)


        self.verticalLayout.addWidget(self.model_quantize_frame4)

        self.quantize_log_groupBox = QGroupBox(self.tab_4)
        self.quantize_log_groupBox.setObjectName(u"quantize_log_groupBox")
        self.quantize_log_groupBox.setMinimumSize(QSize(0, 150))
        self.quantize_log_groupBox.setMaximumSize(QSize(16777215, 150))
        self.horizontalLayout_17 = QHBoxLayout(self.quantize_log_groupBox)
        self.horizontalLayout_17.setSpacing(0)
        self.horizontalLayout_17.setObjectName(u"horizontalLayout_17")
        self.horizontalLayout_17.setContentsMargins(0, 0, 0, 0)
        self.model_quantize_log = QPlainTextEdit(self.quantize_log_groupBox)
        self.model_quantize_log.setObjectName(u"model_quantize_log")
        self.model_quantize_log.setMinimumSize(QSize(0, 130))
        self.model_quantize_log.setMaximumSize(QSize(16777215, 130))
        self.model_quantize_log.setReadOnly(True)

        self.horizontalLayout_17.addWidget(self.model_quantize_log)


        self.verticalLayout.addWidget(self.quantize_log_groupBox)

        self.tabWidget.addTab(self.tab_4, "")
        self.tab_5 = QWidget()
        self.tab_5.setObjectName(u"tab_5")
        self.verticalLayout_6 = QVBoxLayout(self.tab_5)
        self.verticalLayout_6.setSpacing(0)
        self.verticalLayout_6.setObjectName(u"verticalLayout_6")
        self.verticalLayout_6.setContentsMargins(0, 0, 0, 0)
        self.frame = QFrame(self.tab_5)
        self.frame.setObjectName(u"frame")
        self.frame.setMinimumSize(QSize(0, 31))
        self.frame.setMaximumSize(QSize(16777215, 31))
        self.frame.setFrameShape(QFrame.StyledPanel)
        self.frame.setFrameShadow(QFrame.Raised)
        self.horizontalLayout_11 = QHBoxLayout(self.frame)
        self.horizontalLayout_11.setSpacing(0)
        self.horizontalLayout_11.setObjectName(u"horizontalLayout_11")
        self.horizontalLayout_11.setContentsMargins(0, 0, 0, 0)
        self.embedding_endpoint_label = QLabel(self.frame)
        self.embedding_endpoint_label.setObjectName(u"embedding_endpoint_label")
        self.embedding_endpoint_label.setMinimumSize(QSize(80, 30))
        self.embedding_endpoint_label.setMaximumSize(QSize(80, 30))
        self.embedding_endpoint_label.setLineWidth(0)
        self.embedding_endpoint_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_11.addWidget(self.embedding_endpoint_label)

        self.embedding_txt_api_lineedit = QLineEdit(self.frame)
        self.embedding_txt_api_lineedit.setObjectName(u"embedding_txt_api_lineedit")
        self.embedding_txt_api_lineedit.setEnabled(True)
        self.embedding_txt_api_lineedit.setMinimumSize(QSize(0, 30))
        self.embedding_txt_api_lineedit.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_11.addWidget(self.embedding_txt_api_lineedit)

        self.embedding_txt_modelpath_button = QPushButton(self.frame)
        self.embedding_txt_modelpath_button.setObjectName(u"embedding_txt_modelpath_button")
        self.embedding_txt_modelpath_button.setEnabled(True)
        self.embedding_txt_modelpath_button.setMinimumSize(QSize(80, 30))
        self.embedding_txt_modelpath_button.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_11.addWidget(self.embedding_txt_modelpath_button)


        self.verticalLayout_6.addWidget(self.frame)

        self.frame_3 = QFrame(self.tab_5)
        self.frame_3.setObjectName(u"frame_3")
        self.frame_3.setMinimumSize(QSize(0, 31))
        self.frame_3.setMaximumSize(QSize(16777215, 31))
        self.frame_3.setFrameShape(QFrame.StyledPanel)
        self.frame_3.setFrameShadow(QFrame.Raised)
        self.frame_3.setLineWidth(0)
        self.horizontalLayout_5 = QHBoxLayout(self.frame_3)
        self.horizontalLayout_5.setSpacing(0)
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.horizontalLayout_5.setContentsMargins(0, 0, 0, 0)
        self.embedding_split_label = QLabel(self.frame_3)
        self.embedding_split_label.setObjectName(u"embedding_split_label")
        self.embedding_split_label.setMinimumSize(QSize(80, 0))
        self.embedding_split_label.setMaximumSize(QSize(80, 16777215))
        self.embedding_split_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_5.addWidget(self.embedding_split_label)

        self.embedding_split_spinbox = QSpinBox(self.frame_3)
        self.embedding_split_spinbox.setObjectName(u"embedding_split_spinbox")
        self.embedding_split_spinbox.setMinimumSize(QSize(80, 30))
        self.embedding_split_spinbox.setMaximumSize(QSize(80, 16777215))
        self.embedding_split_spinbox.setMinimum(64)
        self.embedding_split_spinbox.setMaximum(2048)
        self.embedding_split_spinbox.setValue(300)

        self.horizontalLayout_5.addWidget(self.embedding_split_spinbox)

        self.embedding_overlap_label = QLabel(self.frame_3)
        self.embedding_overlap_label.setObjectName(u"embedding_overlap_label")
        self.embedding_overlap_label.setMinimumSize(QSize(80, 0))
        self.embedding_overlap_label.setMaximumSize(QSize(80, 16777215))
        self.embedding_overlap_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_5.addWidget(self.embedding_overlap_label)

        self.embedding_overlap_spinbox = QSpinBox(self.frame_3)
        self.embedding_overlap_spinbox.setObjectName(u"embedding_overlap_spinbox")
        self.embedding_overlap_spinbox.setMinimumSize(QSize(80, 30))
        self.embedding_overlap_spinbox.setMaximumSize(QSize(80, 16777215))
        self.embedding_overlap_spinbox.setMaximum(1024)
        self.embedding_overlap_spinbox.setValue(50)

        self.horizontalLayout_5.addWidget(self.embedding_overlap_spinbox)

        self.embedding_source_doc_label = QLabel(self.frame_3)
        self.embedding_source_doc_label.setObjectName(u"embedding_source_doc_label")
        self.embedding_source_doc_label.setMinimumSize(QSize(80, 0))
        self.embedding_source_doc_label.setMaximumSize(QSize(80, 16777215))
        self.embedding_source_doc_label.setLineWidth(0)
        self.embedding_source_doc_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_5.addWidget(self.embedding_source_doc_label)

        self.embedding_txt_lineEdit = QLineEdit(self.frame_3)
        self.embedding_txt_lineEdit.setObjectName(u"embedding_txt_lineEdit")
        self.embedding_txt_lineEdit.setEnabled(False)
        self.embedding_txt_lineEdit.setMinimumSize(QSize(0, 30))

        self.horizontalLayout_5.addWidget(self.embedding_txt_lineEdit)

        self.embedding_txt_upload = QPushButton(self.frame_3)
        self.embedding_txt_upload.setObjectName(u"embedding_txt_upload")
        self.embedding_txt_upload.setMinimumSize(QSize(80, 30))
        self.embedding_txt_upload.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_5.addWidget(self.embedding_txt_upload)


        self.verticalLayout_6.addWidget(self.frame_3)

        self.frame_5 = QFrame(self.tab_5)
        self.frame_5.setObjectName(u"frame_5")
        self.frame_5.setMinimumSize(QSize(0, 31))
        self.frame_5.setMaximumSize(QSize(16777215, 31))
        self.frame_5.setFrameShape(QFrame.StyledPanel)
        self.frame_5.setFrameShadow(QFrame.Raised)
        self.frame_5.setLineWidth(0)
        self.horizontalLayout_10 = QHBoxLayout(self.frame_5)
        self.horizontalLayout_10.setSpacing(0)
        self.horizontalLayout_10.setObjectName(u"horizontalLayout_10")
        self.horizontalLayout_10.setContentsMargins(0, 0, 0, 0)
        self.embedding_describe_label = QLabel(self.frame_5)
        self.embedding_describe_label.setObjectName(u"embedding_describe_label")
        self.embedding_describe_label.setMinimumSize(QSize(80, 0))
        self.embedding_describe_label.setMaximumSize(QSize(80, 16777215))
        self.embedding_describe_label.setLineWidth(0)
        self.embedding_describe_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_10.addWidget(self.embedding_describe_label)

        self.embedding_txt_describe_lineEdit = QLineEdit(self.frame_5)
        self.embedding_txt_describe_lineEdit.setObjectName(u"embedding_txt_describe_lineEdit")
        self.embedding_txt_describe_lineEdit.setEnabled(True)
        self.embedding_txt_describe_lineEdit.setMinimumSize(QSize(0, 30))

        self.horizontalLayout_10.addWidget(self.embedding_txt_describe_lineEdit)

        self.embedding_txt_embedding = QPushButton(self.frame_5)
        self.embedding_txt_embedding.setObjectName(u"embedding_txt_embedding")
        self.embedding_txt_embedding.setEnabled(True)
        self.embedding_txt_embedding.setMinimumSize(QSize(80, 30))
        self.embedding_txt_embedding.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_10.addWidget(self.embedding_txt_embedding)


        self.verticalLayout_6.addWidget(self.frame_5)

        self.frame_2 = QFrame(self.tab_5)
        self.frame_2.setObjectName(u"frame_2")
        self.frame_2.setMinimumSize(QSize(0, 370))
        self.frame_2.setFrameShape(QFrame.StyledPanel)
        self.frame_2.setFrameShadow(QFrame.Raised)
        self.frame_2.setLineWidth(0)
        self.horizontalLayout_4 = QHBoxLayout(self.frame_2)
        self.horizontalLayout_4.setSpacing(0)
        self.horizontalLayout_4.setObjectName(u"horizontalLayout_4")
        self.horizontalLayout_4.setContentsMargins(0, 0, 0, 0)
        self.embedding_txt_wait = QTableWidget(self.frame_2)
        self.embedding_txt_wait.setObjectName(u"embedding_txt_wait")
        self.embedding_txt_wait.setLineWidth(0)

        self.horizontalLayout_4.addWidget(self.embedding_txt_wait)

        self.embedding_txt_over = QTableWidget(self.frame_2)
        self.embedding_txt_over.setObjectName(u"embedding_txt_over")
        self.embedding_txt_over.setEnabled(True)
        self.embedding_txt_over.setLineWidth(0)

        self.horizontalLayout_4.addWidget(self.embedding_txt_over)


        self.verticalLayout_6.addWidget(self.frame_2)

        self.frame_4 = QFrame(self.tab_5)
        self.frame_4.setObjectName(u"frame_4")
        self.frame_4.setMinimumSize(QSize(0, 140))
        self.frame_4.setMaximumSize(QSize(16777215, 140))
        self.frame_4.setFrameShape(QFrame.StyledPanel)
        self.frame_4.setFrameShadow(QFrame.Raised)
        self.frame_4.setLineWidth(0)
        self.horizontalLayout_9 = QHBoxLayout(self.frame_4)
        self.horizontalLayout_9.setSpacing(0)
        self.horizontalLayout_9.setObjectName(u"horizontalLayout_9")
        self.horizontalLayout_9.setContentsMargins(0, 0, 0, 0)
        self.embedding_test_groupBox = QGroupBox(self.frame_4)
        self.embedding_test_groupBox.setObjectName(u"embedding_test_groupBox")
        self.embedding_test_groupBox.setMinimumSize(QSize(0, 125))
        self.embedding_test_groupBox.setMaximumSize(QSize(1000, 125))
        self.horizontalLayout_7 = QHBoxLayout(self.embedding_test_groupBox)
        self.horizontalLayout_7.setSpacing(0)
        self.horizontalLayout_7.setObjectName(u"horizontalLayout_7")
        self.horizontalLayout_7.setContentsMargins(0, 0, 0, 0)
        self.embedding_test_textEdit = QTextEdit(self.embedding_test_groupBox)
        self.embedding_test_textEdit.setObjectName(u"embedding_test_textEdit")
        self.embedding_test_textEdit.setMinimumSize(QSize(0, 100))
        self.embedding_test_textEdit.setMaximumSize(QSize(16777215, 100))

        self.horizontalLayout_7.addWidget(self.embedding_test_textEdit)

        self.embedding_test_pushButton = QPushButton(self.embedding_test_groupBox)
        self.embedding_test_pushButton.setObjectName(u"embedding_test_pushButton")
        self.embedding_test_pushButton.setMinimumSize(QSize(50, 100))
        self.embedding_test_pushButton.setMaximumSize(QSize(50, 100))

        self.horizontalLayout_7.addWidget(self.embedding_test_pushButton)


        self.horizontalLayout_9.addWidget(self.embedding_test_groupBox)

        self.embedding_result_groupBox = QGroupBox(self.frame_4)
        self.embedding_result_groupBox.setObjectName(u"embedding_result_groupBox")
        self.embedding_result_groupBox.setMinimumSize(QSize(250, 125))
        self.embedding_result_groupBox.setMaximumSize(QSize(250, 125))
        self.horizontalLayout_8 = QHBoxLayout(self.embedding_result_groupBox)
        self.horizontalLayout_8.setSpacing(0)
        self.horizontalLayout_8.setObjectName(u"horizontalLayout_8")
        self.horizontalLayout_8.setContentsMargins(0, 0, 0, 0)
        self.embedding_test_result = QPlainTextEdit(self.embedding_result_groupBox)
        self.embedding_test_result.setObjectName(u"embedding_test_result")
        self.embedding_test_result.setMinimumSize(QSize(0, 100))
        self.embedding_test_result.setMaximumSize(QSize(16777215, 100))
        self.embedding_test_result.setReadOnly(True)

        self.horizontalLayout_8.addWidget(self.embedding_test_result)


        self.horizontalLayout_9.addWidget(self.embedding_result_groupBox)

        self.embedding_log_groupBox = QGroupBox(self.frame_4)
        self.embedding_log_groupBox.setObjectName(u"embedding_log_groupBox")
        self.embedding_log_groupBox.setMinimumSize(QSize(250, 125))
        self.embedding_log_groupBox.setMaximumSize(QSize(250, 125))
        self.horizontalLayout_6 = QHBoxLayout(self.embedding_log_groupBox)
        self.horizontalLayout_6.setSpacing(0)
        self.horizontalLayout_6.setObjectName(u"horizontalLayout_6")
        self.horizontalLayout_6.setContentsMargins(0, 0, 0, 0)
        self.embedding_test_log = QPlainTextEdit(self.embedding_log_groupBox)
        self.embedding_test_log.setObjectName(u"embedding_test_log")
        self.embedding_test_log.setMinimumSize(QSize(0, 100))
        self.embedding_test_log.setMaximumSize(QSize(16777215, 100))
        self.embedding_test_log.setReadOnly(True)

        self.horizontalLayout_6.addWidget(self.embedding_test_log)


        self.horizontalLayout_9.addWidget(self.embedding_log_groupBox)


        self.verticalLayout_6.addWidget(self.frame_4)

        self.tabWidget.addTab(self.tab_5, "")
        self.tab_6 = QWidget()
        self.tab_6.setObjectName(u"tab_6")
        self.horizontalLayout_28 = QHBoxLayout(self.tab_6)
        self.horizontalLayout_28.setObjectName(u"horizontalLayout_28")
        self.sd_set_groupBox = QGroupBox(self.tab_6)
        self.sd_set_groupBox.setObjectName(u"sd_set_groupBox")
        self.sd_set_groupBox.setMaximumSize(QSize(350, 16777215))
        self.verticalLayout_9 = QVBoxLayout(self.sd_set_groupBox)
        self.verticalLayout_9.setSpacing(3)
        self.verticalLayout_9.setObjectName(u"verticalLayout_9")
        self.verticalLayout_9.setContentsMargins(0, 0, 0, 0)
        self.frame_6 = QFrame(self.sd_set_groupBox)
        self.frame_6.setObjectName(u"frame_6")
        self.frame_6.setMinimumSize(QSize(0, 26))
        self.frame_6.setMaximumSize(QSize(16777215, 26))
        self.frame_6.setFrameShape(QFrame.StyledPanel)
        self.frame_6.setFrameShadow(QFrame.Raised)
        self.frame_6.setLineWidth(0)
        self.horizontalLayout_3 = QHBoxLayout(self.frame_6)
        self.horizontalLayout_3.setSpacing(0)
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.horizontalLayout_3.setContentsMargins(0, 0, 0, 0)
        self.sd_modelpath_label = QLabel(self.frame_6)
        self.sd_modelpath_label.setObjectName(u"sd_modelpath_label")
        self.sd_modelpath_label.setMinimumSize(QSize(90, 25))
        self.sd_modelpath_label.setMaximumSize(QSize(90, 25))
        self.sd_modelpath_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_3.addWidget(self.sd_modelpath_label)

        self.sd_modelpath_lineEdit = QLineEdit(self.frame_6)
        self.sd_modelpath_lineEdit.setObjectName(u"sd_modelpath_lineEdit")
        self.sd_modelpath_lineEdit.setMinimumSize(QSize(0, 25))
        self.sd_modelpath_lineEdit.setMaximumSize(QSize(16777215, 25))

        self.horizontalLayout_3.addWidget(self.sd_modelpath_lineEdit)

        self.sd_modelpath_pushButton = QPushButton(self.frame_6)
        self.sd_modelpath_pushButton.setObjectName(u"sd_modelpath_pushButton")
        self.sd_modelpath_pushButton.setMinimumSize(QSize(30, 25))
        self.sd_modelpath_pushButton.setMaximumSize(QSize(30, 25))

        self.horizontalLayout_3.addWidget(self.sd_modelpath_pushButton)


        self.verticalLayout_9.addWidget(self.frame_6)

        self.frame_23 = QFrame(self.sd_set_groupBox)
        self.frame_23.setObjectName(u"frame_23")
        self.frame_23.setMinimumSize(QSize(0, 26))
        self.frame_23.setMaximumSize(QSize(16777215, 26))
        self.frame_23.setFrameShape(QFrame.StyledPanel)
        self.frame_23.setFrameShadow(QFrame.Raised)
        self.frame_23.setLineWidth(0)
        self.horizontalLayout_22 = QHBoxLayout(self.frame_23)
        self.horizontalLayout_22.setSpacing(0)
        self.horizontalLayout_22.setObjectName(u"horizontalLayout_22")
        self.horizontalLayout_22.setContentsMargins(0, 0, 0, 0)
        self.sd_vaepath_label = QLabel(self.frame_23)
        self.sd_vaepath_label.setObjectName(u"sd_vaepath_label")
        self.sd_vaepath_label.setMinimumSize(QSize(90, 25))
        self.sd_vaepath_label.setMaximumSize(QSize(90, 25))
        self.sd_vaepath_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_22.addWidget(self.sd_vaepath_label)

        self.sd_vaepath_lineEdit = QLineEdit(self.frame_23)
        self.sd_vaepath_lineEdit.setObjectName(u"sd_vaepath_lineEdit")
        self.sd_vaepath_lineEdit.setMinimumSize(QSize(0, 25))
        self.sd_vaepath_lineEdit.setMaximumSize(QSize(16777215, 25))

        self.horizontalLayout_22.addWidget(self.sd_vaepath_lineEdit)

        self.sd_vaepath_pushButton = QPushButton(self.frame_23)
        self.sd_vaepath_pushButton.setObjectName(u"sd_vaepath_pushButton")
        self.sd_vaepath_pushButton.setMinimumSize(QSize(30, 25))
        self.sd_vaepath_pushButton.setMaximumSize(QSize(30, 25))

        self.horizontalLayout_22.addWidget(self.sd_vaepath_pushButton)


        self.verticalLayout_9.addWidget(self.frame_23)

        self.frame_24 = QFrame(self.sd_set_groupBox)
        self.frame_24.setObjectName(u"frame_24")
        self.frame_24.setMinimumSize(QSize(0, 26))
        self.frame_24.setMaximumSize(QSize(16777215, 26))
        self.frame_24.setFrameShape(QFrame.StyledPanel)
        self.frame_24.setFrameShadow(QFrame.Raised)
        self.frame_24.setLineWidth(0)
        self.horizontalLayout_24 = QHBoxLayout(self.frame_24)
        self.horizontalLayout_24.setSpacing(0)
        self.horizontalLayout_24.setObjectName(u"horizontalLayout_24")
        self.horizontalLayout_24.setContentsMargins(0, 0, 0, 0)
        self.sd_antiprompt_label = QLabel(self.frame_24)
        self.sd_antiprompt_label.setObjectName(u"sd_antiprompt_label")
        self.sd_antiprompt_label.setMinimumSize(QSize(90, 25))
        self.sd_antiprompt_label.setMaximumSize(QSize(90, 25))
        self.sd_antiprompt_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_24.addWidget(self.sd_antiprompt_label)

        self.sd_antiprompt_lineEdit = QLineEdit(self.frame_24)
        self.sd_antiprompt_lineEdit.setObjectName(u"sd_antiprompt_lineEdit")
        self.sd_antiprompt_lineEdit.setMinimumSize(QSize(0, 25))
        self.sd_antiprompt_lineEdit.setMaximumSize(QSize(16777215, 25))

        self.horizontalLayout_24.addWidget(self.sd_antiprompt_lineEdit)


        self.verticalLayout_9.addWidget(self.frame_24)

        self.frame_14 = QFrame(self.sd_set_groupBox)
        self.frame_14.setObjectName(u"frame_14")
        self.frame_14.setFrameShape(QFrame.StyledPanel)
        self.frame_14.setFrameShadow(QFrame.Raised)
        self.frame_14.setLineWidth(0)
        self.gridLayout = QGridLayout(self.frame_14)
        self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout.setHorizontalSpacing(0)
        self.gridLayout.setVerticalSpacing(3)
        self.gridLayout.setContentsMargins(0, 0, 0, 0)
        self.sd_imagewidth_label = QLabel(self.frame_14)
        self.sd_imagewidth_label.setObjectName(u"sd_imagewidth_label")
        self.sd_imagewidth_label.setMinimumSize(QSize(50, 25))
        self.sd_imagewidth_label.setMaximumSize(QSize(50, 25))
        self.sd_imagewidth_label.setLineWidth(0)
        self.sd_imagewidth_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_imagewidth_label, 0, 0, 1, 1)

        self.sd_imagewidth = QSpinBox(self.frame_14)
        self.sd_imagewidth.setObjectName(u"sd_imagewidth")
        self.sd_imagewidth.setMinimumSize(QSize(80, 25))
        self.sd_imagewidth.setMaximumSize(QSize(80, 25))
        self.sd_imagewidth.setMinimum(64)
        self.sd_imagewidth.setMaximum(4096)
        self.sd_imagewidth.setSingleStep(64)
        self.sd_imagewidth.setValue(512)

        self.gridLayout.addWidget(self.sd_imagewidth, 0, 1, 2, 1)

        self.sd_sampletype_label = QLabel(self.frame_14)
        self.sd_sampletype_label.setObjectName(u"sd_sampletype_label")
        self.sd_sampletype_label.setMinimumSize(QSize(50, 25))
        self.sd_sampletype_label.setMaximumSize(QSize(50, 25))
        self.sd_sampletype_label.setLineWidth(0)
        self.sd_sampletype_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_sampletype_label, 0, 2, 1, 1)

        self.sd_sampletype = QComboBox(self.frame_14)
        self.sd_sampletype.setObjectName(u"sd_sampletype")
        self.sd_sampletype.setMinimumSize(QSize(80, 25))
        self.sd_sampletype.setMaximumSize(QSize(80, 25))

        self.gridLayout.addWidget(self.sd_sampletype, 0, 3, 1, 1)

        self.sd_samplesteps_label = QLabel(self.frame_14)
        self.sd_samplesteps_label.setObjectName(u"sd_samplesteps_label")
        self.sd_samplesteps_label.setMinimumSize(QSize(50, 25))
        self.sd_samplesteps_label.setMaximumSize(QSize(50, 25))
        self.sd_samplesteps_label.setLineWidth(0)
        self.sd_samplesteps_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_samplesteps_label, 1, 2, 2, 1)

        self.sd_imageheight_label = QLabel(self.frame_14)
        self.sd_imageheight_label.setObjectName(u"sd_imageheight_label")
        self.sd_imageheight_label.setMinimumSize(QSize(50, 25))
        self.sd_imageheight_label.setMaximumSize(QSize(50, 25))
        self.sd_imageheight_label.setLineWidth(0)
        self.sd_imageheight_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_imageheight_label, 2, 0, 1, 1)

        self.sd_imageheight = QSpinBox(self.frame_14)
        self.sd_imageheight.setObjectName(u"sd_imageheight")
        self.sd_imageheight.setMinimumSize(QSize(80, 25))
        self.sd_imageheight.setMaximumSize(QSize(80, 25))
        self.sd_imageheight.setMinimum(64)
        self.sd_imageheight.setMaximum(4096)
        self.sd_imageheight.setSingleStep(64)
        self.sd_imageheight.setValue(512)

        self.gridLayout.addWidget(self.sd_imageheight, 2, 1, 1, 1)

        self.sd_samplesteps = QSpinBox(self.frame_14)
        self.sd_samplesteps.setObjectName(u"sd_samplesteps")
        self.sd_samplesteps.setMinimumSize(QSize(80, 25))
        self.sd_samplesteps.setMaximumSize(QSize(80, 25))
        self.sd_samplesteps.setMinimum(1)
        self.sd_samplesteps.setMaximum(100)
        self.sd_samplesteps.setValue(20)

        self.gridLayout.addWidget(self.sd_samplesteps, 2, 3, 1, 1)

        self.sd_cfg_label = QLabel(self.frame_14)
        self.sd_cfg_label.setObjectName(u"sd_cfg_label")
        self.sd_cfg_label.setMinimumSize(QSize(50, 25))
        self.sd_cfg_label.setMaximumSize(QSize(50, 25))
        self.sd_cfg_label.setLineWidth(0)
        self.sd_cfg_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_cfg_label, 3, 0, 1, 1)

        self.sd_cfgscale = QDoubleSpinBox(self.frame_14)
        self.sd_cfgscale.setObjectName(u"sd_cfgscale")
        self.sd_cfgscale.setMinimumSize(QSize(80, 25))
        self.sd_cfgscale.setMaximumSize(QSize(80, 25))
        self.sd_cfgscale.setDecimals(1)
        self.sd_cfgscale.setSingleStep(0.500000000000000)
        self.sd_cfgscale.setValue(7.500000000000000)

        self.gridLayout.addWidget(self.sd_cfgscale, 3, 1, 1, 1)

        self.sd_imagenums_label = QLabel(self.frame_14)
        self.sd_imagenums_label.setObjectName(u"sd_imagenums_label")
        self.sd_imagenums_label.setMinimumSize(QSize(50, 25))
        self.sd_imagenums_label.setMaximumSize(QSize(50, 25))
        self.sd_imagenums_label.setLineWidth(0)
        self.sd_imagenums_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_imagenums_label, 3, 2, 1, 1)

        self.sd_batch_count = QSpinBox(self.frame_14)
        self.sd_batch_count.setObjectName(u"sd_batch_count")
        self.sd_batch_count.setMinimumSize(QSize(80, 25))
        self.sd_batch_count.setMaximumSize(QSize(80, 25))
        self.sd_batch_count.setMinimum(1)
        self.sd_batch_count.setMaximum(4096)
        self.sd_batch_count.setSingleStep(1)
        self.sd_batch_count.setValue(1)

        self.gridLayout.addWidget(self.sd_batch_count, 3, 3, 1, 1)

        self.sd_seed_label = QLabel(self.frame_14)
        self.sd_seed_label.setObjectName(u"sd_seed_label")
        self.sd_seed_label.setMinimumSize(QSize(50, 25))
        self.sd_seed_label.setMaximumSize(QSize(50, 25))
        self.sd_seed_label.setLineWidth(0)
        self.sd_seed_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_seed_label, 4, 0, 1, 1)

        self.sd_seed = QSpinBox(self.frame_14)
        self.sd_seed.setObjectName(u"sd_seed")
        self.sd_seed.setMinimumSize(QSize(80, 25))
        self.sd_seed.setMaximumSize(QSize(80, 25))
        self.sd_seed.setMinimum(-1)
        self.sd_seed.setMaximum(7758258)
        self.sd_seed.setValue(-1)

        self.gridLayout.addWidget(self.sd_seed, 4, 1, 1, 1)

        self.sd_clip_label = QLabel(self.frame_14)
        self.sd_clip_label.setObjectName(u"sd_clip_label")
        self.sd_clip_label.setMinimumSize(QSize(50, 25))
        self.sd_clip_label.setMaximumSize(QSize(50, 25))
        self.sd_clip_label.setLineWidth(0)
        self.sd_clip_label.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.sd_clip_label, 4, 2, 1, 1)

        self.sd_skipclip = QSpinBox(self.frame_14)
        self.sd_skipclip.setObjectName(u"sd_skipclip")
        self.sd_skipclip.setMinimumSize(QSize(80, 25))
        self.sd_skipclip.setMaximumSize(QSize(80, 25))
        self.sd_skipclip.setMinimum(1)
        self.sd_skipclip.setMaximum(5)
        self.sd_skipclip.setValue(2)

        self.gridLayout.addWidget(self.sd_skipclip, 4, 3, 1, 1)


        self.verticalLayout_9.addWidget(self.frame_14)

        self.sd_prompt_groupBox = QGroupBox(self.sd_set_groupBox)
        self.sd_prompt_groupBox.setObjectName(u"sd_prompt_groupBox")
        self.horizontalLayout_23 = QHBoxLayout(self.sd_prompt_groupBox)
        self.horizontalLayout_23.setSpacing(3)
        self.horizontalLayout_23.setObjectName(u"horizontalLayout_23")
        self.horizontalLayout_23.setContentsMargins(0, 0, 0, 0)
        self.sd_prompt_textEdit = QTextEdit(self.sd_prompt_groupBox)
        self.sd_prompt_textEdit.setObjectName(u"sd_prompt_textEdit")

        self.horizontalLayout_23.addWidget(self.sd_prompt_textEdit)

        self.sd_draw_pushButton = QPushButton(self.sd_prompt_groupBox)
        self.sd_draw_pushButton.setObjectName(u"sd_draw_pushButton")
        self.sd_draw_pushButton.setMinimumSize(QSize(60, 30))
        self.sd_draw_pushButton.setMaximumSize(QSize(60, 30))

        self.horizontalLayout_23.addWidget(self.sd_draw_pushButton)


        self.verticalLayout_9.addWidget(self.sd_prompt_groupBox)

        self.sd_upload_groupBox = QGroupBox(self.sd_set_groupBox)
        self.sd_upload_groupBox.setObjectName(u"sd_upload_groupBox")
        self.horizontalLayout_29 = QHBoxLayout(self.sd_upload_groupBox)
        self.horizontalLayout_29.setSpacing(3)
        self.horizontalLayout_29.setObjectName(u"horizontalLayout_29")
        self.horizontalLayout_29.setContentsMargins(0, 0, 0, 0)
        self.sd_uploadimage_textEdit = QTextEdit(self.sd_upload_groupBox)
        self.sd_uploadimage_textEdit.setObjectName(u"sd_uploadimage_textEdit")

        self.horizontalLayout_29.addWidget(self.sd_uploadimage_textEdit)

        self.sd_draw_pushButton_2 = QPushButton(self.sd_upload_groupBox)
        self.sd_draw_pushButton_2.setObjectName(u"sd_draw_pushButton_2")
        self.sd_draw_pushButton_2.setEnabled(False)
        self.sd_draw_pushButton_2.setMinimumSize(QSize(60, 30))
        self.sd_draw_pushButton_2.setMaximumSize(QSize(60, 30))

        self.horizontalLayout_29.addWidget(self.sd_draw_pushButton_2)


        self.verticalLayout_9.addWidget(self.sd_upload_groupBox)


        self.horizontalLayout_28.addWidget(self.sd_set_groupBox)

        self.sd_result_groupBox = QGroupBox(self.tab_6)
        self.sd_result_groupBox.setObjectName(u"sd_result_groupBox")
        self.sd_result_groupBox.setMinimumSize(QSize(512, 0))
        self.verticalLayout_11 = QVBoxLayout(self.sd_result_groupBox)
        self.verticalLayout_11.setSpacing(3)
        self.verticalLayout_11.setObjectName(u"verticalLayout_11")
        self.verticalLayout_11.setContentsMargins(0, 0, 0, 0)
        self.sd_result = QTextEdit(self.sd_result_groupBox)
        self.sd_result.setObjectName(u"sd_result")
        self.sd_result.setReadOnly(False)

        self.verticalLayout_11.addWidget(self.sd_result)

        self.sd_log_groupBox = QGroupBox(self.sd_result_groupBox)
        self.sd_log_groupBox.setObjectName(u"sd_log_groupBox")
        self.sd_log_groupBox.setMinimumSize(QSize(0, 160))
        self.sd_log_groupBox.setMaximumSize(QSize(16777215, 160))
        self.horizontalLayout_25 = QHBoxLayout(self.sd_log_groupBox)
        self.horizontalLayout_25.setSpacing(0)
        self.horizontalLayout_25.setObjectName(u"horizontalLayout_25")
        self.horizontalLayout_25.setContentsMargins(0, 0, 0, 0)
        self.sd_log = QPlainTextEdit(self.sd_log_groupBox)
        self.sd_log.setObjectName(u"sd_log")
        self.sd_log.setMinimumSize(QSize(0, 150))
        self.sd_log.setMaximumSize(QSize(16777215, 150))
        self.sd_log.setReadOnly(True)

        self.horizontalLayout_25.addWidget(self.sd_log)


        self.verticalLayout_11.addWidget(self.sd_log_groupBox)


        self.horizontalLayout_28.addWidget(self.sd_result_groupBox)

        self.tabWidget.addTab(self.tab_6, "")
        self.tab_7 = QWidget()
        self.tab_7.setObjectName(u"tab_7")
        self.verticalLayout_5 = QVBoxLayout(self.tab_7)
        self.verticalLayout_5.setSpacing(0)
        self.verticalLayout_5.setObjectName(u"verticalLayout_5")
        self.verticalLayout_5.setContentsMargins(-1, 0, 0, 0)
        self.frame_15 = QFrame(self.tab_7)
        self.frame_15.setObjectName(u"frame_15")
        self.frame_15.setMinimumSize(QSize(0, 31))
        self.frame_15.setMaximumSize(QSize(16777215, 31))
        self.frame_15.setFrameShape(QFrame.StyledPanel)
        self.frame_15.setFrameShadow(QFrame.Raised)
        self.frame_15.setLineWidth(0)
        self.horizontalLayout_2 = QHBoxLayout(self.frame_15)
        self.horizontalLayout_2.setSpacing(0)
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.horizontalLayout_2.setContentsMargins(0, 0, 0, 0)
        self.whisper_modelpath_label = QLabel(self.frame_15)
        self.whisper_modelpath_label.setObjectName(u"whisper_modelpath_label")
        self.whisper_modelpath_label.setMinimumSize(QSize(0, 30))
        self.whisper_modelpath_label.setMaximumSize(QSize(16777215, 30))
        self.whisper_modelpath_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_2.addWidget(self.whisper_modelpath_label)

        self.whisper_load_modelpath_linedit = QLineEdit(self.frame_15)
        self.whisper_load_modelpath_linedit.setObjectName(u"whisper_load_modelpath_linedit")
        self.whisper_load_modelpath_linedit.setEnabled(False)
        self.whisper_load_modelpath_linedit.setMinimumSize(QSize(0, 30))
        self.whisper_load_modelpath_linedit.setReadOnly(False)

        self.horizontalLayout_2.addWidget(self.whisper_load_modelpath_linedit)

        self.whisper_load_modelpath_button = QPushButton(self.frame_15)
        self.whisper_load_modelpath_button.setObjectName(u"whisper_load_modelpath_button")
        self.whisper_load_modelpath_button.setMinimumSize(QSize(80, 0))
        self.whisper_load_modelpath_button.setMaximumSize(QSize(80, 30))
        self.whisper_load_modelpath_button.setFont(font)

        self.horizontalLayout_2.addWidget(self.whisper_load_modelpath_button)


        self.verticalLayout_5.addWidget(self.frame_15)

        self.voice_load_groupBox_4 = QGroupBox(self.tab_7)
        self.voice_load_groupBox_4.setObjectName(u"voice_load_groupBox_4")
        self.voice_load_groupBox_4.setFont(font)
        self.verticalLayout_10 = QVBoxLayout(self.voice_load_groupBox_4)
        self.verticalLayout_10.setSpacing(0)
        self.verticalLayout_10.setObjectName(u"verticalLayout_10")
        self.verticalLayout_10.setContentsMargins(0, 0, 0, 0)
        self.whisper_log = QPlainTextEdit(self.voice_load_groupBox_4)
        self.whisper_log.setObjectName(u"whisper_log")
        self.whisper_log.setFont(font)
        self.whisper_log.setReadOnly(True)

        self.verticalLayout_10.addWidget(self.whisper_log)

        self.frame_19 = QFrame(self.voice_load_groupBox_4)
        self.frame_19.setObjectName(u"frame_19")
        self.frame_19.setMinimumSize(QSize(0, 31))
        self.frame_19.setMaximumSize(QSize(16777215, 31))
        self.frame_19.setFrameShape(QFrame.StyledPanel)
        self.frame_19.setFrameShadow(QFrame.Raised)
        self.frame_19.setLineWidth(0)
        self.horizontalLayout_26 = QHBoxLayout(self.frame_19)
        self.horizontalLayout_26.setSpacing(0)
        self.horizontalLayout_26.setObjectName(u"horizontalLayout_26")
        self.horizontalLayout_26.setContentsMargins(0, 0, 0, 0)
        self.whisper_wav2text_label = QLabel(self.frame_19)
        self.whisper_wav2text_label.setObjectName(u"whisper_wav2text_label")
        self.whisper_wav2text_label.setMinimumSize(QSize(0, 30))
        self.whisper_wav2text_label.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_26.addWidget(self.whisper_wav2text_label)

        self.whisper_wavpath_lineedit = QLineEdit(self.frame_19)
        self.whisper_wavpath_lineedit.setObjectName(u"whisper_wavpath_lineedit")
        self.whisper_wavpath_lineedit.setMinimumSize(QSize(0, 30))
        self.whisper_wavpath_lineedit.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_26.addWidget(self.whisper_wavpath_lineedit)

        self.whisper_wavpath_pushButton = QPushButton(self.frame_19)
        self.whisper_wavpath_pushButton.setObjectName(u"whisper_wavpath_pushButton")
        self.whisper_wavpath_pushButton.setMinimumSize(QSize(80, 30))
        self.whisper_wavpath_pushButton.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_26.addWidget(self.whisper_wavpath_pushButton)

        self.whisper_format_label = QLabel(self.frame_19)
        self.whisper_format_label.setObjectName(u"whisper_format_label")
        self.whisper_format_label.setMinimumSize(QSize(80, 30))
        self.whisper_format_label.setMaximumSize(QSize(80, 30))
        self.whisper_format_label.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_26.addWidget(self.whisper_format_label)

        self.whisper_output_format = QComboBox(self.frame_19)
        self.whisper_output_format.setObjectName(u"whisper_output_format")
        self.whisper_output_format.setMinimumSize(QSize(110, 30))
        self.whisper_output_format.setMaximumSize(QSize(110, 30))

        self.horizontalLayout_26.addWidget(self.whisper_output_format)

        self.whisper_execute_pushbutton = QPushButton(self.frame_19)
        self.whisper_execute_pushbutton.setObjectName(u"whisper_execute_pushbutton")
        self.whisper_execute_pushbutton.setMinimumSize(QSize(80, 30))
        self.whisper_execute_pushbutton.setMaximumSize(QSize(80, 30))

        self.horizontalLayout_26.addWidget(self.whisper_execute_pushbutton)


        self.verticalLayout_10.addWidget(self.frame_19)


        self.verticalLayout_5.addWidget(self.voice_load_groupBox_4)

        self.tabWidget.addTab(self.tab_7, "")
        self.tab_8 = QWidget()
        self.tab_8.setObjectName(u"tab_8")
        self.verticalLayout_14 = QVBoxLayout(self.tab_8)
        self.verticalLayout_14.setObjectName(u"verticalLayout_14")
        self.horizontalLayout_27 = QHBoxLayout()
        self.horizontalLayout_27.setObjectName(u"horizontalLayout_27")
        self.label_14 = QLabel(self.tab_8)
        self.label_14.setObjectName(u"label_14")
        self.label_14.setMinimumSize(QSize(0, 30))
        self.label_14.setMaximumSize(QSize(16777215, 30))
        self.label_14.setAlignment(Qt.AlignCenter)

        self.horizontalLayout_27.addWidget(self.label_14)

        self.voice_source_comboBox = QComboBox(self.tab_8)
        self.voice_source_comboBox.setObjectName(u"voice_source_comboBox")
        self.voice_source_comboBox.setEnabled(False)
        self.voice_source_comboBox.setMinimumSize(QSize(0, 30))
        self.voice_source_comboBox.setMaximumSize(QSize(16777215, 30))

        self.horizontalLayout_27.addWidget(self.voice_source_comboBox)

        self.voice_enable_radioButton = QRadioButton(self.tab_8)
        self.voice_enable_radioButton.setObjectName(u"voice_enable_radioButton")
        self.voice_enable_radioButton.setMinimumSize(QSize(0, 30))
        self.voice_enable_radioButton.setMaximumSize(QSize(16777215, 30))
        self.voice_enable_radioButton.setChecked(False)

        self.horizontalLayout_27.addWidget(self.voice_enable_radioButton)


        self.verticalLayout_14.addLayout(self.horizontalLayout_27)

        self.voice_log = QTextEdit(self.tab_8)
        self.voice_log.setObjectName(u"voice_log")

        self.verticalLayout_14.addWidget(self.voice_log)

        self.tabWidget.addTab(self.tab_8, "")

        self.horizontalLayout_46.addWidget(self.tabWidget)


        self.retranslateUi(Expend)

        self.tabWidget.setCurrentIndex(1)


        QMetaObject.connectSlotsByName(Expend)
    # setupUi

    def retranslateUi(self, Expend):
        Expend.setWindowTitle(QCoreApplication.translate("Expend", u"\u589e\u6b96\u7a97\u53e3", None))
        self.info_card.setHtml(QCoreApplication.translate("Expend", u"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'Arial'; font-size:10pt; font-weight:400; font-style:normal;\">\n"
"<p align=\"justify\" style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-family:'SimSun'; font-size:9pt;\"><br /></p></body></html>", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_1), QCoreApplication.translate("Expend", u"\u8f6f\u4ef6\u4ecb\u7ecd", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_2), QCoreApplication.translate("Expend", u"\u6a21\u578b\u8bcd\u8868", None))
        self.modellog_card.setPlainText("")
        self.modellog_card.setPlaceholderText(QCoreApplication.translate("Expend", u"\u88c5\u8f7d\u6a21\u578b\u540e\u67e5\u770b", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_3), QCoreApplication.translate("Expend", u"\u6a21\u578b\u65e5\u5fd7", None))
        self.model_quantize_label.setText(QCoreApplication.translate("Expend", u"\u5f85\u91cf\u5316\u6a21\u578b\u8def\u5f84", None))
        self.model_quantize_row_modelpath_lineedit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u5c3d\u91cf\u9009\u62e9f32\u6216f16\u7684gguf\u6a21\u578b", None))
        self.model_quantize_row_modelpath_pushButton.setText(QCoreApplication.translate("Expend", u"...", None))
        self.model_quantize_label_2.setText(QCoreApplication.translate("Expend", u"\u91cd\u8981\u6027\u77e9\u9635\u8def\u5f84", None))
        self.model_quantize_important_datapath_lineedit.setPlaceholderText(QCoreApplication.translate("Expend", u"iq\u91cf\u5316\u65b9\u6cd5\u5fc5\u987b\uff0c\u5176\u5b83\u91cf\u5316\u65b9\u6cd5\u4e0d\u8981\u586b\u5199\u672c\u9879\uff0c\u91cd\u8981\u6027\u77e9\u9635\u9700\u8981\u7528\u539f\u9879\u76eeimatrix\u5de5\u5177\u548c\u6587\u672c\u6570\u636e\u751f\u6210", None))
        self.model_quantize_important_datapath_pushButton.setText(QCoreApplication.translate("Expend", u"...", None))
        self.model_quantize_label_3.setText(QCoreApplication.translate("Expend", u"\u91cf\u5316\u540e\u6a21\u578b\u8def\u5f84", None))
        self.model_quantize_output_modelpath_lineedit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u9009\u62e9\u597d\u5f85\u91cf\u5316\u6a21\u578b\u8def\u5f84\u548c\u91cf\u5316\u65b9\u6cd5\u540e\u4f1a\u81ea\u52a8\u586b\u5145", None))
        self.quantize_info_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u91cf\u5316\u65b9\u6cd5\u8bf4\u660e", None))
        self.model_quantize_type_label.setText(QCoreApplication.translate("Expend", u"\u9009\u62e9\u91cf\u5316\u65b9\u6cd5", None))
        self.model_quantize_execute.setText(QCoreApplication.translate("Expend", u"\u6267\u884c\u91cf\u5316", None))
        self.quantize_log_groupBox.setTitle(QCoreApplication.translate("Expend", u"quantize.exe \u6267\u884c\u65e5\u5fd7", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_4), QCoreApplication.translate("Expend", u"\u6a21\u578b\u91cf\u5316", None))
        self.embedding_endpoint_label.setText(QCoreApplication.translate("Expend", u"\u5d4c\u5165\u7aef\u70b9", None))
        self.embedding_txt_api_lineedit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u9009\u62e9\u4e00\u4e2a\u5d4c\u5165\u6a21\u578b\u5e76\u542f\u52a8\u670d\u52a1\u7528\u6765\u8ba1\u7b97\u8bcd\u5411\u91cf\uff0c\u6216\u76f4\u63a5\u8f93\u5165\u5d4c\u5165\u670d\u52a1\u7684v1/embeddings\u7aef\u70b9\u5730\u5740", None))
        self.embedding_txt_modelpath_button.setText(QCoreApplication.translate("Expend", u"...", None))
        self.embedding_split_label.setText(QCoreApplication.translate("Expend", u"\u5206\u6bb5\u957f\u5ea6", None))
        self.embedding_overlap_label.setText(QCoreApplication.translate("Expend", u"\u91cd\u53e0\u957f\u5ea6", None))
        self.embedding_source_doc_label.setText(QCoreApplication.translate("Expend", u"\u6e90\u6587\u6863", None))
        self.embedding_txt_lineEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u9009\u62e9\u4e0a\u4f20\u4e00\u4e2atxt\u6587\u4ef6\uff0c\u53ef\u4ee5\u5728\u5f85\u5d4c\u5165\u6587\u672c\u533a\u8fdb\u884c\u4fee\u6539", None))
        self.embedding_txt_upload.setText(QCoreApplication.translate("Expend", u"...", None))
        self.embedding_describe_label.setText(QCoreApplication.translate("Expend", u"\u77e5\u8bc6\u5e93\u63cf\u8ff0", None))
        self.embedding_txt_describe_lineEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u7b80\u5355\u63cf\u8ff0\u8fd9\u4e2a\u77e5\u8bc6\u5e93\u7684\u5185\u5bb9\uff0c\u6709\u52a9\u4e8e\u6a21\u578b\u8c03\u7528\u77e5\u8bc6\u5e93\u5de5\u5177", None))
        self.embedding_txt_embedding.setText(QCoreApplication.translate("Expend", u"\u5d4c\u5165\u6587\u672c\u6bb5", None))
        self.embedding_test_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u6d4b\u8bd5", None))
        self.embedding_test_textEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u5d4c\u5165\u6587\u672c\u6bb5\u540e\u5728\u8fd9\u91cc\u8f93\u5165\u95ee\u9898\uff0c\u70b9\u51fb\u68c0\u7d22\uff0c\u901a\u8fc7\u5411\u5d4c\u5165\u7aef\u70b9\u53d1\u9001\u67e5\u8be2\uff0c\u5c06\u8fd4\u56de\u6587\u672c\u76f8\u4f3c\u5ea6\u6700\u9ad8\u76843\u4e2a\u6587\u672c\u6bb5", None))
        self.embedding_test_pushButton.setText(QCoreApplication.translate("Expend", u"\u68c0\u7d22", None))
        self.embedding_result_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u68c0\u7d22\u7ed3\u679c", None))
        self.embedding_log_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u65e5\u5fd7", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_5), QCoreApplication.translate("Expend", u"\u77e5\u8bc6\u5e93", None))
        self.sd_set_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u914d\u7f6e", None))
        self.sd_modelpath_label.setText(QCoreApplication.translate("Expend", u"sd\u6a21\u578b\u8def\u5f84", None))
        self.sd_modelpath_lineEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u9009\u62e9stable-diffusion\u6a21\u578b", None))
        self.sd_modelpath_pushButton.setText(QCoreApplication.translate("Expend", u"...", None))
        self.sd_vaepath_label.setText(QCoreApplication.translate("Expend", u"vae\u8def\u5f84", None))
        self.sd_vaepath_lineEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u589e\u5f3a\u7740\u8272\u5668\uff0c\u6ca1\u6709\u53ef\u4e0d\u9009", None))
        self.sd_vaepath_pushButton.setText(QCoreApplication.translate("Expend", u"...", None))
        self.sd_antiprompt_label.setText(QCoreApplication.translate("Expend", u"\u53cd\u5411\u8bcd", None))
#if QT_CONFIG(tooltip)
        self.sd_antiprompt_lineEdit.setToolTip(QCoreApplication.translate("Expend", u"\u53f3\u51fb\u6062\u590d\u9ed8\u8ba4\u503c", None))
#endif // QT_CONFIG(tooltip)
        self.sd_antiprompt_lineEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u4f7f\u7528\u7eaf\u82f1\u6587\u63cf\u8ff0\uff0c\u4f60\u4e0d\u60f3\u770b\u5230\u7684\u6837\u5b50", None))
        self.sd_imagewidth_label.setText(QCoreApplication.translate("Expend", u"\u56fe\u50cf\u5bbd\u5ea6", None))
        self.sd_sampletype_label.setText(QCoreApplication.translate("Expend", u"\u91c7\u6837\u7b97\u6cd5", None))
        self.sd_samplesteps_label.setText(QCoreApplication.translate("Expend", u"\u91c7\u6837\u6b65\u6570", None))
        self.sd_imageheight_label.setText(QCoreApplication.translate("Expend", u"\u56fe\u50cf\u9ad8\u5ea6", None))
        self.sd_cfg_label.setText(QCoreApplication.translate("Expend", u"\u76f8\u5173\u7cfb\u6570", None))
        self.sd_imagenums_label.setText(QCoreApplication.translate("Expend", u"\u51fa\u56fe\u5f20\u6570", None))
        self.sd_seed_label.setText(QCoreApplication.translate("Expend", u"\u968f\u673a\u79cd\u5b50", None))
        self.sd_clip_label.setText(QCoreApplication.translate("Expend", u"clip\u8df3\u5c42", None))
        self.sd_prompt_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u63d0\u793a\u8bcd", None))
#if QT_CONFIG(tooltip)
        self.sd_prompt_textEdit.setToolTip(QCoreApplication.translate("Expend", u"\u53f3\u51fb\u6765\u753b\u51cc\u6ce2\u4e3d", None))
#endif // QT_CONFIG(tooltip)
        self.sd_prompt_textEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u4f7f\u7528\u7eaf\u82f1\u6587\u63cf\u8ff0\uff0c\u5df2\u7ecf\u9ed8\u8ba4\u5e94\u7528masterpieces, best quality, beauty, detailed, Pixar, 8k,", None))
        self.sd_draw_pushButton.setText(QCoreApplication.translate("Expend", u"\u6587\u751f\u56fe", None))
        self.sd_upload_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u4e0a\u4f20\u56fe\u50cf", None))
        self.sd_uploadimage_textEdit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u9700\u8981\u56fe\u751f\u56fe\u7684\u8bdd\uff0c\u5c06\u56fe\u50cf\u62d6\u8fdb\u6765", None))
        self.sd_draw_pushButton_2.setText(QCoreApplication.translate("Expend", u"\u56fe\u751f\u56fe", None))
        self.sd_result_groupBox.setTitle(QCoreApplication.translate("Expend", u"\u7ed3\u679c", None))
        self.sd_log_groupBox.setTitle(QCoreApplication.translate("Expend", u"sd.exe \u65e5\u5fd7", None))
        self.sd_log.setPlainText(QCoreApplication.translate("Expend", u"\u968f\u673a\u79cd\u5b50\u4e3a-1\u65f6\u6bcf\u6b21\u90fd\u4f1a\u751f\u6210\u4e0d\u4e00\u6837\u7684\u56fe\u50cf\n"
"\u63d0\u793a\u8bcd\u540e\u8ddf<lora:lora\u6a21\u578b\u540d\u5b57:1>\u53ef\u4ee5\u8c03\u7528lora\uff0c:1\u7684\u610f\u601d\u662f\u5e94\u7528lora\u7684\u5f3a\u5ea6\u7b49\u7ea7\n"
"\u5e26xl\u540d\u5b57\u7684\u6a21\u578b\u63a8\u8350\u5bbd\u9ad8\u5728768\u4ee5\u4e0a\n"
"\u753b\u73b0\u5b9eclip\u8df3\u5c42\u63a8\u83501\uff0c\u52a8\u753b\u63a8\u83502\u4ee5\u4e0a\n"
"\u82e5\u9700\u8981\u81ea\u5df1\u91cf\u5316\u6a21\u578b\u4e3agguf\u683c\u5f0f\uff0c\u4f7f\u7528\u673a\u4f53\u91ca\u653e\u7684sd.exe\u7a0b\u5e8f\uff0c\u547d\u4ee4\u884c sd.exe -M \"convert\" -m \u4f60\u7684\u6a21\u578b\u8def\u5f84 -o \u8f93\u51fa\u6a21\u578b\u540d\u79f0.gguf --type q8_0\n"
"", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_6), QCoreApplication.translate("Expend", u"\u6587\u751f\u56fe", None))
        self.whisper_modelpath_label.setText(QCoreApplication.translate("Expend", u"whisper\u6a21\u578b\u8def\u5f84", None))
        self.whisper_load_modelpath_linedit.setPlaceholderText(QCoreApplication.translate("Expend", u"\u6307\u5b9awhisper.exe\u8fd0\u884c\u65f6\u4f7f\u7528\u7684\u6a21\u578b\u8def\u5f84", None))
        self.whisper_load_modelpath_button.setText(QCoreApplication.translate("Expend", u"...", None))
        self.voice_load_groupBox_4.setTitle(QCoreApplication.translate("Expend", u"whisper.exe \u65e5\u5fd7", None))
        self.whisper_wav2text_label.setText(QCoreApplication.translate("Expend", u"\u624b\u52a8\u9009\u62e9wav\u8f6c\u6587\u5b57", None))
        self.whisper_wavpath_pushButton.setText(QCoreApplication.translate("Expend", u"\u9009\u62e9wav\u6587\u4ef6", None))
        self.whisper_format_label.setText(QCoreApplication.translate("Expend", u"\u8f6c\u6362\u683c\u5f0f", None))
        self.whisper_execute_pushbutton.setText(QCoreApplication.translate("Expend", u"\u6267\u884c\u8f6c\u6362", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_7), QCoreApplication.translate("Expend", u"\u58f0\u8f6c\u6587", None))
        self.label_14.setText(QCoreApplication.translate("Expend", u"\u53ef\u7528\u7cfb\u7edf\u58f0\u6e90", None))
        self.voice_enable_radioButton.setText(QCoreApplication.translate("Expend", u"\u542f\u7528", None))
        self.voice_log.setHtml(QCoreApplication.translate("Expend", u"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'Arial'; font-size:10pt; font-weight:400; font-style:normal;\">\n"
"<p style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">\u65bd\u5de5\u4e2d...</p></body></html>", None))
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_8), QCoreApplication.translate("Expend", u"\u6587\u8f6c\u58f0", None))
    # retranslateUi

