#include "document_analyzer.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/*
 * Derives an individual report filename from the input file path.
 * Example: "document1.txt" -> "document1_report.txt"
 *          "docs/notes.txt" -> "notes_report.txt"
 */
static void buildReportFilename(const char *inputPath, char *outputBuf, size_t bufSize) {
    /* Extract basename (portion after the last '/' or '\\') */
    const char *base = inputPath;
    for (const char *p = inputPath; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    if (*base == '\0') {
        base = "document";
    }

    /* Strip file extension if present (only after the first character) */
    const char *dot = strrchr(base, '.');
    size_t stemLen = (dot != NULL && dot != base) ? (size_t)(dot - base) : strlen(base);

    char stem[MAX_PATH_LEN];
    if (stemLen >= sizeof(stem)) {
        stemLen = sizeof(stem) - 1;
    }
    memcpy(stem, base, stemLen);
    stem[stemLen] = '\0';

    snprintf(outputBuf, bufSize, "%s_report.txt", stem);
}

/* Appends a 1-based line number to the task's dynamic matchingLines array */
static int recordMatchingLine(DocumentTask *task, int lineNumber) {
    if (task->matchingLineCount >= task->matchingLinesCapacity) {
        int newCap = (task->matchingLinesCapacity == 0)
                     ? INITIAL_LINES_CAP
                     : task->matchingLinesCapacity * 2;
        int *resized = (int *)realloc(task->matchingLines, (size_t)newCap * sizeof(int));
        if (!resized) {
            task->status = STATUS_MEMORY_ERROR;
            snprintf(task->errorMessage, sizeof(task->errorMessage),
                     "Memory allocation failed while recording matching line numbers");
            return 0;
        }
        task->matchingLines = resized;
        task->matchingLinesCapacity = newCap;
    }

    task->matchingLines[task->matchingLineCount++] = lineNumber;
    return 1;
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

void initThreadTask(DocumentTask *task, int threadNum,
                    const char *inputFilename, const char *keyword) {
    if (!task) {
        return;
    }
    memset(task, 0, sizeof(DocumentTask));
    task->threadNum = threadNum;

    if (inputFilename) {
        strncpy(task->inputFilename, inputFilename, MAX_PATH_LEN - 1);
        task->inputFilename[MAX_PATH_LEN - 1] = '\0';
        buildReportFilename(inputFilename, task->outputFilename, sizeof(task->outputFilename));
    }

    if (keyword) {
        strncpy(task->keyword, keyword, MAX_KEYWORD_LEN - 1);
        task->keyword[MAX_KEYWORD_LEN - 1] = '\0';
    }

    task->status = STATUS_PENDING;
    task->matchingLines = NULL;
    task->matchingLinesCapacity = 0;
    task->threadCreated = 0;
}

void freeThreadTask(DocumentTask *task) {
    if (!task) {
        return;
    }
    if (task->matchingLines != NULL) {
        free(task->matchingLines);
        task->matchingLines = NULL;
    }
    task->matchingLinesCapacity = 0;
}

const char *statusToString(ProcessingStatus status) {
    switch (status) {
        case STATUS_SUCCESS:      return "Completed";
        case STATUS_INPUT_ERROR:  return "Failed (Input File Error)";
        case STATUS_OUTPUT_ERROR: return "Failed (Report Write Error)";
        case STATUS_MEMORY_ERROR: return "Failed (Memory Allocation Error)";
        case STATUS_THREAD_ERROR: return "Failed (Thread Creation Error)";
        default:                  return "Pending";
    }
}

int countKeywordInLine(const char *line, const char *keyword) {
    if (!line || !keyword || *keyword == '\0') {
        return 0;
    }

    size_t keyLen = strlen(keyword);
    int occurrences = 0;
    const char *p = line;

    while (*p != '\0') {
        size_t i = 0;
        while (i < keyLen && p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)keyword[i])) {
            i++;
        }
        if (i == keyLen) {
            occurrences++;
            p += keyLen; /* Advance past match to avoid overlapping counts */
        } else {
            p++;
        }
    }

    return occurrences;
}

int analyzeDocument(DocumentTask *task) {
    if (!task) {
        return 0;
    }

    FILE *fp = fopen(task->inputFilename, "r");
    if (!fp) {
        task->status = STATUS_INPUT_ERROR;
        snprintf(task->errorMessage, sizeof(task->errorMessage),
                 "Unable to open input file '%s': %s",
                 task->inputFilename, strerror(errno));
        return 0;
    }

    char buffer[LINE_BUFFER_SIZE];
    int currentLine = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        currentLine++;
        int hitsOnLine = countKeywordInLine(buffer, task->keyword);

        /* Handle lines longer than LINE_BUFFER_SIZE by continuing to the newline */
        size_t len = strlen(buffer);
        while (len > 0 && buffer[len - 1] != '\n' && fgets(buffer, sizeof(buffer), fp) != NULL) {
            hitsOnLine += countKeywordInLine(buffer, task->keyword);
            len = strlen(buffer);
        }

        if (hitsOnLine > 0) {
            task->totalOccurrences += hitsOnLine;
            if (!recordMatchingLine(task, currentLine)) {
                fclose(fp);
                return 0;
            }
        }
    }

    if (ferror(fp)) {
        task->status = STATUS_INPUT_ERROR;
        snprintf(task->errorMessage, sizeof(task->errorMessage),
                 "Read error encountered while processing '%s'", task->inputFilename);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    task->totalLines = currentLine;
    return 1;
}

int generateAnalysisReport(DocumentTask *task) {
    if (!task) {
        return 0;
    }

    FILE *out = fopen(task->outputFilename, "w");
    if (!out) {
        task->status = STATUS_OUTPUT_ERROR;
        snprintf(task->errorMessage, sizeof(task->errorMessage),
                 "Unable to create report file '%s': %s",
                 task->outputFilename, strerror(errno));
        return 0;
    }

    fprintf(out, "============================================================\n");
    fprintf(out, "             DOCUMENT SEARCH & ANALYSIS REPORT\n");
    fprintf(out, "============================================================\n");
    fprintf(out, "Input File            : %s\n", task->inputFilename);
    fprintf(out, "Search Keyword        : %s (case-insensitive)\n", task->keyword);
    fprintf(out, "Total Lines Processed : %d\n", task->totalLines);
    fprintf(out, "Keyword Occurrences   : %d\n", task->totalOccurrences);
    fprintf(out, "Matching Lines Count  : %d\n", task->matchingLineCount);

    fprintf(out, "Matching Line Numbers : ");
    if (task->matchingLineCount == 0) {
        fprintf(out, "None (keyword not found)\n");
    } else {
        for (int i = 0; i < task->matchingLineCount; i++) {
            fprintf(out, "%d%s", task->matchingLines[i],
                    (i + 1 < task->matchingLineCount) ? ", " : "\n");
        }
    }

    if (task->matchingLineCount > 0) {
        fprintf(out, "Processing Status     : SUCCESS (Matches Found)\n");
    } else {
        fprintf(out, "Processing Status     : SUCCESS (No Matches Found)\n");
    }
    fprintf(out, "============================================================\n");

    if (ferror(out) || fclose(out) != 0) {
        task->status = STATUS_OUTPUT_ERROR;
        snprintf(task->errorMessage, sizeof(task->errorMessage),
                 "Error writing to report file '%s'", task->outputFilename);
        return 0;
    }

    task->status = STATUS_SUCCESS;
    return 1;
}

void *processFileThread(void *arg) {
    DocumentTask *task = (DocumentTask *)arg;
    if (!task) {
        return NULL;
    }

    printf("[Thread %d] Processing %s ...\n", task->threadNum, task->inputFilename);
    fflush(stdout);

    if (!analyzeDocument(task)) {
        fprintf(stderr, "[Thread %d] Error processing %s: %s\n",
                task->threadNum, task->inputFilename, task->errorMessage);
        fflush(stderr);
        return NULL;
    }

    if (!generateAnalysisReport(task)) {
        fprintf(stderr, "[Thread %d] Error generating report for %s: %s\n",
                task->threadNum, task->inputFilename, task->errorMessage);
        fflush(stderr);
        return NULL;
    }

    if (task->totalOccurrences == 0) {
        printf("[Thread %d] Successfully processed %s (0 matches) -> %s\n",
               task->threadNum, task->inputFilename, task->outputFilename);
    } else {
        printf("[Thread %d] Successfully processed %s (%d match(es) on %d line(s)) -> %s\n",
               task->threadNum, task->inputFilename,
               task->totalOccurrences, task->matchingLineCount, task->outputFilename);
    }
    fflush(stdout);

    return NULL;
}

void displayProcessingSummary(const DocumentTask *tasks, int taskCount) {
    printf("\n===== PROCESSING SUMMARY =====\n\n");

    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < taskCount; i++) {
        const DocumentTask *t = &tasks[i];
        if (t->status == STATUS_SUCCESS) {
            successCount++;
            printf("  %-24s : Completed (%d lines, %d occurrence(s) on %d line(s)) -> %s\n",
                   t->inputFilename, t->totalLines, t->totalOccurrences,
                   t->matchingLineCount, t->outputFilename);
        } else {
            failCount++;
            printf("  %-24s : %s - %s\n",
                   t->inputFilename, statusToString(t->status), t->errorMessage);
        }
    }

    printf("\nAll processing tasks completed.\n");
    if (failCount == 0) {
        printf("Reports generated successfully (%d/%d).\n", successCount, taskCount);
    } else {
        printf("Summary: %d succeeded, %d failed.\n", successCount, failCount);
    }
}

