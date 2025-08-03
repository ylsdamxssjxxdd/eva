#include "xtool.h"

xTool::xTool(QString applicationDirPath_) {
    applicationDirPath = applicationDirPath_;
    
    qDebug() << "tool init over";
}
xTool::~xTool() { ; }

void xTool::Exec(mcp::json tools_call) {
    QString tools_name = QString::fromStdString(get_string_safely(tools_call,"name"));
    mcp::json tools_args_ = get_json_object_safely(tools_call,"arguments");
    QString tools_args = QString::fromStdString(tools_args_.dump());//arguments字段提取出来还是一个对象所以用dump
    qDebug()<<"tools_name"<<tools_name<<"tools_args"<<tools_args;
    //----------------------计算器------------------
    if (tools_name == "calculator") 
    {
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_,"expression"));
        emit tool2ui_state("tool:" + QString("calculator(") + build_in_tool_arg + ")");
        QString result = QString::number(te_interp(build_in_tool_arg.toStdString().c_str(), 0));
        // qDebug()<<"tool:" + QString("calculator ") + jtr("return") + "\n" + result;
        if (result == "nan")  //计算失败的情况
        {
            emit tool2ui_pushover(QString("calculator ") + jtr("return") + "Calculation failed, please confirm if the calculation formula is reasonable");
        } else {
            emit tool2ui_pushover(QString("calculator ") + jtr("return") + "\n" + result);
            
        }
        emit tool2ui_state("tool:" + QString("calculator ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    }
    //----------------------命令提示符------------------
    else if (tools_name == "execute_command") {
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_,"content"));
        QProcess* process = new QProcess();
        createTempDirectory(applicationDirPath + "/EVA_WORK");//防止没有这个目录
        process->setWorkingDirectory(applicationDirPath + "/EVA_WORK"); // 设置运行目录
        // +++ 添加环境变量支持 +++
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        #ifdef __linux__
            // 追加Linux终端PATH（通常包含用户级工具路径）
            QString path = env.value("PATH");
            env.insert("PATH", "/usr/local/bin:/usr/bin:/bin:" + path);
        #endif
        process->setProcessEnvironment(env); // 关键设置
        // +++++++++++++++++++++++
        QStringList shellArgs;
        shellArgs << CMDGUID << build_in_tool_arg;

        process->start(shell, shellArgs);
        qDebug() << "Executing command:" << shell << shellArgs;
        emit tool2ui_state(QString("tool: ") + shell + " " + shellArgs.join(" "));

        process->waitForFinished();//process->waitForFinished()程序将会阻塞
        QByteArray byteArray = process->readAll();  // 获取标准输出
        QByteArray errorByteArray = process->readAllStandardError();  // 获取标准错误输出
        QByteArray combinedOutput = byteArray + errorByteArray; // 合并输出
    #ifdef Q_OS_WIN
        QString output = QString::fromLocal8Bit(combinedOutput);// 转换为QString
    #else
        QString output = QString::fromUtf8(combinedOutput);// 转换为QString
    #endif
        emit tool2ui_state("tool:" + QString("execute_command ") + "\n" + output, TOOL_SIGNAL);
        emit tool2ui_pushover(QString("execute_command ") + "\n" + output);
        qDebug() << QString("execute_command ") + "\n" + output;

    }
    //----------------------知识库------------------
    else if (tools_name == "knowledge") {
        QElapsedTimer time4;
        time4.start();
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_,"content"));
        QString result;
        if (Embedding_DB.size() == 0) {
            result = jtr("Please tell user to embed knowledge into the knowledge base first");
            emit tool2ui_state("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
            emit tool2ui_pushover(QString("knowledge ") + jtr("return") + "\n" + result);
        } else {
            //查询计算词向量和计算相似度，返回匹配的文本段
            emit tool2ui_state("tool:" + jtr("qureying"));
            result = embedding_query_process(build_in_tool_arg);
            emit tool2ui_state("tool:" + jtr("qurey&timeuse") + QString(": ") + QString::number(time4.nsecsElapsed() / 1000000000.0, 'f', 2) + " s");
            emit tool2ui_state("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
            emit tool2ui_pushover(QString("knowledge ") + jtr("return") + "\n" + result);
        }

    }
    //----------------------鼠标键盘------------------
    else if (tools_name == "controller") {

        std::vector<std::string> build_in_tool_arg = get_string_list_safely(tools_args_,"sequence");

        // 拼接打印参数
        std::ostringstream oss;
        for (size_t i = 0; i < build_in_tool_arg.size(); ++i) {
            oss << build_in_tool_arg[i];
            if (i < build_in_tool_arg.size() - 1) {
                oss << " "; // 用空格分隔
            }
        }
        std::string build_in_tool_arg_ = oss.str();

        emit tool2ui_state("tool:" + QString("controller(") + QString::fromStdString(build_in_tool_arg_) + ")");

        // 执行行动序列
        excute_sequence(build_in_tool_arg);

        emit tool2ui_pushover(QString("controller ") + jtr("return") + "\n" + "excute sequence over" + "\n");
        
    }
    //----------------------文生图------------------
    else if (tools_name == "stablediffusion") {
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_,"prompt"));
        //告诉expend开始绘制
        emit tool2expend_draw(build_in_tool_arg);
    }
    //----------------------读取文件------------------
    else if (tools_name == "read_file") {
        // 获取路径参数
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_,"path"));
        QString filepath = build_in_tool_arg;
        filepath.replace(applicationDirPath + "/EVA_WORK/","");//去重
        filepath = applicationDirPath + "/EVA_WORK/" + filepath;

        // 获取行号参数，默认为1
        int start_line = get_int_safely(tools_args_, "start_line", 1);
        int end_line = get_int_safely(tools_args_, "end_line", INT_MAX);

        // 验证行号有效性
        if (start_line < 1) start_line = 1;
        if (end_line < start_line) end_line = start_line;
        if (end_line - start_line + 1 > 200) end_line = start_line + 199;

        QString result;
        QFile file(filepath);
        // 尝试打开文件
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit tool2ui_pushover(QString("read_file ") + jtr("return") + QString("can not open file: %1").arg(filepath));//返回错误
            return ;
        }

        // 使用 QTextStream 读取文件内容
        QTextStream in(&file);
        in.setCodec("UTF-8");  // 设置编码为UTF-8

        // 读取指定行范围
        QStringList lines;
        int current_line = 0;
        while (!in.atEnd()) {
            current_line++;
            QString line = in.readLine();
            if (current_line >= start_line && current_line <= end_line) {
                lines.append(line);
            }
            if (current_line > end_line) break;
        }
        file.close();

        result = lines.join("\n");
        emit tool2ui_state("tool:" + QString("read_file ") + jtr("return") + QString(" (lines %1-%2)\n").arg(start_line).arg(qMin(current_line, end_line)) + result, TOOL_SIGNAL);
        emit tool2ui_pushover(QString("read_file ") + jtr("return") + QString(" (lines %1-%2)\n").arg(start_line).arg(qMin(current_line, end_line)) + result);//返回结果
    }
    //----------------------写入文件------------------
    else if (tools_name == "write_file") {
        QString filepath = QString::fromStdString(get_string_safely(tools_args_,"path"));
        QString content = QString::fromStdString(get_string_safely(tools_args_,"content"));
        filepath.replace(applicationDirPath + "/EVA_WORK/","");//去重
        filepath = applicationDirPath + "/EVA_WORK/" + filepath;
        // Extract the directory path from the file path
        QFileInfo fileInfo(filepath);
        QString dirPath = fileInfo.absolutePath();
        
        // Create the directory structure if it doesn't exist
        QDir dir;
        if (!dir.mkpath(dirPath)) {
            emit tool2ui_pushover(QString("write_file ") + jtr("return") + "Failed to create directory");//返回错误
            return;  // or handle the error as appropriate
        }
        
        QFile file(filepath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit tool2ui_pushover(QString("write_file ") + jtr("return") + "Could not open file for writing" + file.errorString());//返回错误
            return;  // or handle the error as appropriate
        }
        //处理换行符
        // qDebug()<<content;

        QTextStream out(&file);
        out.setCodec("UTF-8");  // 设置编码为UTF-8
        out << content;
        file.close();
        QString result = "write over";
        emit tool2ui_state("tool:" + QString("write_file ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        emit tool2ui_pushover(QString("write_file ") + jtr("return") + "\n" + result);
    }
    else if (tools_name == "edit_file") {
        // ─────── 1. 解析参数 ───────
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        QString oldStr   = QString::fromStdString(get_string_safely(tools_args_, "old_string"));
        QString newStr   = QString::fromStdString(get_string_safely(tools_args_, "new_string"));

        int expectedRepl = 1;                      // 默认 1 次
        if (tools_args_.contains("expected_replacements")) {
            expectedRepl = static_cast<int>(tools_args_["expected_replacements"].get<int>());
            if (expectedRepl < 1) expectedRepl = 1;
        }

        // 必须使用工作目录根做一次“去重 + 归一化”
        filepath.replace(applicationDirPath + "/EVA_WORK/", "");
        filepath = applicationDirPath + "/EVA_WORK/" + filepath;

        // ─────── 2. 读取文件 ───────
        QFile inFile(filepath);
        if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit tool2ui_pushover("edit_file " + jtr("return") + "Could not open file for reading: " + inFile.errorString());
            return;
        }
        QString fileContent = QString::fromUtf8(inFile.readAll());
        inFile.close();

        // ─────── 3. 统计出现次数并校验 ───────
        int occurrences = fileContent.count(oldStr, Qt::CaseSensitive);
        if (occurrences == 0) {
            emit tool2ui_pushover("edit_file " + jtr("return") + "old_string NOT found.");
            return;
        }
        if (occurrences != expectedRepl) {
            emit tool2ui_pushover(
                "edit_file " + jtr("return") +
                QString("Expected %1 replacement(s) but found %2.").arg(expectedRepl).arg(occurrences));
            return;
        }

        // ─────── 4. 执行替换 ───────
        int replacedCount = 0;
        int idx = 0;
        while ((idx = fileContent.indexOf(oldStr, idx, Qt::CaseSensitive)) != -1) {
            fileContent.replace(idx, oldStr.length(), newStr);
            idx += newStr.length();
            ++replacedCount;
        }

        // 再安全校验
        if (replacedCount != expectedRepl) {
            emit tool2ui_pushover(
                "edit_file " + jtr("return") +
                QString("Replacement count mismatch, replaced %1 time(s).").arg(replacedCount));
            return;
        }

        // ─────── 5. 写回文件 ───────
        QFileInfo fi(filepath);
        QDir dir;
        if (!dir.mkpath(fi.absolutePath())) {
            emit tool2ui_pushover("edit_file " + jtr("return") + "Failed to create directory.");
            return;
        }

        QFile outFile(filepath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            emit tool2ui_pushover("edit_file " + jtr("return") + "Could not open file for writing: " + outFile.errorString());
            return;
        }
        QTextStream ts(&outFile);
        ts.setCodec("UTF-8");
        ts << fileContent;
        outFile.close();

        // ─────── 6. 成功反馈 ───────
        QString result = QString("replaced %1 occurrence(s)").arg(replacedCount);
        emit tool2ui_state("tool:edit_file " + jtr("return") + "\n" + result, TOOL_SIGNAL);
        emit tool2ui_pushover("edit_file " + jtr("return") + "\n" + result);
    }
    else if(tools_name.contains("mcp_tools_list"))//查询mcp可用工具
    {
        emit tool2mcp_toollist();
    }
    else if(tools_name.contains("@"))//如果工具名包含@则假设他是mcp工具
    {
        emit tool2mcp_toolcall(tools_name,tools_args);
    }
    //----------------------没有该工具------------------
    else {
        emit tool2ui_pushover(jtr("not load tool"));
    }
}

//创建临时文件夹EVA_TEMP
bool xTool::createTempDirectory(const QString& path) {
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

//查询计算词向量和计算相似度，返回匹配的文本段
QString xTool::embedding_query_process(QString query_str) {
    QString knowledge_result;
    //---------------计算查询文本段的词向量-------------------------
    ipAddress = getFirstNonLoopbackIPv4Address();
    QEventLoop loop;  // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QString embedding_server_api = "http://" + QString(DEFAULT_EMBEDDING_IP) + ":" + DEFAULT_EMBEDDING_PORT + DEFAULT_EMBEDDING_API;
    QNetworkRequest request(QUrl(embedding_server_api + QString("")));  //加一个""是为了避免语法解析错误
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    //构造请求的数据体
    QJsonObject json;
    json.insert("model", "defult");
    json.insert("encoding_format", "float");
    json.insert("input", query_str);
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    // POST 请求
    QNetworkReply* reply = manager.post(request, data);

    // 处理响应
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        QString jsonString = reply->readAll();
        QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());  // 使用QJsonDocument解析JSON数据
        QJsonObject rootObject = document.object();
        // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
        QJsonArray dataArray = rootObject["data"].toArray();
        QString vector_str = "[";

        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject dataObj = dataArray[i].toObject();

            // 检查"data"对象中是否存在"embedding"
            if (dataObj.contains("embedding")) {
                QJsonArray embeddingArray = dataObj["embedding"].toArray();
                query_embedding_vector.value.resize(embedding_server_dim);
                // 处理"embedding"数组
                for (int j = 0; j < embeddingArray.size(); ++j) {
                    query_embedding_vector.value[j] = embeddingArray[j].toDouble();
                    vector_str += QString::number(query_embedding_vector.value[j], 'f', 4) + ", ";
                }
            }
        }
        vector_str += "]";
        emit tool2ui_state("tool:" + jtr("The query text segment has been embedded") + jtr("dimension") + ": " + QString::number(query_embedding_vector.value.size()) + " " + jtr("word vector") + ": " + vector_str, USUAL_SIGNAL);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            // 请求完成，所有数据都已正常接收
            //------------------------计算余弦相似度---------------------------
            // A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(query_embedding_vector.value, Embedding_DB);  //计算查询文本段和所有嵌入文本段之间的相似度

            if (score.size() > 0) {
                knowledge_result += jtr("The three text segments with the highest similarity") + DEFAULT_SPLITER;
            }

            //将分数前几的结果显示出来
            for (int i = 0; i < embedding_server_resultnumb && i < score.size(); ++i) {
                knowledge_result += QString::number(score[i].first + 1) + jtr("Number text segment similarity") + ": " + QString::number(score[i].second);
                knowledge_result += " " + jtr("content") + DEFAULT_SPLITER + Embedding_DB.at(score[i].first).chunk + "\n";
            }

            if (score.size() > 0) {
                knowledge_result += jtr("Based on this information, reply to the user's previous questions");
            }

        } else {
            // 请求出错
            emit tool2ui_state("tool:" + jtr("Request error") + " " + reply->error(), WRONG_SIGNAL);
            knowledge_result += jtr("Request error") + " " + reply->error();
        }

        reply->abort();  //终止
        reply->deleteLater();
    });

    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    return knowledge_result;
}

//获取ipv4地址
QString xTool::getFirstNonLoopbackIPv4Address() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++) {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            return list[i].toString();
        }
    }
    return QString();
}

// 计算两个向量的余弦相似度，A向量点积B向量除以(A模乘B模)
double xTool::cosine_similarity_1024(const std::vector<double>& a, const std::vector<double>& b) {
    // 确保两个向量维度相同
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must be of the same length");
    }

    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引
std::vector<std::pair<int, double>> xTool::similar_indices(const std::vector<double>& user_vector, const QVector<Embedding_vector>& embedding_DB) {
    std::vector<std::pair<int, double>> scores;  // 存储每个索引和其相应的相似度得分

    // 计算相似度得分
    for (const auto& emb : embedding_DB) {
        double sim = cosine_similarity_1024(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }

    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) { return a.second > b.second; });

    return scores;
}

void xTool::recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_) {
    Embedding_DB.clear();
    Embedding_DB = Embedding_DB_;
    emit tool2ui_state("tool:" + jtr("Received embedded text segment data"), USUAL_SIGNAL);
}

//传递嵌入结果返回个数
void xTool::recv_embedding_resultnumb(int resultnumb) {
    embedding_server_resultnumb = resultnumb;
}

//接收图像绘制完成信号
void xTool::recv_drawover(QString result_, bool ok_) {
    //绘制失败的情况
    if (!ok_) {
        emit tool2ui_pushover(result_);
        return;
    }

    //绘制成功的情况
    //添加绘制成功并显示图像指令
    emit tool2ui_state("tool:" + QString("stablediffusion ") + jtr("return") + "\n" + "<ylsdamxssjxxdd:showdraw>" + result_, TOOL_SIGNAL);
    emit tool2ui_pushover("<ylsdamxssjxxdd:showdraw>" + result_);
}

//传递控制完成结果
void xTool::tool2ui_controller_over(QString result) {
    emit tool2ui_state("tool:" + QString("controller ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    emit tool2ui_pushover(QString("controller ") + jtr("return") + "\n" + result);
}

void xTool::recv_language(int language_flag_) { language_flag = language_flag_; }

// 根据language.json和language_flag中找到对应的文字
QString xTool::jtr(QString customstr) { return wordsObj[customstr].toArray()[language_flag].toString(); }


void xTool::recv_callTool_over(QString result)
{
    if(result=="")
    {
        emit tool2ui_pushover(jtr("not load tool"));
    }
    else
    {
        emit tool2ui_state("tool:" + QString("mcp ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        emit tool2ui_pushover(QString("mcp ") + jtr("return") + "\n" + result);
    }

}

// mcp列出工具完毕
void xTool::recv_calllist_over()
{
    QString result= mcpToolParser(MCP_TOOLS_INFO_ALL);
    emit tool2ui_state("tool:" + QString("mcp_tool_list ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    emit tool2ui_pushover(QString("mcp_tool_list ") + jtr("return") + "\n" + result);

}

// 解析出所有mcp工具信息拼接为一段文本
QString xTool::mcpToolParser(mcp::json toolsinfo)
{
    QString result = "";
    for (const auto& tool : toolsinfo) 
    {
        TOOLS_INFO mcp_tools_info(
            QString::fromStdString(tool["service"].get<std::string>() + "@" + tool["name"].get<std::string>()), // 工具名
            QString::fromStdString(tool["description"]), // 工具描述
            QString::fromStdString(tool["inputSchema"].dump()) // 参数结构
        );

        result += mcp_tools_info.text + "\n";
    }
    return result;
}

// 执行行动序列
void xTool::excute_sequence(std::vector<std::string> build_in_tool_arg)
{
    // build_in_tool_arg的样式为 ["left_down(100,200)", "time_span(0.1)", "left_up()", "time_span(0.5)", "left_down(100,200)", "time_span(0.1)", "left_up()"] 
    for (const auto& action : build_in_tool_arg) {
        // 1. 提取函数名
        size_t pos_start = action.find('(');
        size_t pos_end = action.find(')');
        
        // 检查括号是否存在
        if (pos_start == std::string::npos || pos_end == std::string::npos) {
            // 错误处理：跳过无效格式
            continue;
        }

        // 函数名（左括号前的部分）
        std::string func_name = action.substr(0, pos_start);

        // 2. 提取参数字符串（括号内的内容）
        std::string args_str = action.substr(pos_start + 1, pos_end - pos_start - 1);
        std::vector<std::string> args_list;

        // 3. 分割参数（逗号分隔）
        if (!args_str.empty()) {
            std::istringstream iss(args_str);
            std::string arg;
            while (std::getline(iss, arg, ',')) {
                // 可选：去除参数两端的空格
                auto trim = [](std::string& s) {
                    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }));
                    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                        return !std::isspace(ch);
                    }).base(), s.end());
                };
                trim(arg); // 调用 trim 函数
                args_list.push_back(arg);
            }
        }

        // // 打印提取结果（实际执行时替换为具体操作）
        // std::cout << "Function: " << func_name << ", Args: ";
        // for (const auto& a : args_list) {
        //     std::cout << a << " ";
        // }

        using namespace platform;
        if(func_name=="left_down" && args_list.size()==2)
        {
            std::cout<<"left_down "<<" x "<<args_list[0]<<" y "<<args_list[1];
            leftDown(std::stoi(args_list[0]),std::stoi(args_list[1]));
        }
        else if(func_name=="left_up"){
            std::cout<<"leftUp ";
            leftUp();
        }
        else if(func_name=="right_down" && args_list.size()==2)
        {
            std::cout<<"right_down "<<" x "<<args_list[0]<<" y "<<args_list[1];
            rightDown(std::stoi(args_list[0]),std::stoi(args_list[1]));
        }
        else if(func_name=="right_up"){
            std::cout<<"right_up ";
            rightUp();
        }  
        else if(func_name=="move" && args_list.size()==3)
        {
            int ex=std::stoi(args_list[0]);
            int ey=std::stoi(args_list[1]);
            float sec=std::stof(args_list[2]);
            std::cout<<"move "<<" x "<<args_list[0]<<" y "<<args_list[1]<<" t "<<args_list[2];
    #ifdef _WIN32
            POINT p; GetCursorPos(&p);
            int sx=p.x, sy=p.y;
    #else
            Window root_r,child_r; int sx,sy; int rx,ry; unsigned int mask;
            XQueryPointer(dsp(),DefaultRootWindow(dsp()),&root_r,&child_r,
                        &rx,&ry,&sx,&sy,&mask);
    #endif
            const int fps=60; int steps=std::max(1,int(sec*fps));
            for(int i=1;i<=steps;++i)
            {
                double k=double(i)/steps;
                moveCursor(int(sx+k*(ex-sx)),int(sy+k*(ey-sy)));
                msleep(unsigned(sec*1000/steps));
            }

        }
        else if(func_name=="keyboard" && args_list.size()==1)
        {
            std::string keys = args_list[0];
            // 去掉所有单引号和双引号
            keys.erase(std::remove(keys.begin(), keys.end(), '\''), keys.end());
            keys.erase(std::remove(keys.begin(), keys.end(), '\"'), keys.end());

            std::cout << "keyboard " << keys;

            if (keys.find('+') != std::string::npos) {  // 组合键
                sendKeyCombo(split(keys, '+'));
            } else {
                sendKeyCombo({keys});
            }
        }
        else if(func_name=="time_span" && args_list.size()==1)
        {
            std::cout<<"time_span "<<" t "<<args_list[0];
            msleep(unsigned(std::stof(args_list[0])*1000));
        }

        std::cout << std::endl;
    }
    msleep(100);// 强制等待0.1s，让电脑显示出最终结果
}