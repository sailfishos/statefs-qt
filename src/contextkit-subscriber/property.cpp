#include "property.hpp"
#include <statefs/qt/util.hpp>

#include <cor/mt.hpp>
#include <cor/util.hpp>

#include <contextproperty.h>
#include <QDebug>
#include <QTimer>
#include <QSocketNotifier>
#include <QMutex>
#include <memory>
#include <poll.h>

namespace ckit
{

class Event : public QEvent
{
public:
    enum Type {
        Subscribe = QEvent::User,
        Unsubscribe,
        Subscribed
    };

    virtual ~Event();

protected:
    Event(Type t);
private:
    Event();

};

QMutex PropertyMonitor::actorGuard_;
PropertyMonitor::monitor_ptr PropertyMonitor::instance_;

PropertyMonitor::monitor_ptr PropertyMonitor::instance()
{
    monitor_ptr p = instance_;
    if (p)
        return p;

    QMutexLocker lock(&actorGuard_);

    if (instance_)
        return instance_;

    instance_.reset(new monitor_type([]() {
            return new ckit::PropertyMonitor();
            }));

    return instance_;
}


Event::Event(Event::Type t)
    : QEvent(static_cast<QEvent::Type>(t))
{}

Event::~Event() {}

class SubscribeRequest : public Event
{
public:
    SubscribeRequest(ContextPropertyPrivate const *tgt
                    , QString const&key
                    , std::promise<QVariant> &&res)
        : Event(Event::Subscribe)
        , tgt_(tgt)
        , key_(key)
        , value_(std::move(res))
    {}
    virtual ~SubscribeRequest() {}

    ContextPropertyPrivate const *tgt_;
    QString key_;
    std::promise<QVariant> value_;

private:
    SubscribeRequest(SubscribeRequest const&);
};


class UnsubscribeRequest : public Event
{
public:
    UnsubscribeRequest(ContextPropertyPrivate const *tgt
                    , QString const &key
                    , std::promise<void> &&done)
        : Event(Event::Unsubscribe)
        , tgt_(tgt)
        , key_(key)
        , done_(std::move(done))
    {}
    virtual ~UnsubscribeRequest() {}

    UnsubscribeRequest(SubscribeRequest const&) = delete;

    ContextPropertyPrivate const *tgt_;
    QString key_;
    std::promise<void> done_;
};

bool PropertyMonitor::event(QEvent *e)
{
    if (e->type() < QEvent::User)
        return QObject::event(e);

    auto t = static_cast<Event::Type>(e->type());
    try {
        switch (t) {
        case Event::Subscribe: {
            auto p = dynamic_cast<SubscribeRequest*>(e);
            if (p)
                subscribe(p);
            else
                qWarning() << "PropertyMonitor: !SubscribeRequest";
            break;
        }
        case Event::Unsubscribe: {
            auto p = dynamic_cast<UnsubscribeRequest*>(e);
            if (p)
                unsubscribe(p);
            else
                qWarning() << "PropertyMonitor: !UnsubscribeRequest";
            break;
        }
        default:
            return QObject::event(e);
        }
    } catch (std::exception const &ex) {
        qWarning() << "PropertyMonitor::event: Caught '"
                   << ex.what() << "' for " << t << "\n";
    }
    return true;
}

void PropertyMonitor::subscribe(SubscribeRequest *req)
{
    auto tgt = req->tgt_;
    auto key = req->key_;
    CKitProperty *handler;

    if (!tgt) {
        qWarning() << "Logic issue: subscription target is null";
        return;
    }

    auto it = targets_.find(key);
    if (it == targets_.end()) {
        it = targets_.insert(key, QSet<ContextPropertyPrivate const*>());
        it->insert(tgt);
        handler = add(key);
    } else {
        if (it->contains(tgt)) {
            return;
        }
        it->insert(tgt);
        handler = properties_[key];
    }
    // TODO when qt4 support will be removed
    //connect(handler, &CKitProperty::changed, tgt, &ContextPropertyPrivate::changed);
    connect(handler, SIGNAL(changed(QVariant)), tgt, SLOT(onChanged(QVariant)));

    auto v = handler->subscribe();
    req->value_.set_value(v);
    tgt->onChanged(v);
}

void PropertyMonitor::unsubscribe(UnsubscribeRequest *req)
{
    auto on_exit = cor::on_scope_exit([req]() {
            req->done_.set_value();
        });
    auto tgt = req->tgt_;
    auto key = req->key_;

    auto ptargets = targets_.find(key);
    if (ptargets == targets_.end())
        return;

    if (!ptargets->remove(tgt))
        return;

    auto phandlers = properties_.find(key);
    if (phandlers == properties_.end())
        return;

    auto handler = phandlers.value();

    // TODO when qt4 support will be removed
    // disconnect(handler, &CKitProperty::changed
    //           , tgt, &ContextPropertyPrivate::changed);
    disconnect(handler, SIGNAL(changed(QVariant)), tgt, SLOT(onChanged(QVariant)));
    if (ptargets->isEmpty()) {
        // last subscriber is gone
        targets_.erase(ptargets);
        properties_.erase(phandlers);
        handler->deleteLater();
    }
}

CKitProperty* PropertyMonitor::add(const QString &key)
{
    auto it = properties_.insert(key, new CKitProperty(key, this));
    return it.value();
}

CKitProperty::CKitProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
    , user_file_(statefs::qt::getPath(key))
    , sys_file_(statefs::qt::getSystemPath(key))
    , file_(&user_file_)
    , notifier_(nullptr)
    , reopen_interval_(100)
    , reopen_timer_(new QTimer(this))
    , is_subscribed_(false)
    , rate_(0)
    , now_(::time(nullptr))
    , max_rate_(20)
{
    reopen_timer_->setSingleShot(true);
    connect(reopen_timer_, SIGNAL(timeout()), this, SLOT(trySubscribe()));
}

CKitProperty::~CKitProperty()
{
    unsubscribe();
}

void CKitProperty::trySubscribe()
{
    static const int max_interval_ = 1000 * 60 * 3;
    static const int fast_interval_ = 1000 * 3;
    static const int slow_interval_ = 1000 * 30;
    if (tryOpen()) {
        reopen_interval_ = 500;
        subscribe();
        return;
    }

    if (reopen_interval_ < fast_interval_) {
        reopen_interval_ *= 2;
    } else if (reopen_interval_ < slow_interval_) {
        reopen_interval_ += fast_interval_;
    } else if (reopen_interval_ < max_interval_) {
        reopen_interval_ += slow_interval_;
    } else {
        reopen_interval_ = 1000 * 60 * 3;
    }
    reopen_timer_->start(reopen_interval_);
}

void CKitProperty::resubscribe()
{
    bool was_subscribed = is_subscribed_;
    unsubscribe();

    if (was_subscribed)
        subscribe();
}

bool CKitProperty::update()
{
    static const size_t cap = 31;
    bool is_updated = false;

    if (!tryOpen()) {
        qWarning() << "Can't open " << file_->fileName();
        cache_ = statefs::qt::valueDefault(cache_);
        resubscribe();
        return is_updated;
    }

    // WORKAROUND: file is just opened and closed before reading from
    // real source to make vfs (?) reread file data to cache
    QFile touchFile(file_->fileName());
    touchFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    file_->seek(0);
    auto size = file_->size();
    if (buffer_.size() < size)
        buffer_.resize(size + cap + 1);

    int rc = file_->read(buffer_.data(), size + cap);
    if (rc > size) {
        int read = 0;
        while (rc > 0) {
            read += rc;
            buffer_.resize(buffer_.size() + read + 1);
            rc = file_->read(buffer_.data() + read, read);
        }
        rc = read;
    }
    touchFile.close();
    if (rc >= 0) {
        buffer_[rc] = '\0';
        auto s = QString(buffer_);
        if (s.size()) {
            cache_ = statefs::qt::valueDecode(s);
        } else {
            if (cache_.isNull()) {
                cache_ = s;
            } else {
                cache_ = statefs::qt::valueDefault(cache_);
            }
        }
        is_updated = true;

        if (notifier_)
            notifier_->setEnabled(true);
    } else {
        qWarning() << "Error accessing? " << rc << "..." << file_->fileName();
        resubscribe();
    }
    return is_updated;
}


void CKitProperty::handleActivated(int)
{
    if (notifier_)
        notifier_->setEnabled(false);

    // there is an issue with Qt QSocketNotifier: it handles poll
    // error events in the same way as read events, so if property is
    // not pollable it will be reread with 0 timeout. Workaround is
    // check is rate goes above some reasonable limit and disable
    // polling if it is too high
    auto check_for_poll_err = [](int h) {
        pollfd fd;
        fd.fd = h;
        fd.events = 0;
        fd.revents = 0;
        auto rc = ::poll(&fd, 1, 30);
        if (!rc)
            return false;
        if (rc > 0)
            return (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
        else
            return true;
    };
    if (++rate_ > max_rate_) {
        auto now = ::time(nullptr);
        if (now == now_) {
            if (check_for_poll_err(file_->handle())) {
                qWarning() << "Unpollable file " << file_->fileName()
                           << " is polled, disabling handling";
                return;
            }
            auto fname = file_->fileName();
            if (rate_ < 200) {
                max_rate_ *= 2;
                qDebug() << "Increasing max rate for " << fname
                         << " to " << max_rate_;
            } else {
                qDebug() << "Rate for " << fname << "is very high, reseting";
                rate_ = 0;
            }
        } else {
            rate_ = 0;
            now_ = now;
        }
    }
    update();
    emit changed(cache_);
}

CKitProperty::OpenResult CKitProperty::tryOpen(QFile &f)
{
    if (f.isOpen())
        return Opened;

    if (!f.exists())
        return DoesntExists;

    f.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    if (!f.isOpen())
        return CantOpen;

    return Opened;
}

bool CKitProperty::tryOpen()
{
    OpenResult res0, res1;
    res0 = tryOpen(*file_);
    if (res0 == Opened)
        return true;

    file_ = (file_ == &user_file_) ? &sys_file_ : &user_file_;

    res1 = tryOpen(*file_);
    if (res1 == Opened)
        return true;

    auto info0 = (res0 == DoesntExists ? "no file" : "can't open");
    auto info1 = (res1 == DoesntExists ? "no file" : "can't open");
    QString f0, f1;
    if (file_ == &user_file_) {
        f0 = "Sys";
        f1 = "User";
    } else {
        f0 = "User";
        f1 = "Sys";
    }
    qWarning() << "Error accessing property " << key_ << ": "
               << f0 << ": " << info0 << ", " << f1 << ": " << info1;
    file_ = &user_file_;
    return false;
}

QVariant CKitProperty::subscribe()
{
    if (is_subscribed_)
        return cache_;

    if (!tryOpen()) {
        reopen_timer_->start(reopen_interval_);
        return QVariant();
    }
    is_subscribed_ = true;

    if (update())
        emit changed(cache_);
    notifier_.reset(new QSocketNotifier(file_->handle(), QSocketNotifier::Read));
    connect(notifier_.data(), SIGNAL(activated(int))
            , this, SLOT(handleActivated(int)));
    notifier_->setEnabled(true);
    return cache_;
}

void CKitProperty::unsubscribe()
{
    if (!is_subscribed_)
        return;

    is_subscribed_ = false;
    if (!file_->isOpen())
        return;

    if (notifier_) {
        notifier_->setEnabled(false);
        notifier_.reset();
    }
    file_->close();
}

}


ContextPropertyPrivate::ContextPropertyPrivate(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
    , state_(Unsubscribed)
    , is_cached_(false)
{
}

ContextPropertyPrivate::~ContextPropertyPrivate()
{
    unsubscribe();
}

QString ContextPropertyPrivate::key() const
{
    return key_;
}

QVariant ContextPropertyPrivate::value(const QVariant &defVal) const
{
    return is_cached_ ? cache_ : defVal;
}

QVariant ContextPropertyPrivate::value() const
{
    return value(QVariant());
}

ckit::PropertyMonitor::monitor_ptr ContextPropertyPrivate::actor()
{
    return ckit::PropertyMonitor::instance();
}

void ContextPropertyPrivate::onChanged(QVariant v) const
{
    if (state_ == Subscribing)
        state_ = Subscribed;

    update(v);
    emit valueChanged();
}

bool ContextPropertyPrivate::update(QVariant const &v) const
{
    if ((is_cached_ && v == cache_) || v.isNull())
        return false;

    cache_ = v;
    is_cached_ = true;
    return true;
}


const ContextPropertyInfo* ContextPropertyPrivate::info() const
{
    return nullptr; // TODO
}

void ContextPropertyPrivate::subscribe() const
{
    if (state_ != Unsubscribed)
        return;

    state_ = Subscribing;
    std::promise<QVariant> res;
    on_subscribed_ = res.get_future();
    auto ev = new ckit::SubscribeRequest(this, key_, std::move(res));
    actor()->postEvent(ev);
}

void ContextPropertyPrivate::unsubscribe() const
{
    if (state_ == Unsubscribed)
        return;

    try {
        std::promise<void> res;
        on_unsubscribed_ = res.get_future();
        auto ev = new ckit::UnsubscribeRequest(this, key_, std::move(res));
        actor()->postEvent(ev);
        on_unsubscribed_.wait_for(std::chrono::milliseconds(20000));
        state_ = Unsubscribed;
    } catch (std::exception const &e) {
        qWarning() << "unsubscribe: Caught '" << e.what() << "'\n";
    }
}


void ContextPropertyPrivate::waitForSubscription() const
{
    if (state_ == Subscribed)
        return;

    try {
        auto status = cor::wait_for(on_subscribed_, std::chrono::milliseconds(2000));
        if (status == std::future_status::ready) {
            update(on_subscribed_.get());
        }
        state_ = Subscribed;
    } catch (std::future_error const &e) {
        // skip, nothing can be done, it seems Qt has not delivered
        // QEvent to the target
    } catch (std::exception const &e) {
        qWarning() << "waitForSubscription: Caught '" << e.what() << "'\n";
    }
}

void ContextPropertyPrivate::waitForSubscription(bool block) const
{
    if (block)
        return waitForSubscription();

    while (state_ != Subscribed) {
        QCoreApplication::processEvents();
    }
    state_ = Subscribed;
}

void ContextPropertyPrivate::ignoreCommander()
{
}

void ContextPropertyPrivate::setTypeCheck(bool typeCheck)
{
}


ContextProperty::ContextProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , priv(new ContextPropertyPrivate(key, this))
{
    connect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
    priv->subscribe();
}

ContextProperty::~ContextProperty()
{
    disconnect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

QString ContextProperty::key() const
{
    return priv->key();
}

QVariant ContextProperty::value(const QVariant &def) const
{
    return priv->value(def);
}

QVariant ContextProperty::value() const
{
    return priv->value();
}

const ContextPropertyInfo* ContextProperty::info() const
{
    return priv->info();
}

void ContextProperty::subscribe () const
{
    return priv->subscribe();
}

void ContextProperty::unsubscribe () const
{
    return priv->unsubscribe();
}

void ContextProperty::waitForSubscription() const
{
    return priv->waitForSubscription();
}

void ContextProperty::waitForSubscription(bool block) const
{
    return priv->waitForSubscription(block);
}

void ContextProperty::ignoreCommander()
{
}

void ContextProperty::setTypeCheck(bool typeCheck)
{
}

#if QT_VERSION < 0x050000
void ContextProperty::onValueChanged() { }
#endif
