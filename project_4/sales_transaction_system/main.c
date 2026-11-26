#include "sales_transactions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * SAFE INPUT VALIDATION HELPERS
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

/* Reads a non-empty string without '|' (reserved delimiter in data file) */
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

/* Reads an optional string (pressing Enter leaves buffer empty) */
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

/* Validates basic YYYY-MM-DD date format */
static int isValidDateFormat(const char *date) {
    if (!date || strlen(date) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (date[i] != '-') return 0;
        } else if (!isdigit((unsigned char)date[i])) {
            return 0;
        }
    }
    int month = (date[5] - '0') * 10 + (date[6] - '0');
    int day   = (date[8] - '0') * 10 + (date[9] - '0');
    return (month >= 1 && month <= 12 && day >= 1 && day <= 31);
}

static void readValidatedDate(const char *prompt, char *buffer, size_t size) {
    while (1) {
        readNonEmptyString(prompt, buffer, size);
        if (isValidDateFormat(buffer)) {
            return;
        }
        printf("[ERROR] Invalid date format. Use YYYY-MM-DD (e.g., 2026-11-26).\n");
    }
}

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
            printf("[ERROR] Invalid value. Enter a whole number between %d and %d.\n",
                   minVal, maxVal);
            continue;
        }
        return (int)val;
    }
}

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
            printf("[ERROR] Please enter a valid number (%.2f - %.2f).\n", minVal, maxVal);
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
 * MENU OPERATION HANDLERS
 * ============================================================================ */

static void handleAddTransaction(TransactionList *list) {
    char txId[MAX_TX_ID_LEN];
    char product[MAX_PRODUCT_LEN];
    char date[MAX_DATE_LEN];

    printf("\n--- 1. ADD NEW SALES TRANSACTION ---\n");
    readNonEmptyString("Enter unique Transaction ID   : ", txId, sizeof(txId));

    if (findTransactionIndexById(list, txId) != -1) {
        printf("[ERROR] Transaction ID '%s' already exists. Duplicate IDs are rejected.\n", txId);
        return;
    }

    readNonEmptyString("Enter Product Name            : ", product, sizeof(product));
    int qty = readValidatedInt("Enter Quantity Sold (1-100000): ", 1, 100000);
    double price = readValidatedDouble("Enter Unit Price (> 0)        : ", 0.01, 10000000.0);
    readValidatedDate("Enter Date (YYYY-MM-DD)       : ", date, sizeof(date));

    if (addTransaction(list, txId, product, qty, price, date)) {
        printf("[SUCCESS] Transaction '%s' added (Total: %.2f).\n", txId, (double)qty * price);
    }
}

static void handleUpdateTransaction(TransactionList *list) {
    char txId[MAX_TX_ID_LEN];
    char newProduct[MAX_PRODUCT_LEN];
    char newDate[MAX_DATE_LEN];

    printf("\n--- 3. UPDATE EXISTING TRANSACTION ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions recorded yet.\n");
        return;
    }

    readNonEmptyString("Enter Transaction ID to update: ", txId, sizeof(txId));
    int idx = findTransactionIndexById(list, txId);
    if (idx == -1) {
        printf("[ERROR] Transaction ID '%s' was not found.\n", txId);
        return;
    }

    Transaction *existing = &list->records[idx];
    printf("Current: %s | Qty: %d | Unit Price: %.2f | Total: %.2f | Date: %s\n",
           existing->productName, existing->quantitySold,
           existing->unitPrice, existing->totalAmount, existing->transactionDate);

    readOptionalString("New Product Name (Enter=keep) : ", newProduct, sizeof(newProduct));
    int newQty = readValidatedInt("New Quantity Sold (1-100000)  : ", 1, 100000);
    double newPrice = readValidatedDouble("New Unit Price (> 0)          : ", 0.01, 10000000.0);

    while (1) {
        readOptionalString("New Date YYYY-MM-DD (Enter=keep): ", newDate, sizeof(newDate));
        if (isBlankString(newDate) || isValidDateFormat(newDate)) {
            break;
        }
        printf("[ERROR] Invalid date format. Use YYYY-MM-DD.\n");
    }

    if (updateTransaction(list, txId, newProduct, newQty, newPrice, newDate)) {
        printf("[SUCCESS] Transaction '%s' updated (New Total: %.2f).\n",
               txId, (double)newQty * newPrice);
    }
}

static void handleDeleteTransaction(TransactionList *list) {
    char txId[MAX_TX_ID_LEN];

    printf("\n--- 4. DELETE TRANSACTION ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions recorded yet.\n");
        return;
    }

    readNonEmptyString("Enter Transaction ID to delete: ", txId, sizeof(txId));
    int idx = findTransactionIndexById(list, txId);
    if (idx == -1) {
        printf("[ERROR] Transaction ID '%s' was not found.\n", txId);
        return;
    }

    printf("Target: %s | Product: %s | Total: %.2f\n",
           list->records[idx].transactionId,
           list->records[idx].productName,
           list->records[idx].totalAmount);

    if (confirmAction("Are you sure you want to delete this transaction?")) {
        if (deleteTransaction(list, txId)) {
            printf("[SUCCESS] Transaction '%s' deleted.\n", txId);
        }
    } else {
        printf("[INFO] Deletion cancelled.\n");
    }
}

/* Demonstrates runtime selection and invocation via an array of function pointers */
static void handleProcessSalesData(const TransactionList *list) {
    printf("\n--- 5. PROCESS SALES DATA (FUNCTION POINTERS) ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions available to process.\n");
        return;
    }

    SalesOperationFn operations[] = {
        calcTotalRevenue,
        calcAverageTransactionValue,
        findHighestTransactionValue,
        findLowestTransactionValue,
        calcTotalUnitsSold
    };

    const char *labels[] = {
        "Total Revenue",
        "Average Transaction Value",
        "Highest-Value Transaction",
        "Lowest-Value Transaction",
        "Total Units Sold"
    };

    printf("  1. Calculate total revenue\n");
    printf("  2. Calculate average transaction value\n");
    printf("  3. Identify the highest-value transaction\n");
    printf("  4. Identify the lowest-value transaction\n");
    printf("  5. Calculate total units sold\n");

    int choice = readValidatedInt("Select operation (1-5)        : ", 1, 5);
    int targetIdx = -1;

    /* Assign chosen function pointer dynamically at runtime */
    SalesOperationFn selectedFn = operations[choice - 1];
    double result = executeSalesOperation(list, selectedFn, &targetIdx);

    printf("\n[RESULT] %s: ", labels[choice - 1]);
    if (choice == 5) {
        printf("%.0f unit(s)\n", result);
    } else {
        printf("%.2f\n", result);
    }

    if (targetIdx != -1) {
        printf("         Matched Record -> ID: %s | Product: %s | Qty: %d | Date: %s\n",
               list->records[targetIdx].transactionId,
               list->records[targetIdx].productName,
               list->records[targetIdx].quantitySold,
               list->records[targetIdx].transactionDate);
    }
}

/* Demonstrates passing callback functions as arguments for filtering & counting */
static void handleFilterCallbacks(const TransactionList *list) {
    printf("\n--- 6. FILTER TRANSACTIONS (CALLBACK FUNCTIONS) ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions available to filter.\n");
        return;
    }

    printf("  1. Filter transactions above a specified sales amount\n");
    printf("  2. Identify transactions involving a specified product\n");
    printf("  3. Filter bulk orders (quantity >= minimum) & show count\n");

    int choice = readValidatedInt("Select callback filter (1-3)  : ", 1, 3);

    if (choice == 1) {
        double threshold = readValidatedDouble("Enter minimum sales amount    : ", 0.0, 100000000.0);
        char desc[96];
        snprintf(desc, sizeof(desc), "Total Amount > %.2f", threshold);
        filterAndDisplayTransactions(list, filterAboveAmountCallback, &threshold, desc);
    } else if (choice == 2) {
        char product[MAX_PRODUCT_LEN];
        readNonEmptyString("Enter product name to match   : ", product, sizeof(product));
        char desc[96];
        snprintf(desc, sizeof(desc), "Product matching '%s'", product);
        filterAndDisplayTransactions(list, filterByProductCallback, product, desc);
    } else {
        int minQty = readValidatedInt("Enter minimum quantity sold   : ", 1, 100000);
        char desc[96];
        snprintf(desc, sizeof(desc), "Quantity Sold >= %d", minQty);
        filterAndDisplayTransactions(list, filterHighQuantityCallback, &minQty, desc);
        int totalMatches = countMatchingTransactions(list, filterHighQuantityCallback, &minQty);
        printf("[CALLBACK COUNT] %d transaction(s) met the bulk quantity condition.\n", totalMatches);
    }
}

static void handleSearchTransactions(const TransactionList *list) {
    printf("\n--- 7. SEARCH TRANSACTIONS ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions available to search.\n");
        return;
    }

    printf("  1. Search by Transaction ID\n");
    printf("  2. Search by Product Name\n");
    printf("  3. Search by Exact Total Transaction Amount\n");

    int choice = readValidatedInt("Select search criterion (1-3) : ", 1, 3);
    if (choice == 1) {
        char txId[MAX_TX_ID_LEN];
        readNonEmptyString("Enter Transaction ID          : ", txId, sizeof(txId));
        searchByTransactionId(list, txId);
    } else if (choice == 2) {
        char product[MAX_PRODUCT_LEN];
        readNonEmptyString("Enter Product Name            : ", product, sizeof(product));
        searchByProductName(list, product);
    } else {
        double amount = readValidatedDouble("Enter Total Amount            : ", 0.01, 100000000.0);
        searchByTransactionAmount(list, amount);
    }
}

static void handleSortTransactions(TransactionList *list) {
    printf("\n--- 8. SORT TRANSACTIONS ---\n");
    if (list->count < 2) {
        printf("[INFO] At least 2 transactions are required to sort.\n");
        return;
    }

    printf("  1. Sort by Transaction ID (Ascending)\n");
    printf("  2. Sort by Product Name (Alphabetical)\n");
    printf("  3. Sort by Total Amount (Ascending - Callback)\n");
    printf("  4. Sort by Total Amount (Descending - Callback)\n");

    int choice = readValidatedInt("Select sort option (1-4)      : ", 1, 4);
    switch (choice) {
        case 1: sortByTransactionId(list); break;
        case 2: sortByProductName(list); break;
        case 3: sortByAmountWithCallback(list, compareAmountAscCallback); break;
        case 4: sortByAmountWithCallback(list, compareAmountDescCallback); break;
    }

    printf("[SUCCESS] Transactions sorted.\n");
    displayAllTransactions(list);
}

static void handleGenerateReport(const TransactionList *list) {
    printf("\n--- 9. GENERATE SALES REPORT ---\n");
    if (list->count == 0) {
        printf("[INFO] No transactions available to generate a report.\n");
        return;
    }

    char product[MAX_PRODUCT_LEN];
    readOptionalString("Enter product for revenue breakdown (Enter=skip): ", product, sizeof(product));

    generateSalesReport(list, product, 0, NULL);
    if (confirmAction("Export this report to 'sales_summary_report.txt'?")) {
        generateSalesReport(list, product, 1, TX_REPORT_FILENAME);
    }
}

static void displayMenu(void) {
    printf("\n===== SALES TRANSACTION MANAGEMENT SYSTEM =====\n");
    printf("  1. Add New Transaction\n");
    printf("  2. Display All Transactions\n");
    printf("  3. Update Transaction\n");
    printf("  4. Delete Transaction\n");
    printf("  5. Process Sales Data\n");
    printf("  6. Filter Transactions (Callbacks)\n");
    printf("  7. Search Transactions\n");
    printf("  8. Sort Transactions\n");
    printf("  9. Generate Sales Report\n");
    printf(" 10. Save Transactions\n");
    printf(" 11. Load Transactions\n");
    printf("  0. Exit\n");
    printf("===============================================\n");
}

int main(void) {
    TransactionList list;
    if (!initTransactionList(&list)) {
        return 1;
    }

    printf("Initializing Sales Transaction Management System...\n");
    loadTransactionsFromFile(&list, TX_DATA_FILENAME);

    int running = 1;
    while (running) {
        displayMenu();
        int choice = readValidatedInt("Enter your choice (0-11)      : ", 0, 11);

        switch (choice) {
            case 1:
                handleAddTransaction(&list);
                break;
            case 2:
                displayAllTransactions(&list);
                break;
            case 3:
                handleUpdateTransaction(&list);
                break;
            case 4:
                handleDeleteTransaction(&list);
                break;
            case 5:
                handleProcessSalesData(&list);
                break;
            case 6:
                handleFilterCallbacks(&list);
                break;
            case 7:
                handleSearchTransactions(&list);
                break;
            case 8:
                handleSortTransactions(&list);
                break;
            case 9:
                handleGenerateReport(&list);
                break;
            case 10:
                if (saveTransactionsToFile(&list, TX_DATA_FILENAME)) {
                    printf("[SUCCESS] Transactions saved to '%s'.\n", TX_DATA_FILENAME);
                }
                break;
            case 11:
                loadTransactionsFromFile(&list, TX_DATA_FILENAME);
                break;
            case 0:
                if (list.count > 0 && confirmAction("Save changes before exiting?")) {
                    saveTransactionsToFile(&list, TX_DATA_FILENAME);
                }
                freeTransactionList(&list);
                printf("Allocated memory released. Exiting program.\n");
                running = 0;
                break;
        }
    }

    return 0;
}