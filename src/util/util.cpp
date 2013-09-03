/**
 * @file util.cpp
 * @brief Statefs utilities for Qt-based apps and libraries
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "util_p.hpp"
#include <cor/error.hpp>

#include <QRegExp>
#include <QDebug>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>

namespace statefs {
/// All StateFS Qt bindings are put into this namespace
namespace qt {

/**
 * @defgroup util Miscellaneous StateFS Utilities
 *
 * @{
 */

/**
 * split full property name (dot- or slash-separated) to statefs path
 * parts relative to the namespace/provider root directory
 *
 * @param name full property name (including all namespaces/domains)
 * @param parts output parameter to accept path parts
 *
 * @return true if property name was succesfully splitted
 */
bool splitPropertyName(const QString &name, QStringList &parts)
{
    QRegExp re("[./]");
    re.setPatternSyntax(QRegExp::RegExp);
    parts = name.split(re);
    if (!parts.size()) {
        qWarning() << "Can't parse property name:" << name;
        return false;
    }

    if (parts.size() > 2) {
        auto fname = parts.last();
        parts.pop_back();
        if (!parts[0].size()) // for names like "/..."
            parts.pop_front();
        auto ns = parts.join("_");
        parts.clear();
        parts.push_back(ns);
        parts.push_back(fname);
    }
    // should be 2 parts: namespace and property_name
    return (parts.size() == 2);
}

/**
 * get path to the statefs property file for the statefs instance
 * mounted to the default statefs root
 *
 * @param name dot- or slash-separated full property name
 *
 * @return full path to the property file
 */
QString getPath(const QString &name)
{
    QStringList parts;
    if (!splitPropertyName(name, parts))
        return "";

    parts.push_front("namespaces");
    parts.push_front("state");
    parts.push_front(::getenv("XDG_RUNTIME_DIR")); // TODO hardcoded source!

    return parts.join(QDir::separator());
}

static QRegExp re(QString const &cs)
{
	auto in_spaces = [](QString const& s) {
		static const QString aspaces = "\\s*";
		return aspaces + s + aspaces;
	};
	return QRegExp(in_spaces(cs), Qt::CaseSensitive, QRegExp::RegExp2);
};

static const QString date_re("[0-9]{4}-[0-9]{2}-[0-9]{2}");
static const QString hhmm_re("[0-9]{2}:[0-9]{2}");
static const QString time_re(QString("%1(:[0-9]{2})?").arg(hhmm_re));
static const QString tz_re("(Z|[+-][0-9]{2}(:[0-9]{2})?)");
static const QString datetime_re(QString("%1T%2%3")
								 .arg(date_re, time_re, tz_re));


static const std::initializer_list<std::pair<QRegExp, QVariant::Type> > re_types
= {{re("[+-][0-9]+"), QVariant::Int}
   , {re("[0-9]+"), QVariant::UInt}
   , {re("[+-]?([0-9]+\\.[0-9]*|[0-9]*\\.[0-9]+)"), QVariant::Double}
   , {re(date_re), QVariant::Date}
   , {re(time_re), QVariant::Time}
   , {re(datetime_re), QVariant::DateTime}
};

/**
 * try to convert input string to QVariant using simple
 * heuristics.
 *
 * @param s input string
 *
 * @return result of conversion or input string wrapped into QVariant
 * otherwise
 */
QVariant valueDecode(QString const& s)
{
	if (!s.size())
		return QVariant(s);

	QVariant v(s);
	for (auto const& re_type : re_types) {
		if (re_type.first.exactMatch(s)) {
			v.convert(re_type.second);
			break;
		}
	}
	return v;
}

/**
 * convert QVariant to QString, function reuses QVariant::toString()
 * but can process some types in a different way. E.g. boolean value
 * is encoded as 0/1 to be compatible with conventions used by sysfs
 * etc.
 *
 * @param v value to be converted
 *
 * @return resulting string
 */
QString valueEncode(QVariant const& v)
{
    switch(v.type()) {
    case QVariant::Bool:
        return v.toBool() ? "1" : "0";
    default:
        return v.toString();
    }
}

QVariant valueDefault(QVariant const& v)
{
    switch (v.type()) {
    case QVariant::String:
        return "";
    case QVariant::Int:
        return 0;
    case QVariant::UInt:
        return QVariant((unsigned)0);
    case QVariant::Double:
        return QVariant((double)0);
    case QVariant::Date:
        return QDate();
    case QVariant::DateTime:
        return QDateTime();
    default:
        return QVariant(v.type());
    }
}

static std::unique_ptr<QFile> fileFromName(const QString &name)
{
    return std::unique_ptr<QFile>{new QFile(getPath(name))};
}

WriterImpl::WriterImpl(const QString &name)
    : name_(name)
    , file_(fileFromName(name))
{
}

WriterImpl::~WriterImpl()
{
}

bool WriterImpl::exists() const
{
    return file_->exists();
}

QString const &WriterImpl::name() const
{
    return name_;
}

FileErrorNs::FileError WriterImpl::set(const QVariant &v)
{
    auto s = valueEncode(v);
    if (!file_->open(QIODevice::WriteOnly))
        return file_->error();

    QByteArray data{s.toUtf8().data()};
    if (file_->write(data) != data.size())
        return file_->error();

    file_->close();
    return FileErrorNs::NoError;
}

Writer::Writer(const QString &name)
    : impl(new WriterImpl(name))
{
}

Writer::~Writer()
{
}

bool Writer::exists() const
{
    return impl->exists();
}

QString Writer::name() const
{
    return impl->name();
}

FileErrorNs::FileError Writer::set(const QVariant &v)
{
    return impl->set(v);
}

/**
 * @}
 */

/**
 * @defgroup inout InOut Provider API
 *
 * @brief Support for updating properties supplied through InOut
 * StateFS loader (see statefs documentation for details)
 *
 * @class statefs::qt::InOutWriter
 *
 * @brief Thin wrapper used to update InOut property
 *
 * It updates property synchronously and does not provide any
 * sophisticated ways to handle temporary unavailability the input
 * property file.
 * 
 * @section inout_example Example: InOut supplier and consumer
 *
 * Configuration file:
 *
 * @include inout/statefs-inout-example.conf
 *
 * It should be registered using command like:
 *
 @verbatim
 statefs register --statefs-type=inout <path-to-conf>/statefs-inout-example.conf
 @endverbatim
 *
 * Supplier:
 *
 * @include inout/inout-writer.cpp
 *
 * It should be built with pkgconfig statefs-qt5 definitions.
 *
 * Consumer using Contextkit interfaces:
 *
 * @include inout/inout-reader.cpp
 *
 * statefs-qt5-examples.spec in the rpm dir demonstrates dependencies
 * and proper registration
 *
 * @{
 */

/**
 * Construct InOut property writer used to change corresponding
 * output property
 *
 * @param name full output property name (dot- or
 * slash-separated)
 */
InOutWriter::InOutWriter(const QString &name)
    : impl(new WriterImpl(QString("@") + name))
{
}

InOutWriter::~InOutWriter()
{
}

/**
 * can be used to check does corresponding input property exists at
 * the moment (it can be absent e.g. when statefs is not started yet
 * or is restarting)
 *
 * @return true if input property with this name exists
 */
bool InOutWriter::exists() const
{
    return impl->exists();
}

/**
 * @return output property name
 */
QString InOutWriter::name() const
{
    return impl->name().mid(1);
}

/**
 * set input property to supplied value, value is converted to string
 * using valueEncode(). Output property will be updated if input
 * property exists and writable.
 *
 * @param v value to be set
 *
 * @return file operation status
 */
FileErrorNs::FileError InOutWriter::set(const QVariant &v)
{
    return impl->set(v);
}

/**
 * @}
 */

}}
