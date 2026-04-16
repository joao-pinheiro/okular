/*
    SPDX-FileCopyrightText: 2013 Albert Astals Cid <aacid@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// clazy:excludeall=qstring-allocations

#include <QSignalSpy>
#include <QTest>

#include "../core/annotations.h"
#include "../core/document_p.h"
#include "../core/form.h"
#include "../core/misc.h"
#include "../core/page.h"
#include "../part/pageview.h"
#include "../part/part.h"
#include "../part/presentationwidget.h"
#include "../part/sidebar.h"
#include "../part/toc.h"
#include "../settings.h"
#include "closedialoghelper.h"

#include <KActionCollection>
#include <KActionMenu>
#include <KConfigDialog>
#include <KParts/OpenUrlArguments>

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>

namespace Okular
{
class PartTestA : public QObject
{
    Q_OBJECT

    static bool openDocument(Okular::Part *part, const QString &filePath);

Q_SIGNALS:
    void urlHandler(const QUrl &url); // NOLINT(readability-inconsistent-declaration-parameter-name)

private Q_SLOTS:
    void init();

    void testZoomWithCrop();
    void testSelectText();
    void testSelectTextMultiline();
    void testCopyTextSelectionModes();
    void testCopyTextWithoutLineBreaksMultiline();
    void testMouseMoveOverLinkWhileInSelectionMode();
    void testClickUrlLinkWhileInSelectionMode();
    void testeTextSelectionOverAndAcrossLinks_data();
    void testeTextSelectionOverAndAcrossLinks();
    void testClickUrlLinkWhileLinkTextIsSelected();
    void testRClickWhileLinkTextIsSelected();
    void testRClickOverLinkWhileLinkTextIsSelected();
    void testRClickOnSelectionModeShoulShowFollowTheLinkMenu();
    void testClickAnywhereAfterSelectionShouldUnselect();
    void testeRectSelectionStartingOnLinks();
    void testCheckBoxReadOnly();
    void testCrashTextEditDestroy();
    void testAnnotWindow();
    void testAdditionalActionTriggers();
    void testTypewriterAnnotTool();
    void testJumpToPage();
    void testOpenAtPage();
    void testForwardBackwardNavigation();
    void testTabletProximityBehavior();
    void testOpenPrintPreview();
    void testMouseModeMenu();
    void testFullScreenRequest();
    void testZoomInFacingPages();
    void testLinkWithCrop();
    void testFieldFormatting();

private:
    void simulateMouseSelection(double startX, double startY, double endX, double endY, QWidget *target);
};

bool PartTestA::openDocument(Okular::Part *part, const QString &filePath)
{
    part->openDocument(filePath);
    return part->m_document->isOpened();
}

void PartTestA::init()
{
    // Default settings for every test
    Okular::Settings::self()->setDefaults();

    // Clean docdatas
    const QList<QUrl> urls = {QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/file1.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/file2.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/simple-multipage.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/tocreload.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/pdf_with_links.pdf")),
                              QUrl::fromUserInput(QStringLiteral("file://" KDESRCDIR "data/RequestFullScreen.pdf"))};

    for (const QUrl &url : urls) {
        QFileInfo fileReadTest(url.toLocalFile());
        const QString docDataPath = Okular::DocumentPrivate::docDataFileName(url, fileReadTest.size());
        QFile::remove(docDataPath);
    }
}

void PartTestA::testSelectText()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));
    part.widget()->show();

    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const int mouseY = height * 0.052;
    const int mouseStartX = width * 0.12;
    const int mouseEndX = width * 0.7;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::AsProvided)));

    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Hola que tal"));
}

void PartTestA::testSelectTextMultiline()
{
    // This test tests a specific variation of multiline selection
    // Select from middle to end of line, then continue to select next line
    // then move selection back on next line past the point of the first line
    // https://bugs.kde.org/show_bug.cgi?id=482249 has a nice animation.
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(1);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const int startY = height * 0.052;
    const int startX = width * 0.22;
    const int endY = height * 0.072;
    const int endX = width * 0.6;

    const int steps = 5;
    const double diffX = endX - startX;
    const double diffXStep = diffX / steps;

    QTestEventList events;
    events.addMouseMove(QPoint(startX, startY));
    events.addMousePress(Qt::LeftButton, Qt::NoModifier, QPoint(startX, startY));
    for (int i = 0; i < steps - 1; ++i) {
        events.addMouseMove(QPoint(startX + i * diffXStep, startY));
        events.addDelay(100);
    }
    events.addMouseMove(QPoint(endX, startY));
    events.addDelay(100);
    events.addMouseMove(QPoint(endX, endY));
    events.addDelay(100);
    for (int i = 0; i < (steps); i++) {
        events.addMouseMove(QPoint(endX - (i * diffXStep), endY));
        events.addDelay(100);
    }
    events.addMouseMove(QPoint(endX - (diffXStep * (steps)), endY));
    events.addDelay(100);
    events.addMouseMove(QPoint(endX - (diffXStep * (steps + 0.5)), endY));
    events.addDelay(100);

    events.addMouseRelease(Qt::LeftButton, Qt::NoModifier, QPoint(endX - (diffXStep * (steps + 0.5)), endY));

    events.simulate(part.m_pageView->viewport());

    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::AsProvided)));

    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("cks!\nOf c"));
}

void PartTestA::testCopyTextSelectionModes()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }
    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const int mouseY = height * 0.052;
    const int mouseStartX = width * 0.12;
    const int mouseEndX = width * 0.7;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    QApplication::clipboard()->clear();

    // Test AsProvided mode (default behavior)
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::AsProvided)));
    QString rawText = QApplication::clipboard()->text();
    QCOMPARE(rawText, QStringLiteral("Hola que tal"));

    // Test WithoutLineBreaks mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::WithoutLineBreaks)));
    QString cleanText = QApplication::clipboard()->text();

    // Verify clean text is not changing this text, since it's single-line
    QCOMPARE(cleanText, QStringLiteral("Hola que tal"));
    QVERIFY(!cleanText.isEmpty());
}

void PartTestA::testCopyTextWithoutLineBreaksMultiline()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);

    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }
    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(1);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const int startY = height * 0.052;
    const int startX = width * 0.22;
    const int endY = height * 0.072;
    const int endX = width * 0.6;

    simulateMouseSelection(startX, startY, endX, endY, part.m_pageView->viewport());

    QApplication::clipboard()->clear();

    // Test AsProvided mode - text should contain line breaks
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::AsProvided)));
    QString rawText = QApplication::clipboard()->text();

    qDebug() << "Selected text:" << rawText;
    QVERIFY(!rawText.isEmpty());

    // Clear and test WithoutLineBreaks mode
    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection", Q_ARG(PageView::TextCopyMode, PageView::TextCopyMode::WithoutLineBreaks)));
    QString cleanText = QApplication::clipboard()->text();

    QVERIFY(!cleanText.isEmpty());

    // If original had newlines...
    if (rawText.contains(QLatin1Char('\n'))) {
        int rawNewlineCount = rawText.count(QLatin1Char('\n'));
        int cleanNewlineCount = cleanText.count(QLatin1Char('\n'));

        // ...The clean version should have fewer newlines!
        QVERIFY(cleanNewlineCount <= rawNewlineCount);

        // Verify no hyphen-newline combinations remain
        QVERIFY(!cleanText.contains(QStringLiteral("-\n")));
        QVERIFY(!cleanText.contains(QStringLiteral("- \n")));
    }

    QCOMPARE(cleanText, QStringLiteral("cks! Of course it does"));
}

// cursor switches to Hand when hovering over link in TextSelect mode.
void PartTestA::testMouseMoveOverLinkWhileInSelectionMode()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    // move mouse over link
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.250, height * 0.127));

    // check if mouse icon changed to proper icon
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::PointingHandCursor);
}

// clicking on hyperlink jumps to destination in TextSelect mode.
void PartTestA::testClickUrlLinkWhileInSelectionMode()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    // overwrite urlHandler for 'mailto' urls
    QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "urlHandler");
    QSignalSpy openUrlSignalSpy(this, &PartTest::urlHandler);

    // click on url
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.250, height * 0.127));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * 0.250, height * 0.127));

    // expect that the urlHandler signal was called
    QTRY_COMPARE(openUrlSignalSpy.count(), 1);
    QList<QVariant> arguments = openUrlSignalSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<QUrl>(), QUrl(QStringLiteral("mailto:foo@foo.bar")));
}

void PartTestA::testeTextSelectionOverAndAcrossLinks_data()
{
    QTest::addColumn<double>("mouseStartX");
    QTest::addColumn<double>("mouseEndX");
    QTest::addColumn<QString>("expectedResult");

    // can text-select "over and across" hyperlink.
    QTest::newRow("start selection before link") << 0.1564 << 0.2943 << QStringLiteral(" a link: foo@foo.b");
    // can text-select starting at text and ending selection in middle of hyperlink.
    QTest::newRow("start selection in the middle of the link") << 0.28 << 0.382 << QStringLiteral("o.bar");
    // text selection works when selecting left to right or right to left
    QTest::newRow("start selection after link") << 0.40 << 0.05 << QStringLiteral("This is a link: foo@foo.bar");
}

// can text-select "over and across" hyperlink.
void PartTestA::testeTextSelectionOverAndAcrossLinks()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const double mouseY = height * 0.127;
    QFETCH(double, mouseStartX);
    QFETCH(double, mouseEndX);

    mouseStartX = width * mouseStartX;
    mouseEndX = width * mouseEndX;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection"));

    QFETCH(QString, expectedResult);
    QCOMPARE(QApplication::clipboard()->text(), expectedResult);
}

// can jump to link while there's an active selection of text.
void PartTestA::testClickUrlLinkWhileLinkTextIsSelected()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const double mouseY = height * 0.127;
    const double mouseStartX = width * 0.13;
    const double mouseEndX = width * 0.40;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    // overwrite urlHandler for 'mailto' urls
    QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "urlHandler");
    QSignalSpy openUrlSignalSpy(this, &PartTest::urlHandler);

    // click on url
    const double mouseClickX = width * 0.2997;
    const double mouseClickY = height * 0.1293;

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(mouseClickX, mouseClickY));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(mouseClickX, mouseClickY), 1000);

    // expect that the urlHandler signal was called
    QTRY_COMPARE(openUrlSignalSpy.count(), 1);
    QList<QVariant> arguments = openUrlSignalSpy.takeFirst();
    QCOMPARE(arguments.at(0).value<QUrl>(), QUrl(QStringLiteral("mailto:foo@foo.bar")));
}

// r-click on the selected text gives the "Go To:" content menu option
void PartTestA::testRClickWhileLinkTextIsSelected()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const double mouseY = height * 0.162;
    const double mouseStartX = width * 0.42;
    const double mouseEndX = width * 0.60;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    // Need to do this because the pop-menu will have his own mainloop and will block tests until
    // the menu disappear
    PageView *view = part.m_pageView;
    bool menuClosed = false;
    QTimer::singleShot(2000, view, [view, &menuClosed]() {
        // check if popup menu is active and visible
        QMenu *menu = qobject_cast<QMenu *>(view->findChild<QMenu *>(QStringLiteral("PopupMenu")));
        QVERIFY(menu);
        QVERIFY(menu->isVisible());

        // check if the menu contains go-to link action
        QAction *goToAction = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("GoToAction")));
        QVERIFY(goToAction);

        // check if the "follow this link" action is not visible
        QAction *processLinkAction = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("ProcessLinkAction")));
        QVERIFY(!processLinkAction);

        // check if the "copy link address" action is not visible
        QAction *copyLinkLocation = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("CopyLinkLocationAction")));
        QVERIFY(!copyLinkLocation);

        // close menu to continue test
        menu->close();
        menuClosed = true;
    });

    // click on url
    const double mouseClickX = width * 0.425;
    const double mouseClickY = height * 0.162;

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(mouseClickX, mouseClickY));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::RightButton, Qt::NoModifier, QPoint(mouseClickX, mouseClickY), 1000);

    // will continue after pop-menu get closed
    QTRY_VERIFY(menuClosed);
}

// r-click on the link gives the "follow this link" content menu option
void PartTestA::testRClickOverLinkWhileLinkTextIsSelected()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const double mouseY = height * 0.162;
    const double mouseStartX = width * 0.42;
    const double mouseEndX = width * 0.60;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    // Need to do this because the pop-menu will have his own mainloop and will block tests until
    // the menu disappear
    PageView *view = part.m_pageView;
    bool menuClosed = false;
    QTimer::singleShot(2000, view, [view, &menuClosed]() {
        // check if popup menu is active and visible
        QMenu *menu = qobject_cast<QMenu *>(view->findChild<QMenu *>(QStringLiteral("PopupMenu")));
        QVERIFY(menu);
        QVERIFY(menu->isVisible());

        // check if the menu contains "follow this link" action
        QAction *processLinkAction = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("ProcessLinkAction")));
        QVERIFY(processLinkAction);

        // check if the menu contains "copy link address" action
        QAction *copyLinkLocation = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("CopyLinkLocationAction")));
        QVERIFY(copyLinkLocation);

        // close menu to continue test
        menu->close();
        menuClosed = true;
    });

    // click on url
    const double mouseClickX = width * 0.593;
    const double mouseClickY = height * 0.162;

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(mouseClickX, mouseClickY));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::RightButton, Qt::NoModifier, QPoint(mouseClickX, mouseClickY), 1000);

    // will continue after pop-menu get closed
    QTRY_VERIFY(menuClosed);
}

void PartTestA::testRClickOnSelectionModeShoulShowFollowTheLinkMenu()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    // Need to do this because the pop-menu will have his own mainloop and will block tests until
    // the menu disappear
    PageView *view = part.m_pageView;
    bool menuClosed = false;
    QTimer::singleShot(2000, view, [view, &menuClosed]() {
        // check if popup menu is active and visible
        QMenu *menu = qobject_cast<QMenu *>(view->findChild<QMenu *>(QStringLiteral("PopupMenu")));
        QVERIFY(menu);
        QVERIFY(menu->isVisible());

        // check if the menu contains "Follow this link" action
        QAction *processLink = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("ProcessLinkAction")));
        QVERIFY(processLink);

        // chek if the menu contains  "Copy Link Address" action
        QAction *actCopyLinkLocation = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("CopyLinkLocationAction")));
        QVERIFY(actCopyLinkLocation);

        // close menu to continue test
        menu->close();
        menuClosed = true;
    });

    // r-click on url
    const double mouseClickX = width * 0.604;
    const double mouseClickY = height * 0.162;

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(mouseClickX, mouseClickY));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::RightButton, Qt::NoModifier, QPoint(mouseClickX, mouseClickY), 1000);

    // will continue after pop-menu get closed
    QTRY_VERIFY(menuClosed);
}

void PartTestA::testClickAnywhereAfterSelectionShouldUnselect()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // resize window to avoid problem with selection areas
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseTextSelect"));

    const double mouseY = height * 0.162;
    const double mouseStartX = width * 0.42;
    const double mouseEndX = width * 0.60;

    simulateMouseSelection(mouseStartX, mouseY, mouseEndX, mouseY, part.m_pageView->viewport());

    // click on url
    const double mouseClickX = width * 0.10;

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(mouseClickX, mouseY));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(mouseClickX, mouseY), 1000);

    QApplication::clipboard()->clear();
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "copyTextSelection"));

    // check if copied text is empty what means no text selected
    QVERIFY(QApplication::clipboard()->text().isEmpty());
}

void PartTestA::testeRectSelectionStartingOnLinks()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_links.pdf")));
    // hide info messages as they interfere with selection area
    Okular::Settings::self()->setShowEmbeddedContentMessages(false);
    Okular::Settings::self()->setShowOSD(false);

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // enter text-selection mode
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseSelect"));

    const double mouseStartY = height * 0.127;
    const double mouseEndY = height * 0.127;
    const double mouseStartX = width * 0.28;
    const double mouseEndX = width * 0.382;

    // Need to do this because the pop-menu will have his own mainloop and will block tests until
    // the menu disappear
    PageView *view = part.m_pageView;
    bool menuClosed = false;
    QTimer::singleShot(2000, view, [view, &menuClosed]() {
        QApplication::clipboard()->clear();

        // check if popup menu is active and visible
        QMenu *menu = qobject_cast<QMenu *>(view->findChild<QMenu *>(QStringLiteral("PopupMenu")));
        QVERIFY(menu);
        QVERIFY(menu->isVisible());

        // check if the copy selected text to clipboard is present
        QAction *copyAct = qobject_cast<QAction *>(menu->findChild<QAction *>(QStringLiteral("CopyTextToClipboard")));
        QVERIFY(copyAct);

        menu->close();
        menuClosed = true;
    });

    simulateMouseSelection(mouseStartX, mouseStartY, mouseEndX, mouseEndY, part.m_pageView->viewport());

    // wait menu get closed
    QTRY_VERIFY(menuClosed);
}

void PartTestA::simulateMouseSelection(double startX, double startY, double endX, double endY, QWidget *target)
{
    const int steps = 5;
    const double diffX = endX - startX;
    const double diffY = endY - startY;
    const double diffXStep = diffX / steps;
    const double diffYStep = diffY / steps;

    QTestEventList events;
    events.addMouseMove(QPoint(startX, startY));
    events.addMousePress(Qt::LeftButton, Qt::NoModifier, QPoint(startX, startY));
    for (int i = 0; i < steps - 1; ++i) {
        events.addMouseMove(QPoint(startX + i * diffXStep, startY + i * diffYStep));
        events.addDelay(100);
    }
    events.addMouseMove(QPoint(endX, endY));
    events.addDelay(100);
    events.addMouseRelease(Qt::LeftButton, Qt::NoModifier, QPoint(endX, endY));

    events.simulate(target);
}

void PartTestA::testCheckBoxReadOnly()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/checkbox_ro.pdf");
    Okular::Part part(nullptr, {});
    part.openDocument(testFile);

    // The test document uses the activation action of checkboxes
    // to update the read only state. For this we need the part so that
    // undo / redo activates the activation action.

    QVERIFY(part.m_document->isOpened());

    const Okular::Page *page = part.m_document->page(0);

    QMap<QString, Okular::FormField *> fields;

    // Field names in test document are:
    // CBMakeRW, CBMakeRO, TargetDefaultRO, TargetDefaultRW

    const QList<Okular::FormField *> pageFormFields = page->formFields();
    for (Okular::FormField *ff : pageFormFields) {
        fields.insert(ff->name(), static_cast<Okular::FormField *>(ff));
    }

    // First grab all fields and check that the setup is as expected.
    auto cbMakeRW = dynamic_cast<Okular::FormFieldButton *>(fields[QStringLiteral("CBMakeRW")]);
    auto cbMakeRO = dynamic_cast<Okular::FormFieldButton *>(fields[QStringLiteral("CBMakeRO")]);

    auto targetDefaultRW = dynamic_cast<Okular::FormFieldText *>(fields[QStringLiteral("TargetDefaultRw")]);
    auto targetDefaultRO = dynamic_cast<Okular::FormFieldText *>(fields[QStringLiteral("TargetDefaultRo")]);

    QVERIFY(cbMakeRW);
    QVERIFY(cbMakeRO);
    QVERIFY(targetDefaultRW);
    QVERIFY(targetDefaultRO);

    QVERIFY(!cbMakeRW->state());
    QVERIFY(!cbMakeRO->state());

    QVERIFY(!targetDefaultRW->isReadOnly());
    QVERIFY(targetDefaultRO->isReadOnly());

    QList<Okular::FormFieldButton *> btns;
    btns << cbMakeRW << cbMakeRO;

    // Now check both boxes
    QList<bool> btnStates;
    btnStates << true << true;

    part.m_document->editFormButtons(0, btns, btnStates);

    // Read only should be inverted
    QVERIFY(targetDefaultRW->isReadOnly());
    QVERIFY(!targetDefaultRO->isReadOnly());

    // Test that undo / redo works
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(!targetDefaultRW->isReadOnly());
    QVERIFY(targetDefaultRO->isReadOnly());

    part.m_document->redo();
    QVERIFY(targetDefaultRW->isReadOnly());
    QVERIFY(!targetDefaultRO->isReadOnly());

    btnStates.clear();
    btnStates << false << true;

    part.m_document->editFormButtons(0, btns, btnStates);
    QVERIFY(targetDefaultRW->isReadOnly());
    QVERIFY(targetDefaultRO->isReadOnly());

    // Now set both to checked again and confirm that
    // save / load works.
    btnStates.clear();
    btnStates << true << true;
    part.m_document->editFormButtons(0, btns, btnStates);

    QTemporaryFile saveFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
    QVERIFY(saveFile.open());
    saveFile.close();

    // Save
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), Part::NoSaveAsFlags));
    part.closeUrl();

    // Load
    part.openDocument(saveFile.fileName());
    QVERIFY(part.m_document->isOpened());

    page = part.m_document->page(0);

    fields.clear();

    {
        const QList<Okular::FormField *> pageFormFields = page->formFields();
        for (Okular::FormField *ff : pageFormFields) {
            fields.insert(ff->name(), static_cast<Okular::FormField *>(ff));
        }
    }

    cbMakeRW = dynamic_cast<Okular::FormFieldButton *>(fields[QStringLiteral("CBMakeRW")]);
    cbMakeRO = dynamic_cast<Okular::FormFieldButton *>(fields[QStringLiteral("CBMakeRO")]);

    targetDefaultRW = dynamic_cast<Okular::FormFieldText *>(fields[QStringLiteral("TargetDefaultRw")]);
    targetDefaultRO = dynamic_cast<Okular::FormFieldText *>(fields[QStringLiteral("TargetDefaultRo")]);

    QVERIFY(cbMakeRW->state());
    QVERIFY(cbMakeRO->state());
    QVERIFY(targetDefaultRW->isReadOnly());
    QVERIFY(!targetDefaultRO->isReadOnly());
}

void PartTestA::testCrashTextEditDestroy()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/formSamples.pdf");
    Okular::Part part(nullptr, {});
    part.openDocument(testFile);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.widget()->findChild<QTextEdit *>()->setText(QStringLiteral("HOLA"));
    part.actionCollection()->action(QStringLiteral("view_toggle_forms"))->trigger();
}

void PartTestA::testTypewriterAnnotTool()
{
    Okular::Part part(nullptr, QVariantList());

    part.openUrl(QUrl::fromLocalFile(QStringLiteral(KDESRCDIR "data/file1.pdf")));

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    part.m_document->setViewportPage(0);

    // Find the TypeWriter annotation
    QAction *typeWriterAction = part.actionCollection()->action(QStringLiteral("annotation_typewriter"));
    QVERIFY(typeWriterAction);

    typeWriterAction->trigger();

    QTest::qWait(1000); // Wait for the "add new note" dialog to appear
    TestingUtils::CloseDialogHelper closeDialogHelper(QDialogButtonBox::Ok);

    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * 0.5, height * 0.2));

    Annotation *annot = part.m_document->page(0)->annotations().constFirst();
    TextAnnotation *ta = static_cast<TextAnnotation *>(annot);
    QVERIFY(annot);
    QVERIFY(ta);
    QCOMPARE(annot->subType(), Okular::Annotation::AText);
    QCOMPARE(annot->style().color(), QColor(255, 255, 255, 0));
    QCOMPARE(ta->textType(), Okular::TextAnnotation::InPlace);
    QCOMPARE(ta->inplaceIntent(), Okular::TextAnnotation::TypeWriter);
}

void PartTestA::testJumpToPage()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/simple-multipage.pdf");
    const int targetPage = 25;
    Okular::Part part(nullptr, QVariantList());
    part.openDocument(testFile);
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->pages();
    part.m_document->setViewportPage(targetPage);

    /* Document::setViewportPage triggers pixmap rendering in another thread.
     * We want to test how things look AFTER finished signal arrives back,
     * because PageView::slotRelayoutPages may displace the viewport again.
     */
    QTRY_VERIFY(part.m_document->page(targetPage)->hasPixmap(part.m_pageView));

    const int contentAreaHeight = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();
    const int pageWithSpaceTop = contentAreaHeight / part.m_document->pages() * targetPage;

    /*
     * This is a test for a "known by trial" displacement.
     * We'd need access to part.m_pageView->d->items[targetPage]->croppedGeometry().top(),
     * to determine the expected viewport position, but we don't have access.
     */
    QCOMPARE(part.m_pageView->verticalScrollBar()->value(), pageWithSpaceTop - 4);
}

void PartTestA::testOpenAtPage()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/simple-multipage.pdf");
    QUrl url = QUrl::fromLocalFile(testFile);
    Okular::Part part(nullptr, QVariantList());

    const uint targetPageNumA = 25;
    const uint expectedPageA = targetPageNumA - 1;
    url.setFragment(QString::number(targetPageNumA));
    part.openUrl(url);
    QCOMPARE(part.m_document->currentPage(), expectedPageA);

    // 'page=<pagenum>' param as specified in RFC 3778
    const uint targetPageNumB = 15;
    const uint expectedPageB = targetPageNumB - 1;
    url.setFragment(QStringLiteral("page=") + QString::number(targetPageNumB));
    part.openUrl(url);
    QCOMPARE(part.m_document->currentPage(), expectedPageB);
}

void PartTestA::testForwardBackwardNavigation()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/simple-multipage.pdf");
    Okular::Part part(nullptr, QVariantList());
    part.openDocument(testFile);
    part.widget()->resize(800, 600);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    // Go to some page
    const int targetPageA = 15;
    part.m_document->setViewportPage(targetPageA);

    QVERIFY(part.m_document->viewport() == DocumentViewport(targetPageA));

    // Go to some other page
    const int targetPageB = 25;
    part.m_document->setViewportPage(targetPageB);
    QVERIFY(part.m_document->viewport() == DocumentViewport(targetPageB));

    // Go back to page A
    QVERIFY(QMetaObject::invokeMethod(&part, "slotHistoryBack"));
    QVERIFY(part.m_document->viewport().pageNumber == targetPageA);

    // Go back to page B
    QVERIFY(QMetaObject::invokeMethod(&part, "slotHistoryNext"));
    QVERIFY(part.m_document->viewport().pageNumber == targetPageB);
}

void PartTestA::testTabletProximityBehavior()
{
    QVariantList dummyArgs;
    Okular::Part part {nullptr, dummyArgs};
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));
    part.slotShowPresentation();
    PresentationWidget *w = part.m_presentationWidget;
    QVERIFY(w);
    part.widget()->show();

    // close the KMessageBox "There are two ways of exiting[...]"
    TestingUtils::CloseDialogHelper closeDialogHelper(w, QDialogButtonBox::Ok); // confirm the "To leave, press ESC"

    auto pointingDevice = new QPointingDevice(QStringLiteral("test"), 42, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen, QInputDevice::Capability::All, 3, 3);
    QTabletEvent enterProximityEvent {QEvent::TabletEnterProximity, pointingDevice, QPointF(10, 10), QPointF(10, 10), 1., 0, 0, 1., 1., 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton};
    QTabletEvent leaveProximityEvent {QEvent::TabletLeaveProximity, pointingDevice, QPointF(10, 10), QPointF(10, 10), 1., 0, 0, 1., 1., 0, Qt::NoModifier, Qt::NoButton, Qt::NoButton};

    // Test with the Okular::Settings::EnumSlidesCursor::Visible setting
    Okular::Settings::self()->setSlidesCursor(Okular::Settings::EnumSlidesCursor::Visible);

    // Send an enterProximity event
    qApp->notify(qApp, &enterProximityEvent);

    // The cursor should be a cross-hair
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::CrossCursor));

    // Send a leaveProximity event
    qApp->notify(qApp, &leaveProximityEvent);

    // After the leaveProximityEvent, the cursor should be an arrow again, because
    // we have set the slidesCursor mode to 'Visible'
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::ArrowCursor));

    // Test with the Okular::Settings::EnumSlidesCursor::Hidden setting
    Okular::Settings::self()->setSlidesCursor(Okular::Settings::EnumSlidesCursor::Hidden);

    qApp->notify(qApp, &enterProximityEvent);
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::CrossCursor));
    qApp->notify(qApp, &leaveProximityEvent);
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::BlankCursor));

    // Moving the mouse should not bring the cursor back
    QTest::mouseMove(w, QPoint(100, 100));
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::BlankCursor));

    // First test with the Okular::Settings::EnumSlidesCursor::HiddenDelay setting
    Okular::Settings::self()->setSlidesCursor(Okular::Settings::EnumSlidesCursor::HiddenDelay);

    qApp->notify(qApp, &enterProximityEvent);
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::CrossCursor));
    qApp->notify(qApp, &leaveProximityEvent);

    // After the leaveProximityEvent, the cursor should be blank, because
    // we have set the slidesCursor mode to 'HiddenDelay'
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::BlankCursor));

    // Moving the mouse should bring the cursor back
    QTest::mouseMove(w, QPoint(150, 150));
    QVERIFY(w->cursor().shape() == Qt::CursorShape(Qt::ArrowCursor));

    delete pointingDevice;
}

void PartTestA::testOpenPrintPreview()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));
    TestingUtils::CloseDialogHelper closeDialogHelper(QDialogButtonBox::Close);
    part.slotPrintPreview();
}

void PartTestA::testMouseModeMenu()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));

    QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseNormal");

    // Get mouse mode menu action
    QAction *mouseModeAction = part.actionCollection()->action(QStringLiteral("mouse_selecttools"));
    QVERIFY(mouseModeAction);
    QMenu *mouseModeActionMenu = mouseModeAction->menu();

    // Test that actions are usable (not disabled)
    QVERIFY(mouseModeActionMenu->actions().at(0)->isEnabled());
    QVERIFY(mouseModeActionMenu->actions().at(1)->isEnabled());
    QVERIFY(mouseModeActionMenu->actions().at(2)->isEnabled());

    // Test activating area selection mode
    mouseModeActionMenu->actions().at(0)->trigger();
    QCOMPARE(Okular::Settings::mouseMode(), (int)Okular::Settings::EnumMouseMode::RectSelect);

    // Test activating text selection mode
    mouseModeActionMenu->actions().at(1)->trigger();
    QCOMPARE(Okular::Settings::mouseMode(), (int)Okular::Settings::EnumMouseMode::TextSelect);

    // Test activating table selection mode
    mouseModeActionMenu->actions().at(2)->trigger();
    QCOMPARE(Okular::Settings::mouseMode(), (int)Okular::Settings::EnumMouseMode::TableSelect);
}

void PartTestA::testFullScreenRequest()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);

    // Open file.  For this particular file, a dialog has to appear asking whether
    // one wants to comply with the wish to go to presentation mode directly.
    // Answer 'no'
    auto dialogHelper = std::make_unique<TestingUtils::CloseDialogHelper>(&part, QDialogButtonBox::No);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/RequestFullScreen.pdf")));

    // Check that we are not in presentation mode
    QEXPECT_FAIL("", "The presentation widget should not be shown because we clicked No in the dialog", Continue);
    QTRY_VERIFY_WITH_TIMEOUT(part.m_presentationWidget, 1000);

    // Reload the file.  The initial dialog should no appear again.
    // (This is https://bugs.kde.org/show_bug.cgi?id=361740)
    part.reload();

    // Open the file again.  Now we answer "yes, go to presentation mode"
    dialogHelper = std::make_unique<TestingUtils::CloseDialogHelper>(&part, QDialogButtonBox::Yes);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/RequestFullScreen.pdf")));

    // Test whether we really are in presentation mode
    QTRY_VERIFY(part.m_presentationWidget);
}

void PartTestA::testZoomInFacingPages()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));
    QAction *facingAction = part.m_pageView->findChild<QAction *>(QStringLiteral("view_render_mode_facing"));
    KSelectAction *zoomSelectAction = part.m_pageView->findChild<KSelectAction *>(QStringLiteral("zoom_to"));
    part.widget()->resize(600, 400);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));
    facingAction->trigger();
    while (zoomSelectAction->currentText() != QStringLiteral("12%")) {
        QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomOut"));
    }
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomIn"));
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomIn"));
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomIn"));
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomIn"));
    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomIn"));
    QTRY_COMPARE(zoomSelectAction->currentText(), QStringLiteral("66%"));

    // Back to single mode
    part.m_pageView->findChild<QAction *>(QStringLiteral("view_render_mode_single"))->trigger();
}

void PartTestA::testZoomWithCrop()
{
    // We test that all zoom levels can be achieved with cropped pages, bug 342003

    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));

    KActionMenu *cropMenu = part.m_pageView->findChild<KActionMenu *>(QStringLiteral("view_trim_mode"));
    KToggleAction *cropAction = cropMenu->menu()->findChild<KToggleAction *>(QStringLiteral("view_trim_margins"));
    KSelectAction *zoomSelectAction = part.m_pageView->findChild<KSelectAction *>(QStringLiteral("zoom_to"));

    part.widget()->resize(600, 400);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    // Activate "Trim Margins"
    QVERIFY(!Okular::Settings::trimMargins());
    cropAction->trigger();
    QVERIFY(Okular::Settings::trimMargins());

    // Wait for the bounding boxes
    QTRY_VERIFY(part.m_document->page(0)->isBoundingBoxKnown());
    QTRY_VERIFY(part.m_document->page(1)->isBoundingBoxKnown());

    // Zoom out
    for (int i = 0; i < 20; i++) {
        QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotZoomOut"));
    }
    QCOMPARE(zoomSelectAction->currentText(), QStringLiteral("12%"));

    // Zoom in and out and check that all zoom levels appear
    QSet<QString> zooms_ref {QStringLiteral("12%"),
                             QStringLiteral("25%"),
                             QStringLiteral("33%"),
                             QStringLiteral("50%"),
                             QStringLiteral("66%"),
                             QStringLiteral("75%"),
                             QStringLiteral("100%"),
                             QStringLiteral("125%"),
                             QStringLiteral("150%"),
                             QStringLiteral("200%"),
                             QStringLiteral("400%"),
                             QStringLiteral("800%"),
                             QStringLiteral("1,600%"),
                             QStringLiteral("2,500%"),
                             QStringLiteral("5,000%"),
                             QStringLiteral("10,000%")};

    for (int j = 0; j < 2; j++) {
        QSet<QString> zooms;
        for (int i = 0; i < 18; i++) {
            zooms << zoomSelectAction->currentText();
            QVERIFY(QMetaObject::invokeMethod(part.m_pageView, j == 0 ? "slotZoomIn" : "slotZoomOut"));
        }

        QVERIFY(zooms.contains(zooms_ref));
    }

    // Deactivate "Trim Margins"
    QVERIFY(Okular::Settings::trimMargins());
    cropAction->trigger();
    QVERIFY(!Okular::Settings::trimMargins());
}

void PartTestA::testLinkWithCrop()
{
    // We test that link targets are correct with cropping, related to bug 198427

    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/pdf_with_internal_links.pdf")));

    KActionMenu *cropMenu = part.m_pageView->findChild<KActionMenu *>(QStringLiteral("view_trim_mode"));
    KToggleAction *cropAction = cropMenu->menu()->findChild<KToggleAction *>(QStringLiteral("view_trim_selection"));

    part.widget()->resize(600, 400);
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->viewport()->width();
    const int height = part.m_pageView->viewport()->height();

    // Move to a location without a link
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.1, width * 0.1));

    // The cursor should be normal
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::CursorShape(Qt::OpenHandCursor));

    // Activate "Trim Margins"
    cropAction->trigger();

    // The cursor should be a cross-hair
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::CursorShape(Qt::CrossCursor));

    const int mouseStartY = height * 0.2;
    const int mouseEndY = height * 0.8;
    const int mouseStartX = width * 0.2;
    const int mouseEndX = width * 0.8;

    // Trim the page
    simulateMouseSelection(mouseStartX, mouseStartY, mouseEndX, mouseEndY, part.m_pageView->viewport());

    // We seem to have a trimmed view where we just by sheer luck ends up with mouse over a link at least sometimes
    // So move the mouse
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.1, width * 0.1));

    // The cursor should be normal again
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::CursorShape(Qt::OpenHandCursor));

    // Click a link
    const QPoint click(width * 0.2, height * 0.2);
    QTest::mouseMove(part.m_pageView->viewport(), click);
    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, click);

    QTRY_VERIFY2_WITH_TIMEOUT(qAbs(part.m_document->viewport().rePos.normalizedY - 0.167102333237) < 0.01, qPrintable(QStringLiteral("We are at %1").arg(part.m_document->viewport().rePos.normalizedY)), 500);

    // Deactivate "Trim Margins"
    cropAction->trigger();
}

void PartTestA::testFieldFormatting()
{
    // Test field formatting. This has to be a parttest so that we
    // can properly test focus in / out which triggers formatting.
    const QString testFile = QStringLiteral(KDESRCDIR "data/fieldFormat.pdf");
    Okular::Part part(nullptr, QVariantList());
    part.openDocument(testFile);
    part.widget()->resize(800, 600);

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    // Field names in test document are:
    //
    // us_currency_fmt for formatting like "$ 1,234.56"
    // de_currency_fmt for formatting like "1.234,56 €"
    // de_simple_sum for calculation test and formatting like "1.234,56€"
    // date_mm_dd_yyyy for dates like "18/06/2018"
    // date_dd_mm_yyyy for dates like "06/18/2018"
    // percent_fmt for percent format like "100,00%" if you enter 1
    // time_HH_MM_fmt for times like "23:12"
    // time_HH_MM_ss_fmt for times like "23:12:34"
    // special_phone_number for an example of a special format selectable in Acrobat.
    QMap<QString, Okular::FormField *> fields;
    const Okular::Page *page = part.m_document->page(0);
    const auto formFields = page->formFields();
    for (Okular::FormField *ff : formFields) {
        fields.insert(ff->name(), static_cast<Okular::FormField *>(ff));
    }

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    part.actionCollection()->action(QStringLiteral("view_toggle_forms"))->trigger();

    // Note as of version 1.5:
    // The test document is prepared for future extensions to formatting for dates etc.
    // Currently we only have the number format to test.
    const auto ff_us = dynamic_cast<Okular::FormFieldText *>(fields.value(QStringLiteral("us_currency_fmt")));
    const auto ff_de = dynamic_cast<Okular::FormFieldText *>(fields.value(QStringLiteral("de_currency_fmt")));
    const auto ff_sum = dynamic_cast<Okular::FormFieldText *>(fields.value(QStringLiteral("de_simple_sum")));

    const QPoint usPos(width * 0.25, height * 0.025);
    const QPoint dePos(width * 0.25, height * 0.05);
    const QPoint deSumPos(width * 0.25, height * 0.075);

    const auto viewport = part.m_pageView->viewport();

    QVERIFY(viewport);

    auto usCurrencyWidget = dynamic_cast<QLineEdit *>(viewport->childAt(usPos));
    auto deCurrencyWidget = dynamic_cast<QLineEdit *>(viewport->childAt(dePos));
    auto sumCurrencyWidget = dynamic_cast<QLineEdit *>(viewport->childAt(deSumPos));

    // Check that the widgets were found at the right position
    QVERIFY(usCurrencyWidget);
    QVERIFY(deCurrencyWidget);
    QVERIFY(sumCurrencyWidget);

    QTest::mousePress(usCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(usCurrencyWidget->hasFocus());
    // locale is en_US for this test. Enter a value and check it.
    usCurrencyWidget->setText(QStringLiteral("1234.56"));
    // Check that the internal text matches
    QCOMPARE(ff_us->text(), QStringLiteral("1234.56"));

    // Now move the focus to trigger formatting.
    QTest::mousePress(deCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(deCurrencyWidget->hasFocus());

    QCOMPARE(usCurrencyWidget->text(), QStringLiteral("$ 1,234.56"));
    QCOMPARE(ff_us->text(), QStringLiteral("1234.56"));

    // And again with an invalid number
    QTest::mousePress(usCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(usCurrencyWidget->hasFocus());

    usCurrencyWidget->setText(QStringLiteral("131234.567"));
    QTest::mousePress(deCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(deCurrencyWidget->hasFocus());
    // Check that the internal text still contains it.
    QCOMPARE(ff_us->text(), QStringLiteral("131234.567"));

    // Just check that the text does not match the internal text.
    // We don't check for a concrete value to keep NaN handling flexible
    QVERIFY(ff_us->text() != usCurrencyWidget->text());

    // Move the focus back and modify it a bit more
    QTest::mousePress(usCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(usCurrencyWidget->hasFocus());

    usCurrencyWidget->setText(QStringLiteral("1234.567"));
    QTest::mousePress(deCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(deCurrencyWidget->hasFocus());

    QCOMPARE(usCurrencyWidget->text(), QStringLiteral("$ 1,234.57"));

    // Sum should already match
    QCOMPARE(sumCurrencyWidget->text(), QStringLiteral("1.234,57€"));

    // Set a text in the de field
    deCurrencyWidget->setText(QStringLiteral("1123234,567"));
    QTest::mousePress(usCurrencyWidget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    QTRY_VERIFY(usCurrencyWidget->hasFocus());

    QCOMPARE(deCurrencyWidget->text(), QStringLiteral("1.123.234,57 €"));
    QCOMPARE(ff_de->text(), QStringLiteral("1123234,567"));
    QCOMPARE(sumCurrencyWidget->text(), QStringLiteral("1.124.469,13€"));
    QCOMPARE(ff_sum->text(), QStringLiteral("1124469.1340000000782310962677002"));
}

} // namespace Okular

int main(int argc, char *argv[])
{
    // Force consistent locale
    QLocale locale(QStringLiteral("en_US.UTF-8"));
    if (locale == QLocale::c()) { // This is the way to check if the above worked
        locale = QLocale(QLocale::English, QLocale::UnitedStates);
    }

    QLocale::setDefault(locale);
    qputenv("LC_ALL", "en_US.UTF-8"); // For UNIX, third-party libraries

    // Ensure consistent configs/caches
    QTemporaryDir homeDir; // QTemporaryDir automatically cleans up when it goes out of scope
    Q_ASSERT(homeDir.isValid());
    QByteArray homePath = QFile::encodeName(homeDir.path());
    qDebug() << homePath;
    qputenv("USERPROFILE", homePath);
    qputenv("HOME", homePath);
    qputenv("XDG_DATA_HOME", QByteArray(homePath + "/.local"));
    qputenv("XDG_CONFIG_HOME", QByteArray(homePath + "/.kde-unit-test/xdg/config"));

    // Disable fancy debug output
    qunsetenv("QT_MESSAGE_PATTERN");

    Okular::Settings::instance(QStringLiteral("okularparttest"));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("okularparttest"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));
    app.setQuitOnLastWindowClosed(false);

    qRegisterMetaType<QUrl>(); /*as done by kapplication*/
    qRegisterMetaType<QList<QUrl>>();

    Okular::PartTest test;

    return QTest::qExec(&test, argc, argv);
}

#include "parttest_a.moc"
