/**
 * ============================================================================
 * Project 1: Smart Occupancy Monitoring and Access Control System
 * Platform : Arduino Uno (Tinkercad Simulation)
 * Language : C/C++ (Arduino Framework)
 *
 * Description:
 *   Tracks real-time facility occupancy using non-blocking push button inputs,
 *   dynamically allocated data structures, pointer-based state updates, dual
 *   status LEDs, a 16x2 LCD display, and Serial Monitor diagnostics.
 * ============================================================================
 */

#include <LiquidCrystal.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// 1. HARDWARE PIN MAPPING & SYSTEM CONSTANTS
// ============================================================================
const uint8_t LCD_RS_PIN = 12;
const uint8_t LCD_E_PIN  = 11;
const uint8_t LCD_D4_PIN = 5;
const uint8_t LCD_D5_PIN = 4;
const uint8_t LCD_D6_PIN = 3;
const uint8_t LCD_D7_PIN = 2;

const uint8_t ENTRY_BTN_PIN = 6;
const uint8_t EXIT_BTN_PIN  = 7;
const uint8_t GREEN_LED_PIN = 8;
const uint8_t RED_LED_PIN   = 9;

const int MAX_CAPACITY = 10;                 // Predefined maximum facility capacity
const unsigned long DEBOUNCE_DELAY_MS = 50;  // Non-blocking button debounce window
const unsigned long LCD_REFRESH_MS    = 250; // Periodic LCD update interval
const unsigned long SERIAL_BAUD_RATE  = 9600;

// ============================================================================
// 2. DATA STRUCTURES
// ============================================================================

/**
 * Stores facility occupancy metrics and cumulative event statistics.
 * Allocated dynamically on the heap at startup.
 */
struct OccupancyRecord {
  int currentCount;
  int maxCapacity;
  unsigned long totalEntries;
  unsigned long totalExits;
  unsigned long rejectedEntries;
  unsigned long rejectedExits;
};

/**
 * Tracks non-blocking millis() debouncing state for a single push button.
 */
struct ButtonState {
  uint8_t pin;
  int lastReading;
  int stableState;
  unsigned long lastDebounceTime;
};

// ============================================================================
// 3. GLOBAL OBJECTS & STATE POINTERS
// ============================================================================
LiquidCrystal lcd(LCD_RS_PIN, LCD_E_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

OccupancyRecord* occupancy = NULL;  // Pointer to heap-allocated occupancy record

ButtonState entryButton = {ENTRY_BTN_PIN, LOW, LOW, 0};
ButtonState exitButton  = {EXIT_BTN_PIN,  LOW, LOW, 0};

unsigned long lastLcdUpdateTime = 0;

// ============================================================================
// 4. DYNAMIC MEMORY MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Dynamically allocates and zero-initializes an OccupancyRecord using calloc().
 *
 * @param maxCap Maximum allowed occupancy for the facility.
 * @return Pointer to the allocated OccupancyRecord, or NULL if allocation fails.
 */
struct OccupancyRecord* createOccupancyRecord(int maxCap) {
  OccupancyRecord* newRecord = (OccupancyRecord*)calloc(1, sizeof(OccupancyRecord));
  if (newRecord == NULL) {
    return NULL;
  }

  newRecord->currentCount = 0;
  newRecord->maxCapacity = maxCap;
  return newRecord;
}

/**
 * Safely deallocates an OccupancyRecord using free() and sets the caller's
 * pointer to NULL to prevent dangling pointer access.
 *
 * @param recordPtr Address of the OccupancyRecord pointer to free.
 */
void destroyOccupancyRecord(struct OccupancyRecord** recordPtr) {
  if (recordPtr != NULL && *recordPtr != NULL) {
    free(*recordPtr);
    *recordPtr = NULL;
  }
}

// ============================================================================
// 5. NON-BLOCKING INPUT & REPORTING FUNCTIONS
// ============================================================================

/**
 * Performs non-blocking button debouncing using millis().
 * Detects a single rising-edge transition (LOW -> HIGH) per physical press.
 *
 * @param btn Pointer to the ButtonState structure for the target button.
 * @return true once when a valid press is confirmed; false otherwise.
 */
bool isButtonPressed(struct ButtonState* btn) {
  if (btn == NULL) {
    return false;
  }

  int currentReading = digitalRead(btn->pin);
  unsigned long now = millis();

  if (currentReading != btn->lastReading) {
    btn->lastDebounceTime = now;
  }

  bool pressedEvent = false;

  if ((now - btn->lastDebounceTime) >= DEBOUNCE_DELAY_MS) {
    if (currentReading != btn->stableState) {
      btn->stableState = currentReading;
      if (btn->stableState == HIGH) {
        pressedEvent = true;
      }
    }
  }

  btn->lastReading = currentReading;
  return pressedEvent;
}

/**
 * Calculates remaining available spaces in the facility.
 *
 * @param record Read-only pointer to the OccupancyRecord.
 * @return Number of available spaces remaining.
 */
int getRemainingCapacity(const struct OccupancyRecord* record) {
  if (record == NULL) {
    return 0;
  }
  return record->maxCapacity - record->currentCount;
}

/**
 * Outputs an event or error message along with current occupancy statistics
 * to the Serial Monitor.
 *
 * @param record Read-only pointer to the OccupancyRecord.
 * @param eventMsg Descriptive status or error string to display.
 */
void printSerialReport(const struct OccupancyRecord* record, const char* eventMsg) {
  if (record == NULL) {
    return;
  }

  Serial.println("----------------------------------------");
  Serial.println(eventMsg);
  Serial.print("Current Occupancy  : ");
  Serial.print(record->currentCount);
  Serial.print(" / ");
  Serial.println(record->maxCapacity);

  Serial.print("Remaining Capacity : ");
  Serial.println(getRemainingCapacity(record));

  Serial.print("Totals -> Entries: ");
  Serial.print(record->totalEntries);
  Serial.print(" | Exits: ");
  Serial.print(record->totalExits);
  Serial.print(" | Rejected Entries: ");
  Serial.print(record->rejectedEntries);
  Serial.print(" | Rejected Exits: ");
  Serial.println(record->rejectedExits);
}

// ============================================================================
// 6. CORE OCCUPANCY LOGIC (POINTER-BASED)
// ============================================================================

/**
 * Handles an entry event. Increments occupancy if capacity is available;
 * otherwise logs a full-capacity error and increments rejectedEntries.
 *
 * @param record Pointer to the OccupancyRecord to update.
 */
void handleEntry(struct OccupancyRecord* record) {
  if (record == NULL) {
    return;
  }

  if (record->currentCount >= record->maxCapacity) {
    record->rejectedEntries++;
    printSerialReport(record, "[ERROR] Entry rejected: Facility is at maximum capacity!");
    return;
  }

  record->currentCount++;
  record->totalEntries++;
  printSerialReport(record, "[EVENT] Person entered the facility.");
}

/**
 * Handles an exit event. Decrements occupancy if at least one person is inside;
 * otherwise logs an empty-facility error and increments rejectedExits.
 *
 * @param record Pointer to the OccupancyRecord to update.
 */
void handleExit(struct OccupancyRecord* record) {
  if (record == NULL) {
    return;
  }

  if (record->currentCount <= 0) {
    record->rejectedExits++;
    printSerialReport(record, "[ERROR] Exit rejected: Facility is already empty (0 people)!");
    return;
  }

  record->currentCount--;
  record->totalExits++;
  printSerialReport(record, "[EVENT] Person exited the facility.");
}

// ============================================================================
// 7. LED & LCD OUTPUT FUNCTIONS
// ============================================================================

/**
 * Updates the Green and Red status LEDs according to facility capacity:
 *   - Available capacity (0 <= current < max): Green ON, Red OFF
 *   - Maximum capacity (current >= max)      : Green OFF, Red ON
 *
 * @param record Read-only pointer to the OccupancyRecord.
 */
void updateLEDs(const struct OccupancyRecord* record) {
  if (record == NULL) {
    return;
  }

  if (record->currentCount >= record->maxCapacity) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  }
}

/**
 * Updates the 16x2 LCD with current occupancy and remaining capacity.
 * Formats fixed-width 16-character rows to avoid flicker without lcd.clear().
 *
 * @param record Read-only pointer to the OccupancyRecord.
 */
void updateLCD(const struct OccupancyRecord* record) {
  if (record == NULL) {
    return;
  }

  char line1[17];
  char line2[17];
  int remaining = getRemainingCapacity(record);

  snprintf(line1, sizeof(line1), "Inside: %2d / %-2d ", record->currentCount, record->maxCapacity);
  snprintf(line2, sizeof(line2), "Available: %-5d", remaining);

  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// ============================================================================
// 8. SERIAL COMMAND HANDLER
// ============================================================================

/**
 * Listens for Serial commands. Sending 'R' or 'r' demonstrates explicit heap
 * deallocation via free() followed by clean re-allocation.
 */
void handleSerialCommands() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'R' || cmd == 'r') {
      Serial.println("[MEMORY] Freeing current OccupancyRecord via free()...");
      destroyOccupancyRecord(&occupancy);

      occupancy = createOccupancyRecord(MAX_CAPACITY);
      if (occupancy == NULL) {
        Serial.println("[CRITICAL ERROR] Re-allocation failed!");
        while (true) {}
      }

      updateLEDs(occupancy);
      updateLCD(occupancy);
      printSerialReport(occupancy, "[SYSTEM] Memory freed and OccupancyRecord reset.");
    }
  }
}

// ============================================================================
// 9. ARDUINO SETUP & NON-BLOCKING MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  pinMode(ENTRY_BTN_PIN, INPUT);
  pinMode(EXIT_BTN_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();

  // Allocate and validate dynamic occupancy structure
  occupancy = createOccupancyRecord(MAX_CAPACITY);
  if (occupancy == NULL) {
    Serial.println("[CRITICAL ERROR] Memory allocation failed for OccupancyRecord!");
    lcd.print("Memory Error!");
    while (true) {}
  }

  updateLEDs(occupancy);
  updateLCD(occupancy);
  printSerialReport(occupancy, "[SYSTEM] Smart Occupancy System Ready. Send 'R' to reset memory.");
}

void loop() {
  // Check entry and exit buttons without blocking
  if (isButtonPressed(&entryButton)) {
    handleEntry(occupancy);
    updateLEDs(occupancy);
  }

  if (isButtonPressed(&exitButton)) {
    handleExit(occupancy);
    updateLEDs(occupancy);
  }

  // Periodic non-blocking LCD refresh using millis()
  unsigned long now = millis();
  if ((now - lastLcdUpdateTime) >= LCD_REFRESH_MS) {
    lastLcdUpdateTime = now;
    updateLCD(occupancy);
  }

  // Check for Serial reset command
  handleSerialCommands();
}