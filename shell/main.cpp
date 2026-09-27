/*
    SPDX-FileCopyrightText: 2002 Wilco Greven <greven@kde.org>
    SPDX-FileCopyrightText: 2003 Christophe Devriese <Christophe.Devriese@student.kuleuven.ac.be>
    SPDX-FileCopyrightText: 2003 Laurent Montel <montel@kde.org>
    SPDX-FileCopyrightText: 2003-2007 Albert Astals Cid <aacid@kde.org>
    SPDX-FileCopyrightText: 2004 Andy Goossens <andygoossens@telenet.be>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shell.h"

#include "aboutdata.h"
#include "okular_main.h"
#include "shellutils.h"
#include <KAboutData>
#include <KConfigGroup>
#include <KCrash>
#include <KIconTheme>
#include <KLocalizedString>
#include <KWindowSystem>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QtGlobal>

#include "config-okular.h"

#define HAVE_STYLE_MANAGER __has_include(<KStyleManager>)
#if HAVE_STYLE_MANAGER
#include <KStyleManager>
#endif

#if defined(Q_OS_WIN) && !HAVE_DBUS
#include "SingleApplication/SingleApplication"
#endif

/**
 * Event handler for macOS file opening via QFileOpenEvent.
 * Should not do anything on Windows/Linux as QFileOpenEvent is never fired there.
 */

class FileOpenEventHandler : public QObject
{
    Q_OBJECT
protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            auto *foe = static_cast<QFileOpenEvent *>(event);
            // Find existing Shell window
            const auto widgets = QApplication::topLevelWidgets();
            for (QWidget *widget : widgets) {
                if (Shell *shell = qobject_cast<Shell *>(widget)) {
                    QString serializedOptions = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());
                    shell->openDocument(foe->url(), serializedOptions);
                    shell->raise();
                    shell->activateWindow();
                    return true;
                }
            }
            return false;
        }
        return QObject::eventFilter(obj, event);
    }
};

int main(int argc, char **argv)
{
    /**
     * trigger initialisation of proper icon theme
     */
#if KICONTHEMES_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    KIconTheme::initTheme();
#endif

    QCoreApplication::setAttribute(Qt::AA_CompressTabletEvents);

    // Use SingleApplication to provide IPC for instance reuse on Windows.
    // IMPORTANT: It exclusively replaces QApplication.
#if defined(Q_OS_WIN) && !HAVE_DBUS
    // Set standard metadata BEFORE SingleApplication so it generates the right IPC key
    QCoreApplication::setApplicationName(QStringLiteral("okular"));
    QCoreApplication::setOrganizationName(QStringLiteral("KDE"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));

    SingleApplication app(argc, argv, true);
#else
    QApplication app(argc, argv);
#endif

    /**
     * Install event filter to handle macOS file opening.
     * This enables double-click and "Open With" on macOS.
     */
    FileOpenEventHandler eventHandler;
    app.installEventFilter(&eventHandler);
    KLocalizedString::setApplicationDomain("okular");

#if HAVE_STYLE_MANAGER
    /**
     * trigger initialisation of proper application style
     */
    KStyleManager::initStyle();
#else
    /**
     * For Windows and macOS: use Breeze if available
     * Of all tested styles that works the best for us
     */
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QApplication::setStyle(QStringLiteral("breeze"));
#endif
#endif

    KAboutData aboutData = okularAboutData();
    KAboutData::setApplicationData(aboutData);
    // set icon for shells which do not use desktop file metadata
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("okular")));

    KCrash::initialize();

    QCommandLineParser parser;
    // The KDE4 version accepted flags such as -unique with a single dash -> preserve compatibility
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    aboutData.setupCommandLine(&parser);

    parser.addOption(QCommandLineOption({QStringLiteral("p"), QStringLiteral("page")}, i18n("Page of the document to be shown"), QStringLiteral("number")));
    parser.addOption(QCommandLineOption({QStringLiteral("presentation")}, i18n("Start the document in presentation mode")));
    parser.addOption(QCommandLineOption({QStringLiteral("print")}, i18n("Start with print dialog")));
    parser.addOption(QCommandLineOption({QStringLiteral("print-and-exit")}, i18n("Start with print dialog and exit after printing")));
    parser.addOption(QCommandLineOption({QStringLiteral("unique")}, i18n("\"Unique instance\" control")));
    parser.addOption(QCommandLineOption({QStringLiteral("noraise")}, i18n("Not raise window")));
    parser.addOption(QCommandLineOption({QStringLiteral("find")}, i18n("Find a string on the text"), QStringLiteral("string")));
    parser.addOption(QCommandLineOption({QStringLiteral("editor-cmd")}, i18n("Sets the external editor command"), QStringLiteral("string")));
    parser.addPositionalArgument(QStringLiteral("urls"), i18n("Documents to open. Specify '-' to read from stdin."));

    parser.process(app);
    aboutData.processCommandLine(&parser);

#if defined(Q_OS_WIN) && !HAVE_DBUS
    // Primary instance: listen for URLs from secondary instances.
    if (app.isPrimary()) {
        QObject::connect(&app, &SingleApplication::receivedMessage, &app, [](quint32 instanceId, QByteArray message) {
            Q_UNUSED(instanceId);

            const QJsonDocument doc = QJsonDocument::fromJson(message);
            if (!doc.isObject()) {
                return;
            }

            const QJsonObject obj = doc.object();
            const QJsonArray urls = obj.value(QStringLiteral("urls")).toArray();
            const QString opts = obj.value(QStringLiteral("serializedOptions")).toString();

            // Find the existing Shell window
            Shell *shell = nullptr;
            const auto widgets = QApplication::topLevelWidgets();
            for (QWidget *widget : widgets) {
                shell = qobject_cast<Shell *>(widget);
                if (shell) {
                    break;
                }
            }

            if (!shell) {
                return;
            }

            for (const QJsonValue &urlVal : urls) {
                const QUrl url(urlVal.toObject().value(QStringLiteral("url")).toString());
                if (!shell->openDocument(url, opts)) {
                    // Tabs disabled or full: fallback to opening a new window.
                    Shell *newShell = new Shell(opts);
                    if (newShell->isValid()) {
                        newShell->show();
                        newShell->openDocument(url, opts);
                        shell = newShell;
                    } else {
                        delete newShell;
                    }
                }
            }

            // Raise and activate the window
            shell->raise();
            shell->activateWindow();
            KWindowSystem::activateWindow(shell->windowHandle());
        });
    }
#endif // defined(Q_OS_WIN) && !HAVE_DBUS

    // see if we are starting with session management
    if (app.isSessionRestored()) {
        kRestoreMainWindows<Shell>();
    } else {
        // no session.. just start up normally
        QStringList paths;
        for (int i = 0; i < parser.positionalArguments().count(); ++i) {
            paths << parser.positionalArguments().at(i);
        }
        Okular::Status status = Okular::main(paths, ShellUtils::serializeOptions(parser));
        switch (status) {
        case Okular::Error:
            return -1;
        case Okular::AttachedOtherProcess:
            return 0;
        case Okular::Success:
            // Do nothing
            break;
        }
    }

    return app.exec();
}
#include "main.moc"
