#include "ui_widget.h"
#include "widget.h"
#include "../utils/textparse.h"
#include "../utils/openai_compat.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QUrl>

namespace
{
// 对用户输入的端点进行规整：去空白、补协议、压缩斜杠并去掉尾部 /v1，便于后续拼接 /v1/models
QString normalizeOpenAiBaseForModels(const QString &rawEndpoint)
{
    const QString compact = TextParse::removeAllWhitespace(rawEndpoint);
    if (compact.isEmpty()) return QString();

    QUrl url = QUrl::fromUserInput(compact);
    if (!url.isValid()) return QString();
    if (url.scheme().isEmpty()) url.setScheme(QStringLiteral("https"));

    QString path = url.path();
    QString collapsed;
    collapsed.reserve(path.size());
    bool prevSlash = false;
    for (const QChar ch : path)
    {
        if (ch == QLatin1Char('/'))
        {
            if (!prevSlash) collapsed.append(ch);
            prevSlash = true;
        }
        else
        {
            collapsed.append(ch);
            prevSlash = false;
        }
    }
    path = collapsed;
    while (path.endsWith('/') && path.length() > 1) path.chop(1);
    const QString lowerPath = path.toLower();
    if (lowerPath.endsWith(QStringLiteral("/v1"))) path.chop(3);
    if (path.isEmpty()) path = QStringLiteral("/");

    url.setPath(path);
    return url.toString(QUrl::RemoveFragment);
}
} // namespace

//-------------------------------------------------------------------------
//--------------------------------api选项相关--------------------------------
//-------------------------------------------------------------------------
void Widget::setApiDialog()
{
    api_dialog = new QDialog();
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); // 隐藏?按钮
    api_dialog->resize(420, 120);

    QVBoxLayout *layout = new QVBoxLayout(api_dialog); // 纵向布局器
    linkTabWidget = new QTabWidget(api_dialog);
    linkTabWidget->setTabPosition(QTabWidget::North);

    // API 链接页
    apiTabWidget = new QWidget(api_dialog);
    QVBoxLayout *apiLayout = new QVBoxLayout(apiTabWidget);
    QHBoxLayout *layout_H1 = new QHBoxLayout(); // 横向布局器
    api_endpoint_label = new QLabel(jtr("api endpoint"), this);
    api_endpoint_label->setFixedWidth(80);
    layout_H1->addWidget(api_endpoint_label);
    api_endpoint_LineEdit = new QLineEdit(this);
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_endpoint_LineEdit->setText(apis.api_endpoint);
    layout_H1->addWidget(api_endpoint_LineEdit);
    apiLayout->addLayout(layout_H1); // 将布局添加到纵向布局

    // api_key
    QHBoxLayout *layout_H2 = new QHBoxLayout(); // 横向布局器
    api_key_label = new QLabel(jtr("api key"), this);
    api_key_label->setFixedWidth(80);
    layout_H2->addWidget(api_key_label);
    api_key_LineEdit = new QLineEdit(this);
    api_key_LineEdit->setEchoMode(QLineEdit::Password);
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_key_LineEdit->setText(apis.api_key);
    layout_H2->addWidget(api_key_LineEdit);
    apiLayout->addLayout(layout_H2); // 将布局添加到纵向布局

    // api_model
    QHBoxLayout *layout_H3 = new QHBoxLayout(); // 横向布局器
    api_model_label = new QLabel(jtr("api model"), this);
    api_model_label->setFixedWidth(80);
    layout_H3->addWidget(api_model_label);
    api_model_LineEdit = new QLineEdit(this);
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    api_model_LineEdit->setText(apis.api_model);
    layout_H3->addWidget(api_model_LineEdit);
    apiLayout->addLayout(layout_H3); // 将布局添加到纵向布局

    // 模型候选补全：非阻塞拉取 /v1/models 列表并在行编辑点击时弹出
    if (!apiModelCompleter_)
    {
        apiModelCompleter_ = new QCompleter(this);
        apiModelCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
        apiModelCompleter_->setFilterMode(Qt::MatchContains);
        apiModelCompleter_->setModel(new QStringListModel(apiModelCompleter_));
    }
    api_model_LineEdit->setCompleter(apiModelCompleter_);

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
    // 控制令牌当前不需要用户输入，隐藏保护
    control_token_label->setVisible(false);
    control_token_LineEdit->setVisible(false);
    ctrlLayout->addLayout(ctrlTokenLayout);
    ctrlLayout->addStretch();
    controlTabWidget->setLayout(ctrlLayout);
    linkTabWidget->addTab(controlTabWidget, jtr("control link tab"));

    layout->addWidget(linkTabWidget);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog); // 创建 QDialogButtonBox
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::beginControlLink);
    connect(buttonBox, &QDialogButtonBox::accepted, api_dialog, &QDialog::reject); // 点击确定后直接退出
    connect(buttonBox, &QDialogButtonBox::rejected, api_dialog, &QDialog::reject);

    // 端点/密钥变更后自动探测模型列表，使用计时器去抖避免阻塞 UI 主线程
    if (!apiModelProbeTimer_)
    {
        apiModelProbeTimer_ = new QTimer(this);
        apiModelProbeTimer_->setSingleShot(true);
        apiModelProbeTimer_->setInterval(400);
        connect(apiModelProbeTimer_, &QTimer::timeout, this, &Widget::fetchOpenAiModelsAsync);
    }
    api_endpoint_LineEdit->installEventFilter(this);
    api_key_LineEdit->installEventFilter(this);
    api_model_LineEdit->installEventFilter(this);
    connect(api_endpoint_LineEdit, &QLineEdit::textEdited, this, &Widget::scheduleRemoteModelDiscovery);
    connect(api_key_LineEdit, &QLineEdit::textEdited, this, &Widget::scheduleRemoteModelDiscovery);
    connect(api_model_LineEdit, &QLineEdit::textEdited, this, &Widget::scheduleRemoteModelDiscovery);
    scheduleRemoteModelDiscovery();
}

// 启动去抖计时器，用户停止输入后再拉取模型列表，避免阻塞事件循环
void Widget::scheduleRemoteModelDiscovery()
{
    if (!apiModelProbeTimer_) return;
    apiModelProbeTimer_->start();
}

// 异步请求 /v1/models 并将模型 ID 写入补全器
void Widget::fetchOpenAiModelsAsync()
{
    if (apiModelFetchInFlight_) return;
    if (!api_endpoint_LineEdit || !api_key_LineEdit) return;

    const QString baseInput = api_endpoint_LineEdit->text();
    const QString keyInput = api_key_LineEdit->text();
    const QString normalizedBase = normalizeOpenAiBaseForModels(baseInput);
    const QString cleanKey = TextParse::removeAllWhitespace(keyInput);
    if (normalizedBase.isEmpty() || cleanKey.isEmpty())
    {
        apiModelCandidates_.clear();
        applyModelCompleter();
        return;
    }

    // 若端点/密钥未变且已有候选，直接复用缓存，避免重复请求
    if (normalizedBase == apiModelProbeLastEndpoint_ && cleanKey == apiModelProbeLastKey_ && !apiModelCandidates_.isEmpty())
    {
        applyModelCompleter();
        return;
    }

    apiModelFetchInFlight_ = true;
    apiModelProbeLastEndpoint_ = normalizedBase;
    apiModelProbeLastKey_ = cleanKey;

    // 模型列表端点：大多数 OpenAI 兼容服务使用 /v1/models；
    // 但火山方舟 Ark 的 base 已经包含 /api/v3，因此应使用 /models（最终拼成 /api/v3/models）
    const QUrl baseUrl(normalizedBase);
    const QUrl url = OpenAiCompat::joinPath(baseUrl, OpenAiCompat::modelsPath(baseUrl));
    if (!url.isValid())
    {
        apiModelFetchInFlight_ = false;
        return;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ") + cleanKey.toUtf8());

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *rp = nam->get(req);
    connect(rp, &QNetworkReply::finished, this, [this, nam, rp]()
            {
        rp->deleteLater();
        nam->deleteLater();
        apiModelFetchInFlight_ = false;

        if (rp->error() != QNetworkReply::NoError)
        {
            apiModelCandidates_.clear();
            applyModelCompleter();
            return;
        }

        const QByteArray body = rp->readAll();
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError)
        {
            apiModelCandidates_.clear();
            applyModelCompleter();
            return;
        }

        QStringList models;
        QSet<QString> seen;

        // 过滤“Provider: Friendly Name”这类展示用模型名（常见：OpenRouter / 聚合平台）。
        // 这类字符串往往包含冒号与空白（例如 "Google: Gemini 3 Flash Preview"），
        // 不能作为真正的 model id 使用，且与同一对象里的 id/model 字段重复，保留只会造成误选与连接失败。
        auto hasWhitespace = [](const QString &s) -> bool {
            for (const QChar ch : s)
                if (ch.isSpace()) return true;
            return false;
        };
        auto push = [&](const QString &rawId) {
            const QString id = rawId.trimmed();
            if (id.isEmpty() || seen.contains(id)) return;
            if (id.contains(QLatin1Char(':')) && hasWhitespace(id)) return;
            seen.insert(id);
            models.append(id);
        };

        if (doc.isObject())
        {
            const QJsonObject root = doc.object();
            if (root.contains("data") && root.value("data").isArray())
            {
                const QJsonArray arr = root.value("data").toArray();
                for (const auto &v : arr)
                {
                    if (!v.isObject()) continue;
                    const QJsonObject m = v.toObject();
                    push(m.value("id").toString());
                    push(m.value("model").toString());
                    push(m.value("name").toString());
                }
            }
            if (models.isEmpty() && root.contains("models") && root.value("models").isArray())
            {
                const QJsonArray arr = root.value("models").toArray();
                for (const auto &v : arr)
                {
                    if (!v.isObject()) continue;
                    const QJsonObject m = v.toObject();
                    push(m.value("model").toString());
                    push(m.value("name").toString());
                }
            }
            if (models.isEmpty())
            {
                push(root.value("id").toString());
                push(root.value("model").toString());
                push(root.value("name").toString());
            }
        }

        apiModelCandidates_ = models;
        applyModelCompleter();
        if (apiModelCompleter_ && api_model_LineEdit && api_model_LineEdit->hasFocus())
        {
            apiModelCompleter_->complete();
        }
    });
}

// 刷新模型行编辑的自动补全数据源
void Widget::applyModelCompleter()
{
    if (!api_model_LineEdit) return;
    if (!apiModelCompleter_)
    {
        apiModelCompleter_ = new QCompleter(this);
        apiModelCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
        apiModelCompleter_->setFilterMode(Qt::MatchContains);
        apiModelCompleter_->setModel(new QStringListModel(apiModelCompleter_));
    }
    auto *model = qobject_cast<QStringListModel *>(apiModelCompleter_->model());
    if (model) model->setStringList(apiModelCandidates_);
    api_model_LineEdit->setCompleter(apiModelCompleter_);
}
