#include "document_analyzer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(const char *programName) {
    fprintf(stderr, "Usage: %s \"<keyword>\" <file1> [file2 ...]\n", programName);
    fprintf(stderr, "Example: %s \"IoT\" document1.txt document2.txt document3.txt\n", programName);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[ERROR] Insufficient arguments provided.\n");
        printUsage(argv[0]);
        return 1;
    }

    const char *keyword = argv[1];
    if (keyword[0] == '\0') {
        fprintf(stderr, "[ERROR] Search keyword cannot be empty.\n");
        printUsage(argv[0]);
        return 1;
    }

    int fileCount = argc - 2;

    /* Allocate independent task structs and thread handles for each input file */
    DocumentTask *tasks = (DocumentTask *)calloc((size_t)fileCount, sizeof(DocumentTask));
    pthread_t *threads = (pthread_t *)calloc((size_t)fileCount, sizeof(pthread_t));
    if (!tasks || !threads) {
        fprintf(stderr, "[CRITICAL ERROR] Failed to allocate memory for thread management.\n");
        free(tasks);
        free(threads);
        return 1;
    }

    printf("===== MULTI-THREADED DOCUMENT ANALYZER =====\n\n");
    printf("Search Keyword: %s\n", keyword);
    printf("Number of Input Files: %d\n\n", fileCount);

    /* Initialize task arguments and launch one POSIX thread per input file */
    for (int i = 0; i < fileCount; i++) {
        initThreadTask(&tasks[i], i + 1, argv[i + 2], keyword);

        int rc = pthread_create(&threads[i], NULL, processFileThread, &tasks[i]);
        if (rc != 0) {
            tasks[i].threadCreated = 0;
            tasks[i].status = STATUS_THREAD_ERROR;
            snprintf(tasks[i].errorMessage, sizeof(tasks[i].errorMessage),
                     "pthread_create() failed with error code %d (%s)", rc, strerror(rc));
            fprintf(stderr, "[Main] Error creating Thread %d for %s: %s\n",
                    tasks[i].threadNum, tasks[i].inputFilename, tasks[i].errorMessage);
        } else {
            tasks[i].threadCreated = 1;
            printf("[Main] Created Thread %d for %s\n",
                   tasks[i].threadNum, tasks[i].inputFilename);
            fflush(stdout);
        }
    }

    /* Join every thread that was successfully created before exiting */
    for (int i = 0; i < fileCount; i++) {
        if (tasks[i].threadCreated) {
            int joinRc = pthread_join(threads[i], NULL);
            if (joinRc != 0) {
                fprintf(stderr, "[Main] Warning: pthread_join() failed for Thread %d (%s)\n",
                        tasks[i].threadNum, strerror(joinRc));
            }
        }
    }

    displayProcessingSummary(tasks, fileCount);

    int hasErrors = 0;
    for (int i = 0; i < fileCount; i++) {
        if (tasks[i].status != STATUS_SUCCESS) {
            hasErrors = 1;
        }
        freeThreadTask(&tasks[i]);
    }

    free(tasks);
    free(threads);

    return hasErrors ? 1 : 0;
}
