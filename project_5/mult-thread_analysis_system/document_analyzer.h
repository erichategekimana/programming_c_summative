#ifndef DOCUMENT_ANALYZER_H
#define DOCUMENT_ANALYZER_H

#include <stddef.h>

/* ============================================================================
 * SYSTEM CONSTANTS
 * ============================================================================ */
#define MAX_PATH_LEN        256
#define MAX_KEYWORD_LEN     128
#define MAX_ERROR_MSG_LEN   512
#define INITIAL_LINES_CAP   16
#define LINE_BUFFER_SIZE    4096

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/* Processing outcome for a single document task */
typedef enum {
    STATUS_PENDING = 0,
    STATUS_SUCCESS,
    STATUS_INPUT_ERROR,
    STATUS_OUTPUT_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_THREAD_ERROR
} ProcessingStatus;

/*
 * Encapsulates all input parameters and independent output results for a
 * single worker thread. Each thread operates exclusively on its own instance,
 * avoiding shared mutable state and the need for thread synchronization.
 */
typedef struct {
    int threadNum;                          /* 1-based logical thread identifier */
    char inputFilename[MAX_PATH_LEN];       /* Assigned input text file path */
    char outputFilename[MAX_PATH_LEN];      /* Generated individual report filename */
    char keyword[MAX_KEYWORD_LEN];          /* Case-insensitive search keyword */

    int totalLines;                         /* Total lines read from input file */
    int totalOccurrences;                   /* Total keyword occurrences found */
    int matchingLineCount;                  /* Number of lines with >= 1 match */
    int *matchingLines;                     /* Dynamic array of 1-based line numbers */
    int matchingLinesCapacity;              /* Allocated capacity of matchingLines */

    ProcessingStatus status;                /* Final processing status */
    char errorMessage[MAX_ERROR_MSG_LEN];   /* Detailed error description, if any */
    int threadCreated;                      /* 1 if pthread_create succeeded, 0 otherwise */
} DocumentTask;

/* ============================================================================
 * MODULAR FUNCTION DECLARATIONS
 * ============================================================================ */

/* Initializes a DocumentTask struct and derives its output report filename */
void initThreadTask(DocumentTask *task, int threadNum,
                    const char *inputFilename, const char *keyword);

/* Releases dynamically allocated memory owned by a DocumentTask */
void freeThreadTask(DocumentTask *task);

/* Counts non-overlapping case-insensitive occurrences of keyword in line */
int countKeywordInLine(const char *line, const char *keyword);

/* Reads and analyzes the assigned input file, populating task metrics */
int analyzeDocument(DocumentTask *task);

/* Writes the individual analysis report file for a processed document */
int generateAnalysisReport(DocumentTask *task);

/* POSIX thread entry point: processes one file and generates its report */
void *processFileThread(void *arg);

/* Displays the final processing summary for all tasks on the terminal */
void displayProcessingSummary(const DocumentTask *tasks, int taskCount);

/* Converts a ProcessingStatus enum to a human-readable string */
const char *statusToString(ProcessingStatus status);

#endif /* DOCUMENT_ANALYZER_H */
