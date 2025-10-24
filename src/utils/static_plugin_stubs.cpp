#include <QtPlugin>

#if defined(EVA_ENABLE_QT_TTS) && defined(EVA_LINUX_STATIC_BUILD) && defined(EVA_SKIP_FLITE_PLUGIN)
#include <QObject>

static QObject *evaNullTtsInstance()
{
    return nullptr;
}

static const char *evaNullTtsMetaData()
{
    static const char metaData[] =
        "{"
        "\"IID\":\"org.qt-project.Qt.QTextToSpeechEngineFactoryInterface\","
        "\"MetaData\":{"
        "\"DefaultState\":false,"
        "\"Capabilities\":[]"
        "}"
        "}";
    return metaData;
}

extern "C" const QStaticPlugin qt_static_plugin_QTextToSpeechEngineFlite()
{
    return QStaticPlugin{evaNullTtsInstance, evaNullTtsMetaData};
}
#endif
