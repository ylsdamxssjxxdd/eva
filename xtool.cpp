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
        //qDebug()<<"tool:" + QString("calculator ") + wordsObj["return"].toString() + result;
        emit tool2ui_state("tool:" + QString("calculator ") + wordsObj["return"].toString() + result,TOOL_);
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
            emit tool2ui_state("tool:" +QString("cmd ") + wordsObj["return"].toString() + process->errorString(),TOOL_);
            emit tool2ui_pushover(QString("cmd ") + wordsObj["return"].toString() + process->errorString());
            qDebug() << QString("cmd ") + wordsObj["return"].toString() + process->errorString();
        } 
        else 
        {
            // 获取命令的输出
            QByteArray byteArray = process->readAll();
            QString output = QString::fromLocal8Bit(byteArray);
            emit tool2ui_state("tool:" +QString("cmd ") + wordsObj["return"].toString() + output,TOOL_);
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
    else if(func_arg_list.front() == "positron")
    {
        emit tool2ui_state("tool:" + QString("positron(") + func_arg_list.last() + ")");

        //阳电子步枪开始充能
        emit positron_starter();
        //positron_p->start(1000);
    }
    else if(func_arg_list.front() == "llm")
    {
        
        emit tool2ui_pushover(wordsObj["not set tool"].toString());
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
    qDebug()<<"tool:" + QString("positron ") + wordsObj["return"].toString() + result;
    emit tool2ui_state("tool:" + QString("positron ") + wordsObj["return"].toString() + result,TOOL_);
    emit tool2ui_pushover(QString("positron ") + wordsObj["return"].toString() + result);
}

void xTool::recv_func_arg(QStringList func_arg_list_)
{
    if(warmup){getWords(":/chinese.json");warmup=false;}
    func_arg_list = func_arg_list_;
}

void xTool::getWords(QString json_file_path)
{
    QFile jfile(json_file_path );
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
//阳电子步枪充能,数到3就发射
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