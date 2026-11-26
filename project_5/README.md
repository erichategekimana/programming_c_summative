# Project 5: Multi-threaded Document Search and Analysis System in C

## Overview

A modular, POSIX-threads (`pthreads`) C application that searches and analyzes multiple text documents concurrently. Each input file is assigned its own worker thread with an isolated `DocumentTask` structure and its own output report file, demonstrating independent parallel file processing without shared mutable state or thread synchronization overhead.

## Directory Structure

```text
project_5/
├── README.md
├── mult-thread_analysis_system/
│   ├── document_analyzer.h       # Constants, enums, DocumentTask struct, function prototypes
│   ├── document_analyzer.c       # Core analysis, case-insensitive search, report & summary logic
│   ├── main.c                    # CLI validation, thread lifecycle (pthread_create / pthread_join)
│   ├── document1.txt             # Sample input: Smart Agriculture (5 matches across 4 lines)
│   ├── document2.txt             # Sample input: Connected Healthcare (3 matches across 3 lines)
│   ├── document3.txt             # Sample input: Industrial Automation (3 matches across 3 lines)
│   ├── document4_nomatch.txt     # Sample input: Database Normalization (0 matches edge case)
│   ├── document1_report.txt      # Generated report for document1.txt
│   ├── document2_report.txt      # Generated report for document2.txt
│   ├── document3_report.txt      # Generated report for document3.txt
│   └── document4_nomatch_report.txt
└── screenshots/
    ├── 01_compile_and_concurrent_run.png
    ├── 02_generated_reports_verification.png
    └── 03_error_handling_and_edge_cases.png
```

## Compilation & Usage

```bash
cd project_5/mult-thread_analysis_system
gcc -Wall -Wextra -std=c11 -pthread main.c document_analyzer.c -o document_analyzer
./document_analyzer "IoT" document1.txt document2.txt document3.txt
```

## Key C Concepts Implemented

- **POSIX Multi-threading (`pthread_create` / `pthread_join`)**: Dynamically allocates one `pthread_t` and one `DocumentTask` struct per input file. Every thread works exclusively on its own task struct and output report file, eliminating data races without requiring mutexes. All successfully created threads are joined before exit, even if another thread fails to spawn.
- **Modular Architecture**:
  - `initThreadTask()` / `freeThreadTask()` — initializes task parameters, derives `<basename>_report.txt`, and releases heap memory.
  - `countKeywordInLine()` — performs non-overlapping, case-insensitive substring matching (`tolower()`).
  - `analyzeDocument()` — streams lines safely (including lines longer than the buffer), counts total lines, total keyword occurrences, and dynamically stores matching 1-based line numbers using `realloc()`.
  - `generateAnalysisReport()` — writes each thread's independent report with full `fopen`/`ferror`/`fclose` error checking.
  - `processFileThread()` — thread entry point that logs start, completion, and errors in real time.
  - `displayProcessingSummary()` — prints the consolidated terminal summary after all threads join.
- **Comprehensive Error Handling**: Gracefully handles insufficient CLI arguments, empty keywords, missing/unreadable input files (`strerror(errno)`), report write failures, zero-match documents, and `pthread_create()` failures.

## Verification Evidence

1. **Compilation & Concurrent Execution**: `screenshots/01_compile_and_concurrent_run.png`
2. **Individual Generated Reports**: `screenshots/02_generated_reports_verification.png`
3. **Error Handling & Edge Cases (0 matches, missing file, invalid CLI args)**: `screenshots/03_error_handling_and_edge_cases.png`

