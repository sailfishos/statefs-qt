/**
 * @file client.hpp
 * @brief Statefs QML plugin
 * @copyright (C) 2013-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "plugin.hpp"
#include "property.hpp"

#include <qqml.h>

void StatefsPlugin::registerTypes(char const* uri)
{
    qmlRegisterType<StateProperty>(uri, 1, 1, "StateProperty");
    qmlRegisterType<StateProperty>(uri, 1, 1, "ContextProperty");
}
