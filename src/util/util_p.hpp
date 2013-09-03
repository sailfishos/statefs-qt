#ifndef _STATEFS_QT_UTIL_P_HPP_
#define _STATEFS_QT_UTIL_P_HPP_

#include <statefs/qt/util.hpp>

class QFile;

namespace statefs { namespace qt {

class WriterImpl
{
public:
    WriterImpl(const QString&);
    virtual ~WriterImpl();

    bool exists() const;
    QString const &name() const;
    FileErrorNs::FileError set(const QVariant&);
private:
    QString name_;
    std::unique_ptr<QFile> file_;
};

}}

#endif // _STATEFS_QT_UTIL_P_HPP_
