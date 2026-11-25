#include "student_records.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * SAFE INPUT HELPER FUNCTIONS (PREVENT BUFFER OVERFLOWS & SCANF LEFTOVERS)
 * ============================================================================ */

static void trimNewline(char *str) {
    if (str) {
        str[strcspn(str, "\r\n")] = '\0';
    }
}

static int isBlankString(const char *str) {
    if (!str) return 1;
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

/* Reads a non-empty line without pipe '|' characters (reserved as file delimiter) */
static void readNonEmptyString(const char *prompt, char *buffer, size_t size) {
    while (1) {
        printf("%s", prompt);
        if (!fgets(buffer, (int)size, stdin)) {
            buffer[0] = '\0';
            return;
        }
        trimNewline(buffer);

        if (isBlankString(buffer)) {
            printf("[ERROR] Input cannot be empty. Please try again.\n");
            continue;
        }
        if (strchr(buffer, '|') != NULL) {
            printf("[ERROR] Character '|' is reserved and cannot be used.\n");
            continue;
        }
        return;
    }
}

/* Reads an optional line (pressing Enter keeps the existing value) */
static void readOptionalString(const char *prompt, char *buffer, size_t size) {
    while (1) {
        printf("%s", prompt);
        if (!fgets(buffer, (int)size, stdin)) {
            buffer[0] = '\0';
            return;
        }
        trimNewline(buffer);

        if (strchr(buffer, '|') != NULL) {
            printf("[ERROR] Character '|' is reserved and cannot be used.\n");
            continue;
        }
        return;
    }
}

/* Reads a validated integer within [minVal, maxVal] */
static int readValidatedInt(const char *prompt, int minVal, int maxVal) {
    char line[64];
    char *endPtr;

    while (1) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) {
            return minVal;
        }
        trimNewline(line);

        if (isBlankString(line)) {
            printf("[ERROR] Please enter a valid integer (%d-%d).\n", minVal, maxVal);
            continue;
        }

        long val = strtol(line, &endPtr, 10);
        while (*endPtr && isspace((unsigned char)*endPtr)) {
            endPtr++;
        }

        if (*endPtr != '\0' || val < minVal || val > maxVal) {
            printf("[ERROR] Invalid input. Enter a whole number between %d and %d.\n",
                   minVal, maxVal);
            continue;
        }
        return (int)val;
    }
}

/* Reads a validated floating-point number within [minVal, maxVal] */
static double readValidatedDouble(const char *prompt, double minVal, double maxVal) {
    char line[64];
    char *endPtr;

    while (1) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) {
            return minVal;
        }
        trimNewline(line);

        if (isBlankString(line)) {
            printf("[ERROR] Please enter a valid numeric value (%.2f - %.2f).\n", minVal, maxVal);
            continue;
        }

        double val = strtod(line, &endPtr);
        while (*endPtr && isspace((unsigned char)*endPtr)) {
            endPtr++;
        }

        if (*endPtr != '\0' || val < minVal || val > maxVal) {
            printf("[ERROR] Invalid value. Enter a number between %.2f and %.2f.\n",
                   minVal, maxVal);
            continue;
        }
        return val;
    }
}

static int confirmAction(const char *prompt) {
    char line[32];
    while (1) {
        printf("%s (y/n): ", prompt);
        if (!fgets(line, sizeof(line), stdin)) {
            return 0;
        }
        trimNewline(line);
        if (strcmp(line, "y") == 0 || strcmp(line, "Y") == 0) return 1;
        if (strcmp(line, "n") == 0 || strcmp(line, "N") == 0) return 0;
        printf("[ERROR] Invalid response. Enter 'y' or 'n'.\n");
    }
}

/* ============================================================================
 * MENU HANDLERS
 * ============================================================================ */

static void handleRegisterStudent(StudentDatabase *db) {
    char id[MAX_ID_LEN];
    char firstName[MAX_NAME_LEN];
    char lastName[MAX_NAME_LEN];
    char programme[MAX_PROGRAMME_LEN];

    printf("\n--- REGISTER NEW STUDENT ---\n");
    readNonEmptyString("Enter unique Student ID       : ", id, sizeof(id));

    if (findStudentIndexById(db, id) != -1) {
        printf("[ERROR] Student ID '%s' already exists. Duplicate IDs are not allowed.\n", id);
        return;
    }

    readNonEmptyString("Enter First Name              : ", firstName, sizeof(firstName));
    readNonEmptyString("Enter Last Name               : ", lastName, sizeof(lastName));
    readNonEmptyString("Enter Programme of Study      : ", programme, sizeof(programme));
    int year = readValidatedInt("Enter Year of Study (1-6)     : ", 1, 6);

    if (registerStudent(db, id, firstName, lastName, programme, year)) {
        printf("[SUCCESS] Student '%s %s' (ID: %s) registered successfully.\n",
               firstName, lastName, id);
    }
}

static void handleUpdateStudent(StudentDatabase *db) {
    char id[MAX_ID_LEN];
    char newFirst[MAX_NAME_LEN];
    char newLast[MAX_NAME_LEN];
    char newProg[MAX_PROGRAMME_LEN];
    char yearInput[16];

    printf("\n--- UPDATE STUDENT INFORMATION ---\n");
    readNonEmptyString("Enter Student ID to update    : ", id, sizeof(id));

    int idx = findStudentIndexById(db, id);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' was not found.\n", id);
        return;
    }

    printf("Current record: %s %s | %s | Year %d\n",
           db->records[idx].firstName, db->records[idx].lastName,
           db->records[idx].programme, db->records[idx].yearOfStudy);
    printf("(Press [Enter] to keep current value for any field)\n");

    readOptionalString("New First Name                : ", newFirst, sizeof(newFirst));
    readOptionalString("New Last Name                 : ", newLast, sizeof(newLast));
    readOptionalString("New Programme of Study        : ", newProg, sizeof(newProg));

    int newYear = db->records[idx].yearOfStudy;
    readOptionalString("New Year of Study (1-6)       : ", yearInput, sizeof(yearInput));
    if (!isBlankString(yearInput)) {
        char *endPtr;
        long parsed = strtol(yearInput, &endPtr, 10);
        if (*endPtr == '\0' && parsed >= 1 && parsed <= 6) {
            newYear = (int)parsed;
        } else {
            printf("[WARNING] Invalid year entered; keeping existing Year %d.\n", newYear);
        }
    }

    if (updateStudentInfo(db, id, newFirst, newLast, newProg, newYear)) {
        printf("[SUCCESS] Student record '%s' updated successfully.\n", id);
    }
}

static void handleDeleteStudent(StudentDatabase *db) {
    char id[MAX_ID_LEN];
    printf("\n--- DELETE STUDENT RECORD ---\n");
    readNonEmptyString("Enter Student ID to delete    : ", id, sizeof(id));

    int idx = findStudentIndexById(db, id);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' was not found.\n", id);
        return;
    }

    printf("Target Student: %s %s (%s)\n",
           db->records[idx].firstName, db->records[idx].lastName, id);

    if (confirmAction("Are you sure you want to permanently delete this student record?")) {
        if (deleteStudent(db, id)) {
            printf("[SUCCESS] Student record '%s' deleted and memory freed.\n", id);
        }
    } else {
        printf("[INFO] Deletion cancelled.\n");
    }
}

static void handleAddCourse(StudentDatabase *db) {
    char id[MAX_ID_LEN];
    char code[MAX_CODE_LEN];
    char name[MAX_COURSE_LEN];

    printf("\n--- ADD COURSE TO STUDENT RECORD ---\n");
    readNonEmptyString("Enter Student ID              : ", id, sizeof(id));

    if (findStudentIndexById(db, id) == -1) {
        printf("[ERROR] Student ID '%s' does not exist.\n", id);
        return;
    }

    readNonEmptyString("Enter Course Code (e.g. CS101): ", code, sizeof(code));
    readNonEmptyString("Enter Course Name             : ", name, sizeof(name));
    double maxMarks = readValidatedDouble("Enter Maximum Marks (1-1000)  : ", 1.0, 1000.0);
    double marks = readValidatedDouble("Enter Marks Obtained          : ", 0.0, maxMarks);

    if (addCourseToStudent(db, id, code, name, marks, maxMarks)) {
        printf("[SUCCESS] Course '%s' added to student '%s'.\n", code, id);
    }
}

static void handleUpdateCourseMarks(StudentDatabase *db) {
    char id[MAX_ID_LEN];
    char code[MAX_CODE_LEN];

    printf("\n--- UPDATE COURSE MARKS ---\n");
    readNonEmptyString("Enter Student ID              : ", id, sizeof(id));

    int idx = findStudentIndexById(db, id);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' does not exist.\n", id);
        return;
    }

    if (db->records[idx].courseCount == 0) {
        printf("[INFO] Student '%s' has no registered courses to update.\n", id);
        return;
    }

    readNonEmptyString("Enter Course Code to update   : ", code, sizeof(code));

    /* Find course to display its allowed maximum marks */
    double maxAllowed = -1.0;
    for (int i = 0; i < db->records[idx].courseCount; i++) {
        if (strcmp(db->records[idx].courses[i].courseCode, code) == 0) {
            maxAllowed = db->records[idx].courses[i].maxMarks;
            break;
        }
    }
    if (maxAllowed < 0.0) {
        maxAllowed = 1000.0; /* Let updateCourseMarks report exact case-insensitive error if missing */
    }

    double newMarks = readValidatedDouble("Enter New Marks Obtained      : ", 0.0, maxAllowed);
    if (updateCourseMarks(db, id, code, newMarks)) {
        printf("[SUCCESS] Marks for course '%s' updated to %.2f.\n", code, newMarks);
    }
}

static void handleSearchMenu(const StudentDatabase *db) {
    printf("\n--- SEARCH STUDENT RECORDS ---\n");
    printf("  1. Search by Student ID\n");
    printf("  2. Search by First or Last Name\n");
    printf("  3. Search by Programme of Study\n");
    int opt = readValidatedInt("Select search option (1-3)    : ", 1, 3);

    char query[MAX_PROGRAMME_LEN];
    if (opt == 1) {
        readNonEmptyString("Enter Student ID              : ", query, sizeof(query));
        searchByStudentId(db, query);
    } else if (opt == 2) {
        readNonEmptyString("Enter First or Last Name      : ", query, sizeof(query));
        searchByStudentName(db, query);
    } else {
        readNonEmptyString("Enter Programme of Study      : ", query, sizeof(query));
        searchByProgramme(db, query);
    }
}

static void handleSortMenu(StudentDatabase *db) {
    if (db->count < 2) {
        printf("\n[INFO] Need at least 2 student records to perform sorting.\n");
        return;
    }

    printf("\n--- SORT STUDENT RECORDS ---\n");
    printf("  1. Sort by Student ID (Ascending)\n");
    printf("  2. Sort by Student Name (Alphabetical)\n");
    printf("  3. Sort by Overall Academic Average (Highest First)\n");
    printf("  4. Sort by Programme of Study (Alphabetical)\n");
    int opt = readValidatedInt("Select sort criterion (1-4)   : ", 1, 4);

    switch (opt) {
        case 1: sortByStudentId(db); break;
        case 2: sortByStudentName(db); break;
        case 3: sortByAcademicAverage(db); break;
        case 4: sortByProgramme(db); break;
    }

    printf("[SUCCESS] Student records sorted.\n");
    displayAllStudents(db);
}

static void displayMainMenu(void) {
    printf("\n======================================================================\n");
    printf("       STUDENT ACADEMIC RECORDS & PERFORMANCE ANALYTICS SYSTEM\n");
    printf("======================================================================\n");
    printf("  1. Register a new student\n");
    printf("  2. Display all registered students\n");
    printf("  3. View detailed information for a student\n");
    printf("  4. Update student information\n");
    printf("  5. Delete a student record\n");
    printf("  6. Add a course to a student's academic record\n");
    printf("  7. Update marks for a particular course\n");
    printf("  8. Display a student's academic performance\n");
    printf("  9. Search student records (ID / Name / Programme)\n");
    printf(" 10. Sort student records (ID / Name / Average / Programme)\n");
    printf(" 11. Generate academic performance analytics report\n");
    printf(" 12. Save records to file\n");
    printf(" 13. Save & Exit\n");
    printf("======================================================================\n");
}

int main(void) {
    StudentDatabase db;
    if (!initDatabase(&db)) {
        return 1;
    }

    printf("Initializing Student Academic Records System...\n");
    loadRecordsFromFile(&db, DATA_FILENAME);

    int running = 1;
    while (running) {
        displayMainMenu();
        int choice = readValidatedInt("Enter your choice (1-13)      : ", 1, 13);

        switch (choice) {
            case 1:
                handleRegisterStudent(&db);
                break;
            case 2:
                displayAllStudents(&db);
                break;
            case 3: {
                char id[MAX_ID_LEN];
                readNonEmptyString("Enter Student ID              : ", id, sizeof(id));
                viewStudentDetails(&db, id);
                break;
            }
            case 4:
                handleUpdateStudent(&db);
                break;
            case 5:
                handleDeleteStudent(&db);
                break;
            case 6:
                handleAddCourse(&db);
                break;
            case 7:
                handleUpdateCourseMarks(&db);
                break;
            case 8: {
                char id[MAX_ID_LEN];
                readNonEmptyString("Enter Student ID              : ", id, sizeof(id));
                displayStudentPerformance(&db, id);
                break;
            }
            case 9:
                handleSearchMenu(&db);
                break;
            case 10:
                handleSortMenu(&db);
                break;
            case 11:
                generateAcademicReport(&db, 0, NULL);
                if (db.count > 0 && confirmAction("Export this report to a text file?")) {
                    generateAcademicReport(&db, 1, REPORT_FILENAME);
                }
                break;
            case 12:
                if (saveRecordsToFile(&db, DATA_FILENAME)) {
                    printf("[SUCCESS] All records saved to '%s'.\n", DATA_FILENAME);
                }
                break;
            case 13:
                if (saveRecordsToFile(&db, DATA_FILENAME)) {
                    printf("[SUCCESS] Records saved to '%s'.\n", DATA_FILENAME);
                }
                freeDatabase(&db);
                printf("Dynamically allocated memory released. Goodbye!\n");
                running = 0;
                break;
        }
    }

    return 0;
}