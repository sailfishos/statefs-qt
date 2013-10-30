#include <QCoreApplication>
#include <contextproperty.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QRegExp>
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
    auto prefix = d.dirName() + ".";
    files = files.replaceInStrings(QRegExp("^"), prefix);
    qDebug() << files;

    auto begin = files.begin(), end = files.end();
    for (auto pos = begin; pos != end; ++pos) {
        auto p = std::make_shared<ContextProperty>(*pos);
        app.connect(p.get(), &ContextProperty::valueChanged, [p]() {
                qDebug() << p->key() << "=" << p->value();
            });
    }
    return app.exec();
}
