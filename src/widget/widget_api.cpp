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
    api_dialog->resize(420, 120);

    QVBoxLayout *layout = new QVBoxLayout(api_dialog); // 垂直布局器
    linkTabWidget = new QTabWidget(api_dialog);
    linkTabWidget->setTabPosition(QTabWidget::North);

    // API 链接页
    apiTabWidget = new QWidget(api_dialog);
    QVBoxLayout *apiLayout = new QVBoxLayout(apiTabWidget);
    QHBoxLayout *layout_H1 = new QHBoxLayout(); // 水平布局器
    api_endpoint_label = new QLabel(jtr("api endpoint"), this);
    api_endpoint_label->setFixedWidth(80);
    layout_H1->addWidget(api_endpoint_label);
    api_endpoint_LineEdit = new QLineEdit(this);
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_endpoint_LineEdit->setText(apis.api_endpoint);
    layout_H1->addWidget(api_endpoint_LineEdit);
    apiLayout->addLayout(layout_H1); // 将布局添加到垂直布局
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
    apiLayout->addLayout(layout_H2); // 将布局添加到垂直布局
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
    apiLayout->addLayout(layout_H3); // 将布局添加到垂直布局
    apiTabWidget->setLayout(apiLayout);
    linkTabWidget->addTab(apiTabWidget, jtr("api link tab"));

    // 机体控制页
    controlTabWidget = new QWidget(api_dialog);
    QVBoxLayout *ctrlLayout = new QVBoxLayout(controlTabWidget);
    QHBoxLayout *ctrlHostLayout = new QHBoxLayout();
    control_host_label = new QLabel(jtr("control target"), this);
    control_host_label->setFixedWidth(80);
    ctrlHostLayout->addWidget(control_host_label);
    control_host_LineEdit = new QLineEdit(this);
    control_host_LineEdit->setPlaceholderText(jtr("control target placeholder"));
    control_host_LineEdit->setText(controlTargetHost_.isEmpty() ? QStringLiteral("127.0.0.1") : controlTargetHost_);
    ctrlHostLayout->addWidget(control_host_LineEdit);
    ctrlLayout->addLayout(ctrlHostLayout);

    QHBoxLayout *ctrlPortLayout = new QHBoxLayout();
    control_port_label = new QLabel(jtr("control port"), this);
    control_port_label->setFixedWidth(80);
    ctrlPortLayout->addWidget(control_port_label);
    control_port_LineEdit = new QLineEdit(this);
    control_port_LineEdit->setPlaceholderText(QString::number(DEFAULT_CONTROL_PORT));
    control_port_LineEdit->setText(QString::number(controlTargetPort_));
    ctrlPortLayout->addWidget(control_port_LineEdit);
    ctrlLayout->addLayout(ctrlPortLayout);

    QHBoxLayout *ctrlTokenLayout = new QHBoxLayout();
    control_token_label = new QLabel(jtr("control token"), this);
    control_token_label->setFixedWidth(80);
    ctrlTokenLayout->addWidget(control_token_label);
    control_token_LineEdit = new QLineEdit(this);
    control_token_LineEdit->setPlaceholderText(jtr("control token placeholder"));
    control_token_LineEdit->setEchoMode(QLineEdit::Password);
    control_token_LineEdit->setText(controlToken_);
    ctrlTokenLayout->addWidget(control_token_LineEdit);
    // 控制令牌目前无需用户输入，隐藏避免干扰
    control_token_label->setVisible(false);
    control_token_LineEdit->setVisible(false);
    ctrlLayout->addLayout(ctrlTokenLayout);
    ctrlLayout->addStretch();
    controlTabWidget->setLayout(ctrlLayout);
    linkTabWidget->addTab(controlTabWidget, jtr("control link tab"));

    layout->addWidget(linkTabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog); // 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::beginControlLink);
    connect(buttonBox, &QDialogButtonBox::accepted, api_dialog, &QDialog::reject); // 点击确定后直接退出
    connect(buttonBox, &QDialogButtonBox::rejected, api_dialog, &QDialog::reject);
}
