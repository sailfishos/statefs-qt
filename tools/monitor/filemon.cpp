#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QRegExp>
#include <QSocketNotifier>
#include <map>
#include <memory>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    if (args.size() <= 1) {
        qDebug() << "Usage: " << args[0] << " <namespace_path>...";
        return -1;
    }
    auto dirname = args[1];
    QDir d(dirname);
    auto files = d.entryList(QDir::Files);
    auto prefix = d.path() + "/";
    files = files.replaceInStrings(QRegExp("^"), prefix);
    qDebug() << files;

    auto begin = files.begin(), end = files.end();
    for (auto pos = begin; pos != end; ++pos) {
        auto f = std::make_shared<QFile>(*pos);
        f->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        auto p = std::make_shared<QSocketNotifier>(f->handle(), QSocketNotifier::Read);
        app.connect(p.get(), &QSocketNotifier::activated, [p, f](int) mutable {
                qDebug() << "Activated " << f->fileName() << ", " << f->isOpen();
                //p->setEnabled(false);
                //QFile touchFile(f->fileName());
                //touchFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
                f->seek(0);
                QByteArray buf(128, 0);
                qDebug() << f->read(buf.data(), 120);
                qDebug() << "(" << QString(buf) << ")";
                //p->setEnabled(true);
            });
        p->setEnabled(true);
    }
    return app.exec();
}
