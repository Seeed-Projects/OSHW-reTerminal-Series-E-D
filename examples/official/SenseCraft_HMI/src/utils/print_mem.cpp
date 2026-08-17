#include "utils/print_mem.h"

#include <Arduino.h>
#include <esp_system.h>

static const unsigned long PRINT_INTERVAL_MS = 20000;
unsigned long  lastPrintTime = 0;

void printMemoryStatus() {
  // -- DRAM (Internal Heap) status -- //
  size_t heapTotal       = ESP.getHeapSize();         // Total heap area size (bytes)
  size_t heapFree        = ESP.getFreeHeap();         // Free heap size (bytes)
  size_t heapMaxBlock    = ESP.getMaxAllocHeap();     // Largest single allocatable heap block (bytes)
  size_t heapUsed        = heapTotal - heapFree;      // Used heap size (bytes)

  // -- PSRAM section (if available) -- //
  size_t psramTotal      = ESP.getPsramSize();        // Total external PSRAM size (bytes), 0 if unsupported
  size_t psramFree       = 0;
  size_t psramMaxBlock   = 0;
  size_t psramUsed       = 0;
  if (psramTotal > 0) {
    psramFree     = ESP.getFreePsram();               // Free PSRAM size (bytes)
    psramMaxBlock = ESP.getMaxAllocPsram();           // Largest single allocatable PSRAM block (bytes)
    psramUsed     = psramTotal - psramFree;           // Used PSRAM size (bytes)
  }

  // Print header
  Serial1.println("Memory Status:");
  Serial1.printf("    %-12s  /  %-12s  /  %-12s  /  %-12s\n",
                 "Max Alloc", "Used", "Free", "Total");
  Serial1.println("  DRAM:");
  Serial1.printf("    [%10u B /%10u B /%10u B /%10u B]\n",
                 heapMaxBlock, heapUsed, heapFree, heapTotal);

  // If PSRAM is available, print PSRAM stats
  if (psramTotal > 0) {
    Serial1.println("  PSRAM:");
    Serial1.printf("    [%10u B /%10u B /%10u B /%10u B]\n",
                   psramMaxBlock, psramUsed, psramFree, psramTotal);
  } else {
    Serial1.println("  PSRAM: Unavailable");
  }

  Serial1.println();
}