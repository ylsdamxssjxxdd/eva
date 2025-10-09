
#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------记忆相关--------------------------------
//-------------------------------------------------------------------------

//传递记忆向量和上下文长度
void Expend::recv_brainvector(std::vector<Brain_Cell> Brain_vector_, int nctx_, bool reflash)
{
    if (nctx != nctx_)
    {
        init_brain_matrix();
    }
    nctx = nctx_;
    Brain_vector = Brain_vector_;

    if (reflash && ui->tabWidget->currentIndex() == 1)
    {
        reflash_brain_matrix();
    }
}

//重置记忆矩阵(新词表过来时/nctx变化时)
void Expend::init_brain_matrix()
{
    ui->brain_tableWidget->clear();
    ui->brain_tableWidget->setColumnCount(3);                                                   //设置多少列
    ui->brain_tableWidget->setRowCount(nctx);                                                   //创建很多行
    ui->brain_tableWidget->setHorizontalHeaderLabels(QStringList{"sequence", "token", "word"}); //设置列名
}

//刷新一次记忆矩阵
void Expend::reflash_brain_matrix()
{
    ui->brain_tableWidget->clear();
    ui->brain_tableWidget->setHorizontalHeaderLabels(QStringList{"sequence", "token", "word"}); //设置列名
    QTableWidgetItem *lastItem = nullptr;                                                       // 初始化指向最后一个单元格的指针
    for (int i = 0; i < int(Brain_vector.size()); ++i)
    {
        QTableWidgetItem *newItem1 = new QTableWidgetItem(QString::number(Brain_vector.at(i).id));
        newItem1->setFlags(newItem1->flags() & ~Qt::ItemIsEditable); //单元格不可编辑
        newItem1->setBackground(LCL_ORANGE);                         // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 0, newItem1);

        QTableWidgetItem *newItem2 = new QTableWidgetItem(QString::number(Brain_vector.at(i).token));
        newItem2->setFlags(newItem2->flags() & ~Qt::ItemIsEditable); //单元格不可编辑
        newItem2->setBackground(LCL_ORANGE);                         // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 1, newItem2);

        QTableWidgetItem *newItem3 = new QTableWidgetItem(Brain_vector.at(i).word.replace("\n", "\\n"));
        newItem3->setFlags(newItem3->flags() & ~Qt::ItemIsEditable); //单元格不可编辑
        newItem3->setBackground(LCL_ORANGE);                         // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 2, newItem3);

        lastItem = newItem3; // 更新最后一个单元格的引用
    }
    if (lastItem != nullptr)
    {
        // 滚动到最后一个添加的单元格
        ui->brain_tableWidget->scrollToItem(lastItem);
    }
}

// 用于设置whisper模型路径
void Expend::setWhisperModelpath(QString modelpath)
{
    ui->whisper_load_modelpath_linedit->setText(modelpath);
    whisper_params.model = modelpath.toStdString();
}

// 用于设置sd模型路径
void Expend::setSdModelpath(QString modelpath)
{
    ui->sd_modelpath_lineEdit->setText(modelpath);
    ui->params_template_comboBox->setCurrentText("sd1.5-anything-3"); // 默认
}

// 递归删除文件夹及其内容的函数
bool Expend::removeDir(const QString &dirName)
{
    QDir dir(dirName);

    if (!dir.exists())
    {
        return false;
    }

    // 删除目录中的所有文件和子目录
    foreach (QFileInfo item, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries))
    {
        if (item.isDir())
        {
            // 如果是子目录，递归删除
            if (!removeDir(item.absoluteFilePath()))
            {
                return false;
            }
        }
        else
        {
            // 如果是文件，删除文件
            if (!QFile::remove(item.absoluteFilePath()))
            {
                return false;
            }
        }
    }

    // 删除目录自身
    return dir.rmdir(dirName);
}
