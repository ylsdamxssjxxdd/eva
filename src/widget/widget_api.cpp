#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//--------------------------------api选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::setApiDialog()
{
    api_dialog = new QDialog();
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); // 隐藏?按钮
    // api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);        //隐藏关闭按钮
    api_dialog->resize(400, 100); // 设置宽度,高度

    QVBoxLayout *layout = new QVBoxLayout(api_dialog); // 垂直布局器
    // api_endpoint
    QHBoxLayout *layout_H1 = new QHBoxLayout(); // 水平布局器
    api_endpoint_label = new QLabel(jtr("api endpoint"), this);
    api_endpoint_label->setFixedWidth(80);
    layout_H1->addWidget(api_endpoint_label);
    api_endpoint_LineEdit = new QLineEdit(this);
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_endpoint_LineEdit->setText(apis.api_endpoint);
    layout_H1->addWidget(api_endpoint_LineEdit);
    layout->addLayout(layout_H1); // 将布局添加到总布局
    // api_key
    QHBoxLayout *layout_H2 = new QHBoxLayout(); // 水平布局器
    api_key_label = new QLabel(jtr("api key"), this);
    api_key_label->setFixedWidth(80);
    layout_H2->addWidget(api_key_label);
    api_key_LineEdit = new QLineEdit(this);
    api_key_LineEdit->setEchoMode(QLineEdit::Password);
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_key_LineEdit->setText(apis.api_key);
    layout_H2->addWidget(api_key_LineEdit);
    layout->addLayout(layout_H2); // 将布局添加到总布局
    // api_model
    QHBoxLayout *layout_H3 = new QHBoxLayout(); // 水平布局器
    api_model_label = new QLabel(jtr("api model"), this);
    api_model_label->setFixedWidth(80);
    layout_H3->addWidget(api_model_label);
    api_model_LineEdit = new QLineEdit(this);
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    api_model_LineEdit->setText(apis.api_model);
    layout_H3->addWidget(api_model_LineEdit);
    layout->addLayout(layout_H3); // 将布局添加到总布局

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog); // 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_api);
    connect(buttonBox, &QDialogButtonBox::accepted, api_dialog, &QDialog::reject); // 点击确定后直接退出
    connect(buttonBox, &QDialogButtonBox::rejected, api_dialog, &QDialog::reject);
}
