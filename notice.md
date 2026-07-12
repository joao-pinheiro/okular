# Async Printing in Okular — Reviewer Guide

This document accompanies the merge request that fixes the UI-freeze-during-print
bug. It is intended for reviewers and maintainers who want to understand, test,
or extend the change.

---

## 1. What Was Fixed

**Bug:** Okular becomes unresponsive ("Not Responding") whenever a user prints a
document. The window freezes until the print job completes.

**Root cause:** `Part::doPrint()` called `m_document->print()` synchronously on
the main Qt event-loop thread. The Poppler generator holds a `userMutex` for the
entire duration, starving every other UI operation of the event loop.

**Fix:** Printing now runs on a dedicated `QThread`. The main thread sets up the
printer and returns immediately; a `PrintWorker` object on the background thread
opens an **independent** `Okular::Document` (its own Poppler handle, no shared
locks) and performs the actual print. `KNotification` system pop-ups report
progress back to the user.

---

## 2. Files Changed at a Glance

| File | Type | What changed |
|------|------|--------------|
| `CMakeLists.txt` | Build | Added `KF6::Notifications` dependency; installed `okular.notifyrc` |
| `core/generator.h` | API | Added `PrintAsync` generator capability flag |
| `generators/poppler/generator_pdf.cpp` | PDF generator | Advertises `PrintAsync` capability |
| `part/part.h` | Header | New member variables and slot for async print state |
| `part/part.cpp` | Core logic | `PrintWorker` class + full async `doPrint()` + `onPrintFinished()` |
| `part/okular.notifyrc` | Data | New KDE notification event definitions |

---

## 3. How to Build

### Prerequisites
Ensure `KNotification` (part of KF6) is available. On Arch Linux:
```bash
# Already present if you have a full KDE Frameworks install.
# Check with:
pkg-config --modversion KF6Notifications
```

On Debian/Ubuntu:
```bash
sudo apt install libkf6notifications-dev
```

### Build Steps
```bash
# From the okular source root:
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

> **Note for kde-builder users:** If you build KF6 libraries locally (e.g. into
> `~/kde/usr/`), see §6 below for a known `std::locale` crash and its fix.

### Run the Development Build
```bash
./build/bin/okular
# or with a test document:
./build/bin/okular /path/to/document.pdf
```

---

## 4. How to Test the Async Print Feature

### Basic test — verify UI stays responsive
1. Open any PDF in your development build of Okular.
2. Open **File → Print** (or press `Ctrl+P`).
3. Configure the printer and click **Print**.
4. **Expected:** The print dialog closes, a system notification pops up saying
   *"Printing <document name>…"* and the Okular window remains **fully
   interactive** — you can scroll, zoom, switch pages while printing.
5. When the job completes, a second notification appears:
   *"Print job sent to printer."*

### Verify notifications appear
The three notification events are defined in `part/okular.notifyrc`:

| Event key | When fired | Default action |
|-----------|-----------|----------------|
| `printStarted` | Print job begins on the worker thread | Popup (auto-closes) |
| `printFinished` | Worker completes with no error | Popup (auto-closes) |
| `printError` | Worker returns a non-zero `PrintError` | Popup + Sound (persistent) |

You can customise these events in **System Settings → Notifications → Okular**.

### Test with unsaved annotations
1. Open a PDF and add an annotation (e.g. a highlight).
2. **Do not save** the file.
3. Print the document.
4. **Expected:** The annotation is present in the printout. (The code
   automatically writes a temporary PDF capturing in-memory changes before
   handing the path to the worker thread.)

### Test concurrent print guard
1. Start a long print job on a large document.
2. Immediately try **File → Print** again.
3. **Expected:** An information dialog appears: *"A print job is already in
   progress. Please wait for it to finish."* No second job is started.

### Test `--print-and-exit` CLI mode
```bash
./build/bin/okular --print-and-exit /path/to/document.pdf
```
**Expected:** Okular opens, silently prints, then exits with code `0` on success
or a non-zero code on failure.

---

## 5. Key Configuration & Code Locations

### Where to find the notification config
- **Source:** [`part/okular.notifyrc`](part/okular.notifyrc)
- **Installed to:** `$(KDE_INSTALL_KNOTIFYRCDIR)/okular.notifyrc`
  (typically `/usr/share/knotifications6/okular.notifyrc` on a system install,
  or `~/kde/usr/share/knotifications6/okular.notifyrc` for a local build)
- **User override:** System Settings → Notifications → Okular

### Where the async logic lives
All async print code is self-contained in [`part/part.cpp`](part/part.cpp):

| Symbol | Line (approx.) | Purpose |
|--------|---------------|---------|
| `class PrintWorker` | ~3615 | Background QObject; opens independent doc, calls `print()` |
| `Part::doPrint()` | ~3670 | Orchestrates async path: temp file, thread, notification |
| `Part::onPrintFinished()` | ~3790 | Thread teardown, temp-file cleanup, re-enables actions |

### The `PrintAsync` capability flag
- **Declared in:** [`core/generator.h`](core/generator.h) — `GeneratorFeature::PrintAsync`
- **Advertised by:** [`generators/poppler/generator_pdf.cpp`](generators/poppler/generator_pdf.cpp)
  — `setFeature(PrintAsync)` in `PDFGenerator::PDFGenerator()`

Generators that do **not** advertise `PrintAsync` will silently fall back to the
original synchronous print path (`doPrint(..., async=false)`), preserving
backward compatibility with all non-PDF generators.

### Build system changes
In [`CMakeLists.txt`](CMakeLists.txt):
- Line ~111: `Notifications` added to `find_package(KF6 ... REQUIRED COMPONENTS ...)`
- Line ~591: `KF6::Notifications` added to `target_link_libraries(okularpart ...)`
- Line ~624: `install(FILES part/okular.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})`

---

## 6. Known Issue During Development Build (Local KDE Frameworks)

> **This only affects developers building KF6 from source with `kde-builder`.**
> It does not affect packaged releases.

**Symptom:** `./build/bin/okular` crashes immediately with a segfault in
`kconfigDBusSanitizePath()` (`std::isalnum` / `std::locale` at address `0x20`).

**Root cause:** GCC 16 inlines `std::locale` symbols directly into
`libKF6ConfigCore.so.6` (so it does not list `libstdc++` as a `NEEDED`
dependency). When both this `.so` and the system `libstdc++.so.6` are loaded
into the same process, two independent `std::locale` runtime states exist. The
`std::ctype<char>` facet pointer from one runtime is looked up in the other's
table → NULL deref.

**Fix (already applied to `/home/muruga/kde/src/kconfig`):**
In `kconfig/src/core/dbussanitizer_p.h`, replace:
```cpp
// BEFORE — triggers locale ABI conflict
std::isalnum(character.toLatin1(), std::locale::classic())
```
```cpp
// AFTER — uses C-locale directly, no std::locale object involved
std::isalnum(static_cast<unsigned char>(character.toLatin1()))
```
Then rebuild and reinstall kconfig:
```bash
cmake --build ~/kde/build/kconfig -j$(nproc)
cp ~/kde/build/kconfig/bin/libKF6ConfigCore.so.6.*.* ~/kde/usr/lib/
```

This fix is safe and semantically identical: `std::locale::classic()` *is* the
C locale, and `std::isalnum(unsigned char)` from `<cctype>` is the standard way
to call the C-locale version without constructing a `std::locale` object.

---

## 7. Backward Compatibility

- **Non-PDF generators** (DjVu, PS, EPUB, etc.) are completely unaffected. They
  do not advertise `PrintAsync`, so `doPrint()` falls through to the original
  synchronous `m_document->print()` call.
- **`--print-and-exit` CLI flag** continues to work. `m_cliPrintAndExit` is
  checked in `onPrintFinished()` and the process exits with the correct code.
- **KParts embedding:** The change is confined to `okularpart`. Host
  applications embedding Okular via KParts see no API change.
- **No `.desktop` file changes required.**

---

## 8. Merge Request Checklist

- [x] UI no longer freezes during print
- [x] System notifications fire on start / finish / error
- [x] Unsaved annotation changes are captured before printing
- [x] Concurrent print guard prevents double-submission
- [x] `--print-and-exit` exits with correct code
- [x] Non-PDF generators fall back to synchronous path (no regression)
- [x] `KF6::Notifications` added to CMake dependency list
- [x] `okular.notifyrc` installed via CMake
- [x] `PrintAsync` capability flag documented in generator.h
