#include "xnet.h"

xNet::xNet()
{
    ;
}
xNet::~xNet()
{
    ;
}
void xNet::run()
{
    emit net2ui_state("net:" + wordsObj["send message to api"].toString());
    
    QElapsedTimer time;time.start();
    QElapsedTimer time2;
    bool is_first_token = true;//接收到第一个token开始记录速度
    int tokens = 0;
    
    QEventLoop loop;// 进入事件循环，等待回复
    QNetworkAccessManager manager;

    //对话模式
    if(!endpoint_data.complete_mode)
    {
        // 设置请求的端点 URL
        QNetworkRequest request(QUrl("http://" + apis.api_ip + ":" + apis.api_port + apis.api_chat_endpoint));
        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QString api_key ="Bearer " + QString("sjxx");
        request.setRawHeader("Authorization", api_key.toUtf8());
        //构造请求的数据体
        QByteArray data = createChatBody();
        
        // 发送 POST 请求
        QNetworkReply *reply = manager.post(request, data);
        
        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() 
        {
            
            if(is_first_token){is_first_token = false;time2.start();}
            QString jsonString = reply->readAll();
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
                        QJsonObject delta = firstChoice["delta"].toObject();
                        QString content = delta["content"].toString();
                        QString content_flag;
                        if(content == "\n" || content == "\r"){content_flag = wordsObj["<enter>"].toString();}
                        else if(content == " "){content_flag = wordsObj["<space>"].toString();}
                        else if(firstChoice.value("finish_reason").toString() == "stop"){content_flag = wordsObj["<end>"].toString();}
                        else{content_flag=content;}
                        if(content!="")
                        {
                            tokens++;
                            emit net2ui_state("net:" + wordsObj["recv output"].toString() + " " + content_flag);
                            emit net2ui_output(content,1);
                        }
                        
                    }
                } else {
                    emit net2ui_state("net:resolve json fail",WRONG_);
                    qDebug() << jsonString;
                    qDebug() << dataList;
                }
            }

            if(is_stop)
            {
                is_stop =false;
                reply->abort();//终止
            }
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() 
        {
            if (reply->error() == QNetworkReply::NoError) 
            {
                // 请求完成，所有数据都已正常接收
                if(endpoint_data.n_predict == 1){emit net2ui_state("net:" + wordsObj["use time"].toString() + " " + QString::number(time.nsecsElapsed()/1000000000.0,'f',2) + " s ",SUCCESS_);}
            else{emit net2ui_state("net:" + wordsObj["use time"].toString() + " " + QString::number(time.nsecsElapsed()/1000000000.0,'f',2) + " s "+ wordsObj["singl decode"].toString() + " " + QString::number(tokens / (time2.nsecsElapsed()/1000000000.0),'f',2) + " token/s",SUCCESS_);}
            } 
            else 
            {
                // 请求出错
                emit net2ui_state("net:" + reply->errorString(),WRONG_);
                //qDebug() << "Error:" << reply->errorString();
            }
            
            reply->abort();//终止
            reply->deleteLater();
            emit net2ui_pushover();
        });

        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    }
    else//补完模式
    {
        // 设置请求的端点 URL
        QNetworkRequest request(QUrl("http://" + apis.api_ip + ":" + apis.api_port + apis.api_complete_endpoint));
        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", "Bearer no-key");
        //构造请求的数据体
        QByteArray data = createCompleteBody();
        // 发送 POST 请求
        QNetworkReply *reply = manager.post(request, data);
        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() 
        {
            if(is_first_token){is_first_token = false;time2.start();}
            QString jsonString = reply->readAll();
            if(jsonString.contains("\n\ndata: ")){jsonString = jsonString.split("\n\ndata: ")[1];}
            QString jsonData = jsonString.mid(jsonString.indexOf("{"));// 首先去掉前面的非JSON部分
            QJsonDocument document = QJsonDocument::fromJson(jsonData.toUtf8());// 使用QJsonDocument解析JSON数据
            QJsonObject rootObject = document.object();
            QString content = rootObject.value("content").toString();// 得到content字段的值
            QString content_flag;
            if(content == "\n" || content == "\r"){content_flag = wordsObj["<enter>"].toString();}
            else if(content == " "){content_flag = wordsObj["<space>"].toString();}
            else if(rootObject.value("stop").toBool()){content_flag = wordsObj["<end>"].toString();}
            else{content_flag=content;}
            tokens++;
            emit net2ui_state("net:" + wordsObj["recv output"].toString() + " " + content_flag);
            emit net2ui_output(content,1);
            if(is_stop)
            {
                is_stop =false;
                reply->abort();//终止
            }

        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() 
        {
            if (reply->error() == QNetworkReply::NoError) 
            {
                // 请求完成，所有数据都已正常接收
                emit net2ui_state("net:" + wordsObj["use time"].toString() + " " + QString::number(time.nsecsElapsed()/1000000000.0,'f',2) + " s "+ wordsObj["singl decode"].toString() + " " + QString::number(tokens / (time2.nsecsElapsed()/1000000000.0),'f',2) + " token/s",SUCCESS_);
            } 
            else 
            {
                // 请求出错
                emit net2ui_state("net:" + reply->errorString(),WRONG_);
                //qDebug() << "Error:" << reply->errorString();
            }
            
            reply->abort();//终止
            reply->deleteLater();
            emit net2ui_pushover();

        });
        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    }

    // 进入事件循环，等待回复
    loop.exec();
}

//构造请求的数据体
QByteArray xNet::createChatBody()
{
    // 创建 JSON 数据,添加一些采样参数
    QJsonObject json;
    json.insert("model", "gpt-3.5-turbo");
    json.insert("stream", true);
    json.insert("temperature", 2*endpoint_data.temp);
    json.insert("max_tokens", endpoint_data.n_predict);
    QJsonArray stopkeys;stopkeys.append("<|im_end|>");//强制结束的词
    json.insert("stop", stopkeys);

    QJsonArray messagesArray;//总消息
    //构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", "system");
    systemMessage.insert("content", endpoint_data.date_prompt);
    messagesArray.append(systemMessage);//添加消息
    //构造历史和当前的用户输入，一条用户的一条模型的这样构造，直到历史记录没有
    //qDebug()<<"--------------------------------";
    while(endpoint_data.user_history.size()>0)
    {
        if(endpoint_data.user_history.size()>0)
        {
            QJsonObject userMessage;
            userMessage.insert("role", "user");
            userMessage.insert("content", endpoint_data.user_history.at(0));
            //qDebug() << "user" << endpoint_data.user_history.at(0);
            messagesArray.append(userMessage);
            endpoint_data.user_history.removeFirst();
            
        }
        if(endpoint_data.assistant_history.size()>0)
        {
            QJsonObject assistantMessage;
            assistantMessage.insert("role", "assistant");
            assistantMessage.insert("content", endpoint_data.assistant_history.at(0));
            //qDebug() << "assistant" << endpoint_data.assistant_history.at(0);
            messagesArray.append(assistantMessage);
            endpoint_data.assistant_history.removeFirst();
        }
    }
    
    // 将 JSON 对象转换为字节序列
    json.insert("messages", messagesArray);//插入总消息
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    return data;
}

//构造请求的数据体,补完模式
QByteArray xNet::createCompleteBody()
{
    // 创建 JSON 数据
    QJsonObject json;
    if(apis.is_cache){json.insert("cache_prompt", apis.is_cache);}
    json.insert("seed", 2023);
    json.insert("ignore_eos", false);//是否无视结束标志
    json.insert("prompt", endpoint_data.input_prompt);
    json.insert("n_predict", endpoint_data.n_predict);
    json.insert("stream", true);
    json.insert("temperature", endpoint_data.temp);
    // 修改系统指令,好像没什么意义
    // QJsonObject systemPrompt;
    // systemPrompt.insert("prompt", endpoint_data.date_prompt);
    // systemPrompt.insert("anti_prompt", endpoint_data.input_pfx);
    // systemPrompt.insert("assistant_name", endpoint_data.input_sfx);
    // json.insert("system_prompt", systemPrompt);

    // 将 JSON 对象转换为字节序列
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    return data;
}
// 传递端点参数
void xNet::recv_data(ENDPOINT_DATA data)
{
    endpoint_data = data;
}
//传递api设置参数
void xNet::recv_apis(APIS apis_)
{
    apis = apis_;
}
void xNet::recv_stop(bool stop)
{
    is_stop = stop;
}

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
        QString json = match.captured(1); // 捕获单个 JSON 对象字符串
        QString content = extractContentFromJson(json);
        if (!content.isEmpty()) {
            contentList << content;
        }
    }

    return contentList;
}