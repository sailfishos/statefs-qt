/**
 * @file property.cpp
 * @brief Statefs property binding
 * @copyright (C) 2012-2014 Jolla Ltd.
 * @par License: LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "property.hpp"

using statefs::qt::DiscreteProperty;
using statefs::qt::PropertyWriter;

StateProperty::StateProperty(QObject* parent)
    : QObject(parent)
    , isActive_(false)
    , impl_(null_qobject_unique<DiscreteProperty>())
    , writer_(null_qobject_unique<PropertyWriter>())
{
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
    if (isActive_ && !key_.isEmpty()) {
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

bool StateProperty::getActive() const
{
    return isActive_;
}

void StateProperty::setActive(bool v)
{
    if (v != isActive_) {
        isActive_ = v;
        updateImpl();
        emit activeChanged();
    }
}

void StateProperty::onValueChanged(QVariant v)
{
    value_ = std::move(v);
    emit valueChanged();
}
