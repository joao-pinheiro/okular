/*
    SPDX-FileCopyrightText: 2026 JPinheiro <jpinheiro@blackshield.pt>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "core/textdocumentgenerator.h"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>

#include <cmath>
#include <memory>

class ViewportConverter : public Okular::TextDocumentConverter
{
    Q_OBJECT

public:
    using Okular::TextDocumentConverter::calculateViewport;
};

class TextDocumentViewportTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBlockOnLaterPage();
    void testBlockOverflowingPage();

private:
    std::unique_ptr<QTextDocument> createDocument();
};

// Many headings, each followed by a paragraph long enough to wrap over
// several lines, so that the document spans many pages and some paragraphs
// cross a page boundary.
std::unique_ptr<QTextDocument> TextDocumentViewportTest::createDocument()
{
    QString html;
    for (int i = 0; i < 60; ++i) {
        html += QStringLiteral("<h2>Section %1</h2>\n<p>").arg(i);
        for (int j = 0; j < 160; ++j) {
            html += QStringLiteral("word%1 ").arg(j);
        }
        html += QStringLiteral("</p>\n");
    }
    html += QStringLiteral("<h2>Last</h2>");

    auto document = std::make_unique<QTextDocument>();
    document->setPageSize(QSizeF(980, 1307));
    document->setHtml(html);

    QTextFrameFormat frameFormat;
    frameFormat.setMargin(45);
    document->rootFrame()->setFrameFormat(frameFormat);

    return document;
}

void TextDocumentViewportTest::testBlockOnLaterPage()
{
    const std::unique_ptr<QTextDocument> document = createDocument();
    const qreal pageHeight = document->pageSize().height();
    QVERIFY(document->pageCount() >= 3);

    ViewportConverter converter;
    int blocksAfterFirstPage = 0;
    int lastBlockPage = -1;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QRectF rect = document->documentLayout()->blockBoundingRect(block);
        const int expectedPage = int(std::floor(rect.y() / pageHeight));
        const qreal yInPage = rect.y() - expectedPage * pageHeight;
        if (yInPage + rect.height() > pageHeight) {
            // blocks that do not fit in their page are covered by testBlockOverflowingPage
            continue;
        }

        const Okular::DocumentViewport viewport = converter.calculateViewport(document.get(), block);
        QCOMPARE(viewport.pageNumber, expectedPage);
        QVERIFY(viewport.pageNumber < document->pageCount());
        QVERIFY(viewport.rePos.enabled);
        QVERIFY(qAbs(viewport.rePos.normalizedY * pageHeight - yInPage) < 1.0);

        if (expectedPage > 0) {
            ++blocksAfterFirstPage;
        }
        lastBlockPage = viewport.pageNumber;
    }
    QVERIFY(blocksAfterFirstPage > 0);
    // the document ends with a one line heading, so it has to be on the last page
    QCOMPARE(lastBlockPage, document->pageCount() - 1);
}

void TextDocumentViewportTest::testBlockOverflowingPage()
{
    const std::unique_ptr<QTextDocument> document = createDocument();
    const qreal pageHeight = document->pageSize().height();

    ViewportConverter converter;
    int overflowingBlocks = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QRectF rect = document->documentLayout()->blockBoundingRect(block);
        const int startPage = int(std::floor(rect.y() / pageHeight));
        const qreal yInPage = rect.y() - startPage * pageHeight;
        if (yInPage + rect.height() <= pageHeight) {
            continue;
        }
        ++overflowingBlocks;

        // a block that spills over the end of its page is addressed as the top of the next page
        const Okular::DocumentViewport viewport = converter.calculateViewport(document.get(), block);
        QCOMPARE(viewport.pageNumber, startPage + 1);
        QVERIFY(viewport.pageNumber < document->pageCount());
        QCOMPARE(viewport.rePos.normalizedY, 0.0);
    }
    QVERIFY(overflowingBlocks > 0);
}

QTEST_MAIN(TextDocumentViewportTest)
#include "textdocumentviewporttest.moc"
