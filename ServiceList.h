#ifndef HEADER_ServiceList
#define HEADER_ServiceList
/*
htop - ServiceList.h
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "Object.h"
#include "Panel.h"
#include "Service.h"
#include "RichString.h"
#include "Settings.h"
#include "UsersTable.h"
#include "Vector.h"

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#endif


#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

// typedef unsigned long long int memory_t;
#define MEMORY_MAX ULLONG_MAX

typedef struct ServiceList_ {
   const Settings* settings;

   Vector* services;
   Vector* services2;
   Hashtable* serviceTable;
   UsersTable* usersTable;

   Hashtable* displayTreeSet;
   Hashtable* draftingTreeSet;

   Hashtable* dynamicMeters;  /* runtime-discovered meters */
   Hashtable* dynamicColumns; /* runtime-discovered Columns */

   struct timeval realtime;   /* time of the current sample */
   uint64_t realtimeMs;       /* current time in milliseconds */
   uint64_t monotonicMs;      /* same, but from monotonic clock */

   Panel* panel;
   int following;
   uid_t userId;
   const char* incFilter;
   Hashtable* pidMatchList;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   unsigned int totalTasks;
   unsigned int runningTasks;
   unsigned int userlandThreads;
   unsigned int kernelThreads;

   // memory_t totalMem;
   // memory_t usedMem;
   // memory_t buffersMem;
   // memory_t cachedMem;
   // memory_t sharedMem;
   // memory_t availableMem;

   // memory_t totalSwap;
   // memory_t usedSwap;
   // memory_t cachedSwap;

   unsigned int activeCPUs;
   unsigned int existingCPUs;
} ServiceList;

/* Implemented by platforms */
ServiceList* ServiceList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);
void ServiceList_delete(ServiceList* pl);
void ServiceList_goThroughEntries(ServiceList* super, bool pauseServiceUpdate);
bool ServiceList_isCPUonline(const ServiceList* super, unsigned int id);


ServiceList* ServiceList_init(ServiceList* this, const ObjectClass* klass, UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ServiceList_done(ServiceList* this);

void ServiceList_setPanel(ServiceList* this, Panel* panel);

void ServiceList_printHeader(const ServiceList* this, RichString* header);

void ServiceList_add(ServiceList* this, Service* p);

void ServiceList_remove(ServiceList* this, const Service* p);

void ServiceList_sort(ServiceList* this);

ServiceField ServiceList_keyAt(const ServiceList* this, int at);

void ServiceList_expandTree(ServiceList* this);

void ServiceList_collapseAllBranches(ServiceList* this);

void ServiceList_rebuildPanel(ServiceList* this);

Service* ServiceList_getService(ServiceList* this, pid_t pid, bool* preExisting, Service_New constructor);

void ServiceList_scan(ServiceList* this, bool pauseServiceUpdate);

static inline Service* ServiceList_findService(ServiceList* this, pid_t pid) {
   return (Service*) Hashtable_get(this->serviceTable, pid);
}

#endif
