/*
    SPDX-FileCopyrightText: 2013 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// clazy:excludeall=qstring-allocations

#include <QApplication>
#include <QFile>
#include <QMimeDatabase>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../core/document.h"
#include "../core/page.h"
#include "../core/textpage.h"
#include "../settings_core.h"

Q_DECLARE_METATYPE(Okular::Document::SearchStatus)

constexpr int allDocumentTestSearchID = 100;

static bool openForSearch(Okular::Document &document, const QString &filePath, uint expectedPages)
{
    QMimeDatabase db;
    if (document.openDocument(filePath, QUrl::fromLocalFile(filePath), db.mimeTypeForFile(filePath)) != Okular::Document::OpenSuccess) {
        return false;
    }
    return document.pages() == expectedPages;
}

class SearchFinishedReceiver : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void searchFinished(int id, Okular::Document::SearchStatus status)
    {
        m_id = id;
        m_status = status;
    }

public:
    int m_id;
    Okular::Document::SearchStatus m_status;
};

class SearchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testNextAndPrevious();
    void test311232();
    void test323262();
    void test323263();
    void test430243();
    void testDottedI();
    void testHyphenAtEndOfLineWithoutYOverlap();
    void testHyphenWithYOverlap();
    void testHyphenAtEndOfPage();
    void testOneColumn();
    void testTwoColumns();
    void testAllDocumentSearchReportsEachScannedPage();
    void testReportedRectIsTheMatchNotTheStartOfThePage();
    void testMatchesAreHighlightedWhileTheScanIsStillRunning();
    void testSearchFromCurrentPageExpandsOutward();
    void testRestartingASearchDropsThePreviousScan();
    void testRestartingAfterResetDropsThePreviousScan();
    void testCancellingASearchStopsTheScan();
};

void SearchTest::initTestCase()
{
    qRegisterMetaType<Okular::Document::SearchStatus>();
    qRegisterMetaType<QList<Okular::NormalizedRect>>();
    Okular::SettingsCore::instance(QStringLiteral("searchtest"));
}

static void createTextPage(const QList<QString> &text, const QList<Okular::NormalizedRect> &rect, Okular::TextPage *&tp, Okular::Page *&page)
{
    tp = new Okular::TextPage();
    for (int i = 0; i < text.size(); i++) {
        tp->append(text[i], rect[i]);
    }

    // The Page::setTextPage method invokes the layout analysis algorithms tested by some tests here
    // and also sets the tp->d->m_page field (the latter was used in older versions of Okular by
    // TextPage::stringLengthAdaptedWithHyphen).
    // Note that calling "delete page;" will delete the TextPage as well.
    page = new Okular::Page(1, 100, 100, Okular::Rotation0);
    page->setTextPage(tp);
}

#define CREATE_PAGE                                                                                                                                                                                                                            \
    QCOMPARE(text.size(), rect.size());                                                                                                                                                                                                        \
    Okular::Page *page;                                                                                                                                                                                                                        \
    Okular::TextPage *tp;                                                                                                                                                                                                                      \
    createTextPage(text, rect, tp, page);

#define TEST_NEXT_PREV(searchType, expectedStatus)                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                          \
        Okular::RegularAreaRect *result = tp->findText(0, searchString, searchType, Qt::CaseSensitive, NULL);                                                                                                                                  \
        QCOMPARE(!!result, expectedStatus);                                                                                                                                                                                                    \
        delete result;                                                                                                                                                                                                                         \
    }

// The test testNextAndPrevious checks that
// a) if one starts a new search, then the first or last match is found, depending on the search direction
//   (2 cases: FromTop/FromBottom)
// b) if the last search has found a match,
//   then clicking the "Next" button moves to the next occurrence an "Previous" to the previous one
//   (if there is any). Altogether there are four combinations of the last search and new search
//   direction: Next-Next, Previous-Previous, Next-Previous, Previous-Next; the first two combination
//   have two subcases (the new search may give a match or not, so altogether 6 cases to test).
// This gives 8 cases altogether. By taking into account the cases where the last search has given no match,
// we would have 4 more cases (Next (no match)-Next, Next (no match)-Previous, Previous (no match)-Previous,
// Previous (no match)-Next), but those are more the business of Okular::Document::searchText rather than
// Okular::TextPage (at least in the multi-page case).

//   We have four test situations: four documents and four corresponding search strings.
//   The first situation (document="ababa", search string="b") is a generic one where the
// two matches are not side-by-side and neither the first character nor the last character of
// the document match. The only special thing is that the search string has only length 1.
//   The second situation (document="abab", search string="ab") is notable for that the two occurrences
// of the search string are side-by-side with no characters in between, so some off-by-one errors
// would be detected by this test. As the first match starts at the beginning at the document the
// last match ends at the end of the document, it also detects off-by-one errors for finding the first/last match.
//   The third situation (document="abababa", search string="aba") is notable for it shows whether
// the next match is allowed to contain letters from the previous one: currently it is not
//(as in the majority of browsers, viewers and editors), and therefore "abababa" is considered to
// contain not three but two occurrences of "aba" (if one starts search from the beginning of the document).
//   The fourth situation (document="a ba b", search string="a b") demonstrates the case when one TinyTextEntity
// contains multiple characters that are contained in different matches (namely, the middle "ba" is one TinyTextEntity);
// in particular, since these matches are side-by-side, this test would detect some off-by-one
// offset errors.

void SearchTest::testNextAndPrevious()
{
#define TEST_NEXT_PREV_SITUATION_COUNT 4

    QList<QString> texts[TEST_NEXT_PREV_SITUATION_COUNT] = {QList<QString>() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a"),
                                                            QList<QString>() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a") << QStringLiteral("b"),
                                                            QList<QString>() << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a") << QStringLiteral("b") << QStringLiteral("a"),
                                                            QList<QString>() << QStringLiteral("a") << QStringLiteral(" ") << QStringLiteral("ba") << QStringLiteral(" ") << QStringLiteral("b")};

    QString searchStrings[TEST_NEXT_PREV_SITUATION_COUNT] = {QStringLiteral("b"), QStringLiteral("ab"), QStringLiteral("aba"), QStringLiteral("a b")};

    for (int i = 0; i < TEST_NEXT_PREV_SITUATION_COUNT; i++) {
        const QList<QString> &text = texts[i];
        const QString &searchString = searchStrings[i];

        QList<Okular::NormalizedRect> rect;

        for (int i = 0; i < text.size(); i++) {
            rect << Okular::NormalizedRect(0.1 * i, 0.0, 0.1 * (i + 1), 0.1);
        }

        CREATE_PAGE;

        // Test 3 of the 8 cases listed above:
        // FromTop, Next-Next (match) and Next-Next (no match)
        TEST_NEXT_PREV(Okular::FromTop, true);
        TEST_NEXT_PREV(Okular::NextResult, true);
        TEST_NEXT_PREV(Okular::NextResult, false);

        // Test 5 cases: FromBottom, Previous-Previous (match), Previous-Next,
        // Next-Previous, Previous-Previous (no match)
        TEST_NEXT_PREV(Okular::FromBottom, true);
        TEST_NEXT_PREV(Okular::PreviousResult, true);
        TEST_NEXT_PREV(Okular::NextResult, true);
        TEST_NEXT_PREV(Okular::PreviousResult, true);
        TEST_NEXT_PREV(Okular::PreviousResult, false);

        delete page;
    }
}

void SearchTest::test311232()
{
    Okular::Document d(nullptr);
    SearchFinishedReceiver receiver;
    QSignalSpy spy(&d, &Okular::Document::searchFinished);

    QObject::connect(&d, &Okular::Document::searchFinished, &receiver, &SearchFinishedReceiver::searchFinished);

    const QString testFile = QStringLiteral(KDESRCDIR "data/file1.pdf");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);
    d.openDocument(testFile, QUrl(), mime);

    const int searchId = 0;
    d.searchText(searchId, QStringLiteral(" i "), true, Qt::CaseSensitive, Okular::Document::NextMatch, false, QColor());
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(receiver.m_id, searchId);
    QCOMPARE(receiver.m_status, Okular::Document::MatchFound);

    d.continueSearch(searchId, Okular::Document::PreviousMatch);
    QTRY_COMPARE(spy.count(), 2);
    QCOMPARE(receiver.m_id, searchId);
    QCOMPARE(receiver.m_status, Okular::Document::NoMatchFound);
}

void SearchTest::test323262()
{
    QList<QString> text;
    text << QStringLiteral("a\n");

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(1, 2, 3, 4);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("a"), Okular::FromBottom, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    delete result;

    delete page;
}

void SearchTest::test323263()
{
    QList<QString> text;
    text << QStringLiteral("a") << QStringLiteral("a") << QStringLiteral("b");

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0, 0, 1, 1) << Okular::NormalizedRect(1, 0, 2, 1) << Okular::NormalizedRect(2, 0, 3, 1);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("ab"), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    Okular::RegularAreaRect expected;
    expected.append(rect[1]);
    expected.append(rect[2]);
    expected.simplify();
    QCOMPARE(*result, expected);
    delete result;

    delete page;
}

void SearchTest::test430243()
{
    // 778 is COMBINING RING ABOVE
    // 197 is LATIN CAPITAL LETTER A WITH RING ABOVE
    QList<QString> text;
    text << QStringLiteral("A") << QString(QChar(778));

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0, 0, 1, 1) << Okular::NormalizedRect(1, 0, 2, 1);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QString(QChar(197)), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    Okular::RegularAreaRect expected;
    expected.append(rect[0] | rect[1]);
    QCOMPARE(*result, expected);
    delete result;

    delete page;
}

void SearchTest::testDottedI()
{
    // Earlier versions of okular had the bug that the letter "İ" (capital dotter i) did not match itself
    // in case-insensitive mode (this was caused by an unnecessary call of toLower() and the fact that
    // QString::fromUtf8("İ").compare(QString::fromUtf8("İ").toLower(), Qt::CaseInsensitive) == FALSE,
    // at least in Qt 4.8).

    // In the future it would be nice to add support for matching "İ"<->"i" and "I"<->"ı" in case-insensitive
    // mode as well (QString::compare does not match them, at least in non-Turkish locales, since it follows
    // the Unicode case-folding rules https://www.unicode.org/Public/6.2.0/ucd/CaseFolding.txt).

    QList<QString> text;
    text << QStringLiteral("İ");

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(1, 2, 3, 4);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("İ"), Okular::FromTop, Qt::CaseInsensitive, nullptr);
    QVERIFY(result);
    delete result;

    delete page;
}

void SearchTest::testHyphenAtEndOfLineWithoutYOverlap()
{
    QList<QString> text;
    text << QStringLiteral("super-") << QStringLiteral("cali-\n") << QStringLiteral("fragilistic") << QStringLiteral("-") << QStringLiteral("expiali") << QStringLiteral("-\n") << QStringLiteral("docious");

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0.4, 0.0, 0.9, 0.1) << Okular::NormalizedRect(0.0, 0.1, 0.6, 0.2) << Okular::NormalizedRect(0.0, 0.2, 0.8, 0.3) << Okular::NormalizedRect(0.8, 0.2, 0.9, 0.3) << Okular::NormalizedRect(0.0, 0.3, 0.8, 0.4)
         << Okular::NormalizedRect(0.8, 0.3, 0.9, 0.4) << Okular::NormalizedRect(0.0, 0.4, 0.7, 0.5);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("supercalifragilisticexpialidocious"), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    Okular::RegularAreaRect expected;
    for (int i = 0; i < text.size(); i++) {
        expected.append(rect[i]);
    }
    expected.simplify();
    QCOMPARE(*result, expected);
    delete result;

    result = tp->findText(0, QStringLiteral("supercalifragilisticexpialidocious"), Okular::FromBottom, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    QCOMPARE(*result, expected);
    delete result;

    // If the user is looking for the text explicitly with the hyphen also find it
    result = tp->findText(0, QStringLiteral("super-cali-fragilistic"), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    delete result;

    // If the user is looking for the text explicitly with the hyphen also find it
    result = tp->findText(0, QStringLiteral("super-cali-fragilistic"), Okular::FromBottom, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    delete result;

    delete page;
}

#define CREATE_PAGE_AND_TEST_SEARCH(searchString, matchExpected)                                                                                                                                                                               \
    {                                                                                                                                                                                                                                          \
        CREATE_PAGE;                                                                                                                                                                                                                           \
                                                                                                                                                                                                                                               \
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral(searchString), Okular::FromTop, Qt::CaseSensitive, NULL);                                                                                                             \
                                                                                                                                                                                                                                               \
        QCOMPARE(!!result, matchExpected);                                                                                                                                                                                                     \
                                                                                                                                                                                                                                               \
        delete result;                                                                                                                                                                                                                         \
        delete page;                                                                                                                                                                                                                           \
    }

void SearchTest::testHyphenWithYOverlap()
{
    QList<QString> text;
    text << QStringLiteral("a-") << QStringLiteral("b");

    QList<Okular::NormalizedRect> rect(2);

    // different lines (50% y-coordinate overlap), first rectangle has larger height
    rect[0] = Okular::NormalizedRect(0.0, 0.0, 0.9, 0.35);
    rect[1] = Okular::NormalizedRect(0.0, 0.3, 0.2, 0.4);
    CREATE_PAGE_AND_TEST_SEARCH("ab", true);

    // different lines (50% y-coordinate overlap), second rectangle has larger height
    rect[0] = Okular::NormalizedRect(0.0, 0.0, 0.9, 0.1);
    rect[1] = Okular::NormalizedRect(0.0, 0.05, 0.2, 0.4);
    CREATE_PAGE_AND_TEST_SEARCH("ab", true);

    // same line (90% y-coordinate overlap), first rectangle has larger height
    rect[0] = Okular::NormalizedRect(0.0, 0.0, 0.4, 0.2);
    rect[1] = Okular::NormalizedRect(0.4, 0.11, 0.6, 0.21);
    CREATE_PAGE_AND_TEST_SEARCH("ab", false);
    CREATE_PAGE_AND_TEST_SEARCH("a-b", true);

    // same line (90% y-coordinate overlap), second rectangle has larger height
    rect[0] = Okular::NormalizedRect(0.0, 0.0, 0.4, 0.1);
    rect[1] = Okular::NormalizedRect(0.4, 0.01, 0.6, 0.2);
    CREATE_PAGE_AND_TEST_SEARCH("ab", false);
    CREATE_PAGE_AND_TEST_SEARCH("a-b", true);
}

void SearchTest::testHyphenAtEndOfPage()
{
    // Tests for segmentation fault that would occur if
    // we tried look ahead (for determining whether the
    // next character is at the same line) at the end of the page.

    QList<QString> text;
    text << QStringLiteral("a-");

    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0, 0, 1, 1);

    CREATE_PAGE;

    {
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("a"), Okular::FromTop, Qt::CaseSensitive, nullptr);
        QVERIFY(result);
        delete result;
    }

    {
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("a"), Okular::FromBottom, Qt::CaseSensitive, nullptr);
        QVERIFY(result);
        delete result;
    }

    delete page;
}

void SearchTest::testOneColumn()
{
    // Tests that the layout analysis algorithm does not create too many columns.
    // Bug 326207 was caused by the fact that if all the horizontal breaks in a line
    // had the same length and were smaller than vertical breaks between lines then
    // the horizontal breaks were treated as column separators.
    //(Note that "same length" means "same length after rounding rectangles to integer pixels".
    // The resolution used by the XY Cut algorithm with a square page is 1000 x 1000,
    // and the horizontal spaces in the example are 0.1, so they are indeed both exactly 100 pixels.)

    QList<QString> text;
    text << QStringLiteral("Only") << QStringLiteral("one") << QStringLiteral("column") << QStringLiteral("here");

    // characters and line breaks have length 0.05, word breaks 0.1
    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0.0, 0.0, 0.2, 0.1) << Okular::NormalizedRect(0.3, 0.0, 0.5, 0.1) << Okular::NormalizedRect(0.6, 0.0, 0.9, 0.1) << Okular::NormalizedRect(0.0, 0.15, 0.2, 0.25);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("Only one column"), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(result);
    delete result;

    delete page;
}

void SearchTest::testTwoColumns()
{
    // Tests that the layout analysis algorithm can detect two columns.

    QList<QString> text;
    text << QStringLiteral("This") << QStringLiteral("text") << QStringLiteral("in") << QStringLiteral("two") << QStringLiteral("is") << QStringLiteral("set") << QStringLiteral("columns.");

    // characters, word breaks and line breaks have length 0.05
    QList<Okular::NormalizedRect> rect;
    rect << Okular::NormalizedRect(0.0, 0.0, 0.20, 0.1) << Okular::NormalizedRect(0.25, 0.0, 0.45, 0.1) << Okular::NormalizedRect(0.6, 0.0, 0.7, 0.1) << Okular::NormalizedRect(0.75, 0.0, 0.9, 0.1)
         << Okular::NormalizedRect(0.0, 0.15, 0.1, 0.25) << Okular::NormalizedRect(0.15, 0.15, 0.3, 0.25) << Okular::NormalizedRect(0.6, 0.15, 1.0, 0.25);

    CREATE_PAGE;

    Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("This text in"), Okular::FromTop, Qt::CaseSensitive, nullptr);
    QVERIFY(!result);
    delete result;

    delete page;
}

void SearchTest::testAllDocumentSearchReportsEachScannedPage()
{
    // simple-multipage.pdf has 40 pages, each reading "Page <n>"
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);
    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);

    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);

    QVERIFY(finishedSpy.wait());

    QCOMPARE(scannedSpy.count(), 40);
    for (int i = 0; i < scannedSpy.count(); ++i) {
        const QList<QVariant> args = scannedSpy.at(i);
        QCOMPARE(args.at(0).toInt(), allDocumentTestSearchID);
        QCOMPARE(args.at(1).toInt(), i);
        QCOMPARE(args.at(2).value<QList<Okular::NormalizedRect>>().count(), 1);
    }
}

void SearchTest::testReportedRectIsTheMatchNotTheStartOfThePage()
{
    // file2.pdf reads "Hola que tal" then "This is nice": "nice" is on the second line
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/file2.pdf"), 2));

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);
    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);

    document.searchText(allDocumentTestSearchID, QStringLiteral("nice"), true, Qt::CaseInsensitive, Okular::Document::AllDocument, false, Qt::yellow);
    QVERIFY(finishedSpy.wait());

    QCOMPARE(scannedSpy.at(0).at(1).toInt(), 0);
    const QList<Okular::NormalizedRect> reported = scannedSpy.at(0).at(2).value<QList<Okular::NormalizedRect>>();
    QCOMPARE(reported.count(), 1);

    // what the match actually covers, according to the page itself
    const Okular::Page *page = document.page(0);
    std::unique_ptr<Okular::RegularAreaRect> match(page->findText(0, QStringLiteral("nice"), Okular::FromTop, Qt::CaseInsensitive));
    QVERIFY(match);
    QVERIFY(!match->isEmpty());
    Okular::NormalizedRect expected = match->first();
    for (const Okular::NormalizedRect &part : *match) {
        expected |= part;
    }

    QVERIFY(expected.top > 0.0);
    QCOMPARE(reported.at(0), expected);
}

void SearchTest::testMatchesAreHighlightedWhileTheScanIsStillRunning()
{
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    bool highlightedWhenReported = false;
    bool checkedAnyPage = false;
    QObject::connect(&document, &Okular::Document::searchPageScanned, &document, [&](int searchID, int pageNumber, const QList<Okular::NormalizedRect> &matches) {
        if (matches.isEmpty() || checkedAnyPage) {
            return;
        }
        checkedAnyPage = true;
        highlightedWhenReported = document.page(pageNumber)->hasHighlights(searchID);
    });

    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);
    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);
    QVERIFY(finishedSpy.wait());

    QVERIFY(checkedAnyPage);
    QVERIFY(highlightedWhenReported);
}

void SearchTest::testSearchFromCurrentPageExpandsOutward()
{
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    document.setViewportPage(10);
    QCOMPARE(document.currentPage(), 10u);

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);
    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);

    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), false, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);
    QVERIFY(finishedSpy.wait());

    QList<int> reportedPages;
    for (int i = 0; i < scannedSpy.count(); ++i) {
        reportedPages << scannedSpy.at(i).at(1).toInt();
    }

    QList<int> expectedPages;
    expectedPages << 10;
    for (int distance = 1; distance <= 10; ++distance) {
        expectedPages << 10 + distance << 10 - distance;
    }
    for (int page = 21; page < 40; ++page) {
        expectedPages << page;
    }

    QCOMPARE(reportedPages, expectedPages);
}

void SearchTest::testRestartingASearchDropsThePreviousScan()
{
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);
    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);

    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);
    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);

    // a superseded scan still walking would report more pages after the finish
    QVERIFY(finishedSpy.wait(30000));
    QTest::qWait(250);

    QCOMPARE(scannedSpy.count(), 40);
}

void SearchTest::testRestartingAfterResetDropsThePreviousScan()
{
    // cancel, reset, search again: what the panel does to restart a search
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);

    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);

    // stop it mid-scan exactly as the panel does, then start the same search again
    bool restarted = false;
    QObject::connect(&document, &Okular::Document::searchPageScanned, &document, [&](int searchID, int, const QList<Okular::NormalizedRect> &) {
        if (restarted) {
            return;
        }
        restarted = true;
        document.cancelSearch();
        document.resetSearch(searchID);
        document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);
    });

    // one scan of 40 pages plus the page reported before the restart
    QTRY_VERIFY_WITH_TIMEOUT(scannedSpy.count() >= 41, 30000);
    QTest::qWait(250);

    QVERIFY(restarted);
    QVERIFY2(scannedSpy.count() <= 41, qPrintable(QStringLiteral("%1 pages reported for a 40 page document").arg(scannedSpy.count())));
}

void SearchTest::testCancellingASearchStopsTheScan()
{
    Okular::Document document(nullptr);
    QVERIFY(openForSearch(document, QStringLiteral(KDESRCDIR "data/simple-multipage.pdf"), 40));

    QSignalSpy scannedSpy(&document, &Okular::Document::searchPageScanned);

    bool cancelled = false;
    QObject::connect(&document, &Okular::Document::searchPageScanned, &document, [&](int searchID, int, const QList<Okular::NormalizedRect> &) {
        if (cancelled) {
            return;
        }
        cancelled = true;
        document.cancelSearch();
        document.resetSearch(searchID);
    });

    QSignalSpy finishedSpy(&document, &Okular::Document::searchFinished);
    document.searchText(allDocumentTestSearchID, QStringLiteral("Page"), true, Qt::CaseSensitive, Okular::Document::AllDocument, false, Qt::yellow);

    QVERIFY(finishedSpy.wait(30000));
    QCOMPARE(finishedSpy.at(0).at(1).value<Okular::Document::SearchStatus>(), Okular::Document::SearchCancelled);
    QTest::qWait(250);

    QVERIFY(cancelled);
    QVERIFY2(scannedSpy.count() < 40, qPrintable(QStringLiteral("scanned %1 pages of 40 after cancelling").arg(scannedSpy.count())));
}

int main(int argc, char *argv[])
{
    // a throwaway home: Okular remembers a document's last page under $XDG_DATA_HOME
    QTemporaryDir homeDir;
    Q_ASSERT(homeDir.isValid());
    const QByteArray homePath = QFile::encodeName(homeDir.path());
    qputenv("USERPROFILE", homePath);
    qputenv("HOME", homePath);
    qputenv("XDG_DATA_HOME", QByteArray(homePath + "/.local"));
    qputenv("XDG_CONFIG_HOME", QByteArray(homePath + "/.config"));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("searchtest"));
    app.setOrganizationDomain(QStringLiteral("kde.org"));

    SearchTest testObject;
    return QTest::qExec(&testObject, argc, argv);
}

#include "searchtest.moc"
