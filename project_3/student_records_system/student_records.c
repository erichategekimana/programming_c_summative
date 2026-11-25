#include "student_records.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_DB_CAPACITY 4

/* ============================================================================
 * INTERNAL STRING HELPER (CASE-INSENSITIVE COMPARISON & SUBSTRING MATCH)
 * ============================================================================ */

static int strCaseCmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static int strContainsCaseInsensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle || *needle == '\0') {
        return 0;
    }
    size_t needleLen = strlen(needle);
    for (const char *p = haystack; *p != '\0'; p++) {
        size_t i = 0;
        while (i < needleLen && p[i] != '\0' &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == needleLen) {
            return 1;
        }
    }
    return 0;
}

/* ============================================================================
 * 1. DYNAMIC MEMORY MANAGEMENT & FILE PERSISTENCE
 * ============================================================================ */

int initDatabase(StudentDatabase *db) {
    if (!db) {
        return 0;
    }
    db->records = (Student *)calloc(INITIAL_DB_CAPACITY, sizeof(Student));
    if (!db->records) {
        fprintf(stderr, "[CRITICAL ERROR] Initial memory allocation failed.\n");
        db->count = 0;
        db->capacity = 0;
        return 0;
    }
    db->count = 0;
    db->capacity = INITIAL_DB_CAPACITY;
    return 1;
}

void freeDatabase(StudentDatabase *db) {
    if (!db) {
        return;
    }
    if (db->records != NULL) {
        for (int i = 0; i < db->count; i++) {
            if (db->records[i].courses != NULL) {
                free(db->records[i].courses);
                db->records[i].courses = NULL;
            }
        }
        free(db->records);
        db->records = NULL;
    }
    db->count = 0;
    db->capacity = 0;
}

static int ensureDatabaseCapacity(StudentDatabase *db) {
    if (db->count < db->capacity) {
        return 1;
    }
    int newCapacity = (db->capacity == 0) ? INITIAL_DB_CAPACITY : db->capacity * 2;
    Student *temp = (Student *)realloc(db->records, (size_t)newCapacity * sizeof(Student));
    if (!temp) {
        fprintf(stderr, "[ERROR] Memory reallocation failed while expanding student records.\n");
        return 0;
    }
    db->records = temp;
    db->capacity = newCapacity;
    return 1;
}

int saveRecordsToFile(const StudentDatabase *db, const char *filename) {
    if (!db || !filename) {
        return 0;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Unable to open file '%s' for writing.\n", filename);
        return 0;
    }

    fprintf(fp, "STUDENT_COUNT|%d\n", db->count);
    for (int i = 0; i < db->count; i++) {
        const Student *s = &db->records[i];
        fprintf(fp, "STUDENT|%s|%s|%s|%s|%d|%d\n",
                s->studentId, s->firstName, s->lastName,
                s->programme, s->yearOfStudy, s->courseCount);

        for (int j = 0; j < s->courseCount; j++) {
            const Course *c = &s->courses[j];
            fprintf(fp, "COURSE|%s|%s|%.2f|%.2f\n",
                    c->courseCode, c->courseName, c->marksObtained, c->maxMarks);
        }
    }

    fclose(fp);
    return 1;
}

int loadRecordsFromFile(StudentDatabase *db, const char *filename) {
    if (!db || !filename) {
        return 0;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[INFO] No existing data file '%s' found. Starting with an empty database.\n", filename);
        return 0;
    }

    char line[512];
    int expectedStudents = 0;
    if (!fgets(line, sizeof(line), fp) || sscanf(line, "STUDENT_COUNT|%d", &expectedStudents) != 1) {
        fprintf(stderr, "[ERROR] Corrupted or invalid header in '%s'.\n", filename);
        fclose(fp);
        return 0;
    }

    for (int i = 0; i < expectedStudents; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fprintf(stderr, "[ERROR] Unexpected end of file while reading student #%d.\n", i + 1);
            fclose(fp);
            return 0;
        }
        line[strcspn(line, "\r\n")] = '\0';

        char *tag = strtok(line, "|");
        char *id = strtok(NULL, "|");
        char *first = strtok(NULL, "|");
        char *last = strtok(NULL, "|");
        char *prog = strtok(NULL, "|");
        char *yearStr = strtok(NULL, "|");
        char *courseCountStr = strtok(NULL, "|");

        if (!tag || strcmp(tag, "STUDENT") != 0 || !id || !first || !last || !prog || !yearStr || !courseCountStr) {
            fprintf(stderr, "[ERROR] Malformed student record in '%s'.\n", filename);
            fclose(fp);
            return 0;
        }

        int year = atoi(yearStr);
        int numCourses = atoi(courseCountStr);

        if (!registerStudent(db, id, first, last, prog, year)) {
            fprintf(stderr, "[ERROR] Failed to reconstruct student '%s' in memory.\n", id);
            fclose(fp);
            return 0;
        }

        for (int j = 0; j < numCourses; j++) {
            if (!fgets(line, sizeof(line), fp)) {
                fprintf(stderr, "[ERROR] Missing course record for student '%s'.\n", id);
                fclose(fp);
                return 0;
            }
            line[strcspn(line, "\r\n")] = '\0';

            char *cTag = strtok(line, "|");
            char *cCode = strtok(NULL, "|");
            char *cName = strtok(NULL, "|");
            char *marksStr = strtok(NULL, "|");
            char *maxStr = strtok(NULL, "|");

            if (!cTag || strcmp(cTag, "COURSE") != 0 || !cCode || !cName || !marksStr || !maxStr) {
                fprintf(stderr, "[ERROR] Malformed course record for student '%s'.\n", id);
                fclose(fp);
                return 0;
            }

            double marks = atof(marksStr);
            double maxMarks = atof(maxStr);
            if (!addCourseToStudent(db, id, cCode, cName, marks, maxMarks)) {
                fprintf(stderr, "[ERROR] Failed to reconstruct course '%s' for student '%s'.\n", cCode, id);
                fclose(fp);
                return 0;
            }
        }
    }

    fclose(fp);
    printf("[INFO] Successfully loaded %d student record(s) from '%s'.\n", db->count, filename);
    return 1;
}

/* ============================================================================
 * 2. STUDENT & COURSE CRUD OPERATIONS
 * ============================================================================ */

int findStudentIndexById(const StudentDatabase *db, const char *studentId) {
    if (!db || !studentId) {
        return -1;
    }
    for (int i = 0; i < db->count; i++) {
        if (strCaseCmp(db->records[i].studentId, studentId) == 0) {
            return i;
        }
    }
    return -1;
}

int registerStudent(StudentDatabase *db, const char *id, const char *firstName,
                    const char *lastName, const char *programme, int yearOfStudy) {
    if (!db || !id || !firstName || !lastName || !programme) {
        return 0;
    }

    if (findStudentIndexById(db, id) != -1) {
        printf("[ERROR] Duplicate Student ID '%s'. Registration rejected.\n", id);
        return 0;
    }

    if (!ensureDatabaseCapacity(db)) {
        return 0;
    }

    Student *newStudent = &db->records[db->count];
    strncpy(newStudent->studentId, id, MAX_ID_LEN - 1);
    newStudent->studentId[MAX_ID_LEN - 1] = '\0';

    strncpy(newStudent->firstName, firstName, MAX_NAME_LEN - 1);
    newStudent->firstName[MAX_NAME_LEN - 1] = '\0';

    strncpy(newStudent->lastName, lastName, MAX_NAME_LEN - 1);
    newStudent->lastName[MAX_NAME_LEN - 1] = '\0';

    strncpy(newStudent->programme, programme, MAX_PROGRAMME_LEN - 1);
    newStudent->programme[MAX_PROGRAMME_LEN - 1] = '\0';

    newStudent->yearOfStudy = yearOfStudy;
    newStudent->courseCount = 0;
    newStudent->courses = NULL;

    db->count++;
    return 1;
}

double calculateStudentAverage(const Student *student) {
    if (!student || student->courseCount <= 0 || !student->courses) {
        return 0.0; /* Prevents division by zero when no courses are registered */
    }

    double totalObtained = 0.0;
    double totalMax = 0.0;

    for (int i = 0; i < student->courseCount; i++) {
        totalObtained += student->courses[i].marksObtained;
        totalMax += student->courses[i].maxMarks;
    }

    if (totalMax <= 0.0) {
        return 0.0;
    }

    return (totalObtained / totalMax) * 100.0;
}

void displayAllStudents(const StudentDatabase *db) {
    if (!db || db->count == 0) {
        printf("\n[INFO] No student records currently registered.\n");
        return;
    }

    printf("\n======================================================================================\n");
    printf("%-12s %-24s %-24s %-6s %-8s %-9s\n",
           "STUDENT ID", "FULL NAME", "PROGRAMME", "YEAR", "COURSES", "AVG (%)");
    printf("======================================================================================\n");

    for (int i = 0; i < db->count; i++) {
        const Student *s = &db->records[i];
        char fullName[MAX_NAME_LEN * 2 + 2];
        snprintf(fullName, sizeof(fullName), "%s %s", s->firstName, s->lastName);

        if (s->courseCount == 0) {
            printf("%-12s %-24s %-24s %-6d %-8d %-9s\n",
                   s->studentId, fullName, s->programme, s->yearOfStudy, 0, "N/A");
        } else {
            double avg = calculateStudentAverage(s);
            printf("%-12s %-24s %-24s %-6d %-8d %6.2f%%\n",
                   s->studentId, fullName, s->programme, s->yearOfStudy, s->courseCount, avg);
        }
    }
    printf("======================================================================================\n");
}

void viewStudentDetails(const StudentDatabase *db, const char *studentId) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Student with ID '%s' was not found.\n", studentId);
        return;
    }

    const Student *s = &db->records[idx];
    printf("\n----------------------------------------------------------------------\n");
    printf("STUDENT PROFILE & ACADEMIC RECORD\n");
    printf("----------------------------------------------------------------------\n");
    printf("  Student ID         : %s\n", s->studentId);
    printf("  Name               : %s %s\n", s->firstName, s->lastName);
    printf("  Programme of Study : %s\n", s->programme);
    printf("  Year of Study      : Year %d\n", s->yearOfStudy);
    printf("  Registered Courses : %d\n", s->courseCount);

    if (s->courseCount == 0) {
        printf("  Overall Average    : N/A (No courses registered)\n");
    } else {
        double avg = calculateStudentAverage(s);
        printf("  Overall Average    : %.2f%% (%s)\n", avg, (avg >= 50.0) ? "PASS" : "FAIL");
        printf("\n  %-12s %-28s %-10s %-10s %-10s\n",
               "CODE", "COURSE NAME", "MARKS", "MAX", "PCT (%)");
        printf("  --------------------------------------------------------------------\n");
        for (int i = 0; i < s->courseCount; i++) {
            const Course *c = &s->courses[i];
            double pct = (c->maxMarks > 0.0) ? (c->marksObtained / c->maxMarks) * 100.0 : 0.0;
            printf("  %-12s %-28s %-10.2f %-10.2f %6.2f%%\n",
                   c->courseCode, c->courseName, c->marksObtained, c->maxMarks, pct);
        }
    }
    printf("----------------------------------------------------------------------\n");
}

int updateStudentInfo(StudentDatabase *db, const char *studentId,
                      const char *newFirst, const char *newLast,
                      const char *newProgramme, int newYear) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Cannot update: Student ID '%s' does not exist.\n", studentId);
        return 0;
    }

    Student *s = &db->records[idx];
    if (newFirst && strlen(newFirst) > 0) {
        strncpy(s->firstName, newFirst, MAX_NAME_LEN - 1);
        s->firstName[MAX_NAME_LEN - 1] = '\0';
    }
    if (newLast && strlen(newLast) > 0) {
        strncpy(s->lastName, newLast, MAX_NAME_LEN - 1);
        s->lastName[MAX_NAME_LEN - 1] = '\0';
    }
    if (newProgramme && strlen(newProgramme) > 0) {
        strncpy(s->programme, newProgramme, MAX_PROGRAMME_LEN - 1);
        s->programme[MAX_PROGRAMME_LEN - 1] = '\0';
    }
    if (newYear >= 1 && newYear <= 10) {
        s->yearOfStudy = newYear;
    }

    return 1;
}

int deleteStudent(StudentDatabase *db, const char *studentId) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Cannot delete: Student ID '%s' does not exist.\n", studentId);
        return 0;
    }

    /* Release dynamically allocated course list for the deleted student */
    if (db->records[idx].courses != NULL) {
        free(db->records[idx].courses);
        db->records[idx].courses = NULL;
    }

    /* Shift remaining student records left */
    for (int i = idx; i < db->count - 1; i++) {
        db->records[i] = db->records[i + 1];
    }
    db->count--;

    /* Shrink allocated array using realloc() when count drops significantly */
    if (db->count > 0 && db->count <= db->capacity / 4 && db->capacity > INITIAL_DB_CAPACITY) {
        int newCapacity = db->capacity / 2;
        Student *shrunk = (Student *)realloc(db->records, (size_t)newCapacity * sizeof(Student));
        if (shrunk != NULL) {
            db->records = shrunk;
            db->capacity = newCapacity;
        }
    }

    return 1;
}

int addCourseToStudent(StudentDatabase *db, const char *studentId,
                       const char *code, const char *name,
                       double marks, double maxMarks) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' not found.\n", studentId);
        return 0;
    }

    if (maxMarks <= 0.0 || marks < 0.0 || marks > maxMarks) {
        printf("[ERROR] Invalid marks (%.2f / %.2f). Marks must be between 0 and max marks.\n",
               marks, maxMarks);
        return 0;
    }

    Student *s = &db->records[idx];

    /* Prevent duplicate course code for the same student */
    for (int i = 0; i < s->courseCount; i++) {
        if (strCaseCmp(s->courses[i].courseCode, code) == 0) {
            printf("[ERROR] Course '%s' is already registered for student '%s'.\n", code, studentId);
            return 0;
        }
    }

    Course *updatedCourses = (Course *)realloc(s->courses, (size_t)(s->courseCount + 1) * sizeof(Course));
    if (!updatedCourses) {
        fprintf(stderr, "[ERROR] Memory allocation failed while adding course '%s'.\n", code);
        return 0;
    }

    s->courses = updatedCourses;
    Course *newCourse = &s->courses[s->courseCount];

    strncpy(newCourse->courseCode, code, MAX_CODE_LEN - 1);
    newCourse->courseCode[MAX_CODE_LEN - 1] = '\0';

    strncpy(newCourse->courseName, name, MAX_COURSE_LEN - 1);
    newCourse->courseName[MAX_COURSE_LEN - 1] = '\0';

    newCourse->marksObtained = marks;
    newCourse->maxMarks = maxMarks;

    s->courseCount++;
    return 1;
}

int updateCourseMarks(StudentDatabase *db, const char *studentId,
                      const char *courseCode, double newMarks) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' not found.\n", studentId);
        return 0;
    }

    Student *s = &db->records[idx];
    for (int i = 0; i < s->courseCount; i++) {
        if (strCaseCmp(s->courses[i].courseCode, courseCode) == 0) {
            if (newMarks < 0.0 || newMarks > s->courses[i].maxMarks) {
                printf("[ERROR] Invalid marks %.2f. Allowed range for %s is 0.00 to %.2f.\n",
                       newMarks, courseCode, s->courses[i].maxMarks);
                return 0;
            }
            s->courses[i].marksObtained = newMarks;
            return 1;
        }
    }

    printf("[ERROR] Course '%s' is not registered under student '%s'.\n", courseCode, studentId);
    return 0;
}

void displayStudentPerformance(const StudentDatabase *db, const char *studentId) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[ERROR] Student ID '%s' not found.\n", studentId);
        return;
    }

    const Student *s = &db->records[idx];
    printf("\n----------------------------------------------------------------------\n");
    printf("PERFORMANCE SUMMARY: %s %s (%s)\n", s->firstName, s->lastName, s->studentId);
    printf("----------------------------------------------------------------------\n");

    if (s->courseCount == 0) {
        printf("No courses registered yet. Academic average cannot be computed.\n");
        return;
    }

    int passedCourses = 0;
    for (int i = 0; i < s->courseCount; i++) {
        const Course *c = &s->courses[i];
        double pct = (c->marksObtained / c->maxMarks) * 100.0;
        const char *status = (pct >= 50.0) ? "PASS" : "FAIL";
        if (pct >= 50.0) {
            passedCourses++;
        }
        printf("  %-10s | %-25s | %6.2f / %6.2f (%6.2f%%) [%s]\n",
               c->courseCode, c->courseName, c->marksObtained, c->maxMarks, pct, status);
    }

    double overallAvg = calculateStudentAverage(s);
    printf("----------------------------------------------------------------------\n");
    printf("  Courses Passed  : %d / %d\n", passedCourses, s->courseCount);
    printf("  Overall Average : %.2f%% (%s)\n", overallAvg, (overallAvg >= 50.0) ? "PASSING" : "FAILING");
    printf("----------------------------------------------------------------------\n");
}

/* ============================================================================
 * 3. MANUAL SEARCHING ALGORITHMS
 * ============================================================================ */

void searchByStudentId(const StudentDatabase *db, const char *studentId) {
    int idx = findStudentIndexById(db, studentId);
    if (idx == -1) {
        printf("[INFO] No student found matching ID '%s'.\n", studentId);
        return;
    }
    viewStudentDetails(db, db->records[idx].studentId);
}

void searchByStudentName(const StudentDatabase *db, const char *nameQuery) {
    if (!db || db->count == 0) {
        printf("[INFO] Database is empty.\n");
        return;
    }

    int matches = 0;
    printf("\nSearch Results for Name matching '%s':\n", nameQuery);
    printf("----------------------------------------------------------------------\n");
    for (int i = 0; i < db->count; i++) {
        const Student *s = &db->records[i];
        if (strContainsCaseInsensitive(s->firstName, nameQuery) ||
            strContainsCaseInsensitive(s->lastName, nameQuery)) {
            printf("  ID: %-10s | Name: %-22s | Programme: %-20s | Year: %d\n",
                   s->studentId,
                   strcat(strcpy((char[105]){0}, s->firstName), strcat(strcpy((char[55]){0}, " "), s->lastName)),
                   s->programme, s->yearOfStudy);
            matches++;
        }
    }
    if (matches == 0) {
        printf("  No matching student records found.\n");
    }
    printf("----------------------------------------------------------------------\n");
}

void searchByProgramme(const StudentDatabase *db, const char *programmeQuery) {
    if (!db || db->count == 0) {
        printf("[INFO] Database is empty.\n");
        return;
    }

    int matches = 0;
    printf("\nSearch Results for Programme matching '%s':\n", programmeQuery);
    printf("----------------------------------------------------------------------\n");
    for (int i = 0; i < db->count; i++) {
        const Student *s = &db->records[i];
        if (strContainsCaseInsensitive(s->programme, programmeQuery)) {
            printf("  ID: %-10s | Name: %s %s | Programme: %s | Year: %d\n",
                   s->studentId, s->firstName, s->lastName, s->programme, s->yearOfStudy);
            matches++;
        }
    }
    if (matches == 0) {
        printf("  No students found in programme matching '%s'.\n", programmeQuery);
    }
    printf("----------------------------------------------------------------------\n");
}

/* ============================================================================
 * 4. MANUAL SORTING ALGORITHMS (SELECTION SORT - NO BUILT-IN QSORT)
 * ============================================================================ */

static void swapStudents(Student *a, Student *b) {
    Student temp = *a;
    *a = *b;
    *b = temp;
}

void sortByStudentId(StudentDatabase *db) {
    if (!db || db->count < 2) return;
    for (int i = 0; i < db->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < db->count; j++) {
            if (strCaseCmp(db->records[j].studentId, db->records[minIdx].studentId) < 0) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            swapStudents(&db->records[i], &db->records[minIdx]);
        }
    }
}

void sortByStudentName(StudentDatabase *db) {
    if (!db || db->count < 2) return;
    for (int i = 0; i < db->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < db->count; j++) {
            int cmpLast = strCaseCmp(db->records[j].lastName, db->records[minIdx].lastName);
            if (cmpLast < 0 || (cmpLast == 0 &&
                strCaseCmp(db->records[j].firstName, db->records[minIdx].firstName) < 0)) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            swapStudents(&db->records[i], &db->records[minIdx]);
        }
    }
}

void sortByAcademicAverage(StudentDatabase *db) {
    if (!db || db->count < 2) return;
    /* Descending order: highest academic average first */
    for (int i = 0; i < db->count - 1; i++) {
        int maxIdx = i;
        double maxAvg = (db->records[maxIdx].courseCount > 0)
                        ? calculateStudentAverage(&db->records[maxIdx]) : -1.0;

        for (int j = i + 1; j < db->count; j++) {
            double currAvg = (db->records[j].courseCount > 0)
                             ? calculateStudentAverage(&db->records[j]) : -1.0;
            if (currAvg > maxAvg) {
                maxIdx = j;
                maxAvg = currAvg;
            }
        }
        if (maxIdx != i) {
            swapStudents(&db->records[i], &db->records[maxIdx]);
        }
    }
}

void sortByProgramme(StudentDatabase *db) {
    if (!db || db->count < 2) return;
    for (int i = 0; i < db->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < db->count; j++) {
            int cmpProg = strCaseCmp(db->records[j].programme, db->records[minIdx].programme);
            if (cmpProg < 0 || (cmpProg == 0 &&
                strCaseCmp(db->records[j].studentId, db->records[minIdx].studentId) < 0)) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            swapStudents(&db->records[i], &db->records[minIdx]);
        }
    }
}

/* ============================================================================
 * 5. ACADEMIC PERFORMANCE ANALYTICS & REPORT GENERATION
 * ============================================================================ */

void generateAcademicReport(const StudentDatabase *db, int exportToFile, const char *filename) {
    if (!db || db->count == 0) {
        printf("\n[INFO] Cannot generate report: No student records exist.\n");
        return;
    }

    FILE *out = stdout;
    if (exportToFile && filename) {
        out = fopen(filename, "w");
        if (!out) {
            fprintf(stderr, "[ERROR] Unable to open '%s' to export report.\n", filename);
            return;
        }
    }

    int totalStudents = db->count;
    int totalCoursesRegistered = 0;
    int studentsWithCourses = 0;
    double sumStudentAverages = 0.0;

    int highestIdx = -1;
    int lowestIdx = -1;
    double highestAvg = -1.0;
    double lowestAvg = 101.0;

    int passingStudents = 0;
    int failingStudents = 0;

    for (int i = 0; i < db->count; i++) {
        const Student *s = &db->records[i];
        totalCoursesRegistered += s->courseCount;

        if (s->courseCount > 0) {
            double avg = calculateStudentAverage(s);
            studentsWithCourses++;
            sumStudentAverages += avg;

            if (avg >= 50.0) {
                passingStudents++;
            } else {
                failingStudents++;
            }

            if (avg > highestAvg) {
                highestAvg = avg;
                highestIdx = i;
            }
            if (avg < lowestAvg) {
                lowestAvg = avg;
                lowestIdx = i;
            }
        }
    }

    double overallClassAvg = (studentsWithCourses > 0)
                             ? (sumStudentAverages / studentsWithCourses) : 0.0;

    fprintf(out, "\n======================================================================\n");
    fprintf(out, "              ACADEMIC PERFORMANCE ANALYTICS REPORT\n");
    fprintf(out, "======================================================================\n");
    fprintf(out, "Total Registered Students        : %d\n", totalStudents);
    fprintf(out, "Total Courses Registered         : %d\n", totalCoursesRegistered);
    fprintf(out, "Overall Average Performance      : %.2f%% (across %d graded student(s))\n",
            overallClassAvg, studentsWithCourses);

    if (highestIdx != -1) {
        fprintf(out, "Highest Academic Average         : %.2f%% (%s %s - %s)\n",
                highestAvg, db->records[highestIdx].firstName,
                db->records[highestIdx].lastName, db->records[highestIdx].studentId);
    } else {
        fprintf(out, "Highest Academic Average         : N/A\n");
    }

    if (lowestIdx != -1) {
        fprintf(out, "Lowest Academic Average          : %.2f%% (%s %s - %s)\n",
                lowestAvg, db->records[lowestIdx].firstName,
                db->records[lowestIdx].lastName, db->records[lowestIdx].studentId);
    } else {
        fprintf(out, "Lowest Academic Average          : N/A\n");
    }

    fprintf(out, "Students Passing (>= 50%%)        : %d\n", passingStudents);
    fprintf(out, "Students Failing (< 50%%)         : %d\n", failingStudents);

    /* Aggregate performance per unique course code */
    fprintf(out, "\n----------------------------------------------------------------------\n");
    fprintf(out, "AVERAGE PERFORMANCE PER COURSE\n");
    fprintf(out, "----------------------------------------------------------------------\n");
    fprintf(out, "%-12s %-30s %-10s %-12s\n", "COURSE CODE", "COURSE NAME", "ENROLLED", "AVG (%)");
    fprintf(out, "----------------------------------------------------------------------\n");

    if (totalCoursesRegistered == 0) {
        fprintf(out, "No course records available.\n");
    } else {
        char seenCodes[200][MAX_CODE_LEN];
        int seenCount = 0;

        for (int i = 0; i < db->count; i++) {
            const Student *s = &db->records[i];
            for (int j = 0; j < s->courseCount; j++) {
                const char *code = s->courses[j].courseCode;
                int alreadyProcessed = 0;
                for (int k = 0; k < seenCount; k++) {
                    if (strCaseCmp(seenCodes[k], code) == 0) {
                        alreadyProcessed = 1;
                        break;
                    }
                }
                if (alreadyProcessed) continue;

                if (seenCount < 200) {
                    strncpy(seenCodes[seenCount], code, MAX_CODE_LEN - 1);
                    seenCodes[seenCount][MAX_CODE_LEN - 1] = '\0';
                    seenCount++;
                }

                double coursePctSum = 0.0;
                int enrolled = 0;
                for (int x = 0; x < db->count; x++) {
                    for (int y = 0; y < db->records[x].courseCount; y++) {
                        if (strCaseCmp(db->records[x].courses[y].courseCode, code) == 0) {
                            double pct = (db->records[x].courses[y].marksObtained /
                                          db->records[x].courses[y].maxMarks) * 100.0;
                            coursePctSum += pct;
                            enrolled++;
                        }
                    }
                }
                fprintf(out, "%-12s %-30s %-10d %6.2f%%\n",
                        code, s->courses[j].courseName, enrolled,
                        (enrolled > 0) ? (coursePctSum / enrolled) : 0.0);
            }
        }
    }

    /* Count students per unique programme of study */
    fprintf(out, "\n----------------------------------------------------------------------\n");
    fprintf(out, "ENROLLMENT BY PROGRAMME OF STUDY\n");
    fprintf(out, "----------------------------------------------------------------------\n");
    char seenProgs[100][MAX_PROGRAMME_LEN];
    int progCount = 0;

    for (int i = 0; i < db->count; i++) {
        const char *prog = db->records[i].programme;
        int alreadyCounted = 0;
        for (int k = 0; k < progCount; k++) {
            if (strCaseCmp(seenProgs[k], prog) == 0) {
                alreadyCounted = 1;
                break;
            }
        }
        if (alreadyCounted) continue;

        if (progCount < 100) {
            strncpy(seenProgs[progCount], prog, MAX_PROGRAMME_LEN - 1);
            seenProgs[progCount][MAX_PROGRAMME_LEN - 1] = '\0';
            progCount++;
        }

        int countInProg = 0;
        for (int x = 0; x < db->count; x++) {
            if (strCaseCmp(db->records[x].programme, prog) == 0) {
                countInProg++;
            }
        }
        fprintf(out, "  %-35s : %d student(s)\n", prog, countInProg);
    }
    fprintf(out, "======================================================================\n");

    if (exportToFile && filename) {
        fclose(out);
        printf("[SUCCESS] Academic performance report exported to '%s'.\n", filename);
    }
}