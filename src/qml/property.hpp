#ifndef _STATEFS_QML_PROPERTY_HPP_
#define _STATEFS_QML_PROPERTY_HPP_
/**
 * @file property.cpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <QObject>
#include <QVariant>
#include <QString>
#include <statefs/qt/client.hpp>
#include <qtaround/util.hpp>

class StateProperty : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "value")

    Q_PROPERTY(QString key
               READ getKey
               WRITE setKey)

    Q_PROPERTY(QVariant value
               READ getValue
               WRITE setValue
               NOTIFY valueChanged)

    Q_PROPERTY(bool active
               READ getActive
               WRITE setActive
               NOTIFY activeChanged)

public:
    StateProperty(QObject* parent = 0);
    ~StateProperty();

    StateProperty(StateProperty const &) = delete;
    StateProperty& operator = (StateProperty const &) = delete;

    QString getKey() const;
    void setKey(QString);

    QVariant getValue() const;
    void setValue(QVariant);

    bool getActive() const;
    void setActive(bool);

public slots:
    void refresh() const;

signals:
    void valueChanged();
    void activeChanged();

private slots:
    void onValueChanged(QVariant);

private:
    void updateImpl();
    
    QString key_;
    QVariant value_;
    bool isActive_;
    UNIQUE_PTR(statefs::qt::DiscreteProperty) impl_;
    UNIQUE_PTR(statefs::qt::PropertyWriter) writer_;
};

#endif // _STATEFS_QML_PROPERTY_HPP_
