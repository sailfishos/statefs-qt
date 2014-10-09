#ifndef _STATEFS_QT_CLIENT_HPP_
#define _STATEFS_QT_CLIENT_HPP_
/**
 * @file client.hpp
 * @brief Statefs Qt client interface
 * @copyright (C) 2013-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <QObject>
#include <QVariant>

namespace statefs { namespace qt {

class DiscretePropertyImpl;

class DiscreteProperty : public QObject
{
    Q_OBJECT;
public:
    DiscreteProperty(QString const &, QObject *parent = nullptr);
    ~DiscreteProperty();

signals:
    void changed(QVariant);
private:
    DiscretePropertyImpl *impl_;
};

class PropertyWriterImpl;

class PropertyWriter : public QObject
{
    Q_OBJECT;
public:
    PropertyWriter(QString const &, QObject *parent = nullptr);
    ~PropertyWriter();

    void set(QVariant);
signals:
    void updated(bool);
private:
    PropertyWriterImpl *impl_;
};

}}

#endif // _STATEFS_QT_CLIENT_HPP_
