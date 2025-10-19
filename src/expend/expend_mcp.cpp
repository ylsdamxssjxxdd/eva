
#include "expend.h"

#include "../utils/statusindicator.h"
#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------MCP服务器相关--------------------------------
//-------------------------------------------------------------------------

void Expend::on_mcp_server_reflash_pushButton_clicked()
{
    ui->mcp_server_reflash_pushButton->setEnabled(false);
    ui->mcp_server_state_listWidget->clear(); // 清空展示的服务选项
    ui->mcp_server_statusLed->setState(MCP_CONNECT_MISS);
    mcpServerStates.clear();
    QString mcp_json_str = ui->mcp_server_config_textEdit->toPlainText(); // 获取用户的mcp服务配置
    ui->mcp_server_log_plainTextEdit->appendPlainText("start add servers...");
    if (ui->mcp_server_progressBar)
    {
        ui->mcp_server_progressBar->setRange(0, 0);
        ui->mcp_server_progressBar->setVisible(true);
    }
    emit expend2mcp_addService(mcp_json_str);
}

void Expend::on_mcp_server_refreshTools_pushButton_clicked()
{
    ui->mcp_server_log_plainTextEdit->appendPlainText("refresh tools info...");
    emit expend2mcp_refreshTools();
}

void Expend::on_mcp_server_disconnect_pushButton_clicked()
{
    ui->mcp_server_log_plainTextEdit->appendPlainText("disconnect all services");
    mcpServerStates.clear();
    if (ui->mcp_server_progressBar) ui->mcp_server_progressBar->setVisible(false);
    emit expend2mcp_disconnectAll();
}

// 帮助
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
    ui->mcp_server_config_textEdit->setText(config); // 直接将示例填入
}

// 响应mcp添加服务完毕时事件
void Expend::recv_addService_over(MCP_CONNECT_STATE state)
{
    ui->mcp_server_statusLed->setState(state);
    ui->mcp_server_reflash_pushButton->setEnabled(true);
    ui->mcp_server_log_plainTextEdit->appendPlainText("add servers over");
    if (ui->mcp_server_progressBar) ui->mcp_server_progressBar->setVisible(false);
    populateMcpToolEntries();
}

// 添加某个mcp服务完成
void Expend::recv_addService_single_over(QString name, MCP_CONNECT_STATE state)
{
    add_mcp_server_iteration(name, state);
}
// 添加mcp服务信息
void Expend::add_mcp_server_iteration(QString name, MCP_CONNECT_STATE state)
{
    mcpServerStates.insert(name, state);
}

void Expend::recv_mcp_message(QString message)
{
    ui->mcp_server_log_plainTextEdit->appendPlainText(message);
}

void Expend::recv_mcp_tools_refreshed()
{
    populateMcpToolEntries();
    if (ui->mcp_server_progressBar) ui->mcp_server_progressBar->setVisible(false);
    ui->mcp_server_log_plainTextEdit->appendPlainText("tools info updated");
}

void Expend::populateMcpToolEntries()
{
    ui->mcp_server_state_listWidget->clear();

    const QStringList previousSelection = mcpEnabledCache_;
    auto buildSelectionList = []() -> QStringList
    {
        QStringList names;
        names.reserve(static_cast<int>(MCP_TOOLS_INFO_LIST.size()));
        for (const auto &info : MCP_TOOLS_INFO_LIST)
        {
            names << info.name;
        }
        return names;
    };

    auto addToolSelection = [](const QString &toolKey, const QString &description, const QString &arguments)
    {
        auto it = std::find_if(MCP_TOOLS_INFO_LIST.begin(), MCP_TOOLS_INFO_LIST.end(),
                               [&toolKey](const TOOLS_INFO &info)
                               { return info.name == toolKey; });
        if (it == MCP_TOOLS_INFO_LIST.end())
        {
            MCP_TOOLS_INFO_LIST.emplace_back(toolKey, description, arguments);
        }
    };

    auto removeToolSelection = [](const QString &toolKey)
    {
        MCP_TOOLS_INFO_LIST.erase(std::remove_if(MCP_TOOLS_INFO_LIST.begin(),
                                                 MCP_TOOLS_INFO_LIST.end(),
                                                 [&toolKey](const TOOLS_INFO &info)
                                                 { return info.name == toolKey; }),
                                  MCP_TOOLS_INFO_LIST.end());
    };

    if (MCP_TOOLS_INFO_ALL.empty())
    {
        // 无可用工具时仍展示服务器连接状态
        for (auto it = mcpServerStates.cbegin(); it != mcpServerStates.cend(); ++it)
        {
            QListWidgetItem *item = new QListWidgetItem();
            item->setSizeHint(QSize(360, 48));

            QWidget *itemWidget = new QWidget();
            auto *layout = new QHBoxLayout(itemWidget);
            layout->setContentsMargins(8, 6, 8, 6);
            layout->setSpacing(6);

            auto *statusLed = new StatusLed(itemWidget);
            statusLed->setState(it.value());
            layout->addWidget(statusLed);

            auto *label = new QLabel(it.key(), itemWidget);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(label, 1);

            ui->mcp_server_state_listWidget->addItem(item);
            ui->mcp_server_state_listWidget->setItemWidget(item, itemWidget);
        }
        if (mcpServerStates.isEmpty())
        {
            QListWidgetItem *item = new QListWidgetItem(QStringLiteral("No MCP server connected."), ui->mcp_server_state_listWidget);
            item->setFlags(Qt::ItemIsEnabled);
        }
        const QStringList currentSelectionEmpty = buildSelectionList();
        if (currentSelectionEmpty != previousSelection)
        {
            mcpEnabledCache_ = currentSelectionEmpty;
            if (!currentSelectionEmpty.isEmpty() || !previousSelection.isEmpty())
            {
                emit expend2ui_mcpToolsChanged();
            }
        }
        else
        {
            mcpEnabledCache_ = currentSelectionEmpty;
        }
        return;
    }

    const bool autoSelectAll = MCP_TOOLS_INFO_LIST.empty();

    for (const auto &tool : MCP_TOOLS_INFO_ALL)
    {
        if (!tool.is_object()) { continue; }

        const QString serviceName = QString::fromStdString(get_string_safely(tool, "service"));
        const QString toolName = QString::fromStdString(get_string_safely(tool, "name"));
        if (serviceName.isEmpty() || toolName.isEmpty())
        {
            continue;
        }

        const QString description = QString::fromStdString(get_string_safely(tool, "description"));
        mcp::json schema = sanitize_schema(tool.value("inputSchema", mcp::json::object()));
        const QString arguments = QString::fromStdString(schema.dump());
        const QString toolKey = serviceName + "@" + toolName;

        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, toolKey);
        item->setData(Qt::UserRole + 1, description);
        item->setData(Qt::UserRole + 2, arguments);
        item->setData(Qt::UserRole + 3, serviceName);
        item->setSizeHint(QSize(420, 76));

        QWidget *itemWidget = new QWidget();
        auto *outerLayout = new QHBoxLayout(itemWidget);
        outerLayout->setContentsMargins(8, 8, 8, 8);
        outerLayout->setSpacing(10);

        auto *statusLed = new StatusLed(itemWidget);
        statusLed->setState(mcpServerStates.value(serviceName, MCP_CONNECT_MISS));
        outerLayout->addWidget(statusLed);

        auto *textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        auto *titleLabel = new QLabel(serviceName + QStringLiteral(" · ") + toolName, itemWidget);
        titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        titleLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
        textLayout->addWidget(titleLabel);

        auto *descLabel = new QLabel(description.isEmpty() ? QStringLiteral("No description provided.") : description.trimmed(), itemWidget);
        descLabel->setWordWrap(true);
        descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        descLabel->setStyleSheet(QStringLiteral("color: #555555;"));
        textLayout->addWidget(descLabel);

        outerLayout->addLayout(textLayout, 1);

        auto *toggle = new ToggleSwitch(itemWidget);
        toggle->setFixedSize(48, 24);
        outerLayout->addWidget(toggle, 0, Qt::AlignRight | Qt::AlignVCenter);

        ui->mcp_server_state_listWidget->addItem(item);
        ui->mcp_server_state_listWidget->setItemWidget(item, itemWidget);

        bool isSelected = std::any_of(MCP_TOOLS_INFO_LIST.begin(), MCP_TOOLS_INFO_LIST.end(),
                                      [&toolKey](const TOOLS_INFO &info)
                                      { return info.name == toolKey; });
        if (autoSelectAll)
        {
            addToolSelection(toolKey, description, arguments);
            isSelected = true;
        }

        toggle->blockSignals(true);
        toggle->setChecked(isSelected);
        static_cast<ToggleSwitch *>(toggle)->setHandlePosition(isSelected ? 1.0 : 0.0);
        toggle->blockSignals(false);

        connect(toggle, &QAbstractButton::toggled, this, [this, toolKey, description, arguments, addToolSelection, removeToolSelection, buildSelectionList](bool checked)
                {
                    if (checked)
                    {
                        addToolSelection(toolKey, description, arguments);
                    }
                    else
                    {
                        removeToolSelection(toolKey);
                    }
                    emit expend2ui_mcpToolsChanged();
                    ui->mcp_server_log_plainTextEdit->appendPlainText(QStringLiteral("tool %1 %2").arg(toolKey, checked ? QStringLiteral("enabled") : QStringLiteral("disabled")));
                    mcpEnabledCache_ = buildSelectionList();
                });
    }

    const QStringList currentSelection = buildSelectionList();
    if (currentSelection != previousSelection)
    {
        mcpEnabledCache_ = currentSelection;
        if (!currentSelection.isEmpty() || !previousSelection.isEmpty())
        {
            emit expend2ui_mcpToolsChanged();
        }
    }
    else
    {
        mcpEnabledCache_ = currentSelection;
    }
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
