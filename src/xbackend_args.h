// Helper utilities for building llama.cpp server arguments
#ifndef XBACKEND_ARGS_H
#define XBACKEND_ARGS_H

#include "xconfig.h"
#include <QString>
#include <QStringList>

struct LocalServerArgsInput
{
    SETTINGS settings;
    QString host;
    QString port;
    QString modelPath;
    QString mmprojPath;
    QString loraPath;
    QString resolvedDevice;
};

QStringList buildLocalServerArgs(const LocalServerArgsInput &input);

#endif // XBACKEND_ARGS_H
