/**
 * @file qml/property.cpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2015 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "contextkit_property.hpp"
#include <contextsubscriber/contextproperty.h>

ContextPropertyDeclarative::ContextPropertyDeclarative(QObject* parent)
    : QObject(parent)
    , state_(State::Unknown)
    , impl_(nullptr)
{
}

ContextPropertyDeclarative::~ContextPropertyDeclarative()
{
}

void ContextPropertyDeclarative::componentComplete()
{
    if (state_ == State::Unknown)
        setSubscribed(true);
    else
        updateImpl();
}

void ContextPropertyDeclarative::updateImpl()
{
    if (state_ == State::Subscribed) {
        if (impl_) {
            if (impl_->key() != key_) {
                delete impl_;
            } else {
                impl_->subscribe();
            }
        }
        if (!impl_) {
            impl_ = new ContextProperty(key_, this);
            connect(impl_, &ContextProperty::valueChanged
                    , this, &ContextPropertyDeclarative::valueChanged);
        }
    } else if (state_ == State::Unsubscribed) {
        if (impl_)
            impl_->unsubscribe();
    }
}

QString ContextPropertyDeclarative::getKey() const
{
    return key_;
}

void ContextPropertyDeclarative::setKey(QString const &key)
{
    if (key_ != key) {
        key_ = key;
        updateImpl();
        emit keyChanged();
    }
}

QVariant ContextPropertyDeclarative::getValue() const
{
    return impl_ ? impl_->value(default_value_) : default_value_;
}

void ContextPropertyDeclarative::setDefaultValue(QVariant const &v)
{
    default_value_ = v;
}

bool ContextPropertyDeclarative::getSubscribed() const
{
    return state_ == State::Subscribed;
}

void ContextPropertyDeclarative::setSubscribed(bool v)
{
    auto newState = v ? State::Subscribed : State::Unsubscribed;
    if (state_ != newState) {
        state_ = newState;
        updateImpl();
        emit subscribedChanged();
    }
}

void ContextPropertyDeclarative::subscribe()
{
    setSubscribed(true);
}

void ContextPropertyDeclarative::unsubscribe()
{
    setSubscribed(false);
}
