#include "xtool.h"

xTool::xTool()
{
    positron_p = new QTimer(this);//持续检测延迟
    connect(positron_p, SIGNAL(timeout()), this, SLOT(positronPower()));
    connect(this,&xTool::positron_starter,this,&xTool::positronPower);
}
xTool::~xTool()
{
    ;
}

void xTool::run()
{
    if(func_arg_list.front() == "calculator")
    {
        emit tool2ui_state("tool:" + QString("calculator(") + func_arg_list.last() + ")");
        QScriptEngine enging;
        QScriptValue result_ = enging.evaluate(func_arg_list.last());
        QString result = QString::number(result_.toNumber());
        //qDebug()<<"tool:" + QString("calculator ") + wordsObj["return"].toString() + " " + result;
        emit tool2ui_state("tool:" + QString("calculator ") + wordsObj["return"].toString() + " " + result,TOOL_);
        emit tool2ui_pushover(QString("calculator ") + wordsObj["return"].toString() + " " + result);
    }
    else if(func_arg_list.front() == "cmd")
    {

        QProcess *process = new QProcess();

        #ifdef Q_OS_WIN
        // 在Windows上执行
        
        process->start("cmd.exe", QStringList() << "/c" << func_arg_list.last());//使用start()方法来执行命令。Windows中的命令提示符是cmd.exe，参数/c指示命令提示符执行完毕后关闭，后面跟着的是实际要执行的命令。
        #else
        // 在Unix-like系统上执行
        process->start("/bin/sh", QStringList() << "-c" << commandString);
        #endif

        if(!process->waitForFinished()) 
        {
            // 处理错误
            emit tool2ui_state("tool:" +QString("cmd ") + wordsObj["return"].toString() + " " + process->errorString(),TOOL_);
            emit tool2ui_pushover(QString("cmd ") + wordsObj["return"].toString() + " " + process->errorString());
            qDebug() << QString("cmd ") + wordsObj["return"].toString() + " " + process->errorString();
        } 
        else 
        {
            // 获取命令的输出
            QByteArray byteArray = process->readAll();
            QString output = QString::fromLocal8Bit(byteArray);
            emit tool2ui_state("tool:" +QString("cmd ") + wordsObj["return"].toString() + " " + output,TOOL_);
            emit tool2ui_pushover(QString("cmd ") + wordsObj["return"].toString() + " " + output);
            qDebug() << QString("cmd ") + wordsObj["return"].toString() + " " + output;
        }

    }
    else if(func_arg_list.front() == "search")
    {
        
        emit tool2ui_pushover(wordsObj["not set tool"].toString());
    }
    else if(func_arg_list.front() == "knowledge")
    {
        QElapsedTimer time4;time4.start();
        QString result;
        if(Embedding_DB.size()==0)
        {
            result = "请用户先嵌入知识到知识库中";
            emit tool2ui_state("tool:" + QString("knowledge ") + wordsObj["return"].toString() + " " + result, TOOL_);
            emit tool2ui_pushover(QString("knowledge ") + wordsObj["return"].toString() + " " + result);
        }
        else
        {
            //查询计算词向量和计算相似度，返回匹配的文本段
            result = embedding_query_process(func_arg_list.last());
            emit tool2ui_state("tool:" + wordsObj["qurey&timeuse"].toString() + QString(": ") + QString::number(time4.nsecsElapsed()/1000000000.0,'f',2)+" s");
            emit tool2ui_state("tool:" + QString("knowledge ") + wordsObj["return"].toString() + " " + result, TOOL_);
            emit tool2ui_pushover(QString("knowledge ") + wordsObj["return"].toString() + " " + result);
        }
        
    }
    else if(func_arg_list.front() == "positron")
    {
        emit tool2ui_state("tool:" + QString("positron(") + func_arg_list.last() + ")");

        //阳电子步枪开始充能
        emit positron_starter();
        //positron_p->start(1000);
    }
    else if(func_arg_list.front() == "stablediffusion")
    {
        //告诉expend开始绘制
        emit tool2expend_draw(func_arg_list.last());

        
    }
    else
    {
        //没有该工具
        emit tool2ui_pushover(wordsObj["not load tool"].toString());
    }
    
}
//阳电子步枪发射
void xTool::positronShoot()
{
    QString result;
    qsrand(QTime::currentTime().msec());// 设置随机数种子
    int randomValue = (qrand() % 3);//0-2随机数
    if(randomValue==0){result = wordsObj["positron_result1"].toString();}
    else if(randomValue==1){result = wordsObj["positron_result2"].toString();}
    else if(randomValue==2)//使徒逃窜
    {
        int randomValue2 = (qrand() % 360);//0-359随机数
        result = wordsObj["positron_result3"].toString() + " " + QString::number(randomValue2) + wordsObj["degree"].toString();
    }
    //qDebug()<<"tool:" + QString("positron ") + wordsObj["return"].toString() + " " + result;
    emit tool2ui_state("tool:" + QString("positron ") + wordsObj["return"].toString() + " " + result,TOOL_);
    emit tool2ui_pushover(QString("positron ") + wordsObj["return"].toString() + " " + result);
}

void xTool::recv_func_arg(QStringList func_arg_list_)
{
    func_arg_list = func_arg_list_;
}

//查询计算词向量和计算相似度，返回匹配的文本段
QString xTool::embedding_query_process(QString query_str)
{
    QString knowledge_result;
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
        emit tool2ui_state("tool:" + QString("查询文本段嵌入完毕! ") + "维度:"+QString::number(query_embedding_vector.value.size()) + " " + "词向量: "+ vector_str,USUAL_);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() 
    {
        if (reply->error() == QNetworkReply::NoError) 
        {
            // 请求完成，所有数据都已正常接收
            //计算余弦相似度
            //A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(query_embedding_vector.value,Embedding_DB);

            if(score.size()>0){knowledge_result += "相似度最高的三个文本段:\n";}

            //将分数前三的结果显示出来
            for(int i = 0;i < 3 && i < score.size();++i)
            {
                knowledge_result += QString::number(score[i].first + 1) + "号文本段 相似度: " + QString::number(score[i].second);
                knowledge_result += " 内容:\n" + Embedding_DB.at(score[i].first).chunk + "\n";
            }

            if(score.size()>0){knowledge_result += "以这些信息为基础，回复用户之前的问题，不再输出json";}

        } 
        else 
        {
            // 请求出错
            emit tool2ui_state("tool:请求出错 " + reply->error(), WRONG_);
            knowledge_result += "请求出错 " + reply->error();
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

// 阳电子步枪充能,数到5就发射
void xTool::positronPower()
{
    if(positron_power<5)
    {
        positron_power ++;
        QString power_bar;
        for(int i = 0;i<positron_power;++i)
        {
            power_bar += wordsObj["cube"].toString();
        }
        emit tool2ui_state("tool:" + wordsObj["positron_powering"].toString() + power_bar,USUAL_);
        //阳电子步枪继续充能
        positron_p->start(1000);
    }
    else
    {
        positron_power = 0;
        positron_p->stop();
        positronShoot();
    }
}


// 计算两个向量的余弦相似度
double xTool::cosine_similarity(const std::array<double, 1024>& a, const std::array<double, 1024>& b)
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
        double sim = cosine_similarity(user_vector, emb.value);
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
    emit tool2ui_state("tool:接收到已嵌入文本段数据",USUAL_);
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
    emit tool2ui_state("tool:" + QString("stablediffusion ") + wordsObj["return"].toString() + " " + "<ylsdamxssjxxdd:showdraw>" + result_,TOOL_);
    emit tool2ui_pushover("<ylsdamxssjxxdd:showdraw>" + result_);
}