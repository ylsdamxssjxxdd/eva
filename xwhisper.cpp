#include "xwhisper.h"

xWhisper::xWhisper()
{
    ;
}

xWhisper::~xWhisper()
{
    ;
}

void xWhisper::run()
{
    ;
}

void xWhisper::recv_modelpath(QString model_path_)
{
    model_path = model_path_;
    qDebug()<<"whisper:"<<model_path;
}