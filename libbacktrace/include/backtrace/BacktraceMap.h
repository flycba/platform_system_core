/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BACKTRACE_BACKTRACE_MAP_H
#define _BACKTRACE_BACKTRACE_MAP_H

#include <stdint.h>
#include <sys/types.h>
#ifdef _WIN32
// MINGW does not define these constants.
#define PROT_NONE 0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4
#else
#include <sys/mman.h>
#endif

#include <deque>
#include <string>
#include <vector>

// Special flag to indicate a map is in /dev/. However, a map in
// /dev/ashmem/... does not set this flag.
static constexpr int PROT_DEVICE_MAP = 0x8000;

struct backtrace_map_t {
  uintptr_t start = 0;
  uintptr_t end = 0;
  uintptr_t offset = 0;
  uintptr_t load_bias = 0;
  int flags = 0;
  std::string name;
};

namespace unwindstack {
class Memory;
}

class BacktraceMap {
public:
  // If uncached is true, then parse the current process map as of the call.
  // Passing a map created with uncached set to true to Backtrace::Create()
  // is unsupported.
  static BacktraceMap* Create(pid_t pid, bool uncached = false);
  // Same as above, but is compatible with the new unwinder.
  static BacktraceMap* CreateNew(pid_t pid, bool uncached = false);

  static BacktraceMap* Create(pid_t pid, const std::vector<backtrace_map_t>& maps);

  virtual ~BacktraceMap();

  // Fill in the map data structure for the given address.
  virtual void FillIn(uintptr_t addr, backtrace_map_t* map);

  // Only supported with the new unwinder.
  virtual std::string GetFunctionName(uintptr_t /*pc*/, uintptr_t* /*offset*/) { return ""; }
  virtual std::shared_ptr<unwindstack::Memory> GetProcessMemory() { return nullptr; }

  // The flags returned are the same flags as used by the mmap call.
  // The values are PROT_*.
  int GetFlags(uintptr_t pc) {
    backtrace_map_t map;
    FillIn(pc, &map);
    if (IsValid(map)) {
      return map.flags;
    }
    return PROT_NONE;
  }

  bool IsReadable(uintptr_t pc) { return GetFlags(pc) & PROT_READ; }
  bool IsWritable(uintptr_t pc) { return GetFlags(pc) & PROT_WRITE; }
  bool IsExecutable(uintptr_t pc) { return GetFlags(pc) & PROT_EXEC; }

  // In order to use the iterators on this object, a caller must
  // call the LockIterator and UnlockIterator function to guarantee
  // that the data does not change while it's being used.
  virtual void LockIterator() {}
  virtual void UnlockIterator() {}

  typedef std::deque<backtrace_map_t>::iterator iterator;
  iterator begin() { return maps_.begin(); }
  iterator end() { return maps_.end(); }

  typedef std::deque<backtrace_map_t>::const_iterator const_iterator;
  const_iterator begin() const { return maps_.begin(); }
  const_iterator end() const { return maps_.end(); }

  size_t size() const { return maps_.size(); }

  virtual bool Build();

  static inline bool IsValid(const backtrace_map_t& map) {
    return map.end > 0;
  }

  void SetSuffixesToIgnore(std::vector<std::string> suffixes) {
    suffixes_to_ignore_.insert(suffixes_to_ignore_.end(), suffixes.begin(), suffixes.end());
  }

  const std::vector<std::string>& GetSuffixesToIgnore() { return suffixes_to_ignore_; }

 protected:
  BacktraceMap(pid_t pid);

  virtual bool ParseLine(const char* line, backtrace_map_t* map);

  pid_t pid_;
  std::deque<backtrace_map_t> maps_;
  std::vector<std::string> suffixes_to_ignore_;
};

class ScopedBacktraceMapIteratorLock {
public:
  explicit ScopedBacktraceMapIteratorLock(BacktraceMap* map) : map_(map) {
    map->LockIterator();
  }

  ~ScopedBacktraceMapIteratorLock() {
    map_->UnlockIterator();
  }

private:
  BacktraceMap* map_;
};

#endif // _BACKTRACE_BACKTRACE_MAP_H
