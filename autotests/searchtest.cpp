/*
    SPDX-FileCopyrightText: 2013 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// clazy:excludeall=qstring-allocations

#include <QMimeDatabase>
#include <QSignalSpy>
#include <QTest>

#include "../core/document.h"
#include "../core/page.h"
#include "../core/textpage.h"
#include "../settings_core.h"

Q_DECLARE_METATYPE(Okular::Document::SearchStatus)

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
    void testRtlText();
    void testRtlMixedRunsText();
    void testRtlLineText();
    void testRtlSubWordSearchArea();
    void testLtrTextUnchanged();
    void testRtlMarkFollowsItsBase();
    void testRtlMarkInsideWord();
    void testWordPartsWrittenAsSeparateRuns();
    void testRtlWordWithMarksWrittenAsRuns();
    void testArabicLettersFoldedInSearch();
};

void SearchTest::initTestCase()
{
    qRegisterMetaType<Okular::Document::SearchStatus>();
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

// The generators hand over the characters in the order of the glyphs on the page,
// so reading a word or a line back turns its runs around, while an LTR run keeps
// its order.

// Bounds of all the rectangles of a match.
static Okular::NormalizedRect areaBounds(const Okular::RegularAreaRect *area)
{
    Okular::NormalizedRect bounds;
    bool first = true;
    for (const Okular::NormalizedRect &rect : *area) {
        if (first) {
            bounds = rect;
            first = false;
        } else {
            bounds |= rect;
        }
    }
    return bounds;
}

void SearchTest::testRtlText()
{
    // "مرحبا" as the producer wrote it, i.e. in visual order.
    QList<QString> text;
    text << QStringLiteral("ا") << QStringLiteral("ب") << QStringLiteral("ح") << QStringLiteral("ر") << QStringLiteral("م");

    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < text.size(); i++) {
        rect << Okular::NormalizedRect(0.1 * i, 0.0, 0.1 * (i + 1), 0.1);
    }

    CREATE_PAGE;

    QCOMPARE(tp->text(nullptr), QStringLiteral("مرحبا"));

    delete page;
}

void SearchTest::testRtlMixedRunsText()
{
    // "تجربةTesting" holds an RTL and an LTR run with no space in between. The
    // characters are all emitted left to right, so the Arabic run is stored in
    // visual order and the Latin one is already in reading order.
    QList<QString> text;
    text << QStringLiteral("T") << QStringLiteral("e") << QStringLiteral("s") << QStringLiteral("t") << QStringLiteral("i") << QStringLiteral("n") << QStringLiteral("g") << QStringLiteral("ة") << QStringLiteral("ب") << QStringLiteral("ر")
         << QStringLiteral("ج") << QStringLiteral("ت");

    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < text.size(); i++) {
        rect << Okular::NormalizedRect(0.1 + 0.01 * i, 0.0, 0.1 + 0.01 * (i + 1), 0.1);
    }

    CREATE_PAGE;

    // The two runs touch, so they are one word, and only the Arabic run is
    // turned around.
    QCOMPARE(tp->text(nullptr), QStringLiteral("تجربةTesting"));

    delete page;
}

void SearchTest::testRtlLineText()
{
    // "مرحبا بالعالم": the logically first word is the rightmost one.
    QList<QString> text;
    // "بالعالم" in visual order
    text << QStringLiteral("م") << QStringLiteral("ل") << QStringLiteral("ا") << QStringLiteral("ع") << QStringLiteral("ل") << QStringLiteral("ا") << QStringLiteral("ب");
    // "مرحبا" in visual order
    text << QStringLiteral("ا") << QStringLiteral("ب") << QStringLiteral("ح") << QStringLiteral("ر") << QStringLiteral("م");

    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 7; i++) {
        rect << Okular::NormalizedRect(0.01 * i, 0.0, 0.01 * (i + 1), 0.1);
    }
    for (int i = 0; i < 5; i++) {
        rect << Okular::NormalizedRect(0.09 + 0.01 * i, 0.0, 0.09 + 0.01 * (i + 1), 0.1);
    }

    CREATE_PAGE;

    QCOMPARE(tp->text(nullptr), QStringLiteral("مرحبا بالعالم"));

    delete page;
}

void SearchTest::testRtlSubWordSearchArea()
{
    QList<QString> text;
    // "بالعالم" in visual order
    text << QStringLiteral("م") << QStringLiteral("ل") << QStringLiteral("ا") << QStringLiteral("ع") << QStringLiteral("ل") << QStringLiteral("ا") << QStringLiteral("ب");
    // "مرحبا" in visual order
    text << QStringLiteral("ا") << QStringLiteral("ب") << QStringLiteral("ح") << QStringLiteral("ر") << QStringLiteral("م");

    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 7; i++) {
        rect << Okular::NormalizedRect(0.01 * i, 0.0, 0.01 * (i + 1), 0.1);
    }
    for (int i = 0; i < 5; i++) {
        rect << Okular::NormalizedRect(0.09 + 0.01 * i, 0.0, 0.09 + 0.01 * (i + 1), 0.1);
    }

    CREATE_PAGE;

    // A whole word matches the glyphs of that word, whichever side it is on.
    {
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("مرحبا"), Okular::FromTop, Qt::CaseSensitive, nullptr);
        QVERIFY(result);
        QVERIFY(areaBounds(result) == Okular::NormalizedRect(0.09, 0.0, 0.14, 0.1));
        delete result;
    }
    {
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("بالعالم"), Okular::FromTop, Qt::CaseSensitive, nullptr);
        QVERIFY(result);
        QVERIFY(areaBounds(result) == Okular::NormalizedRect(0.00, 0.0, 0.07, 0.1));
        delete result;
    }
    // One character in the middle of the word: the highlight is the rectangle of
    // that character, not of any other one of the word.
    {
        Okular::RegularAreaRect *result = tp->findText(0, QStringLiteral("ح"), Okular::FromTop, Qt::CaseSensitive, nullptr);
        QVERIFY(result);
        QVERIFY(areaBounds(result) == Okular::NormalizedRect(0.11, 0.0, 0.12, 0.1));
        delete result;
    }

    delete page;
}

void SearchTest::testLtrTextUnchanged()
{
    // The order of a left to right line is already its reading order.
    QList<QString> text;
    text << QStringLiteral("H") << QStringLiteral("e") << QStringLiteral("l") << QStringLiteral("l") << QStringLiteral("o");
    text << QStringLiteral("W") << QStringLiteral("o") << QStringLiteral("r") << QStringLiteral("l") << QStringLiteral("d");

    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 5; i++) {
        rect << Okular::NormalizedRect(0.01 * i, 0.0, 0.01 * (i + 1), 0.1);
    }
    for (int i = 0; i < 5; i++) {
        rect << Okular::NormalizedRect(0.07 + 0.01 * i, 0.0, 0.07 + 0.01 * (i + 1), 0.1);
    }

    CREATE_PAGE;

    QCOMPARE(tp->text(nullptr), QStringLiteral("Hello World"));

    delete page;
}

void SearchTest::testRtlMarkFollowsItsBase()
{
    // 0628 BEH and 064E FATHA: reading the word from its right end must not move
    // the mark in front of the base it belongs to.
    const ushort storedCodes[] = {0x0628, 0x064e};

    QList<QString> text;
    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 2; i++) {
        text << QString(QChar(storedCodes[i]));
        rect << Okular::NormalizedRect(0.10 + 0.02 * i, 0.10, 0.10 + 0.02 * (i + 1), 0.20);
    }

    CREATE_PAGE;

    QString expected;
    for (const ushort code : storedCodes) {
        expected += QChar(code);
    }
    QCOMPARE(tp->text(nullptr), expected);

    delete page;
}

void SearchTest::testRtlMarkInsideWord()
{
    // 0644 LAM, 062A TEH, 0651 SHADDA, 0643 KAF and 0645 MEEM: the mark must stay
    // behind its letter when the word is read back.
    const ushort storedCodes[] = {0x0644, 0x062a, 0x0651, 0x0643, 0x0645};

    QList<QString> text;
    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 5; i++) {
        text << QString(QChar(storedCodes[i]));
        rect << Okular::NormalizedRect(0.10 + 0.02 * i, 0.10, 0.10 + 0.02 * (i + 1), 0.20);
    }

    CREATE_PAGE;

    const ushort expectedCodes[] = {0x0645, 0x0643, 0x062a, 0x0651, 0x0644};
    QString expected;
    for (const ushort code : expectedCodes) {
        expected += QChar(code);
    }
    QCOMPARE(tp->text(nullptr), expected);

    delete page;
}

void SearchTest::testWordPartsWrittenAsSeparateRuns()
{
    // 0627 ALEF, 0642 QAF and 0645 MEEM, in the order a producer writes a word it
    // splits into two runs, the gap between them being narrower than a space.
    QList<QString> text;
    text << QString(QChar(0x0627)) << QString(QChar(0x0642)) << QString(QChar(0x0645));

    {
        QList<Okular::NormalizedRect> rect;
        rect << Okular::NormalizedRect(0.300, 0.100, 0.320, 0.200) << Okular::NormalizedRect(0.320, 0.100, 0.340, 0.200) << Okular::NormalizedRect(0.341, 0.100, 0.361, 0.200);

        CREATE_PAGE;

        // 0645 0642 0627, i.e. "مقا", with no space in it.
        const ushort expectedCodes[] = {0x0645, 0x0642, 0x0627};
        QString expected;
        for (const ushort code : expectedCodes) {
            expected += QChar(code);
        }
        QCOMPARE(tp->text(nullptr), expected);

        // The gap between the runs is not a word break: the word is one area,
        // not one area per run.
        std::unique_ptr<Okular::RegularAreaRect> word = tp->wordAt(Okular::NormalizedPoint(0.315, 0.150));
        QVERIFY(word);
        QCOMPARE(word->count(), 1);
        QCOMPARE(word->first(), Okular::NormalizedRect(0.300, 0.100, 0.361, 0.200));

        delete page;
    }

    {
        QList<Okular::NormalizedRect> rect;
        rect << Okular::NormalizedRect(0.300, 0.100, 0.320, 0.200) << Okular::NormalizedRect(0.320, 0.100, 0.340, 0.200) << Okular::NormalizedRect(0.365, 0.100, 0.385, 0.200);

        CREATE_PAGE;

        // The same characters with a real space between the two runs.
        const ushort expectedCodes[] = {0x0645, 0x0020, 0x0642, 0x0627};
        QString expected;
        for (const ushort code : expectedCodes) {
            expected += QChar(code);
        }
        QCOMPARE(tp->text(nullptr), expected);

        delete page;
    }
}

void SearchTest::testRtlWordWithMarksWrittenAsRuns()
{
    // A word written by LibreOffice as three runs, with the characters and the
    // rectangles (normalized to the page of the document) the generator hands
    // over for them, including the 0020 spaces it adds after each run.
    const ushort storedCodes[] = {0x0627, 0x062f, 0x064b, 0x0020, 0x0631, 0x0651, 0x064e, 0x0634, 0x0020, 0x0645, 0x064f};
    const double storedRects[][4] = {
        {0.865356, 0.0678126, 0.870144, 0.0837197}, // 0627
        {0.870144, 0.0678126, 0.873246, 0.0837197}, // 062F
        {0.873246, 0.0678126, 0.876349, 0.0837197}, // 064B
        {0.876349, 0.0678126, 0.879383, 0.0837197}, // space
        {0.879383, 0.0678126, 0.881682, 0.0846699}, // 0631
        {0.881682, 0.0678126, 0.883992, 0.0846699}, // 0651
        {0.883992, 0.0678126, 0.886270, 0.0846699}, // 064E
        {0.886270, 0.0678126, 0.896954, 0.0846699}, // 0634
        {0.896954, 0.0678126, 0.898281, 0.0846699}, // space
        {0.898281, 0.0693568, 0.901538, 0.0852639}, // 0645
        {0.901538, 0.0693568, 0.904795, 0.0852639}, // 064F
    };

    QList<QString> text;
    QList<Okular::NormalizedRect> rect;
    for (int i = 0; i < 11; i++) {
        text << QString(QChar(storedCodes[i]));
        rect << Okular::NormalizedRect(storedRects[i][0], storedRects[i][1], storedRects[i][2], storedRects[i][3]);
    }
    // The generator puts the end of the line on the last character of the line.
    text[10] += QLatin1Char('\n');

    // The rectangles are normalized to the page of the document, and the space
    // heuristic compares a horizontal gap with a vertical height, so the page of
    // the text page has to have the size of the document's page; the square page
    // the other tests use would distort the comparison.
    auto page = std::make_unique<Okular::Page>(1, 595.303937, 841.889764, Okular::Rotation0);
    auto *tp = new Okular::TextPage();
    for (int i = 0; i < text.size(); i++) {
        tp->append(text[i], rect[i]);
    }
    page->setTextPage(tp);

    // 0645 064F 0634 0631 064E 0651 062F 064B 0627 and the end of the line, i.e.
    // "مُشَرَّدًا", with no space in it: the gaps between the runs are too small
    // to be spaces, and the marks of the reh keep their place behind the reh.
    const ushort expectedCodes[] = {0x0645, 0x064f, 0x0634, 0x0631, 0x064e, 0x0651, 0x062f, 0x064b, 0x0627, 0x000a};
    QString expected;
    for (const ushort code : expectedCodes) {
        expected += QChar(code);
    }
    QCOMPARE(tp->text(nullptr), expected);

    // The three runs are one word: a click on any of them selects all of them.
    std::unique_ptr<Okular::RegularAreaRect> word = tp->wordAt(Okular::NormalizedPoint(0.890, 0.075));
    QVERIFY(word);
    const Okular::NormalizedRect wordBounds = areaBounds(word.get());
    QVERIFY(wordBounds.left <= 0.865356);
    QVERIFY(wordBounds.top <= 0.0678126);
    QVERIFY(wordBounds.right >= 0.904795);
    QVERIFY(wordBounds.bottom >= 0.0852639);

    // The word is found without writing the diacritics.
    const ushort queryCodes[] = {0x0645, 0x0634, 0x0631, 0x062f, 0x0627}; // م ش ر د ا
    QString query;
    for (const ushort code : queryCodes) {
        query += QChar(code);
    }
    {
        std::unique_ptr<Okular::RegularAreaRect> result(tp->findText(0, query, Okular::FromTop, Qt::CaseInsensitive, nullptr));
        QVERIFY(result);
        QCOMPARE(areaBounds(result.get()), Okular::NormalizedRect(0.865356, 0.0678126, 0.904795, 0.0852639));
    }
    {
        // A case sensitive search is exact, so this one has no match.
        std::unique_ptr<Okular::RegularAreaRect> result(tp->findText(0, query, Okular::FromTop, Qt::CaseSensitive, nullptr));
        QVERIFY(!result);
    }

    // The first letters too. The area of a match is the area of every character
    // from the first to the last one, and in reading order the damma of the meem
    // lies between the meem and the sheen, so the area reaches its right edge.
    {
        const ushort prefixCodes[] = {0x0645, 0x0634}; // م ش
        QString prefix;
        for (const ushort code : prefixCodes) {
            prefix += QChar(code);
        }

        std::unique_ptr<Okular::RegularAreaRect> result(tp->findText(0, prefix, Okular::FromTop, Qt::CaseInsensitive, nullptr));
        QVERIFY(result);
        QCOMPARE(areaBounds(result.get()), Okular::NormalizedRect(0.886270, 0.0678126, 0.904795, 0.0852639));
    }
}

void SearchTest::testArabicLettersFoldedInSearch()
{
    // The word on the page and the word as a reader types it: the hamza is often
    // left out, a final taa marbuta is typed as a heh, an alef maksura as a yeh,
    // and an elongation is not typed at all.
    const QList<QPair<QString, QString>> foldings = {
        {QStringLiteral("مكتبة"), QStringLiteral("مكتبه")}, // 0629 TEH MARBUTA and 0647 HEH
        {QStringLiteral("أحمد"), QStringLiteral("احمد")},   // 0623 ALEF WITH HAMZA ABOVE and 0627 ALEF
        {QStringLiteral("مسؤول"), QStringLiteral("مسوول")}, // 0624 WAW WITH HAMZA ABOVE and 0648 WAW
        {QStringLiteral("قائل"), QStringLiteral("قايل")},   // 0626 YEH WITH HAMZA ABOVE and 064A YEH
        {QStringLiteral("مصطفى"), QStringLiteral("مصطفي")}, // 0649 ALEF MAKSURA and 064A YEH
        {QStringLiteral("كتـاب"), QStringLiteral("كتاب")},  // 0640 ARABIC TATWEEL
    };

    for (const auto &[written, typed] : foldings) {
        QList<QString> text;
        text << written;

        QList<Okular::NormalizedRect> rect;
        rect << Okular::NormalizedRect(0.1, 0.1, 0.2, 0.2);

        CREATE_PAGE;

        std::unique_ptr<Okular::RegularAreaRect> result(tp->findText(0, typed, Okular::FromTop, Qt::CaseInsensitive, nullptr));
        QVERIFY(result);

        delete page;
    }

    // A case sensitive search is exact and does not fold anything.
    {
        QList<QString> text;
        text << QStringLiteral("مكتبة");

        QList<Okular::NormalizedRect> rect;
        rect << Okular::NormalizedRect(0.1, 0.1, 0.2, 0.2);

        CREATE_PAGE;

        std::unique_ptr<Okular::RegularAreaRect> result(tp->findText(0, QStringLiteral("مكتبه"), Okular::FromTop, Qt::CaseSensitive, nullptr));
        QVERIFY(!result);

        delete page;
    }
}

QTEST_MAIN(SearchTest)
#include "searchtest.moc"
