#include <QtPlugin>

#if defined(EVA_LINUX_STATIC_BUILD) && defined(EVA_SKIP_FLITE_PLUGIN)
#include <QObject>

static QObject *evaNullTtsInstance()
{
    return nullptr;
}

static const QMetaObject *evaNullTtsMetaObject()
{
    return nullptr;
}

extern "C" const QStaticPlugin qt_static_plugin_QTextToSpeechEngineFlite()
{
    return QStaticPlugin{evaNullTtsInstance, evaNullTtsMetaObject};
}
#endif

