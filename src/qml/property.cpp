/**
 * @file property.cpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "property.hpp"

using statefs::qt::DiscreteProperty;

StateMonitor::StateMonitor(QObject* parent)
    : QObject(parent)
    , isActive_(false)
    , impl_(null_qobject_unique<DiscreteProperty>())
{
}

StateMonitor::~StateMonitor()
{
}

QString StateMonitor::getKey() const
{
    return key_;
}

void StateMonitor::updateImpl()
{
    if (isActive_ && !key_.isEmpty()) {
        impl_ = make_qobject_unique<DiscreteProperty>(key_);
        connect(impl_.get(), &DiscreteProperty::changed
                , this, &StateMonitor::onValueChanged);
    } else {
        impl_.reset();
    }
}

void StateMonitor::setKey(QString key)
{
    if (key_ != key) {
        key_ = std::move(key);
        updateImpl();
    }
}

QVariant StateMonitor::getValue() const
{
    return value_;
}

bool StateMonitor::getActive() const
{
    return isActive_;
}

void StateMonitor::setActive(bool v)
{
    if (v != isActive_) {
        isActive_ = v;
        updateImpl();
        emit activeChanged();
    }
}

void StateMonitor::onValueChanged(QVariant v)
{
    value_ = std::move(v);
    emit valueChanged();
}
