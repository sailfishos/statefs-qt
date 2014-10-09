#ifndef _STATEFS_CKIT_PROPERTY_HPP_
#define _STATEFS_CKIT_PROPERTY_HPP_

#include "actor.hpp"

//#include <cor/mt.hpp>
#include <qtaround/mt.hpp>
#include <functional>
#include <future>

#include <QObject>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QByteArray>
#include <QMutex>
#include <QSet>
#include <QMap>
#include <QSocketNotifier>

class ContextPropertyInfo;
class QSocketNotifier;
class QTimer;


class ContextPropertyPrivate;

namespace ckit {


class Property : public QObject
{
    Q_OBJECT;
public:
    Property(QString const &key, QObject *parent);
    virtual ~Property();

    QVariant subscribe();
    void unsubscribe();

signals:
    void changed(QVariant) const;

private slots:
    void handleActivated(int);
    void trySubscribe();

private:
    bool tryOpen();
    enum OpenResult {
        Opened,
        DoesntExists,
        CantOpen
    };
    OpenResult tryOpen(QFile &);
    void resubscribe();
    QVariant subscribe_();

    bool update();

    QString key_;
    QFile user_file_;
    QFile sys_file_;
    QFile *file_;
    mutable QScopedPointer<QSocketNotifier> notifier_;
    QByteArray buffer_;
    mutable int reopen_interval_;
    mutable QTimer *reopen_timer_;
    bool is_subscribed_;
    QVariant cache_;
};

class SubscribeRequest;
class UnsubscribeRequest;

class PropertyMonitor : public QObject
{
    Q_OBJECT;
public:
    virtual bool event(QEvent *);

    typedef qtaround::mt::ActorHandle monitor_ptr;
    static monitor_ptr instance();
private:
    void subscribe(SubscribeRequest*);
    void unsubscribe(UnsubscribeRequest*);
    Property *add(const QString &);

    QMap<QString, QSet<ContextPropertyPrivate const*> > targets_;
    QMap<QString, Property*> properties_;

    static std::once_flag once_;
    static monitor_ptr instance_;
};

}

class ContextPropertyPrivate : public QObject
{
    Q_OBJECT;

public:
    explicit ContextPropertyPrivate(const QString &key, QObject *parent = 0);

    virtual ~ContextPropertyPrivate();

    QString key() const;
    QVariant value(const QVariant &def) const;
    QVariant value() const;

    const ContextPropertyInfo* info() const;

    void subscribe () const;
    void unsubscribe () const;

    void waitForSubscription() const;
    void waitForSubscription(bool block) const;

    static void ignoreCommander();
    static void setTypeCheck(bool typeCheck);

signals:
    void valueChanged() const;

public slots:
    void onChanged(QVariant) const;
private:

    enum State {
        Initial,
        Unsubscribing,
        Subscribing,
        Subscribed
    };

    bool update(QVariant const&) const;

    static ckit::PropertyMonitor::monitor_ptr actor();
    QString key_;
    mutable State state_;
    mutable bool is_cached_;
    mutable QVariant cache_;

    mutable std::future<QVariant> on_subscribed_;
    mutable std::future<void> on_unsubscribed_;
};

#endif // _STATEFS_CKIT_PROPERTY_HPP_
