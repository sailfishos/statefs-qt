#ifndef _STATEFS_QML_PROPERTY_HPP_
#define _STATEFS_QML_PROPERTY_HPP_
/**
 * @file qml/property.hpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <QObject>
#include <QVariant>
#include <QString>
#include <QQmlParserStatus>

#include <statefs/qt/client.hpp>
#include <qtaround/util.hpp>

/**
 * Declarative component providing access to the system/session state
 * properties.
 *
 * It replaces deprecated ContextProperty component. Name is set to
 * reflect the origin of information.
 *
 * Comparing to the ContextProperty the following features are added:
 *
 * - refresh() method adds ability to access continious properties:
 *   ones changed constantly (they can't be polled, so they should be
 *   read explicitely)
 *
 * - value can be set - to be used with properties allowing to change
 *   the state
 *
 * Comparing to ContextProperty there are following changes:
 *
 * - if subscribed property is not set explicitely, component
 *   subscribes for changes only when it is fully instantiated by Qml
 *   engine (on componentComplete()), so there will not be any
 *   redundant un/subscription cycles during initialization
 * 
 * - redundant un/subscribe() methods are removed
 */
class StateProperty : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "value")
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QString key
               READ getKey
               WRITE setKey)

    Q_PROPERTY(QVariant value
               READ getValue
               WRITE setValue
               NOTIFY valueChanged)

    Q_PROPERTY(bool subscribed
               READ getSubscribed
               WRITE setSubscribed
               NOTIFY subscribedChanged)

public:
    StateProperty(QObject* parent = 0);
    ~StateProperty();

    StateProperty(StateProperty const &) = delete;
    StateProperty& operator = (StateProperty const &) = delete;

    QString getKey() const;
    void setKey(QString);

    QVariant getValue() const;
    void setValue(QVariant);

    bool getSubscribed() const;
    void setSubscribed(bool);

public slots:
    void refresh() const;

signals:
    void valueChanged();
    void subscribedChanged();

private slots:
    void onValueChanged(QVariant);

protected:
    // QQmlParserStatus
    virtual void classBegin();
    virtual void componentComplete();

private:
    void updateImpl();
    
    QString key_;
    QVariant value_;
    enum class State { Unknown, Subscribed, Unsubscribed };
    State state_;
    UNIQUE_PTR(statefs::qt::DiscreteProperty) impl_;
    UNIQUE_PTR(statefs::qt::PropertyWriter) writer_;
};

#endif // _STATEFS_QML_PROPERTY_HPP_
