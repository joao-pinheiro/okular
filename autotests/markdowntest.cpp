/*
    SPDX-FileCopyrightText: 2020 Markus Brenneis <support.gulp21+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "../settings_core.h"
#include "core/action.h"
#include "core/area.h"
#include "core/document.h"
#include "core/global.h"
#include "core/page.h"
#include "generators/markdown/converter.h"
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <memory>

class MarkdownTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void testFancyPantsEnabled();
    void testFancyPantsDisabled();
    void testImageSizes();
    void testSpecialCharsInImageFileName();
    void testStrikeThrough();
    void testHtmlTagFixup();
    void testTableOfContents();
    void testSynopsis();
    void testSynopsisPages();
    void testIssue4LinkToAnchorOnNextPageOfParagraph();
    void testIssue4LinkToAnchorOnFirstPageOfParagraph();
    void testIssue4LinkTargetIsLineOfAnchor();
    void testIssue4LinkToAnchorAtStartOfBlock();

private:
    void findImages(QTextFrame *parent, QList<QTextImageFormat> &images);
    void findImages(const QTextBlock &parent, QList<QTextImageFormat> &images);
};

void MarkdownTest::initTestCase()
{
    // don't let the settings of whoever runs the test (backend font, SmartyPants) change the result
    QStandardPaths::setTestModeEnabled(true);
    Okular::SettingsCore::instance(QStringLiteral("markdowntest"));
}

void MarkdownTest::testFancyPantsEnabled()
{
    Markdown::Converter converter;
    converter.setFancyPantsEnabled(true);
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/imageSizes.md")));

    QTextFrame::iterator secondFrame = ++(document->rootFrame()->begin());
    QVERIFY(secondFrame.currentBlock().text().startsWith(QStringLiteral("©")));
}

void MarkdownTest::testFancyPantsDisabled()
{
    Markdown::Converter converter;
    converter.setFancyPantsEnabled(false);
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/imageSizes.md")));

    QTextFrame::iterator secondFrame = ++(document->rootFrame()->begin());
    QVERIFY(secondFrame.currentBlock().text().startsWith(QStringLiteral("(c)")));
}

void MarkdownTest::testImageSizes()
{
    Markdown::Converter converter;
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/imageSizes.md")));

    QTextFrame *parent = document->rootFrame();

    QList<QTextImageFormat> images;
    findImages(parent, images);

    QCOMPARE(images.size(), 17);

    qreal expectedSizes[][2] = {// width, height
                                // small image
                                {412, 349},
                                {100, 84.70873786407767},
                                {118.0515759312321, 100},
                                {100, 100},
                                {890, 753.9077669902913},
                                {890, 890},
                                // wide image
                                {890, 178},
                                {100, 20},
                                {500, 100},
                                {100, 100},
                                {890, 178},
                                {890, 890},
                                // tall image
                                {300, 1500},
                                {100, 500},
                                {20, 100},
                                {100, 100},
                                {890, 890}};

    for (int i = 0; i < images.size(); i++) {
        QCOMPARE(images[i].width(), expectedSizes[i][0]);
        QCOMPARE(images[i].height(), expectedSizes[i][1]);
    }
}

void MarkdownTest::findImages(QTextFrame *parent, QList<QTextImageFormat> &images)
{
    for (QTextFrame::iterator it = parent->begin(); !it.atEnd(); ++it) {
        QTextFrame *textFrame = it.currentFrame();
        const QTextBlock textBlock = it.currentBlock();

        if (textFrame) {
            findImages(textFrame, images);
        } else if (textBlock.isValid()) {
            findImages(textBlock, images);
        }
    }
}

void MarkdownTest::findImages(const QTextBlock &parent, QList<QTextImageFormat> &images)
{
    for (QTextBlock::iterator it = parent.begin(); !it.atEnd(); ++it) {
        const QTextFragment textFragment = it.fragment();
        if (textFragment.isValid()) {
            const QTextCharFormat textCharFormat = textFragment.charFormat();
            if (textCharFormat.isImageFormat()) {
                images.append(textCharFormat.toImageFormat());
            }
        }
    }
}

void MarkdownTest::testSpecialCharsInImageFileName()
{
    Markdown::Converter converter;
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/imageUrlsWithSpecialChars.md")));

    QTextFrame *parent = document->rootFrame();

    QList<QTextImageFormat> images;
    findImages(parent, images);

    QCOMPARE(images.size(), 1);
    QVERIFY(images[0].name().endsWith(QStringLiteral("kartöffelchen.jpg")));
    QVERIFY(!images[0].name().contains(QStringLiteral("kart%C3%B6ffelchen.jpg")));
}

void MarkdownTest::testStrikeThrough()
{
    Markdown::Converter converter;
    converter.setFancyPantsEnabled(true);
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/strikethrough.md")));

    const QTextFrame *rootFrame = document->rootFrame();
    auto frameIter = rootFrame->begin();

    // Header line.
    QCOMPARE_NE(frameIter, rootFrame->end());
    QCOMPARE(frameIter.currentBlock().text(), QStringLiteral("Test for strikethrough tag workaround "));

    // Ordinary line.
    {
        ++frameIter;
        QCOMPARE_NE(frameIter, rootFrame->end());
        auto block = frameIter.currentBlock();
        QCOMPARE(block.text(), QStringLiteral("Line without strikethrough "));
        // Single format for the entire line.
        auto formats = block.textFormats();
        QCOMPARE(formats.size(), 1);
        QVERIFY(!formats[0].format.fontStrikeOut());
    }

    // Part of the line has a strikethrough.
    {
        ++frameIter;
        QCOMPARE_NE(frameIter, rootFrame->end());
        auto block = frameIter.currentBlock();
        QCOMPARE(block.text(), QStringLiteral("Line with strikethrough "));
        // The "with" should be the only thing striked out.
        auto formats = block.textFormats();
        QCOMPARE(formats.size(), 3);
        QCOMPARE(block.text().sliced(formats[0].start, formats[0].length), QStringLiteral("Line "));
        QVERIFY(!formats[0].format.fontStrikeOut());
        QCOMPARE(block.text().sliced(formats[1].start, formats[1].length), QStringLiteral("with"));
        QVERIFY(formats[1].format.fontStrikeOut());
        QCOMPARE(block.text().sliced(formats[2].start, formats[2].length), QStringLiteral(" strikethrough "));
        QVERIFY(!formats[2].format.fontStrikeOut());
    }

    // Code block shouldn't have leading spaces, or be modified by our fixup.
    {
        ++frameIter;
        QCOMPARE_NE(frameIter, rootFrame->end());
        auto block = frameIter.currentBlock();
        QCOMPARE(block.text(), QStringLiteral("~~Strikethrough~~ should be <del>ignored</del> in a <s>code block</s>"));
    }
}

void MarkdownTest::testHtmlTagFixup()
{
    // These should passthrough unchanged.
    const QString testCases[] = {
        QStringLiteral("basic test"),
        QStringLiteral("<p>test with tag</p>"),
        QStringLiteral("line with <em>combined <strong>tags</strong></em>"),
    };
    for (const QString &inputHtml : testCases) {
        const QString outputHtml = Markdown::detail::fixupHtmlTags(QString(inputHtml));
        QCOMPARE(outputHtml, inputHtml);
    }

    // <del> should become <s>.
    {
        const QString outputHtml = Markdown::detail::fixupHtmlTags(QStringLiteral("<del>test</del>"));
        QCOMPARE(outputHtml, QStringLiteral("<s>test</s>"));
    }
}

void MarkdownTest::testTableOfContents()
{
    struct Title {
        int level;
        QString title;
        int blockHeadingLevel;
        QString blockText;
    };
    QList<Title> titles;

    Markdown::Converter converter;
    connect(&converter, &Okular::TextDocumentConverter::addTitle, this, [&titles](int level, const QString &title, const QTextBlock &block) { titles.append({level, title, block.blockFormat().headingLevel(), block.text()}); });
    std::unique_ptr<QTextDocument> document(converter.convert(QStringLiteral(KDESRCDIR "data/toc.md")));
    QVERIFY(document);

    const QList<QPair<int, QString>> expected = {
        {1, QStringLiteral("Top emph and code")},
        {2, QStringLiteral("Sub one")},
        {3, QStringLiteral("Deep")},
        {1, QStringLiteral("Setext")},
        {5, QStringLiteral("Skipped levels")},
        // the heading containing only an existing image is skipped
        {2, QStringLiteral("Pictured")},
        // a missing image is replaced by its alt text
        {2, QStringLiteral("Gone")},
        {2, QStringLiteral("Two lines")},
        {2, QStringLiteral("Raw html")},
        {3, QStringLiteral("In cell")},
        // "# not a heading" in the code block is skipped
        {2, QStringLiteral("Sub two")},
    };

    QList<QPair<int, QString>> actual;
    for (const Title &title : std::as_const(titles)) {
        actual.append({title.level, title.title});
    }
    QCOMPARE(actual, expected);
    for (int i = 0; i < expected.size(); ++i) {
        // the block handed over has to be the heading itself, it is what the title links to
        QCOMPARE(titles[i].blockHeadingLevel, expected[i].first);
        QVERIFY(titles[i].blockText.simplified().endsWith(expected[i].second));
    }
}

void MarkdownTest::testSynopsis()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/toc.md");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);

    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);

    const Okular::DocumentSynopsis *synopsis = document.documentSynopsis();
    QVERIFY(synopsis);

    const QList<Okular::DocumentSynopsis::Element> topLevel = synopsis->children();
    QCOMPARE(topLevel.size(), 2);
    QCOMPARE(topLevel[0].title(), QStringLiteral("Top emph and code"));
    QCOMPARE(topLevel[1].title(), QStringLiteral("Setext"));

    const QList<Okular::DocumentSynopsis::Element> topChildren = topLevel[0].children();
    QCOMPARE(topChildren.size(), 1);
    QCOMPARE(topChildren[0].title(), QStringLiteral("Sub one"));
    const QList<Okular::DocumentSynopsis::Element> subChildren = topChildren[0].children();
    QCOMPARE(subChildren.size(), 1);
    QCOMPARE(subChildren[0].title(), QStringLiteral("Deep"));

    const QList<Okular::DocumentSynopsis::Element> setextChildren = topLevel[1].children();
    QCOMPARE(setextChildren.size(), 6);
    QCOMPARE(setextChildren[0].title(), QStringLiteral("Skipped levels"));
    QCOMPARE(setextChildren[4].title(), QStringLiteral("Raw html"));
    QCOMPARE(setextChildren[5].title(), QStringLiteral("Sub two"));
    const QList<Okular::DocumentSynopsis::Element> rawHtmlChildren = setextChildren[4].children();
    QCOMPARE(rawHtmlChildren.size(), 1);
    QCOMPARE(rawHtmlChildren[0].title(), QStringLiteral("In cell"));

    // every entry points further down the first page than the one before it
    const QList<Okular::DocumentSynopsis::Element> inOrder = {topLevel[0], topChildren[0], subChildren[0], topLevel[1]};
    double previousY = -1;
    for (const Okular::DocumentSynopsis::Element &element : inOrder) {
        const Okular::DocumentViewport viewport = element.viewPort().value_or(Okular::DocumentViewport());
        QCOMPARE(viewport.pageNumber, 0);
        QVERIFY(viewport.rePos.normalizedY > previousY);
        previousY = viewport.rePos.normalizedY;
    }

    document.closeDocument();
}

void MarkdownTest::testSynopsisPages()
{
    // 60 headings named "Heading NNN" spread over several pages
    const QString testFile = QStringLiteral(KDESRCDIR "data/toc-long.md");
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(testFile);

    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), mime), Okular::Document::OpenSuccess);
    QVERIFY(document.pages() >= 3);

    const Okular::DocumentSynopsis *synopsis = document.documentSynopsis();
    QVERIFY(synopsis);
    const QList<Okular::DocumentSynopsis::Element> entries = synopsis->children();
    QCOMPARE(entries.size(), 60);

    int previousPage = 0;
    double previousY = -1;
    for (const Okular::DocumentSynopsis::Element &entry : entries) {
        const Okular::DocumentViewport viewport = entry.viewPort().value_or(Okular::DocumentViewport());
        QVERIFY(viewport.isValid());
        QVERIFY(viewport.pageNumber >= previousPage);
        QVERIFY(viewport.pageNumber < int(document.pages()));
        QVERIFY(viewport.pageNumber > previousPage || viewport.rePos.normalizedY > previousY);

        // the entry has to point at the page that shows the heading
        document.requestTextPage(viewport.pageNumber);
        const QByteArray where = QStringLiteral("%1 points at page %2").arg(entry.title()).arg(viewport.pageNumber).toUtf8();
        QVERIFY2(document.page(viewport.pageNumber)->text().contains(entry.title()), where.constData());

        previousPage = viewport.pageNumber;
        previousY = viewport.rePos.normalizedY;
    }
    // the headings do not all fit in the first page
    QVERIFY(previousPage > 0);

    document.closeDocument();
}

// anchors-long.md: 12 paragraphs that start with the word PARAnn and hold the anchors earlyNN,
// middleNN and lateNN, each on the word of the same name in upper case; two of the paragraphs
// are longer than a page with a font of 8 points or more. 4 headings that start with the anchor headNN on the word HEADnn.
// Before all that one link per line to every anchor, in the order of anchorsLongLinks().
static QStringList anchorsLongLinks()
{
    QStringList links;
    for (int paragraph = 1; paragraph <= 12; ++paragraph) {
        for (const char *kind : {"EARLY", "MIDDLE", "LATE"}) {
            links << QStringLiteral("%1%2").arg(QLatin1String(kind)).arg(paragraph, 2, 10, QLatin1Char('0'));
        }
    }
    for (int heading = 1; heading <= 4; ++heading) {
        links << QStringLiteral("HEAD%1").arg(heading, 2, 10, QLatin1Char('0'));
    }
    return links;
}

// The target of each internal link of the document, in the order the links are laid out
static QList<Okular::DocumentViewport> linkTargets(const Okular::Document &document)
{
    QList<Okular::DocumentViewport> targets;
    for (uint i = 0; i < document.pages(); ++i) {
        QList<QPair<int, const Okular::GotoAction *>> linksOfPage; // top of the link, what it does
        const QList<Okular::ObjectRect *> rects = document.page(i)->objectRects();
        for (const Okular::ObjectRect *rect : rects) {
            if (rect->objectType() != Okular::ObjectRect::Action) {
                continue;
            }
            const Okular::Action *action = static_cast<const Okular::Action *>(rect->object());
            if (action->actionType() != Okular::Action::Goto) {
                continue;
            }
            linksOfPage.append({rect->boundingRect(10000, 10000).top(), static_cast<const Okular::GotoAction *>(action)});
        }
        std::sort(linksOfPage.begin(), linksOfPage.end(), [](const auto &left, const auto &right) { return left.first < right.first; });
        for (const auto &link : std::as_const(linksOfPage)) {
            targets.append(link.second->destViewport());
        }
    }
    return targets;
}

static QStringList pageTexts(Okular::Document &document)
{
    QStringList texts;
    for (uint i = 0; i < document.pages(); ++i) {
        document.requestTextPage(i);
        texts << document.page(i)->text();
    }
    return texts;
}

// The page whose text has the word, -1 if there is none
static int pageShowing(const QStringList &texts, const QString &word)
{
    for (int i = 0; i < texts.size(); ++i) {
        if (texts[i].contains(word)) {
            return i;
        }
    }
    return -1;
}

// What is wrong when the target is not the line of the page that shows the word, nothing otherwise.
// Which page shows the word comes from the text of the pages. Where on the page the word is gets
// computed the way the target is, so this shows that the target is the line of the word, not
// where that line is drawn: textdocumentviewporttest checks that.
static QByteArray notAtLineOf(const Okular::Document &document, const Okular::DocumentViewport &target, int wordPage, const QString &word)
{
    const std::unique_ptr<Okular::RegularAreaRect> area(document.page(wordPage)->findText(0, word, Okular::FromTop, Qt::CaseSensitive));
    if (!area || area->isEmpty()) {
        return QStringLiteral("%1 was not found on page %2").arg(word).arg(wordPage).toUtf8();
    }
    const Okular::NormalizedRect wordRect = area->first();
    // the target is the top of a line: the word is in that line when their tops are less than half its height apart
    if (target.pageNumber == wordPage && qAbs(target.rePos.normalizedY - wordRect.top) < (wordRect.bottom - wordRect.top) / 2) {
        return QByteArray();
    }
    return QStringLiteral("the link to %1 opens page %2 at %3, the word is on page %4 from %5 to %6").arg(word).arg(target.pageNumber).arg(target.rePos.normalizedY).arg(wordPage).arg(wordRect.top).arg(wordRect.bottom).toUtf8();
}

// Issue 4, claim 1: a link to an anchor in the part of a paragraph that is laid out on a
// following page opens the page that shows the anchor
void MarkdownTest::testIssue4LinkToAnchorOnNextPageOfParagraph()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/anchors-long.md");
    QMimeDatabase db;
    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), db.mimeTypeForFile(testFile)), Okular::Document::OpenSuccess);

    const QStringList links = anchorsLongLinks();
    const QList<Okular::DocumentViewport> targets = linkTargets(document);
    QCOMPARE(targets.size(), links.size());
    const QStringList texts = pageTexts(document);

    int anchorsOnNextPage = 0;
    for (int i = 0; i < links.size(); ++i) {
        if (links[i].startsWith(QLatin1String("HEAD"))) {
            continue;
        }
        const int anchorPage = pageShowing(texts, links[i]);
        const int paragraphPage = pageShowing(texts, QStringLiteral("PARA") + links[i].right(2));
        QVERIFY(anchorPage >= 0);
        QVERIFY(paragraphPage >= 0);
        if (anchorPage == paragraphPage) {
            continue;
        }
        ++anchorsOnNextPage;
        const QByteArray wrong = notAtLineOf(document, targets[i], anchorPage, links[i]);
        QVERIFY2(wrong.isEmpty(), wrong.constData());
    }
    // two paragraphs are longer than a page, unless the font is very small
    QVERIFY(anchorsOnNextPage > 0);

    document.closeDocument();
}

// Issue 4, claim 2: a link to an anchor on the page its paragraph starts on opens the line
// of the anchor, not the start of the paragraph
void MarkdownTest::testIssue4LinkTargetIsLineOfAnchor()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/anchors-long.md");
    QMimeDatabase db;
    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), db.mimeTypeForFile(testFile)), Okular::Document::OpenSuccess);

    const QStringList links = anchorsLongLinks();
    const QList<Okular::DocumentViewport> targets = linkTargets(document);
    QCOMPARE(targets.size(), links.size());
    const QStringList texts = pageTexts(document);

    int anchorsOnPageOfParagraph = 0;
    for (int i = 0; i < links.size(); ++i) {
        if (links[i].startsWith(QLatin1String("HEAD"))) {
            continue;
        }
        const int anchorPage = pageShowing(texts, links[i]);
        QVERIFY(anchorPage >= 0);
        if (anchorPage != pageShowing(texts, QStringLiteral("PARA") + links[i].right(2))) {
            continue;
        }
        ++anchorsOnPageOfParagraph;
        const QByteArray wrong = notAtLineOf(document, targets[i], anchorPage, links[i]);
        QVERIFY2(wrong.isEmpty(), wrong.constData());
    }
    QVERIFY(anchorsOnPageOfParagraph > 0);

    document.closeDocument();
}

// Issue 4, guard: a link to an anchor in the first part of a paragraph that continues on the
// next page opens the page the paragraph starts on
void MarkdownTest::testIssue4LinkToAnchorOnFirstPageOfParagraph()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/anchors-long.md");
    QMimeDatabase db;
    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), db.mimeTypeForFile(testFile)), Okular::Document::OpenSuccess);

    const QStringList links = anchorsLongLinks();
    const QList<Okular::DocumentViewport> targets = linkTargets(document);
    QCOMPARE(targets.size(), links.size());
    const QStringList texts = pageTexts(document);

    int anchorsBeforePageEnd = 0;
    for (int i = 0; i < links.size(); ++i) {
        if (links[i].startsWith(QLatin1String("HEAD"))) {
            continue;
        }
        const int anchorPage = pageShowing(texts, links[i]);
        const int paragraphPage = pageShowing(texts, QStringLiteral("PARA") + links[i].right(2));
        const int paragraphEndPage = pageShowing(texts, QStringLiteral("LATE") + links[i].right(2));
        QVERIFY(anchorPage >= 0);
        QVERIFY(paragraphPage >= 0);
        QVERIFY(paragraphEndPage >= 0);
        if (anchorPage != paragraphPage || paragraphEndPage == paragraphPage) {
            continue;
        }
        ++anchorsBeforePageEnd;
        const QByteArray where = QStringLiteral("the link to %1 opens page %2, the anchor is on page %3").arg(links[i]).arg(targets[i].pageNumber).arg(anchorPage).toUtf8();
        QVERIFY2(targets[i].pageNumber == anchorPage, where.constData());
    }
    // two paragraphs are longer than a page, unless the font is very small
    QVERIFY(anchorsBeforePageEnd > 0);

    document.closeDocument();
}

// Issue 4, guard: a link to an anchor at the start of its block, here a heading, opens the
// line of the block
void MarkdownTest::testIssue4LinkToAnchorAtStartOfBlock()
{
    const QString testFile = QStringLiteral(KDESRCDIR "data/anchors-long.md");
    QMimeDatabase db;
    Okular::Document document(nullptr);
    QCOMPARE(document.openDocument(testFile, QUrl(), db.mimeTypeForFile(testFile)), Okular::Document::OpenSuccess);

    const QStringList links = anchorsLongLinks();
    const QList<Okular::DocumentViewport> targets = linkTargets(document);
    QCOMPARE(targets.size(), links.size());
    const QStringList texts = pageTexts(document);

    int headingsAfterFirstPage = 0;
    for (int i = 0; i < links.size(); ++i) {
        if (!links[i].startsWith(QLatin1String("HEAD"))) {
            continue;
        }
        const int anchorPage = pageShowing(texts, links[i]);
        QVERIFY(anchorPage >= 0);
        if (anchorPage > 0) {
            ++headingsAfterFirstPage;
        }
        const QByteArray wrong = notAtLineOf(document, targets[i], anchorPage, links[i]);
        QVERIFY2(wrong.isEmpty(), wrong.constData());
    }
    QVERIFY(headingsAfterFirstPage > 0);

    document.closeDocument();
}

QTEST_MAIN(MarkdownTest)
#include "markdowntest.moc"
