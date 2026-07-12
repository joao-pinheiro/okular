# Async Printing Scan Summary

## 1. What's Async?
- The current codebase introduces a `PrintJob` abstraction (inheriting from `KJob`).
- For the `poppler` generator, `PDFGenerator::printAsync` returns a `PdfPrintJob`.
- The execution of the print job is deferred from the immediate call stack using `QTimer::singleShot(0, this, &PdfPrintJob::doWork)` in `PdfPrintJob::start()`.
- For standard UNIX/CUPS printing (when rasterization is not forced), `PdfPrintJob::doWork` offloads the `psConverter->convert()` and `Okular::FilePrinter::printFile()` calls to a background thread using `QtConcurrent::run` and monitors completion with a `QFutureWatcher`.

## 2. What's Fake-Async (Still Blocking)?
- **Rasterization Mode (Windows & Forced Rasterize on Linux):** If `forceRasterize` is true (which it is by default on Windows via `#ifdef Q_OS_WIN`), `doWork()` does not use `QtConcurrent::run`. Instead, it loops through every page, calling `pp->renderToImage(...)` and painting it to the printer directly on the **main thread**. This completely blocks the event loop for the entire print duration.

## 3. Where is the actual bottleneck?
Even when using the "proper" async path with `QtConcurrent::run` (on Linux), the app still becomes unresponsive due to thread synchronization bottlenecks:
- Inside the `QtConcurrent::run` lambda, the background thread calls `userMutex->lock()` before starting `psConverter->convert()` and holds the lock for the entire conversion process.
- The `userMutex` is the primary lock used throughout `PDFGenerator` to serialize access to the underlying `Poppler::Document` (e.g., in `generatePixmap`, `textPage`, `metaData`).
- Consequently, while the background thread is converting the PDF to PostScript, the main thread will inevitably block the moment it tries to repaint the UI, load a new page, or extract text, because it will be waiting to acquire the `userMutex`. This causes the "Not Responding" state.

## 4. Baseline Comparison
In the `okular-a6c8d6dac3d44878e48f01a12d6c736c2195b62e` baseline, printing was completely synchronous. The `Part::slotPrint()` method would call `m_document->print(printer)`, which flowed down to `PDFGenerator::print()`, executing the process synchronously on the main thread and returning a `Document::PrintError` directly. The current codebase is an attempt to make this async via `KJob`/`QtConcurrent`, but it falls short of providing a non-blocking UI due to the mutex contention and the main-thread rasterization loop.
