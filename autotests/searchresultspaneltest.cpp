/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeDatabase>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "../core/document.h"
#include "../core/page.h"
#include "../part/searchresultsmodel.h"
#include "../part/searchresultspanel.h"
#include "../settings.h"

class SearchResultsPanelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testPanelListsEveryMatchOfTheDocument();
    void testSearchHighlightsTheMatchesInTheDocument();
    void testSnippetShowsTheMatchNotTheStartOfThePage();
    void testPickedResultIsHighlightedApartFromTheOtherMatches();
    void testResultsBeginAtThePageBeingRead();
    void testResultsEndUpInDocumentOrder();
    void testScrollingAwayFromTheSelectionIsNotUndone();
    void testTheDocumentStaysOnThePickedResultWhileTheScanContinues();
    void testThePickedResultStaysInViewWhenEveryOtherResultIsInsertedAboveIt();
    void testTheFirstResultFoundIsSelected();
    void testClearingTheFieldStopsTheRunningSearch();
    void testClearingTheFieldStopsTheScanEvenWithoutFindAsYouType();
    void testNextAndPreviousMoveTheSelectionAndTheDocumentView();
    void testReturnStepsThroughTheResults();
    void testNextWrapsAroundToTheFirstResult();
    void testUncheckingSearchAsYouTypeWaitsForReturn();
    void testUncheckingFromCurrentPageStartsAtPageOne();
    void testNextWorksWhileTheScanIsStillRunning();
    void testManyResultInsertionsWhileScrolledAwayStayFast();
    void testTheAnchoredRowNeverFlashesWrongContentDuringABurst();
    void testNextIsNotDraggedBackByAPendingRestore();
    void testClearingTheSearchDropsResultsStillWaitingToBeListed();
    void testAnUnsearchableDocumentSaysSoAndDisablesInput();
};

void SearchResultsPanelTest::initTestCase()
{
    Okular::Settings::instance(QStringLiteral("searchresultspaneltest"));
}

void SearchResultsPanelTest::cleanup()
{
    // the options are persisted settings: restore them after every test, even a failed one
    Okular::Settings::setSearchPanelCaseSensitive(false);
    Okular::Settings::setSearchPanelFromCurrentPage(true);
    Okular::Settings::setSearchPanelFindAsYouType(true);
    Okular::Settings::self()->save();
}

// A document with a test file open (by default simple-multipage.pdf: 40 pages reading
// "Page <n>"), a panel observing it, and the widgets tests drive. Check ok() first.
struct PanelFixture {
    Okular::Document document {nullptr};
    SearchResultsPanel *panel = nullptr;
    QLineEdit *searchLine = nullptr;
    QListView *view = nullptr;
    SearchResultsModel *model = nullptr;

    explicit PanelFixture(const QString &fileName = QStringLiteral("simple-multipage.pdf"))
    {
        panel = new SearchResultsPanel(&document);
        document.addObserver(panel);

        const QString testFile = QStringLiteral(KDESRCDIR "data/") + fileName;
        QMimeDatabase db;
        if (document.openDocument(testFile, QUrl::fromLocalFile(testFile), db.mimeTypeForFile(testFile)) != Okular::Document::OpenSuccess) {
            document.removeObserver(panel);
            delete panel;
            panel = nullptr;
            return;
        }

        // the search starts at the page being read: pin it, whatever the previous test did
        document.setViewportPage(0);

        searchLine = panel->findChild<QLineEdit *>();
        view = panel->findChild<QListView *>();
        model = view ? qobject_cast<SearchResultsModel *>(view->model()) : nullptr;
    }

    ~PanelFixture()
    {
        if (panel) {
            document.removeObserver(panel);
            delete panel;
        }
    }
    PanelFixture(const PanelFixture &) = delete;
    PanelFixture &operator=(const PanelFixture &) = delete;

    bool ok() const
    {
        return panel && searchLine && view && model;
    }

    // sized to hold several rows, but too few to show all forty
    bool showSized(int width = 320, int height = 400)
    {
        panel->resize(width, height);
        panel->show();
        return QTest::qWaitForWindowExposed(panel);
    }

    bool searchAndWait(const QString &text = QStringLiteral("Page"))
    {
        QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);
        searchLine->setText(text);
        return finishedSpy.wait(30000);
    }
};

// Records the page number shown in the results view's top row every time its viewport
// actually paints. With insertion and scroll correction done in one indivisible pass
// (see SearchResultsPanel::slotFlushPendingResults()), no paint should ever catch the
// list mid-shift; this filter is how a test proves that on every frame, not just the
// final settled one.
class TopRowPaintRecorder : public QObject
{
public:
    explicit TopRowPaintRecorder(QListView *view)
        : QObject(view)
        , m_view(view)
    {
        m_view->viewport()->installEventFilter(this);
    }

    bool recording = false;
    int anchoredPage = -1;
    int visiblePaints = 0;
    int wrongFrames = 0;
    int firstWrongPage = -1;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (recording && watched == m_view->viewport() && event->type() == QEvent::Paint) {
            ++visiblePaints;
            const int drawn = m_view->indexAt(QPoint(1, 1)).data(SearchResultsModel::PageNumberRole).toInt();
            if (drawn != anchoredPage && wrongFrames++ == 0) {
                firstWrongPage = drawn;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QListView *const m_view;
};

static QList<SearchResult> resultsOnPage(int pageNumber, int count)
{
    QList<SearchResult> results;
    results.reserve(count);
    for (int i = 0; i < count; ++i) {
        SearchResult result;
        result.pageNumber = pageNumber;
        result.snippet = QStringLiteral("x");
        results.append(result);
    }
    return results;
}

void SearchResultsPanelTest::testPanelListsEveryMatchOfTheDocument()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.searchLine->isEnabled());
    QVERIFY(f.searchAndWait());

    QCOMPARE(f.model->rowCount(), 40);
    const QModelIndex first = f.model->index(0, 0);
    QCOMPARE(first.data(SearchResultsModel::PageNumberRole).toInt(), 0);
    QVERIFY(first.data(SearchResultsModel::SnippetRole).toString().contains(QStringLiteral("Page 1")));
}

void SearchResultsPanelTest::testSearchHighlightsTheMatchesInTheDocument()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(!f.document.page(0)->hasHighlights(SEARCH_PANEL_SEARCH_ID));

    QVERIFY(f.searchAndWait());

    QVERIFY(f.document.page(0)->hasHighlights(SEARCH_PANEL_SEARCH_ID));
    QVERIFY(f.document.page(39)->hasHighlights(SEARCH_PANEL_SEARCH_ID));
}

void SearchResultsPanelTest::testSnippetShowsTheMatchNotTheStartOfThePage()
{
    // file2.pdf reads "Hola que tal" on one line and "This is nice" on the next
    PanelFixture f(QStringLiteral("file2.pdf"));
    QVERIFY(f.ok());
    QVERIFY(f.searchAndWait(QStringLiteral("nice")));

    QCOMPARE(f.model->rowCount(), 1);
    const QString snippet = f.model->index(0, 0).data(SearchResultsModel::SnippetRole).toString();
    QCOMPARE(snippet, QStringLiteral("This is nice"));
    QVERIFY(!snippet.contains(QStringLiteral("Hola")));
}

void SearchResultsPanelTest::testPickedResultIsHighlightedApartFromTheOtherMatches()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.searchAndWait());

    Q_EMIT f.view->activated(f.model->index(5, 0));

    QVERIFY(f.document.page(5)->hasHighlights(SEARCH_PANEL_SEARCH_ID));
    QVERIFY(f.document.page(5)->hasHighlights(SEARCH_PANEL_CURRENT_MATCH_ID));

    Q_EMIT f.view->activated(f.model->index(12, 0));
    QVERIFY(!f.document.page(5)->hasHighlights(SEARCH_PANEL_CURRENT_MATCH_ID));
    QVERIFY(f.document.page(5)->hasHighlights(SEARCH_PANEL_SEARCH_ID));
    QVERIFY(f.document.page(12)->hasHighlights(SEARCH_PANEL_CURRENT_MATCH_ID));
}

void SearchResultsPanelTest::testResultsBeginAtThePageBeingRead()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);
    QCOMPARE(f.document.currentPage(), 25u);

    int firstReportedPage = -1;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int first, int) {
        if (firstReportedPage < 0) {
            firstReportedPage = f.model->index(first, 0).data(SearchResultsModel::PageNumberRole).toInt();
        }
    });

    QVERIFY(f.searchAndWait());
    QCOMPARE(firstReportedPage, 25);
}

void SearchResultsPanelTest::testResultsEndUpInDocumentOrder()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);

    QVERIFY(f.searchAndWait());

    QCOMPARE(f.model->rowCount(), 40);
    for (int row = 0; row < 40; ++row) {
        QCOMPARE(f.model->index(row, 0).data(SearchResultsModel::PageNumberRole).toInt(), row);
    }
}

void SearchResultsPanelTest::testScrollingAwayFromTheSelectionIsNotUndone()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());
    f.document.setViewportPage(25);

    // once the list actually scrolls, scroll away from the selected result and
    // remember the row on top
    int watchedPage = -1;
    bool reallyScrolled = false;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int, int) {
        QScrollBar *scrollBar = f.view->verticalScrollBar();
        if (watchedPage >= 0 || scrollBar->maximum() == 0) {
            return;
        }
        scrollBar->setValue(scrollBar->maximum());
        reallyScrolled = scrollBar->value() > 0;
        watchedPage = f.model->index(scrollBar->value(), 0).data(SearchResultsModel::PageNumberRole).toInt();
    });

    QVERIFY(f.searchAndWait());

    QVERIFY(watchedPage >= 0);
    QVERIFY(reallyScrolled);
    const int topRow = f.view->verticalScrollBar()->value();
    QCOMPARE(f.model->index(topRow, 0).data(SearchResultsModel::PageNumberRole).toInt(), watchedPage);
}

void SearchResultsPanelTest::testTheDocumentStaysOnThePickedResultWhileTheScanContinues()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);

    int pickedPage = -1;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int, int) {
        if (pickedPage < 0 && f.model->rowCount() >= 3) {
            const QModelIndex picked = f.model->index(2, 0);
            pickedPage = picked.data(SearchResultsModel::PageNumberRole).toInt();
            Q_EMIT f.view->activated(picked);
        }
    });

    QVERIFY(f.searchAndWait());

    QVERIFY(pickedPage >= 0);
    QCOMPARE(int(f.document.currentPage()), pickedPage);
}

void SearchResultsPanelTest::testThePickedResultStaysInViewWhenEveryOtherResultIsInsertedAboveIt()
{
    // worst case for the anchoring: every other result is inserted above the picked one
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());
    f.document.setViewportPage(39);
    QCOMPARE(f.document.currentPage(), 39u);

    int pickedPage = -1;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int, int) {
        if (pickedPage < 0) {
            const QModelIndex picked = f.model->index(0, 0);
            pickedPage = picked.data(SearchResultsModel::PageNumberRole).toInt();
            f.view->setCurrentIndex(picked);
        }
    });

    QVERIFY(f.searchAndWait());

    QCOMPARE(pickedPage, 39);
    QCOMPARE(f.model->rowCount(), 40);
    QVERIFY(f.view->verticalScrollBar()->maximum() > 0);

    const QModelIndex current = f.view->currentIndex();
    QCOMPARE(current.data(SearchResultsModel::PageNumberRole).toInt(), 39);
    QVERIFY(f.view->viewport()->rect().intersects(f.view->visualRect(current)));
}

void SearchResultsPanelTest::testTheFirstResultFoundIsSelected()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);
    QCOMPARE(f.document.currentPage(), 25u);

    QVERIFY(f.searchAndWait());

    const QModelIndex current = f.view->currentIndex();
    QVERIFY(current.isValid());
    QCOMPARE(current.data(SearchResultsModel::PageNumberRole).toInt(), 25);

    QVERIFY(f.document.page(25)->hasHighlights(SEARCH_PANEL_CURRENT_MATCH_ID));
    // selected, but the document view has not moved
    QCOMPARE(f.document.currentPage(), 25u);
}

void SearchResultsPanelTest::testClearingTheFieldStopsTheRunningSearch()
{
    PanelFixture f;
    QVERIFY(f.ok());

    QSignalSpy scannedSpy(&f.document, &Okular::Document::searchPageScanned);
    bool stopped = false;
    QObject::connect(&f.document, &Okular::Document::searchPageScanned, &f.document, [&](int, int, const QList<Okular::SearchMatch> &) {
        if (stopped) {
            return;
        }
        stopped = true;
        f.searchLine->clear();
    });

    // the cancellation reports a finish; a scan that kept going would report more after it
    QVERIFY(f.searchAndWait());
    QTest::qWait(250);

    QVERIFY(stopped);
    QVERIFY2(scannedSpy.count() < 40, qPrintable(QStringLiteral("scanned %1 pages of 40 after the field was cleared").arg(scannedSpy.count())));
}

void SearchResultsPanelTest::testClearingTheFieldStopsTheScanEvenWithoutFindAsYouType()
{
    PanelFixture f;
    QVERIFY(f.ok());

    QAction *searchAsYouTypeAction = f.panel->findChild<QAction *>(QStringLiteral("searchResultsSearchAsYouTypeAction"));
    QVERIFY(searchAsYouTypeAction);
    searchAsYouTypeAction->setChecked(false);

    QSignalSpy scannedSpy(&f.document, &Okular::Document::searchPageScanned);
    bool stopped = false;
    QObject::connect(&f.document, &Okular::Document::searchPageScanned, &f.document, [&](int, int, const QList<Okular::SearchMatch> &) {
        if (stopped) {
            return;
        }
        stopped = true;
        f.searchLine->clear();
    });

    QSignalSpy finishedSpy(&f.document, &Okular::Document::searchFinished);
    f.searchLine->setText(QStringLiteral("Page"));
    QTest::keyClick(f.searchLine, Qt::Key_Return);

    QVERIFY(finishedSpy.wait(30000));
    QTest::qWait(250);

    QVERIFY(stopped);
    QVERIFY2(scannedSpy.count() < 40, qPrintable(QStringLiteral("scanned %1 pages of 40 after the field was cleared").arg(scannedSpy.count())));
}

void SearchResultsPanelTest::testNextAndPreviousMoveTheSelectionAndTheDocumentView()
{
    PanelFixture f;
    QVERIFY(f.ok());

    QPushButton *nextButton = f.panel->findChild<QPushButton *>(QStringLiteral("searchResultsNextButton"));
    QPushButton *previousButton = f.panel->findChild<QPushButton *>(QStringLiteral("searchResultsPreviousButton"));
    QVERIFY(nextButton);
    QVERIFY(previousButton);
    QVERIFY(!nextButton->isEnabled());
    QVERIFY(!previousButton->isEnabled());

    f.document.setViewportPage(25);
    QCOMPARE(f.document.currentPage(), 25u);

    QVERIFY(f.searchAndWait());

    QCOMPARE(f.model->rowCount(), 40);
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 25);
    QVERIFY(nextButton->isEnabled());
    QVERIFY(previousButton->isEnabled());

    nextButton->click();
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 26);
    QCOMPARE(f.document.currentPage(), 26u);

    previousButton->click();
    previousButton->click();
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 24);
    QCOMPARE(f.document.currentPage(), 24u);

    f.searchLine->clear();
    QTest::keyClick(f.searchLine, Qt::Key_Return);
    QVERIFY(!nextButton->isEnabled());
    QVERIFY(!previousButton->isEnabled());
}

void SearchResultsPanelTest::testReturnStepsThroughTheResults()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);
    QVERIFY(f.searchAndWait());

    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 25);
    QSignalSpy scannedSpy(&f.document, &Okular::Document::searchPageScanned);

    QTest::keyClick(f.searchLine, Qt::Key_Return);
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 26);
    QCOMPARE(f.document.currentPage(), 26u);

    QTest::keyClick(f.searchLine, Qt::Key_Return, Qt::ShiftModifier);
    QTest::keyClick(f.searchLine, Qt::Key_Return, Qt::ShiftModifier);
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 24);
    QCOMPARE(f.document.currentPage(), 24u);

    QCOMPARE(scannedSpy.count(), 0); // stepping did not search again

    // editing the text makes the next Return search instead of stepping
    QSignalSpy finishedSpy(&f.document, &Okular::Document::searchFinished);
    f.searchLine->setText(QStringLiteral("Page 3"));
    QTest::keyClick(f.searchLine, Qt::Key_Return);
    QVERIFY(finishedSpy.wait(30000));
    QVERIFY(scannedSpy.count() > 0);
}

void SearchResultsPanelTest::testNextWrapsAroundToTheFirstResult()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.searchAndWait());

    QCOMPARE(f.model->rowCount(), 40);
    f.view->setCurrentIndex(f.model->index(39, 0));

    // selecting a row does not move the view: move it off page 0, or the wrap proves nothing
    f.document.setViewportPage(20);
    QCOMPARE(f.document.currentPage(), 20u);

    QPushButton *nextButton = f.panel->findChild<QPushButton *>(QStringLiteral("searchResultsNextButton"));
    QVERIFY(nextButton);
    nextButton->click();

    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 0);
    QCOMPARE(f.document.currentPage(), 0u);

    QPushButton *previousButton = f.panel->findChild<QPushButton *>(QStringLiteral("searchResultsPreviousButton"));
    QVERIFY(previousButton);
    previousButton->click();

    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), 39);
    QCOMPARE(f.document.currentPage(), 39u);
}

void SearchResultsPanelTest::testUncheckingSearchAsYouTypeWaitsForReturn()
{
    PanelFixture f;
    QVERIFY(f.ok());

    QAction *searchAsYouTypeAction = f.panel->findChild<QAction *>(QStringLiteral("searchResultsSearchAsYouTypeAction"));
    QVERIFY(searchAsYouTypeAction);
    QVERIFY(searchAsYouTypeAction->isChecked());
    searchAsYouTypeAction->setChecked(false);

    QSignalSpy scannedSpy(&f.document, &Okular::Document::searchPageScanned);
    f.searchLine->setText(QStringLiteral("Page"));

    QTest::qWait(900); // past the 700 ms typing delay
    QCOMPARE(scannedSpy.count(), 0);

    QSignalSpy finishedSpy(&f.document, &Okular::Document::searchFinished);
    QTest::keyClick(f.searchLine, Qt::Key_Return);
    QVERIFY(finishedSpy.wait(30000));
    QVERIFY(scannedSpy.count() > 0);
}

void SearchResultsPanelTest::testUncheckingFromCurrentPageStartsAtPageOne()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);
    QCOMPARE(f.document.currentPage(), 25u);

    QAction *fromCurrentPageAction = f.panel->findChild<QAction *>(QStringLiteral("searchResultsFromCurrentPageAction"));
    QVERIFY(fromCurrentPageAction);
    QVERIFY(fromCurrentPageAction->isChecked());
    fromCurrentPageAction->setChecked(false);

    int firstReportedPage = -1;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int first, int) {
        if (firstReportedPage < 0) {
            firstReportedPage = f.model->index(first, 0).data(SearchResultsModel::PageNumberRole).toInt();
        }
    });

    QVERIFY(f.searchAndWait());
    QCOMPARE(firstReportedPage, 0);
}

void SearchResultsPanelTest::testNextWorksWhileTheScanIsStillRunning()
{
    PanelFixture f;
    QVERIFY(f.ok());
    f.document.setViewportPage(25);
    QCOMPARE(f.document.currentPage(), 25u);

    QPushButton *nextButton = f.panel->findChild<QPushButton *>(QStringLiteral("searchResultsNextButton"));
    QVERIFY(nextButton);

    bool clicked = false;
    int pageBeforeClick = -1;
    int pageAfterClick = -1;
    uint documentPageAfterClick = 0;
    QObject::connect(f.model, &QAbstractItemModel::rowsInserted, f.model, [&](const QModelIndex &, int, int) {
        // once page 26 is listed, click from the event loop, as a person could
        if (!clicked && f.model->rowCount() >= 3) {
            clicked = true;
            QTimer::singleShot(0, nextButton, [&, nextButton]() {
                pageBeforeClick = f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt();
                nextButton->click(); // a no-op on a disabled button
                pageAfterClick = f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt();
                documentPageAfterClick = f.document.currentPage();
            });
        }
    });

    QVERIFY(f.searchAndWait());

    QVERIFY(clicked);
    QCOMPARE(pageBeforeClick, 25);
    QCOMPARE(pageAfterClick, 26);
    QCOMPARE(documentPageAfterClick, 26u);
}

void SearchResultsPanelTest::testManyResultInsertionsWhileScrolledAwayStayFast()
{
    // a per-page scroll correction re-lays out the whole list each time, quadratic over a scan
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());

    // seed rows, then scroll away as the reader would
    for (int page = 2000; page < 2200; ++page) {
        f.panel->enqueueResults(resultsOnPage(page, 1));
    }
    QCoreApplication::processEvents();
    QVERIFY(f.view->verticalScrollBar()->maximum() > 0);
    f.view->verticalScrollBar()->setValue(f.view->verticalScrollBar()->maximum());
    QCOMPARE(f.view->verticalScrollBar()->value(), f.view->verticalScrollBar()->maximum());

    // Half the pages land past the end, half before the start, as the outward
    // walk produces, with the event loop turning every 100 pages the way a real
    // scan's yields do. The scrollbar spy proves corrections actually ran - a
    // run where nothing was corrected would measure nothing.
    const int pageCount = 4000; // 25 matches each -> 100,000 more rows
    int nextHighPage = 2200;
    int nextLowPage = 1999;
    QSignalSpy restoreSpy(f.view->verticalScrollBar(), &QAbstractSlider::valueChanged);

    QElapsedTimer timer;
    timer.start();
    for (int batch = 0; batch < pageCount; ++batch) {
        const int page = (batch % 2 == 0) ? nextHighPage++ : nextLowPage--;
        f.panel->enqueueResults(resultsOnPage(page, 25));
        if (batch % 100 == 99) {
            QCoreApplication::processEvents();
        }
    }
    QCoreApplication::processEvents();
    const qint64 elapsedMs = timer.elapsed();

    QCOMPARE(f.model->rowCount(), 200 + pageCount * 25);
    QVERIFY2(restoreSpy.count() > 5, qPrintable(QStringLiteral("only %1 scrollbar movements during the whole burst; the position was never actually restored, so nothing was measured").arg(restoreSpy.count())));
    QVERIFY2(elapsedMs < 2000, qPrintable(QStringLiteral("listing %1 reported pages while scrolled away took %2 ms; results are not being coalesced across the burst").arg(pageCount).arg(elapsedMs)));
}

void SearchResultsPanelTest::testTheAnchoredRowNeverFlashesWrongContentDuringABurst()
{
    // Insertion and scroll correction happen in one indivisible pass per
    // event-loop trip (see slotFlushPendingResults()) precisely so no paint can
    // catch the list shifted. Assert that on every frame that reaches the
    // viewport, not just the settled end state: deferred-correction designs
    // jittered (or, suppressing paints, went blank) in the field.
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());

    for (int page = 1000; page < 1200; ++page) {
        f.panel->enqueueResults(resultsOnPage(page, 1));
    }
    QCoreApplication::processEvents();
    QVERIFY(f.view->verticalScrollBar()->maximum() > 0);
    f.view->verticalScrollBar()->setValue(f.view->verticalScrollBar()->maximum());

    auto *recorder = new TopRowPaintRecorder(f.view);
    recorder->anchoredPage = f.model->index(f.view->verticalScrollBar()->value(), 0).data(SearchResultsModel::PageNumberRole).toInt();

    // burst around the anchor, grabbing a frame at every loop boundary as a compositor would
    recorder->recording = true;
    int nextHighPage = 1200;
    int nextLowPage = 999;
    for (int batch = 0; batch < 300; ++batch) {
        const int page = (batch % 2 == 0) ? nextLowPage-- : nextHighPage++;
        f.panel->enqueueResults(resultsOnPage(page, 25));
        QCoreApplication::processEvents();
        f.view->viewport()->repaint();
    }
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    recorder->recording = false;

    QVERIFY2(recorder->wrongFrames == 0,
             qPrintable(QStringLiteral("view actually drew page %1 at the top while anchored to page %2, on %3 of %4 visible frames - the reader would have seen the list jitter")
                            .arg(recorder->firstWrongPage)
                            .arg(recorder->anchoredPage)
                            .arg(recorder->wrongFrames)
                            .arg(recorder->visiblePaints)));
    QVERIFY2(recorder->visiblePaints > 0, "no paint ever reached the viewport; the burst was invisible and this test observed nothing");
}

void SearchResultsPanelTest::testNextIsNotDraggedBackByAPendingRestore()
{
    // results queued before the click, listed above the result afterwards, must not shove the view off it
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());

    for (int page = 20; page < 40; ++page) {
        f.panel->enqueueResults(resultsOnPage(page, 10));
    }
    QCoreApplication::processEvents();
    QVERIFY(f.view->verticalScrollBar()->maximum() > 0);

    f.view->verticalScrollBar()->setValue(f.view->verticalScrollBar()->maximum());
    f.panel->enqueueResults(resultsOnPage(0, 25));

    QVERIFY(QMetaObject::invokeMethod(f.panel, "slotNextResult", Qt::DirectConnection));
    QCOMPARE(f.view->currentIndex().row(), 0);
    const int pageAfterNext = f.view->indexAt(QPoint(1, 1)).data(SearchResultsModel::PageNumberRole).toInt();

    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const int pageOnTop = f.view->indexAt(QPoint(1, 1)).data(SearchResultsModel::PageNumberRole).toInt();
    QVERIFY2(pageOnTop == pageAfterNext, qPrintable(QStringLiteral("the view slid from page %1 to page %2 on top once the queued results were listed; Next was shoved off the result the reader asked for").arg(pageAfterNext).arg(pageOnTop)));
    QCOMPARE(f.view->currentIndex().data(SearchResultsModel::PageNumberRole).toInt(), pageAfterNext);
}

void SearchResultsPanelTest::testClearingTheSearchDropsResultsStillWaitingToBeListed()
{
    PanelFixture f;
    QVERIFY(f.ok());
    QVERIFY(f.showSized());

    // one page's results are still queued when the field is cleared
    f.searchLine->setText(QStringLiteral("x"));
    f.panel->enqueueResults(resultsOnPage(30, 25));
    f.searchLine->clear();

    // the next search's first page flushes everything queued: leftovers would surface here
    f.panel->enqueueResults(resultsOnPage(5, 3));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(f.model->index(0, 0).data(SearchResultsModel::PageNumberRole).toInt(), 5);
    QVERIFY2(f.model->rowCount() == 3, qPrintable(QStringLiteral("%1 rows listed where the new search reported 3; results queued by the abandoned search leaked into its successor's list").arg(f.model->rowCount())));
}

void SearchResultsPanelTest::testAnUnsearchableDocumentSaysSoAndDisablesInput()
{
    PanelFixture f(QStringLiteral("potato.jpg"));
    QVERIFY(f.ok());
    QVERIFY(!f.document.supportsSearching());

    QVERIFY(!f.searchLine->isEnabled());
    QLabel *status = f.panel->findChild<QLabel *>();
    QVERIFY(status);
    QCOMPARE(status->text(), QStringLiteral("This document cannot be searched."));
}

int main(int argc, char *argv[])
{
    // a throwaway home, so the test neither writes into the real one nor reopens the
    // document where a previous run left it
    QTemporaryDir homeDir;
    Q_ASSERT(homeDir.isValid());
    const QByteArray homePath = QFile::encodeName(homeDir.path());
    qputenv("USERPROFILE", homePath);
    qputenv("HOME", homePath);
    qputenv("XDG_DATA_HOME", QByteArray(homePath + "/.local"));
    qputenv("XDG_CONFIG_HOME", QByteArray(homePath + "/.config"));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("searchresultspaneltest"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));

    SearchResultsPanelTest testObject;
    return QTest::qExec(&testObject, argc, argv);
}

#include "searchresultspaneltest.moc"
