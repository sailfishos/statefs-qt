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
#include <sys/types.h>
#include <sys/socket.h>
#include <QSocketNotifier>

static int sigFd[2];

void onExit(int)
{
    char a = 1;
    ::write(sigFd[0], &a, sizeof(a));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QSocketNotifier *sigNot;
    auto args = app.arguments();
    if (args.size() <= 1) {
        qDebug() << "Usage: " << args[0] << " <namespace_path>...";
        return -1;
    }
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, sigFd);
    sigNot = new QSocketNotifier(sigFd[1], QSocketNotifier::Read, &app);
    app.connect(sigNot, &QSocketNotifier::activated, []() {
            auto app = QCoreApplication::instance();
            if (app)
                app->quit();
        });
    for (auto i : {SIGTERM, SIGINT})
        ::signal(i, onExit);
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
