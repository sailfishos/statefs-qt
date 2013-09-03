#ifndef _STATEFS_QT_UTIL_HPP_
#define _STATEFS_QT_UTIL_HPP_
/**
 * @file util.hpp
 * @brief Statefs utilities for Qt-based apps and libraries
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>

#include <errno.h>
#include <memory>

namespace statefs { namespace qt {

#if QT_VERSION < 0x050000
typedef QFile FileErrorNs;
#else
typedef QFileDevice FileErrorNs;
#endif

class WriterImpl;

/**
 * @addtogroup util
 *
 * @{
 */

class Writer
{
public:
    Writer(const QString&);
    virtual ~Writer();

    bool exists() const;
    QString name() const;
    FileErrorNs::FileError set(const QVariant&);
protected:
    mutable std::unique_ptr<WriterImpl> impl;
};

bool splitPropertyName(const QString &, QStringList &);
QString getPath(const QString &);

QVariant valueDecode(QString const&);
QString valueEncode(QVariant const&);
QVariant valueDefault(QVariant const&);

/// @}

/**
 * @addtogroup inout
 *
 * @{
 */

class InOutWriter
{
public:
    InOutWriter(const QString&);
    virtual ~InOutWriter();

    bool exists() const;
    QString name() const;
    FileErrorNs::FileError set(const QVariant&);
protected:
    mutable std::unique_ptr<WriterImpl> impl;
};

/// @}

}}

#endif // _STATEFS_QT_UTIL_HPP_
