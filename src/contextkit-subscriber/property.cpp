#include "property.hpp"
#include <statefs/qt/util.hpp>
#include <statefs/qt/client.hpp>

#include <cor/mt.hpp>
#include <cor/util.hpp>

#include <qtaround/util.hpp>

#include <contextproperty.h>
#include <QDebug>
#include <QTimer>
#include <QSocketNotifier>
#include <QMutex>
#include <memory>
#include <poll.h>

namespace debug = qtaround::debug;

class ContextPropertyPrivateHandle
{
public:
    ContextPropertyPrivateHandle(QString const &key)
        : impl_(new ContextPropertyPrivate(key))
    {}
    virtual ~ContextPropertyPrivateHandle()
    {
        impl_->detach();
    }
protected:
    ContextPropertyPrivate *impl_;
};

namespace {

// to wrap code to be executed from the code which does not support
// exception handling
template <typename F, typename OnFutureT>
void execute_nothrow_process_future_error
(F fn, char const *msg, OnFutureT on_future_error)
{
    try{
        fn();
    } catch (std::future_error const &e) {
        on_future_error(e);
    } catch (std::exception const &e) {
        debug::warning("Ignoring exception: ", e.what(), ". ", msg);
    // } catch (...) {
    //     // Those exceptions should not be catched, let's abort. Maybe
    //     // this is something bad
    //     qWarning() << "Unknown exception, ignoring. " << msg;
    }
}

template <typename F>
void execute_nothrow(F &&fn, char const *msg)
{
    execute_nothrow_process_future_error
        (std::forward<F>(fn), msg, [msg](std::future_error const &e) {
            // skip, nothing can be done, maybe Qt has not delivered
            // QEvent to the target, maybe something else is happened.
            debug::warning("Future error: ", e.code().value()
                           , ":", e.what(), ". ", msg);
        });
}

template <typename T>
T *event_cast(char const *src, char const *type_name, QEvent *e)
{
    auto res = dynamic_cast<T*>(e);
    if (!res)
        debug::warning(src, "Event", e->type(), "isn't", type_name);
    return res;
}

#define EVENT_CAST(ev, ev_type) event_cast<ev_type> \
    ((char const*)__FUNCTION__, (char const*)#ev_type, ev)

}

namespace statefs { namespace qt {

// TODO now for simplicity it is implemented using
// ContextPropertyPrivate while maybe it should be done in the
// contrary way
class DiscretePropertyImpl : public QObject
                           , private ContextPropertyPrivateHandle
{
    Q_OBJECT;
public:
    DiscretePropertyImpl(QString const &, QObject *parent = nullptr);
    ~DiscretePropertyImpl();

    void refresh() const;

signals:
    void changed(QVariant);
private slots:
    void onChanged();
};

class PropertyWriterImpl : public QObject
{
    Q_OBJECT;
public:
    PropertyWriterImpl(QString const &);
    ~PropertyWriterImpl() {}

    void set(QVariant &&);
    virtual bool event(QEvent *);

signals:
    void updated(bool);
private:
    friend class PropertyWriter;
    void detach();
    QSharedPointer<PropertyWriterImpl> handle_;
    QString key_;
};

}}


namespace statefs { namespace qt {

class Event : public QEvent
{
public:
    enum Type {
        Subscribe = QEvent::User,
        Unsubscribe,
        Subscribed,
        Write,
        Refresh,
        Data,
        WriteStatus
    };

    virtual ~Event();

protected:
    Event(Type t);
private:
    Event();

};

void File::close()
{
    if (file_) {
        file_.reset();
        failures_count_ = 0;
        type_ = User;
    }
}

qint64 File::read(QByteArray &dst, size_t size, size_t offset)
{
    qint64 res = 0;
    if (file_) {
        if ((size_t)dst.size() < offset + size) {
            debug::warning("Logical error: wrong dst QByteArray size");
        } else {
            res = file_->read(dst.data() + offset, size);
        }
    }
    return res;
}

File::File(QString const &key)
    : type_(File::User)
    , key_(key)
    , failures_count_(0)
{
}

QString File::getError(file_ptr const &f) const
{
    return (!f
            ? "No file"
            : (f->isOpen()
               ? QString()
               : (!f->exists()
                  ? "No file"
                  : "No access")));
}

QString File::nameFor(Type t) const
{
    return (t == File::User
            ? statefs::qt::getPath(key_)
            : statefs::qt::getSystemPath(key_));
}

File::files_type File::openNew(QIODevice::OpenMode mode)
{
    auto open = [](QIODevice::OpenMode mode, QString const &name) {
        auto res = cor::make_unique<QFile>(name);
        res->open(mode);
        if (res->isOpen())
            debug::debug("Opened", res->fileName());
        return std::move(res);
    };
    files_type res;
    auto f = open(mode, nameFor(type_));
    if (!f->isOpen()) {
        type_ = (type_ == System ? User : System);
        f = open(mode, nameFor(type_));
        if (!f->isOpen())
            f.reset();
    }
    res[type_] = std::move(f);
    return std::move(res);
}

bool File::tryOpen(QIODevice::OpenMode mode)
{
    if (file_) {
        if (!file_->isOpen()) {
            debug::warning("Property", key_, "is missed"
                           , getError(file_));
            close();
        }
    }
    if (!file_) {
        auto files = openNew(mode);
        if (files[type_]) {
            file_ = std::move(files[type_]);
            failures_count_ = 0;
        } else {
            type_ = User;
            if (!failures_count_++) {
                debug::warning("Can't open property", key_
                               , "Sys:", getError(files[System])
                               , "User:", getError(files[User]));
            } else {
                debug::info("Failed try #", failures_count_, "to access", key_);
            }
        }
    }
    return !!file_;
}

void FileReader::close()
{
    notifier_.reset();
    File::close();
}

bool FileWriter::write(QByteArray const &data)
{
    bool res = false;
    QString reason;
    if (!file_) {
        reason = "File is not opened for " + key();
    } else {
        auto len = file_->write(data);
        if (len == data.size()) {
            res = true;
        } else {
            reason = QString("Wrong len returned: %1 (vs %2) for %3. Error '%4'")
                .arg(len).arg(data.size()).arg(file_->fileName())
                .arg(file_->error());
        }
    }
    if (!res)
        debug::warning("Failed to write: ", reason);

    return res;
}

std::once_flag PropertyMonitor::once_;
PropertyMonitor::monitor_ptr PropertyMonitor::instance_;

PropertyMonitor::monitor_ptr PropertyMonitor::instance()
{
    namespace mt = qtaround::mt;
    std::call_once(once_, []() {
            using statefs::qt::PropertyMonitor;
            auto ctor = []() { return make_qobject_unique<PropertyMonitor>(); };
            instance_ = mt::startActorSync<PropertyMonitor>(ctor);
            mt::deleteOnApplicationExit(instance_);
        });
    return instance_;
}


Event::Event(Event::Type t)
    : QEvent(static_cast<QEvent::Type>(t))
{}

Event::~Event() {}

class ReplyEvent : public Event
{
public:
    ReplyEvent(Event::Type t, target_handle const &tgt)
        : Event(t)
        , tgt_(tgt)
    {}

    target_handle tgt_;
};

class DataReplyEvent : public ReplyEvent
{
public:
    DataReplyEvent(QVariant v, target_handle const &tgt)
        : ReplyEvent(Event::Data, tgt)
        , data_(std::move(v))
    {}

    QVariant data_;
};

class SubscribeRequest : public Event
{
public:
    SubscribeRequest(target_handle tgt
                    , QString const&key
                    , std::promise<QVariant> &&res)
        : Event(Event::Subscribe)
        , tgt_(tgt)
        , key_(key)
        , value_(std::move(res))
    {}
    virtual ~SubscribeRequest();

    target_handle tgt_;
    QString key_;
    std::promise<QVariant> value_;
    QVariant result;

private:
    SubscribeRequest(SubscribeRequest const&);
};

SubscribeRequest::~SubscribeRequest()
{
    auto notify_fn = [this]() {
        tgt_->postEvent(new DataReplyEvent(result, tgt_));
        value_.set_value(result);
    };
    execute_nothrow(notify_fn, __PRETTY_FUNCTION__);
}

class UnsubscribeRequest : public Event
{
public:
    UnsubscribeRequest(target_handle tgt
                    , QString const &key
                    , std::promise<void> &&done)
        : Event(Event::Unsubscribe)
        , tgt_(tgt)
        , key_(key)
        , done_(std::move(done))
    {}
    virtual ~UnsubscribeRequest() {
        done_.set_value();
    }

    target_handle tgt_;
    QString key_;
    std::promise<void> done_;
};

using statefs::qt::PropertyWriterImpl;
class WriteReply : public Event
{
public:
    WriteReply(QSharedPointer<PropertyWriterImpl> const &hold
                , bool is_updated)
        : Event(Event::WriteStatus)
        , hold_(hold)
        , is_updated_(is_updated)
    { }

    QSharedPointer<PropertyWriterImpl> hold_;
    bool is_updated_;
};

class WriteRequest : public Event
{
public:
    WriteRequest(QSharedPointer<PropertyWriterImpl> const &tgt
                 , QString const &key
                 , QVariant &&value)
        : Event(Event::Write)
        , tgt_(tgt)
        , key_(key)
        , value_(std::move(value))
    { }

    void updated(bool is_updated)
    {
        QEvent *p = (QEvent*)new WriteReply(tgt_, is_updated);
        QCoreApplication::postEvent
            (tgt_.data(), p);
    }

    QSharedPointer<PropertyWriterImpl> tgt_;
    QString key_;
    QVariant value_;
};

class RefreshRequest : public Event
{
public:
    RefreshRequest(target_handle tgt
                    , QString const &key)
        : Event(Event::Refresh)
        , tgt_(tgt)
        , key_(key)
    {}
    virtual ~RefreshRequest() {}

    target_handle tgt_;
    QString key_;
};

bool PropertyMonitor::event(QEvent *e)
{
    if (e->type() < QEvent::User)
        return QObject::event(e);

    auto res = true;
    auto fn = [this, e, &res]() {
        auto t = static_cast<Event::Type>(e->type());
        switch (t) {
        case Event::Subscribe: {
            auto p = EVENT_CAST(e, SubscribeRequest);
            if (p) subscribe(p);
            break;
        }
        case Event::Unsubscribe: {
            auto p = EVENT_CAST(e, UnsubscribeRequest);
            if (p) unsubscribe(p);
            break;
        }
        case Event::Write: {
            auto p = EVENT_CAST(e, WriteRequest);
            if (p) write(p);
            break;
        }
        case Event::Refresh: {
            auto p = EVENT_CAST(e, RefreshRequest);
            if (p) refresh(p);
            break;
        }
        default:
            debug::warning("Unknown user event");
            res = QObject::event(e);
        }
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
    return res;
}

void PropertyMonitor::write(WriteRequest *req)
{
    auto isOk = false;
    auto emit_on_exit = cor::on_scope_exit([req, isOk]() {
            emit req->updated(isOk);
        });
    // implementation is quick and dirty: one redundant try to access
    // session(user) file
    FileWriter dst(req->key_);
    if (dst.tryOpen()) {
        auto s = statefs::qt::valueEncode(req->value_);
        auto data = s.toUtf8();
        isOk = dst.write(s.toUtf8());
    } else {
        debug::warning("Can't access", req->key_);
    }
}

void Property::changed(QVariant const &v) const
{
    for (auto tgt : targets_)
        tgt->postEvent(new DataReplyEvent(v, tgt));
}

bool Property::add(target_handle const &target)
{
    auto res = false;
    auto it = targets_.find(target);
    if (it == targets_.end()) {
        targets_.insert(target);
        res = true;
    }
    return res;
}

Property::Removed Property::remove(target_handle const &target)
{
    auto res = Removed::No;
    auto it = targets_.find(target);
    if (it != targets_.end()) {
        targets_.erase(it);
        res = (targets_.size() ? Removed::Yes : Removed::Last);
    }
    return res;
}

void PropertyMonitor::subscribe(SubscribeRequest *req)
{
    auto tgt = req->tgt_;
    auto key = req->key_;
    Property *handler;
    QVariant retval;

    if (!tgt) {
        debug::warning("Logic issue: subscription target is null");
        return;
    }

    if (key.isEmpty()) {
        debug::warning("Empty contextkit key");
        return;
    }

    auto it = properties_.find(key);
    if (it == properties_.end()) {
        handler = add(key);
    } else {
        handler = it.value();
    }
    handler->add(tgt);

    req->result = handler->subscribe();
}

void PropertyMonitor::unsubscribe(UnsubscribeRequest *req)
{
    auto tgt = req->tgt_;
    auto key = req->key_;

    auto phandlers = properties_.find(key);
    if (phandlers == properties_.end())
        return;

    auto handler = phandlers.value();

    if (handler->remove(tgt) == Property::Removed::Last) {
        // last subscriber is gone
        properties_.erase(phandlers);
        handler->deleteLater();
    }
}

void PropertyMonitor::refresh(RefreshRequest *req)
{
    auto key = req->key_;
    auto phandlers = properties_.find(key);
    if (phandlers == properties_.end())
        return;

    auto handler = phandlers.value();
    handler->update();
}

Property* PropertyMonitor::add(const QString &key)
{
    auto it = properties_.insert(key, new Property(key, this));
    return it.value();
}

Property::Property(const QString &key, QObject *parent)
    : QObject(parent)
    , file_(key)
    , reopen_interval_(100)
    , reopen_timer_(new QTimer(this))
    , is_subscribed_(false)
{
    reopen_timer_->setSingleShot(true);
    connect(reopen_timer_, SIGNAL(timeout()), this, SLOT(trySubscribe()));
}

Property::~Property()
{
    unsubscribe();
}

void Property::trySubscribe()
{
    static const int max_interval_ = 1000 * 60 * 3;
    static const int fast_interval_ = 1000 * 3;
    static const int slow_interval_ = 1000 * 30;
    if (file_.tryOpen()) {
        reopen_interval_ = 500;
        subscribe_();
        return;
    }

    if (reopen_interval_ < fast_interval_) {
        reopen_interval_ *= 2;
    } else if (reopen_interval_ < slow_interval_) {
        reopen_interval_ += fast_interval_;
    } else if (reopen_interval_ < max_interval_) {
        reopen_interval_ += slow_interval_;
    } else {
        reopen_interval_ = max_interval_;
    }
    reopen_timer_->start(reopen_interval_);
}

void Property::resubscribe()
{
    if (is_subscribed_) {
        unsubscribe();
        subscribe_();
    }
}

bool Property::update()
{
    // 1MB?
    static const size_t max_statefs_file_size = 1024 * 1024;

    static const size_t cap = 31;
    bool is_updated = false;

    if (!file_.tryOpen()) {
        debug::warning("Can't open ", file_.fileName());
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
    // statefs file size can change, so need to read more and check:
    // if amount of data read > size, continue to read. Also readAll()
    // is not suitable for the same reason
    auto to_read = size + cap;
    if (buffer_.size() < to_read + 1)
        buffer_.resize(to_read + 1 /* for \0 */);

    auto rc = file_.read(buffer_, to_read);
    // read all data
    if (rc > size) {
        int bytes_read = 0;
        while (rc > 0) {
            bytes_read += rc;
            if ((size_t)bytes_read > max_statefs_file_size) {
                debug::warning("File size for " + file_.fileName() +
                               "reached max ", max_statefs_file_size);
                break;
            }
            // maybe there is more data to read
            buffer_.resize(buffer_.size() + bytes_read + 1);
            rc = file_.read(buffer_, bytes_read, bytes_read);
        }
        rc = bytes_read;
    }
    touchFile.close();
    if (rc >= 0) {
        buffer_[(int)rc] = '\0';
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
    } else {
        debug::warning("Error accessing? ", rc, "..." + file_.fileName());
        resubscribe();
    }
    return is_updated;
}


void Property::handleActivated(int)
{
    if (update())
        changed(cache_);
}

QVariant Property::subscribe()
{
    return (!is_subscribed_ ? subscribe_() : cache_);
}

QVariant Property::subscribe_()
{
    if (!file_.tryOpen()) {
        reopen_timer_->start(reopen_interval_);
        return QVariant();
    }
    is_subscribed_ = true;

    file_.connect(this, &Property::handleActivated);

    if (update())
        changed(cache_);

    return cache_;
}

void Property::unsubscribe()
{
    if (is_subscribed_) {
        is_subscribed_ = false;
        file_.close();
    }
}

}}

using statefs::qt::PropertyMonitor;
using statefs::qt::ReplyEvent;

ContextPropertyPrivate::ContextPropertyPrivate(const QString &key)
    : key_(key)
    , state_(Initial)
    , is_cached_(false)
    , handle_(this)
{
}

ContextPropertyPrivate::~ContextPropertyPrivate()
{
    unsubscribe();
}

void ContextPropertyPrivate::detach()
{
    handle_.reset();
}

bool ContextPropertyPrivate::waitForUnsubscription() const
{
    static const auto min_timeout = std::chrono::milliseconds(5000);
    static const auto count_end = 3;

    if (state_ == Initial)
        return true;

    auto res = false;
    auto fn = [this, &res]() {
        std::future_status status;
        for (auto count = 0; count != count_end; --count) {
            // cor::wait_for for compatibility with gcc 4.6
            status = cor::wait_for(on_unsubscribed_, min_timeout);
            if (status != std::future_status::timeout) {
                res = true;
                return;
            } else if (!count) {
                debug::warning("Waiting for ages unsubscribing:", key_);
            }
        }
        debug::warning("Timeout unsubscribing:", key_);
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
    return res;
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

PropertyMonitor::monitor_ptr ContextPropertyPrivate::actor()
{
    return PropertyMonitor::instance();
}

void ContextPropertyPrivate::onChanged(QVariant v) const
{
    if (state_ == Subscribing)
        state_ = Subscribed;

    if (update(v))
        emit valueChanged();
}

void ContextPropertyPrivate::postEvent(ReplyEvent *e)
{
    QCoreApplication::postEvent(this, static_cast<QEvent*>(e));
}


bool ContextPropertyPrivate::event(QEvent *e)
{
    using statefs::qt::Event;
    using statefs::qt::DataReplyEvent;
    if (e->type() < QEvent::User)
        return QObject::event(e);

    auto res = true;
    auto fn = [this, e, &res]() {
        auto t = static_cast<Event::Type>(e->type());
        switch (t) {
        case Event::Data: {
            auto p = EVENT_CAST(e, DataReplyEvent);
            if (p) onChanged(std::move(p->data_));
            break;
        }
        default:
            debug::warning("Unknown user event");
            res = QObject::event(e);
        }
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
    return res;
}

bool ContextPropertyPrivate::update(QVariant const &v) const
{
    bool res = true;
    if (!is_cached_) {
        cache_ = v;
        is_cached_ = true;
    } else if (v != cache_) {
        cache_ = v;
    } else {
        res = false;
    }
    return res;
}


const ContextPropertyInfo* ContextPropertyPrivate::info() const
{
    return nullptr; // TODO
}

void ContextPropertyPrivate::subscribe() const
{
    auto fn = [this]() {
        using statefs::qt::SubscribeRequest;
        debug::debug("Subscribe request:", key_);
        if (state_ == Subscribing || state_ == Subscribed) {
            debug::debug("Already subscribed", key_);
            return;
        }
        // unsubscription is asynchronous, so wait for it to be finished
        // if resubcribing
        if (state_ == Unsubscribing) {
            debug::debug("Waiting for being unsubcribed", key_);
            if (!waitForUnsubscription())
                debug::warning("Resubscribing while not unsubscribed yet:", key_);
        }

        state_ = Subscribing;
        std::promise<QVariant> res;
        on_subscribed_ = res.get_future();
        auto ev = new SubscribeRequest(this->handle_, key_, std::move(res));
        actor()->postEvent(ev);
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
}

void ContextPropertyPrivate::unsubscribe() const
{
    auto fn = [this]() {
        using statefs::qt::UnsubscribeRequest;
        if (state_ == Unsubscribing)
            return;

        std::promise<void> res;
        on_unsubscribed_ = res.get_future();
        auto ev = new UnsubscribeRequest(this->handle_, key_, std::move(res));
        actor()->postEvent(ev);
        state_ = Unsubscribing;
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
}


void ContextPropertyPrivate::waitForSubscription() const
{
    static const auto min_timeout = std::chrono::milliseconds(5000);
    static const auto count_end = 3;

    auto fn = [this]() {
        if (state_ != Subscribing)
            return;

        std::future_status status;
        for (auto count = 0; count != count_end; --count) {
            // cor::wait_for for compatibility with gcc 4.6
            status = cor::wait_for(on_subscribed_, min_timeout);
            if (status != std::future_status::timeout) {
                update(on_subscribed_.get());
                state_ = Subscribed;
                return;
            } else if (!count) {
                debug::warning("Waiting for ages subscribing:", key_);
            }
        }
        debug::warning("Timeout subscribing:", key_);
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
}

void ContextPropertyPrivate::waitForSubscription(bool block) const
{
    auto fn = [this, block]() {
        if (state_ != Subscribing)
            return;

        if (block)
            return waitForSubscription();

        while (state_ != Subscribed)
            QCoreApplication::processEvents();

        state_ = Subscribed;
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
}

void ContextPropertyPrivate::ignoreCommander()
{
}

void ContextPropertyPrivate::setTypeCheck(bool)
{
}

void ContextPropertyPrivate::refresh() const
{
    using statefs::qt::RefreshRequest;
    actor()->postEvent(new RefreshRequest(this->handle_, key_));
}

ContextProperty::ContextProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , priv(new ContextPropertyPrivate(key))
{
    connect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
    priv->subscribe();
}

ContextProperty::~ContextProperty()
{
    disconnect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
    priv->detach();
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

void ContextProperty::setTypeCheck(bool)
{
}

namespace statefs { namespace qt {

DiscreteProperty::DiscreteProperty
(QString const &key, QObject *parent)
    : QObject(parent)
    , impl_(new DiscretePropertyImpl(key, this))
{
    connect(impl_, &DiscretePropertyImpl::changed
            , this, &DiscreteProperty::changed
            , Qt::DirectConnection);
}

DiscreteProperty::~DiscreteProperty()
{
}

void DiscreteProperty::refresh() const
{
    impl_->refresh();
}

DiscretePropertyImpl::DiscretePropertyImpl
(QString const &key, QObject *parent)
    : QObject(parent)
    , ContextPropertyPrivateHandle(key)
{
    connect(impl_, &ContextPropertyPrivate::valueChanged
            , this, &DiscretePropertyImpl::onChanged
            , Qt::DirectConnection);
    impl_->subscribe();
}

DiscretePropertyImpl::~DiscretePropertyImpl()
{
    disconnect(impl_, &ContextPropertyPrivate::valueChanged
               , this, &DiscretePropertyImpl::onChanged);
}

void DiscretePropertyImpl::onChanged()
{
    emit changed(impl_->value());
}

void DiscretePropertyImpl::refresh() const
{
    impl_->refresh();
}

PropertyWriter::PropertyWriter
(QString const &key, QObject *parent)
    : QObject(parent)
    , impl_(new PropertyWriterImpl(key))
{
    connect(impl_, &PropertyWriterImpl::updated
            , this, &PropertyWriter::updated
            , Qt::DirectConnection);
}

PropertyWriter::~PropertyWriter()
{
    disconnect(impl_, &PropertyWriterImpl::updated
            , this, &PropertyWriter::updated);
    impl_->detach();
}

void PropertyWriter::set(QVariant v)
{
    impl_->set(std::move(v));
}

PropertyWriterImpl::PropertyWriterImpl(QString const &key)
    : key_(key)
{
}

void PropertyWriterImpl::detach()
{
    handle_.reset();
}

void PropertyWriterImpl::set(QVariant &&v)
{
    using namespace statefs::qt;
    auto monitor = PropertyMonitor::instance();
    monitor->postEvent(new WriteRequest(handle_, key_, std::move(v)));
}

bool PropertyWriterImpl::event(QEvent *e)
{
    using namespace statefs::qt;

    if (e->type() < QEvent::User)
        return QObject::event(e);

    auto res = true;
    auto fn = [this, e, &res]() {
        auto t = static_cast<Event::Type>(e->type());
        switch (t) {
        case Event::WriteStatus: {
            auto p = EVENT_CAST(e, WriteReply);
            if (p)
                emit updated(p->is_updated_);
            break;
        }
        default:
            debug::warning("Unknown user event");
            res = QObject::event(e);
        }
    };
    execute_nothrow(fn, __PRETTY_FUNCTION__);
    return res;
}

}}

#include "property.moc"
