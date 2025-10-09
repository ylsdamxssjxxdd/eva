#include "expend.h"

#include "ui_expend.h"
#include "../utils/pathutil.h"
#include "../utils/devicemanager.h"

//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择嵌入模型路径时响应
void Expend::on_embedding_txt_modelpath_button_clicked()
{
    // 停止已有嵌入服务，避免重复启动导致残留
    stopEmbeddingServer(true);
    Embedding_DB.clear();                                                                        // 清空向量数据库
    ui->embedding_txt_over->clear();                                                             //清空已嵌入文本段表格内容
    ui->embedding_txt_over->setRowCount(0);                                                      //设置已嵌入文本段表格为0行
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")}); //设置列名
    // 清空表格
    currentpath = customOpenfile(currentpath, jtr("select embedding model"), "(*.bin *.gguf)");
    embedding_params.modelpath = currentpath;
    if (embedding_params.modelpath == "")
    {
        ui->embedding_model_lineedit->setText(""); //清空启动的服务
        return;
    }

    // 100 ms后尝试启动服务, 因为要等待server_process->kill()
    QTimer::singleShot(100, this, &Expend::embedding_server_start);
}

// 尝试启动server
void Expend::embedding_server_start()
{
    // Resolve llama.cpp server from current backend
    QString program = DeviceManager::programPath(QStringLiteral("llama-server"));
    if (program.isEmpty() || !QFileInfo::exists(program)) { ui->embedding_test_log->appendPlainText("[error] llama-server not found under current device folder"); return; }

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << ensureToolFriendlyFilePath(embedding_params.modelpath);
    arguments << "--host"
              << "0.0.0.0";                                        //暴露本机ip
    arguments << "--port" << DEFAULT_EMBEDDING_PORT;               //服务端口
    arguments << "--threads" << QString::number(max_thread * 0.5); //使用线程
    arguments << "-cb";                                            //允许连续批处理
    arguments << "--embedding";                                    //允许词嵌入
    // arguments << "--log-disable";                                   //不要日志
    if (!DEFAULT_USE_MMAP) { arguments << "--no-mmap"; }
    // 开始运行程序
    // Add tool dir to library search path and set working directory
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(program).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif
    server_process->setProcessEnvironment(env);
    server_process->setWorkingDirectory(toolDir);
    server_process->start(program, arguments);
    // 记录 PID（可能需要在 started 信号后再次刷新）
    embedding_server_pid = server_process->processId();
}

// 获取server_process日志输出
void Expend::readyRead_server_process_StandardOutput()
{
    QString server_output = server_process->readAllStandardOutput();
    // qDebug()<<"std "<<server_output;
}

// 获取server_process日志输出
void Expend::readyRead_server_process_StandardError()
{
    QString server_output = server_process->readAllStandardError();
    // qDebug()<<"error "<<server_output;
    //启动成功的标志
    QString log_output;
    if (server_output.contains(SERVER_START))
    {
        keep_embedding_server = true;
        ui->embedding_model_lineedit->setText(embedding_params.modelpath);
        keep_embedding_server = false;

        ui->embedding_txt_modelpath_button->setText(jtr("abort server"));

        qDebug() << "嵌入服务启动成功" << embedding_server_api;
        log_output += jtr("embedding") + jtr("service startup completed") + "\n";
        log_output += jtr("embd api") + ": " + embedding_server_api;
        log_output += "\n" + jtr("embd dim") + ": " + QString::number(embedding_server_dim);
    }
    if (log_output != "")
    {
        ui->embedding_test_log->appendPlainText(log_output);
    }

    if (server_output.contains(SERVER_EMBD_INFO))
    {
        embedding_server_dim = server_output.split(SERVER_EMBD_INFO).at(1).split("\n").at(0).toInt();
        ui->embedding_dim_spinBox->setValue(embedding_server_dim);
        qDebug() << "该模型的嵌入维度为: " << embedding_server_dim << ui->embedding_dim_spinBox->value();
        if (embedding_embed_need) //用来自动构建知识库
        {
            embedding_embed_need = false;
            QTimer::singleShot(1000, this, SLOT(embedding_processing())); //1s后再执行构建以免冲突
        }
    }
}

//进程开始响应
void Expend::server_onProcessStarted() { embedding_server_pid = server_process->processId(); }

//进程结束响应
void Expend::server_onProcessFinished()
{
    ui->embedding_test_log->appendPlainText(jtr("embedding server abort"));
    ui->embedding_txt_modelpath_button->setText("...");
    qDebug() << "嵌入服务终止";
    embedding_server_pid = -1;
}

//用户点击上传路径时响应
void Expend::on_embedding_txt_upload_clicked()
{
    currentpath = customOpenfile(currentpath, jtr("choose a txt to embed"), "(*.txt)");
    txtpath = currentpath;
    ui->embedding_txt_lineEdit->setText(txtpath);

    preprocessTXT(); //预处理文件内容
}

// 分词函数
QStringList Expend::tokenizeContent(const QString &content)
{
    QStringList tokens;
    // 正则表达式匹配中文、英文单词、Emoji及其他字符
    QRegularExpression re(
        "(\\p{Han}+)"                // 中文字符
        "|([A-Za-z]+)"               // 英文单词
        "|([\\x{1F300}-\\x{1F5FF}])" // Emoji 范围1
        "|([\\x{1F600}-\\x{1F64F}])" // Emoji 范围2
        "|([\\x{1F680}-\\x{1F6FF}])" // Emoji 范围3
        "|([\\x{2600}-\\x{26FF}])"   // 其他符号
        "|(\\s+)"                    // 空白字符
        "|(.)",                      // 其他任意单字符
        QRegularExpression::UseUnicodePropertiesOption);

    QRegularExpressionMatchIterator it = re.globalMatch(content);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        if (match.hasMatch())
        {
            QString token;
            if (match.captured(1).length() > 0)
                token = match.captured(1); // 中文
            else if (match.captured(2).length() > 0)
                token = match.captured(2); // 英文单词
            else if (match.captured(3).length() > 0 ||
                     match.captured(4).length() > 0 ||
                     match.captured(5).length() > 0 ||
                     match.captured(6).length() > 0)
                token = match.captured(); // Emoji
            else if (match.captured(7).length() > 0)
                token = match.captured(7); // 空白
            else
                token = match.captured(8); // 其他字符
            tokens << token;
        }
    }
    return tokens;
}

//预处理文件内容
void Expend::preprocessTXT()
{

    // 读取文件内容
    QString content;
    QFile file(txtpath);
    // 打开文件
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file); // 创建 QTextStream 对象
        in.setCodec("UTF-8");
        content = in.readAll(); // 读取文件内容
    }
    file.close();

    // -------------------分词-----------------
    QStringList tokens = tokenizeContent(content);

    // -------------------分段-----------------
    QStringList paragraphs;
    int splitLength = ui->embedding_split_spinbox->value();
    int overlap = ui->embedding_overlap_spinbox->value();
    int splitNums = 0;
    int startTokenIndex = 0;

    while (startTokenIndex < tokens.size())
    {
        int currentLength = 0;
        int endTokenIndex = startTokenIndex;

        // 构建当前段落，确保不超过splitLength
        while (endTokenIndex < tokens.size())
        {
            int tokenLength = tokens[endTokenIndex].length();
            if (currentLength + tokenLength > splitLength)
            {
                break;
            }
            currentLength += tokenLength;
            endTokenIndex++;
        }

        // 提取当前段落的tokens
        QString paragraph;
        for (int i = startTokenIndex; i < endTokenIndex; ++i)
        {
            paragraph += tokens[i];
        }
        paragraphs << paragraph;
        splitNums++;

        // 计算下一段的起始index，考虑重叠部分
        if (endTokenIndex < tokens.size())
        {
            int overlapLength = 0;
            int overlapTokens = 0;
            // 从当前结束位置回退，直到覆盖至少'overlap'个字符
            while (endTokenIndex - overlapTokens > startTokenIndex && overlapLength < overlap)
            {
                overlapLength += tokens[endTokenIndex - 1 - overlapTokens].length();
                overlapTokens++;
            }
            startTokenIndex = endTokenIndex - overlapTokens;
        }
        else
        {
            break;
        }
    }

    // 打印总分段数
    qDebug() << "分段数:" << splitNums;

    //显示在待嵌入表格中
    ui->embedding_txt_wait->clear();
    ui->embedding_txt_wait->setRowCount(splitNums); //创建splitNums很多行

    for (int i = 0; i < paragraphs.size(); ++i)
    {
        QTableWidgetItem *newItem = new QTableWidgetItem(paragraphs.at(i));
        ui->embedding_txt_wait->setItem(i, 0, newItem);
    }
    ui->embedding_txt_wait->setColumnWidth(0, qMax(ui->embedding_txt_wait->width(), 400)); // 列宽保持控件宽度

    ui->embedding_txt_wait->resizeRowsToContents();                                                // 自动调整行高
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")}); //设置列名
}

//右击表格显示菜单
void Expend::show_embedding_txt_wait_menu(const QPoint &pos)
{
    // 创建菜单并添加动作
    QMenu contextMenu(tr("Context menu"), this);

    QAction actionAdd(jtr("add"), this);
    connect(&actionAdd, &QAction::triggered, this, &Expend::embedding_txt_wait_onAdd);
    contextMenu.addAction(&actionAdd);

    QAction actionDelete(jtr("delete"), this);
    connect(&actionDelete, &QAction::triggered, this, &Expend::embedding_txt_wait_onDelete);
    contextMenu.addAction(&actionDelete);

    // 显示菜单
    contextMenu.exec(ui->embedding_txt_wait->viewport()->mapToGlobal(pos));
}

//添加表格
void Expend::embedding_txt_wait_onAdd()
{
    // 获取选中的行
    int row = ui->embedding_txt_wait->currentRow() + 1;
    ui->embedding_txt_wait->insertRow(row); // 在选中的行的下一行添加一行

    // 根据需要设置新行的内容
    QTableWidgetItem *newItem = new QTableWidgetItem(jtr("please input the text that needs to be embedded"));
    ui->embedding_txt_wait->setItem(row, 0, newItem); // 假设我们只设置第一列
}
//删除表格
void Expend::embedding_txt_wait_onDelete()
{
    // 获取选中的行号
    QList<QTableWidgetItem *> selectedItems = ui->embedding_txt_wait->selectedItems();
    QSet<int> rows;
    for (auto item : selectedItems)
    {
        rows.insert(item->row()); // 只添加行号
    }

    // 转换为列表并排序（从大到小）
    QList<int> sortedRows = QList<int>(rows.begin(), rows.end());
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    // 删除行
    for (int row : sortedRows)
    {
        ui->embedding_txt_wait->removeRow(row);
    }
}

//---------------构建知识库------------------
//用户点击嵌入时响应
void Expend::on_embedding_txt_embedding_clicked()
{
    embedding_processing();
}

//用户点击检索时响应
void Expend::on_embedding_test_pushButton_clicked()
{
    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);           //上传按钮
    ui->embedding_txt_embedding->setEnabled(0);        //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);      //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0); //选择模型按钮

    QEventLoop loop; // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(embedding_server_api + QString("")));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    //构造请求的数据体
    QJsonObject json;
    json.insert("model", "default");
    json.insert("encoding_format", "float");
    json.insert("input", ui->embedding_test_textEdit->toPlainText());
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    // POST 请求
    QNetworkReply *reply = manager.post(request, data);

    // 处理响应
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        QString jsonString = reply->readAll();
        QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8()); // 使用QJsonDocument解析JSON数据
        QJsonObject rootObject = document.object();
        // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
        QJsonArray dataArray = rootObject["data"].toArray();
        QString vector_str = "[";

        for (int i = 0; i < dataArray.size(); ++i)
        {
            QJsonObject dataObj = dataArray[i].toObject();

            // 检查"data"对象中是否存在"embedding"
            if (dataObj.contains("embedding"))
            {
                QJsonArray embeddingArray = dataObj["embedding"].toArray();
                user_embedding_vector.value.resize(ui->embedding_dim_spinBox->value()); // 分配空间

                // 处理"embedding"数组
                int arraySize = embeddingArray.size();
                if (arraySize != ui->embedding_dim_spinBox->value()) { ui->embedding_test_log->appendPlainText(QString::number(arraySize) + " query embedding dim not match! Fill with 0"); }
                for (int j = 0; j < ui->embedding_dim_spinBox->value(); ++j)
                {
                    if (j < arraySize) { user_embedding_vector.value[j] = embeddingArray[j].toDouble(); }
                    else
                    {
                        user_embedding_vector.value[j] = 0.0;
                    } //返回的向量不足的维度用0填充
                    vector_str += QString::number(user_embedding_vector.value[j], 'f', 4) + ", ";
                }
            }
        }
        vector_str += "]";
        ui->embedding_test_log->appendPlainText(jtr("The query text segment has been embedded") + "! " + jtr("dimension") + ": " + QString::number(user_embedding_vector.value.size()) + " " + jtr("word vector") + ": " + vector_str);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        if (reply->error() == QNetworkReply::NoError)
        {
            // 请求完成，所有数据都已正常接收
            //计算余弦相似度
            // A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(user_embedding_vector.value, Embedding_DB);
            ui->embedding_test_result->appendPlainText(jtr("The text segments with the highest similarity") + ":");
            //将分数前几的结果显示出来
            for (int i = 0; i < embedding_resultnumb && i < score.size(); ++i)
            {
                // qDebug()<<score[i].first<<score[i].second;
                ui->embedding_test_result->appendPlainText(QString::number(score[i].first + 1) + " " + jtr("Number text segment similarity") + ": " + QString::number(score[i].second));
            }
        }
        else
        {
            // 请求出错
            ui->embedding_test_log->appendPlainText(jtr("Request error, please make sure to start the embedded service"));
        }

        reply->abort(); //终止
        reply->deleteLater();
    });

    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);           //上传按钮
    ui->embedding_txt_embedding->setEnabled(1);        //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);      //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1); //选择模型按钮
}

// 计算两个向量的余弦相似度
double Expend::cosine_similarity(const std::vector<double> &a, const std::vector<double> &b)
{
    // 确保两个向量维度相同
    if (a.size() != b.size())
    {
        throw std::invalid_argument("Vectors must be of the same length");
        return 0;
    }

    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引
std::vector<std::pair<int, double>> Expend::similar_indices(const std::vector<double> &user_vector, const QVector<Embedding_vector> &embedding_DB)
{
    std::vector<std::pair<int, double>> scores; // 存储每个索引和其相应的相似度得分

    // 计算相似度得分
    for (const auto &emb : embedding_DB)
    {
        double sim = cosine_similarity(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }

    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b) { return a.second > b.second; });

    return scores;
}

//知识库构建过程
void Expend::embedding_processing()
{
    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);           //上传按钮
    ui->embedding_txt_embedding->setEnabled(0);        //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);      //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0); //选择模型按钮

    ui->embedding_txt_over->clear();                                                             //清空已嵌入文本段表格内容
    ui->embedding_txt_over->setRowCount(0);                                                      //设置已嵌入文本段表格为0行
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")}); //设置列名
    show_chunk_index = 0;                                                                        //待显示的嵌入文本段的序号

    //----------------------相同的内容不再嵌入, 先保留再新增-----------------------
    QVector<Embedding_vector> new_Embedding_DB;
    QVector<int> save_list;
    //构造一个如果文本段一致则保留的数据库
    for (int i = 0; i < Embedding_DB.size(); ++i)
    {
        bool remove_flag = true;
        int current_index_table = 0; //记录未嵌入之前在表格中的序号，保证将来在表格的位置
        //如果原来的数据库中有当前待嵌入文本一致的内容则保留
        for (int j = 0; j < ui->embedding_txt_wait->rowCount(); ++j)
        {
            QTableWidgetItem *item = ui->embedding_txt_wait->item(j, 0);
            if (item)
            {
                if (Embedding_DB.at(i).chunk == item->text())
                {
                    bool already_save = false;

                    // 若已经保留则不保留
                    for (int k = 0; k < new_Embedding_DB.size(); ++k)
                    {
                        if (new_Embedding_DB.at(k).chunk == item->text())
                        {
                            already_save = true;
                        }
                    }

                    if (!already_save)
                    {
                        remove_flag = false;
                        current_index_table = j;
                    }
                }
            }
        }
        if (!remove_flag)
        {
            new_Embedding_DB.append(Embedding_DB.at(i));
            new_Embedding_DB.last().index = current_index_table;
            save_list.append(current_index_table);
            // qDebug()<<"保留的"<<i<<Embedding_DB.at(i).chunk;
        }
    }
    Embedding_DB.clear();
    Embedding_DB = new_Embedding_DB;

    //读取待嵌入表格中的内容
    for (int i = 0; i < ui->embedding_txt_wait->rowCount(); ++i)
    {
        QTableWidgetItem *item = ui->embedding_txt_wait->item(i, 0);
        if (item)
        {
            if (!save_list.contains(i))
            {
                Embedding_DB.append({i, item->text()});
                // qDebug()<<"新增的"<<i<<item->text();
            }
        }
    }

    //先排好序
    std::sort(Embedding_DB.begin(), Embedding_DB.end(), [](const Embedding_vector &a, const Embedding_vector &b) { return a.index < b.index; });

    //进行嵌入工作,发送ready_embedding_chunks给llama-server
    //测试v1/embedding端点
    QElapsedTimer time;
    time.start();
    QEventLoop loop; // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(embedding_server_api + QString("")));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());

    //-------------------循环发送请求直到文本段处理完-------------------
    int toleran_times = 3; //最大重试次数
    QList<int> remain_index;
    for (int o = 0; o < Embedding_DB.size(); o++)
    {
        remain_index << o;
    }

    // 一直向服务发送请求，直到所有数据被正确处理
    while (remain_index.size() != 0 && toleran_times != 0)
    {
        //已经嵌入的就不处理了
        if (save_list.contains(Embedding_DB.at(remain_index.front()).index))
        {
            ui->embedding_txt_over->insertRow(ui->embedding_txt_over->rowCount()); // 在表格末尾添加新行
            QTableWidgetItem *newItem = new QTableWidgetItem(Embedding_DB.at(remain_index.front()).chunk);
            newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable); //单元格不可编辑
            newItem->setBackground(QColor(255, 165, 0, 60));           // 设置单元格背景颜色,橘黄色
            ui->embedding_txt_over->setItem(remain_index.front(), 0, newItem);
            ui->embedding_txt_over->setColumnWidth(0, qMax(ui->embedding_txt_over->width(), 400)); // 列宽保持控件宽度
            ui->embedding_txt_over->resizeRowsToContents();                                        // 自动调整行高
            ui->embedding_txt_over->scrollToItem(newItem, QAbstractItemView::PositionAtTop);       // 滚动到新添加的行
            show_chunk_index++;
            remain_index.removeFirst();
            continue;
        }

        //构造请求的数据体
        QJsonObject json;
        json.insert("model", "default");
        json.insert("encoding_format", "float");
        json.insert("input", Embedding_DB.at(remain_index.front()).chunk); //待嵌入文本段
        QJsonDocument doc(json);
        QByteArray data = doc.toJson();

        // POST 请求
        QNetworkReply *reply = manager.post(request, data);

        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            QString jsonString = reply->readAll();

            QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8()); // 使用QJsonDocument解析JSON数据
            QJsonObject rootObject = document.object();

            // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
            QJsonArray dataArray = rootObject["data"].toArray();
            QString vector_str = "[";
            for (int i = 0; i < dataArray.size(); ++i)
            {
                QJsonObject dataObj = dataArray[i].toObject();

                // 检查"data"对象中是否存在"embedding"
                if (dataObj.contains("embedding"))
                {
                    QJsonArray embeddingArray = dataObj["embedding"].toArray();
                    Embedding_DB[remain_index.front()].value.resize(ui->embedding_dim_spinBox->value()); // 分配空间
                    // 处理"embedding"数组
                    int arraySize = embeddingArray.size();
                    if (arraySize != ui->embedding_dim_spinBox->value()) { ui->embedding_test_log->appendPlainText(QString::number(arraySize) + " query embedding dim not match! Fill with 0"); }
                    for (int j = 0; j < ui->embedding_dim_spinBox->value(); ++j)
                    {
                        if (j < arraySize) { Embedding_DB[remain_index.front()].value[j] = embeddingArray[j].toDouble(); }
                        else
                        {
                            Embedding_DB[remain_index.front()].value[j] = 0.0;
                        } //返回的向量不足的维度用0填充
                        vector_str += QString::number(Embedding_DB[remain_index.front()].value[j], 'f', 4) + ", ";
                    }
                }
            }
            vector_str += "]";
            QString message = QString::number(Embedding_DB.at(remain_index.front()).index + 1) + " " + jtr("Number text segment embedding over") + "! " + jtr("dimension") + ": " + QString::number(Embedding_DB.at(remain_index.front()).value.size()) + " " + jtr("word vector") + ": " + vector_str;
            ui->embedding_test_log->appendPlainText(message);
            ui->embedding_test_log->verticalScrollBar()->setValue(ui->embedding_test_log->verticalScrollBar()->maximum());     //滚动条滚动到最下面
            ui->embedding_test_log->horizontalScrollBar()->setValue(ui->embedding_test_log->horizontalScrollBar()->minimum()); // 水平滚动条滚动到最左边
            emit expend2ui_state("expend:" + message, USUAL_SIGNAL);
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() {
            if (reply->error() == QNetworkReply::NoError)
            {
                // 请求完成，所有数据都已正常接收
                // 显示在已嵌入表格中
                ui->embedding_txt_over->insertRow(ui->embedding_txt_over->rowCount()); // 在表格末尾添加新行
                QTableWidgetItem *newItem = new QTableWidgetItem(Embedding_DB.at(show_chunk_index).chunk);
                newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable); //单元格不可编辑
                newItem->setBackground(LCL_ORANGE);                        // 设置单元格背景颜色,橘黄色
                ui->embedding_txt_over->setItem(show_chunk_index, 0, newItem);
                ui->embedding_txt_over->setColumnWidth(0, qMax(ui->embedding_txt_over->width(), 400)); // 列宽保持控件宽度
                ui->embedding_txt_over->resizeRowsToContents();                                        // 自动调整行高
                ui->embedding_txt_over->scrollToItem(newItem, QAbstractItemView::PositionAtTop);       // 滚动到新添加的行
                show_chunk_index++;
                embedding_server_need = true; // 下次启动自动执行嵌入
                remain_index.removeFirst();
            }
            else
            {
                // 请求出错
                ui->embedding_test_log->appendPlainText(jtr("Request error, please make sure to start the embedded service"));
                embedding_server_need = false;
                toleran_times--;
            }

            reply->abort(); //终止
            reply->deleteLater();
        });

        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        // 进入事件循环
        loop.exec();
    }

    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);           //上传按钮
    ui->embedding_txt_embedding->setEnabled(1);        //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);      //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1); //选择模型按钮

    ui->embedding_test_log->appendPlainText(jtr("embedding over") + " " + jtr("use time") + QString::number(time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s");
    emit expend2tool_embeddingdb(Embedding_DB); //发送已嵌入文本段数据给tool
}

//嵌入服务端点改变响应
void Expend::on_embedding_model_lineedit_textChanged()
{
    if (!keep_embedding_server)
    {
        server_process->kill();                                                                      // 终止server
        Embedding_DB.clear();                                                                        // 清空向量数据库
        ui->embedding_txt_over->clear();                                                             //清空已嵌入文本段表格内容
        ui->embedding_txt_over->setRowCount(0);                                                      //设置已嵌入文本段表格为0行
        ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")}); //设置列名
    }
}

//知识库描述改变响应
void Expend::on_embedding_txt_describe_lineEdit_textChanged()
{
    emit expend2ui_embeddingdb_describe(ui->embedding_txt_describe_lineEdit->text()); //传递知识库的描述
}

//嵌入结果返回个数改变响应
void Expend::on_embedding_resultnumb_spinBox_valueChanged(int value)
{
    embedding_resultnumb = value;
    emit expend2ui_embedding_resultnumb(embedding_resultnumb);
}



