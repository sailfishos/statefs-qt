#include <QCoreApplication>
#include <contextproperty.h>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QRegExp>
#include <QTimer>
#include <map>
#include <memory>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <QSocketNotifier>
#include <qtaround/debug.hpp>
#include <statefs/qt/client.hpp>

namespace debug = qtaround::debug;

static int sigFd[2];

void onExit(int)
{
    char a = 1;
    if (::write(sigFd[0], &a, sizeof(a)) < 0) {
        // ignore
    }
}

int usage(QStringList const &args, int rc)
{
    qDebug() << "Usage: " << args[0] << " <namespace_path>...";
    return rc;
}

void writeProp(QString const &key, QString const &v)
{
    using statefs::qt::PropertyWriter;
    auto app = QCoreApplication::instance();
    debug::info("Writing", key, "=", v);
    auto w = new PropertyWriter{key, app};
    app->connect(w, &PropertyWriter::updated, [key](bool b) {
            debug::info("Updated?", b, key);
        });
    w->set(v);
}

void monitorProps(QStringList keys)
{
    using statefs::qt::DiscreteProperty;
    auto app = QCoreApplication::instance();
    for (auto name : keys) {
        auto p = new DiscreteProperty(name, app);
        app->connect(p, &DiscreteProperty::changed, [name](QVariant v) {
                debug::print(name, "=", v);
            });
    }
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

    auto t = new QTimer(&app);
    t->setSingleShot(true);
    if (args[1] == "-w") {
        if (args.size() <= 3)
            return usage(args, -1);

        app.connect(t, &QTimer::timeout, [args]() {
                writeProp(args[2], args[3]);
            });
    } else {
        auto dirname = args[1];
        QDir d(dirname);
        auto files = d.entryList(QDir::Files);
        auto prefix = d.dirName() + ".";
        files = files.replaceInStrings(QRegExp("^"), prefix);
        qDebug() << files;

        app.connect(t, &QTimer::timeout, [files]() {
                monitorProps(files);
            });
    }
    t->start(0);
    return app.exec();
}
