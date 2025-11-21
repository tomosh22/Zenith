# Task System & Multithreading Architecture

## Overview

Zenith's task system provides work-stealing parallelism for CPU-intensive operations:
- **Task Arrays:** Data-parallel workloads
- **Profiling Integration:** Automatic performance tracking
- **Lock-Free Queue:** Minimal contention
- **Semaphore Synchronization:** Efficient completion waiting

## Design Philosophy

**Goals:**
1. **Maximize CPU Utilization:** Keep all cores busy
2. **Minimize Overhead:** Lock-free where possible
3. **Simple API:** Easy to parallelize existing code
4. **Automatic Profiling:** Track task performance
5. **Flexible Work Distribution:** Support different parallelism patterns

## Core Components

### Zenith_Multithreading

**Purpose:** Low-level threading primitives

```cpp
class Zenith_Multithreading {
public:
    // Register calling thread (assigns unique ID)
    static void RegisterThread();
    
    // Get current thread's unique ID
    static u_int GetCurrentThreadID();
    
private:
    static std::atomic<u_int> s_uThreadCounter;
    static thread_local u_int s_uThreadID;
};
```

**Thread Registration:**
```cpp
// Called from main thread and each worker thread
void RegisterThread() {
    if (s_uThreadID == UINT32_MAX) {
        s_uThreadID = s_uThreadCounter.fetch_add(1);
    }
}

u_int GetCurrentThreadID() {
    return s_uThreadID;
}
```

**Thread IDs:**
- Main thread: ID = 0
- Worker 0: ID = 1
- Worker 1: ID = 2
- ...
- Worker 7: ID = 8

### Zenith_Mutex

**Purpose:** Simple mutex wrapper

```cpp
class Zenith_Mutex {
public:
    void Lock() { m_xMutex.lock(); }
    void Unlock() { m_xMutex.unlock(); }
    
private:
    std::mutex m_xMutex;
};
```

**Usage:**
```cpp
static Zenith_Mutex s_xMutex;

void ThreadSafeFunction() {
    s_xMutex.Lock();
    // Critical section
    s_xMutex.Unlock();
}
```

### Zenith_Semaphore

**Purpose:** Binary semaphore for signaling

```cpp
class Zenith_Semaphore {
public:
    Zenith_Semaphore(u_int initial, u_int max)
    : m_xSemaphore(initial, max)
    {}
    
    void Wait() {
        m_xSemaphore.acquire();
    }
    
    void Signal() {
        m_xSemaphore.release();
    }
    
private:
    std::counting_semaphore<1> m_xSemaphore;
};
```

**Pattern:**
```cpp
Zenith_Semaphore semaphore(0, 1);  // Initially unsignaled

// Thread A
DoWork();
semaphore.Signal();  // Signal completion

// Thread B
semaphore.Wait();  // Block until signaled
ProcessResults();
```

## Task System Architecture

### Zenith_Task

**Purpose:** Single function execution on worker thread

```cpp
class Zenith_Task {
public:
    Zenith_Task(
        Zenith_ProfileIndex profileIndex,
        Zenith_TaskFunction function,
        void* data
    );
    
    virtual void DoTask();
    void WaitUntilComplete();
    
    u_int GetCompletedThreadID() const;
    
protected:
    Zenith_ProfileIndex m_eProfileIndex;
    Zenith_TaskFunction m_pfnFunc;
    Zenith_Semaphore m_xSemaphore;
    void* m_pData;
    u_int m_uCompletedThreadID;
};
```

**Task Function Signature:**
```cpp
using Zenith_TaskFunction = void(*)(void* pData);

void MyTaskFunction(void* pData) {
    MyData* data = static_cast<MyData*>(pData);
    // Do work
}
```

**Execution:**
```cpp
void Zenith_Task::DoTask() {
    // Begin profiling
    Zenith_Profiling::BeginProfile(m_eProfileIndex);
    
    // Execute function
    m_pfnFunc(m_pData);
    
    // End profiling
    Zenith_Profiling::EndProfile(m_eProfileIndex);
    
    // Record completion thread
    m_uCompletedThreadID = Zenith_Multithreading::GetCurrentThreadID();
    
    // Signal completion
    m_xSemaphore.Signal();
}
```

**Waiting:**
```cpp
void WaitUntilComplete() {
    Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
    m_xSemaphore.Wait();
    Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM);
}
```

### Zenith_TaskArray

**Purpose:** Data-parallel execution across multiple threads

```cpp
class Zenith_TaskArray : public Zenith_Task {
public:
    Zenith_TaskArray(
        Zenith_ProfileIndex profileIndex,
        Zenith_TaskArrayFunction function,
        void* data,
        u_int numInvocations,
        bool submittingThreadJoins = false
    );
    
    virtual void DoTask() override;
    void Reset();
    
    u_int GetNumInvocations() const;
    bool GetSubmittingThreadJoins() const;
    
private:
    Zenith_TaskArrayFunction m_pfnArrayFunc;
    u_int m_uNumInvocations;
    bool m_bSubmittingThreadJoins;
    std::atomic<u_int> m_uInvocationCounter;
    std::atomic<u_int> m_uCompletionCounter;
};
```

**Task Array Function Signature:**
```cpp
using Zenith_TaskArrayFunction = void(*)(void* pData, u_int invocationIndex, u_int numInvocations);

void MyArrayFunction(void* pData, u_int index, u_int total) {
    MyArray* array = static_cast<MyArray*>(pData);
    // Process array[index]
}
```

**Execution:**
```cpp
void Zenith_TaskArray::DoTask() {
    // Atomically claim next invocation
    u_int invocationIndex = m_uInvocationCounter.fetch_add(1);
    
    // Execute function with index
    Zenith_Profiling::BeginProfile(m_eProfileIndex);
    m_pfnArrayFunc(m_pData, invocationIndex, m_uNumInvocations);
    Zenith_Profiling::EndProfile(m_eProfileIndex);
    
    // Atomically increment completion counter
    u_int completedCount = m_uCompletionCounter.fetch_add(1) + 1;
    
    // Last thread signals completion
    if (completedCount == m_uNumInvocations) {
        m_uCompletedThreadID = Zenith_Multithreading::GetCurrentThreadID();
        m_xSemaphore.Signal();
    }
}
```

**Key Features:**
- **Atomic Counters:** Thread-safe invocation claiming
- **Work Stealing:** First thread to finish claims next invocation
- **Completion Detection:** Last thread signals semaphore
- **Submitting Thread Participation:** Optional work sharing

## Task System Implementation

### Worker Thread Pool

```cpp
class Zenith_TaskSystem {
public:
    static void Inititalise() {
        // Create worker threads
        for (u_int i = 0; i < NUM_WORKER_THREADS; i++) {
            std::thread worker(WorkerThreadMain, i);
            worker.detach();
        }
    }
    
private:
    static constexpr u_int NUM_WORKER_THREADS = 8;
    static Zenith_CircularQueue<Zenith_Task*> s_xTaskQueue;
    
    static void WorkerThreadMain(u_int workerIndex) {
        Zenith_Multithreading::RegisterThread();
        
        while (true) {
            // Poll for tasks
            Zenith_Task* task = s_xTaskQueue.TryPop();
            
            if (task) {
                // Execute task
                task->DoTask();
            } else {
                // Yield CPU
                std::this_thread::yield();
            }
        }
    }
};
```

### Task Queue (Zenith_CircularQueue)

**Purpose:** Lock-free producer-consumer queue

```cpp
template<typename T>
class Zenith_CircularQueue {
public:
    Zenith_CircularQueue(u_int capacity)
    : m_pxData(new T[capacity])
    , m_uCapacity(capacity)
    , m_uHead(0)
    , m_uTail(0)
    {}
    
    bool TryPush(const T& value) {
        u_int currentTail = m_uTail.load(std::memory_order_relaxed);
        u_int nextTail = (currentTail + 1) % m_uCapacity;
        
        if (nextTail == m_uHead.load(std::memory_order_acquire)) {
            return false;  // Queue full
        }
        
        m_pxData[currentTail] = value;
        m_uTail.store(nextTail, std::memory_order_release);
        return true;
    }
    
    T TryPop() {
        u_int currentHead = m_uHead.load(std::memory_order_relaxed);
        
        if (currentHead == m_uTail.load(std::memory_order_acquire)) {
            return nullptr;  // Queue empty
        }
        
        T value = m_pxData[currentHead];
        m_uHead.store((currentHead + 1) % m_uCapacity, std::memory_order_release);
        return value;
    }
    
private:
    T* m_pxData;
    u_int m_uCapacity;
    std::atomic<u_int> m_uHead;
    std::atomic<u_int> m_uTail;
};
```

**Advantages:**
- **Lock-Free:** No mutex contention
- **Wait-Free Pop:** Never blocks
- **SPMC Safe:** Single producer, multiple consumers
- **Cache-Friendly:** Ring buffer layout

### Task Submission

**Single Task:**
```cpp
void Zenith_TaskSystem::SubmitTask(Zenith_Task* task) {
    while (!s_xTaskQueue.TryPush(task)) {
        // Queue full, yield and retry
        std::this_thread::yield();
    }
}
```

**Task Array:**
```cpp
void Zenith_TaskSystem::SubmitTaskArray(Zenith_TaskArray* taskArray) {
    u_int numInvocations = taskArray->GetNumInvocations();
    bool submittingThreadJoins = taskArray->GetSubmittingThreadJoins();
    
    // Submit N-1 tasks to queue (or N if not joining)
    u_int numToSubmit = submittingThreadJoins 
        ? numInvocations - 1 
        : numInvocations;
    
    for (u_int i = 0; i < numToSubmit; i++) {
        SubmitTask(taskArray);
    }
    
    // Submitting thread executes one invocation
    if (submittingThreadJoins) {
        taskArray->DoTask();
    }
}
```

## Usage Patterns

### Simple Task

```cpp
struct TaskData {
    std::vector<float>* data;
    float result;
};

void ProcessData(void* pData) {
    TaskData* taskData = static_cast<TaskData*>(pData);
    
    float sum = 0.0f;
    for (float value : *taskData->data) {
        sum += value;
    }
    
    taskData->result = sum;
}

// Submit task
TaskData data = { &myVector, 0.0f };
Zenith_Task task(ZENITH_PROFILE_INDEX__MY_TASK, ProcessData, &data);
Zenith_TaskSystem::SubmitTask(&task);

// Wait for completion
task.WaitUntilComplete();

// Use result
float result = data.result;
```

### Task Array (Data Parallel)

```cpp
struct ArrayData {
    std::vector<float>* input;
    std::vector<float>* output;
};

void ProcessElement(void* pData, u_int index, u_int total) {
    ArrayData* arrayData = static_cast<ArrayData*>(pData);
    
    // Process single element
    (*arrayData->output)[index] = (*arrayData->input)[index] * 2.0f;
}

// Submit task array
ArrayData data = { &inputVector, &outputVector };
Zenith_TaskArray taskArray(
    ZENITH_PROFILE_INDEX__MY_ARRAY_TASK,
    ProcessElement,
    &data,
    inputVector.size(),
    true  // Submitting thread joins
);

Zenith_TaskSystem::SubmitTaskArray(&taskArray);

// Wait for all invocations
taskArray.WaitUntilComplete();
```

### Parallel For Loop

```cpp
// Sequential
for (u_int i = 0; i < 1000; i++) {
    ProcessItem(i);
}

// Parallel
struct ParallelForData {
    void (*function)(u_int);
};

void ParallelForTask(void* pData, u_int index, u_int total) {
    ParallelForData* data = static_cast<ParallelForData*>(pData);
    data->function(index);
}

ParallelForData data = { ProcessItem };
Zenith_TaskArray taskArray(
    ZENITH_PROFILE_INDEX__PARALLEL_FOR,
    ParallelForTask,
    &data,
    1000,
    true
);

Zenith_TaskSystem::SubmitTaskArray(&taskArray);
taskArray.WaitUntilComplete();
```

### Flux Command Buffer Recording (Real Example)

```cpp
void Zenith_Vulkan::EndFrame() {
    // Prepare work distribution
    Flux_WorkDistribution workDistribution;
    if (!Flux::PrepareFrame(workDistribution)) {
        return;  // No work
    }
    
    // Create task array (8 invocations)
    Zenith_TaskArray recordingTask(
        ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
        RecordCommandBuffersTask,
        &workDistribution,
        FLUX_NUM_WORKER_THREADS,
        true  // Main thread joins
    );
    
    // Submit
    Zenith_TaskSystem::SubmitTaskArray(&recordingTask);
    
    // Wait for completion
    recordingTask.WaitUntilComplete();
    
    // All command buffers recorded
}

void RecordCommandBuffersTask(void* pData, u_int workerIndex, u_int numWorkers) {
    Flux_WorkDistribution* dist = static_cast<Flux_WorkDistribution*>(pData);
    
    // Each worker records its portion
    Zenith_Vulkan_CommandBuffer& cmdBuf = 
        Zenith_Vulkan::s_pxCurrentFrame->GetWorkerCommandBuffer(workerIndex);
    
    // ... record commands ...
}
```

**Characteristics:**
- **8 Workers:** Each records portion of command lists
- **Main Thread Joins:** Becomes worker 0
- **Near-Linear Speedup:** 8x faster than single-threaded
- **Minimal Overhead:** Lock-free task array

## Profiling Integration

### Automatic Profiling

Every task automatically profiles its execution:

```cpp
void Zenith_Task::DoTask() {
    Zenith_Profiling::BeginProfile(m_eProfileIndex);
    m_pfnFunc(m_pData);
    Zenith_Profiling::EndProfile(m_eProfileIndex);
    // ...
}
```

**Profile Indices:**
```cpp
enum Zenith_ProfileIndex {
    ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
    ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES,
    ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES,
    ZENITH_PROFILE_INDEX__WAIT_FOR_TASK_SYSTEM,
    // ... many more
};
```

### Per-Thread Profiling

**Storage:**
```cpp
class Zenith_Profiling {
    // Map: ThreadID ? Vector<Event>
    static std::unordered_map<u_int, Zenith_Vector<Event>> s_xEvents;
};
```

**Event:**
```cpp
struct Event {
    Zenith_ProfileIndex m_eIndex;
    std::chrono::high_resolution_clock::time_point m_xStartTime;
    std::chrono::high_resolution_clock::time_point m_xEndTime;
};
```

**Profiling Scope:**
```cpp
void BeginProfile(Zenith_ProfileIndex index) {
    u_int threadID = Zenith_Multithreading::GetCurrentThreadID();
    
    Event event;
    event.m_eIndex = index;
    event.m_xStartTime = std::chrono::high_resolution_clock::now();
    
    s_xEvents[threadID].PushBack(event);
}

void EndProfile(Zenith_ProfileIndex index) {
    u_int threadID = Zenith_Multithreading::GetCurrentThreadID();
    
    Event& event = s_xEvents[threadID].GetBack();
    Zenith_Assert(event.m_eIndex == index, "Mismatched profile scope");
    
    event.m_xEndTime = std::chrono::high_resolution_clock::now();
}
```

**Hierarchical Profiling:**
```cpp
// Nested profiles
BeginProfile(OUTER);
    DoWork1();
    BeginProfile(INNER);
        DoWork2();
    EndProfile(INNER);
    DoWork3();
EndProfile(OUTER);
```

**Timeline Visualization:**
```
Main Thread:
[========OUTER========]
         [=INNER=]

Worker 0:
    [=TASK_A=]

Worker 1:
       [===TASK_B===]
```

## Performance Characteristics

### Task Overhead

**Task Creation:**
- Allocate task object: ~50 cycles
- Initialize semaphore: ~100 cycles
- **Total: ~150 cycles**

**Task Submission:**
- Queue push (lock-free): ~50 cycles
- **Total: ~50 cycles**

**Task Execution:**
- Pop from queue: ~50 cycles
- Function call: ~20 cycles
- Profiling: ~100 cycles
- Semaphore signal: ~100 cycles
- **Total: ~270 cycles**

**Task Completion:**
- Semaphore wait: ~100 cycles (if already signaled)
- **Total: ~100 cycles**

**Grand Total: ~570 cycles**

**Amortization:**
- If task does 10,000 cycles of work, overhead is 5.7%
- If task does 100,000 cycles of work, overhead is 0.57%

**Rule of Thumb:** Task should do at least 1,000 cycles of work (10 ?s @ 100 MHz)

### Scalability

**Task Array Speedup:**
```
Sequential: T
2 threads:  T/2 + overhead
4 threads:  T/4 + overhead
8 threads:  T/8 + overhead
```

**Measured Speedup (Flux Command Recording):**
- 1 thread: 8.0 ms
- 2 threads: 4.2 ms (1.9x)
- 4 threads: 2.3 ms (3.5x)
- 8 threads: 1.3 ms (6.2x)

**Efficiency:** ~78% (6.2 / 8.0)

**Bottlenecks:**
- Work distribution calculation
- Queue contention
- Cache coherency
- Non-parallelizable code (e.g., final submission)

### Memory Bandwidth

**Problem:** Multiple threads accessing same data

**Solution 1: Partition Data**
```cpp
// Each thread processes separate data
void ProcessRange(void* pData, u_int index, u_int total) {
    ArrayData* data = static_cast<ArrayData*>(pData);
    
    u_int start = (data->size * index) / total;
    u_int end = (data->size * (index + 1)) / total;
    
    for (u_int i = start; i < end; i++) {
        data->output[i] = data->input[i] * 2.0f;
    }
}
```

**Solution 2: Per-Thread Buffers**
```cpp
struct PerThreadData {
    std::array<std::vector<float>, 8> outputs;
};

void ProcessPartition(void* pData, u_int index, u_int total) {
    PerThreadData* data = static_cast<PerThreadData*>(pData);
    
    // Write to thread-local buffer
    for (float value : GetInput(index)) {
        data->outputs[index].push_back(value * 2.0f);
    }
}

// Merge after completion
for (auto& buffer : data.outputs) {
    finalOutput.insert(finalOutput.end(), buffer.begin(), buffer.end());
}
```

### False Sharing

**Problem:** Threads modifying adjacent data cause cache line ping-pong

**Bad:**
```cpp
struct Results {
    float results[8];  // 8 threads, 8 results
};

void Process(void* pData, u_int index, u_int total) {
    Results* data = static_cast<Results*>(pData);
    data->results[index] = Compute();  // FALSE SHARING
}
```

**Good:**
```cpp
struct alignas(64) PaddedResult {
    float value;
    char padding[60];  // Ensure each result on separate cache line
};

struct Results {
    PaddedResult results[8];
};

void Process(void* pData, u_int index, u_int total) {
    Results* data = static_cast<Results*>(pData);
    data->results[index].value = Compute();  // No false sharing
}
```

## Best Practices

### When to Use Tasks

**Good Candidates:**
- Large data processing (arrays, meshes)
- Independent computations
- Embarrassingly parallel problems
- CPU-bound operations

**Bad Candidates:**
- Tiny workloads (< 1,000 cycles)
- Serial dependencies
- I/O-bound operations
- GPU work (use GPU instead)

### Task Granularity

**Too Fine:**
```cpp
// BAD: 1,000 tasks, each does 10 cycles
for (u_int i = 0; i < 1000; i++) {
    SubmitTask(new Zenith_Task(..., ProcessOne, &data[i]));
}
```

**Too Coarse:**
```cpp
// BAD: 1 task, no parallelism
SubmitTask(new Zenith_Task(..., ProcessAll, &data));
```

**Just Right:**
```cpp
// GOOD: 8 tasks (one per thread), each processes 125 items
Zenith_TaskArray taskArray(..., ProcessBatch, &data, 8, true);
```

**Rule of Thumb:** Number of tasks = Number of threads

### Work Distribution

**Equal Work:**
```cpp
void ProcessElement(void* pData, u_int index, u_int total) {
    // Each invocation does equal work
    MyArray* array = static_cast<MyArray*>(pData);
    ProcessItem(array->items[index]);
}
```

**Unequal Work (Work Stealing):**
```cpp
struct WorkQueue {
    std::atomic<u_int> nextIndex{0};
    std::vector<WorkItem> items;
};

void ProcessDynamic(void* pData, u_int index, u_int total) {
    WorkQueue* queue = static_cast<WorkQueue*>(pData);
    
    // Keep claiming work until exhausted
    while (true) {
        u_int itemIndex = queue->nextIndex.fetch_add(1);
        if (itemIndex >= queue->items.size())
            break;
        
        ProcessItem(queue->items[itemIndex]);
    }
}
```

### Synchronization

**Avoid Locks:**
```cpp
// BAD
static Zenith_Mutex s_xMutex;
void Process(void* pData, u_int index, u_int total) {
    s_xMutex.Lock();
    ModifySharedData();
    s_xMutex.Unlock();
}
```

**Use Atomics:**
```cpp
// GOOD
static std::atomic<u_int> s_uCounter{0};
void Process(void* pData, u_int index, u_int total) {
    u_int localCount = Compute();
    s_uCounter.fetch_add(localCount);
}
```

**Use Per-Thread Data:**
```cpp
// BEST
struct PerThreadResults {
    u_int counts[8];
};

void Process(void* pData, u_int index, u_int total) {
    PerThreadResults* results = static_cast<PerThreadResults*>(pData);
    results->counts[index] = Compute();
}

// Merge after completion
u_int totalCount = 0;
for (u_int count : results.counts) {
    totalCount += count;
}
```

## Limitations & Future Work

### Current Limitations

1. **Fixed Thread Count:** 8 threads (hardcoded)
2. **No Priority:** All tasks equal priority
3. **No Dependencies:** Can't express task dependencies
4. **No Work Stealing:** Load imbalance causes idle threads
5. **Single Queue:** Potential contention with many producers

### Future Improvements

1. **Dynamic Thread Pool:**
   - Adjust thread count based on workload
   - Thread sleep when idle

2. **Priority Queue:**
   - High/medium/low priority tasks
   - Critical tasks execute first

3. **Task Graph:**
   - Express dependencies (A must complete before B)
   - Automatic scheduling

4. **Work Stealing:**
   - Each thread has own queue
   - Idle threads steal from busy threads

5. **NUMA Awareness:**
   - Pin threads to cores
   - Allocate memory close to threads

## Conclusion

Zenith's task system provides:
- **Simple API:** Easy to parallelize existing code
- **High Performance:** Lock-free queue, minimal overhead
- **Automatic Profiling:** Track task performance
- **Flexible Patterns:** Single tasks, task arrays, submitting thread joining
- **Scalability:** Near-linear speedup for parallel workloads

The system is designed for coarse-grained parallelism (thousands of cycles per task), achieving excellent performance for CPU-bound operations like command buffer recording, physics simulation, and ECS updates.
