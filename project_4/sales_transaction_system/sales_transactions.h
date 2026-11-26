#ifndef SALES_TRANSACTIONS_H
#define SALES_TRANSACTIONS_H

#include <stddef.h>

/* ============================================================================
 * SYSTEM CONSTANTS
 * ============================================================================ */
#define MAX_TX_ID_LEN       20
#define MAX_PRODUCT_LEN     60
#define MAX_DATE_LEN        16
#define TX_DATA_FILENAME    "sales_transactions.dat"
#define TX_REPORT_FILENAME  "sales_summary_report.txt"

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/* Represents a single retail/business sales transaction */
typedef struct {
    char transactionId[MAX_TX_ID_LEN];
    char productName[MAX_PRODUCT_LEN];
    int quantitySold;
    double unitPrice;
    double totalAmount;
    char transactionDate[MAX_DATE_LEN];
} Transaction;

/* Manages the dynamically allocated array of Transaction records */
typedef struct {
    Transaction *records;  /* Allocated via calloc() and resized via realloc() */
    int count;
    int capacity;
} TransactionList;

/* ============================================================================
 * FUNCTION POINTER & CALLBACK TYPE DEFINITIONS
 * ============================================================================ */

/* Function pointer type for runtime sales-processing operations (Section B) */
typedef double (*SalesOperationFn)(const Transaction *records, int count, int *outIndex);

/* Predicate callback type for filtering and counting transactions (Section C) */
typedef int (*TransactionFilterCallback)(const Transaction *tx, const void *context);

/* Comparator callback type for manual sorting by total amount (Section C & E) */
typedef int (*TransactionCompareCallback)(const Transaction *a, const Transaction *b);

/* ============================================================================
 * 1. DYNAMIC MEMORY MANAGEMENT & FILE HANDLING
 * ============================================================================ */
int initTransactionList(TransactionList *list);
void freeTransactionList(TransactionList *list);
int saveTransactionsToFile(const TransactionList *list, const char *filename);
int loadTransactionsFromFile(TransactionList *list, const char *filename);

/* ============================================================================
 * 2. TRANSACTION CRUD OPERATIONS
 * ============================================================================ */
int findTransactionIndexById(const TransactionList *list, const char *txId);
int addTransaction(TransactionList *list, const char *txId, const char *productName,
                   int quantity, double unitPrice, const char *date);
void displayAllTransactions(const TransactionList *list);
int updateTransaction(TransactionList *list, const char *txId, const char *newProduct,
                      int newQuantity, double newUnitPrice, const char *newDate);
int deleteTransaction(TransactionList *list, const char *txId);

/* ============================================================================
 * 3. FUNCTION POINTER SALES-PROCESSING OPERATIONS
 * ============================================================================ */
double calcTotalRevenue(const Transaction *records, int count, int *outIndex);
double calcAverageTransactionValue(const Transaction *records, int count, int *outIndex);
double findHighestTransactionValue(const Transaction *records, int count, int *outIndex);
double findLowestTransactionValue(const Transaction *records, int count, int *outIndex);
double calcTotalUnitsSold(const Transaction *records, int count, int *outIndex);

double executeSalesOperation(const TransactionList *list, SalesOperationFn operation, int *outIndex);

/* ============================================================================
 * 4. CALLBACK FUNCTIONS & HIGHER-ORDER PROCESSORS
 * ============================================================================ */
int filterAboveAmountCallback(const Transaction *tx, const void *context);
int filterByProductCallback(const Transaction *tx, const void *context);
int filterHighQuantityCallback(const Transaction *tx, const void *context);

int compareAmountAscCallback(const Transaction *a, const Transaction *b);
int compareAmountDescCallback(const Transaction *a, const Transaction *b);

void filterAndDisplayTransactions(const TransactionList *list,
                                  TransactionFilterCallback predicate,
                                  const void *context,
                                  const char *filterDescription);
int countMatchingTransactions(const TransactionList *list,
                              TransactionFilterCallback predicate,
                              const void *context);

/* ============================================================================
 * 5. MANUAL SEARCHING & SORTING ALGORITHMS (NO QSORT)
 * ============================================================================ */
void searchByTransactionId(const TransactionList *list, const char *txId);
void searchByProductName(const TransactionList *list, const char *productQuery);
void searchByTransactionAmount(const TransactionList *list, double targetAmount);

void sortByTransactionId(TransactionList *list);
void sortByProductName(TransactionList *list);
void sortByAmountWithCallback(TransactionList *list, TransactionCompareCallback compareFn);

/* ============================================================================
 * 6. SALES REPORT GENERATION
 * ============================================================================ */
void generateSalesReport(const TransactionList *list, const char *selectedProduct,
                         int exportToFile, const char *filename);

#endif /* SALES_TRANSACTIONS_H */