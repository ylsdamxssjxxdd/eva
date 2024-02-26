#include "xtool.h"

xTool::xTool()
{
    ;
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
        qDebug()<<"tool:" + QString("calculator ") + wordsObj["return"].toString() + result;
        emit tool2ui_state("tool:" + QString("calculator ") + wordsObj["return"].toString() + result,1);
        emit tool2ui_pushover(QString("calculator ") + wordsObj["return"].toString() + result);
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
            emit tool2ui_pushover(QString("cmd ") + wordsObj["return"].toString() + process->errorString());
            qDebug() << QString("cmd ") + wordsObj["return"].toString() + process->errorString();
        } 
        else 
        {
            // 获取命令的输出
            QString output = process->readAll();
            emit tool2ui_pushover(QString("cmd ") + wordsObj["return"].toString() + output);
            qDebug() << QString("cmd ") + wordsObj["return"].toString() + output;
        }

    }
    else if(func_arg_list.front() == "search")
    {
        emit tool2ui_pushover(wordsObj["not set tool"].toString());
    }
    else if(func_arg_list.front() == "knowledge")
    {
        emit tool2ui_pushover(wordsObj["not set tool"].toString());
    }
    else
    {
        //没有该工具
        emit tool2ui_pushover(wordsObj["not load tool"].toString());
    }
    
    
}

void xTool::recv_func_arg(QStringList func_arg_list_)
{
    if(warmup){getWords();warmup=false;}
    func_arg_list = func_arg_list_;
}

void xTool::getWords()
{
    QFile jfile(":/ui/chinese.json");
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading.";
        return;
    }

    QTextStream in(&jfile);
    in.setCodec("UTF-8"); // 确保使用UTF-8编码读取文件
    QString data = in.readAll();
    jfile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = doc.object();
    wordsObj = jsonObj["words"].toObject();
}