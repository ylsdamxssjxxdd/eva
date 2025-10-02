
#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------MCP服务器相关--------------------------------
//-------------------------------------------------------------------------

void Expend::on_mcp_server_reflash_pushButton_clicked()
{
    ui->mcp_server_reflash_pushButton->setEnabled(false);
    ui->mcp_server_state_listWidget->clear(); //清空展示的服务选项
    ui->mcp_server_statusLed->setState(MCP_CONNECT_MISS);
    QString mcp_json_str = ui->mcp_server_config_textEdit->toPlainText(); //获取用户的mcp服务配置
    ui->mcp_server_log_plainTextEdit->appendPlainText("start add servers...");
    emit expend2mcp_addService(mcp_json_str);
}

//帮助
void Expend::on_mcp_server_help_pushButton_clicked()
{
    QString config = R"({
    "mcpServers": {
        "sse_server": {
            "url": "http://192.168.229.67:3001"
        },
        "stdio_server": {
            "command": "npx",
            "args": ["-y", "@modelcontextprotocol/server-everything"],
            "env": {"MCP_DEBUG": "1"}
        }
    }
})";
    ui->mcp_server_config_textEdit->setText(config); //直接将示例填入
}

//响应mcp添加服务完毕时事件
void Expend::recv_addService_over(MCP_CONNECT_STATE state)
{
    ui->mcp_server_statusLed->setState(state);
    ui->mcp_server_reflash_pushButton->setEnabled(true);
    ui->mcp_server_log_plainTextEdit->appendPlainText("add servers over");
    //列出所有可用工具
}

// 添加某个mcp服务完成
void Expend::recv_addService_single_over(QString name, MCP_CONNECT_STATE state)
{
    add_mcp_server_iteration(name, state);
}
//添加mcp服务信息
void Expend::add_mcp_server_iteration(QString name, MCP_CONNECT_STATE state)
{
    QListWidgetItem *item = new QListWidgetItem();
    item->setData(Qt::UserRole, name);      // 存储服务名称
    item->setData(Qt::UserRole + 1, state); // 存储服务连接状态
    item->setSizeHint(QSize(300, 50));      // 设置项大小
    QWidget *itemWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setSpacing(3);                  // 设置间距为0
    layout->setContentsMargins(3, 3, 3, 3); // 设置外部间距为0
    QPlainTextEdit *label = new QPlainTextEdit(name);
    label->setLineWrapMode(QPlainTextEdit::NoWrap);
    label->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 取消垂直滚动条
    label->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 取消垂直滚动条
    label->setReadOnly(1);
    StatusLed *statusLed = new StatusLed(this);
    statusLed->setState(state);
    layout->addWidget(label);
    layout->addWidget(statusLed);
    itemWidget->setLayout(layout);
    ui->mcp_server_state_listWidget->addItem(item);
    ui->mcp_server_state_listWidget->setItemWidget(item, itemWidget);
}

void Expend::recv_mcp_message(QString message)
{
    ui->mcp_server_log_plainTextEdit->appendPlainText(message);
}

// //添加mcp可用工具选项
// void Expend::add_mcp_tool_iteration(mcp::json toolsinfo)
// {
//     ui->mcp_server_state_listWidget->clear();
//     for (const auto& tool : toolsinfo)
//     {
//         TOOLS_INFO mcp_tools_info(
//             QString::fromStdString(tool["service"].get<std::string>() + "@" + tool["name"].get<std::string>()), // 工具名
//             QString::fromStdString(tool["description"]), // 工具描述
//             QString::fromStdString(tool["inputSchema"].dump()) // 参数结构
//         );
//         QListWidgetItem *item = new QListWidgetItem();
//         item->setData(Qt::UserRole, mcp_tools_info.name);  // 存储工具名称
//         item->setData(Qt::UserRole + 1, mcp_tools_info.description);     // 存储工具描述
//         item->setData(Qt::UserRole + 2, mcp_tools_info.arguments);     // 存储工具参数结构
//         item->setSizeHint(QSize(300, 50));          // 设置项大小
//         QWidget *itemWidget = new QWidget();
//         QHBoxLayout *layout = new QHBoxLayout(itemWidget);
//         layout->setSpacing(3);                        // 设置间距为0
//         layout->setContentsMargins(3, 3, 3, 3);       // 设置外部间距为0
//         QPlainTextEdit *label = new QPlainTextEdit(mcp_tools_info.name + ": " + mcp_tools_info.description);
//         label->setLineWrapMode(QPlainTextEdit::NoWrap);
//         label->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // 取消垂直滚动条
//         label->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // 取消垂直滚动条
//         label->setReadOnly(1);
//         ToggleSwitch *toggleSwitch = new ToggleSwitch(this);
//         toggleSwitch->setFixedSize(40, 20);
//         layout->addWidget(label);
//         layout->addWidget(toggleSwitch);
//         itemWidget->setLayout(layout);
//         ui->mcp_server_state_listWidget->addItem(item);
//         ui->mcp_server_state_listWidget->setItemWidget(item, itemWidget);
//         connect(toggleSwitch, &QAbstractButton::toggled, this, [this, item](bool checked) {
//             QString toolName = item->data(Qt::UserRole).toString();
//             QString description = item->data(Qt::UserRole + 1).toString();
//             QString parameters = item->data(Qt::UserRole + 2).toString();

//             if (checked) {
//                 // 检查是否已存在，避免重复添加
//                 bool exists = false;
//                 for (const auto& info : MCP_TOOLS_INFO_LIST) {
//                     if (info.name == toolName) {
//                         exists = true;
//                         break;
//                     }
//                 }
//                 if (!exists) {
//                     MCP_TOOLS_INFO_LIST.push_back({toolName, description, parameters});
//                 }
//             } else {
//                 // 移除所有匹配的工具
//                 MCP_TOOLS_INFO_LIST.erase(
//                     std::remove_if(
//                         MCP_TOOLS_INFO_LIST.begin(),
//                         MCP_TOOLS_INFO_LIST.end(),
//                         [&toolName](const TOOLS_INFO& info) {
//                             return info.name == toolName;
//                         }
//                     ),
//                     MCP_TOOLS_INFO_LIST.end()
//                 );
//             }
//         });
//     }
// }
