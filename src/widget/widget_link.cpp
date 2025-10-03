#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//----------------------------------链接相关--------------------------------
//-------------------------------------------------------------------------

//应用api设置
void Widget::set_api()
{
    // 纯请求式：不再使用本地嵌入模型进程（xbot）
    is_load = false;  // 重置
    historypath = ""; // 重置

    //获取设置值
    apis.api_endpoint = api_endpoint_LineEdit->text();
    apis.api_key = api_key_LineEdit->text();
    apis.api_model = api_model_LineEdit->text();

    ui_mode = LINK_MODE; //按照链接模式的行为来
    reflash_state("ui:" + jtr("eva link"), EVA_SIGNAL);
    if (ui_state == CHAT_STATE)
    {
        current_api = apis.api_endpoint + apis.api_chat_endpoint;
    }
    else
    {
        current_api = apis.api_endpoint + apis.api_completion_endpoint;
    }
    EVA_title = jtr("current api") + " " + current_api;
    reflash_state("ui:" + EVA_title, USUAL_SIGNAL);
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon); //设置应用程序图标
    trayIcon->setIcon(EVA_icon);           // 设置系统托盘图标
    ui->kv_bar->setToolTip("");

    emit ui2net_apis(apis);
    ui->output->clear();
    reflash_output(ui_DATES.date_prompt, 0, SYSTEM_BLUE);
    //构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    ui_state_normal();
    auto_save_user();
}

//链接模式下工具返回结果时延迟发送
void Widget::tool_testhandleTimeout()
{
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.date_prompt;
    data.input_pfx = ui_DATES.user_name;
    data.input_sfx = ui_DATES.model_name;
    data.stopwords = ui_DATES.extra_stop_words;
    if (ui_state == COMPLETE_STATE)
    {
        data.is_complete_state = true;
    }
    else
    {
        data.is_complete_state = false;
    }
    data.temp = ui_SETTINGS.temp;
    data.repeat = ui_SETTINGS.repeat;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.messagesArray = ui_messagesArray;

    emit ui2net_data(data);
    emit ui2net_push();
}

void Widget::send_testhandleTimeout()
{
    on_send_clicked();
}

//链接模式切换时某些控件可见状态
void Widget::change_api_dialog(bool enable)
{
    settings_ui->repeat_label->setVisible(enable);
    settings_ui->repeat_slider->setVisible(enable);
    settings_ui->nctx_label->setVisible(enable);
    settings_ui->nctx_slider->setVisible(enable);
    settings_ui->nthread_label->setVisible(enable);
    settings_ui->nthread_slider->setVisible(enable);
    settings_ui->mmproj_label->setVisible(enable);
    settings_ui->mmproj_LineEdit->setVisible(enable);
    settings_ui->ngl_label->setVisible(enable);
    settings_ui->ngl_slider->setVisible(enable);
    settings_ui->lora_label->setVisible(enable);
    settings_ui->lora_LineEdit->setVisible(enable);
    settings_ui->port_label->setVisible(enable);
    settings_ui->port_lineEdit->setVisible(enable);
    // 服务状态已移除
    settings_ui->frame_label->setVisible(enable);
    settings_ui->frame_lineEdit->setVisible(enable);
}
