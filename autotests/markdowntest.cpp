/*
    SPDX-FileCopyrightText: 2020 Markus Brenneis <support.gulp21+kde@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "../settings_core.h"
#include "core/document.h"
#include "generators/markdown/converter.h"
#include <QMimeDatabase>
#include <QMimeType>
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

private:
    void findImages(QTextFrame *parent, QList<QTextImageFormat> &images);
    void findImages(const QTextBlock &parent, QList<QTextImageFormat> &images);
};

void MarkdownTest::initTestCase()
{
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

    QCOMPARE(titles.size(), expected.size());
    for (int i = 0; i < expected.size(); ++i) {
        QCOMPARE(titles[i].level, expected[i].first);
        QCOMPARE(titles[i].title, expected[i].second);
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
        QVERIFY(element.viewPort().has_value());
        QCOMPARE(element.viewPort()->pageNumber, 0);
        QVERIFY(element.viewPort()->rePos.normalizedY > previousY);
        previousY = element.viewPort()->rePos.normalizedY;
    }

    document.closeDocument();
}

QTEST_MAIN(MarkdownTest)
#include "markdowntest.moc"
