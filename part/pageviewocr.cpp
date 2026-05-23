#include "pageviewocr.h"

#include <KLocalizedString>

#include <QDir>
#include <QImage>
#include <QLocale>
#include <QThread>

#include <tesseract/baseapi.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace
{
// Screen DPI is typically ~96; Tesseract is optimised for ~300 DPI. A 3× upscale
// bridges most of that gap without being excessively slow on large selections.
constexpr int kOcrUpscaleFactor = 3;
// At 3× upscale a 30 MP cap corresponds to about a 3.3 MP source selection.
// This keeps peak memory of the temporary grayscale image under ~30 MB.
constexpr qint64 kMaxOcrPixelCount = 30'000'000;
// Tesseract language code used when no system language data can be found.
constexpr QLatin1StringView kFallbackTesseractLanguage("eng");

struct ImagePreparationResult {
    Okular::PageViewOcr::TaskStatus status = Okular::PageViewOcr::TaskStatus::PreparationFailed;
    QImage image;
};

QStringList normalizeLanguageList(const QStringList &languages)
{
    QStringList normalized;
    normalized.reserve(languages.size());

    for (const QString &language : languages) {
        const QString normalizedLanguage = language.trimmed().toLower();
        if (!normalizedLanguage.isEmpty() && !normalized.contains(normalizedLanguage)) {
            normalized.append(normalizedLanguage);
        }
    }

    return normalized;
}

QString languageSpecFromList(const QStringList &languageList)
{
    return languageList.join(QLatin1Char('+'));
}

QStringList effectiveLanguageList(const QStringList &configuredLanguages)
{
    QStringList languages = normalizeLanguageList(configuredLanguages);
    if (!languages.isEmpty()) {
        return languages;
    }

    const QStringList available = Okular::PageViewOcr::availableLanguageCodes();
    const QString systemDefault = Okular::PageViewOcr::defaultLanguageCode();

    if (available.contains(systemDefault)) {
        languages.append(systemDefault);
    } else if (available.contains(kFallbackTesseractLanguage)) {
        languages.append(kFallbackTesseractLanguage);
    } else if (!available.isEmpty()) {
        languages.append(available.constFirst());
    } else {
        languages.append(systemDefault);
    }

    return languages;
}

QString displayLanguageList(const QString &languageSpec)
{
    const QStringList languages = languageSpec.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (languages.isEmpty()) {
        return languageSpec;
    }

    return QLocale().createSeparatedList(languages);
}

bool initTesseractWithLanguages(tesseract::TessBaseAPI &api, const QString &languageSpec)
{
    const QByteArray languagesUtf8 = languageSpec.toUtf8();
    return api.Init(nullptr, languagesUtf8.constData()) == 0;
}

// Otsu's thresholding method:
// N. Otsu, "A Threshold Selection Method from Gray-Level Histograms,"
// IEEE Transactions on Systems, Man, and Cybernetics, 1979.
// DOI: 10.1109/TSMC.1979.4310076
int getOtsuThreshold(const QImage &image)
{
    if (image.isNull()) {
        return 0;
    }

    // Raw scanline access below assumes one byte per pixel grayscale input.
    if (image.format() != QImage::Format_Grayscale8) {
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
        sum += static_cast<double>(i) * histogram[i];
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

        sumB += static_cast<double>(t) * histogram[t];

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

    if (input.isNull()) {
        result.status = Okular::PageViewOcr::TaskStatus::EmptyImage;
        return result;
    }

    const qint64 scaledWidth = static_cast<qint64>(input.width()) * kOcrUpscaleFactor;
    const qint64 scaledHeight = static_cast<qint64>(input.height()) * kOcrUpscaleFactor;

    // QImage dimensions are int-based, so check against int range before converting.
    if (scaledWidth > std::numeric_limits<int>::max() || scaledHeight > std::numeric_limits<int>::max()) {
        result.status = Okular::PageViewOcr::TaskStatus::ImageTooLarge;
        return result;
    }

    if (scaledWidth * scaledHeight > kMaxOcrPixelCount) {
        result.status = Okular::PageViewOcr::TaskStatus::ImageTooLarge;
        return result;
    }

    QImage image = input.scaled(static_cast<int>(scaledWidth), static_cast<int>(scaledHeight), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.isNull()) {
        result.status = Okular::PageViewOcr::TaskStatus::PreparationFailed;
        return result;
    }

    image = applyOtsuBinarization(image);
    if (image.isNull()) {
        result.status = Okular::PageViewOcr::TaskStatus::PreparationFailed;
        return result;
    }

    result.status = Okular::PageViewOcr::TaskStatus::Success;
    result.image = image;
    return result;
}
}

namespace Okular::PageViewOcr
{
QString defaultLanguageCode()
{
    const QLocale::Language systemLanguage = QLocale::system().language();
    QString languageCode = QLocale::languageToCode(systemLanguage, QLocale::ISO639Part2B).toLower();
    if (languageCode.isEmpty()) {
        languageCode = QLocale::languageToCode(systemLanguage, QLocale::ISO639Part2T).toLower();
    }
    if (languageCode.isEmpty() || languageCode == QStringLiteral("und")) {
        languageCode = kFallbackTesseractLanguage;
    }
    return languageCode;
}

QStringList availableLanguageCodes()
{
    QString tessdataPath;

    if (qEnvironmentVariableIsSet("TESSDATA_PREFIX")) {
        tessdataPath = QString::fromUtf8(qgetenv("TESSDATA_PREFIX"));
    }

    if (tessdataPath.isEmpty()) {
        tesseract::TessBaseAPI api;
        const char *path = api.GetDatapath();
        if (path) {
            tessdataPath = QString::fromUtf8(path);
        }
    }

    if (tessdataPath.isEmpty()) {
#if defined(Q_OS_WIN)
        // Standard Windows installations rely on TESSDATA_PREFIX set by installer or Craft
#elif defined(Q_OS_MAC)
        tessdataPath = QStringLiteral("/opt/homebrew/share/tessdata");
        if (!QDir(tessdataPath).exists()) {
            tessdataPath = QStringLiteral("/opt/local/share/tessdata");
        }
#else
        const QStringList paths = {QStringLiteral("/usr/share/tesseract-ocr/5/tessdata"), QStringLiteral("/usr/share/tesseract-ocr/4.00/tessdata"), QStringLiteral("/usr/share/tessdata"), QStringLiteral("/usr/local/share/tessdata")};
        for (const QString &path : paths) {
            if (QDir(path).exists()) {
                tessdataPath = path;
                break;
            }
        }
#endif
    }

    if (tessdataPath.isEmpty() || !QDir(tessdataPath).exists()) {
        return {};
    }

    QDir dir(tessdataPath);
    const QStringList files = dir.entryList({QStringLiteral("*.traineddata")}, QDir::Files);

    QStringList codes;
    codes.reserve(files.size());

    for (const QString &file : files) {
        const QString code = file.section(QLatin1Char('.'), 0, 0).trimmed().toLower();
        if (code.isEmpty() || code == QStringLiteral("osd") || codes.contains(code)) {
            continue;
        }
        codes.append(code);
    }

    std::sort(codes.begin(), codes.end());
    return codes;
}

TaskResultMessage messageForTaskResult(const TaskResult &result)
{
    TaskResultMessage message;

    switch (result.status) {
    case TaskStatus::Success:
        message.message = i18n("Text copied to clipboard.");
        message.type = MessageType::Info;
        message.copyRecognizedText = true;
        return message;
    case TaskStatus::NoText:
        message.message = i18n("No text could be recognized.");
        message.type = MessageType::Warning;
        return message;
    case TaskStatus::EmptyImage:
        message.message = i18n("The selection is empty.");
        message.type = MessageType::Error;
        return message;
    case TaskStatus::ImageTooLarge:
        message.message = i18n("The selected area is too large. Try a smaller selection.");
        message.type = MessageType::Error;
        return message;
    case TaskStatus::PreparationFailed:
        message.message = i18n("Failed to prepare the image for text recognition.");
        message.type = MessageType::Error;
        return message;
    case TaskStatus::InitFailed:
        message.message = i18n("Failed to initialize Tesseract.");
        message.details = i18n("Check that Tesseract language data is installed for: %1.", displayLanguageList(result.languages));
        if (!result.dataPath.isEmpty()) {
            message.details += QLatin1Char('\n') + i18n("Tesseract data path: %1", result.dataPath);
        }
        message.type = MessageType::Error;
        return message;
    case TaskStatus::ExtractionFailed:
        message.message = i18n("Failed to extract text from the image.");
        message.type = MessageType::Error;
        return message;
    }

    return message;
}

TaskResult runTask(const QImage &sourceImage, const QStringList &ocrLanguages)
{
    TaskResult result;

    const QStringList normalizedLanguages = effectiveLanguageList(ocrLanguages);

    result.languages = languageSpecFromList(normalizedLanguages);

    const ImagePreparationResult preparation = prepareImageForTask(sourceImage);
    switch (preparation.status) {
    case TaskStatus::Success:
        break;
    case TaskStatus::EmptyImage:
    case TaskStatus::ImageTooLarge:
    case TaskStatus::PreparationFailed:
        result.status = preparation.status;
        return result;
    default:
        Q_UNREACHABLE();
        return result;
    }

    if (QThread::currentThread()->isInterruptionRequested()) {
        result.status = TaskStatus::EmptyImage;
        return result;
    }

    tesseract::TessBaseAPI api;

    if (!initTesseractWithLanguages(api, result.languages)) {
        result.status = TaskStatus::InitFailed;
        result.dataPath = QString::fromUtf8(api.GetDatapath() ? api.GetDatapath() : "");
        return result;
    }

    const QImage &image = preparation.image;
    api.SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
    api.SetImage(image.constBits(), image.width(), image.height(), 1, image.bytesPerLine());

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
