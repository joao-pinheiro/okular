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

class MainShellTestA : public QObject, public Okular::PartTest
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

    void testShell_data();
    void testShell();
    void test2FilesError_data();
    void test2FilesError();
    void testOpenInvalidFiles_data();
    void testOpenInvalidFiles();
    void testOpenTheSameFileSeveralTimes();

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

void MainShellTestA::initTestCase()
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

void MainShellTestA::cleanupTestCase()
{
}

void MainShellTestA::init()
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

void MainShellTestA::cleanup()
{
    Shell *s;
    while ((s = findShell())) {
        delete s;
    }
}

void MainShellTestA::testShell_data()
{
    QTest::addColumn<QStringList>("paths");
    QTest::addColumn<QString>("serializedOptions");
    QTest::addColumn<bool>("useTabs");
    QTest::addColumn<QString>("externalProcessPath");
    QTest::addColumn<uint>("expectedPage");
    QTest::addColumn<bool>("expectPresentation");
    QTest::addColumn<bool>("expectPrintDialog");
    QTest::addColumn<bool>("unique");
    QTest::addColumn<uint>("externalProcessExpectedPage");
    QTest::addColumn<bool>("externalProcessExpectPresentation");
    QTest::addColumn<bool>("externalProcessExpectPrintDialog");
    QTest::addColumn<QString>("externalProcessExpectFind");

    const QStringList contentsEpub = QStringList(QStringLiteral(KDESRCDIR "data/contents.epub"));
    const QStringList file1 = QStringList(QStringLiteral(KDESRCDIR "data/file1.pdf"));
    QStringList file1AndToc;
    file1AndToc << QStringLiteral(KDESRCDIR "data/file1.pdf");
    file1AndToc << QStringLiteral(KDESRCDIR "data/tocreload.pdf");
    const QString tocReload = QStringLiteral(KDESRCDIR "data/tocreload.pdf");

    const QString optionsPage2 = ShellUtils::serializeOptions(false, false, false, false, false, QStringLiteral("2"), QString(), QString());
    const QString optionsPage2Presentation = ShellUtils::serializeOptions(true, false, false, false, false, QStringLiteral("2"), QString(), QString());
    const QString optionsPrint = ShellUtils::serializeOptions(false, true, false, false, false, QString(), QString(), QString());
    const QString optionsUnique = ShellUtils::serializeOptions(false, false, false, true, false, QString(), QString(), QString());
    const QString optionsFind = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QStringLiteral("si:next-testing parameters!"), QString());

    QTest::newRow("just show shell") << QStringList() << QString() << false << QString() << 0u << false << false << false << 0u << false << false << QString();
    QTest::newRow("open file") << file1 << QString() << false << QString() << 0u << false << false << false << 0u << false << false << QString();
    QTest::newRow("two files no tabs") << file1AndToc << QString() << false << QString() << 0u << false << false << false << 0u << false << false << QString();
    QTest::newRow("two files with tabs") << file1AndToc << QString() << true << QString() << 0u << false << false << false << 0u << false << false << QString();
    QTest::newRow("two files sequence no tabs") << file1 << QString() << false << tocReload << 0u << false << false << false << 0u << false << false << QString();
#if HAVE_DBUS
    QTest::newRow("two files sequence with tabs") << file1 << QString() << true << tocReload << 0u << false << false << false << 0u << false << false << QString();
#endif // HAVE_DBUS
    QTest::newRow("open file page number") << contentsEpub << optionsPage2 << false << QString() << 1u << false << false << false << 0u << false << false << QString();
    QTest::newRow("open file page number and presentation") << contentsEpub << optionsPage2Presentation << false << QString() << 1u << true << false << false << 0u << false << false << QString();
    QTest::newRow("open file find") << file1 << optionsFind << false << QString() << 0u << false << false << false << 0u << false << false << QStringLiteral("si:next-testing parameters!");
    QTest::newRow("open file print") << file1 << optionsPrint << false << QString() << 0u << false << true << false << 0u << false << false << QString();
#if HAVE_DBUS
    QTest::newRow("open two files unique") << file1 << optionsUnique << false << tocReload << 0u << false << false << true << 0u << false << false << QString();
    QTest::newRow("open two files unique tabs") << file1 << optionsUnique << true << tocReload << 0u << false << false << true << 0u << false << false << QString();
    QTest::newRow("page number attach tabs") << file1 << QString() << true << contentsEpub[0] << 0u << false << false << false << 2u << false << false << QString();
    QTest::newRow("presentation attach tabs") << file1 << QString() << true << contentsEpub[0] << 0u << false << false << false << 2u << true << false << QString();
    QTest::newRow("print attach tabs") << file1 << QString() << true << contentsEpub[0] << 0u << false << true << false << 2u << false << true << QString();
    QTest::newRow("page number attach unique") << file1 << optionsUnique << false << contentsEpub[0] << 0u << false << false << true << 3u << false << false << QString();
    QTest::newRow("presentation attach unique") << file1 << optionsUnique << false << contentsEpub[0] << 0u << false << false << true << 2u << true << false << QString();
    QTest::newRow("print attach unique") << file1 << optionsUnique << false << contentsEpub[0] << 0u << false << false << true << 2u << false << true << QString();
    QTest::newRow("page number attach unique tabs") << file1 << optionsUnique << true << contentsEpub[0] << 0u << false << false << true << 3u << false << false << QString();
    QTest::newRow("presentation attach unique tabs") << file1 << optionsUnique << true << contentsEpub[0] << 0u << false << false << true << 2u << true << false << QString();
    QTest::newRow("print attach unique tabs") << file1 << optionsUnique << true << contentsEpub[0] << 0u << false << false << true << 2u << false << true << QString();
#endif // HAVE_DBUS
}

void MainShellTestA::testShell()
{
    QFETCH(QStringList, paths);
    QFETCH(QString, serializedOptions);
    QFETCH(bool, useTabs);
    QFETCH(QString, externalProcessPath);
    QFETCH(uint, expectedPage);
    QFETCH(bool, expectPresentation);
    QFETCH(bool, expectPrintDialog);
    QFETCH(bool, unique);
    QFETCH(uint, externalProcessExpectedPage);
    QFETCH(bool, externalProcessExpectPresentation);
    QFETCH(bool, externalProcessExpectPrintDialog);
    QFETCH(QString, externalProcessExpectFind);

    QScopedPointer<ClosePrintDialogHelper> helper;

    Okular::Settings::self()->setShellOpenFileInTabs(useTabs);

    if (expectPrintDialog || externalProcessExpectPrintDialog) {
        const int expectedTab = externalProcessExpectPrintDialog && !unique ? 1 : 0;
        helper.reset(new ClosePrintDialogHelper(expectedTab));
        QTimer::singleShot(0, helper.data(), &ClosePrintDialogHelper::closePrintDialog);
    }

    Okular::Status status = Okular::main(paths, serializedOptions);
    QCOMPARE(status, Okular::Success);
    Shell *s = findShell();
    QVERIFY(s);

    if (paths.count() == 1) {
        QCOMPARE(s->m_tabs.count(), 1);
        Okular::Part *part = s->findChild<Okular::Part *>();
        QVERIFY(part);
        QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(paths[0]));
        QCOMPARE(partDocument(part)->currentPage(), expectedPage);
        // Testing if the bar is shown or hidden as expected
        QCOMPARE(findWidget(part)->isHidden(), externalProcessExpectFind.isEmpty());
        QCOMPARE(findWidget(part)->findChild<KLineEdit *>()->text(), externalProcessExpectFind);
        // Checking if the encryption/decryption worked
        QCOMPARE(externalProcessExpectFind, ShellUtils::find(serializedOptions));

    } else if (paths.count() == 2) {
        if (useTabs) {
            Shell *s = findShell();
            QVERIFY(s);
            QCOMPARE(s->m_tabs.count(), 2);
            Okular::Part *part = dynamic_cast<Okular::Part *>(s->m_tabs[0].part);
            Okular::Part *part2 = dynamic_cast<Okular::Part *>(s->m_tabs[1].part);
            QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(paths[0]));
            QCOMPARE(part2->url().url(), QStringLiteral("file://%1").arg(paths[1]));
            QCOMPARE(partDocument(part)->currentPage(), expectedPage);
            QCOMPARE(partDocument(part2)->currentPage(), expectedPage);
        } else {
            QSet<QString> openUrls;
            Shell *s = findShell();
            QVERIFY(s);
            QCOMPARE(s->m_tabs.count(), 1);
            Okular::Part *part = s->findChild<Okular::Part *>();
            QVERIFY(part);
            QCOMPARE(partDocument(part)->currentPage(), expectedPage);
            openUrls << part->url().url();

            Shell *s2 = findShell(s);
            QVERIFY(s2);
            QCOMPARE(s2->m_tabs.count(), 1);
            Okular::Part *part2 = s2->findChild<Okular::Part *>();
            QVERIFY(part2);
            QCOMPARE(partDocument(part2)->currentPage(), expectedPage);
            openUrls << part2->url().url();

            for (const QString &path : std::as_const(paths)) {
                QVERIFY(openUrls.contains(QStringLiteral("file://%1").arg(path)));
            }
        }
    }

    if (!externalProcessPath.isEmpty()) {
        Okular::Part *part = s->findChild<Okular::Part *>();

        QProcess p;
        QStringList args;
        args << externalProcessPath;
        if (unique) {
            args << QStringLiteral("-unique");
        }
        if (externalProcessExpectedPage != 0) {
            args << QStringLiteral("-page") << QString::number(externalProcessExpectedPage + 1);
        }
        if (externalProcessExpectPresentation) {
            args << QStringLiteral("-presentation");
        }
        if (externalProcessExpectPrintDialog) {
            args << QStringLiteral("-print");
        }
        p.start(QStringLiteral(OKULAR_BINARY), args);
        p.waitForStarted();
        QCOMPARE(p.state(), QProcess::Running);

        if (useTabs || unique) {
            // It is attaching to us, so will eventually stop
            QTRY_COMPARE_WITH_TIMEOUT(p.state(), QProcess::NotRunning, 20000);
            QCOMPARE(p.exitStatus(), QProcess::NormalExit);
            QCOMPARE(p.exitCode(), 0);

            if (unique) {
                // It is unique so part got "overwritten"
                QCOMPARE(s->m_tabs.count(), 1);
                QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(externalProcessPath));
                QCOMPARE(partDocument(part)->currentPage(), externalProcessExpectedPage);
            } else {
                // It is attaching to us so a second tab is there
                QCOMPARE(s->m_tabs.count(), 2);
                Okular::Part *part2 = dynamic_cast<Okular::Part *>(s->m_tabs[1].part);
                QCOMPARE(part2->url().url(), QStringLiteral("file://%1").arg(externalProcessPath));
                QCOMPARE(partDocument(part2)->currentPage(), externalProcessExpectedPage);
            }
        } else {
            QTest::qWait(750);

            // It opened on a new process, so it is still running, we need to kill it
            QCOMPARE(p.state(), QProcess::Running);
            p.terminate();
            p.waitForFinished();
            QVERIFY(p.state() != QProcess::Running);
            // It opened on a new process, so no change for us
            QCOMPARE(s->m_tabs.count(), 1);
            QCOMPARE(part->url().url(), QStringLiteral("file://%1").arg(paths[0]));
            QCOMPARE(partDocument(part)->currentPage(), externalProcessExpectedPage);
        }
    }

    if (expectPresentation) {
        QCOMPARE(paths.count(), 1);
        Okular::Part *part = s->findChild<Okular::Part *>();
        QTRY_VERIFY(presentationWidget(part) != nullptr);
    }

    if (externalProcessExpectPresentation) {
        Okular::Part *part;
        if (unique) {
            QCOMPARE(s->m_tabs.count(), 1);
            part = dynamic_cast<Okular::Part *>(s->m_tabs[0].part);
        } else {
            QCOMPARE(s->m_tabs.count(), 2);
            part = dynamic_cast<Okular::Part *>(s->m_tabs[1].part);
        }

        QTRY_VERIFY(presentationWidget(part) != nullptr);
    }

    if (helper) {
        QVERIFY(helper->foundDialog);
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
    QCOMPARE(MainShellTestA::tabWidget(s)->currentIndex(), m_expectedTab);
    dialog->close();
    foundDialog = true;
}

void MainShellTestA::test2FilesError_data()
{
    QTest::addColumn<QString>("serializedOptions");

    QTest::newRow("startInPresentation") << ShellUtils::serializeOptions(true, false, false, false, false, QString(), QString(), QString());
    QTest::newRow("showPrintDialog") << ShellUtils::serializeOptions(false, true, false, false, false, QString(), QString(), QString());
#if HAVE_DBUS
    QTest::newRow("unique") << ShellUtils::serializeOptions(false, false, false, true, false, QString(), QString(), QString());
    QTest::newRow("pageNumber") << ShellUtils::serializeOptions(false, false, false, false, false, QStringLiteral("3"), QString(), QString());
#endif // HAVE_DBUS
    QTest::newRow("find") << ShellUtils::serializeOptions(false, false, false, false, false, QString(), QStringLiteral("silly"), QString());
}

void MainShellTestA::test2FilesError()
{
    QFETCH(QString, serializedOptions);

    QStringList paths;
    paths << QStringLiteral(KDESRCDIR "data/file1.pdf") << QStringLiteral(KDESRCDIR "data/tocreload.pdf");
    Okular::Status status = Okular::main(paths, serializedOptions);
    QCOMPARE(status, Okular::Error);

    Shell *s = findShell();
    QVERIFY(!s);
}

void MainShellTestA::testOpenInvalidFiles_data()
{
    QTest::addColumn<QList<QUrl>>("files");
    QTest::addColumn<QString>("options");

    QString options = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());
    QUrl validFile1 = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/file1.pdf"), ShellUtils::qfileExistFunc(), QString());
    QUrl validFile2 = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/file2.pdf"), ShellUtils::qfileExistFunc(), QString());
    QUrl invalidFile = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/non-existing-doc.pdf"), ShellUtils::qfileExistFunc(), QString());

    QList<QUrl> firstCase {invalidFile, validFile1, validFile2};
    QList<QUrl> secondCase {validFile1, validFile2, invalidFile};

    QTest::newRow("opening the invalid file first") << firstCase << options;
    QTest::newRow("opening the valids file first") << secondCase << options;
}

void MainShellTestA::testOpenInvalidFiles()
{
    QFETCH(QList<QUrl>, files);
    QFETCH(QString, options);

    /*
     *  The purpose of this test is to verify that when we open an invalid file, no tab is created in the
     * shell.
     *
     */

    Okular::Settings::self()->setShellOpenFileInTabs(true);
    Okular::Status status = Okular::main(QStringList(), options);
    QCOMPARE(status, Okular::Success);

    Shell *shell = findShell();
    QVERIFY(shell);

    /*
     *  We need to make sure that the KrecentFilesAction is empty before starting, because we will also test that
     * the file gets removed from the recent documents
     *
     */
    shell->m_recent->clear();

    QScopedPointer<TestingUtils::CloseDialogHelper> closeDialogHelper {new TestingUtils::CloseDialogHelper(QDialogButtonBox::StandardButton::Ok)};

    for (const QUrl &file : files) {
        shell->openUrl(file);
    }

    QList<QUrl> recentFiles = shell->m_recent->urls();

    QVERIFY(shell->m_tabs.size() == 2);
    QVERIFY(shell->m_tabWidget->tabBar()->isVisible());

    QVERIFY(!shell->m_tabWidget->tabIcon(0).isNull());
    QVERIFY(!shell->m_tabWidget->tabIcon(1).isNull());

    QVERIFY(recentFiles.size() == 2);
}

void MainShellTestA::testOpenTheSameFileSeveralTimes()
{
    QString options = ShellUtils::serializeOptions(false, false, false, false, false, QString(), QString(), QString());

    Okular::Settings::self()->setShellOpenFileInTabs(true);
    Okular::Status status = Okular::main(QStringList(), options);
    QCOMPARE(status, Okular::Success);

    Shell *shell = findShell();
    QVERIFY(shell);

    QUrl file1 = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/file1.pdf"), ShellUtils::qfileExistFunc(), QString());
    QUrl file2 = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/file2.pdf"), ShellUtils::qfileExistFunc(), QString());
    QUrl file3 = ShellUtils::urlFromArg(QStringLiteral(KDESRCDIR "data/formattest.pdf"), ShellUtils::qfileExistFunc(), QString());

    shell->openUrl(file1);
    shell->openUrl(file2);
    shell->openUrl(file2);

    QVERIFY(shell->m_tabs.size() == 2);

    QVERIFY(shell->m_tabWidget->currentIndex() == 1);

    Okular::Settings::self()->setSwitchToTabIfOpen(false);

    shell->openUrl(file2);

    QVERIFY(shell->m_tabs.size() == 3);

    shell->openUrl(file3);
    QVERIFY(shell->m_tabWidget->currentIndex() == 3);

    QVERIFY(shell->m_tabs.size() == 4);

    Okular::Settings::self()->setSwitchToTabIfOpen(true);
}

QTEST_MAIN(MainShellTestA)
#include "mainshelltest_a.moc"
