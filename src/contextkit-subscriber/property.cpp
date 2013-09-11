#include "property.hpp"
#include <statefs/qt/util.hpp>

#include <cor/mt.hpp>

#include <contextproperty.h>
#include <QDebug>
#include <QTimer>
#include <QSocketNotifier>
#include <QMutex>
#include <memory>
#include <poll.h>


// TODO move to cor
// gcc 4.6 future::wait_for() returns bool instead of future_status
// adding forward-compatibility

template <class ResT> struct ChooseRightFuture {};

template <>
struct ChooseRightFuture<bool> {

    template <class T, class Rep, class Period>
    std::future_status wait_for
    (std::future<T> const &future
     , const std::chrono::duration<Rep,Period>& timeout) {
        bool status = future.wait_for(timeout);
        return status ? std::future_status::ready : std::future_status::timeout;
    }
};

template <>
struct ChooseRightFuture<std::future_status> {

    template <class T, class Rep, class Period>
    std::future_status wait_for
    (std::future<T> const &future
     , const std::chrono::duration<Rep,Period>& timeout) {
        return future.wait_for(timeout);
    }
};

template<class T, class Rep, class Period>
std::future_status wait_for
(std::future<T> const &future, const std::chrono::duration<Rep,Period>& timeout)
{
    auto impl = ChooseRightFuture<decltype(future.wait_for(timeout))>();
    return impl.template wait_for<>(future, timeout);
}


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
ckit::Actor<ckit::PropertyMonitor> *PropertyMonitor::propertyMonitor_ = nullptr;
bool PropertyMonitor::isActorCreated_ = false;

ckit::Actor<ckit::PropertyMonitor> * PropertyMonitor::instance()
{
    if (isActorCreated_)
        return propertyMonitor_;

    QMutexLocker lock(&actorGuard_);

    if (isActorCreated_)
        return propertyMonitor_;

    propertyMonitor_ = new ckit::Actor<ckit::PropertyMonitor>([]() {
            return new ckit::PropertyMonitor();
        });

    isActorCreated_ = true;
    return propertyMonitor_;
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
    connect(handler, SIGNAL(changed(QVariant)), tgt, SLOT(changed(QVariant)));

    auto v = handler->subscribe();
    req->value_.set_value(v);
}

void PropertyMonitor::unsubscribe(UnsubscribeRequest *req)
{
    auto tgt = req->tgt_;
    auto key = req->key_;

    auto t_it = targets_.find(key);
    if (t_it == targets_.end())
        return;

    auto tgt_set = t_it.value();
    auto ptgt = tgt_set.find(tgt);
    if (ptgt == tgt_set.end())
        return;

    auto h_it = properties_.find(key);
    if (h_it == properties_.end())
        return;

    auto handler = h_it.value();

    // TODO when qt4 support will be removed
    // disconnect(handler, &CKitProperty::changed
    //           , tgt, &ContextPropertyPrivate::changed);
    disconnect(handler, SIGNAL(changed(QVariant)), tgt, SLOT(changed(QVariant)));
    tgt_set.erase(ptgt);
    if (!tgt_set.isEmpty())
        return;

    targets_.erase(t_it);
    properties_.erase(h_it);
    handler->deleteLater();
    req->done_.set_value();
}

CKitProperty* PropertyMonitor::add(const QString &key)
{
    auto it = properties_.insert(key, new CKitProperty(key, this));
    return it.value();
}

CKitProperty::CKitProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
    , file_(statefs::qt::getPath(key))
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
    if (tryOpen()) {
        reopen_interval_ = 100;
        subscribe();
        return;
    }

    reopen_interval_ *= 2;
    if (reopen_interval_ > 1000 * 60 * 3)
        reopen_interval_ = 1000 * 60 * 3;

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
        qWarning() << "Can't open " << file_.fileName();
        cache_ = statefs::qt::valueDefault(cache_);
        resubscribe();
        return is_updated;
    }

    // WORKAROUND: file is just opened and closed before reading from
    // real source to make vfs (?) reread file data to cache
    QFile touchFile(file_.fileName());
    touchFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    file_.seek(0);
    auto size = file_.size();
    if (buffer_.size() < size)
        buffer_.resize(size + cap + 1);

    int rc = file_.read(buffer_.data(), size + cap);
    if (rc > size) {
        int read = 0;
        while (rc > 0) {
            read += rc;
            buffer_.resize(buffer_.size() + read + 1);
            rc = file_.read(buffer_.data() + read, read);
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
        qWarning() << "Error accessing? " << rc << "..." << file_.fileName();
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
            if (check_for_poll_err(file_.handle())) {
                qWarning() << "Unpollable file " << file_.fileName()
                           << " is polled, disabling handling";
                return;
            }
            auto fname = file_.fileName();
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

bool CKitProperty::tryOpen()
{
    if (file_.isOpen())
        return true;

    if (!file_.exists()) {
        qWarning() << "No property file " << file_.fileName();
        return false;
    }
    file_.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    if (!file_.isOpen()) {
        qWarning() << "Can't open " << file_.fileName();
        return false;
    }
    return true;
}

QVariant CKitProperty::subscribe()
{
    if (is_subscribed_)
        return cache_;

    is_subscribed_ = true;
    if (!tryOpen()) {
        reopen_timer_->start(reopen_interval_);
        return QVariant();
    }

    if (update())
        emit changed(cache_);
    notifier_.reset(new QSocketNotifier(file_.handle(), QSocketNotifier::Read));
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
    if (!file_.isOpen())
        return;

    if (notifier_) {
        notifier_->setEnabled(false);
        notifier_.reset();
    }
    file_.close();
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

ckit::Actor<ckit::PropertyMonitor> * ContextPropertyPrivate::actor()
{
    return ckit::PropertyMonitor::instance();
}

void ContextPropertyPrivate::changed(QVariant v)
{
    if (state_ == Subscribing)
        state_ = Subscribed;

    if (update(v))
        emit valueChanged();
}

bool ContextPropertyPrivate::update(QVariant const &v) const
{
    if (v.isNull() || (is_cached_ && v == cache_))
        return false;

    is_cached_ = true;
    cache_ = v;
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

    // wait until unsubscribed
    try {
        auto status = wait_for(on_subscribed_, std::chrono::milliseconds(5000));
        if (status == std::future_status::ready) {
            update(on_subscribed_.get());
        }
        state_ = Subscribed;
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
