#ifndef STUDENT_RECORDS_H
#define STUDENT_RECORDS_H

#include <stddef.h>

/* ============================================================================
 * SYSTEM CONSTANTS
 * ============================================================================ */
#define MAX_ID_LEN        20
#define MAX_NAME_LEN      50
#define MAX_PROGRAMME_LEN 60
#define MAX_CODE_LEN      16
#define MAX_COURSE_LEN    60
#define DATA_FILENAME     "students_data.dat"
#define REPORT_FILENAME   "academic_performance_report.txt"

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/* Represents a single course taken by a student */
typedef struct {
    char courseCode[MAX_CODE_LEN];
    char courseName[MAX_COURSE_LEN];
    double marksObtained;
    double maxMarks;
} Course;

/* Represents a student with a dynamically allocated array of courses */
typedef struct {
    char studentId[MAX_ID_LEN];
    char firstName[MAX_NAME_LEN];
    char lastName[MAX_NAME_LEN];
    char programme[MAX_PROGRAMME_LEN];
    int yearOfStudy;
    int courseCount;
    Course *courses;  /* Dynamically allocated via malloc/realloc */
} Student;

/* Manages the dynamic array of Student records */
typedef struct {
    Student *records; /* Dynamically allocated via calloc/realloc */
    int count;
    int capacity;
} StudentDatabase;

/* ============================================================================
 * MEMORY MANAGEMENT & PERSISTENCE FUNCTIONS
 * ============================================================================ */
int initDatabase(StudentDatabase *db);
void freeDatabase(StudentDatabase *db);
int saveRecordsToFile(const StudentDatabase *db, const char *filename);
int loadRecordsFromFile(StudentDatabase *db, const char *filename);

/* ============================================================================
 * STUDENT & COURSE CRUD OPERATIONS
 * ============================================================================ */
int findStudentIndexById(const StudentDatabase *db, const char *studentId);
int registerStudent(StudentDatabase *db, const char *id, const char *firstName,
                    const char *lastName, const char *programme, int yearOfStudy);
void displayAllStudents(const StudentDatabase *db);
void viewStudentDetails(const StudentDatabase *db, const char *studentId);
int updateStudentInfo(StudentDatabase *db, const char *studentId,
                      const char *newFirst, const char *newLast,
                      const char *newProgramme, int newYear);
int deleteStudent(StudentDatabase *db, const char *studentId);
int addCourseToStudent(StudentDatabase *db, const char *studentId,
                       const char *code, const char *name,
                       double marks, double maxMarks);
int updateCourseMarks(StudentDatabase *db, const char *studentId,
                      const char *courseCode, double newMarks);
double calculateStudentAverage(const Student *student);
void displayStudentPerformance(const StudentDatabase *db, const char *studentId);

/* ============================================================================
 * MANUAL SEARCHING & SORTING ALGORITHMS
 * ============================================================================ */
void searchByStudentId(const StudentDatabase *db, const char *studentId);
void searchByStudentName(const StudentDatabase *db, const char *nameQuery);
void searchByProgramme(const StudentDatabase *db, const char *programmeQuery);

void sortByStudentId(StudentDatabase *db);
void sortByStudentName(StudentDatabase *db);
void sortByAcademicAverage(StudentDatabase *db);
void sortByProgramme(StudentDatabase *db);

/* ============================================================================
 * ACADEMIC PERFORMANCE ANALYTICS & REPORTING
 * ============================================================================ */
void generateAcademicReport(const StudentDatabase *db, int exportToFile, const char *filename);

#endif /* STUDENT_RECORDS_H */