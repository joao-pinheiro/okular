/*
    SPDX-FileCopyrightText: 2026 Pierre Bourré <bourre.pi@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "textpagejob_p.h"

#include "area.h"
#include "generator.h"
#include "page.h"
#include "textpage.h"
#include "textpage_p.h"

using namespace Okular;

TextPageJob::TextPageJob(Generator *generator, const Page *page, int pageNumber, int searchID, int generation, double pageWidth, double pageHeight, const QString &searchText, Qt::CaseSensitivity caseSensitivity)
    : m_generator(generator)
    , m_page(page)
    , m_pageNumber(pageNumber)
    , m_searchID(searchID)
    , m_generation(generation)
    , m_pageWidth(pageWidth)
    , m_pageHeight(pageHeight)
    , m_searchText(searchText)
    , m_caseSensitivity(caseSensitivity)
{
    setAutoDelete(false);
}

TextPageJob::~TextPageJob()
{
    for (const TextMatch &match : std::as_const(m_matches)) {
        delete match.area;
    }
    delete m_result;
}

void TextPageJob::run()
{
    TextRequest request(const_cast<Page *>(m_page));
    m_result = m_generator->textPage(&request);
    if (m_result) {
        m_result->prepareForPage(m_page, m_pageWidth, m_pageHeight);

        // search here too, off the main thread; the results stay unrotated (no
        // page geometry is read here) and the caller applies the rotation
        m_result->d->m_matrixOverride = QTransform();
        m_matches = m_result->findAllWithAreas(m_searchID, m_searchText, m_caseSensitivity);
        m_result->d->m_matrixOverride.reset();
    }
}

QList<TextMatch> TextPageJob::takeMatches()
{
    return std::exchange(m_matches, {});
}

int TextPageJob::pageNumber() const
{
    return m_pageNumber;
}

int TextPageJob::searchID() const
{
    return m_searchID;
}

int TextPageJob::generation() const
{
    return m_generation;
}

TextPage *TextPageJob::takeResult()
{
    TextPage *result = m_result;
    m_result = nullptr;
    return result;
}
