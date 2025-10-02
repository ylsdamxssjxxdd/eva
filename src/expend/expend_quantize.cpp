#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------模型量化相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择待量化模型路径时响应
void Expend::on_model_quantize_row_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, jtr("model_quantize_row_modelpath_lineedit_placeholder"), "(*.gguf)");
    ui->model_quantize_row_modelpath_lineedit->setText(currentpath);
}

//用户点击选择重要性矩阵路径时响应
void Expend::on_model_quantize_important_datapath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, jtr("model_quantize_important_datapath_lineedit_placeholder"), "(*.dat)");
    ui->model_quantize_important_datapath_lineedit->setText(currentpath);
}
//待量化模型路径改变响应
void Expend::on_model_quantize_row_modelpath_lineedit_textChanged()
{
    output_modelpath_change(); //根据待量化模型路径和量化方法填入量化后模型路径
}
//展示量化方法
void Expend::show_quantize_types()
{
    //量化方法说明
    quantize_types.clear();
    ui->model_quantize_type->clear();
    // 以f16-7B模型的大小为准，8bit是一个字节，f16就是两字节，或者16bpw(每个权重占16位)
    // f16-7B模型的大小 = 70*10^8*2 / 1024 /1024 /1024 = 13GB
    quantize_types = {
        {"F32", "-100.0%", "0", "⭐"},
        {"BF16", "0.0%", "-0.0050", "⭐"},
        {"F16", "0.0%", "-0.0020", "⭐"},
        {"Q8_0", "48.5%", "+0.0004", "⭐"},
        {"Q6_K", "60.4%", "+0.0008", "⭐"},
        {"Q5_1", "63.8%", "+0.0349", "⭐"},
        {"Q5_K_M", "65.8%", "+0.0122", "⭐⭐"},
        {"Q5_K_S", "66.7%", "+0.0400", "⭐"},
        {"Q5_0", "66.7%", "+0.0683", "⭐"},
        {"Q4_1", "70%", "+0.1585", "⭐"},
        {"Q4_K_M", "70.8%", "+0.0532", "⭐"},
        {"Q4_K_S", "72.4%", "+0.0992", "⭐"},
        {"Q4_0", "72.6%", "+0.2166", "⭐⭐"},
        {"Q3_K_L", "74.2%", "+0.1764", "⭐"},
        {"Q3_K_M", "76.4%", "+0.2496", "⭐⭐"},
        {"Q3_K_S", "78.8%", "+0.5551", "⭐"},
        {"Q2_K", "79.8%", "+0.6717", "⭐"},
        {"Q2_K_S", "83.4%", "+9.0634", ""},
        {"IQ4_NL", "71.9%", jtr("related to the imatrix"), "⭐"},
        {"IQ4_XS", "73.4%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_M", "77.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_S", "78.5%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_XS", "79.4%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_XXS", "80.9%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_M", "83.1%", jtr("related to the imatrix"), "⭐⭐"},
        {"IQ2_S", "84.3%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_XS", "85.6%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_XXS", "87.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ1_M", "89.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ1_S", "90.3%", jtr("related to the imatrix"), "⭐"},
    };

    //添加量化方法选项
    for (int i = 0; i < quantize_types.size(); ++i)
    {
        ui->model_quantize_type->addItem(quantize_types.at(i).typename_);
    }

    //添加量化方法说明
    ui->model_quantize_info->setRowCount(quantize_types.size()); //创建行
    ui->model_quantize_info->setColumnCount(5);
    ui->model_quantize_info->setHorizontalHeaderLabels(QStringList{jtr("quantize type"), jtr("compression") + "（f16->）", jtr("perplexity"), jtr("recommend"), jtr("estimated size") + "（f16->）"}); //设置列名
    for (int i = 0; i < quantize_types.size(); ++i)
    {
        QTableWidgetItem *newItem1 = new QTableWidgetItem(quantize_types.at(i).typename_);
        ui->model_quantize_info->setItem(i, 0, newItem1);

        QTableWidgetItem *newItem2 = new QTableWidgetItem(quantize_types.at(i).bit);
        ui->model_quantize_info->setItem(i, 1, newItem2);

        QTableWidgetItem *newItem3 = new QTableWidgetItem(quantize_types.at(i).perplexity);
        ui->model_quantize_info->setItem(i, 2, newItem3);

        QTableWidgetItem *newItem4 = new QTableWidgetItem(quantize_types.at(i).recommand);
        ui->model_quantize_info->setItem(i, 3, newItem4);
    }
    QHeaderView *headerView = ui->model_quantize_info->horizontalHeader(); //水平表头对象,用来控制列宽
    headerView->setSectionResizeMode(QHeaderView::Stretch);                // 设置所有列为等宽且撑满整个表格宽度

    // 默认的量化级别
    ui->model_quantize_type->setCurrentText("Q5_K_M");
}

// 根据待量化模型路径和量化方法填入量化后模型路径
void Expend::output_modelpath_change()
{
    //提取模型名
    QString modelpath = ui->model_quantize_row_modelpath_lineedit->text();
    if (modelpath.contains(".gguf") && QFile::exists(modelpath))
    {
        //重构名称，将尾部的量化词条更换为当前量化方法
        QString output_modelpath = modelpath.replace(modelpath.split(".gguf")[0].split("-").last(), ui->model_quantize_type->currentText());
        // qDebug()<<output_modelpath<<modelpath.split(".gguf")[0].split("-").last()<<modelpath.split(".gguf")[0];
        ui->model_quantize_output_modelpath_lineedit->setText(output_modelpath);

        //顺便改变量化方法说明中的预估量化后大小
        QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text()); //获取文件大小
        float in_modelsize = fileInfo1.size() / 1024.0 / 1024.0 / 1024.0;

#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        float totalPhysMem = memInfo.ullTotalPhys; // 总内存
#elif __linux__
        // 读取/proc/meminfo文件获取内存信息
        std::ifstream meminfoFile("/proc/meminfo");
        std::string line;
        float totalPhysMem = 0;

        if (meminfoFile.is_open())
        {
            while (getline(meminfoFile, line))
            {
                if (line.find("MemTotal:") == 0)
                {
                    std::istringstream iss(line);
                    std::string key;
                    float value;
                    std::string unit;
                    iss >> key >> value >> unit;
                    if (unit == "kB")
                    {
                        totalPhysMem = value * 1024; // 将kB转换为字节
                    }
                    break;
                }
            }
            meminfoFile.close();
        }
#endif
        //  估计量化后大小
        for (int i = 0; i < quantize_types.size(); ++i)
        {
            QTableWidgetItem *item = ui->model_quantize_info->item(i, 1);
            float estimate_modelsize = in_modelsize * abs(1 - item->text().split("%")[0].toFloat() / 100.0);
            QString estimate_modelsize_str = QString::number(estimate_modelsize, 'f', 1) + " GB";
            QTableWidgetItem *newItem1 = new QTableWidgetItem(estimate_modelsize_str);
            ui->model_quantize_info->setItem(i, 4, newItem1);

            //加星,如果量化后的大小比本机内存小20%以上就加一颗星
            QString star;
            if (estimate_modelsize < totalPhysMem / 1024.0 / 1024.0 / 1024.0 * 0.8)
            {
                star = quantize_types.at(i).recommand + "⭐";
            }
            else
            {
                star = quantize_types.at(i).recommand;
            }
            QTableWidgetItem *newItem4 = new QTableWidgetItem(star);
            ui->model_quantize_info->setItem(i, 3, newItem4);
        }
    }
}
//量化方法改变响应
void Expend::on_model_quantize_type_currentIndexChanged(int index)
{
    //根据待量化模型路径和量化方法填入量化后模型路径
    output_modelpath_change();

    // 表格中对应的量化信息高亮
    ui->model_quantize_info->clearSelection();                                    // 清除当前所有选择
    ui->model_quantize_info->setSelectionBehavior(QAbstractItemView::SelectRows); // 设置选择行为为选择行
    ui->model_quantize_info->selectRow(index);                                    // 选择指定的行
}
//用户点击执行量化按钮时响应
void Expend::on_model_quantize_execute_clicked()
{
    //锁定界面
    ui->model_quantize_frame1->setEnabled(0);
    ui->model_quantize_frame2->setEnabled(0);
    ui->model_quantize_frame3->setEnabled(0);
    ui->model_quantize_frame4->setEnabled(0);

    //执行量化
    quantize(ui->model_quantize_row_modelpath_lineedit->text(), ui->model_quantize_output_modelpath_lineedit->text(), ui->model_quantize_important_datapath_lineedit->text(), ui->model_quantize_type->currentText());
}

//执行量化
void Expend::quantize(QString in_modelpath, QString out_modelpath, QString important_datapath, QString quantize_type)
{
    //结束llama-quantize
    quantize_process->kill();
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-quantize") + SFX_NAME;
    QString program = localPath; // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-quantize") + SFX_NAME;
    QString program = localPath; // 设置要运行的exe文件的路径
#endif
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    if (important_datapath != "")
    {
        arguments << "--imatrix" << important_datapath;
    }                                               //重要性矩阵路径
    arguments << in_modelpath;                      //待量化模型路径
    arguments << out_modelpath;                     //输出路径
    arguments << quantize_type;                     //量化方法
    arguments << QString::number(max_thread * 0.5); //使用线程数

    //连接信号和槽,获取程序的输出
    connect(quantize_process, &QProcess::readyReadStandardOutput, [=]() {
        QString output = quantize_process->readAllStandardOutput();
        ui->model_quantize_log->appendPlainText(output);
    });
    connect(quantize_process, &QProcess::readyReadStandardError, [=]() {
        QString output = quantize_process->readAllStandardError();
        ui->model_quantize_log->appendPlainText(output);
        // if(output.contains("llama_model_quantize_internal: model size  =  "))
        // {
        //     in_modelsize = output.split("llama_model_quantize_internal: model size  =  ")[1];
        //     qDebug()<<in_modelsize;
        // }
    });
    quantize_process->start(program, arguments);
}
//开始信号
void Expend::quantize_onProcessStarted() {}
//结束信号
void Expend::quantize_onProcessFinished()
{
    //解锁界面
    ui->model_quantize_frame1->setEnabled(1);
    ui->model_quantize_frame2->setEnabled(1);
    ui->model_quantize_frame3->setEnabled(1);
    ui->model_quantize_frame4->setEnabled(1);

    ui->model_quantize_log->appendPlainText(jtr("quantize completed! model save") + ":" + ui->model_quantize_output_modelpath_lineedit->text());
    QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text()); //获取文件大小
    float modelsize1_GB = fileInfo1.size() / 1024.0 / 1024.0 / 1024.0;
    QFileInfo fileInfo2(ui->model_quantize_output_modelpath_lineedit->text()); //获取文件大小
    float modelsize2_GB = fileInfo2.size() / 1024.0 / 1024.0 / 1024.0;
    ui->model_quantize_log->appendPlainText(QString::number(modelsize1_GB) + " GB" + " -> " + QString::number(modelsize2_GB) + " GB " + jtr("compression") + " :" + QString::number((1 - modelsize2_GB / modelsize1_GB) * 100) + "%");
}
