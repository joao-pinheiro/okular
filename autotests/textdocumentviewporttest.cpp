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
#include <QTextLayout>

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
    void testViewportPointsAtFirstLine();
    void testViewportPointsAtLineOfPosition();

private:
    std::unique_ptr<QTextDocument> createDocument();
};

// Many one line headings, each followed by a paragraph of a different length, so that the
// document spans many pages and blocks end up at every distance from the end of a page:
// some paragraphs are split over two pages and some blocks do not fit in what is left of
// their page at all.
std::unique_ptr<QTextDocument> TextDocumentViewportTest::createDocument()
{
    QString html;
    for (int i = 0; i < 240; ++i) {
        html += QStringLiteral("<h2>Section %1</h2>\n<p>").arg(i);
        const int words = 10 + (i * 37) % 90;
        for (int j = 0; j < words; ++j) {
            html += QStringLiteral("word%1 ").arg(j);
        }
        html += QStringLiteral("</p>\n");
    }

    auto document = std::make_unique<QTextDocument>();
    document->setPageSize(QSizeF(980, 1307));
    document->setHtml(html);

    QTextFrameFormat frameFormat;
    frameFormat.setMargin(45);
    document->rootFrame()->setFrameFormat(frameFormat);

    return document;
}

void TextDocumentViewportTest::testViewportPointsAtFirstLine()
{
    const std::unique_ptr<QTextDocument> document = createDocument();
    const QAbstractTextDocumentLayout *layout = document->documentLayout();
    const qreal pageHeight = document->pageSize().height();
    QVERIFY(document->pageCount() >= 3);

    ViewportConverter converter;
    int blocksAfterFirstPage = 0;
    int blocksMovedToNextPage = 0;
    int blocksSplitOverPages = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QRectF rect = layout->blockBoundingRect(block);
        const QTextLayout *blockLayout = block.layout();
        QVERIFY(blockLayout->lineCount() > 0);
        const QTextLine firstLine = blockLayout->lineAt(0);
        const QTextLine lastLine = blockLayout->lineAt(blockLayout->lineCount() - 1);

        const Okular::DocumentViewport viewport = converter.calculateViewport(document.get(), block);
        QVERIFY(viewport.pageNumber >= 0);
        QVERIFY(viewport.pageNumber < document->pageCount());
        QVERIFY(viewport.rePos.enabled);
        QVERIFY(viewport.rePos.normalizedY >= 0.0);
        QVERIFY(viewport.rePos.normalizedY < 1.0);

        // ask the layout what is at the place the viewport names (a bit inside the first line),
        // it has to be the first line of this block
        const QPointF target(rect.x() + 1, (viewport.pageNumber + viewport.rePos.normalizedY) * pageHeight + firstLine.height() / 2);
        const int hit = layout->hitTest(target, Qt::ExactHit);
        const QByteArray where = QStringLiteral("block \"%1\" page %2 y %3").arg(block.text().left(20)).arg(viewport.pageNumber).arg(viewport.rePos.normalizedY).toUtf8();
        QVERIFY2(hit >= block.position() && hit < block.position() + block.length(), where.constData());
        QVERIFY2(blockLayout->lineForTextPosition(hit - block.position()).lineNumber() == 0, where.constData());

        // which of the interesting cases the document contains
        if (viewport.pageNumber > 0) {
            ++blocksAfterFirstPage;
        }
        if (firstLine.y() > 0) {
            ++blocksMovedToNextPage;
        }
        if (int((rect.y() + lastLine.y()) / pageHeight) > int((rect.y() + firstLine.y()) / pageHeight)) {
            ++blocksSplitOverPages;
        }
    }
    QVERIFY(blocksAfterFirstPage > 0);
    QVERIFY(blocksMovedToNextPage > 0);
    QVERIFY(blocksSplitOverPages > 0);
}

void TextDocumentViewportTest::testViewportPointsAtLineOfPosition()
{
    const std::unique_ptr<QTextDocument> document = createDocument();
    const QAbstractTextDocumentLayout *layout = document->documentLayout();
    const qreal pageHeight = document->pageSize().height();
    QVERIFY(document->pageCount() >= 3);

    ViewportConverter converter;
    int linesAfterPageOfFirstLine = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QRectF rect = layout->blockBoundingRect(block);
        const QTextLayout *blockLayout = block.layout();
        QVERIFY(blockLayout->lineCount() > 0);
        const int pageOfFirstLine = int((rect.y() + blockLayout->lineAt(0).y()) / pageHeight);

        for (int i = 0; i < blockLayout->lineCount(); ++i) {
            const QTextLine line = blockLayout->lineAt(i);
            // the first and the last position of the line; for the last line that is the end of the block
            for (const int positionInBlock : {line.textStart(), line.textStart() + qMax(line.textLength() - 1, 0), i == blockLayout->lineCount() - 1 ? block.length() - 1 : line.textStart()}) {
                const Okular::DocumentViewport viewport = converter.calculateViewport(document.get(), block.position() + positionInBlock);
                QVERIFY(viewport.pageNumber >= 0);
                QVERIFY(viewport.pageNumber < document->pageCount());
                QVERIFY(viewport.rePos.enabled);
                QVERIFY(viewport.rePos.normalizedY >= 0.0);
                QVERIFY(viewport.rePos.normalizedY < 1.0);

                // ask the layout what is at the place the viewport names (a bit inside the line),
                // it has to be this line of this block
                const QPointF target(rect.x() + 1, (viewport.pageNumber + viewport.rePos.normalizedY) * pageHeight + line.height() / 2);
                const int hit = layout->hitTest(target, Qt::ExactHit);
                const QByteArray where = QStringLiteral("block \"%1\" line %2 position %3 page %4 y %5").arg(block.text().left(20)).arg(i).arg(positionInBlock).arg(viewport.pageNumber).arg(viewport.rePos.normalizedY).toUtf8();
                QVERIFY2(hit >= block.position() && hit < block.position() + block.length(), where.constData());
                QVERIFY2(blockLayout->lineForTextPosition(hit - block.position()).lineNumber() == i, where.constData());

                if (viewport.pageNumber > pageOfFirstLine) {
                    ++linesAfterPageOfFirstLine;
                }
            }
        }
    }
    // some paragraphs are split over two pages
    QVERIFY(linesAfterPageOfFirstLine > 0);
}

QTEST_MAIN(TextDocumentViewportTest)
#include "textdocumentviewporttest.moc"
