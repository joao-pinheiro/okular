#ifndef OKULAR_PART_PAGEVIEWOCR_H
#define OKULAR_PART_PAGEVIEWOCR_H

#include <QImage>
#include <QString>
#include <QStringList>

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

enum class MessageType {
    Info,
    Warning,
    Error,
};

struct TaskResultMessage {
    QString message;
    QString details;
    MessageType type = MessageType::Info;
    bool copyRecognizedText = false;
};

QString defaultLanguageCode();
QStringList availableLanguageCodes();
TaskResultMessage messageForTaskResult(const TaskResult &result);
TaskResult runTask(const QImage &sourceImage, const QStringList &ocrLanguages);
}

#endif