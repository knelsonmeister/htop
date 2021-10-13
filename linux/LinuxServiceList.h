#ifndef HEADER_LinuxServiceList
#define HEADER_LinuxServiceList
/*
htop - LinuxServiceList.h
(C) 2014 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ServiceList.h"
#include "UsersTable.h"
// #include "ZramStats.h"
// #include "zfs/ZfsArcStats.h"

#define HTOP_HUGEPAGE_BASE_SHIFT 16
#define HTOP_HUGEPAGE_COUNT 24

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int systemTime;
   unsigned long long int systemAllTime;
   unsigned long long int idleAllTime;
   unsigned long long int idleTime;
   unsigned long long int niceTime;
   unsigned long long int ioWaitTime;
   unsigned long long int irqTime;
   unsigned long long int softIrqTime;
   unsigned long long int stealTime;
   unsigned long long int guestTime;

   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int systemPeriod;
   unsigned long long int systemAllPeriod;
   unsigned long long int idleAllPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int nicePeriod;
   unsigned long long int ioWaitPeriod;
   unsigned long long int irqPeriod;
   unsigned long long int softIrqPeriod;
   unsigned long long int stealPeriod;
   unsigned long long int guestPeriod;

   double frequency;

   #ifdef HAVE_SENSORS_SENSORS_H
   double temperature;
   #endif

   bool online;
} CPUData;

typedef struct TtyDriver_ {
   char* path;
   unsigned int major;
   unsigned int minorFrom;
   unsigned int minorTo;
} TtyDriver;

typedef struct LinuxServiceList_ {
   ServiceList super;

   CPUData* cpuData;

   TtyDriver* ttyDrivers;
   bool haveSmapsRollup;
   bool haveAutogroup;

   #ifdef HAVE_DELAYACCT
   struct nl_sock* netlink_socket;
   int netlink_family;
   #endif

   // memory_t totalHugePageMem;
   // memory_t usedHugePageMem[HTOP_HUGEPAGE_COUNT];

   // memory_t availableMem;

   // ZfsArcStats zfs;
   // ZramStats zram;
} LinuxServiceList;

typedef struct UnitInfo {
  const char *machine;
  const char *id;
  const char *description;
  const char *load_state;
  const char *active_state;
  const char *sub_state;
  const char *following;
  const char *unit_path;
  uint32_t job_id;
  const char *job_type;
  const char *job_path;
} UnitInfo;

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCCPUINFOFILE
#define PROCCPUINFOFILE PROCDIR "/cpuinfo"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#ifndef PROCARCSTATSFILE
#define PROCARCSTATSFILE PROCDIR "/spl/kstat/zfs/arcstats"
#endif

#ifndef PROCTTYDRIVERSFILE
#define PROCTTYDRIVERSFILE PROCDIR "/tty/drivers"
#endif

#ifndef PROC_LINE_LENGTH
#define PROC_LINE_LENGTH 4096
#endif

ServiceList* ServiceList_new(UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId);

void ServiceList_delete(ServiceList* pl);

void ServiceList_goThroughEntries(ServiceList* super, bool pauseServiceUpdate);

bool ServiceList_isCPUonline(const ServiceList* super, unsigned int id);

#endif
