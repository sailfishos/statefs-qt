#ifndef _STATEFS_QML_PLUGIN_QT5_HPP_
#define _STATEFS_QML_PLUGIN_QT5_HPP_

#include <QtQml/QQmlExtensionPlugin>

class StatefsPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Mer.State")
public:
    void registerTypes(char const* uri);
};

#endif // _STATEFS_QML_PLUGIN_QT5_HPP_
