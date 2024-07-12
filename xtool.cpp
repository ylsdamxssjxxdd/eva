#include "xtool.h"

xTool::xTool(QString applicationDirPath_)
{
    applicationDirPath = applicationDirPath_;
    qDebug()<<"tool init over";

}
xTool::~xTool()
{
    ;
}

void xTool::run()
{
    //----------------------计算器------------------
    if(func_arg_list.first == "calculator")
    {
        emit tool2ui_state("tool:" + QString("calculator(") + func_arg_list.second + ")");
        QScriptEngine enging;
        QScriptValue result_ = enging.evaluate(func_arg_list.second.remove("\""));//手动去除公式中的引号
        QString result = QString::number(result_.toNumber());
        //qDebug()<<"tool:" + QString("calculator ") + jtr("return") + "\n" + result;
        if(result == "nan")//计算失败的情况
        {
            emit tool2ui_pushover(QString("calculator ") + jtr("return") + "计算失败，请确认计算公式是否合理");
        }
        else
        {
            emit tool2ui_pushover(QString("calculator ") + jtr("return") + "\n" + result);
        }
        emit tool2ui_state("tool:" + QString("calculator ") + jtr("return") + "\n" + result,TOOL_);
    }
    //----------------------命令提示符------------------
    else if(func_arg_list.first == "terminal")
    {

        QProcess *process = new QProcess();

        #ifdef Q_OS_WIN
        // 在Windows上执行
        
        process->start("cmd.exe", QStringList() << "/c" << func_arg_list.second);//使用start()方法来执行命令。Windows中的命令提示符是terminal.exe，参数/c指示命令提示符执行完毕后关闭，后面跟着的是实际要执行的命令。
        emit tool2ui_state(QString("tool: ") + "cmd.exe " + "/c " + func_arg_list.second);
        #else
        // 在Unix-like系统上执行
        process->start("/bin/sh", QStringList() << "-c" << func_arg_list.second);
        emit tool2ui_state(QString("tool: ") + "/bin/sh " + "/c " + func_arg_list.second);
        #endif

        if(!process->waitForFinished()) 
        {
            // 处理错误
            emit tool2ui_state("tool:" + QString("terminal ") + jtr("return") + "\n" + process->errorString(),TOOL_);
            emit tool2ui_pushover(QString("terminal ") + jtr("return") + "\n" + process->errorString());
            qDebug() << QString("terminal ") + jtr("return") + "\n" + process->errorString();
        } 
        else 
        {
            // 获取命令的输出
            QByteArray byteArray = process->readAll();
            QString output = QString::fromLocal8Bit(byteArray);
            emit tool2ui_state("tool:" +QString("terminal ") + jtr("return") + "\n" + output,TOOL_);
            emit tool2ui_pushover(QString("terminal ") + jtr("return") + "\n" + output);
            qDebug() << QString("terminal ") + jtr("return") + "\n" + output;
        }

    }
    //----------------------知识库------------------
    else if(func_arg_list.first == "knowledge")
    {
        QElapsedTimer time4;time4.start();
        QString result;
        if(Embedding_DB.size()==0)
        {
            result = jtr("Please tell user to embed knowledge into the knowledge base first");
            emit tool2ui_state("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_);
            emit tool2ui_pushover(QString("knowledge ") + jtr("return") + "\n" + result);
        }
        else
        {
            //查询计算词向量和计算相似度，返回匹配的文本段
            emit tool2ui_state("tool:" + jtr("qureying"));
            result = embedding_query_process(func_arg_list.second);
            emit tool2ui_state("tool:" + jtr("qurey&timeuse") + QString(": ") + QString::number(time4.nsecsElapsed()/1000000000.0,'f',2)+" s");
            emit tool2ui_state("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_);
            emit tool2ui_pushover(QString("knowledge ") + jtr("return") + "\n" + result);
        }
        
    }
    //----------------------控制台------------------
    else if(func_arg_list.first == "controller")
    {
        emit tool2ui_state("tool:" + QString("controller(") + func_arg_list.second + ")");
        //执行相应界面控制
        emit tool2ui_controller(func_arg_list.second.toInt());
    }
    //----------------------文生图------------------
    else if(func_arg_list.first == "stablediffusion")
    {
        //告诉expend开始绘制
        emit tool2expend_draw(func_arg_list.second);
    }
    //----------------------代码解释器------------------
    else if(func_arg_list.first == "interpreter")
    {
        QString result;
        //---内容写入interpreter.py---
        createTempDirectory(applicationDirPath + "/EVA_TEMP");
        QFile file(applicationDirPath + "/EVA_TEMP/interpreter.py");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) 
        {
            QTextStream out(&file);
            out.setCodec("UTF-8"); // 设置编码为UTF-8
            out << func_arg_list.second;
            file.close();
            //---运行interpreter.py---
            QProcess *process = new QProcess();
            // 构建Python命令
#ifdef Q_OS_WIN
// 在Windows上执行
            QString command = "python";
#else
// 在linux上执行 默认用python3
            QString command = "python3";
#endif
            QStringList args;
            args << applicationDirPath + "/EVA_TEMP/interpreter.py";

            // 连接信号以获取输出
            connect(process, &QProcess::readyReadStandardOutput, [&]() {
                QByteArray rawOutput = process->readAllStandardOutput();
                QTextCodec *codec = QTextCodec::codecForName("GBK");  // 使用GBK编码正确解析windows下内容
                result += codec->toUnicode(rawOutput);
            });
            connect(process, &QProcess::readyReadStandardError, [&]() {
                QByteArray rawOutput = process->readAllStandardError();
                QTextCodec *codec = QTextCodec::codecForName("GBK");  // 使用GBK编码正确解析windows下内容
                result += codec->toUnicode(rawOutput);
                
            });

            emit tool2ui_state("tool: " + command + " " + args.at(0));
            // 开始运行脚本
            process->start(command, args);

            // 等待脚本结束（可选）
            process->waitForFinished();
            qDebug() << result;
        } 
        else 
        {
            result += "Failed to open file for writing";
            qDebug() << "Failed to open file for writing";
        }

        emit tool2ui_state("tool:" +QString("interpreter ") + jtr("return") + "\n" + result,TOOL_);
        emit tool2ui_pushover(QString("interpreter ") + jtr("return") + "\n" + result);
    }
    //----------------------没有该工具------------------
    else
    {
        emit tool2ui_pushover(jtr("not load tool"));
    }
    
}

//创建临时文件夹EVA_TEMP
bool xTool::createTempDirectory(const QString &path) {
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path)) {
        return false;
    } else {
        // 尝试创建目录
        if (dir.mkpath(path)) {
            return true;
        } else {
            return false;
        }
    }
}

void xTool::recv_func_arg(QPair<QString, QString> func_arg_list_)
{
    func_arg_list = func_arg_list_;
}

//查询计算词向量和计算相似度，返回匹配的文本段
QString xTool::embedding_query_process(QString query_str)
{
    
    QString knowledge_result;
    //---------------计算查询文本段的词向量-------------------------
    ipAddress = getFirstNonLoopbackIPv4Address();
    QEventLoop loop;// 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(embedding_server_api+""));//加一个""是为了避免语法解析错误
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key ="Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    //构造请求的数据体
    QJsonObject json;
    json.insert("model", "gpt-3.5-turbo");
    json.insert("encoding_format", "float");
    json.insert("input", query_str);
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    
    // POST 请求
    QNetworkReply *reply = manager.post(request, data);

    // 处理响应
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() 
    {
        QString jsonString = reply->readAll();
        QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());// 使用QJsonDocument解析JSON数据
        QJsonObject rootObject = document.object();
        // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
        QJsonArray dataArray = rootObject["data"].toArray();
        QString vector_str = "[";
        
        for(int i = 0; i < dataArray.size(); ++i)
        {
            QJsonObject dataObj = dataArray[i].toObject();
            
            // 检查"data"对象中是否存在"embedding"
            if(dataObj.contains("embedding"))
            {
                QJsonArray embeddingArray = dataObj["embedding"].toArray();
                // 处理"embedding"数组
                for(int j = 0; j < embeddingArray.size(); ++j)
                {
                    query_embedding_vector.value[j] = embeddingArray[j].toDouble();
                    vector_str += QString::number(query_embedding_vector.value[j],'f',4)+", ";
                }
            }
        }
        vector_str += "]";
        emit tool2ui_state("tool:" + jtr("The query text segment has been embedded") + jtr("dimension") + ": "+QString::number(query_embedding_vector.value.size()) + " " + jtr("word vector") + ": "+ vector_str,USUAL_);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() 
    {
        if (reply->error() == QNetworkReply::NoError) 
        {
            // 请求完成，所有数据都已正常接收
            //------------------------计算余弦相似度---------------------------
            //A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(query_embedding_vector.value,Embedding_DB);//计算查询文本段和所有嵌入文本段之间的相似度

            if(score.size()>0){knowledge_result += jtr("The three text segments with the highest similarity") + DEFAULT_SPLITER;}

            //将分数前三的结果显示出来
            for(int i = 0;i < 3 && i < score.size();++i)
            {
                knowledge_result += QString::number(score[i].first + 1) + jtr("Number text segment similarity") + ": " + QString::number(score[i].second);
                knowledge_result += " " + jtr("content") + DEFAULT_SPLITER + Embedding_DB.at(score[i].first).chunk + "\n";
            }

            if(score.size()>0){knowledge_result += jtr("Based on this information, reply to the user's previous questions");}

        } 
        else 
        {
            // 请求出错
            emit tool2ui_state("tool:" + jtr("Request error") + " " + reply->error(), WRONG_);
            knowledge_result += jtr("Request error") + " " + reply->error();
        }
        
        reply->abort();//终止
        reply->deleteLater();
    });

    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    return knowledge_result;
}

//获取ipv4地址
QString xTool::getFirstNonLoopbackIPv4Address()
{
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++) {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            return list[i].toString();
        }
    }
    return QString();
}

// 计算两个向量的余弦相似度，A向量点积B向量除以(A模乘B模)
double xTool::cosine_similarity_1024(const std::array<double, 1024>& a, const std::array<double, 1024>& b)
{
    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (int i = 0; i < 1024; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引
std::vector<std::pair<int, double>> xTool::similar_indices(const std::array<double, 1024>& user_vector, const QVector<Embedding_vector>& embedding_DB)
{
    std::vector<std::pair<int, double>> scores; // 存储每个索引和其相应的相似度得分

    // 计算相似度得分
    for (const auto& emb : embedding_DB) {
        double sim = cosine_similarity_1024(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }

    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) 
    {
        return a.second > b.second;
    });

    return scores;
}

void xTool::recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_)
{
    Embedding_DB.clear();
    Embedding_DB = Embedding_DB_;
    emit tool2ui_state("tool:" +  jtr("Received embedded text segment data"),USUAL_);
}

//传递嵌入服务端点
void xTool::recv_serverapi(QString serverapi)
{
    embedding_server_api = serverapi;
}

//接收图像绘制完成信号
void xTool::recv_drawover(QString result_, bool ok_)
{
    //绘制失败的情况
    if(!ok_)
    {
        emit tool2ui_pushover(result_);
        return;
    }

    //绘制成功的情况
    //添加绘制成功并显示图像指令
    emit tool2ui_state("tool:" + QString("stablediffusion ") + jtr("return") + "\n" + "<ylsdamxssjxxdd:showdraw>" + result_,TOOL_);
    emit tool2ui_pushover("<ylsdamxssjxxdd:showdraw>" + result_);
}

//传递控制完成结果
void xTool::tool2ui_controller_over(QString result)
{
    emit tool2ui_state("tool:" + QString("controller ") + jtr("return") + "\n" + result, TOOL_);
    emit tool2ui_pushover(QString("controller ") + jtr("return") + "\n" + result);
}

void xTool::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

// 根据language.json和language_flag中找到对应的文字
QString xTool::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}