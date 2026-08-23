/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

#include "../core/area.h"
#include "../core/document.h"
#include "../core/page.h"
#include "../core/textpage.h"
#include "../part/searchresultsmodel.h"
#include "../settings_core.h"

class SearchResultsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testSnippetIsTheLineContainingTheMatch();
    void testMatchIsLocatedInsideTheSnippet();
    void testMatchIsLocatedWhenTheWordRepeatsInTheLine();
    void testLongSnippetIsShortenedAroundTheMatch();
    void testBuiltResultIsShortenedToTheRequestedLength();
    void testModelAddsResultsAsRows();
    void testModelExposesResultFieldsAsRoles();
    void testModelClearDropsEveryResult();
    void testViewportCentresOnTheMatch();
    void testModelKeepsResultsInPageOrder();
    void testInsertReturnsWhereTheBatchLanded();
    void testInsertingManyBatchesStaysFast();
};

void SearchResultsTest::initTestCase()
{
    Okular::SettingsCore::instance(QStringLiteral("searchresultstest"));
}

// Builds a page holding two lines of text:
//   "The quick brown fox"
//   "over the lazy dog"
// Words are 0.1 apart on a line, lines are 0.05 apart, as in searchtest.cpp.
static Okular::Page *createTwoLinePage()
{
    const QList<QString> words = {QStringLiteral("The"), QStringLiteral("quick"), QStringLiteral("brown"), QStringLiteral("fox"), QStringLiteral("over"), QStringLiteral("the"), QStringLiteral("lazy"), QStringLiteral("dog")};
    const QList<Okular::NormalizedRect> rects = {Okular::NormalizedRect(0.00, 0.00, 0.15, 0.10),
                                                 Okular::NormalizedRect(0.25, 0.00, 0.45, 0.10),
                                                 Okular::NormalizedRect(0.55, 0.00, 0.75, 0.10),
                                                 Okular::NormalizedRect(0.85, 0.00, 1.00, 0.10),
                                                 Okular::NormalizedRect(0.00, 0.15, 0.15, 0.25),
                                                 Okular::NormalizedRect(0.25, 0.15, 0.40, 0.25),
                                                 Okular::NormalizedRect(0.50, 0.15, 0.65, 0.25),
                                                 Okular::NormalizedRect(0.75, 0.15, 0.90, 0.25)};

    Okular::TextPage *tp = new Okular::TextPage();
    for (int i = 0; i < words.size(); ++i) {
        tp->append(words[i], rects[i]);
    }
    Okular::Page *page = new Okular::Page(0, 100, 100, Okular::Rotation0);
    page->setTextPage(tp);
    return page;
}

void SearchResultsTest::testSnippetIsTheLineContainingTheMatch()
{
    // A result's snippet is the whole line of text the match sits on, so the user
    // reads the match in context. It must not bleed into the neighbouring line.
    Okular::Page *page = createTwoLinePage();

    // the bounding rectangle of the word "brown", on the first line
    const SearchResult result = SearchResultsModel::searchResultForMatch(page, 3, Okular::NormalizedRect(0.55, 0.00, 0.75, 0.10));

    QCOMPARE(result.pageNumber, 3);
    QCOMPARE(result.snippet, QStringLiteral("The quick brown fox"));

    delete page;
}

void SearchResultsTest::testMatchIsLocatedInsideTheSnippet()
{
    // The result records where the match sits inside the snippet, so the view can
    // draw that part of the line differently from its context.
    Okular::Page *page = createTwoLinePage();

    const SearchResult result = SearchResultsModel::searchResultForMatch(page, 0, Okular::NormalizedRect(0.55, 0.00, 0.75, 0.10));

    QCOMPARE(result.snippet.mid(result.matchOffset, result.matchLength), QStringLiteral("brown"));

    delete page;
}

// Builds a page holding the single line "one two one three", where the searched
// word appears twice.
static Okular::Page *createRepeatedWordPage()
{
    const QList<QString> words = {QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("one"), QStringLiteral("three")};
    const QList<Okular::NormalizedRect> rects = {
        Okular::NormalizedRect(0.00, 0.00, 0.15, 0.10), Okular::NormalizedRect(0.25, 0.00, 0.40, 0.10), Okular::NormalizedRect(0.50, 0.00, 0.65, 0.10), Okular::NormalizedRect(0.75, 0.00, 0.95, 0.10)};

    Okular::TextPage *tp = new Okular::TextPage();
    for (int i = 0; i < words.size(); ++i) {
        tp->append(words[i], rects[i]);
    }
    Okular::Page *page = new Okular::Page(0, 100, 100, Okular::Rotation0);
    page->setTextPage(tp);
    return page;
}

void SearchResultsTest::testMatchIsLocatedWhenTheWordRepeatsInTheLine()
{
    // When the searched word appears more than once on the line, the offset must point
    // at the occurrence that was actually matched, not simply at the first one.
    Okular::Page *page = createRepeatedWordPage();

    // the bounding rectangle of the *second* "one"
    const SearchResult result = SearchResultsModel::searchResultForMatch(page, 0, Okular::NormalizedRect(0.50, 0.00, 0.65, 0.10));

    QCOMPARE(result.snippet, QStringLiteral("one two one three"));
    QCOMPARE(result.matchOffset, 8);
    QCOMPARE(result.snippet.mid(result.matchOffset, result.matchLength), QStringLiteral("one"));

    delete page;
}

void SearchResultsTest::testLongSnippetIsShortenedAroundTheMatch()
{
    SearchResult result;
    result.snippet = QStringLiteral("aaaa ").repeated(30) + QStringLiteral("needle") + QStringLiteral(" bbbb").repeated(30);
    result.matchOffset = result.snippet.indexOf(QStringLiteral("needle"));
    result.matchLength = 6;

    SearchResultsModel::elideSnippetAroundMatch(result, 40);

    QVERIFY(result.snippet.length() <= 42);
    QCOMPARE(result.snippet.mid(result.matchOffset, result.matchLength), QStringLiteral("needle"));
    QVERIFY(result.snippet.startsWith(QChar(0x2026)));
    QVERIFY(result.snippet.endsWith(QChar(0x2026)));
}

void SearchResultsTest::testBuiltResultIsShortenedToTheRequestedLength()
{
    // Building a result applies the shortening itself, so no caller can forget to.
    Okular::Page *page = createTwoLinePage();

    // "The quick brown fox" is 19 characters, so a limit of 10 must shorten it
    const SearchResult result = SearchResultsModel::searchResultForMatch(page, 0, Okular::NormalizedRect(0.55, 0.00, 0.75, 0.10), 10);

    QVERIFY(result.snippet.length() <= 12);
    QCOMPARE(result.snippet.mid(result.matchOffset, result.matchLength), QStringLiteral("brown"));

    delete page;
}

static SearchResult makeResult(int pageNumber, const QString &snippet)
{
    SearchResult result;
    result.pageNumber = pageNumber;
    result.snippet = snippet;
    return result;
}

void SearchResultsTest::testModelAddsResultsAsRows()
{
    SearchResultsModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);

    model.insertResults({makeResult(2, QStringLiteral("first")), makeResult(2, QStringLiteral("second"))});
    model.insertResults({makeResult(7, QStringLiteral("third"))});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(insertedSpy.count(), 2);
    QCOMPARE(insertedSpy.at(1).at(1).toInt(), 2); // first row of the second batch
    QCOMPARE(insertedSpy.at(1).at(2).toInt(), 2); // last row of the second batch
    QCOMPARE(model.resultAt(2).pageNumber, 7);
    QCOMPARE(model.resultAt(2).snippet, QStringLiteral("third"));
}

void SearchResultsTest::testModelExposesResultFieldsAsRoles()
{
    SearchResultsModel model;
    SearchResult result = makeResult(41, QStringLiteral("The quick brown fox"));
    result.matchOffset = 10;
    result.matchLength = 5;
    model.insertResults({result});

    const QModelIndex index = model.index(0, 0);

    QCOMPARE(index.data(SearchResultsModel::PageNumberRole).toInt(), 41);
    QCOMPARE(index.data(SearchResultsModel::SnippetRole).toString(), QStringLiteral("The quick brown fox"));
    QCOMPARE(index.data(SearchResultsModel::MatchOffsetRole).toInt(), 10);
    QCOMPARE(index.data(SearchResultsModel::MatchLengthRole).toInt(), 5);
    QCOMPARE(index.data(Qt::DisplayRole).toString(), QStringLiteral("The quick brown fox"));
}

void SearchResultsTest::testModelClearDropsEveryResult()
{
    SearchResultsModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.insertResults({makeResult(1, QStringLiteral("stale")), makeResult(2, QStringLiteral("stale"))});
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.clear();

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(removedSpy.count(), 1);
}

void SearchResultsTest::testViewportCentresOnTheMatch()
{
    SearchResult result;
    result.pageNumber = 7;
    result.boundingRect = Okular::NormalizedRect(0.2, 0.4, 0.6, 0.5);

    const Okular::DocumentViewport viewport = SearchResultsModel::viewportForResult(result);

    QCOMPARE(viewport.pageNumber, 7);
    QVERIFY(viewport.rePos.enabled);
    QCOMPARE(viewport.rePos.pos, Okular::DocumentViewport::Center);
    QCOMPARE(viewport.rePos.normalizedX, 0.4);
    QCOMPARE(viewport.rePos.normalizedY, 0.45);
}

void SearchResultsTest::testModelKeepsResultsInPageOrder()
{
    // batches arrive in scan order, outward from the page being read, not in page order
    SearchResultsModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    model.insertResults({makeResult(25, QStringLiteral("a"))});
    model.insertResults({makeResult(26, QStringLiteral("b"))});
    model.insertResults({makeResult(0, QStringLiteral("c"))});
    model.insertResults({makeResult(1, QStringLiteral("d"))});

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.resultAt(0).pageNumber, 0);
    QCOMPARE(model.resultAt(1).pageNumber, 1);
    QCOMPARE(model.resultAt(2).pageNumber, 25);
    QCOMPARE(model.resultAt(3).pageNumber, 26);
    QCOMPARE(model.resultAt(0).snippet, QStringLiteral("c"));
    QCOMPARE(model.resultAt(3).snippet, QStringLiteral("b"));
}

void SearchResultsTest::testInsertReturnsWhereTheBatchLanded()
{
    SearchResultsModel model;

    QCOMPARE(model.insertResults({makeResult(25, QStringLiteral("a"))}), 0);
    QCOMPARE(model.insertResults({makeResult(26, QStringLiteral("b"))}), 1);
    QCOMPARE(model.insertResults({makeResult(0, QStringLiteral("c"))}), 0);
    QCOMPARE(model.insertResults({makeResult(1, QStringLiteral("d"))}), 1);
    QCOMPARE(model.insertResults({}), -1);
}

void SearchResultsTest::testInsertingManyBatchesStaysFast()
{
    // a linear insertion-point search is quadratic over a scan (14 s here, against 13 ms)
    SearchResultsModel model;

    const int pageCount = 150000;
    QList<SearchResult> batch(1);

    QElapsedTimer timer;
    timer.start();
    for (int page = 0; page < pageCount; ++page) {
        batch[0] = makeResult(page, QStringLiteral("x"));
        model.insertResults(batch);
    }
    const qint64 elapsedMs = timer.elapsed();

    QCOMPARE(model.rowCount(), pageCount);
    QVERIFY2(elapsedMs < 2000, qPrintable(QStringLiteral("insertResults took %1 ms for %2 batches; a linear insertion-point scan has regressed").arg(elapsedMs).arg(pageCount)));
}

QTEST_MAIN(SearchResultsTest)
#include "searchresultstest.moc"
