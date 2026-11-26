#include "sales_transactions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define INITIAL_TX_CAPACITY 4
#define FLOAT_EPSILON       0.01

/* ============================================================================
 * INTERNAL STRING HELPER FUNCTIONS
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

int initTransactionList(TransactionList *list) {
    if (!list) {
        return 0;
    }
    list->records = (Transaction *)calloc(INITIAL_TX_CAPACITY, sizeof(Transaction));
    if (!list->records) {
        fprintf(stderr, "[CRITICAL ERROR] Memory allocation failed during initialization.\n");
        list->count = 0;
        list->capacity = 0;
        return 0;
    }
    list->count = 0;
    list->capacity = INITIAL_TX_CAPACITY;
    return 1;
}

void freeTransactionList(TransactionList *list) {
    if (!list) {
        return;
    }
    if (list->records != NULL) {
        free(list->records);
        list->records = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

static int ensureTransactionCapacity(TransactionList *list) {
    if (list->count < list->capacity) {
        return 1;
    }
    int newCapacity = (list->capacity == 0) ? INITIAL_TX_CAPACITY : list->capacity * 2;
    Transaction *resized = (Transaction *)realloc(list->records,
                                                  (size_t)newCapacity * sizeof(Transaction));
    if (!resized) {
        fprintf(stderr, "[ERROR] Memory reallocation failed while expanding transaction list.\n");
        return 0;
    }
    list->records = resized;
    list->capacity = newCapacity;
    return 1;
}

int saveTransactionsToFile(const TransactionList *list, const char *filename) {
    if (!list || !filename) {
        return 0;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[ERROR] Cannot open file '%s' for writing.\n", filename);
        return 0;
    }

    fprintf(fp, "TX_COUNT|%d\n", list->count);
    for (int i = 0; i < list->count; i++) {
        const Transaction *tx = &list->records[i];
        fprintf(fp, "TX|%s|%s|%d|%.2f|%.2f|%s\n",
                tx->transactionId,
                tx->productName,
                tx->quantitySold,
                tx->unitPrice,
                tx->totalAmount,
                tx->transactionDate);
    }

    if (ferror(fp)) {
        fprintf(stderr, "[ERROR] A write error occurred while saving to '%s'.\n", filename);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    return 1;
}

int loadTransactionsFromFile(TransactionList *list, const char *filename) {
    if (!list || !filename) {
        return 0;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[INFO] No saved transaction file '%s' found. Starting with an empty list.\n", filename);
        return 0;
    }

    char line[512];
    int expectedCount = 0;
    if (!fgets(line, sizeof(line), fp) || sscanf(line, "TX_COUNT|%d", &expectedCount) != 1 || expectedCount < 0) {
        fprintf(stderr, "[ERROR] Invalid or corrupted header in '%s'.\n", filename);
        fclose(fp);
        return 0;
    }

    /* Reset current records before loading saved records */
    list->count = 0;

    for (int i = 0; i < expectedCount; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fprintf(stderr, "[ERROR] Unexpected end of file while reading transaction #%d.\n", i + 1);
            fclose(fp);
            return 0;
        }
        line[strcspn(line, "\r\n")] = '\0';

        char *tag = strtok(line, "|");
        char *txId = strtok(NULL, "|");
        char *product = strtok(NULL, "|");
        char *qtyStr = strtok(NULL, "|");
        char *priceStr = strtok(NULL, "|");
        char *totalStr = strtok(NULL, "|");
        char *date = strtok(NULL, "|");

        if (!tag || strcmp(tag, "TX") != 0 || !txId || !product ||
            !qtyStr || !priceStr || !totalStr || !date) {
            fprintf(stderr, "[ERROR] Malformed transaction record at line %d in '%s'.\n", i + 2, filename);
            fclose(fp);
            return 0;
        }

        int qty = atoi(qtyStr);
        double price = atof(priceStr);

        if (!addTransaction(list, txId, product, qty, price, date)) {
            fprintf(stderr, "[ERROR] Failed to reconstruct transaction '%s' in memory.\n", txId);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    printf("[INFO] Loaded %d transaction(s) from '%s'.\n", list->count, filename);
    return 1;
}

/* ============================================================================
 * 2. TRANSACTION CRUD OPERATIONS
 * ============================================================================ */

int findTransactionIndexById(const TransactionList *list, const char *txId) {
    if (!list || !txId) {
        return -1;
    }
    for (int i = 0; i < list->count; i++) {
        if (strCaseCmp(list->records[i].transactionId, txId) == 0) {
            return i;
        }
    }
    return -1;
}

int addTransaction(TransactionList *list, const char *txId, const char *productName,
                   int quantity, double unitPrice, const char *date) {
    if (!list || !txId || !productName || !date) {
        return 0;
    }

    if (findTransactionIndexById(list, txId) != -1) {
        printf("[ERROR] Duplicate Transaction ID '%s'. Transaction rejected.\n", txId);
        return 0;
    }

    if (quantity <= 0) {
        printf("[ERROR] Quantity sold must be greater than 0.\n");
        return 0;
    }

    if (unitPrice <= 0.0) {
        printf("[ERROR] Unit price must be greater than 0.00.\n");
        return 0;
    }

    if (!ensureTransactionCapacity(list)) {
        return 0;
    }

    Transaction *tx = &list->records[list->count];
    strncpy(tx->transactionId, txId, MAX_TX_ID_LEN - 1);
    tx->transactionId[MAX_TX_ID_LEN - 1] = '\0';

    strncpy(tx->productName, productName, MAX_PRODUCT_LEN - 1);
    tx->productName[MAX_PRODUCT_LEN - 1] = '\0';

    tx->quantitySold = quantity;
    tx->unitPrice = unitPrice;
    tx->totalAmount = (double)quantity * unitPrice;

    strncpy(tx->transactionDate, date, MAX_DATE_LEN - 1);
    tx->transactionDate[MAX_DATE_LEN - 1] = '\0';

    list->count++;
    return 1;
}

static void printTransactionTableHeader(void) {
    printf("\n===========================================================================================\n");
    printf("%-12s %-26s %-8s %-12s %-14s %-12s\n",
           "TX ID", "PRODUCT NAME", "QTY", "UNIT PRICE", "TOTAL AMOUNT", "DATE");
    printf("===========================================================================================\n");
}

static void printTransactionRow(const Transaction *tx) {
    printf("%-12s %-26s %-8d %10.2f   %12.2f   %-12s\n",
           tx->transactionId,
           tx->productName,
           tx->quantitySold,
           tx->unitPrice,
           tx->totalAmount,
           tx->transactionDate);
}

void displayAllTransactions(const TransactionList *list) {
    if (!list || list->count == 0) {
        printf("\n[INFO] No sales transactions recorded yet.\n");
        return;
    }

    printTransactionTableHeader();
    for (int i = 0; i < list->count; i++) {
        printTransactionRow(&list->records[i]);
    }
    printf("===========================================================================================\n");
    printf("Total Records: %d | Allocated Capacity: %d\n", list->count, list->capacity);
}

int updateTransaction(TransactionList *list, const char *txId, const char *newProduct,
                      int newQuantity, double newUnitPrice, const char *newDate) {
    int idx = findTransactionIndexById(list, txId);
    if (idx == -1) {
        printf("[ERROR] Transaction ID '%s' was not found.\n", txId);
        return 0;
    }

    if (newQuantity <= 0 || newUnitPrice <= 0.0) {
        printf("[ERROR] Quantity and unit price must both be positive values.\n");
        return 0;
    }

    Transaction *tx = &list->records[idx];
    if (newProduct && strlen(newProduct) > 0) {
        strncpy(tx->productName, newProduct, MAX_PRODUCT_LEN - 1);
        tx->productName[MAX_PRODUCT_LEN - 1] = '\0';
    }
    tx->quantitySold = newQuantity;
    tx->unitPrice = newUnitPrice;
    tx->totalAmount = (double)newQuantity * newUnitPrice;

    if (newDate && strlen(newDate) > 0) {
        strncpy(tx->transactionDate, newDate, MAX_DATE_LEN - 1);
        tx->transactionDate[MAX_DATE_LEN - 1] = '\0';
    }

    return 1;
}

int deleteTransaction(TransactionList *list, const char *txId) {
    int idx = findTransactionIndexById(list, txId);
    if (idx == -1) {
        printf("[ERROR] Transaction ID '%s' was not found.\n", txId);
        return 0;
    }

    for (int i = idx; i < list->count - 1; i++) {
        list->records[i] = list->records[i + 1];
    }
    list->count--;

    /* Shrink dynamic memory via realloc() when usage drops to <= 25% of capacity */
    if (list->count > 0 && list->count <= list->capacity / 4 && list->capacity > INITIAL_TX_CAPACITY) {
        int newCapacity = list->capacity / 2;
        Transaction *shrunk = (Transaction *)realloc(list->records,
                                                     (size_t)newCapacity * sizeof(Transaction));
        if (shrunk != NULL) {
            list->records = shrunk;
            list->capacity = newCapacity;
        }
    }

    return 1;
}

/* ============================================================================
 * 3. FUNCTION POINTER SALES-PROCESSING OPERATIONS
 * ============================================================================ */

double calcTotalRevenue(const Transaction *records, int count, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!records || count <= 0) return 0.0;

    double total = 0.0;
    for (int i = 0; i < count; i++) {
        total += records[i].totalAmount;
    }
    return total;
}

double calcAverageTransactionValue(const Transaction *records, int count, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!records || count <= 0) return 0.0; /* Avoid division by zero */

    double total = calcTotalRevenue(records, count, NULL);
    return total / (double)count;
}

double findHighestTransactionValue(const Transaction *records, int count, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!records || count <= 0) return 0.0;

    int maxIdx = 0;
    for (int i = 1; i < count; i++) {
        if (records[i].totalAmount > records[maxIdx].totalAmount) {
            maxIdx = i;
        }
    }
    if (outIndex) *outIndex = maxIdx;
    return records[maxIdx].totalAmount;
}

double findLowestTransactionValue(const Transaction *records, int count, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!records || count <= 0) return 0.0;

    int minIdx = 0;
    for (int i = 1; i < count; i++) {
        if (records[i].totalAmount < records[minIdx].totalAmount) {
            minIdx = i;
        }
    }
    if (outIndex) *outIndex = minIdx;
    return records[minIdx].totalAmount;
}

double calcTotalUnitsSold(const Transaction *records, int count, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!records || count <= 0) return 0.0;

    long totalUnits = 0;
    for (int i = 0; i < count; i++) {
        totalUnits += records[i].quantitySold;
    }
    return (double)totalUnits;
}

/* Executes any selected sales operation dynamically through a function pointer */
double executeSalesOperation(const TransactionList *list, SalesOperationFn operation, int *outIndex) {
    if (outIndex) *outIndex = -1;
    if (!list || !operation || list->count <= 0) {
        return 0.0;
    }
    return operation(list->records, list->count, outIndex);
}

/* ============================================================================
 * 4. CALLBACK FUNCTIONS & HIGHER-ORDER PROCESSORS
 * ============================================================================ */

/* Predicate callback 1: matches transactions strictly above a threshold amount */
int filterAboveAmountCallback(const Transaction *tx, const void *context) {
    if (!tx || !context) return 0;
    double minThreshold = *(const double *)context;
    return (tx->totalAmount > minThreshold) ? 1 : 0;
}

/* Predicate callback 2: matches transactions involving a specified product */
int filterByProductCallback(const Transaction *tx, const void *context) {
    if (!tx || !context) return 0;
    const char *productQuery = (const char *)context;
    return strContainsCaseInsensitive(tx->productName, productQuery);
}

/* Predicate callback 3: matches bulk orders with quantity >= specified minimum */
int filterHighQuantityCallback(const Transaction *tx, const void *context) {
    if (!tx || !context) return 0;
    int minQty = *(const int *)context;
    return (tx->quantitySold >= minQty) ? 1 : 0;
}

/* Comparator callback 1: ascending order by total amount */
int compareAmountAscCallback(const Transaction *a, const Transaction *b) {
    if (a->totalAmount < b->totalAmount) return -1;
    if (a->totalAmount > b->totalAmount) return 1;
    return 0;
}

/* Comparator callback 2: descending order by total amount */
int compareAmountDescCallback(const Transaction *a, const Transaction *b) {
    if (a->totalAmount > b->totalAmount) return -1;
    if (a->totalAmount < b->totalAmount) return 1;
    return 0;
}

/* Higher-order function that invokes a callback predicate to filter and display records */
void filterAndDisplayTransactions(const TransactionList *list,
                                  TransactionFilterCallback predicate,
                                  const void *context,
                                  const char *filterDescription) {
    if (!list || !predicate) return;

    if (list->count == 0) {
        printf("\n[INFO] No transaction records available to filter.\n");
        return;
    }

    printf("\nFilter Applied: %s\n", filterDescription ? filterDescription : "Custom Callback Filter");
    int matched = 0;

    for (int i = 0; i < list->count; i++) {
        if (predicate(&list->records[i], context)) {
            if (matched == 0) {
                printTransactionTableHeader();
            }
            printTransactionRow(&list->records[i]);
            matched++;
        }
    }

    if (matched == 0) {
        printf("[INFO] 0 transactions matched the specified condition.\n");
    } else {
        printf("===========================================================================================\n");
        printf("Matched Transactions: %d / %d\n", matched, list->count);
    }
}

/* Higher-order function that invokes a callback predicate to count matching records */
int countMatchingTransactions(const TransactionList *list,
                              TransactionFilterCallback predicate,
                              const void *context) {
    if (!list || !predicate || list->count <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < list->count; i++) {
        if (predicate(&list->records[i], context)) {
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * 5. MANUAL SEARCHING & SORTING ALGORITHMS (NO QSORT)
 * ============================================================================ */

void searchByTransactionId(const TransactionList *list, const char *txId) {
    int idx = findTransactionIndexById(list, txId);
    if (idx == -1) {
        printf("\n[INFO] No transaction found with ID '%s'.\n", txId);
        return;
    }
    printTransactionTableHeader();
    printTransactionRow(&list->records[idx]);
    printf("===========================================================================================\n");
}

void searchByProductName(const TransactionList *list, const char *productQuery) {
    if (!list || list->count == 0) {
        printf("\n[INFO] Transaction list is empty.\n");
        return;
    }

    int found = 0;
    for (int i = 0; i < list->count; i++) {
        if (strContainsCaseInsensitive(list->records[i].productName, productQuery)) {
            if (found == 0) {
                printTransactionTableHeader();
            }
            printTransactionRow(&list->records[i]);
            found++;
        }
    }

    if (found == 0) {
        printf("\n[INFO] No transactions found for product matching '%s'.\n", productQuery);
    } else {
        printf("===========================================================================================\n");
        printf("Found %d matching transaction(s).\n", found);
    }
}

void searchByTransactionAmount(const TransactionList *list, double targetAmount) {
    if (!list || list->count == 0) {
        printf("\n[INFO] Transaction list is empty.\n");
        return;
    }

    int found = 0;
    for (int i = 0; i < list->count; i++) {
        if (fabs(list->records[i].totalAmount - targetAmount) < FLOAT_EPSILON) {
            if (found == 0) {
                printTransactionTableHeader();
            }
            printTransactionRow(&list->records[i]);
            found++;
        }
    }

    if (found == 0) {
        printf("\n[INFO] No transactions found with total amount %.2f.\n", targetAmount);
    } else {
        printf("===========================================================================================\n");
        printf("Found %d matching transaction(s).\n", found);
    }
}

static void swapTransactions(Transaction *a, Transaction *b) {
    Transaction temp = *a;
    *a = *b;
    *b = temp;
}

void sortByTransactionId(TransactionList *list) {
    if (!list || list->count < 2) return;
    for (int i = 0; i < list->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < list->count; j++) {
            if (strCaseCmp(list->records[j].transactionId, list->records[minIdx].transactionId) < 0) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            swapTransactions(&list->records[i], &list->records[minIdx]);
        }
    }
}

void sortByProductName(TransactionList *list) {
    if (!list || list->count < 2) return;
    for (int i = 0; i < list->count - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < list->count; j++) {
            int cmp = strCaseCmp(list->records[j].productName, list->records[minIdx].productName);
            if (cmp < 0 || (cmp == 0 &&
                strCaseCmp(list->records[j].transactionId, list->records[minIdx].transactionId) < 0)) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            swapTransactions(&list->records[i], &list->records[minIdx]);
        }
    }
}

/* Manual Selection Sort driven by a comparator callback function */
void sortByAmountWithCallback(TransactionList *list, TransactionCompareCallback compareFn) {
    if (!list || !compareFn || list->count < 2) return;
    for (int i = 0; i < list->count - 1; i++) {
        int selectedIdx = i;
        for (int j = i + 1; j < list->count; j++) {
            if (compareFn(&list->records[j], &list->records[selectedIdx]) < 0) {
                selectedIdx = j;
            }
        }
        if (selectedIdx != i) {
            swapTransactions(&list->records[i], &list->records[selectedIdx]);
        }
    }
}

/* ============================================================================
 * 6. SALES SUMMARY REPORT GENERATION
 * ============================================================================ */

void generateSalesReport(const TransactionList *list, const char *selectedProduct,
                         int exportToFile, const char *filename) {
    if (!list || list->count == 0) {
        printf("\n[INFO] Cannot generate sales report: No transactions recorded.\n");
        return;
    }

    FILE *out = stdout;
    if (exportToFile && filename) {
        out = fopen(filename, "w");
        if (!out) {
            fprintf(stderr, "[ERROR] Unable to open '%s' to save sales report.\n", filename);
            return;
        }
    }

    int highIdx = -1;
    int lowIdx = -1;
    double totalRevenue = executeSalesOperation(list, calcTotalRevenue, NULL);
    double avgValue = executeSalesOperation(list, calcAverageTransactionValue, NULL);
    double highestVal = executeSalesOperation(list, findHighestTransactionValue, &highIdx);
    double lowestVal = executeSalesOperation(list, findLowestTransactionValue, &lowIdx);
    double totalUnits = executeSalesOperation(list, calcTotalUnitsSold, NULL);

    fprintf(out, "\n======================================================================\n");
    fprintf(out, "                    SALES TRANSACTION SUMMARY REPORT\n");
    fprintf(out, "======================================================================\n");
    fprintf(out, "Total Number of Transactions     : %d\n", list->count);
    fprintf(out, "Total Quantity of Products Sold  : %.0f unit(s)\n", totalUnits);
    fprintf(out, "Total Revenue                    : %.2f\n", totalRevenue);
    fprintf(out, "Average Transaction Value        : %.2f\n", avgValue);

    if (highIdx != -1) {
        fprintf(out, "Highest Transaction Value        : %.2f (ID: %s | Product: %s)\n",
                highestVal, list->records[highIdx].transactionId,
                list->records[highIdx].productName);
    }
    if (lowIdx != -1) {
        fprintf(out, "Lowest Transaction Value         : %.2f (ID: %s | Product: %s)\n",
                lowestVal, list->records[lowIdx].transactionId,
                list->records[lowIdx].productName);
    }

    if (selectedProduct && strlen(selectedProduct) > 0) {
        double productRevenue = 0.0;
        int productUnits = 0;
        int productTxCount = countMatchingTransactions(list, filterByProductCallback, selectedProduct);

        for (int i = 0; i < list->count; i++) {
            if (filterByProductCallback(&list->records[i], selectedProduct)) {
                productRevenue += list->records[i].totalAmount;
                productUnits += list->records[i].quantitySold;
            }
        }

        fprintf(out, "----------------------------------------------------------------------\n");
        fprintf(out, "SELECTED PRODUCT ANALYSIS ('%s')\n", selectedProduct);
        fprintf(out, "  Matching Transactions          : %d\n", productTxCount);
        fprintf(out, "  Units Sold                     : %d\n", productUnits);
        fprintf(out, "  Revenue Generated              : %.2f\n", productRevenue);
    }

    fprintf(out, "======================================================================\n");

    if (exportToFile && filename) {
        fclose(out);
        printf("[SUCCESS] Sales report saved to '%s'.\n", filename);
    }
}