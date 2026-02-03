#pragma once

#include <QJsonObject>

#include "xconfig.h"

// 一次网络请求的不可变快照（避免跨线程共享可变字段）。
struct RequestSnapshot
{
    APIS apis;
    ENDPOINT_DATA endpoint;
    QJsonObject wordsObj;
    int languageFlag = EVA_LANG_ZH;
    quint64 turnId = 0;
};

Q_DECLARE_METATYPE(RequestSnapshot)
