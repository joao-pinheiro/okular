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
class PartTestB : public QObject
{
    Q_OBJECT

    static bool openDocument(Okular::Part *part, const QString &filePath);

private Q_SLOTS:
    void init();

    void testReload();
    void testCanceledReload();
    void testTOCReload();
    void testForwardPDF();
    void testForwardPDF_data();
    void testGeneratorPreferences();
    void testRemoveLineBreaks_data();
    void testRemoveLineBreaks();
    void testClickInternalLink();
    void testScrollBarAndMouseWheel();
    void testOpenUrlArguments();
    void test388288();
    void testSaveAs();
    void testSaveAs_data();
    void testSaveAsToNonExistingPath();
    void testSaveAsToSymlink();
    void testSaveIsSymlink();
    void testSidebarItemAfterSaving();
    void testViewModeSavingPerFile();
    void testSaveAsUndoStackAnnotations();
    void testSaveAsUndoStackAnnotations_data();
    void testSaveAsUndoStackForms();
    void testSaveAsUndoStackForms_data();
};

class PartThatHijacksQueryClose : public Okular::Part
{
    Q_OBJECT
public:
    PartThatHijacksQueryClose(QObject *parent, const QVariantList &args)
        : Okular::Part(parent, args)
        , behavior(PassThru)
    {
    }

    enum Behavior { PassThru, ReturnTrue, ReturnFalse };

    void setQueryCloseBehavior(Behavior new_behavior)
    {
        behavior = new_behavior;
    }

    bool queryClose() override
    {
        if (behavior == PassThru) {
            return Okular::Part::queryClose();
        } else { // ReturnTrue or ReturnFalse
            return (behavior == ReturnTrue);
        }
    }

private:
    Behavior behavior;
};

bool PartTestB::openDocument(Okular::Part *part, const QString &filePath)
{
    part->openDocument(filePath);
    return part->m_document->isOpened();
}

void PartTestB::init()
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

// Test that Okular doesn't crash after a successful reload
void PartTestB::testReload()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));
    part.reload();
    qApp->processEvents();
}

// Test that Okular doesn't crash after a canceled reload
void PartTestB::testCanceledReload()
{
    QVariantList dummyArgs;
    PartThatHijacksQueryClose part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));

    // When queryClose() returns false, the reload operation is canceled (as if
    // the user had chosen Cancel in the "Save changes?" message box)
    part.setQueryCloseBehavior(PartThatHijacksQueryClose::ReturnFalse);

    part.reload();

    qApp->processEvents();
}

void PartTestB::testTOCReload()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/tocreload.pdf")));
    QCOMPARE(part.m_toc->expandedNodes().count(), 0);
    part.m_toc->m_treeView->expandAll();
    QCOMPARE(part.m_toc->expandedNodes().count(), 3);
    part.reload();
    qApp->processEvents();
    QCOMPARE(part.m_toc->expandedNodes().count(), 3);
}

void PartTestB::testForwardPDF()
{
    QFETCH(QString, dir);

    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);

    // Create temp dir named like this: ${system temp dir}/${random string}/${dir}
    const QTemporaryDir tempDir;
    const QDir workDir(QDir(tempDir.path()).filePath(dir));
    workDir.mkpath(QStringLiteral("."));

    const QString pdfResult = workDir.path() + QStringLiteral("/synctextest.pdf");
    QVERIFY(QFile::copy(QStringLiteral(KDESRCDIR "data/synctextest.pdf"), pdfResult));
    const QString gzDestination = workDir.path() + QStringLiteral("/synctextest.synctex.gz");
    QVERIFY(QFile::copy(QStringLiteral(KDESRCDIR "data/synctextest.synctex.gz"), gzDestination));

    QVERIFY(openDocument(&part, pdfResult));
    part.m_document->setViewportPage(0);
    QCOMPARE(part.m_document->currentPage(), 0u);
    part.closeUrl();

    QUrl u(QUrl::fromLocalFile(pdfResult));
    // Update this if you regenerate the synctextest.pdf somewhere else
    u.setFragment(QStringLiteral("src:100/home/tsdgeos/devel/kde/okular/autotests/data/synctextest.tex"));
    part.openUrl(u);
    QCOMPARE(part.m_document->currentPage(), 1u);
}

void PartTestB::testForwardPDF_data()
{
    QTest::addColumn<QString>("dir");

    QTest::newRow("non-utf8") << QStringLiteral("synctextest");
    // QStringliteral is broken on windows with non ascii chars so using QString::fromUtf8
    QTest::newRow("utf8") << QString::fromUtf8("ßðđđŋßðđŋ");
}

void PartTestB::testGeneratorPreferences()
{
    KConfigDialog *dialog;
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);

    // Test that we don't crash while opening the dialog
    dialog = part.slotGeneratorPreferences();
    qApp->processEvents();
    delete dialog; // closes the dialog and recursively destroys all widgets

    // Test that we don't crash while opening a new instance of the dialog
    // This catches attempts to reuse widgets that have been destroyed
    dialog = part.slotGeneratorPreferences();
    qApp->processEvents();
    delete dialog;
}

void PartTestB::testRemoveLineBreaks_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("simple newline") << QStringLiteral("Hello\nWorld") << QStringLiteral("Hello World");

    QTest::newRow("hyphen-newline") << QStringLiteral("hyphen-\nated") << QStringLiteral("hyphenated");

    QTest::newRow("hyphen-space-newline") << QStringLiteral("hyphen- \nated") << QStringLiteral("hyphenated");

    QTest::newRow("double newline preserved") << QStringLiteral("Paragraph1\n\nParagraph2") << QStringLiteral("Paragraph1\n\nParagraph2");

    QTest::newRow("multiple spaces") << QStringLiteral("Hello   \n   World") << QStringLiteral("Hello World");

    QTest::newRow("empty string") << QString() << QString();
}

void PartTestB::testRemoveLineBreaks()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    // Directly test the string-cleanup function!
    QString result = Okular::removeLineBreaks(input);

    QCOMPARE(result, expected);
}

void PartTestB::testClickInternalLink()
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

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseNormal"));

    QCOMPARE(part.m_document->currentPage(), 0u);
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.17, height * 0.05));
    QTest::mouseClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * 0.17, height * 0.05));
    QTRY_COMPARE(part.m_document->currentPage(), 1u);

    // make sure cursor goes back to being an open hand again.  Bug 421437
    QTRY_COMPARE_WITH_TIMEOUT(part.m_pageView->cursor().shape(), Qt::OpenHandCursor, 1000);
}

// Test for bug 421159, which is: When scrolling down with the scroll bar
// followed by scrolling down with the mouse wheel, the mouse wheel scrolling
// will make the viewport jump back to the first page.
void PartTestB::testScrollBarAndMouseWheel()
{
    QVariantList dummyArgs;
    Okular::Part part(nullptr, dummyArgs);
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf")));
    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    // Make sure we are on the first page
    QCOMPARE(part.m_document->currentPage(), 0u);

    // Two clicks on the vertical scrollbar
    auto scrollBar = part.m_pageView->verticalScrollBar();

    QTest::mouseClick(scrollBar, Qt::LeftButton);
    QTest::qWait(QApplication::doubleClickInterval() * 2); // Wait a tiny bit
    QTest::mouseClick(scrollBar, Qt::LeftButton);

    // We have scrolled enough to be on the second page now
    QCOMPARE(part.m_document->currentPage(), 1u);

    // Scroll further down using the mouse wheel
    auto wheelDown = new QWheelEvent({}, {}, {}, {0, -150}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::postEvent(part.m_pageView->viewport(), wheelDown);

    // Wait a little for the scrolling to actually happen.
    // We should still be on the second page after that.
    QTest::qWait(1000);

    QCOMPARE(part.m_document->currentPage(), 1u);
}

void PartTestB::testSaveAsToNonExistingPath()
{
    Okular::Part part(nullptr, {});
    part.openDocument(QStringLiteral(KDESRCDIR "data/file1.pdf"));

    QString saveFilePath;
    {
        QTemporaryFile saveFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
        bool success = saveFile.open();
        QVERIFY(success);
        saveFilePath = saveFile.fileName();
        // QTemporaryFile is destroyed and the file it created is gone, this is a TOCTOU but who cares
    }

    QVERIFY(!QFileInfo::exists(saveFilePath));

    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFilePath), Part::NoSaveAsFlags));

    QFile::remove(saveFilePath);
}

void PartTestB::testSaveAsToSymlink()
{
#ifdef Q_OS_UNIX
    Okular::Part part(nullptr, {});
    part.openDocument(QStringLiteral(KDESRCDIR "data/file1.pdf"));

    QTemporaryFile newFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
    bool success = newFile.open();
    QVERIFY(success);

    QString linkFilePath;
    {
        QTemporaryFile linkFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
        success = linkFile.open();
        QVERIFY(success);
        linkFilePath = linkFile.fileName();
        // QTemporaryFile is destroyed and the file it created is gone, this is a TOCTOU but who cares
    }

    QFile::link(newFile.fileName(), linkFilePath);

    QVERIFY(QFileInfo(linkFilePath).isSymLink());

    QVERIFY(part.saveAs(QUrl::fromLocalFile(linkFilePath), Part::NoSaveAsFlags));

    QVERIFY(QFileInfo(linkFilePath).isSymLink());

    QFile::remove(linkFilePath);
#endif
}

void PartTestB::testSaveIsSymlink()
{
#ifdef Q_OS_UNIX
    Okular::Part part(nullptr, {});

    QString newFilePath;
    {
        QTemporaryFile newFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
        bool success = newFile.open();
        QVERIFY(success);
        newFilePath = newFile.fileName();
        // QTemporaryFile is destroyed and the file it created is gone, this is a TOCTOU but who cares
    }

    QFile::copy(QStringLiteral(KDESRCDIR "data/file1.pdf"), newFilePath);

    QString linkFilePath;
    {
        QTemporaryFile linkFile(QStringLiteral("%1/okrXXXXXX.pdf").arg(QDir::tempPath()));
        bool success = linkFile.open();
        QVERIFY(success);
        linkFilePath = linkFile.fileName();
        // QTemporaryFile is destroyed and the file it created is gone, this is a TOCTOU but who cares
    }

    QFile::link(newFilePath, linkFilePath);

    QVERIFY(QFileInfo(linkFilePath).isSymLink());

    part.openDocument(linkFilePath);
    QVERIFY(part.saveAs(QUrl::fromLocalFile(linkFilePath), Part::NoSaveAsFlags));

    QVERIFY(QFileInfo(linkFilePath).isSymLink());

    QFile::remove(newFilePath);
    QFile::remove(linkFilePath);
#endif
}

void PartTestB::testSaveAs()
{
    QFETCH(QString, file);
    QFETCH(QString, extension);
    QFETCH(bool, nativelySupportsAnnotations);
    QFETCH(bool, canSwapBackingFile);

    QScopedPointer<TestingUtils::CloseDialogHelper> closeDialogHelper;

    QString annotName;
    QTemporaryFile archiveSave(QStringLiteral("%1/okrXXXXXX.okular").arg(QDir::tempPath()));
    QTemporaryFile nativeDirectSave(QStringLiteral("%1/okrXXXXXX.%2").arg(QDir::tempPath(), extension));
    QTemporaryFile nativeFromArchiveFile(QStringLiteral("%1/okrXXXXXX.%2").arg(QDir::tempPath(), extension));
    QVERIFY(archiveSave.open());
    archiveSave.close();
    QVERIFY(nativeDirectSave.open());
    nativeDirectSave.close();
    QVERIFY(nativeFromArchiveFile.open());
    nativeFromArchiveFile.close();

    qDebug() << "Open file, add annotation and save both natively and to .okular";
    {
        Okular::Part part(nullptr, {});
        new QAbstractItemModelTester(part.annotationsModel(), &part);
        part.openDocument(file);
        part.m_document->documentInfo();

        QCOMPARE(part.m_document->canSwapBackingFile(), canSwapBackingFile);

        Okular::Annotation *annot = new Okular::TextAnnotation();
        annot->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.15, 0.15));
        annot->setContents(QStringLiteral("annot contents"));
        part.m_document->addPageAnnotation(0, annot);
        annotName = annot->uniqueName();

        if (canSwapBackingFile) {
            if (!nativelySupportsAnnotations) {
                closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
            }
            QVERIFY(part.saveAs(QUrl::fromLocalFile(nativeDirectSave.fileName()), Part::NoSaveAsFlags));
            // For backends that don't support annotations natively we mark the part as still modified
            // after a save because we keep the annotation around but it will get lost if the user closes the app
            // so we want to give her a last chance to save on close with the "you have changes dialog"
            QCOMPARE(part.isModified(), !nativelySupportsAnnotations);
            QVERIFY(part.saveAs(QUrl::fromLocalFile(archiveSave.fileName()), Part::SaveAsOkularArchive));
        } else {
            // We need to save to archive first otherwise we lose the annotation

            closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::Yes)); // this is the "you're going to lose the undo/redo stack" dialog
            QVERIFY(part.saveAs(QUrl::fromLocalFile(archiveSave.fileName()), Part::SaveAsOkularArchive));

            if (!nativelySupportsAnnotations) {
                closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
            }
            QVERIFY(part.saveAs(QUrl::fromLocalFile(nativeDirectSave.fileName()), Part::NoSaveAsFlags));
        }

        QCOMPARE(part.m_document->documentInfo().get(Okular::DocumentInfo::FilePath), part.m_document->currentDocument().toDisplayString());
        part.closeUrl();
    }

    qDebug() << "Open the .okular, check that the annotation is present and save to native";
    {
        Okular::Part part(nullptr, {});
        new QAbstractItemModelTester(part.annotationsModel(), &part);
        part.openDocument(archiveSave.fileName());
        part.m_document->documentInfo();

        QCOMPARE(part.m_document->page(0)->annotations().size(), 1);
        QCOMPARE(part.m_document->page(0)->annotations().constFirst()->uniqueName(), annotName);

        if (!nativelySupportsAnnotations) {
            closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
        }
        QVERIFY(part.saveAs(QUrl::fromLocalFile(nativeFromArchiveFile.fileName()), Part::NoSaveAsFlags));

        if (canSwapBackingFile && !nativelySupportsAnnotations) {
            // For backends that don't support annotations natively we mark the part as still modified
            // after a save because we keep the annotation around but it will get lost if the user closes the app
            // so we want to give her a last chance to save on close with the "you have changes dialog"
            closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "do you want to save or discard" dialog
        }

        QCOMPARE(part.m_document->documentInfo().get(Okular::DocumentInfo::FilePath), part.m_document->currentDocument().toDisplayString());
        part.closeUrl();
    }

    qDebug() << "Open the native file saved directly, and check that the annot"
             << "is there iff we expect it";
    {
        Okular::Part part(nullptr, {});
        new QAbstractItemModelTester(part.annotationsModel(), &part);
        part.openDocument(nativeDirectSave.fileName());

        QCOMPARE(part.m_document->page(0)->annotations().size(), nativelySupportsAnnotations ? 1 : 0);
        if (nativelySupportsAnnotations) {
            QCOMPARE(part.m_document->page(0)->annotations().constFirst()->uniqueName(), annotName);
        }

        part.closeUrl();
    }

    qDebug() << "Open the native file saved from the .okular, and check that the annot"
             << "is there iff we expect it";
    {
        Okular::Part part(nullptr, {});
        part.openDocument(nativeFromArchiveFile.fileName());

        QCOMPARE(part.m_document->page(0)->annotations().size(), nativelySupportsAnnotations ? 1 : 0);
        if (nativelySupportsAnnotations) {
            QCOMPARE(part.m_document->page(0)->annotations().constFirst()->uniqueName(), annotName);
        }

        part.closeUrl();
    }
}

void PartTestB::testSaveAs_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<bool>("nativelySupportsAnnotations");
    QTest::addColumn<bool>("canSwapBackingFile");

    QTest::newRow("pdf") << KDESRCDIR "data/file1.pdf" << "pdf" << true << true;
    QTest::newRow("pdf.gz") << KDESRCDIR "data/file1.pdf.gz" << "pdf" << true << true;
    QTest::newRow("epub") << KDESRCDIR "data/contents.epub" << "epub" << false << false;
    QTest::newRow("jpg") << KDESRCDIR "data/potato.jpg" << "jpg" << false << true;
}

void PartTestB::testSidebarItemAfterSaving()
{
    Okular::Part part(nullptr, {});
    QWidget *currentSidebarItem = part.m_sidebar->currentItem(); // thumbnails
    openDocument(&part, QStringLiteral(KDESRCDIR "data/tocreload.pdf"));
    // since it has TOC it changes to TOC
    QVERIFY(currentSidebarItem != part.m_sidebar->currentItem());
    // now change back to thumbnails
    part.m_sidebar->setCurrentItem(currentSidebarItem);

    part.saveAs(QUrl::fromLocalFile(QStringLiteral(KDESRCDIR "data/tocreload.pdf")));

    // Check it is still thumbnails after saving
    QCOMPARE(currentSidebarItem, part.m_sidebar->currentItem());
}

void PartTestB::testViewModeSavingPerFile()
{
    Okular::Part part(nullptr, {});

    // Open some file
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));

    // Switch to 'continuous' view mode
    part.m_pageView->setCapability(Okular::View::ViewCapability::Continuous, QVariant(true));

    // Close document
    part.closeUrl();

    // Open another file
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file2.pdf")));

    // Switch to 'non-continuous' mode
    part.m_pageView->setCapability(Okular::View::ViewCapability::Continuous, QVariant(false));

    // Close that document, too
    part.closeUrl();

    // Open first document again
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));

    // If per-file view mode saving works, the view mode should be 'continuous' again.
    QVERIFY(part.m_pageView->capability(Okular::View::ViewCapability::Continuous).toBool());
}

void PartTestB::testSaveAsUndoStackAnnotations()
{
    QFETCH(QString, file);
    QFETCH(QString, extension);
    QFETCH(bool, nativelySupportsAnnotations);
    QFETCH(bool, canSwapBackingFile);
    QFETCH(bool, saveToArchive);

    const Part::SaveAsFlag saveFlags = saveToArchive ? Part::SaveAsOkularArchive : Part::NoSaveAsFlags;

    QScopedPointer<TestingUtils::CloseDialogHelper> closeDialogHelper;

    // closeDialogHelper relies on the availability of the "Continue" button to drop changes
    // when saving to a file format not supporting those. However, this button is only sensible
    // and available for "Save As", but not for "Save". By alternately saving to saveFile1 and
    // saveFile2 we always force "Save As", so closeDialogHelper keeps working.
    QTemporaryFile saveFile1(QStringLiteral("%1/okrXXXXXX_1.%2").arg(QDir::tempPath(), extension));
    QVERIFY(saveFile1.open());
    saveFile1.close();
    QTemporaryFile saveFile2(QStringLiteral("%1/okrXXXXXX_2.%2").arg(QDir::tempPath(), extension));
    QVERIFY(saveFile2.open());
    saveFile2.close();

    Okular::Part part(nullptr, {});
    part.openDocument(file);
    new QAbstractItemModelTester(part.annotationsModel(), &part);

    QCOMPARE(part.m_document->canSwapBackingFile(), canSwapBackingFile);

    Okular::Annotation *annot = new Okular::TextAnnotation();
    annot->setBoundingRectangle(Okular::NormalizedRect(0.1, 0.1, 0.15, 0.15));
    annot->setContents(QStringLiteral("annot contents"));
    part.m_document->addPageAnnotation(0, annot);
    QString annotName = annot->uniqueName();

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }

    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));

    if (!canSwapBackingFile) {
        // The undo/redo stack gets lost if you can not swap the backing file
        QVERIFY(!part.m_document->canUndo());
        QVERIFY(!part.m_document->canRedo());
        return;
    }

    // Check we can still undo the annot add after save
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(!part.m_document->canUndo());

    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->page(0)->annotations().isEmpty());

    // Check we can redo the annot add after save
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(!part.m_document->canRedo());

    if (nativelySupportsAnnotations) {
        // If the annots are provided by the backend we need to refetch the pointer after save
        annot = part.m_document->page(0)->annotation(annotName);
        QVERIFY(annot);
    }

    // Remove the annotation, creates another undo command
    QVERIFY(part.m_document->canRemovePageAnnotation(annot));
    part.m_document->removePageAnnotation(0, annot);
    QVERIFY(part.m_document->page(0)->annotations().isEmpty());

    // Check we can still undo the annot remove after save
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.m_document->canUndo());
    QCOMPARE(part.m_document->page(0)->annotations().count(), 1);

    // Check we can still undo the annot add after save
    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile2.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(!part.m_document->canUndo());
    QVERIFY(part.m_document->page(0)->annotations().isEmpty());

    // Redo the add annotation
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.m_document->canUndo());
    QVERIFY(part.m_document->canRedo());

    if (nativelySupportsAnnotations) {
        // If the annots are provided by the backend we need to refetch the pointer after save
        annot = part.m_document->page(0)->annotation(annotName);
        QVERIFY(annot);
    }

    // Add translate, adjust and modify commands
    part.m_document->translatePageAnnotation(0, annot, Okular::NormalizedPoint(0.1, 0.1));
    part.m_document->adjustPageAnnotation(0, annot, Okular::NormalizedPoint(0.1, 0.1), Okular::NormalizedPoint(0.1, 0.1));
    part.m_document->prepareToModifyAnnotationProperties(annot);
    part.m_document->modifyPageAnnotationProperties(0, annot);

    // Now check we can still undo/redo/save at all the intermediate states and things still work
    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile2.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.m_document->canUndo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.m_document->canUndo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile2.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.m_document->canUndo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(!part.m_document->canUndo());
    QVERIFY(part.m_document->canRedo());
    QVERIFY(part.m_document->page(0)->annotations().isEmpty());

    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.m_document->canRedo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile2.fileName()), saveFlags));
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.m_document->canRedo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile1.fileName()), saveFlags));
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.m_document->canRedo());

    if (!nativelySupportsAnnotations && !saveToArchive) {
        closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "you're going to lose the annotations" dialog
    }
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile2.fileName()), saveFlags));
    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(!part.m_document->canRedo());

    closeDialogHelper.reset(new TestingUtils::CloseDialogHelper(&part, QDialogButtonBox::No)); // this is the "do you want to save or discard" dialog
    part.closeUrl();
}

void PartTestB::testSaveAsUndoStackAnnotations_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<bool>("nativelySupportsAnnotations");
    QTest::addColumn<bool>("canSwapBackingFile");
    QTest::addColumn<bool>("saveToArchive");

    QTest::newRow("pdf") << KDESRCDIR "data/file1.pdf" << "pdf" << true << true << false;
    QTest::newRow("epub") << KDESRCDIR "data/contents.epub" << "epub" << false << false << false;
    QTest::newRow("jpg") << KDESRCDIR "data/potato.jpg" << "jpg" << false << true << false;
    QTest::newRow("pdfarchive") << KDESRCDIR "data/file1.pdf" << "okular" << true << true << true;
    QTest::newRow("jpgarchive") << KDESRCDIR "data/potato.jpg" << "okular" << false << true << true;
}

void PartTestB::testSaveAsUndoStackForms()
{
    QFETCH(QString, file);
    QFETCH(QString, extension);
    QFETCH(bool, saveToArchive);

    const Part::SaveAsFlag saveFlags = saveToArchive ? Part::SaveAsOkularArchive : Part::NoSaveAsFlags;

    QTemporaryFile saveFile(QStringLiteral("%1/okrXXXXXX.%2").arg(QDir::tempPath(), extension));
    QVERIFY(saveFile.open());
    saveFile.close();

    Okular::Part part(nullptr, {});
    part.openDocument(file);

    const QList<Okular::FormField *> pageFormFields = part.m_document->page(0)->formFields();
    for (FormField *ff : pageFormFields) {
        if (ff->id() == 65537) {
            QCOMPARE(ff->type(), FormField::FormText);
            FormFieldText *fft = static_cast<FormFieldText *>(ff);
            part.m_document->editFormText(0, fft, QStringLiteral("BlaBla"), 6, 0, 0, QString());
        } else if (ff->id() == 65538) {
            QCOMPARE(ff->type(), FormField::FormButton);
            FormFieldButton *ffb = static_cast<FormFieldButton *>(ff);
            QCOMPARE(ffb->buttonType(), FormFieldButton::Radio);
            part.m_document->editFormButtons(0, QList<FormFieldButton *>() << ffb, QList<bool>() << true);
        } else if (ff->id() == 65542) {
            QCOMPARE(ff->type(), FormField::FormChoice);
            FormFieldChoice *ffc = static_cast<FormFieldChoice *>(ff);
            QCOMPARE(ffc->choiceType(), FormFieldChoice::ListBox);
            part.m_document->editFormList(0, ffc, QList<int>() << 1);
        } else if (ff->id() == 65543) {
            QCOMPARE(ff->type(), FormField::FormChoice);
            FormFieldChoice *ffc = static_cast<FormFieldChoice *>(ff);
            QCOMPARE(ffc->choiceType(), FormFieldChoice::ComboBox);
            part.m_document->editFormCombo(0, ffc, QStringLiteral("combo2"), 3, 0, 0);
        }
    }

    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canUndo());
    part.m_document->undo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));
    QVERIFY(!part.m_document->canUndo());

    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));

    QVERIFY(part.m_document->canRedo());
    part.m_document->redo();
    QVERIFY(part.saveAs(QUrl::fromLocalFile(saveFile.fileName()), saveFlags));
}

void PartTestB::testSaveAsUndoStackForms_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("extension");
    QTest::addColumn<bool>("saveToArchive");

    QTest::newRow("pdf") << KDESRCDIR "data/formSamples.pdf" << "pdf" << false;
    QTest::newRow("pdfarchive") << KDESRCDIR "data/formSamples.pdf" << "okular" << true;
}

void PartTestB::testOpenUrlArguments()
{
    Okular::Part part(nullptr, {});

    KParts::OpenUrlArguments args;
    args.setMimeType(QStringLiteral("text/rtf"));

    part.setArguments(args);

    part.openUrl(QUrl::fromLocalFile(QStringLiteral(KDESRCDIR "data/file1.pdf")));

    QCOMPARE(part.arguments().mimeType(), QStringLiteral("text/rtf"));
}

void PartTestB::test388288()
{
    Okular::Part part(nullptr, {});

    part.openUrl(QUrl::fromLocalFile(QStringLiteral(KDESRCDIR "data/file1.pdf")));
    new QAbstractItemModelTester(part.annotationsModel(), &part);

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseNormal"));

    auto annot = new Okular::HighlightAnnotation();
    annot->setHighlightType(Okular::HighlightAnnotation::Highlight);
    const Okular::NormalizedRect r(0.36, 0.16, 0.51, 0.17);
    annot->setBoundingRectangle(r);
    Okular::HighlightAnnotation::Quad q;
    q.setCapStart(false);
    q.setCapEnd(false);
    q.setFeather(1.0);
    q.setPoint(Okular::NormalizedPoint(r.left, r.bottom), 0);
    q.setPoint(Okular::NormalizedPoint(r.right, r.bottom), 1);
    q.setPoint(Okular::NormalizedPoint(r.right, r.top), 2);
    q.setPoint(Okular::NormalizedPoint(r.left, r.top), 3);
    annot->highlightQuads().append(q);

    part.m_document->addPageAnnotation(0, annot);

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.5, height * 0.5));
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::OpenHandCursor);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.4, height * 0.165));
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::ArrowCursor);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.1, height * 0.165));

    part.m_document->undo();

    annot = new Okular::HighlightAnnotation();
    annot->setHighlightType(Okular::HighlightAnnotation::Highlight);
    annot->setBoundingRectangle(r);
    annot->highlightQuads().append(q);

    part.m_document->addPageAnnotation(0, annot);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * 0.5, height * 0.5));
    QTRY_COMPARE(part.m_pageView->cursor().shape(), Qt::OpenHandCursor);
}


void PartTestB::testAnnotWindow()
{
    Okular::Part part(nullptr, {});
    QVERIFY(openDocument(&part, QStringLiteral(KDESRCDIR "data/file1.pdf")));
    part.widget()->show();
    part.widget()->resize(800, 600);
    new QAbstractItemModelTester(part.annotationsModel(), &part);
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    part.m_document->setViewportPage(0);

    QVERIFY(QMetaObject::invokeMethod(part.m_pageView, "slotSetMouseNormal"));

    QCOMPARE(part.m_document->currentPage(), 0u);

    // Create two distinct text annotations
    Okular::Annotation *annot1 = new Okular::TextAnnotation();
    annot1->setBoundingRectangle(Okular::NormalizedRect(0.8, 0.1, 0.85, 0.15));
    annot1->setContents(QStringLiteral("Annot contents 111111"));

    Okular::Annotation *annot2 = new Okular::TextAnnotation();
    annot2->setBoundingRectangle(Okular::NormalizedRect(0.8, 0.3, 0.85, 0.35));
    annot2->setContents(QStringLiteral("Annot contents 222222"));

    // Add annot1 and annot2 to document
    part.m_document->addPageAnnotation(0, annot1);
    part.m_document->addPageAnnotation(0, annot2);
    QVERIFY(part.m_document->page(0)->annotations().size() == 2);

    QTimer *delayResizeEventTimer = part.m_pageView->findChildren<QTimer *>(QStringLiteral("delayResizeEventTimer")).at(0);
    QVERIFY(delayResizeEventTimer->isActive());
    QTest::qWait(delayResizeEventTimer->interval() * 2);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    // Double click the first annotation to open its window (move mouse for visual feedback)
    const NormalizedPoint annot1pt = annot1->boundingRectangle().center();
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * annot1pt.x, height * annot1pt.y));
    QTest::mouseDClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * annot1pt.x, height * annot1pt.y));
    QTRY_COMPARE(part.m_pageView->findChildren<QFrame *>(QStringLiteral("AnnotWindow")).size(), 1);
    // Verify that the window is visible
    QFrame *win1 = part.m_pageView->findChild<QFrame *>(QStringLiteral("AnnotWindow"));
    QVERIFY(!win1->visibleRegion().isEmpty());

    // Double click the second annotation to open its window (move mouse for visual feedback)
    const NormalizedPoint annot2pt = annot2->boundingRectangle().center();
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * annot2pt.x, height * annot2pt.y));
    QTest::mouseDClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * annot2pt.x, height * annot2pt.y));
    QTRY_COMPARE(part.m_pageView->findChildren<QFrame *>(QStringLiteral("AnnotWindow")).size(), 2);
    // Verify that the first window is hidden covered by the second, which is visible
    QList<QFrame *> lstWin = part.m_pageView->findChildren<QFrame *>(QStringLiteral("AnnotWindow"));
    QFrame *win2;
    if (lstWin[0] == win1) {
        win2 = lstWin[1];
    } else {
        win2 = lstWin[0];
    }
    QVERIFY(win1->visibleRegion().isEmpty());
    QVERIFY(!win2->visibleRegion().isEmpty());

    // Double click the first annotation to raise its window (move mouse for visual feedback)
    QTest::mouseMove(part.m_pageView->viewport(), QPoint(width * annot1pt.x, height * annot1pt.y));
    QTest::mouseDClick(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(width * annot1pt.x, height * annot1pt.y));
    // Verify that the second window is hidden covered by the first, which is visible
    QVERIFY(!win1->visibleRegion().isEmpty());
    QVERIFY(win2->visibleRegion().isEmpty());

    // Move annotation window 1 to partially show annotation window 2
    win1->move(QPoint(win2->pos().x(), win2->pos().y() + 50));
    // Verify that both windows are partially visible
    QVERIFY(!win1->visibleRegion().isEmpty());
    QVERIFY(!win2->visibleRegion().isEmpty());

    // Click the second annotation window to raise it (move mouse for visual feedback)
    auto widget = win2->window()->childAt(win2->mapTo(win2->window(), QPoint(10, 10)));
    QTest::mouseMove(win2->window(), win2->mapTo(win2->window(), QPoint(10, 10)));
    QTest::mouseClick(widget, Qt::LeftButton, Qt::NoModifier, widget->mapFrom(win2, QPoint(10, 10)));
    QVERIFY(win1->visibleRegion().rectCount() == 3);
    QVERIFY(win2->visibleRegion().rectCount() == 4);
}

// Helper for testAdditionalActionTriggers
static void verifyTargetStates(const QString &triggerName, const QMap<QString, Okular::FormField *> &fields, bool focusVisible, bool cursorVisible, bool mouseVisible, int line)
{
    Okular::FormField *focusTarget = fields.value(triggerName + QStringLiteral("_focus_target"));
    Okular::FormField *cursorTarget = fields.value(triggerName + QStringLiteral("_cursor_target"));
    Okular::FormField *mouseTarget = fields.value(triggerName + QStringLiteral("_mouse_target"));

    QVERIFY(focusTarget);
    QVERIFY(cursorTarget);
    QVERIFY(mouseTarget);

    QTRY_VERIFY2(focusTarget->isVisible() == focusVisible, QStringLiteral("line: %1 focus for %2 not matched. Expected %3 Actual %4").arg(line).arg(triggerName).arg(focusTarget->isVisible()).arg(focusVisible).toUtf8().constData());
    QTRY_VERIFY2(cursorTarget->isVisible() == cursorVisible, QStringLiteral("line: %1 cursor for %2 not matched. Actual %3 Expected %4").arg(line).arg(triggerName).arg(cursorTarget->isVisible()).arg(cursorVisible).toUtf8().constData());
    QTRY_VERIFY2(mouseTarget->isVisible() == mouseVisible, QStringLiteral("line: %1 mouse for %2 not matched. Expected %3 Actual %4").arg(line).arg(triggerName).arg(mouseTarget->isVisible()).arg(mouseVisible).toUtf8().constData());
}

void PartTestB::testAdditionalActionTriggers()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/additionalFormActions.pdf");
    Okular::Part part(nullptr, QVariantList());
    part.openDocument(testFile);
    part.widget()->resize(800, 600);

    part.widget()->show();
    if (qgetenv("KDECI_CANNOT_CREATE_WINDOWS") == "1") {
        QSKIP("KDE CI can't create a window on this platform, skipping some gui tests");
    }

    QVERIFY(QTest::qWaitForWindowExposed(part.widget()));

    QTimer *delayResizeEventTimer = part.m_pageView->findChildren<QTimer *>(QStringLiteral("delayResizeEventTimer")).at(0);
    QVERIFY(delayResizeEventTimer->isActive());
    QTest::qWait(delayResizeEventTimer->interval() * 2);

    part.m_document->setViewportPage(0);

    // wait for pixmap
    QTRY_VERIFY(part.m_document->page(0)->hasPixmap(part.m_pageView));

    QMap<QString, Okular::FormField *> fields;
    // Field names in test document are:
    // For trigger fields: tf, cb, rb, dd, pb
    // For target fields: <trigger_name>_focus_target, <trigger_name>_cursor_target,
    // <trigger_name>_mouse_target
    const Okular::Page *page = part.m_document->page(0);
    const QList<Okular::FormField *> pageFormFields = page->formFields();
    for (Okular::FormField *ff : pageFormFields) {
        fields.insert(ff->name(), static_cast<Okular::FormField *>(ff));
    }

    // Verify that everything is set up.
    verifyTargetStates(QStringLiteral("tf"), fields, true, true, true, __LINE__);
    verifyTargetStates(QStringLiteral("cb"), fields, true, true, true, __LINE__);
    verifyTargetStates(QStringLiteral("rb"), fields, true, true, true, __LINE__);
    verifyTargetStates(QStringLiteral("dd"), fields, true, true, true, __LINE__);
    verifyTargetStates(QStringLiteral("pb"), fields, true, true, true, __LINE__);

    const int width = part.m_pageView->horizontalScrollBar()->maximum() + part.m_pageView->viewport()->width();
    const int height = part.m_pageView->verticalScrollBar()->maximum() + part.m_pageView->viewport()->height();

    part.actionCollection()->action(QStringLiteral("view_toggle_forms"))->trigger();

    QPoint tfPos(width * 0.045, height * 0.05);
    QPoint cbPos(width * 0.045, height * 0.08);
    QPoint rbPos(width * 0.045, height * 0.12);
    QPoint ddPos(width * 0.045, height * 0.16);
    QPoint pbPos(width * 0.045, height * 0.26);

    // Test text field
    auto widget = part.m_pageView->viewport()->childAt(tfPos);
    QVERIFY(widget);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(tfPos));
    verifyTargetStates(QStringLiteral("tf"), fields, true, false, true, __LINE__);
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("tf"), fields, false, false, false, __LINE__);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("tf"), fields, false, false, true, __LINE__);

    // Checkbox
    widget = part.m_pageView->viewport()->childAt(cbPos);
    QVERIFY(widget);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(cbPos));
    verifyTargetStates(QStringLiteral("cb"), fields, true, false, true, __LINE__);
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("cb"), fields, false, false, false, __LINE__);
    // Confirm that the textfield no longer has any invisible
    verifyTargetStates(QStringLiteral("tf"), fields, true, true, true, __LINE__);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("cb"), fields, false, false, true, __LINE__);

    // Radio
    widget = part.m_pageView->viewport()->childAt(rbPos);
    QVERIFY(widget);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(rbPos));
    verifyTargetStates(QStringLiteral("rb"), fields, true, false, true, __LINE__);
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("rb"), fields, false, false, false, __LINE__);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("rb"), fields, false, false, true, __LINE__);

    // Dropdown
    widget = part.m_pageView->viewport()->childAt(ddPos);
    QVERIFY(widget);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(ddPos));
    verifyTargetStates(QStringLiteral("dd"), fields, true, false, true, __LINE__);
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("dd"), fields, false, false, false, __LINE__);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("dd"), fields, false, false, true, __LINE__);

    // Pushbutton
    widget = part.m_pageView->viewport()->childAt(pbPos);
    QVERIFY(widget);

    QTest::mouseMove(part.m_pageView->viewport(), QPoint(pbPos));
    verifyTargetStates(QStringLiteral("pb"), fields, true, false, true, __LINE__);
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("pb"), fields, false, false, false, __LINE__);
    QTest::mouseRelease(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("pb"), fields, false, false, true, __LINE__);

    // Confirm that a mouse release outside does not trigger the show action.
    QTest::mousePress(widget, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
    verifyTargetStates(QStringLiteral("pb"), fields, false, false, false, __LINE__);
    QTest::mouseRelease(part.m_pageView->viewport(), Qt::LeftButton, Qt::NoModifier, tfPos);
    verifyTargetStates(QStringLiteral("pb"), fields, false, false, false, __LINE__);
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

#include "parttest_b.moc"
