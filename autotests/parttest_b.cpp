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

    qDebug() << "Open the native file saved directly, and check that the annot" << "is there iff we expect it";
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

    qDebug() << "Open the native file saved from the .okular, and check that the annot" << "is there iff we expect it";
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

    Okular::PartTestB test;

    return QTest::qExec(&test, argc, argv);
}

#include "parttest_b.moc"
