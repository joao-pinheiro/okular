# Contribution Intent: Asynchronous Printing with KNotification

## The Bug

When a user triggers the print function in Okular (via File → Print or Ctrl+P),
the application window becomes completely unresponsive ("Not Responding") until
the print job finishes. This is because `Part::doPrint()` called
`m_document->print()` directly on the main UI thread, which in turn called into
the Poppler PDF generator — a blocking, synchronous operation that holds the
shared `userMutex` lock for the entire duration of printing.

**Symptoms:**
- The Okular window freezes and turns grey during printing.
- The OS marks the process as "Not Responding".
- No feedback is given to the user about progress or completion.
- Long documents or slow printers can freeze the UI for many seconds.

## Contribution Intent

Fix the UI freeze by making the entire print pipeline asynchronous:

1. The main thread sets up `QPrinter` and resolves page ranges, then **returns
   immediately** from `slotPrint()`.
2. A dedicated `QThread` + `PrintWorker` object opens an **independent**
   `Okular::Document` instance (which creates its own Poppler document and its
   own generator locks), performs the print in the background, and emits a
   `done` signal when finished.
3. System notifications (`KNotification`) inform the user when printing starts,
   finishes, or fails — without blocking the UI thread at all.

---

## Files Changed

### `CMakeLists.txt`
**Why changed:**
- Added `KF6::Notifications` to the `find_package(KF6 ...)` required components
  list.
- Added `KF6::Notifications` to the `target_link_libraries(okularpart ...)` list.
- Added an `install()` rule for the new `part/okular.notifyrc` file to the
  correct KNotify resource directory.

**Issue fixed:** Without these changes, the build would fail to find
`KNotification` and the `.notifyrc` event definitions would not be installed,
preventing system notifications from appearing.

---

### `core/generator.h`
**Why changed:**
- Added a new `PrintAsync` capability flag to the `GeneratorFeature` enum.

**Issue fixed:** Provides a clean, discoverable API flag that any generator can
advertise to declare that its `print()` method is safe to call from a non-main
thread (i.e., it does not share mutable state with the main-thread document
instance). This allows `Part::doPrint()` to make an informed decision about
whether to use the async path.

---

### `generators/poppler/generator_pdf.cpp`
**Why changed:**
- Called `setFeature(PrintAsync)` in `PDFGenerator`'s constructor.

**Issue fixed:** The Poppler PDF generator opens an independent `Poppler::Document`
handle inside `PrintWorker` (a fresh `openDocument()` call). Because this is a
separate handle with its own internal lock, there is zero shared mutable state
with the main-thread generator — making background printing safe. Advertising
`PrintAsync` here enables the async path in `Part::doPrint()`.

---

### `part/part.h`
**Why changed:**
- Forward-declared `QThread`.
- Changed `doPrint(QPrinter &printer)` to `doPrint(QPrinter *printer, bool async = true)`.
  The printer is now heap-allocated and owned by the worker thread.
- Added two private member variables:
  - `QThread *m_printThread = nullptr` — tracks the live background print thread.
  - `QString m_printTempFilePath` — path to a temporary PDF written when the
    document has unsaved in-memory changes (annotations, form edits) that must
    be captured before printing.
- Added private slot `onPrintFinished(Okular::Document::PrintError error)`.

**Issue fixed:** These declarations are the header-level contracts required by
the async implementation in `part.cpp`.

---

### `part/part.cpp`
**Why changed — `slotPrint()` / `slotPrintPreview()`:**
- `slotPrint()` now allocates `QPrinter` on the heap and passes ownership to
  `doPrint()`. The `// Do NOT exit here` comment documents that the return value
  of `doPrint()` is `true` immediately (before printing completes), so no
  post-print cleanup should happen in this slot.

**Why changed — new `PrintWorker` class (inner class defined in `part.cpp`):**
- A `QObject`-derived worker that is moved to a `QThread`.
- Its `run()` slot opens a fresh `Okular::Document`, opens the document file,
  calls `printDoc.print(*m_printer)`, and emits `done(PrintError)`.
- Critically, this document instance is **completely independent** from the UI
  thread's `m_document`. No locks are shared between the two.
- The worker owns the `QPrinter*` and deletes it in its destructor.

**Why changed — `doPrint(QPrinter *printer, bool async)`:**
- Replaced the old single synchronous call `m_document->print(printer)` with
  the full async pipeline:
  1. Check `AllowPrint` permission (and delete printer if denied).
  2. Resolve `QPrinter::CurrentPage` to an explicit page range on the main thread
     (printer state must be read before the printer is moved to the worker).
  3. **Sync fallback:** if `async == false`, do the old blocking print call
     (used for generators that do not advertise `PrintAsync`).
  4. **Temp-file handling:** if `m_document->canSaveChanges()` is true, the
     document has unsaved in-memory state (annotations, form data). Save these
     to a `QTemporaryFile` so the worker thread sees the latest version.
  5. **Guard:** if `m_printThread` is already running, show an information
     message and refuse to start a second job.
  6. Disable the Print and Print Preview actions while the job runs.
  7. Fire a `KNotification` "printStarted" event.
  8. Create `QThread` + `PrintWorker`, wire up signals/slots with
     `Qt::QueuedConnection`, and call `m_printThread->start()`.
  9. Return `true` immediately — the UI is now free.

**Why changed — new `onPrintFinished()` slot:**
- Called (via `QueuedConnection`) when the worker emits `done`.
- Quits and joins the thread, then deletes it.
- Removes any temporary PDF file that was created.
- Re-enables Print and Print Preview actions.
- Fires a `KNotification` "printFinished" or "printError" event.
- If `--print-and-exit` CLI mode was active, exits with the appropriate code.

**Issue fixed:** Together these changes eliminate 100% of the UI freeze. The main
thread never waits on the Poppler generator during printing.

---

### `part/okular.notifyrc` *(new file)*
**Why added:**
- Defines three KDE notification events consumed by `KNotification::event()`:
  - `printStarted` — transient popup when a print job begins.
  - `printFinished` — transient popup when a print job succeeds.
  - `printError` — persistent popup + sound when printing fails.

**Issue fixed:** Without a `.notifyrc` the `KNotification` calls are silently
ignored and no feedback is shown to the user.

---

## Summary of the Issue Fixed

| Before | After |
|--------|-------|
| `doPrint()` blocks the main thread calling `m_document->print()` | `doPrint()` returns immediately; printing runs on `QThread` |
| UI freezes for the entire print duration | UI remains fully responsive |
| No feedback to user during or after printing | System notifications shown on start, finish, and error |
| Shared `Poppler::Document` contended between UI and print | Worker opens its own independent `Poppler::Document` |
| Annotations / form changes printed only if already saved to disk | Unsaved changes flushed to a temp PDF before printing |
