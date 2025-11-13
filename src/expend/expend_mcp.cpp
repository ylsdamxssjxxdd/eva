
#include "expend.h"

#include "../utils/statusindicator.h"
#include <algorithm>
#include <limits>
#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QPixmap>
#include <QRect>
#include <QSaveFile>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QVector>
#include "ui_expend.h"

void Expend::on_mcp_server_reflash_pushButton_clicked()
{
    flushMcpConfigToDisk();
    ui->mcp_server_reflash_pushButton->setEnabled(false);
    if (ui->mcp_server_treeWidget) ui->mcp_server_treeWidget->clear();
    ui->mcp_server_statusLed->setState(MCP_CONNECT_MISS);
    mcpServerStates.clear();
    QString mcp_json_str = ui->mcp_server_config_textEdit->toPlainText();
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
    static const QString dialogText = R"(必填结构
{
    "mcpServers": {
        "<唯一标识>": {
            "type": "sse|streamableHttp|stdio",
            "description": "服务说明",
            "isActive": true,
            "...其它字段..."
        }
    }
}

示例一：streamableHttp
{
    "mcpServers": {
        "TestStreamable": {
            "type": "streamableHttp",
            "name": "内网 streamable 服务",
            "baseUrl": "http://192.168.0.10/mcp/server/XXXX/mcp",
            "headers": {
                "Authorization": "Bearer sk-***"
            }
        }
    }
}

示例二：sse
{
    "mcpServers": {
        "QwenTextToSpeech": {
            "type": "sse",
            "name": "DashScope 语音",
            "baseUrl": "https://dashscope.aliyuncs.com/api/v1/mcps/QwenTextToSpeech/sse",
            "headers": {
                "Authorization": "Bearer {api-key}"
            }
        }
    }
}

示例三：stdio
{
    "mcpServers": {
        "EverythingServer": {
            "type": "stdio",
            "name": "MCP Stdio Demo",
            "command": "npx",
            "args": ["-y", "@modelcontextprotocol/server-everything"],
            "env": {
                "MCP_DEBUG": "1"
            }
        }
    }
})";

    QDialog dialog(this);
    dialog.setWindowTitle(jtr("MCP examples"));
    dialog.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *label = new QLabel(jtr("MCP Examples tips"));
    label->setWordWrap(true);
    layout->addWidget(label);

    QPlainTextEdit *viewer = new QPlainTextEdit(&dialog);
    viewer->setPlainText(dialogText);
    viewer->setReadOnly(true);
    QFont mono = viewer->font();
    mono.setFamily(QStringLiteral("Consolas"));
    viewer->setFont(mono);
    layout->addWidget(viewer);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.resize(560, 520);
    dialog.exec();
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
    ui->mcp_server_treeWidget->setIndentation(14);
    ui->mcp_server_treeWidget->setUniformRowHeights(false);
    ui->mcp_server_treeWidget->setRootIsDecorated(false);
    ui->mcp_server_treeWidget->setItemsExpandable(true);
    ui->mcp_server_treeWidget->setAnimated(true);
    ui->mcp_server_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->mcp_server_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->mcp_server_treeWidget->setFocusPolicy(Qt::NoFocus);
    ui->mcp_server_treeWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->mcp_server_treeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    if (!mcpTreeSignalsInitialized_)
    {
        mcpTreeSignalsInitialized_ = true;
        connect(ui->mcp_server_treeWidget, &QTreeWidget::itemExpanded, this,
                [this](QTreeWidgetItem *item) { updateMcpServiceExpander(item, true); });
        connect(ui->mcp_server_treeWidget, &QTreeWidget::itemCollapsed, this,
                [this](QTreeWidgetItem *item) { updateMcpServiceExpander(item, false); });
    }

    mcpServiceExpandButtons_.clear();

    const int viewportWidth = std::max(220, ui->mcp_server_treeWidget->viewport()->width());
    auto calcTextHeight = [](const QFontMetrics &metrics, const QString &text, int width, int lineHeight, int maxLines) -> int
    {
        if (width <= 0) return lineHeight;
        const int maxHeight = (maxLines > 0) ? (lineHeight * maxLines) : (std::numeric_limits<int>::max() / 4);
        const QRect rect = metrics.boundingRect(QRect(0, 0, width, maxHeight), Qt::TextWordWrap, text);
        const int bounded = std::max(lineHeight, rect.height());
        return (maxLines > 0) ? std::min(bounded, maxHeight) : bounded;
    };

    auto connectStateLabel = [this](MCP_CONNECT_STATE state) -> QString
    {
        switch (state)
        {
            case MCP_CONNECT_LINK: return tr("已连接");
            case MCP_CONNECT_WIP: return tr("等待连接");
            case MCP_CONNECT_MISS: default: return tr("未连接");
        }
    };

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
        serviceWidget->setObjectName(QStringLiteral("mcpServiceRow"));
        serviceWidget->setAttribute(Qt::WA_StyledBackground, true);
        serviceWidget->setStyleSheet(QStringLiteral("#mcpServiceRow { background-color: rgba(30, 38, 70, 0.35); border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; }"));
        serviceWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *serviceLayout = new QHBoxLayout(serviceWidget);
        serviceLayout->setContentsMargins(12, 10, 12, 10);
        serviceLayout->setSpacing(12);

        auto *expanderButton = new QToolButton(serviceWidget);
        expanderButton->setAutoRaise(true);
        expanderButton->setCursor(Qt::PointingHandCursor);
        expanderButton->setToolTip(tr("展开/收起工具"));
        expanderButton->setIconSize(QSize(14, 14));
        expanderButton->setFixedSize(26, 26);
        expanderButton->setFocusPolicy(Qt::NoFocus);
        expanderButton->setStyleSheet(QStringLiteral("QToolButton { border: none; background-color: transparent; } QToolButton:hover { background-color: rgba(255,255,255,0.08); border-radius: 13px; }"));
        serviceLayout->addWidget(expanderButton);

        auto *statusLed = new StatusLed(serviceWidget);
        statusLed->setState(mcpServerStates.value(serviceName, MCP_CONNECT_MISS));
        serviceLayout->addWidget(statusLed);

        auto *serviceToggle = new ToggleSwitch(serviceWidget);
        serviceToggle->setFixedSize(48, 24);
        serviceToggle->blockSignals(true);
        serviceToggle->setChecked(serviceEnabled);
        serviceToggle->setHandlePosition(serviceEnabled ? 1.0 : 0.0);
        serviceToggle->blockSignals(false);

        QWidget *textArea = new QWidget(serviceWidget);
        textArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *textLayout = new QVBoxLayout(textArea);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(4);

        auto *nameLabel = new QLabel(serviceName, serviceWidget);
        nameLabel->setObjectName(QStringLiteral("mcpServiceName"));
        nameLabel->setWordWrap(true);
        nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        nameLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
        nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        nameLabel->setToolTip(serviceName);

        auto *metaLabel = new QLabel(serviceWidget);
        metaLabel->setObjectName(QStringLiteral("mcpServiceMeta"));
        metaLabel->setWordWrap(true);
        metaLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        metaLabel->setStyleSheet(QStringLiteral("color: #93a2cd; font-size: 12px;"));

        QStringList metaPieces;
        metaPieces << tr("%1 个工具").arg(tools.size());
        metaPieces << connectStateLabel(mcpServerStates.value(serviceName, MCP_CONNECT_MISS));
        if (!serviceEnabled) metaPieces << tr("服务已禁用");
        metaLabel->setText(metaPieces.join(QStringLiteral(" · ")));

        const int reservedWidth = expanderButton->sizeHint().width() + statusLed->sizeHint().width()
                                  + serviceToggle->sizeHint().width() + serviceLayout->contentsMargins().left()
                                  + serviceLayout->contentsMargins().right() + serviceLayout->spacing() * 3;
        const int textWidth = std::max(200, viewportWidth - reservedWidth);
        const QFontMetrics nameMetrics(nameLabel->font());
        const int nameLineHeight = nameMetrics.lineSpacing();
        const int nameHeight = calcTextHeight(nameMetrics, serviceName, textWidth, nameLineHeight, 3);
        nameLabel->setMinimumHeight(nameHeight);
        nameLabel->setMaximumHeight(nameHeight);

        const QFontMetrics metaMetrics(metaLabel->font());
        const int metaLineHeight = metaMetrics.lineSpacing();
        const int metaHeight = calcTextHeight(metaMetrics, metaLabel->text(), textWidth, metaLineHeight, 2);
        metaLabel->setMinimumHeight(metaHeight);
        metaLabel->setMaximumHeight(metaHeight);

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(metaLabel);
        serviceLayout->addWidget(textArea, 1);
        serviceLayout->addWidget(serviceToggle, 0, Qt::AlignRight | Qt::AlignVCenter);

        const int serviceRowHeight = std::max(64, serviceLayout->contentsMargins().top()
                                                       + serviceLayout->contentsMargins().bottom()
                                                       + nameHeight + metaHeight + textLayout->spacing());
        serviceWidget->setMinimumHeight(serviceRowHeight);
        serviceWidget->setMaximumHeight(serviceRowHeight);
        serviceItem->setSizeHint(0, QSize(0, serviceRowHeight));

        ui->mcp_server_treeWidget->setItemWidget(serviceItem, 0, serviceWidget);

        mcpServiceExpandButtons_.insert(serviceItem, expanderButton);
        updateMcpServiceExpander(serviceItem, serviceItem->isExpanded());
        connect(expanderButton, &QToolButton::clicked, this,
                [serviceItem]()
                {
                    if (!serviceItem) return;
                    serviceItem->setExpanded(!serviceItem->isExpanded());
                });

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
                const QString arguments = QString::fromStdString(schema.dump(2));
                const QString toolKey = serviceName + "@" + toolName;

                QTreeWidgetItem *toolItem = new QTreeWidgetItem(serviceItem);
                toolItem->setFirstColumnSpanned(true);

                QWidget *toolWidget = new QWidget();
                toolWidget->setObjectName(QStringLiteral("mcpToolRow"));
                toolWidget->setAttribute(Qt::WA_StyledBackground, true);
                toolWidget->setStyleSheet(QStringLiteral("#mcpToolRow { background-color: rgba(7, 10, 26, 0.65); border: 1px solid rgba(255,255,255,0.05); border-radius: 8px; }"));
                toolWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                auto *toolLayout = new QVBoxLayout(toolWidget);
                toolLayout->setContentsMargins(46, 10, 12, 10);
                toolLayout->setSpacing(8);

                QWidget *headerRow = new QWidget(toolWidget);
                auto *headerLayout = new QHBoxLayout(headerRow);
                headerLayout->setContentsMargins(0, 0, 0, 0);
                headerLayout->setSpacing(12);

                auto *toolExpander = new QToolButton(headerRow);
                toolExpander->setAutoRaise(true);
                toolExpander->setCursor(Qt::PointingHandCursor);
                toolExpander->setArrowType(Qt::RightArrow);
                toolExpander->setCheckable(true);
                toolExpander->setIconSize(QSize(12, 12));
                toolExpander->setFixedSize(24, 24);
                toolExpander->setFocusPolicy(Qt::NoFocus);
                toolExpander->setToolTip(tr("查看参数需求"));
                toolExpander->setStyleSheet(QStringLiteral("QToolButton { border: none; background-color: transparent; } QToolButton:hover { background-color: rgba(255,255,255,0.08); border-radius: 12px; }"));
                headerLayout->addWidget(toolExpander);

                auto *iconLabel = new QLabel(headerRow);
                iconLabel->setFixedSize(22, 22);
                QPixmap toolPixmap(QStringLiteral(":/logo/Tools.ico"));
                if (!toolPixmap.isNull())
                {
                    iconLabel->setPixmap(toolPixmap.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                iconLabel->setAlignment(Qt::AlignCenter);
                headerLayout->addWidget(iconLabel);

                auto *toolToggle = new ToggleSwitch(headerRow);
                toolToggle->setFixedSize(44, 22);

                const int layoutMargins = toolLayout->contentsMargins().left() + toolLayout->contentsMargins().right();
                const int textWidth = std::max(140, viewportWidth - (layoutMargins + iconLabel->sizeHint().width()
                                                                     + toolToggle->sizeHint().width() + toolExpander->sizeHint().width()
                                                                     + headerLayout->spacing() * 3));

                auto *textContainer = new QWidget(headerRow);
                textContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                auto *textLayout = new QVBoxLayout(textContainer);
                textLayout->setContentsMargins(0, 0, 0, 0);
                textLayout->setSpacing(2);

                auto *toolNameLabel = new QLabel(toolName, textContainer);
                toolNameLabel->setWordWrap(true);
                toolNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                toolNameLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                toolNameLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
                toolNameLabel->setToolTip(toolName);
                const int toolNameLineHeight = toolNameLabel->fontMetrics().lineSpacing();
                const int maxToolNameLines = 2;
                const int toolNameHeight = calcTextHeight(toolNameLabel->fontMetrics(), toolName, textWidth,
                                                          toolNameLineHeight, maxToolNameLines);
                toolNameLabel->setMinimumHeight(toolNameHeight);
                toolNameLabel->setMaximumHeight(toolNameHeight);
                toolNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

                auto *toolDescLabel = new QLabel(description, textContainer);
                toolDescLabel->setWordWrap(true);
                toolDescLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                toolDescLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
                toolDescLabel->setStyleSheet(QStringLiteral("color:#99a4bf;"));
                toolDescLabel->setToolTip(description);
                const int toolDescLineHeight = toolDescLabel->fontMetrics().lineSpacing();
                const int maxDescLines = 2;
                const int descHeight = calcTextHeight(toolDescLabel->fontMetrics(), description, textWidth,
                                                      toolDescLineHeight, maxDescLines);
                toolDescLabel->setMinimumHeight(descHeight);
                toolDescLabel->setMaximumHeight(descHeight);
                toolDescLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

                textLayout->addWidget(toolNameLabel);
                textLayout->addWidget(toolDescLabel);
                textContainer->setToolTip(toolName + QStringLiteral("\n") + description);
                headerLayout->addWidget(textContainer, 1);

                headerLayout->addWidget(toolToggle, 0, Qt::AlignRight | Qt::AlignVCenter);
                toolLayout->addWidget(headerRow);

                QWidget *paramContainer = new QWidget(toolWidget);
                paramContainer->setVisible(false);
                auto *paramLayout = new QVBoxLayout(paramContainer);
                paramLayout->setContentsMargins(28, 0, 4, 0);
                paramLayout->setSpacing(6);

                auto *paramLabel = new QLabel(tr("参数需求"), paramContainer);
                paramLabel->setStyleSheet(QStringLiteral("color:#93a2cd; font-size:12px;"));
                paramLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                paramLayout->addWidget(paramLabel, 0, Qt::AlignLeft);

                auto *paramViewer = new QPlainTextEdit(paramContainer);
                paramViewer->setReadOnly(true);
                paramViewer->setLineWrapMode(QPlainTextEdit::WidgetWidth);
                QFont monoFont = paramViewer->font();
                monoFont.setFamily(QStringLiteral("Consolas"));
                paramViewer->setFont(monoFont);
                paramViewer->setMinimumHeight(90);
                paramViewer->setMaximumHeight(200);
                paramViewer->setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; }"));
                paramViewer->setPlainText(arguments.isEmpty() ? tr("该工具未声明 inputSchema。") : arguments);
                paramLayout->addWidget(paramViewer);

                toolLayout->addWidget(paramContainer);

                const int headerHeight = toolNameHeight + descHeight + toolLayout->contentsMargins().top()
                                         + toolLayout->contentsMargins().bottom() + textLayout->spacing();
                toolWidget->setMinimumHeight(headerHeight);
                toolWidget->setMaximumHeight(QWIDGETSIZE_MAX);
                toolItem->setSizeHint(0, QSize(0, headerHeight));

                const bool toolSelected = serviceSelection.contains(toolKey) && serviceEnabled;
                toolToggle->blockSignals(true);
                toolToggle->setChecked(toolSelected);
                toolToggle->setHandlePosition(toolSelected ? 1.0 : 0.0);
                toolToggle->setEnabled(serviceEnabled);
                toolToggle->blockSignals(false);

                ui->mcp_server_treeWidget->setItemWidget(toolItem, 0, toolWidget);

                auto updateToolRowHeight = [this, toolItem, toolWidget]()
                {
                    if (!ui->mcp_server_treeWidget || !toolItem) return;
                    toolItem->setSizeHint(0, QSize(0, toolWidget->sizeHint().height()));
                    ui->mcp_server_treeWidget->doItemsLayout();
                };

                connect(toolExpander, &QToolButton::toggled, this,
                        [paramContainer, toolExpander, updateToolRowHeight](bool expanded)
                        {
                            paramContainer->setVisible(expanded);
                            toolExpander->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
                            updateToolRowHeight();
                        });

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

void Expend::updateMcpServiceExpander(QTreeWidgetItem *item, bool expanded)
{
    if (!item) return;
    auto it = mcpServiceExpandButtons_.find(item);
    if (it == mcpServiceExpandButtons_.end()) return;
    if (QToolButton *button = it.value())
    {
        button->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    }
}

void Expend::setupMcpConfigPersistence()
{
    if (!ui->mcp_server_config_textEdit) return;

    if (!mcpConfigSaveTimer_)
    {
        mcpConfigSaveTimer_ = new QTimer(this);
        mcpConfigSaveTimer_->setSingleShot(true);
        connect(mcpConfigSaveTimer_, &QTimer::timeout, this, &Expend::flushMcpConfigToDisk);
    }

    QString diskContent;
    if (loadMcpConfigFromDisk(&diskContent))
    {
        QSignalBlocker blocker(ui->mcp_server_config_textEdit);
        ui->mcp_server_config_textEdit->setPlainText(diskContent);
        mcpConfigDirty_ = false;
    }
    else
    {
        persistMcpConfigImmediately();
    }

    connect(ui->mcp_server_config_textEdit, &QTextEdit::textChanged, this, &Expend::onMcpConfigEditorTextChanged);
}

void Expend::onMcpConfigEditorTextChanged()
{
    if (!ui->mcp_server_config_textEdit) return;
    mcpConfigDirty_ = true;
    if (!mcpConfigSaveTimer_)
    {
        mcpConfigSaveTimer_ = new QTimer(this);
        mcpConfigSaveTimer_->setSingleShot(true);
        connect(mcpConfigSaveTimer_, &QTimer::timeout, this, &Expend::flushMcpConfigToDisk);
    }
    mcpConfigSaveTimer_->start(1200);
}

void Expend::flushMcpConfigToDisk()
{
    if (!ui->mcp_server_config_textEdit) return;
    if (!mcpConfigDirty_) return;

    if (mcpConfigSaveTimer_) mcpConfigSaveTimer_->stop();

    auto logFailure = [this](const QString &line)
    {
        if (ui->mcp_server_log_plainTextEdit) ui->mcp_server_log_plainTextEdit->appendPlainText(line);
    };

    const QString payload = ui->mcp_server_config_textEdit->toPlainText();
    const QString path = mcpConfigFilePath();
    QFileInfo info(path);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        logFailure(QStringLiteral("failed to prepare %1").arg(dir.absolutePath()));
        return;
    }

    QSaveFile save(path);
    if (!save.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        logFailure(QStringLiteral("failed to open %1").arg(path));
        return;
    }

    QTextStream out(&save);
    out.setCodec("UTF-8");
    out << payload;
    if (!save.commit())
    {
        logFailure(QStringLiteral("failed to commit %1").arg(path));
        return;
    }

    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("Mcpconfig", payload);

    mcpConfigDirty_ = false;
}

void Expend::persistMcpConfigImmediately()
{
    if (!ui->mcp_server_config_textEdit) return;
    mcpConfigDirty_ = true;
    flushMcpConfigToDisk();
}

bool Expend::loadMcpConfigFromDisk(QString *buffer) const
{
    if (!buffer) return false;
    QFile file(mcpConfigFilePath());
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    *buffer = in.readAll();
    return true;
}

QString Expend::mcpConfigFilePath() const
{
    return QDir(applicationDirPath).filePath(QStringLiteral("EVA_TEMP/mcp_servers.json"));
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


