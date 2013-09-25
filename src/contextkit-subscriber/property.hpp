#ifndef _STATEFS_CKIT_PROPERTY_HPP_
#define _STATEFS_CKIT_PROPERTY_HPP_

#include <cor/mt.hpp>
#include <functional>
#include <future>

#include <QObject>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QScopedPointer>
#include <QByteArray>
#include <QWaitCondition>
#include <QThread>
#include <QMutex>
#include <QCoreApplication>
#include <QSet>
#include <QMap>
#include <QSocketNotifier>

class ContextPropertyInfo;
class QSocketNotifier;
class QTimer;


class ContextPropertyPrivate;

namespace ckit {


class CKitProperty : public QObject
{
    Q_OBJECT;
public:
    CKitProperty(QString const &key, QObject *parent);
    virtual ~CKitProperty();

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
    unsigned rate_;
    time_t now_;
    unsigned max_rate_;
};

class Actor_ : public QThread
{
    Q_OBJECT;
protected:
    Actor_(QObject *parent) : QThread(parent) {}
    virtual ~Actor_() {}
};


template <typename T>
class Actor : public Actor_
{
public:
    Actor(std::function<T*()> ctor, QObject *parent = nullptr)
        : Actor_(parent)
        , ctor_(ctor)
    {
        mutex_.lock();
        start();
        cond_.wait(&mutex_);
        mutex_.unlock();
    }

    void run()
    {
        obj_.reset(ctor_());
        mutex_.lock();
        cond_.wakeAll();
        mutex_.unlock();
        exec();
        obj_.reset(nullptr);
    }

    inline void postEvent(QEvent *e)
    {
        QCoreApplication::postEvent(obj_.data(), e);
    }

    inline bool sendEvent(QEvent const *e)
    {
        return QCoreApplication::sendEvent(obj_.data(), e);
    }

private:
    QWaitCondition cond_;
    QMutex mutex_;
    std::function<T*()> ctor_;
    QScopedPointer<T> obj_;
};

class SubscribeRequest;
class UnsubscribeRequest;

class PropertyMonitor : public QObject
{
    Q_OBJECT;
public:
    virtual bool event(QEvent *);

    static ckit::Actor<ckit::PropertyMonitor> * instance();
private:
    void subscribe(SubscribeRequest*);
    void unsubscribe(UnsubscribeRequest*);
    CKitProperty *add(const QString &);

    QMap<QString, QSet<ContextPropertyPrivate const*> > targets_;
    QMap<QString, CKitProperty*> properties_;

    static QMutex actorGuard_;
    static ckit::Actor<PropertyMonitor> *propertyMonitor_;
    static bool isActorCreated_;

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

    void valueChanged();

public slots:
    void changed(QVariant);
private:

    enum State {
        Subscribing,
        Subscribed,
        Unsubscribed
    };

    bool update(QVariant const&) const;

    static ckit::Actor<ckit::PropertyMonitor> *actor();
    QString key_;
    mutable State state_;
    mutable bool is_cached_;
    mutable QVariant cache_;

    mutable std::future<QVariant> on_subscribed_;
    mutable std::future<void> on_unsubscribed_;
};

#endif // _STATEFS_CKIT_PROPERTY_HPP_
