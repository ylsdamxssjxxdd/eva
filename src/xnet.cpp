#include "xnet.h"

xNet::xNet() { qDebug() << "net init over"; }

xNet::~xNet() { ; }

void xNet::run() {
    emit net2ui_state("net:" + jtr("send message to api"));

    QElapsedTimer time;
    time.start();
    QElapsedTimer time2;

    bool is_first_token = true;  //接收到第一个token开始记录速度
    int tokens = 0;
    thinkFlag = false;//重置思考标签
    current_content = "";
    QEventLoop loop;  // 进入事件循环，等待回复
    QNetworkAccessManager manager;

    //对话模式
    if (!endpoint_data.is_complete_state) {
        // 设置请求的端点 URL
        QNetworkRequest request(QUrl(apis.api_endpoint.remove(apis.api_chat_endpoint) + apis.api_chat_endpoint));
        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", "Bearer " + apis.api_key.toUtf8());
        //构造请求的数据体
        QByteArray data = createChatBody();
        // 发送 POST 请求
        QNetworkReply *reply = manager.post(request, data);
        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            if (is_first_token) {
                is_first_token = false;
                time2.start();  //从接收到第一个token开始计时
            }
            QString jsonString = reply->readAll();
            // qDebug()<<"jsonString  "<<jsonString;
            // 由于原始字符串包含非JSON格式的前缀"data: "，我们需要去除这些部分
            QStringList dataList = jsonString.split("\n\n", QString::SkipEmptyParts);
            for (QString &data : dataList) {
                data.remove(0, data.indexOf("{"));  // 去除"data: "前缀
                // 把QString对象转换为QByteArray
                QByteArray jsonData = data.toUtf8();
                // 使用Qt的JSON类解析JSON数据
                QJsonDocument document = QJsonDocument::fromJson(jsonData);
                if (!document.isNull()) {
                    if (document.isObject()) {
                        QJsonObject jsonObject = document.object();
                        QJsonArray choices = jsonObject["choices"].toArray();
                        QJsonObject firstChoice = choices.at(0).toObject();
                        // qDebug()<<"choices "<<firstChoice;//看看接收到了什么
                        QJsonObject delta = firstChoice["delta"].toObject();
                        current_content = delta["content"].toString();
                        QString content_flag;
                        if (firstChoice.value("finish_reason").toString() == "stop") {
                            content_flag = jtr("<end>");
                        } else {
                            content_flag = current_content;
                        }
                        if (current_content != "")  //解析的结果发送到输出区
                        {
                            tokens++;
                            emit net2ui_state("net:" + jtr("recv reply") + " " + content_flag);
                            if(current_content.contains(DEFAULT_THINK_BEGIN)){thinkFlag = true;}// 检测到思考开始标志
                            if(thinkFlag){emit net2ui_output(current_content, true,THINK_GRAY);}
                            else{emit net2ui_output(current_content, true);}
                            if(current_content.contains(DEFAULT_THINK_END)){thinkFlag = false;}// 检测到思考结束标志
                        }
                    }
                } else if (data.contains("DONE")) {
                    emit net2ui_state("net: DONE");
                } else {
                    emit net2ui_state("net:resolve json fail", WRONG_SIGNAL);
                    qDebug() << jsonString;
                    qDebug() << data;
                }
            }
            if (is_stop || (!thinkFlag && current_content.contains(DEFAULT_OBSERVATION_STOPWORD))) {
                qDebug()<<current_content;
                is_stop = false;
                reply->abort();  //终止
            }
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() {
            if (reply->error() == QNetworkReply::NoError) {
                // 请求完成，所有数据都已正常接收
                if (endpoint_data.n_predict == 1) {
                    emit net2ui_state("net:" + jtr("use time") + " " + QString::number(time.nsecsElapsed() / 1000000000.0, 'f', 2) + " s ", SUCCESS_SIGNAL);
                } else {
                    emit net2ui_state("net:" + jtr("use time") + " " + QString::number(time.nsecsElapsed() / 1000000000.0, 'f', 2) + " s " + jtr("single decode") + " " + QString::number(tokens / (time2.nsecsElapsed() / 1000000000.0), 'f', 2) + " token/s", SUCCESS_SIGNAL);
                }
            } else {
                // 请求出错
                emit net2ui_state("net:" + reply->errorString(), WRONG_SIGNAL);
                // qDebug() << "Error:" << reply->errorString();
            }

            reply->abort();  //终止
            reply->deleteLater();
        });

        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    } else  //补完模式
    {
        // 设置请求的端点 URL
        QNetworkRequest request(QUrl(apis.api_endpoint + apis.api_completion_endpoint));
        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", "Bearer " + apis.api_key.toUtf8());
        //构造请求的数据体
        QByteArray data = createCompleteBody();
        // 发送 POST 请求
        QNetworkReply *reply = manager.post(request, data);
        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            if (is_first_token) {
                is_first_token = false;
                time2.start();
            }
            QString jsonString = reply->readAll();
            if (jsonString.contains("\n\ndata: ")) {
                jsonString = jsonString.split("\n\ndata: ")[1];
            }
            QString jsonData = jsonString.mid(jsonString.indexOf("{"));           // 首先去掉前面的非JSON部分
            QJsonDocument document = QJsonDocument::fromJson(jsonData.toUtf8());  // 使用QJsonDocument解析JSON数据
            QJsonObject rootObject = document.object();
            QString content = rootObject.value("content").toString();  // 得到content字段的值
            QString content_flag;
            if (rootObject.value("stop").toBool()) {
                content_flag = jtr("<end>");
            } else {
                content_flag = content;
            }
            tokens++;
            emit net2ui_state("net:" + jtr("recv reply") + " " + content_flag);
            emit net2ui_output(content, true);
            if (is_stop) {
                is_stop = false;
                reply->abort();  //终止
            }
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() {
            if (reply->error() == QNetworkReply::NoError) {
                // 请求完成，所有数据都已正常接收
                emit net2ui_state("net:" + jtr("use time") + " " + QString::number(time.nsecsElapsed() / 1000000000.0, 'f', 2) + " s " + jtr("single decode") + " " + QString::number(tokens / (time2.nsecsElapsed() / 1000000000.0), 'f', 2) + " token/s", SUCCESS_SIGNAL);
            } else {
                // 请求出错
                emit net2ui_state("net:" + reply->errorString(), WRONG_SIGNAL);
                // qDebug() << "Error:" << reply->errorString();
            }

            reply->abort();  //终止
            reply->deleteLater();
        });
        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    }

    // 进入事件循环
    loop.exec();
    emit net2ui_pushover();
}

//构造请求的数据体
QByteArray xNet::createChatBody() {
    // 创建 JSON 数据,添加一些采样参数
    QJsonObject json;
    if (apis.is_cache) {
        json.insert("cache_prompt", apis.is_cache);
    }  // 缓存上文
    json.insert("model", apis.api_model);
    json.insert("stream", true);
    json.insert("temperature", 2 * endpoint_data.temp);  // openai 的温度是从0-2，机体是从0-1所以乘2
    json.insert("max_tokens", endpoint_data.n_predict);
    QJsonArray stopkeys;  //强制结束的词
    for (int i = 0; i < endpoint_data.stopwords.size(); ++i) {
        stopkeys.append(endpoint_data.stopwords.at(i));
    }
    json.insert("stop", stopkeys);
    // 将 JSON 对象转换为字节序列
    json.insert("messages", endpoint_data.messagesArray);  //插入总消息
    // qDebug()<<endpoint_data.messagesArray;
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    return data;
}

//构造请求的数据体,补完模式
QByteArray xNet::createCompleteBody() {
    // 创建 JSON 数据
    QJsonObject json;
    if (apis.is_cache) {
        json.insert("cache_prompt", apis.is_cache);
    }  // 缓存上文
    json.insert("model", apis.api_model);
    json.insert("prompt", endpoint_data.input_prompt);
    json.insert("n_predict", endpoint_data.n_predict);
    json.insert("stream", true);
    json.insert("temperature", endpoint_data.temp);

    // 将 JSON 对象转换为字节序列
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    return data;
}
// 传递端点参数
void xNet::recv_data(ENDPOINT_DATA data) { endpoint_data = data; }
//传递api设置参数
void xNet::recv_apis(APIS apis_) { apis = apis_; }
void xNet::recv_stop(bool stop) { is_stop = stop; }

QString xNet::extractContentFromJson(const QString &jsonString) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) return QString();

    QJsonObject jsonObject = doc.object();
    QJsonArray choicesArray = jsonObject["choices"].toArray();
    if (choicesArray.isEmpty()) return QString();

    QJsonObject firstChoice = choicesArray.first().toObject();
    QJsonObject delta = firstChoice["delta"].toObject();
    return delta["content"].toString();
}
// 处理整个原始数据字符串，提取所有 content 字段
QStringList xNet::extractAllContent(const QString &data) {
    QStringList contentList;

    // 使用正则表达式匹配 JSON 对象
    QRegularExpression re("(\\{.*?\\})");
    QRegularExpressionMatchIterator i = re.globalMatch(data);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString json = match.captured(1);  // 捕获单个 JSON 对象字符串
        QString content = extractContentFromJson(json);
        if (!content.isEmpty()) {
            contentList << content;
        }
    }

    return contentList;
}

void xNet::recv_language(int language_flag_) { language_flag = language_flag_; }

// 根据language.json和language_flag中找到对应的文字
QString xNet::jtr(QString customstr) { return wordsObj[customstr].toArray()[language_flag].toString(); }