#include "pageviewocr.h"

#include "settings.h"

#include <QImage>

#include <tesseract/baseapi.h>

#include <limits>

namespace
{
constexpr int kOcrUpscaleFactor = 3;
constexpr qint64 kMaxOcrPixelCount = 30000000;
constexpr const char *kDefaultTesseractLanguage = "eng";

enum class ImagePreparationStatus {
    Success,
    EmptyImage,
    ImageTooLarge,
    PreparationFailed,
};

struct ImagePreparationResult {
    ImagePreparationStatus status = ImagePreparationStatus::PreparationFailed;
    QImage image;
};

int getOtsuThreshold(const QImage &image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return 0;
    }

    int histogram[256] = {0};
    const qint64 totalPixelCount = static_cast<qint64>(image.width()) * image.height();

    for (int y = 0; y < image.height(); ++y) {
        const uchar *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            histogram[line[x]]++;
        }
    }

    double sum = 0.0;
    for (int i = 0; i < 256; ++i) {
        sum += static_cast<double>(i * histogram[i]);
    }

    double sumB = 0.0;
    qint64 wB = 0;
    qint64 wF = 0;

    double varMax = 0.0;
    int threshold = 0;

    for (int t = 0; t < 256; ++t) {
        wB += histogram[t];
        if (wB == 0) {
            continue;
        }

        wF = totalPixelCount - wB;
        if (wF == 0) {
            break;
        }

        sumB += static_cast<double>(t * histogram[t]);

        const double mB = sumB / static_cast<double>(wB);
        const double mF = (sum - sumB) / static_cast<double>(wF);
        const double meanDelta = mB - mF;
        const double varBetween = static_cast<double>(wB) * static_cast<double>(wF) * meanDelta * meanDelta;

        if (varBetween > varMax) {
            varMax = varBetween;
            threshold = t;
        }
    }

    return threshold;
}

QImage applyOtsuBinarization(const QImage &input)
{
    if (input.isNull()) {
        return {};
    }

    QImage gray = input.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return {};
    }

    const int threshold = getOtsuThreshold(gray);

    for (int y = 0; y < gray.height(); ++y) {
        uchar *line = gray.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            line[x] = (line[x] > threshold) ? 255 : 0;
        }
    }

    return gray;
}

ImagePreparationResult prepareImageForTask(const QImage &input)
{
    ImagePreparationResult result;

    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        result.status = ImagePreparationStatus::EmptyImage;
        return result;
    }

    const qint64 scaledWidth = static_cast<qint64>(input.width()) * kOcrUpscaleFactor;
    const qint64 scaledHeight = static_cast<qint64>(input.height()) * kOcrUpscaleFactor;

    if (scaledWidth > std::numeric_limits<int>::max() || scaledHeight > std::numeric_limits<int>::max()) {
        result.status = ImagePreparationStatus::ImageTooLarge;
        return result;
    }

    if (scaledWidth * scaledHeight > kMaxOcrPixelCount) {
        result.status = ImagePreparationStatus::ImageTooLarge;
        return result;
    }

    QImage image = input.scaled(static_cast<int>(scaledWidth), static_cast<int>(scaledHeight), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.isNull()) {
        result.status = ImagePreparationStatus::PreparationFailed;
        return result;
    }

    image = applyOtsuBinarization(image);
    if (image.isNull()) {
        result.status = ImagePreparationStatus::PreparationFailed;
        return result;
    }

    result.status = ImagePreparationStatus::Success;
    result.image = image;
    return result;
}
}

namespace Okular::PageViewOcr
{
QString languageSpecFromSettings()
{
    QString languages = Okular::Settings::ocrLanguages().trimmed();
    if (languages.isEmpty()) {
        return QString::fromLatin1(kDefaultTesseractLanguage);
    }

    languages = languages.simplified();
    languages.replace(QLatin1Char(','), QLatin1Char('+'));
    languages.replace(QLatin1Char(';'), QLatin1Char('+'));
    languages.replace(QLatin1Char(' '), QLatin1Char('+'));

    while (languages.contains(QStringLiteral("++"))) {
        languages.replace(QStringLiteral("++"), QStringLiteral("+"));
    }

    while (languages.startsWith(QLatin1Char('+'))) {
        languages.remove(0, 1);
    }

    while (languages.endsWith(QLatin1Char('+'))) {
        languages.chop(1);
    }

    if (languages.isEmpty()) {
        return QString::fromLatin1(kDefaultTesseractLanguage);
    }

    return languages;
}

TaskResult runTask(const QImage &sourceImage, const QString &ocrLanguages)
{
    TaskResult result;
    result.languages = ocrLanguages;

    const ImagePreparationResult preparation = prepareImageForTask(sourceImage);
    switch (preparation.status) {
    case ImagePreparationStatus::Success:
        break;
    case ImagePreparationStatus::EmptyImage:
        result.status = TaskStatus::EmptyImage;
        return result;
    case ImagePreparationStatus::ImageTooLarge:
        result.status = TaskStatus::ImageTooLarge;
        return result;
    case ImagePreparationStatus::PreparationFailed:
        result.status = TaskStatus::PreparationFailed;
        return result;
    }

    tesseract::TessBaseAPI api;
    const QByteArray ocrLanguagesUtf8 = ocrLanguages.toUtf8();

    if (api.Init(nullptr, ocrLanguagesUtf8.constData())) {
        result.status = TaskStatus::InitFailed;
        result.dataPath = QString::fromUtf8(api.GetDatapath() ? api.GetDatapath() : "");
        return result;
    }

    const QImage &image = preparation.image;
    api.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
    api.SetImage(image.bits(), image.width(), image.height(), 1, image.bytesPerLine());

    char *outText = api.GetUTF8Text();
    if (!outText) {
        result.status = TaskStatus::ExtractionFailed;
        return result;
    }

    result.text = QString::fromUtf8(outText).trimmed();
    delete[] outText;

    if (result.text.isEmpty()) {
        result.status = TaskStatus::NoText;
        return result;
    }

    result.status = TaskStatus::Success;
    return result;
}
}