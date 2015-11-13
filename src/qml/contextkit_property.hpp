#ifndef _CONTEXTKIT_QML_PROPERTY_HPP_
#define _CONTEXTKIT_QML_PROPERTY_HPP_
/**
 * @file qml/context_property.hpp
 * @brief Contextkit property binding
 * @copyright (C) 2012-2015 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <QObject>
#include <QVariant>
#include <QString>
#include <QQmlParserStatus>

class ContextProperty;

class ContextPropertyDeclarative : public QObject, public QQmlParserStatus
{
    Q_OBJECT;
    Q_PROPERTY(QString key READ getKey WRITE setKey
               NOTIFY keyChanged);
    Q_PROPERTY(QVariant value READ getValue WRITE setDefaultValue
               NOTIFY valueChanged);
    Q_PROPERTY(bool subscribed READ getSubscribed WRITE setSubscribed
               NOTIFY subscribedChanged)
    Q_CLASSINFO("DefaultProperty", "value");
    Q_INTERFACES(QQmlParserStatus)

public:
    ContextPropertyDeclarative(QObject* parent = 0);
    ~ContextPropertyDeclarative();

    QString getKey() const;
    void setKey(QString const&);

    QVariant getValue() const;
    void setDefaultValue(QVariant const&);

    bool getSubscribed() const;
    void setSubscribed(bool subscribed);

    Q_INVOKABLE void subscribe();
    Q_INVOKABLE void unsubscribe();

signals:
    void valueChanged();
    void subscribedChanged();
    void keyChanged();

protected:
    // QQmlParserStatus
    virtual void classBegin() {}
    virtual void componentComplete();

private:

    void updateImpl();

    QString key_;
    QVariant default_value_;
    enum class State { Unknown, Subscribed, Unsubscribed };
    State state_;
    ContextProperty *impl_;
};

#endif
