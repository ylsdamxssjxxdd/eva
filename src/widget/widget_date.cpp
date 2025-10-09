#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//--------------------------------约定选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_DateDialog()
{
    //初始化约定窗口
    date_dialog = new QDialog(this);
    date_dialog->setWindowTitle(jtr("date"));
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); //隐藏?按钮
    date_ui = new Ui::Date_Dialog_Ui;
    date_ui->setupUi(date_dialog);
    for (const QString &key : date_map.keys())
    {
        date_ui->chattemplate_comboBox->addItem(key);
    }
    date_ui->chattemplate_comboBox->addItem(jtr("custom set1")); //添加自定义模板
    date_ui->chattemplate_comboBox->addItem(jtr("custom set2")); //添加自定义模板
    date_ui->chattemplate_comboBox->setCurrentText(ui_template); //默认使用default的提示词模板
    connect(date_ui->chattemplate_comboBox, &QComboBox::currentTextChanged, this, &Widget::prompt_template_change);
    connect(date_ui->confirm_button, &QPushButton::clicked, this, &Widget::date_ui_confirm_button_clicked);
    connect(date_ui->cancel_button, &QPushButton::clicked, this, &Widget::date_ui_cancel_button_clicked);
    connect(date_ui->switch_lan_button, &QPushButton::clicked, this, &Widget::switch_lan_change);
    connect(date_ui->knowledge_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);       //点击工具响应
    connect(date_ui->stablediffusion_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->calculator_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);      //点击工具响应
    connect(date_ui->controller_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);      //点击工具响应
    connect(date_ui->MCPtools_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);        //点击工具响应
    connect(date_ui->engineer_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);        //点击工具响应

    if (language_flag == 0)
    {
        ui_extra_lan = "zh";
    }
    if (language_flag == 1)
    {
        ui_extra_lan = "en";
    }

    prompt_template_change(); //先应用提示词模板    // Auto-save on template/tool toggles (no reset)
    auto autosave = [this]() { get_date(); auto_save_user(); };
    connect(date_ui->chattemplate_comboBox, &QComboBox::currentTextChanged, this, [=](const QString&){ autosave(); });
    connect(date_ui->knowledge_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
    connect(date_ui->stablediffusion_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
    connect(date_ui->calculator_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
    connect(date_ui->controller_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
    connect(date_ui->MCPtools_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
    connect(date_ui->engineer_checkbox, &QCheckBox::stateChanged, this, [=](int){ autosave(); });
}

// 约定选项卡确认按钮响应
void Widget::date_ui_confirm_button_clicked()
{
    date_dialog->close();
    set_date();
}

// 约定选项卡取消按钮响应
void Widget::date_ui_cancel_button_clicked()
{
    date_dialog->close();
    cancel_date();
}


