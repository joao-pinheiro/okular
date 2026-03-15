#ifndef OKULAR_PART_PAGEVIEWOCR_H
#define OKULAR_PART_PAGEVIEWOCR_H

#include <QImage>
#include <QString>

namespace Okular::PageViewOcr
{
enum class TaskStatus {
    Success,
    NoText,
    EmptyImage,
    ImageTooLarge,
    PreparationFailed,
    InitFailed,
    ExtractionFailed,
};

struct TaskResult {
    TaskStatus status = TaskStatus::PreparationFailed;
    QString text;
    QString languages;
    QString dataPath;
};

QString languageSpecFromSettings();
TaskResult runTask(const QImage &sourceImage, const QString &ocrLanguages);
}

#endif