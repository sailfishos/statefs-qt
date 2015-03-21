/**
 * @file qml/property.cpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "property.hpp"

using statefs::qt::DiscreteProperty;
using statefs::qt::PropertyWriter;

StateProperty::StateProperty(QObject* parent)
    : QObject(parent)
    , state_(State::Unknown)
    , impl_(null_qobject_unique<DiscreteProperty>())
    , writer_(null_qobject_unique<PropertyWriter>())
{
}

void StateProperty::classBegin()
{
}

void StateProperty::componentComplete()
{
    if (state_ == State::Unknown)
        setSubscribed(true);
}

StateProperty::~StateProperty()
{
}

QString StateProperty::getKey() const
{
    return key_;
}

void StateProperty::updateImpl()
{
    if ((state_ == State::Subscribed) && !key_.isEmpty()) {
        impl_ = make_qobject_unique<DiscreteProperty>(key_);
        connect(impl_.get(), &DiscreteProperty::changed
                , this, &StateProperty::onValueChanged);
    } else {
        impl_.reset();
    }
}

void StateProperty::setKey(QString key)
{
    if (key_ != key) {
        key_ = std::move(key);
        writer_.reset();
        updateImpl();
    }
}

QVariant StateProperty::getValue() const
{
    return value_;
}

void StateProperty::setValue(QVariant v)
{
    if (!writer_)
        writer_ = make_qobject_unique<PropertyWriter>(key_, this);
    writer_->set(std::move(v));
}

void StateProperty::refresh() const
{
    if (impl_)
        impl_->refresh();
}

bool StateProperty::getSubscribed() const
{
    return (state_ == State::Subscribed);
}

void StateProperty::setSubscribed(bool v)
{
    auto newState = v ? State::Subscribed : State::Unsubscribed;
    if (state_ != newState) {
        state_ = newState;
        updateImpl();
        emit subscribedChanged();
    }
}

void StateProperty::onValueChanged(QVariant v)
{
    value_ = std::move(v);
    emit valueChanged();
}
