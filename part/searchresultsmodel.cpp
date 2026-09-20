/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "searchresultsmodel.h"

#include <algorithm>

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
