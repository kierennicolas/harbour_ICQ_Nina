// Declarations only: lets CI compile the app's Sailfish-specific sources on a
// desktop (without libsailfishapp) to catch C++ errors early. Not linked.
#ifndef SAILFISHAPP_STUB_H
#define SAILFISHAPP_STUB_H
#include <QGuiApplication>
#include <QQuickView>
#include <QUrl>
namespace SailfishApp {
QGuiApplication *application(int &argc, char **argv);
QQuickView *createView();
QUrl pathToMainQml();
QUrl pathTo(const QString &filename);
}
#endif
