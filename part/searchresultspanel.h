/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _SEARCHRESULTSPANEL_H_
#define _SEARCHRESULTSPANEL_H_

#include <QPersistentModelIndex>
#include <QWidget>

#include "okularpart_export.h"

#include "core/document.h"
#include "core/observer.h"
#include "searchresultsmodel.h"

class QAction;
class QLabel;
class QListView;
class QModelIndex;
class QPushButton;
class QTimer;
class SearchLineEdit;

/**
 * Sidebar panel listing every match of a search across the whole document, with the
 * line of text each match sits on and its page number. Results appear page by page
 * while the scan is still running, and clicking one brings the view to it.
 */
class OKULARPART_EXPORT SearchResultsPanel : public QWidget, public Okular::DocumentObserver
{
    Q_OBJECT

public:
    explicit SearchResultsPanel(Okular::Document *document, QWidget *parent = nullptr);
    ~SearchResultsPanel() override;

    // inherited from DocumentObserver
    void notifySetup(const QList<Okular::Page *> &pages, int setupFlags) override;

    /** Public so that tests can feed more results than a test document could produce. */
    void enqueueResults(const QList<SearchResult> &pageResults);

private Q_SLOTS:
    void slotTextChanged();
    void slotSearchStarted();
    void slotSearchStopped();
    void slotPageScanned(int searchID, int pageNumber, const QList<Okular::SearchMatch> &matches);
    void slotResultActivated(const QModelIndex &index);
    void slotCaseSensitivityChanged();
    void slotFromCurrentPageChanged();
    void slotSearchAsYouTypeChanged();
    void slotNextResult();
    void slotPreviousResult();
    void slotRowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void slotScrollBarMoved();
    void slotFlushPendingResults();

private:
    void updateStatus();
    void restoreScrollPosition();
    void clearResults();
    // Selects the result at @p row, wrapping around, and activates it.
    void goToResult(int row);
    // Highlights @p result with the current-match colour, without moving the view.
    void markCurrentResult(const SearchResult &result);

    Okular::Document *m_document;
    SearchLineEdit *m_searchLine;
    QPushButton *m_nextButton;
    QPushButton *m_previousButton;
    QAction *m_caseSensitiveAction;
    QAction *m_fromCurrentPageAction;
    QAction *m_searchAsYouTypeAction;
    QLabel *m_status;
    QListView *m_view;
    SearchResultsModel *m_model;
    QList<QList<SearchResult>> m_pendingResults; // one entry per page, waiting for the next flush
    int m_pendingCount = 0;
    QTimer *m_flushTimer;
    QTimer *m_statusRefreshTimer;
    QPersistentModelIndex m_scrollAnchor; // the row on top, kept across insertions above it
    bool m_readerScrolled = false;        // the reader moved the scrollbar since the search started
    bool m_adjustingScroll = false;       // the panel is moving the scrollbar itself

    bool m_searching = false;
    int m_scannedPages = 0;
    bool m_selectFirstResult = false;
};

#endif
