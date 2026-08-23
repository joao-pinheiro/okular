/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "searchresultsmodel.h"

#include <algorithm>

#include "core/page.h"
#include "core/textpage.h"

SearchResult SearchResultsModel::searchResultForMatch(const Okular::Page *page, int pageNumber, const Okular::NormalizedRect &matchBoundingRect, int maxSnippetLength)
{
    return searchResultForMatch(page->words(nullptr, Okular::TextPage::CentralPixelTextAreaInclusionBehaviour), pageNumber, matchBoundingRect, maxSnippetLength);
}

SearchResult SearchResultsModel::searchResultForMatch(const Okular::TextEntity::List &pageWords, int pageNumber, const Okular::NormalizedRect &matchBoundingRect, int maxSnippetLength)
{
    SearchResult result;
    result.pageNumber = pageNumber;
    result.boundingRect = matchBoundingRect;

    // The context shown is the whole line the match sits on: every word whose
    // centre falls in the match's vertical band. Filtering the caller's word
    // list keeps it to one page walk however many matches the page holds
    // (Page::words() re-transforms every entity on each call).
    const Okular::NormalizedRect lineArea(0.0, matchBoundingRect.top, 1.0, matchBoundingRect.bottom);

    // The match and "text before the match" areas both sit inside the line's vertical
    // band, so they are answered from the same pass.
    const Okular::NormalizedRect beforeMatchArea(0.0, matchBoundingRect.top, matchBoundingRect.left, matchBoundingRect.bottom);

    QString snippet;
    QString matchText;
    QString textBeforeMatch;
    for (const Okular::TextEntity &word : pageWords) {
        const Okular::NormalizedPoint centre = word.area().center();
        if (!lineArea.contains(centre.x, centre.y)) {
            continue;
        }
        snippet += word.text();
        if (matchBoundingRect.contains(centre.x, centre.y)) {
            matchText += word.text();
        }
        if (beforeMatchArea.contains(centre.x, centre.y)) {
            textBeforeMatch += word.text();
        }
    }
    result.snippet = snippet.simplified();
    matchText = matchText.simplified();

    if (!matchText.isEmpty()) {
        textBeforeMatch = textBeforeMatch.simplified();

        // The same word can occur several times on one line, so the match is not
        // necessarily the first one. Count the occurrences sitting to its left on the
        // line, and skip that many when locating it in the snippet.
        int occurrencesBefore = 0;
        for (int at = textBeforeMatch.indexOf(matchText); at >= 0; at = textBeforeMatch.indexOf(matchText, at + matchText.length())) {
            ++occurrencesBefore;
        }

        int offset = -1;
        int searchFrom = 0;
        for (int i = 0; i <= occurrencesBefore; ++i) {
            offset = result.snippet.indexOf(matchText, searchFrom);
            if (offset < 0) {
                break;
            }
            searchFrom = offset + matchText.length();
        }

        if (offset >= 0) {
            result.matchOffset = offset;
            result.matchLength = matchText.length();
        }
    }

    elideSnippetAroundMatch(result, maxSnippetLength);

    return result;
}

Okular::DocumentViewport SearchResultsModel::viewportForResult(const SearchResult &result)
{
    Okular::DocumentViewport viewport(result.pageNumber);
    viewport.rePos.enabled = true;
    viewport.rePos.pos = Okular::DocumentViewport::Center;
    const Okular::NormalizedPoint centre = result.boundingRect.center();
    viewport.rePos.normalizedX = centre.x;
    viewport.rePos.normalizedY = centre.y;
    return viewport;
}

void SearchResultsModel::elideSnippetAroundMatch(SearchResult &result, int maxLength)
{
    if (maxLength <= 0 || result.snippet.length() <= maxLength) {
        return;
    }

    const int matchCentre = result.matchOffset >= 0 ? result.matchOffset + result.matchLength / 2 : 0;
    const int start = qBound(0, matchCentre - maxLength / 2, result.snippet.length() - maxLength);

    QString elided = result.snippet.mid(start, maxLength);
    int offset = result.matchOffset >= 0 ? result.matchOffset - start : -1;

    if (start > 0) {
        elided.prepend(QChar(0x2026));
        if (offset >= 0) {
            ++offset;
        }
    }
    if (start + maxLength < result.snippet.length()) {
        elided.append(QChar(0x2026));
    }

    result.snippet = elided;
    result.matchOffset = offset;
}

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

SearchResultsModel::~SearchResultsModel()
{
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_results.count();
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.count()) {
        return QVariant();
    }

    const SearchResult &result = m_results.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case SnippetRole:
        return result.snippet;
    case PageNumberRole:
        return result.pageNumber;
    case MatchOffsetRole:
        return result.matchOffset;
    case MatchLengthRole:
        return result.matchLength;
    default:
        return QVariant();
    }
}

int SearchResultsModel::insertResults(const QList<SearchResult> &results)
{
    if (results.isEmpty()) {
        return -1;
    }

    // batches arrive in scan order, not page order: binary-search the insertion
    // point in the always-sorted m_results
    const int page = results.first().pageNumber;
    const auto pos = std::upper_bound(m_results.cbegin(), m_results.cend(), page, [](int page, const SearchResult &result) { return page < result.pageNumber; });
    const int at = static_cast<int>(pos - m_results.cbegin());

    beginInsertRows(QModelIndex(), at, at + results.count() - 1);
    if (at == m_results.count()) {
        m_results.append(results);
    } else {
        // make room for the whole batch at once, one shift of the tail
        m_results.insert(at, results.count(), SearchResult());
        std::copy(results.cbegin(), results.cend(), m_results.begin() + at);
    }
    endInsertRows();
    return at;
}

void SearchResultsModel::clear()
{
    if (m_results.isEmpty()) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_results.count() - 1);
    m_results.clear();
    endRemoveRows();
}

const SearchResult &SearchResultsModel::resultAt(int row) const
{
    return m_results.at(row);
}
