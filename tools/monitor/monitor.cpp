#include <QCoreApplication>
#include <contextproperty.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QRegExp>
#include <map>
#include <memory>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <QSocketNotifier>
#include <statefs/qt/client.hpp>

static int sigFd[2];

void onExit(int)
{
    char a = 1;
    ::write(sigFd[0], &a, sizeof(a));
}

int usage(QStringList const &args, int rc)
{
    qDebug() << "Usage: " << args[0] << " <namespace_path>...";
    return rc;
}

void writeProp(QString const &key, QString const &v)
{
    auto w = new statefs::qt::PropertyWriter{key};
    w->set(v);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QSocketNotifier *sigNot;
    auto args = app.arguments();
    if (args.size() <= 1)
        return usage(args, -1);

    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd);
    sigNot = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, &app);
    app.connect(sigNot, &QSocketNotifier::activated, []() {
            auto app = QCoreApplication::instance();
            if (app)
                app->quit();
        });
    for (auto i : {SIGTERM, SIGINT})
        ::signal(i, onExit);
    if (args[1] == "-w") {
        if (args.size() <= 3)
            return usage(args, -1);

        writeProp(args[2], args[3]);
    } else {
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
    }
    return app.exec();
}
