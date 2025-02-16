/*
    SPDX-FileCopyrightText: 2007 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "generator_p.h"

#include <QDebug>
#include <QFutureWatcher>

#include "fontinfo.h"
#include "utils.h"

using namespace Okular;

PixmapGeneratorBase::PixmapGeneratorBase(Generator *generator)
    : mGenerator(generator)
    , mRequest(nullptr)
    , mCalcBoundingBox(false)
{
}

void PixmapGeneratorBase::endGeneration()
{
    mRequest = nullptr;
}

PixmapRequest *PixmapGeneratorBase::request() const
{
    return mRequest;
}

QImage PixmapGeneratorBase::image() const
{
    return mRequest ? PixmapRequestPrivate::get(mRequest)->mResultImage : QImage();
}

bool PixmapGeneratorBase::calcBoundingBox() const
{
    return mCalcBoundingBox;
}

NormalizedRect PixmapGeneratorBase::boundingBox() const
{
    return mBoundingBox;
}

PixmapGeneratorThread::PixmapGeneratorThread(Generator *generator)
    : QThread(nullptr)
    , PixmapGeneratorBase(generator)
{
}

void PixmapGeneratorThread::startGeneration(PixmapRequest *request, bool calcBoundingBox)
{
    mRequest = request;
    mCalcBoundingBox = calcBoundingBox;

    start(QThread::InheritPriority);
}

bool PixmapGeneratorThread::wait()
{
    if (isRunning()) {
        QThread::wait();
    }
    return isFinished();
}

bool PixmapGeneratorThread::isRunning() const
{
    return QThread::isRunning();
}

bool PixmapGeneratorThread::isFinished() const
{
    return QThread::isFinished();
}

void PixmapGeneratorThread::run()
{
    if (mRequest) {
        PixmapRequestPrivate::get(mRequest)->mResultImage = mGenerator->image(mRequest);

        if (mCalcBoundingBox) {
            mBoundingBox = Utils::imageBoundingBox(&PixmapRequestPrivate::get(mRequest)->mResultImage);
        }
    }
}

PixmapGeneratorAsync::PixmapGeneratorAsync(Generator *generator)
    : QObject(nullptr)
    , PixmapGeneratorBase(generator)
{
    mWatcher = new QFutureWatcher<QImage>(this);
    connect(mWatcher, &QFutureWatcher<QImage>::finished, this, &PixmapGeneratorAsync::handleResult);
}

void PixmapGeneratorAsync::startGeneration(PixmapRequest *request, bool calcBoundingBox)
{
    mRequest = request;
    mCalcBoundingBox = calcBoundingBox;

    Q_EMIT started();

    const auto future = mGenerator->d_ptr->imageAsync(mRequest);

    mWatcher->setFuture(future);
}

bool PixmapGeneratorAsync::wait()
{
    if (mWatcher->isRunning()) {
        mWatcher->waitForFinished();
    }
    return mWatcher->isFinished();
}

bool PixmapGeneratorAsync::isRunning() const
{
    return mWatcher->isRunning();
}

bool PixmapGeneratorAsync::isFinished() const
{
    return mWatcher->isFinished();
}

void PixmapGeneratorAsync::handleResult()
{
    if (mRequest) {
        PixmapRequestPrivate::get(mRequest)->mResultImage = mWatcher->result();

        if (mCalcBoundingBox) {
            mBoundingBox = Utils::imageBoundingBox(&PixmapRequestPrivate::get(mRequest)->mResultImage);
        }

        Q_EMIT finished();
    }
}

TextPageGeneratorBase::TextPageGeneratorBase(Generator *generator)
    : mGenerator(generator)
    , mTextPage(nullptr)
{
}

void TextPageGeneratorBase::endGeneration() const
{
    TextRequestPrivate *treqPriv = TextRequestPrivate::get(&mTextRequest);
    treqPriv->mPage = nullptr;
    treqPriv->mShouldAbortExtraction = 0;
}

void TextPageGeneratorBase::abortExtraction() const
{
    if (!mTextPage) {
        TextRequestPrivate *treqPriv = TextRequestPrivate::get(&mTextRequest);
        treqPriv->mShouldAbortExtraction = 1;
    }
}

bool TextPageGeneratorBase::shouldAbortExtraction() const
{
    return mTextRequest.shouldAbortExtraction();
}

void TextPageGeneratorBase::setPage(Page *page) const
{
    TextRequestPrivate *treqPriv = TextRequestPrivate::get(&mTextRequest);
    treqPriv->mPage = page;
    treqPriv->mShouldAbortExtraction = 0;
}

Page *TextPageGeneratorBase::page() const
{
    return mTextRequest.page();
}

TextPage *TextPageGeneratorBase::textPage() const
{
    return mTextPage;
}

TextPageGeneratorThread::TextPageGeneratorThread(Generator *generator)
    : QThread(nullptr)
    , TextPageGeneratorBase(generator)
{
    TextRequestPrivate *treqPriv = TextRequestPrivate::get(&mTextRequest);
    treqPriv->mPage = nullptr;
    treqPriv->mShouldAbortExtraction = 0;
}

bool TextPageGeneratorThread::wait()
{
    if (isRunning()) {
        QThread::wait();
    }
    return isFinished();
}

bool TextPageGeneratorThread::isRunning() const
{
    return QThread::isRunning();
}

bool TextPageGeneratorThread::isFinished() const
{
    return QThread::isFinished();
}

void TextPageGeneratorThread::startGeneration()
{
    if (page()) {
        start(QThread::InheritPriority);
    }
}

void TextPageGeneratorThread::run()
{
    mTextPage = nullptr;

    Q_ASSERT(page());

    mTextPage = mGenerator->textPage(&mTextRequest);

    if (mTextRequest.shouldAbortExtraction()) {
        delete mTextPage;
        mTextPage = nullptr;
    }
}

TextPageGeneratorAsync::TextPageGeneratorAsync(Generator *generator)
    : QObject(nullptr)
    , TextPageGeneratorBase(generator)
{
    mWatcher = new QFutureWatcher<TextPage *>(this);
    connect(mWatcher, &QFutureWatcher<TextPage *>::finished, this, &TextPageGeneratorAsync::handleResult);

    TextRequestPrivate *treqPriv = TextRequestPrivate::get(&mTextRequest);
    treqPriv->mPage = nullptr;
    treqPriv->mShouldAbortExtraction = 0;
}

void TextPageGeneratorAsync::startGeneration()
{
    if (!page()) {
        return;
    }

    const auto future = mGenerator->d_ptr->textPageAsync(page()->number());

    mWatcher->setFuture(future);
}

bool TextPageGeneratorAsync::wait()
{
    if (mWatcher->isRunning()) {
        mWatcher->waitForFinished();
    }
    return mWatcher->isFinished();
}

bool TextPageGeneratorAsync::isRunning() const
{
    return mWatcher->isRunning();
}

bool TextPageGeneratorAsync::isFinished() const
{
    return mWatcher->isFinished();
}

void TextPageGeneratorAsync::handleResult()
{
    mTextPage = nullptr;

    Q_ASSERT(page());

    mTextPage = mWatcher->result();

    if (mTextRequest.shouldAbortExtraction()) {
        delete mTextPage;
        mTextPage = nullptr;
    }

    Q_EMIT finished();
}

FontExtractionThread::FontExtractionThread(Generator *generator, int pages)
    : mGenerator(generator)
    , mNumOfPages(pages)
{
}

void FontExtractionThread::startExtraction(bool async)
{
    if (async) {
        connect(this, &FontExtractionThread::finished, this, &FontExtractionThread::deleteLater);
        start(QThread::InheritPriority);
    } else {
        run();
        deleteLater();
    }
}

void FontExtractionThread::run()
{
    for (int i = -1; i < mNumOfPages && !isInterruptionRequested(); ++i) {
        const FontInfo::List list = mGenerator->fontsForPage(i);
        for (const FontInfo &fi : list) {
            Q_EMIT gotFont(fi);
        }
        Q_EMIT progress(i);
    }
}
