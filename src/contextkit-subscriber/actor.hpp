#ifndef _COR_QT_ACTOR_HPP_
#define _COR_QT_ACTOR_HPP_

#include <QWaitCondition>
#include <QMutex>
#include <QThread>
#include <QCoreApplication>
#include <functional>
#include <QScopedPointer>

namespace cor { namespace qt {

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

}}

#endif // _COR_QT_ACTOR_HPP_
