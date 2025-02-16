/*
    SPDX-FileCopyrightText: 2007 Tobias Koenig <tokoe@kde.org>

    Work sponsored by the LiMux project of the city of Munich:
    SPDX-FileCopyrightText: 2017 Klarälvdalens Datakonsult AB a KDAB Group company <info@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef OKULAR_THREADEDGENERATOR_P_H
#define OKULAR_THREADEDGENERATOR_P_H

#include "area.h"

#include <QFuture>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <QThread>

class QEventLoop;

#include "generator.h"
#include "page.h"

namespace Okular
{
class DocumentObserver;
class DocumentPrivate;
class FontInfo;
class Generator;
class Page;
class PixmapGeneratorBase;
class PixmapRequest;
class TextPage;
class TextPageGeneratorBase;
class TilesManager;

class GeneratorPrivate
{
public:
    GeneratorPrivate();

    virtual ~GeneratorPrivate();

    Q_DECLARE_PUBLIC(Generator)
    Generator *q_ptr;

    virtual PixmapGeneratorBase *pixmapGenerator();
    virtual TextPageGeneratorBase *textPageGenerator();

    void pixmapGenerationFinished();
    void textpageGenerationFinished();

    QMutex *threadsLock();

    virtual QVariant metaData(const QString &key, const QVariant &option) const;
    virtual QImage image(PixmapRequest *);

    virtual QFuture<QImage> imageAsync(PixmapRequest *request);
    virtual QFuture<TextPage *> textPageAsync(int pageNumber);

    DocumentPrivate *m_document;
    // NOTE: the following should be a QSet< GeneratorFeature >,
    // but it is not to avoid #include'ing generator.h
    QSet<int> m_features;
    std::unique_ptr<PixmapGeneratorBase> mPixmapGenerator;
    std::unique_ptr<TextPageGeneratorBase> mTextPageGenerator;
    mutable QMutex m_mutex;
    QMutex m_threadsMutex;
    bool mPixmapReady : 1;
    bool mTextPageReady : 1;
    bool m_closing : 1;
    QEventLoop *m_closingLoop;
    QSizeF m_dpi;
};

class PixmapRequestPrivate
{
public:
    void swap();
    TilesManager *tilesManager() const;

    static PixmapRequestPrivate *get(const PixmapRequest *req);

    DocumentObserver *mObserver;
    int mPageNumber;
    int mWidth;
    int mHeight;
    int mPriority;
    int mFeatures;
    bool mForce : 1;
    bool mTile : 1;
    bool mPartialUpdatesWanted : 1;
    Page *mPage;
    NormalizedRect mNormalizedRect;
    QAtomicInt mShouldAbortRender;
    QImage mResultImage;
};

class TextRequestPrivate
{
public:
    static TextRequestPrivate *get(const TextRequest *req);

    Page *mPage;
    QAtomicInt mShouldAbortExtraction;
};

class PixmapGeneratorBase
{
public:
    virtual ~PixmapGeneratorBase() = default;
    virtual void startGeneration(PixmapRequest *request, bool calcBoundingBox) = 0;
    void endGeneration();

    virtual bool wait() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isFinished() const = 0;

    PixmapRequest *request() const;

    QImage image() const;
    bool calcBoundingBox() const;
    NormalizedRect boundingBox() const;

protected:
    explicit PixmapGeneratorBase(Generator *generator);

    Generator *mGenerator;
    PixmapRequest *mRequest;
    NormalizedRect mBoundingBox;
    bool mCalcBoundingBox : 1;
};

class PixmapGeneratorThread : public QThread, public PixmapGeneratorBase
{
    Q_OBJECT

public:
    explicit PixmapGeneratorThread(Generator *generator);

    void startGeneration(PixmapRequest *request, bool calcBoundingBox) override;

    bool wait() override;
    bool isRunning() const override;
    bool isFinished() const override;

protected:
    void run() override;
};

class PixmapGeneratorAsync : public QObject, public PixmapGeneratorBase
{
    Q_OBJECT

public:
    explicit PixmapGeneratorAsync(Generator *generator);

    void startGeneration(PixmapRequest *request, bool calcBoundingBox) override;

    bool wait() override;
    bool isRunning() const override;
    bool isFinished() const override;

Q_SIGNALS:
    void started();
    void finished();

private Q_SLOTS:
    void handleResult();

private:
    QFutureWatcher<QImage> *mWatcher;
};

class TextPageGeneratorBase
{
public:
    virtual ~TextPageGeneratorBase() = default;
    virtual void startGeneration() = 0;
    void endGeneration() const;

    virtual bool wait() = 0;
    virtual bool isRunning() const = 0;
    virtual bool isFinished() const = 0;

    void abortExtraction() const;
    bool shouldAbortExtraction() const;

    void setPage(Page *page) const;
    Page *page() const;
    TextPage *textPage() const;

protected:
    explicit TextPageGeneratorBase(Generator *generator);

    Generator *mGenerator;
    TextPage *mTextPage;
    TextRequest mTextRequest;
};

class TextPageGeneratorThread : public QThread, public TextPageGeneratorBase
{
    Q_OBJECT

public:
    explicit TextPageGeneratorThread(Generator *generator);

    bool wait() override;
    bool isRunning() const override;
    bool isFinished() const override;

public Q_SLOTS:
    void startGeneration() override;

protected:
    void run() override;
};

class TextPageGeneratorAsync : public QObject, public TextPageGeneratorBase
{
    Q_OBJECT

public:
    explicit TextPageGeneratorAsync(Generator *generator);

    void abortExtraction();
    bool wait() override;
    bool isRunning() const override;
    bool isFinished() const override;

Q_SIGNALS:
    void finished();

public Q_SLOTS:
    void startGeneration() override;

private Q_SLOTS:
    void handleResult();

private:
    QFutureWatcher<TextPage *> *mWatcher;
};

class FontExtractionThread : public QThread
{
    Q_OBJECT

public:
    FontExtractionThread(Generator *generator, int pages);

    void startExtraction(bool async);

Q_SIGNALS:
    void gotFont(const Okular::FontInfo &);
    void progress(int page);

protected:
    void run() override;

private:
    Generator *mGenerator;
    int mNumOfPages;
};

}

Q_DECLARE_METATYPE(Okular::Page *)

#endif
