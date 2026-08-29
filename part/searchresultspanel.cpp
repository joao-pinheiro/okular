/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "searchresultspanel.h"

#include <KLocalizedString>

#include <QAbstractSlider>
#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

#include "core/page.h"
#include "searchlineedit.h"
#include "searchresultsmodel.h"
#include "settings.h"

// Highlights are composited onto the page with CompositionMode_Multiply, so laid over
// the shared SearchMatchColor this leaves the picked result reading as orange against
// the pale yellow of the others.
static const QColor CurrentMatchColor(255, 138, 0);

namespace
{

/**
 * Draws a result as two lines: its page number, then the line of text it was found
 * on with the match itself in bold.
 */
class SearchResultDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QRect contentRect = option.rect.adjusted(Margin, Margin, -Margin, -Margin);
        const bool selected = option.state & QStyle::State_Selected;
        const QPalette::ColorRole textRole = selected ? QPalette::HighlightedText : QPalette::Text;

        painter->save();

        const QFont pageFont = pageNumberFont(option.font);
        const QFontMetrics pageMetrics(pageFont);
        painter->setFont(pageFont);
        painter->setPen(option.palette.color(selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
        const int pageNumber = index.data(SearchResultsModel::PageNumberRole).toInt();
        painter->drawText(QRect(contentRect.left(), contentRect.top(), contentRect.width(), pageMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter, i18n("Page %1", pageNumber + 1));

        SearchResult result;
        result.snippet = index.data(SearchResultsModel::SnippetRole).toString();
        result.matchOffset = index.data(SearchResultsModel::MatchOffsetRole).toInt();
        result.matchLength = index.data(SearchResultsModel::MatchLengthRole).toInt();

        const QFontMetrics snippetMetrics(option.font);
        // re-elide for the actual width, so the match stays visible in a narrow sidebar
        const int charWidth = qMax(1, snippetMetrics.averageCharWidth());
        SearchResultsModel::elideSnippetAroundMatch(result, qMax(8, contentRect.width() / charWidth));

        painter->setPen(option.palette.color(textRole));
        const QRect snippetRect(contentRect.left(), contentRect.top() + pageMetrics.height(), contentRect.width(), snippetMetrics.height());
        drawSnippet(painter, snippetRect, option.font, result);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        return QSize(0, QFontMetrics(pageNumberFont(option.font)).height() + QFontMetrics(option.font).height() + 2 * Margin);
    }

private:
    static constexpr int Margin = 4;

    static QFont pageNumberFont(const QFont &font)
    {
        QFont pageFont = font;
        pageFont.setPointSizeF(pageFont.pointSizeF() * 0.85);
        return pageFont;
    }

    static void drawSnippet(QPainter *painter, const QRect &rect, const QFont &font, const SearchResult &result)
    {
        QTextLayout layout(result.snippet, font, painter->device());

        if (result.matchOffset >= 0 && result.matchLength > 0 && result.matchOffset + result.matchLength <= result.snippet.length()) {
            QTextLayout::FormatRange match;
            match.start = result.matchOffset;
            match.length = result.matchLength;
            match.format.setFontWeight(QFont::Bold);
            layout.setFormats({match});
        }

        layout.beginLayout();
        QTextLine line = layout.createLine();
        if (line.isValid()) {
            line.setLineWidth(rect.width());
        }
        layout.endLayout();

        if (line.isValid()) {
            layout.draw(painter, rect.topLeft());
        }
    }
};

} // namespace

SearchResultsPanel::SearchResultsPanel(Okular::Document *document, QWidget *parent)
    : QWidget(parent)
    , m_document(document)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_searchLine = new SearchLineEdit(this, document);
    m_searchLine->setPlaceholderText(i18n("Search the document…"));
    m_searchLine->setSearchId(SEARCH_PANEL_SEARCH_ID);
    m_searchLine->setSearchType(Okular::Document::AllDocument);
    m_searchLine->setSearchColor(SearchMatchColor);
    m_searchLine->setSearchMoveViewport(false);
    m_searchLine->setSearchCaseSensitivity(Okular::Settings::searchPanelCaseSensitive() ? Qt::CaseSensitive : Qt::CaseInsensitive);
    m_searchLine->setSearchFromStart(!Okular::Settings::searchPanelFromCurrentPage());
    m_searchLine->setFindAsYouType(Okular::Settings::searchPanelFindAsYouType());
    layout->addWidget(m_searchLine);

    QHBoxLayout *toolsLayout = new QHBoxLayout;
    layout->addLayout(toolsLayout);

    m_nextButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-down-search")), i18nc("Go to the next search result", "Next"), this);
    m_nextButton->setObjectName(QStringLiteral("searchResultsNextButton"));
    m_nextButton->setToolTip(i18n("Jump to next match"));
    m_nextButton->setEnabled(false);
    toolsLayout->addWidget(m_nextButton);

    m_previousButton = new QPushButton(QIcon::fromTheme(QStringLiteral("go-up-search")), i18nc("Go to the previous search result", "Previous"), this);
    m_previousButton->setObjectName(QStringLiteral("searchResultsPreviousButton"));
    m_previousButton->setToolTip(i18n("Jump to previous match"));
    m_previousButton->setEnabled(false);
    toolsLayout->addWidget(m_previousButton);

    QPushButton *optionsButton = new QPushButton(i18n("Options"), this);
    optionsButton->setToolTip(i18n("Modify search behavior"));
    QMenu *optionsMenu = new QMenu(optionsButton);
    m_caseSensitiveAction = optionsMenu->addAction(i18n("Case sensitive"));
    m_caseSensitiveAction->setObjectName(QStringLiteral("searchResultsCaseSensitiveAction"));
    m_caseSensitiveAction->setCheckable(true);
    m_caseSensitiveAction->setChecked(Okular::Settings::searchPanelCaseSensitive());
    m_fromCurrentPageAction = optionsMenu->addAction(i18n("From current page"));
    m_fromCurrentPageAction->setObjectName(QStringLiteral("searchResultsFromCurrentPageAction"));
    m_fromCurrentPageAction->setCheckable(true);
    m_fromCurrentPageAction->setChecked(Okular::Settings::searchPanelFromCurrentPage());
    m_searchAsYouTypeAction = optionsMenu->addAction(i18n("Search as you type"));
    m_searchAsYouTypeAction->setObjectName(QStringLiteral("searchResultsSearchAsYouTypeAction"));
    m_searchAsYouTypeAction->setCheckable(true);
    m_searchAsYouTypeAction->setChecked(Okular::Settings::searchPanelFindAsYouType());
    optionsButton->setMenu(optionsMenu);
    toolsLayout->addWidget(optionsButton);
    toolsLayout->addStretch();

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_model = new SearchResultsModel(this);
    m_view = new QListView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new SearchResultDelegate(m_view));
    m_view->setUniformItemSizes(true);
    // one scrollbar step is one row, which is what the anchoring below counts in
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_view);

    m_searchLine->setEnabled(false);

    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(0);

    m_statusRefreshTimer = new QTimer(this);
    m_statusRefreshTimer->setInterval(250);

    connect(m_searchLine, &SearchLineEdit::textChanged, this, &SearchResultsPanel::slotTextChanged);
    connect(m_searchLine, &SearchLineEdit::searchStarted, this, &SearchResultsPanel::slotSearchStarted);
    connect(m_searchLine, &SearchLineEdit::searchStopped, this, &SearchResultsPanel::slotSearchStopped);
    connect(m_searchLine, &SearchLineEdit::nextResultRequested, this, &SearchResultsPanel::slotNextResult);
    connect(m_searchLine, &SearchLineEdit::previousResultRequested, this, &SearchResultsPanel::slotPreviousResult);
    connect(m_caseSensitiveAction, &QAction::toggled, this, &SearchResultsPanel::slotCaseSensitivityChanged);
    connect(m_fromCurrentPageAction, &QAction::toggled, this, &SearchResultsPanel::slotFromCurrentPageChanged);
    connect(m_searchAsYouTypeAction, &QAction::toggled, this, &SearchResultsPanel::slotSearchAsYouTypeChanged);
    connect(m_nextButton, &QAbstractButton::clicked, this, &SearchResultsPanel::slotNextResult);
    connect(m_previousButton, &QAbstractButton::clicked, this, &SearchResultsPanel::slotPreviousResult);
    connect(m_view->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &SearchResultsPanel::slotScrollBarMoved);
    connect(m_model, &QAbstractItemModel::rowsAboutToBeInserted, this, &SearchResultsPanel::slotRowsAboutToBeInserted);
    connect(m_flushTimer, &QTimer::timeout, this, &SearchResultsPanel::slotFlushPendingResults);
    connect(m_statusRefreshTimer, &QTimer::timeout, this, &SearchResultsPanel::updateStatus);
    connect(m_view, &QAbstractItemView::activated, this, &SearchResultsPanel::slotResultActivated);
    connect(m_view, &QAbstractItemView::clicked, this, &SearchResultsPanel::slotResultActivated);
    connect(m_document, &Okular::Document::searchPageScanned, this, &SearchResultsPanel::slotPageScanned);

    updateStatus();
}

SearchResultsPanel::~SearchResultsPanel()
{
    m_document->removeObserver(this);
}

void SearchResultsPanel::notifySetup(const QList<Okular::Page *> &pages, int setupFlags)
{
    Q_UNUSED(pages);

    // a finished search calls notifySetup() too, with no flag set
    if (!(setupFlags & DocumentChanged)) {
        return;
    }

    m_searching = false;
    m_statusRefreshTimer->stop();
    clearResults();
    m_searchLine->clearText();
    m_searchLine->setEnabled(m_document->isOpened() && m_document->supportsSearching());
    updateStatus();
}

void SearchResultsPanel::slotTextChanged()
{
    // an emptied field drops the results at once, not after the input delay
    if (m_searchLine->text().isEmpty()) {
        clearResults();
        m_document->clearHighlightedAreas(SEARCH_PANEL_CURRENT_MATCH_ID);
        updateStatus();
    }
}

void SearchResultsPanel::slotSearchStarted()
{
    clearResults();
    m_document->clearHighlightedAreas(SEARCH_PANEL_CURRENT_MATCH_ID);
    m_selectFirstResult = true;
    m_readerScrolled = false;
    m_searching = true;
    m_statusRefreshTimer->start();
    updateStatus();
}

void SearchResultsPanel::slotSearchStopped()
{
    // the last pages report and finish in the same call: flush now
    m_flushTimer->stop();
    slotFlushPendingResults();

    m_searching = false;
    m_statusRefreshTimer->stop();
    updateStatus();
}

void SearchResultsPanel::slotPageScanned(int searchID, int pageNumber, const QList<Okular::SearchMatch> &matches)
{
    if (searchID != SEARCH_PANEL_SEARCH_ID) {
        return;
    }

    ++m_scannedPages;

    if (!matches.isEmpty()) {
        QList<SearchResult> results;
        results.reserve(matches.count());
        for (const Okular::SearchMatch &match : matches) {
            SearchResult result;
            result.pageNumber = pageNumber;
            result.boundingRect = match.boundingRect;
            result.snippet = match.lineText;
            result.matchOffset = match.offsetInLine;
            result.matchLength = match.length;
            SearchResultsModel::elideSnippetAroundMatch(result, SearchResultsModel::DefaultSnippetLength);
            results.append(result);
        }
        enqueueResults(results);
    }
}

void SearchResultsPanel::enqueueResults(const QList<SearchResult> &pageResults)
{
    if (pageResults.isEmpty()) {
        return;
    }

    m_pendingResults.append(pageResults);
    m_pendingCount += pageResults.count();
    if (!m_flushTimer->isActive()) {
        m_flushTimer->start();
    }
}

void SearchResultsPanel::slotFlushPendingResults()
{
    // insert everything queued and correct the scroll position in one pass, so
    // that no paint sees the list shifted
    if (m_pendingResults.isEmpty()) {
        return;
    }
    const QList<QList<SearchResult>> batches = std::exchange(m_pendingResults, {});
    m_pendingCount = 0;

    for (const QList<SearchResult> &batch : batches) {
        const int insertedAt = m_model->insertResults(batch);
        if (m_selectFirstResult && insertedAt >= 0) {
            // select and mark, without moving the document view
            m_view->setCurrentIndex(m_model->index(insertedAt, 0));
            markCurrentResult(m_model->resultAt(insertedAt));
            m_selectFirstResult = false;
        }
    }

    restoreScrollPosition();
    updateStatus();
}

void SearchResultsPanel::slotResultActivated(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    const SearchResult &result = m_model->resultAt(index.row());
    markCurrentResult(result);
    m_document->setViewport(SearchResultsModel::viewportForResult(result), nullptr, true);
}

void SearchResultsPanel::markCurrentResult(const SearchResult &result)
{
    m_document->setHighlightedArea(SEARCH_PANEL_CURRENT_MATCH_ID, result.pageNumber, result.boundingRect, CurrentMatchColor);
}

void SearchResultsPanel::goToResult(int row)
{
    const int count = m_model->rowCount();
    if (count == 0) {
        return;
    }
    row = ((row % count) + count) % count; // wrap around the results collected so far

    const QModelIndex index = m_model->index(row, 0);
    m_adjustingScroll = true;
    m_view->setCurrentIndex(index);
    m_view->scrollTo(index, QAbstractItemView::EnsureVisible);
    m_adjustingScroll = false;

    // the anchor is stale after this scroll: let the next flush recapture it
    m_scrollAnchor = QPersistentModelIndex();

    slotResultActivated(index);
}

void SearchResultsPanel::slotNextResult()
{
    const QModelIndex current = m_view->currentIndex();
    goToResult(current.isValid() ? current.row() + 1 : 0);
}

void SearchResultsPanel::slotPreviousResult()
{
    const QModelIndex current = m_view->currentIndex();
    goToResult(current.isValid() ? current.row() - 1 : m_model->rowCount() - 1);
}

void SearchResultsPanel::slotRowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    Q_UNUSED(first);
    Q_UNUSED(last);

    // one flush inserts several batches: keep the anchor captured before the first
    if (m_scrollAnchor.isValid()) {
        return;
    }

    // a persistent index follows its row across insertions; a row count could
    // not while the list still fits the viewport and the scrollbar has no range
    m_scrollAnchor = m_model->index(m_view->verticalScrollBar()->value(), 0);
}

void SearchResultsPanel::slotScrollBarMoved()
{
    if (m_adjustingScroll) {
        return;
    }

    m_readerScrolled = true;
    m_scrollAnchor = m_model->index(m_view->verticalScrollBar()->value(), 0);
}

void SearchResultsPanel::restoreScrollPosition()
{
    // keep the selected result on screen until the reader scrolls; then only hold position
    m_adjustingScroll = true;

    const QModelIndex current = m_view->currentIndex();
    if (!m_readerScrolled && current.isValid()) {
        m_view->scrollTo(current, QAbstractItemView::EnsureVisible);
    } else if (m_scrollAnchor.isValid()) {
        // the scrollbar range is stale right after an insertion: lay out first, or setValue() clamps
        m_view->doItemsLayout();
        m_view->verticalScrollBar()->setValue(m_scrollAnchor.row());
    }

    m_adjustingScroll = false;
    m_scrollAnchor = QPersistentModelIndex();
}

void SearchResultsPanel::slotCaseSensitivityChanged()
{
    const bool caseSensitive = m_caseSensitiveAction->isChecked();
    Okular::Settings::setSearchPanelCaseSensitive(caseSensitive);
    Okular::Settings::self()->save();
    m_searchLine->setSearchCaseSensitivity(caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    m_searchLine->restartSearch();
}

void SearchResultsPanel::slotFromCurrentPageChanged()
{
    Okular::Settings::setSearchPanelFromCurrentPage(m_fromCurrentPageAction->isChecked());
    Okular::Settings::self()->save();
    m_searchLine->setSearchFromStart(!m_fromCurrentPageAction->isChecked());
}

void SearchResultsPanel::slotSearchAsYouTypeChanged()
{
    Okular::Settings::setSearchPanelFindAsYouType(m_searchAsYouTypeAction->isChecked());
    Okular::Settings::self()->save();
    m_searchLine->setFindAsYouType(m_searchAsYouTypeAction->isChecked());
}

void SearchResultsPanel::clearResults()
{
    m_model->clear();
    m_flushTimer->stop();
    m_pendingResults.clear();
    m_pendingCount = 0;
    m_scrollAnchor = QPersistentModelIndex();
    m_scannedPages = 0;
    m_selectFirstResult = false;
}

void SearchResultsPanel::updateStatus()
{
    const bool hasResults = m_model->rowCount() > 0;
    m_nextButton->setEnabled(hasResults);
    m_previousButton->setEnabled(hasResults);

    if (!m_document->isOpened()) {
        m_status->clear();
        return;
    }

    if (!m_document->supportsSearching()) {
        m_status->setText(i18n("This document cannot be searched."));
        return;
    }

    // include matches still queued for the next flush, so the count never dips
    const int count = m_model->rowCount() + m_pendingCount;

    if (m_searching) {
        m_status->setText(i18ncp("%1 is a number of matches, %2 and %3 are page counts", "%1 match so far, %2 of %3 pages scanned…", "%1 matches so far, %2 of %3 pages scanned…", count, m_scannedPages, int(m_document->pages())));
        return;
    }

    if (m_searchLine->text().isEmpty()) {
        m_status->clear();
        return;
    }

    m_status->setText(i18ncp("%1 is a number of matches", "%1 match", "%1 matches", count));
}
