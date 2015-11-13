/**
 * @file client.hpp
 * @brief Statefs QML plugin
 * @copyright (C) 2013-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "contextkit_plugin.hpp"
#include "contextkit_property.hpp"

#include <qqml.h>

void ContextkitPlugin::registerTypes(char const* uri)
{
    qmlRegisterType<ContextPropertyDeclarative>(uri, 1, 0, "ContextProperty");
}
