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

namespace ckit
{

static QMutex actorGuard;
static ckit::Actor<ckit::PropertyMonitor> *propertyMonitor = nullptr;
static bool isActorCreated = false;

Event::Event(Event::Type t)
    : QEvent(static_cast<QEvent::Type>(t))
{}

Event::~Event() {}


class PropertyRequest : public Event
{
public:
    PropertyRequest(Event::Type
                    , ContextPropertyPrivate const*
                    , QString const&
                    , std::function<void()>);
    virtual ~PropertyRequest();

    ContextPropertyPrivate const *tgt_;
    QString key_;
    std::function<void()> done_;
};

PropertyRequest::PropertyRequest(Event::Type t
                                 , ContextPropertyPrivate const *tgt
                                 , QString const &key
                                 , std::function<void()> done)
    : Event(t)
    , tgt_(tgt)
    , key_(key)
    , done_(done)
{}

PropertyRequest::~PropertyRequest()
{
    done_();
}

bool PropertyMonitor::event(QEvent *e)
{
    if (e->type() < QEvent::User)
        return QObject::event(e);

    auto t = static_cast<Event::Type>(e->type());
    switch (t) {
    case Event::Subscribe: {
        auto p = static_cast<PropertyRequest*>(e);
        subscribe(p->tgt_, p->key_);
        break;
    }
    case Event::Unsubscribe: {
        auto p = static_cast<PropertyRequest*>(e);
        unsubscribe(p->tgt_, p->key_);
        p->done_();
        break;
    }
    default:
        return QObject::event(e);
    }
    return true;
}

void PropertyMonitor::subscribe(ContextPropertyPrivate const *tgt, const QString &key)
{
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
    handler->subscribe();
}

void PropertyMonitor::unsubscribe
(ContextPropertyPrivate const *tgt, const QString &key)
{
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

void CKitProperty::trySubscribe() const
{
    if (tryOpen()) {
        reopen_interval_ = 100;
        return subscribe();
    }

    reopen_interval_ *= 2;
    if (reopen_interval_ > 1000 * 60 * 3)
        reopen_interval_ = 1000 * 60 * 3;

    reopen_timer_->start(reopen_interval_);
}

void CKitProperty::resubscribe() const
{
    bool was_subscribed = is_subscribed_;
    unsubscribe();

    if (was_subscribed)
        subscribe();
}

void CKitProperty::update()
{
    static const size_t cap = 31;

    if (!tryOpen()) {
        qWarning() << "Can't open " << file_.fileName();
        cache_ = statefs::qt::valueDefault(cache_);
        resubscribe();
        return;
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
            if (cache_.isNull())
                cache_ = s;
            else
                cache_ = statefs::qt::valueDefault(cache_);
        }

        if (notifier_)
            notifier_->setEnabled(true);
    } else {
        qWarning() << "Error accessing? " << rc << "..." << file_.fileName();
        resubscribe();
    }
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

bool CKitProperty::tryOpen() const
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

void CKitProperty::subscribe() const
{
    if (is_subscribed_)
        return;

    is_subscribed_ = true;
    if (!tryOpen())
        return reopen_timer_->start(reopen_interval_);

    notifier_.reset(new QSocketNotifier(file_.handle(), QSocketNotifier::Read));
    connect(notifier_.data(), SIGNAL(activated(int))
            , this, SLOT(handleActivated(int)));
    notifier_->setEnabled(true);
}

void CKitProperty::unsubscribe() const
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
    , is_subscribed_(false)
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
    using namespace ckit;
    if (isActorCreated)
        return propertyMonitor;
    QMutexLocker lock(&ckit::actorGuard);
    if (isActorCreated)
        return propertyMonitor;
    propertyMonitor = new ckit::Actor<ckit::PropertyMonitor>([]() {
            return new ckit::PropertyMonitor();
        });
    isActorCreated = true;
    return propertyMonitor;
}

void ContextPropertyPrivate::changed(QVariant v)
{
    if (v.isNull() || (is_cached_ && v == cache_))
        return;
    is_cached_ = true;
    cache_ = v;
    emit valueChanged();
}


const ContextPropertyInfo* ContextPropertyPrivate::info() const
{
    return nullptr; // TODO
}

void ContextPropertyPrivate::subscribe() const
{
    if (is_subscribed_)
        return;

    auto ev = new ckit::PropertyRequest
        (ckit::Event::Subscribe, this, key_, []() {});
    actor()->postEvent(ev);
    is_subscribed_ = true;
}

void ContextPropertyPrivate::unsubscribe() const
{
    if (!is_subscribed_)
        return;

    cor::Future future;

    auto ev = new ckit::PropertyRequest
        (ckit::Event::Unsubscribe, this, key_, future.waker());
    actor()->postEvent(ev);
    // wait until unsubscribed
    future.wait(std::chrono::milliseconds(20000));
    is_subscribed_ = false;
}

void ContextPropertyPrivate::waitForSubscription() const
{
}

void ContextPropertyPrivate::waitForSubscription(bool block) const
{
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
