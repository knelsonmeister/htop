/*
htop - ServiceList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "ServiceList.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "DynamicColumn.h"
#include "Hashtable.h"
#include "Macros.h"
#include "Platform.h"
#include "Vector.h"
#include "XUtils.h"


ServiceList* ServiceList_init(ServiceList* this, const ObjectClass* klass, UsersTable* usersTable, Hashtable* dynamicMeters, Hashtable* dynamicColumns, Hashtable* pidMatchList, uid_t userId) {
   this->services = Vector_new(klass, true, DEFAULT_SIZE);
   this->services2 = Vector_new(klass, true, DEFAULT_SIZE); // tree-view auxiliary buffer

   this->serviceTable = Hashtable_new(200, false);
   this->displayTreeSet = Hashtable_new(200, false);
   this->draftingTreeSet = Hashtable_new(200, false);

   this->usersTable = usersTable;
   this->pidMatchList = pidMatchList;
   this->dynamicMeters = dynamicMeters;
   this->dynamicColumns = dynamicColumns;

   this->userId = userId;

   // set later by platform-specific code
   this->activeCPUs = 0;
   this->existingCPUs = 0;
   this->monotonicMs = 0;

   // always maintain valid realtime timestamps
   Platform_gettime_realtime(&this->realtime, &this->realtimeMs);

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   if (hwloc_topology_init(&this->topology) == 0) {
      this->topologyOk =
         #if HWLOC_API_VERSION < 0x00020000
         /* try to ignore the top-level machine object type */
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_MACHINE) &&
         /* ignore caches, which don't add structure */
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_CORE) &&
         0 == hwloc_topology_ignore_type_keep_structure(this->topology, HWLOC_OBJ_CACHE) &&
         0 == hwloc_topology_set_flags(this->topology, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM) &&
         #else
         0 == hwloc_topology_set_all_types_filter(this->topology, HWLOC_TYPE_FILTER_KEEP_STRUCTURE) &&
         #endif
         0 == hwloc_topology_load(this->topology);
   }
#endif

   this->following = -1;

   return this;
}

void ServiceList_done(ServiceList* this) {
#ifdef HAVE_LIBHWLOC
   if (this->topologyOk) {
      hwloc_topology_destroy(this->topology);
   }
#endif

   Hashtable_delete(this->draftingTreeSet);
   Hashtable_delete(this->displayTreeSet);
   Hashtable_delete(this->serviceTable);

   Vector_delete(this->services2);
   Vector_delete(this->services);
}

void ServiceList_setPanel(ServiceList* this, Panel* panel) {
   this->panel = panel;
}

static const char* alignedDynamicColumnTitle(const ServiceList* this, int key) {
   const DynamicColumn* column = Hashtable_get(this->dynamicColumns, key);
   if (column == NULL)
      return "- ";
   static char titleBuffer[DYNAMIC_MAX_COLUMN_WIDTH + /* space */ 1 + /* null terminator */ + 1];
   int width = column->width;
   if (!width || abs(width) > DYNAMIC_MAX_COLUMN_WIDTH)
      width = DYNAMIC_DEFAULT_COLUMN_WIDTH;
   xSnprintf(titleBuffer, sizeof(titleBuffer), "%*s", width, column->heading);
   return titleBuffer;
}

static const char* alignedServiceFieldTitle(const ServiceList* this, ServiceField field) {
   if (field >= LAST_SERVICEFIELD)
      return alignedDynamicColumnTitle(this, field);

   const char* title = Service_fields[field].title;
   if (!title)
      return "- ";

   if (!Service_fields[field].pidColumn)
      return title;

   static char titleBuffer[PROCESS_MAX_PID_DIGITS + /* space */ 1 + /* null-terminator */ + 1];
   xSnprintf(titleBuffer, sizeof(titleBuffer), "%*s ", Service_pidDigits, title);

   return titleBuffer;
}

void ServiceList_printHeader(const ServiceList* this, RichString* header) {
   RichString_rewind(header, RichString_size(header));

   const Settings* settings = this->settings;
   const ServiceField* fields = settings->fields;

   ServiceField key = Settings_getActiveSortKey(settings);

   for (int i = 0; fields[i]; i++) {
      int color;
      if (settings->treeView && settings->treeViewAlwaysByPID) {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      } else if (key == fields[i]) {
         color = CRT_colors[PANEL_SELECTION_FOCUS];
      } else {
         color = CRT_colors[PANEL_HEADER_FOCUS];
      }

      RichString_appendWide(header, color, alignedServiceFieldTitle(this, fields[i]));
      if (key == fields[i] && RichString_getCharVal(*header, RichString_size(header) - 1) == ' ') {
         RichString_rewind(header, 1);  // rewind to override space
         RichString_appendnWide(header,
                                CRT_colors[PANEL_SELECTION_FOCUS],
                                CRT_treeStr[Settings_getActiveDirection(this->settings) == 1 ? TREE_STR_ASC : TREE_STR_DESC],
                                1);
      }
      if (COMM == fields[i] && settings->showMergedCommand) {
         RichString_appendAscii(header, color, "(merged)");
      }
   }
}

void ServiceList_add(ServiceList* this, Service* p) {
   assert(Vector_indexOf(this->services, p, Service_pidCompare) == -1);
   assert(Hashtable_get(this->serviceTable, p->pid) == NULL);
   p->serviceList = this;

   // highlighting services found in first scan by first scan marked "far in the past"
   p->seenStampMs = this->monotonicMs;

   Vector_add(this->services, p);
   Hashtable_put(this->serviceTable, p->pid, p);

   assert(Vector_indexOf(this->services, p, Service_pidCompare) != -1);
   assert(Hashtable_get(this->serviceTable, p->pid) != NULL);
   assert(Hashtable_count(this->serviceTable) == Vector_count(this->services));
}

void ServiceList_remove(ServiceList* this, const Service* p) {
   assert(Vector_indexOf(this->services, p, Service_pidCompare) != -1);
   assert(Hashtable_get(this->serviceTable, p->pid) != NULL);

   const Service* pp = Hashtable_remove(this->serviceTable, p->pid);
   assert(pp == p); (void)pp;

   pid_t pid = p->pid;
   int idx = Vector_indexOf(this->services, p, Service_pidCompare);
   assert(idx != -1);

   if (idx >= 0) {
      Vector_remove(this->services, idx);
   }

   if (this->following != -1 && this->following == pid) {
      this->following = -1;
      Panel_setSelectionColor(this->panel, PANEL_SELECTION_FOCUS);
   }

   assert(Hashtable_get(this->serviceTable, pid) == NULL);
   assert(Hashtable_count(this->serviceTable) == Vector_count(this->services));
}

// ServiceList_updateTreeSetLayer sorts this->displayTreeSet,
// relying only on itself.
//
// Algorithm
//
// The algorithm is based on `depth-first search`,
// even though `breadth-first search` approach may be more efficient on first glance,
// after comparison it may be not, as it's not safe to go deeper without first updating the tree structure.
// If it would be safe that approach would likely bring an advantage in performance.
//
// Each call of the function looks for a 'layer'. A 'layer' is a list of services with the same depth.
// First it sorts a list. Then it runs the function recursively for each element of the sorted list.
// After that it updates the settings of services.
//
// It relies on `leftBound` and `rightBound` as an optimization to cut the list size at the time it builds a 'layer'.
//
// It uses a temporary Hashtable `draftingTreeSet` because it's not safe to traverse a tree
// and at the same time make changes in it.
//
static void ServiceList_updateTreeSetLayer(ServiceList* this, unsigned int leftBound, unsigned int rightBound, unsigned int deep, unsigned int left, unsigned int right, unsigned int* index, unsigned int* treeIndex, int indent) {

   // It's guaranteed that layer_size is enough space
   // but most likely it needs less. Specifically on first iteration.
   int layerSize = (right - left) / 2;

   // check if we reach `children` of `leaves`
   if (layerSize == 0)
      return;

   Vector* layer = Vector_new(Vector_type(this->services), false, layerSize);

   // Find all services on the same layer (process with the same `deep` value
   // and included in a range from `leftBound` to `rightBound`).
   //
   // This loop also keeps track of left_bound and right_bound of these services
   // in order not to lose this information once the list is sorted.
   //
   // The variables left_bound and right_bound are different from what the values lhs and rhs represent.
   // While left_bound and right_bound define a range of services to look at, the values given by lhs and rhs are indices into an array
   //
   // In the below example note how filtering a range of indices i is different from filtering for services in the bounds left_bound < x < right_bound …
   //
   // The nested tree set is sorted by left value, which is guaranteed upon entry/exit of this function.
   //
   // i | l | r
   // 1 | 1 | 9
   // 2 | 2 | 8
   // 3 | 4 | 5
   // 4 | 6 | 7
   for (unsigned int i = leftBound; i < rightBound; i++) {
      Service* proc = (Service*)Hashtable_get(this->displayTreeSet, i);
      assert(proc);
      if (proc && proc->tree_depth == deep && proc->tree_left > left && proc->tree_right < right) {
         if (Vector_size(layer) > 0) {
            Service* previous_process = (Service*)Vector_get(layer, Vector_size(layer) - 1);

            // Make a 'right_bound' of previous_process in a layer the current process's index.
            //
            // Use 'tree_depth' as a temporal variable.
            // It's safe to do as later 'tree_depth' will be renovated.
            previous_process->tree_depth = proc->tree_index;
         }

         Vector_add(layer, proc);
      }
   }

   // The loop above changes just up to process-1.
   // So the last process of the layer isn't updated by the above code.
   //
   // Thus, if present, set the `rightBound` to the last process on the layer
   if (Vector_size(layer) > 0) {
      Service* previous_process = (Service*)Vector_get(layer, Vector_size(layer) - 1);
      previous_process->tree_depth = rightBound;
   }

   Vector_quickSort(layer);

   int size = Vector_size(layer);
   for (int i = 0; i < size; i++) {
      Service* proc = (Service*)Vector_get(layer, i);

      unsigned int idx = (*index)++;
      int newLeft = (*treeIndex)++;

      int level = deep == 0 ? 0 : (int)deep - 1;
      int currentIndent = indent == -1 ? 0 : indent | (1 << level);
      int nextIndent = indent == -1 ? 0 : ((i < size - 1) ? currentIndent : indent);

      unsigned int newLeftBound = proc->tree_index;
      unsigned int newRightBound = proc->tree_depth;
      ServiceList_updateTreeSetLayer(this, newLeftBound, newRightBound, deep + 1, proc->tree_left, proc->tree_right, index, treeIndex, nextIndent);

      int newRight = (*treeIndex)++;

      proc->tree_left = newLeft;
      proc->tree_right = newRight;
      proc->tree_index = idx;
      proc->tree_depth = deep;

      if (indent == -1) {
         proc->indent = 0;
      } else if (i == size - 1) {
         proc->indent = -currentIndent;
      } else {
         proc->indent = currentIndent;
      }

      Hashtable_put(this->draftingTreeSet, proc->tree_index, proc);

      // It's not strictly necessary to do this, but doing so anyways
      // allows for checking the correctness of the inner workings.
      Hashtable_remove(this->displayTreeSet, newLeftBound);
   }

   Vector_delete(layer);
}

static void ServiceList_updateTreeSet(ServiceList* this) {
   unsigned int index = 0;
   unsigned int tree_index = 1;

   const int vsize = Vector_size(this->services);

   assert(Hashtable_count(this->draftingTreeSet) == 0);
   assert((int)Hashtable_count(this->displayTreeSet) == vsize);

   ServiceList_updateTreeSetLayer(this, 0, vsize, 0, 0, vsize * 2 + 1, &index, &tree_index, -1);

   Hashtable* tmp = this->draftingTreeSet;
   this->draftingTreeSet = this->displayTreeSet;
   this->displayTreeSet = tmp;

   assert(Hashtable_count(this->draftingTreeSet) == 0);
   assert((int)Hashtable_count(this->displayTreeSet) == vsize);
}

static void ServiceList_buildTreeBranch(ServiceList* this, pid_t pid, int level, int indent, int direction, bool show, int* node_counter, int* node_index) {
   // On OpenBSD the kernel thread 'swapper' has pid 0.
   // Do not treat it as root of any tree.
   if (pid == 0)
      return;

   Vector* children = Vector_new(Class(Service), false, DEFAULT_SIZE);

   for (int i = Vector_size(this->services) - 1; i >= 0; i--) {
      Service* service = (Service*)Vector_get(this->services, i);
      if (service->show && Service_isChildOf(service, pid)) {
         service = (Service*)Vector_take(this->services, i);
         Vector_add(children, service);
      }
   }

   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      int index = (*node_index)++;
      Service* service = (Service*)Vector_get(children, i);

      int lft = (*node_counter)++;

      if (!show) {
         service->show = false;
      }

      int s = Vector_size(this->services2);
      if (direction == 1) {
         Vector_add(this->services2, service);
      } else {
         Vector_insert(this->services2, 0, service);
      }

      assert(Vector_size(this->services2) == s + 1); (void)s;

      int nextIndent = indent | (1 << level);
      ServiceList_buildTreeBranch(this, service->pid, level + 1, (i < size - 1) ? nextIndent : indent, direction, show ? service->showChildren : false, node_counter, node_index);
      if (i == size - 1) {
         service->indent = -nextIndent;
      } else {
         service->indent = nextIndent;
      }

      int rht = (*node_counter)++;

      service->tree_left = lft;
      service->tree_right = rht;
      service->tree_depth = level + 1;
      service->tree_index = index;
      Hashtable_put(this->displayTreeSet, index, service);
   }
   Vector_delete(children);
}

static int ServiceList_treeServiceCompare(const void* v1, const void* v2) {
   const Service* p1 = (const Service*)v1;
   const Service* p2 = (const Service*)v2;

   return SPACESHIP_NUMBER(p1->tree_left, p2->tree_left);
}

static int ServiceList_treeServiceCompareByPID(const void* v1, const void* v2) {
   const Service* p1 = (const Service*)v1;
   const Service* p2 = (const Service*)v2;

   return SPACESHIP_NUMBER(p1->pid, p2->pid);
}

// Builds a sorted tree from scratch, without relying on previously gathered information
static void ServiceList_buildTree(ServiceList* this) {
   int node_counter = 1;
   int node_index = 0;
   int direction = Settings_getActiveDirection(this->settings);

   // Sort by PID
   Vector_quickSortCustomCompare(this->services, ServiceList_treeServiceCompareByPID);
   int vsize = Vector_size(this->services);

   // Find all services whose parent is not visible
   int size;
   while ((size = Vector_size(this->services))) {
      int i;
      for (i = 0; i < size; i++) {
         Service* service = (Service*)Vector_get(this->services, i);

         // Immediately consume services hidden from view
         if (!service->show) {
            service = (Service*)Vector_take(this->services, i);
            service->indent = 0;
            service->tree_depth = 0;
            service->tree_left = node_counter++;
            service->tree_index = node_index++;
            Vector_add(this->services2, service);
            ServiceList_buildTreeBranch(this, service->pid, 0, 0, direction, false, &node_counter, &node_index);
            service->tree_right = node_counter++;
            Hashtable_put(this->displayTreeSet, service->tree_index, service);
            break;
         }

         pid_t ppid = Service_getParentPid(service);

         // Bisect the service vector to find parent
         int l = 0;
         int r = size;

         // If PID corresponds with PPID (e.g. "kernel_task" (PID:0, PPID:0)
         // on Mac OS X 10.11.6) cancel bisecting and regard this service as
         // root.
         if (service->pid == ppid)
            r = 0;

         // On Linux both the init service (pid 1) and the root UMH kernel thread (pid 2)
         // use a ppid of 0. As that PID can't exist, we can skip searching for it.
         if (!ppid)
            r = 0;

         while (l < r) {
            int c = (l + r) / 2;
            pid_t pid = ((Service*)Vector_get(this->services, c))->pid;
            if (ppid == pid) {
               break;
            } else if (ppid < pid) {
               r = c;
            } else {
               l = c + 1;
            }
         }

         // If parent not found, then construct the tree with this node as root
         if (l >= r) {
            service = (Service*)Vector_take(this->services, i);
            service->indent = 0;
            service->tree_depth = 0;
            service->tree_left = node_counter++;
            service->tree_index = node_index++;
            Vector_add(this->services2, service);
            Hashtable_put(this->displayTreeSet, service->tree_index, service);
            ServiceList_buildTreeBranch(this, service->pid, 0, 0, direction, service->showChildren, &node_counter, &node_index);
            service->tree_right = node_counter++;
            break;
         }
      }

      // There should be no loop in the service tree
      assert(i < size);
   }

   // Swap listings around
   Vector* t = this->services;
   this->services = this->services2;
   this->services2 = t;

   // Check consistency of the built structures
   assert(Vector_size(this->services) == vsize); (void)vsize;
   assert(Vector_size(this->services2) == 0);
}

void ServiceList_sort(ServiceList* this) {
   if (this->settings->treeView) {
      ServiceList_updateTreeSet(this);
      Vector_quickSortCustomCompare(this->services, ServiceList_treeServiceCompare);
   } else {
      Vector_insertionSort(this->services);
   }
}

ServiceField ServiceList_keyAt(const ServiceList* this, int at) {
   int x = 0;
   const ServiceField* fields = this->settings->fields;
   ServiceField field;
   for (int i = 0; (field = fields[i]); i++) {
      int len = strlen(alignedServiceFieldTitle(this, field));
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return COMM;
}

void ServiceList_expandTree(ServiceList* this) {
   int size = Vector_size(this->services);
   for (int i = 0; i < size; i++) {
      Service* service = (Service*) Vector_get(this->services, i);
      service->showChildren = true;
   }
}

void ServiceList_collapseAllBranches(ServiceList* this) {
   int size = Vector_size(this->services);
   for (int i = 0; i < size; i++) {
      Service* service = (Service*) Vector_get(this->services, i);
      // FreeBSD has pid 0 = kernel and pid 1 = init, so init has tree_depth = 1
      if (service->tree_depth > 0 && service->pid > 1)
         service->showChildren = false;
   }
}

void ServiceList_rebuildPanel(ServiceList* this) {
   const char* incFilter = this->incFilter;

   const int currPos = Panel_getSelectedIndex(this->panel);
   const int currScrollV = this->panel->scrollV;
   const int currSize = Panel_size(this->panel);

   Panel_prune(this->panel);

   /* Follow main service if followed a userland thread and threads are now hidden */
   const Settings* settings = this->settings;
   if (this->following != -1 && settings->hideUserlandThreads) {
      const Service* followedService = (const Service*) Hashtable_get(this->serviceTable, this->following);
      if (followedService && Service_isThread(followedService) && Hashtable_get(this->serviceTable, followedService->tgid) != NULL) {
         this->following = followedService->tgid;
      }
   }

   const int serviceCount = Vector_size(this->services);
   int idx = 0;
   bool foundFollowed = false;

   for (int i = 0; i < serviceCount; i++) {
      Service* p = (Service*) Vector_get(this->services, i);

      if ( (!p->show)
         || (this->userId != (uid_t) -1 && (p->st_uid != this->userId))
         || (incFilter && !(String_contains_i(Service_getCommand(p), incFilter)))
         || (this->pidMatchList && !Hashtable_get(this->pidMatchList, p->tgid)) )
         continue;

      Panel_set(this->panel, idx, (Object*)p);

      if (this->following != -1 && p->pid == this->following) {
         foundFollowed = true;
         Panel_setSelected(this->panel, idx);
         this->panel->scrollV = currScrollV;
      }
      idx++;
   }

   if (this->following != -1 && !foundFollowed) {
      /* Reset if current followed pid not found */
      this->following = -1;
      Panel_setSelectionColor(this->panel, PANEL_SELECTION_FOCUS);
   }

   if (this->following == -1) {
      /* If the last item was selected, keep the new last item selected */
      if (currPos > 0 && currPos == currSize - 1)
         Panel_setSelected(this->panel, Panel_size(this->panel) - 1);
      else
         Panel_setSelected(this->panel, currPos);

      this->panel->scrollV = currScrollV;
   }
}

Service* ServiceList_getService(ServiceList* this, pid_t pid, bool* preExisting, Service_New constructor) {
   Service* proc = (Service*) Hashtable_get(this->serviceTable, pid);
   *preExisting = proc != NULL;
   if (proc) {
      assert(Vector_indexOf(this->services, proc, Service_pidCompare) != -1);
      assert(proc->pid == pid);
   } else {
      proc = constructor(this->settings);
      assert(proc->cmdline == NULL);
      proc->pid = pid;
   }
   return proc;
}

void ServiceList_scan(ServiceList* this, bool pauseServiceUpdate) {
   // in pause mode only gather global data for meters (CPU/memory/...)
   if (pauseServiceUpdate) {
      ServiceList_goThroughEntries(this, true);
      return;
   }

   // mark all service as "dirty"
   for (int i = 0; i < Vector_size(this->services); i++) {
      Service* p = (Service*) Vector_get(this->services, i);
      p->updated = false;
      p->wasShown = p->show;
      p->show = true;
   }

   this->totalTasks = 0;
   this->userlandThreads = 0;
   this->kernelThreads = 0;
   this->runningTasks = 0;


   // set scan timestamp
   static bool firstScanDone = false;
   if (firstScanDone) {
      Platform_gettime_monotonic(&this->monotonicMs);
   } else {
      this->monotonicMs = 0;
      firstScanDone = true;
   }

   ServiceList_goThroughEntries(this, false);

   // for (int i = Vector_size(this->services) - 1; i >= 0; i--) {
   //    Service* p = (Service*) Vector_get(this->services, i);
   //    Service_makeCommandStr(p);

   //    if (p->tombStampMs > 0) {
   //       // remove tombed service
   //       if (this->monotonicMs >= p->tombStampMs) {
   //          ServiceList_remove(this, p);
   //       }
   //    } else if (p->updated == false) {
   //       // service no longer exists
   //       if (this->settings->highlightChanges && p->wasShown) {
   //          // mark tombed
   //          p->tombStampMs = this->monotonicMs + 1000 * this->settings->highlightDelaySecs;
   //       } else {
   //          // immediately remove
   //          ServiceList_remove(this, p);
   //       }
   //    }
   // }

   // if (this->settings->treeView) {
   //    // Clear out the hashtable to avoid any left-over services from previous build
   //    //
   //    // The sorting algorithm relies on the fact that
   //    // len(this->displayTreeSet) == len(this->services)
   //    Hashtable_clear(this->displayTreeSet);

   //    ServiceList_buildTree(this);
   // }
}
