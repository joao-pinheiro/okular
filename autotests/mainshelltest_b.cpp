/*
    SPDX-FileCopyrightText: 2014 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// clazy:excludeall=qstring-allocations

#include <QTest>
#include <config-okular.h>

#include <KConfigGroup>
#include <KLineEdit>
#include <KRecentFilesAction>

#if HAVE_DBUS
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif // HAVE_DBUS
#include <QPrintDialog>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTimer>
#include <qwidget.h>

#include "../core/document_p.h"
#include "../part/findbar.h"
#include "../part/part.h"
#include "../part/presentationwidget.h"
#include "../settings.h"
#include "../shell/okular_main.h"
#include "../shell/shell.h"
#include "../shell/shellutils.h"
#include "closedialoghelper.h"

#include <sys/types.h>
#ifndef Q_OS_WIN
#include <unistd.h>
#else
#include <process.h>
#endif

namespace Okular
{
class PartTest
{
public:
    Okular::Document *partDocument(Okular::Part *part) const
    {
        return part->m_document;
    }
    QWidget *presentationWidget(Okular::Part *part) const
    {
        return part->m_presentationWidget;
    }
    FindBar *findWidget(Okular::Part *part) const
    {
        return part->m_findBar;
    }
};
}

class ClosePrintDialogHelper : public QObject
{
    Q_OBJECT

public:
    explicit ClosePrintDialogHelper(int expectedTab)
        : foundDialog(false)
        , m_expectedTab(expectedTab)
    {
    }
    bool foundDialog;

public Q_SLOTS:
    void closePrintDialog();

private:
    int m_expectedTab;
};

class MainShellTestB : public QObject, public Okular::PartTest
{
    Q_OBJECT

public:
    static QTabWidget *tabWidget(Shell *s)
    {
        return s->m_tabWidget;
    }

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testFileRemembersPagePosition_data();
    void testFileRemembersPagePosition();
    void testMiddleButtonCloseUndo();
    void testSessionRestore_data();
    void testSessionRestore();

private:
};

QList<Shell *> getShells()
{
    QList<Shell *> shells;
    const QList<KMainWindow *> mainWindows = KMainWindow::memberList();
    for (KMainWindow *kmw : mainWindows) {
        Shell *shell = qobject_cast<Shell *>(kmw);
        if (shell) {
            shells.append(shell);
        }
    }
    return shells;
}

Shell *findShell(Shell *ignore = nullptr)
{
    const QWidgetList wList = QApplication::topLevelWidgets();
    for (QWidget *widget : wList) {
        Shell *s = qobject_cast<Shell *>(widget);
        if (s && s != ignore) {
            return s;
        }
    }
    return nullptr;
}

void MainShellTestB::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    // Don't pollute people's okular settings
    Okular::Settings::instance(QStringLiteral("mainshelltest"));

    // Register in bus as okular
#if HAVE_DBUS
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    QVERIFY(bus->registerService(ShellUtils::currentProcessDbusName()) == QDBusConnectionInterface::ServiceRegistered);
#endif
    // Tell the presentationWidget and queryClose to not be annoying
    KSharedConfigPtr c = KSharedConfig::openConfig(QStringLiteral("mainshelltest.kmessagebox"));
    KConfigGroup cg = c->group(QStringLiteral("General"));
    cg.writeEntry("presentationInfo", 4);
    cg.writeEntry("ShowTabWarning", 4);
}

void MainShellTestB::cleanupTestCase()
{
}

void MainShellTestB::init()
{
    // Default settings for every test
    Okular::Settings::self()->setDefaults();

    // Clean docdatas
    const QList<QUrl> urls = {QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/file1.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/tocreload.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/contents.epub"))};

    for (const QUrl &url : urls) {
        QFileInfo fileReadTest(url.toLocalFile());
        const QString docDataPath = Okular::DocumentPrivate::docDataFileName(url, fileReadTest.size());
        QFile::remove(docDataPath);
    }
}

void MainShellTestB::cleanup()
{
    Shell *s;
    while ((s = findShell())) {
        delete s;
    }
}

void ClosePrintDialogHelper::closePrintDialog()
{
    Shell *s = findShell();
    QPrintDialog *dialog = s->findChild<QPrintDialog *>();
    if (!dialog) {
        QTimer::singleShot(0, this, &ClosePrintDialogHelper::closePrintDialog);
        return;
    }
    QVERIFY(dialog);
    QCOMPARE(MainShellTestB::tabWidget(s)->currentIndex(), m_expectedTab);
    dialog->close();
    foundDialog = true;
}

void MainShellTestB::testFileRemembersPagePosition_data()
{
    QTest::addColumn<int>("mode");

    QTest::newRow("normal") << 1;
    QTest::newRow("unique") << 2;
    QTest::newRow("tabs") << 3;
}

void MainShellTestB::testFileRemembersPagePosition()
{
    QFETCH(int, mode);

    const QStringList paths = QStringList(QStringLiteral(KDESRCDIR "data/contents.epub"));
    QString serializedOptions;
    if (mode == 1 || mode == 3) {
        serializedOptions = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());
    } else {
        serializedOptions = ShellUtils::serializeOptions(false, false, false, true, false, QString(), QString(), QString());
    }

    Okular::Settings::self()->setShellOpenFileInTabs(mode == 3);

    Okular::Status status = Okular::main(paths, serializedOptions);
    QCOMPARE(status, Okular::Success);
    Shell *s = findShell();
    QVERIFY(s);
    Okular::Part *part = s->findChild<Okular::Part *>();
    QVERIFY(part);
    QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(paths[0]));
    QCOMPARE(partDocument(part)->currentPage(), 0u);
    partDocument(part)->setViewportPage(3);
    QCOMPARE(partDocument(part)->currentPage(), 3u);
    s->closeUrl();
    QCOMPARE(part->url().url(), QString());

    if (mode == 1) {
        delete s;
        status = Okular::main(paths, serializedOptions);
        QCOMPARE(status, Okular::Success);
    } else {
        QProcess p;
        QStringList args;
        args << paths[0];
        if (mode == 2) {
            args << QStringLiteral("-unique");
        }
        p.start(QStringLiteral(OKULAR_BINARY), args);
        p.waitForStarted();
        QCOMPARE(p.state(), QProcess::Running);

        // It is attaching to us, so will eventually stop
        QTRY_COMPARE_WITH_TIMEOUT((int)p.state(), (int)QProcess::NotRunning, 20000);
        QCOMPARE((int)p.exitStatus(), (int)QProcess::NormalExit);
        QCOMPARE(p.exitCode(), 0);
    }
    s = findShell();
    QVERIFY(s);
    part = s->findChild<Okular::Part *>();
    QVERIFY(part);
    QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(paths[0]));
    QCOMPARE(partDocument(part)->currentPage(), 3u);
}

void MainShellTestB::testSessionRestore_data()
{
    QTest::addColumn<QStringList>("paths");
    QTest::addColumn<QString>("options");
    QTest::addColumn<bool>("useTabsOpen");
    QTest::addColumn<bool>("useTabsRestore");

    QStringList oneDocPaths(QStringLiteral(KDESRCDIR "data/file1.pdf"));
    QStringList twoDocPaths(oneDocPaths);
    twoDocPaths << QStringLiteral(KDESRCDIR "data/formSamples.pdf");

    const QString options = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());

    QTest::newRow("1 doc, 1 window, tabs") << oneDocPaths << options << true << true;
    QTest::newRow("2 docs, 1 window, tabs") << twoDocPaths << options << true << true;
    QTest::newRow("2 docs, 2 windows, tabs") << twoDocPaths << options << false << true;
    QTest::newRow("2 docs, 2 windows, no tabs") << twoDocPaths << options << false << false;
    QTest::newRow("2 docs, 1 window, no tabs") << twoDocPaths << options << true << false;
}

void MainShellTestB::testSessionRestore()
{
    QFETCH(QStringList, paths);
    QFETCH(QString, options);
    QFETCH(bool, useTabsOpen);
    QFETCH(bool, useTabsRestore);

    Okular::Settings::self()->setShellOpenFileInTabs(useTabsOpen);

    Okular::Status status = Okular::main(paths, options);
    QCOMPARE(status, Okular::Success);

    // Gather some information about the state
    // Verify that the correct number of windows/tabs were opened
    QList<Shell *> shells = getShells();
    QVERIFY(!shells.isEmpty());
    int numDocs = 0;
    for (Shell *shell : std::as_const(shells)) {
        QVERIFY(QTest::qWaitForWindowExposed(shell));
        numDocs += shell->m_tabs.size();
    }

    QCOMPARE(numDocs, paths.size());
    QCOMPARE(shells.size(), useTabsOpen ? 1 : paths.size());
    QTest::qWait(100);

    // Simulate session shutdown. The actual shutdown path comes through
    // QSessionManager XSMP handlers, then KApplication::commitData/saveState,
    // then KMWSessionManager::commitData/saveState. Without simulating an X
    // session manager, the best we can do here is to make a temporary Config
    // and call KMainWindows save functions directly.
    QTemporaryFile configFile;
    QVERIFY(configFile.open());

    int numWindows = 0;
    { // Scope for config so that we can reconstruct from file
        KConfig config(configFile.fileName(), KConfig::SimpleConfig);
        for (Shell *shell : std::as_const(shells)) {
            shell->savePropertiesInternal(&config, ++numWindows);
            // Windows aren't necessarily closed on shutdown, but we'll use
            // this as a way to trigger the destructor code, which is normally
            // connected to the aboutToQuit signal
            shell->close();
        }
    }

    // Wait for shells to delete themselves. QTest::qWait doesn't do deferred
    // deletions so we'll set up a full event loop to do that.
    QEventLoop eventLoop;
    QTimer::singleShot(100, &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::AllEvents);
    // Sometimes the event loop is not enough, so try a bit more to get deferred delete happen
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    shells = getShells();
    QCOMPARE(shells.size(), 0);

    Okular::Settings::self()->setShellOpenFileInTabs(useTabsRestore);

    // Simulate session restore. We can't call KMainWindow::restore() directly
    // because it asks for info from the session manager, which doesn't know
    // about our temporary config. But the logic here mostly mirrors restore().
    KConfig config(configFile.fileName(), KConfig::SimpleConfig);
    for (int i = 1; i <= numWindows; ++i) {
        Shell *shell = new Shell;
        shell->readPropertiesInternal(&config, i);
        shell->show();
    }

    // Verify that the restore state is reasonable
    shells = getShells();
    QVERIFY(!shells.isEmpty());
    numDocs = 0;
    for (Shell *shell : std::as_const(shells)) {
        QVERIFY(QTest::qWaitForWindowExposed(shell));
        numDocs += shell->m_tabs.size();
    }

    QCOMPARE(numDocs, paths.size());
    QCOMPARE(shells.size(), useTabsRestore ? numWindows : paths.size());
}

void MainShellTestB::testMiddleButtonCloseUndo()
{
    const QStringList paths = {QStringLiteral(KDESRCDIR "data/file1.pdf"), QStringLiteral(KDESRCDIR "data/file2.pdf")};
    QString serializedOptions;
    serializedOptions = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());

    Okular::Settings::self()->setShellOpenFileInTabs(true);
    Okular::Status status = Okular::main(paths, serializedOptions);
    QCOMPARE(status, Okular::Success);
    Shell *s = findShell();
    QVERIFY(s);

    QCOMPARE(s->m_tabWidget->count(), paths.size());
    // Close a tab using middle key
    QWidget *firstTab = s->m_tabWidget->tabBar()->tabButton(0, QTabBar::RightSide);
    QVERIFY(firstTab);
    QTest::mouseClick(firstTab, Qt::MiddleButton);
    QCOMPARE(s->m_tabWidget->count(), paths.size() - 1);
    // Undo tab close
    s->undoCloseTab();
    QCOMPARE(s->m_tabWidget->count(), paths.size());
}

QTEST_MAIN(MainShellTestB)
#include "mainshelltest_b.moc"
