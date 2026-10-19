// ninacheck <profile: nina|nina-tls>
// Signs in to NINA with $NINA_UIN / $OSCAR_PASSWORD through the app's
// AccountManager and saves the profile the way the profile page does: the
// "about" text set to a marker (everything else as it is), then checks
// what NINA kept and puts the original back. Output is safe for public
// logs: field names and yes/no only.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#include <QDateTime>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>

#include "accountmanager.h"

static void out(const QString &line)
{
    fprintf(stdout, "%s CHECK-%s\n", qPrintable(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"))),
            qPrintable(line));
    fflush(stdout);
}

static void handler(QtMsgType, const QMessageLogContext &ctx, const QString &msg)
{
    // Only liboscar's notes on profile writes (types and results, no values).
    if (ctx.category && qstrcmp(ctx.category, "liboscar") == 0
            && (msg.contains(QLatin1String("ICQ meta reply")) || msg.contains(QLatin1String("own info update TLVs"))))
        out(QStringLiteral("TRACE ") + msg.simplified());
}

static bool waitFor(std::function<bool()> cond, int ms)
{
    QElapsedTimer t;
    t.start();
    while (!cond() && t.elapsed() < ms)
        QTest::qWait(100);
    return cond();
}

static QVariantMap pageFields(const QVariantMap &info)
{
    // What ProfilePage hands to saveProfile.
    QVariantMap f;
    for (const char *k : { "nickname", "firstName", "lastName", "email", "city", "state", "homepage", "about" })
        f.insert(QLatin1String(k), info.value(QLatin1String(k)).toString());
    f.insert(QStringLiteral("gender"), info.value(QStringLiteral("gender")).toInt());
    if (info.value(QStringLiteral("birthday")).toDate().isValid())
        f.insert(QStringLiteral("birthday"), info.value(QStringLiteral("birthday")).toDate());
    return f;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "minimal");
    QGuiApplication app(argc, argv);
    QLoggingCategory::setFilterRules(QStringLiteral("liboscar.debug=true"));
    qInstallMessageHandler(handler);
    const QString profile = argc > 1 ? QString::fromLatin1(argv[1]) : QStringLiteral("nina-tls");
    const QString uin = QString::fromUtf8(qgetenv("NINA_UIN"));
    const QString password = QString::fromUtf8(qgetenv("OSCAR_PASSWORD"));
    if (uin.isEmpty() || password.isEmpty()) {
        out(QStringLiteral("SKIPPED no account"));
        return 0;
    }
    QTemporaryDir dir;
    AccountManager a(dir.path() + "/config", dir.path() + "/data");
    if (profile == QLatin1String("custom"))   // local tests: ninacheck custom <host>
        a.signIn(uin, password, false, profile, argc > 2 ? QString::fromLatin1(argv[2]) : QStringLiteral("127.0.0.1"), 5190, false);
    else
        a.signIn(uin, password, false, profile);
    if (!waitFor([&] { return a.state() == AccountManager::Online; }, 40000)) {
        out(QStringLiteral("LOGIN failed: %1").arg(a.lastError()));
        return 1;
    }
    out(QStringLiteral("LOGIN ok (%1)").arg(profile));
    QTest::qWait(6000); // own details arrive after the contact list

    QSignalSpy info(&a, &AccountManager::userInfoReceived);
    a.requestUserInfo(uin);
    waitFor([&] { return info.count() > 0; }, 10000);
    const QVariantMap original = a.ownProfile();
    out(QStringLiteral("ORIGINAL known=%1 about=%2 bytes").arg(original.size() > 1 ? "yes" : "no")
        .arg(original.value(QStringLiteral("about")).toString().size()));

    QSignalSpy saved(&a, &AccountManager::profileSaved);
    const QString marker = QStringLiteral("icqnina-core-check-") + QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
    QVariantMap f = pageFields(original);
    f.insert(QStringLiteral("about"), marker);
    if (!a.saveProfile(f)) {
        out(QStringLiteral("SAVE could not start"));
        return 1;
    }
    waitFor([&] { return saved.count() > 0; }, 30000);
    out(QStringLiteral("SAVE answered=%1 ok=%2 message=\"%3\"").arg(saved.count() > 0 ? "yes" : "no")
        .arg(saved.count() > 0 && saved.first().at(0).toBool() ? "yes" : "no")
        .arg(saved.count() > 0 ? saved.first().at(1).toString() : QString()));

    // Later look, as the user would.
    QTest::qWait(5000);
    info.clear();
    a.requestUserInfo(uin);
    waitFor([&] { return info.count() > 0; }, 10000);
    QTest::qWait(1000);
    out(QStringLiteral("LATER about kept=%1").arg(a.ownProfile().value(QStringLiteral("about")).toString() == marker ? "yes" : "no"));

    // Put it back.
    saved.clear();
    a.saveProfile(pageFields(original));
    waitFor([&] { return saved.count() > 0; }, 30000);
    QTest::qWait(5000);
    info.clear();
    a.requestUserInfo(uin);
    waitFor([&] { return info.count() > 0; }, 10000);
    QTest::qWait(1000);
    out(QStringLiteral("RESTORE about back=%1").arg(a.ownProfile().value(QStringLiteral("about")).toString()
                                                    == original.value(QStringLiteral("about")).toString() ? "yes" : "no"));
    a.disconnectAccount();
    return 0;
}
