#ifndef HEADER_Service
#define HEADER_Service
/*
htop - Service.h
(C) 2004-2015 Hisham H. Muhammad
(C) 2020 Red Hat, Inc.  All Rights Reserved.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "Object.h"
#include "ProcessField.h"
#include "RichString.h"


#define PROCESS_FLAG_IO              0x00000001
#define PROCESS_FLAG_CWD             0x00000002

#define DEFAULT_HIGHLIGHT_SECS 5

typedef enum ServiceField_ {
   // NULL_PROCESSFIELD = 0,
   // PID = 1,
   // COMM = 2,
   // STATE = 3,
   // PPID = 4,
   // PGRP = 5,
   // SESSION = 6,
   // TTY = 7,
   // TPGID = 8,
   // MINFLT = 10,
   // MAJFLT = 12,
   // PRIORITY = 18,
   // NICE = 19,
   // STARTTIME = 21,
   // PROCESSOR = 38,
   // M_VIRT = 39,
   // M_RESIDENT = 40,
   // ST_UID = 46,
   // PERCENT_CPU = 47,
   // PERCENT_MEM = 48,
   // USER = 49,
   // TIME = 50,
   // NLWP = 51,
   // TGID = 52,
   // PERCENT_NORM_CPU = 53,
   // ELAPSED = 54,
   // PROC_COMM = 124,
   // PROC_EXE = 125,
   // CWD = 126,

   // /* Platform specific fields, defined in ${platform}/ServiceField.h */
   // PLATFORM_PROCESS_FIELDS

   // /* Do not add new fields after this entry (dynamic entries follow) */
   // LAST_PROCESSFIELD
   LAST_SERVICEFIELD = 255
} ServiceField;

struct Settings_;

/* Holds information about regions of the cmdline that should be
 * highlighted (e.g. program basename, delimiter, comm). */
typedef struct ServiceCmdlineHighlight_ {
   size_t offset; /* first character to highlight */
   size_t length; /* How many characters to highlight, zero if unused */
   int attr;      /* The attributes used to highlight */
   int flags;     /* Special flags used for selective highlighting, zero for always */
} ServiceCmdlineHighlight;

/* ServiceMergedCommand is populated by Service_makeCommandStr: It
 * contains the merged Command string, and the information needed by
 * Service_writeCommand to color the string. str will be NULL for kernel
 * threads and zombies */
typedef struct ServiceMergedCommand_ {
   char* str;                                  /* merged Command string */
   size_t highlightCount;                      /* how many portions of cmdline to highlight */
   ServiceCmdlineHighlight highlights[8];      /* which portions of cmdline to highlight */
   bool cmdlineChanged : 1;                    /* whether cmdline changed */
   bool exeChanged : 1;                        /* whether exe changed */
   bool commChanged : 1;                       /* whether comm changed */
   bool prevMergeSet : 1;                      /* whether showMergedCommand was set */
   bool prevPathSet : 1;                       /* whether showProgramPath was set */
   bool prevCommSet : 1;                       /* whether findCommInCmdline was set */
   bool prevCmdlineSet : 1;                    /* whether stripExeFromCmdline was set */
   bool prevShowThreadNames : 1;               /* whether showThreadNames was set */
} ServiceMergedCommand;

typedef struct Service_ {
   /* Super object for emulated OOP */
   Object super;

   /* Pointer to quasi-global data structures */
   const struct ServiceList_* serviceList;
   const struct Settings_* settings;

   /* Service identifier */
   pid_t pid;

   /* Parent service identifier */
   pid_t ppid;

   /* Thread group identifier */
   pid_t tgid;

   /* Service group identifier */
   int pgrp;

   /* Session identifier */
   int session;

   /* Foreground group identifier of the controlling terminal */
   int tpgid;

   /* This is a kernel (helper) task */
   bool isKernelThread;

   /* This is a userland thread / LWP */
   bool isUserlandThread;

   /* Controlling terminal identifier of the process */
   unsigned long int tty_nr;

   /* Controlling terminal name of the process */
   char* tty_name;

   /* User identifier */
   uid_t st_uid;

   /* User name */
   const char* user;

   /* Service runtime (in hundredth of a second) */
   unsigned long long int time;

   /*
    * Service name including arguments.
    * Use Service_getCommand() for Command actually displayed.
    */
   char* cmdline;

   /* End Offset in cmdline of the process basename */
   int cmdlineBasenameEnd;

   /* Start Offset in cmdline of the process basename */
   int cmdlineBasenameStart;

   /* The process' "command" name */
   char* procComm;

   /* The main process executable */
   char* procExe;

   /* The process/thread working directory */
   char* procCwd;

   /* Offset in procExe of the process basename */
   int procExeBasenameOffset;

   /* Tells if the executable has been replaced in the filesystem since start */
   bool procExeDeleted;

   /* Tells if the process uses replaced shared libraries since start */
   bool usesDeletedLib;

   /* CPU number last executed on */
   int processor;

   /* CPU usage during last cycle (in percent) */
   float percent_cpu;

   /* Memory usage during last cycle (in percent) */
   float percent_mem;

   /* Scheduling priority */
   long int priority;

   /* Nice value */
   long int nice;

   /* Number of threads in this process */
   long int nlwp;

   /* Service start time (in seconds elapsed since the Epoch) */
   time_t starttime_ctime;

   /* Service start time (cached formatted string) */
   char starttime_show[8];

   /* Total program size (in kilobytes) */
   long m_virt;

   /* Resident set size (in kilobytes) */
   long m_resident;

   /* Number of minor faults the process has made which have not required loading a memory page from disk */
   unsigned long int minflt;

   /* Number of major faults the process has made which have required loading a memory page from disk */
   unsigned long int majflt;

   /*
    * Service state (platform dependent):
    *   D  -  Waiting
    *   I  -  Idle
    *   L  -  Acquiring lock
    *   R  -  Running
    *   S  -  Sleeping
    *   T  -  Stopped (on a signal)
    *   X  -  Dead
    *   Z  -  Zombie
    *   t  -  Tracing stop
    *   ?  -  Unknown
    */
   char state;

   /* Whether the process was updated during the current scan */
   bool updated;

   /* Whether the process was tagged by the user */
   bool tag;

   /* Whether to display this process */
   bool show;

   /* Whether this process was shown last cycle */
   bool wasShown;

   /* Whether to show children of this process in tree-mode */
   bool showChildren;

   /*
    * Internal time counts for showing new and exited processes.
    */
   uint64_t seenStampMs;
   uint64_t tombStampMs;

   /*
    * Internal state for tree-mode.
    */
   int indent;
   unsigned int tree_left;
   unsigned int tree_right;
   unsigned int tree_depth;
   unsigned int tree_index;

   /*
    * Internal state for merged Command display
    */
   ServiceMergedCommand mergedCommand;
} Service;

typedef struct ServiceFieldData_ {
   /* Name (displayed in setup menu) */
   const char* name;

   /* Title (display in main screen); must have same width as the printed values */
   const char* title;

   /* Description (displayed in setup menu) */
   const char* description;

   /* Scan flag to enable scan-method otherwise not run */
   uint32_t flags;

   /* Whether the values are process identifiers; adjusts the width of title and values if true */
   bool pidColumn;

   /* Whether the column should be sorted in descending order by default */
   bool defaultSortDesc;
} ServiceFieldData;

// Implemented in platform-specific code:
void Service_writeField(const Service* this, RichString* str, ServiceField field);
int Service_compare(const void* v1, const void* v2);
void Service_delete(Object* cast);
extern const ServiceFieldData Service_fields[LAST_SERVICEFIELD];
#define PROCESS_MAX_PID_DIGITS 19
extern int Service_pidDigits;

typedef Service* (*Service_New)(const struct Settings_*);
typedef void (*Service_WriteField)(const Service*, RichString*, ServiceField);
typedef int (*Service_CompareByKey)(const Service*, const Service*, ServiceField);
typedef const char* (*Service_GetCommandStr)(const Service*);

typedef struct ServiceClass_ {
   const ObjectClass super;
   const Service_WriteField writeField;
   const Service_CompareByKey compareByKey;
   const Service_GetCommandStr getCommandStr;
} ServiceClass;

#define As_Service(this_)                              ((const ServiceClass*)((this_)->super.klass))

#define Service_getCommand(this_)                      (As_Service(this_)->getCommandStr ? As_Service(this_)->getCommandStr((const Service*)(this_)) : Service_getCommandStr((const Service*)(this_)))
#define Service_compareByKey(p1_, p2_, key_)           (As_Service(p1_)->compareByKey ? (As_Service(p1_)->compareByKey(p1_, p2_, key_)) : Service_compareByKey_Base(p1_, p2_, key_))

static inline pid_t Service_getParentPid(const Service* this) {
   return this->tgid == this->pid ? this->ppid : this->tgid;
}

static inline bool Service_isChildOf(const Service* this, pid_t pid) {
   return pid == Service_getParentPid(this);
}

static inline bool Service_isKernelThread(const Service* this) {
   return this->isKernelThread;
}

static inline bool Service_isUserlandThread(const Service* this) {
   return this->isUserlandThread;
}

static inline bool Service_isThread(const Service* this) {
   return Service_isUserlandThread(this) || Service_isKernelThread(this);
}

#define CMDLINE_HIGHLIGHT_FLAG_SEPARATOR  0x00000001
#define CMDLINE_HIGHLIGHT_FLAG_BASENAME   0x00000002
#define CMDLINE_HIGHLIGHT_FLAG_COMM       0x00000004
#define CMDLINE_HIGHLIGHT_FLAG_DELETED    0x00000008

#define ONE_K 1024UL
#define ONE_M (ONE_K * ONE_K)
#define ONE_G (ONE_M * ONE_K)
#define ONE_T (1ULL * ONE_G * ONE_K)
#define ONE_P (1ULL * ONE_T * ONE_K)

#define ONE_DECIMAL_K 1000UL
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)
#define ONE_DECIMAL_T (1ULL * ONE_DECIMAL_G * ONE_DECIMAL_K)
#define ONE_DECIMAL_P (1ULL * ONE_DECIMAL_T * ONE_DECIMAL_K)

void Service_setupColumnWidths(void);

/* Takes number in bytes (base 1024). Prints 6 columns. */
void Service_printBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number in kilo bytes (base 1024). Prints 6 columns. */
void Service_printKBytes(RichString* str, unsigned long long number, bool coloring);

/* Takes number as count (base 1000). Prints 12 columns. */
void Service_printCount(RichString* str, unsigned long long number, bool coloring);

/* Takes time in hundredths of a seconds. Prints 9 columns. */
void Service_printTime(RichString* str, unsigned long long totalHundredths, bool coloring);

/* Takes rate in bare unit (base 1024) per second. Prints 12 columns. */
void Service_printRate(RichString* str, double rate, bool coloring);

void Service_fillStarttimeBuffer(Service* this);

void Service_printLeftAlignedField(RichString* str, int attr, const char* content, unsigned int width);

void Service_printPercentage(float val, char* buffer, int n, int* attr);

void Service_display(const Object* cast, RichString* out);

void Service_done(Service* this);

extern const ServiceClass Service_class;

void Service_init(Service* this, const struct Settings_* settings);

void Service_toggleTag(Service* this);

bool Service_isNew(const Service* this);

bool Service_isTomb(const Service* this);

bool Service_setPriority(Service* this, int priority);

bool Service_changePriorityBy(Service* this, Arg delta);

bool Service_sendSignal(Service* this, Arg sgn);

int Service_pidCompare(const void* v1, const void* v2);

int Service_compareByKey_Base(const Service* p1, const Service* p2, ServiceField key);

// Avoid direct calls, use Service_getCommand instead
const char* Service_getCommandStr(const Service* this);

void Service_updateComm(Service* this, const char* comm);
void Service_updateCmdline(Service* this, const char* cmdline, int basenameStart, int basenameEnd);
void Service_updateExe(Service* this, const char* exe);

/* This function constructs the string that is displayed by
 * Service_writeCommand and also returned by Service_getCommandStr */
void Service_makeCommandStr(Service* this);

void Service_writeCommand(const Service* this, int attr, int baseAttr, RichString* str);

#endif
