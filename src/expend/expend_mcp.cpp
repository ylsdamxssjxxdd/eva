
#include "expend.h"

#include "../utils/statusindicator.h"
#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QVector>
#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------MCP服务器相关--------------------------------
//-------------------------------------------------------------------------

void Expend::on_mcp_server_reflash_pushButton_clicked()
{
    ui->mcp_server_reflash_pushButton->setEnabled(false);
    if (ui->mcp_server_treeWidget) ui->mcp_server_treeWidget->clear(); // 清空展示的服务选项
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
    if (ui->mcp_server_treeWidget) ui->mcp_server_treeWidget->clear();
    mcpServiceSelections_.clear();
    mcpDisabledServices_.clear();
    mcpEnabledCache_.clear();
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
    if (!ui->mcp_server_treeWidget) return;

    ui->mcp_server_treeWidget->clear();
    ui->mcp_server_treeWidget->setColumnCount(1);
    ui->mcp_server_treeWidget->setHeaderHidden(true);
    ui->mcp_server_treeWidget->setIndentation(16);

    const QStringList previousSelection = mcpEnabledCache_;
    auto buildSelectionList = []() -> QStringList
    {
        QStringList names;
        names.reserve(static_cast<int>(MCP_TOOLS_INFO_LIST.size()));
        for (const auto &info : MCP_TOOLS_INFO_LIST) { names << info.name; }
        names.sort();
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

    QHash<QString, QList<const mcp::json *>> serviceTools;
    for (auto it = MCP_TOOLS_INFO_ALL.begin(); it != MCP_TOOLS_INFO_ALL.end(); ++it)
    {
        if (!it->is_object()) continue;
        const QString serviceName = QString::fromStdString(get_string_safely(*it, "service"));
        if (serviceName.isEmpty()) continue;
        serviceTools[serviceName].append(&(*it));
    }

    QSet<QString> serviceNames = mcpServerStates.keys().toSet();
    for (auto it = serviceTools.cbegin(); it != serviceTools.cend(); ++it) serviceNames.insert(it.key());

    QStringList serviceNameList = serviceNames.values();
    serviceNameList.sort(Qt::CaseInsensitive);

    QHash<QString, QSet<QString>> currentSelectionMap;
    for (const auto &info : MCP_TOOLS_INFO_LIST)
    {
        const QString service = info.name.section('@', 0, 0);
        if (service.isEmpty()) continue;
        currentSelectionMap[service].insert(info.name);
    }

    for (auto it = currentSelectionMap.begin(); it != currentSelectionMap.end(); ++it)
    {
        mcpServiceSelections_[it.key()] = it.value();
    }

    QStringList staleSelections;
    for (auto it = mcpServiceSelections_.cbegin(); it != mcpServiceSelections_.cend(); ++it)
    {
        if (!serviceNames.contains(it.key())) staleSelections.append(it.key());
    }
    for (const QString &service : staleSelections)
    {
        mcpServiceSelections_.remove(service);
    }
    for (auto it = mcpDisabledServices_.begin(); it != mcpDisabledServices_.end();)
    {
        if (!serviceNames.contains(*it))
        {
            it = mcpDisabledServices_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const QString &service : serviceNameList)
    {
        if (!mcpServiceSelections_.contains(service))
        {
            mcpServiceSelections_.insert(service, {});
        }
    }

    const bool autoSelectAll = MCP_TOOLS_INFO_LIST.empty();

    if (serviceNameList.isEmpty())
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->mcp_server_treeWidget);
        item->setText(0, tr("No MCP server connected."));
        item->setFlags(Qt::ItemIsEnabled);
    }

    for (const QString &serviceName : serviceNameList)
    {
        const bool serviceEnabled = !mcpDisabledServices_.contains(serviceName);
        QSet<QString> &serviceSelection = mcpServiceSelections_[serviceName];
        const QList<const mcp::json *> tools = serviceTools.value(serviceName);

        if (serviceEnabled && serviceSelection.isEmpty() && autoSelectAll && !tools.isEmpty())
        {
            for (const mcp::json *toolJson : tools)
            {
                const QString toolKey = serviceName + "@" + QString::fromStdString(get_string_safely(*toolJson, "name"));
                const QString description = QString::fromStdString(get_string_safely(*toolJson, "description"));
                mcp::json schema = sanitize_schema(toolJson->value("inputSchema", mcp::json::object()));
                const QString arguments = QString::fromStdString(schema.dump());
                serviceSelection.insert(toolKey);
                addToolSelection(toolKey, description, arguments);
            }
        }

        QTreeWidgetItem *serviceItem = new QTreeWidgetItem(ui->mcp_server_treeWidget);
        serviceItem->setFirstColumnSpanned(true);
        serviceItem->setExpanded(true);

        QWidget *serviceWidget = new QWidget();
        auto *serviceLayout = new QHBoxLayout(serviceWidget);
        serviceLayout->setContentsMargins(8, 4, 8, 4);
        serviceLayout->setSpacing(8);

        auto *statusLed = new StatusLed(serviceWidget);
        statusLed->setState(mcpServerStates.value(serviceName, MCP_CONNECT_MISS));
        serviceLayout->addWidget(statusLed);

        auto *label = new QLabel(serviceName, serviceWidget);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setStyleSheet(QStringLiteral("font-weight: 600;"));
        serviceLayout->addWidget(label, 1);

        auto *serviceToggle = new ToggleSwitch(serviceWidget);
        serviceToggle->setFixedSize(48, 24);
        serviceToggle->blockSignals(true);
        serviceToggle->setChecked(serviceEnabled);
        serviceToggle->setHandlePosition(serviceEnabled ? 1.0 : 0.0);
        serviceToggle->blockSignals(false);
        serviceLayout->addWidget(serviceToggle, 0, Qt::AlignRight | Qt::AlignVCenter);

        ui->mcp_server_treeWidget->setItemWidget(serviceItem, 0, serviceWidget);

        QVector<ToggleSwitch *> childToggles;
        QVector<QString> childToolKeys;
        QVector<QString> childDescriptions;
        QVector<QString> childArguments;

        if (tools.isEmpty())
        {
            QTreeWidgetItem *emptyItem = new QTreeWidgetItem(serviceItem);
            emptyItem->setFirstColumnSpanned(true);
            QWidget *wrap = new QWidget();
            auto *layout = new QHBoxLayout(wrap);
            layout->setContentsMargins(24, 2, 8, 2);
            auto *emptyLabel = new QLabel(tr("No tools available."), wrap);
            emptyLabel->setEnabled(false);
            emptyLabel->setStyleSheet(QStringLiteral("color:#888888;"));
            layout->addWidget(emptyLabel);
            ui->mcp_server_treeWidget->setItemWidget(emptyItem, 0, wrap);
        }
        else
        {
            for (const mcp::json *toolJson : tools)
            {
                const QString toolName = QString::fromStdString(get_string_safely(*toolJson, "name"));
                const QString description = QString::fromStdString(get_string_safely(*toolJson, "description"));
                mcp::json schema = sanitize_schema(toolJson->value("inputSchema", mcp::json::object()));
                const QString arguments = QString::fromStdString(schema.dump());
                const QString toolKey = serviceName + "@" + toolName;

                QTreeWidgetItem *toolItem = new QTreeWidgetItem(serviceItem);
                toolItem->setFirstColumnSpanned(true);

                QWidget *toolWidget = new QWidget();
                toolWidget->setFixedHeight(36);
                auto *toolLayout = new QHBoxLayout(toolWidget);
                toolLayout->setContentsMargins(32, 2, 8, 2);
                toolLayout->setSpacing(6);

                auto *iconLabel = new QLabel(toolWidget);
                iconLabel->setFixedSize(22, 22);
                QPixmap toolPixmap(QStringLiteral(":/logo/Tools.ico"));
                if (!toolPixmap.isNull())
                {
                    iconLabel->setPixmap(toolPixmap.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                iconLabel->setAlignment(Qt::AlignCenter);
                toolLayout->addWidget(iconLabel);

                auto *toolLabel = new QLabel(toolName + QStringLiteral(" — ") + description, toolWidget);
                toolLabel->setWordWrap(false);
                toolLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                toolLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                toolLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                toolLabel->setToolTip(toolName + QStringLiteral("\n") + description);
                toolLayout->addWidget(toolLabel, 1);

                auto *toolToggle = new ToggleSwitch(toolWidget);
                toolToggle->setFixedSize(44, 22);

                const bool toolSelected = serviceSelection.contains(toolKey) && serviceEnabled;
                toolToggle->blockSignals(true);
                toolToggle->setChecked(toolSelected);
                toolToggle->setHandlePosition(toolSelected ? 1.0 : 0.0);
                toolToggle->setEnabled(serviceEnabled);
                toolToggle->blockSignals(false);

                toolLayout->addWidget(toolToggle, 0, Qt::AlignRight | Qt::AlignVCenter);
                ui->mcp_server_treeWidget->setItemWidget(toolItem, 0, toolWidget);

                childToggles.append(toolToggle);
                childToolKeys.append(toolKey);
                childDescriptions.append(description);
                childArguments.append(arguments);

                connect(toolToggle, &QAbstractButton::toggled, this,
                        [this, serviceName, toolKey, description, arguments, addToolSelection, removeToolSelection, buildSelectionList](bool checked)
                        {
                            QSet<QString> &selection = mcpServiceSelections_[serviceName];
                            if (checked)
                            {
                                selection.insert(toolKey);
                                addToolSelection(toolKey, description, arguments);
                            }
                            else
                            {
                                selection.remove(toolKey);
                                removeToolSelection(toolKey);
                            }
                            mcpEnabledCache_ = buildSelectionList();
                            emit expend2ui_mcpToolsChanged();
                            ui->mcp_server_log_plainTextEdit->appendPlainText(QStringLiteral("tool %1 %2")
                                                                                    .arg(toolKey, checked ? QStringLiteral("enabled") : QStringLiteral("disabled")));
                        });
            }
        }

        connect(serviceToggle, &QAbstractButton::toggled, this,
                [this, serviceName, childToggles, childToolKeys, childDescriptions, childArguments, addToolSelection, removeToolSelection, buildSelectionList](bool enabled)
                {
                    if (enabled)
                    {
                        mcpDisabledServices_.remove(serviceName);
                    }
                    else
                    {
                        mcpDisabledServices_.insert(serviceName);
                    }

                    QSet<QString> &selection = mcpServiceSelections_[serviceName];

                    for (int i = 0; i < childToggles.size(); ++i)
                    {
                        ToggleSwitch *toggle = childToggles[i];
                        const QString &toolKey = childToolKeys[i];
                        toggle->blockSignals(true);
                        if (enabled)
                        {
                            const bool shouldEnable = selection.contains(toolKey);
                            toggle->setEnabled(true);
                            toggle->setChecked(shouldEnable);
                            toggle->setHandlePosition(shouldEnable ? 1.0 : 0.0);
                            if (shouldEnable)
                            {
                                addToolSelection(toolKey, childDescriptions[i], childArguments[i]);
                            }
                            else
                            {
                                removeToolSelection(toolKey);
                            }
                        }
                        else
                        {
                            toggle->setChecked(false);
                            toggle->setHandlePosition(0.0);
                            toggle->setEnabled(false);
                            removeToolSelection(toolKey);
                        }
                        toggle->blockSignals(false);
                    }

                    mcpEnabledCache_ = buildSelectionList();
                    emit expend2ui_mcpToolsChanged();
                    ui->mcp_server_log_plainTextEdit->appendPlainText(QStringLiteral("service %1 %2")
                                                                           .arg(serviceName, enabled ? QStringLiteral("enabled") : QStringLiteral("disabled")));
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
