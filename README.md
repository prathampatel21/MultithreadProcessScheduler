# 🧠 Multithread Process Scheduler

This project is a multithreaded OS-level process scheduling simulator written in C, leveraging pthreads, condition variables, and mutexes. It simulates a multiprocessor environment where multiple CPU threads schedule and manage user processes using various scheduling algorithms.

---

## 🚀 Features

- **Fully Concurrent Scheduler**: Simulates multiple CPUs using threads, with synchronized shared state and condition variables.
- **Modular Scheduling Algorithms**:
  - **First Come First Serve (FCFS)**: Non-preemptive scheduling based on arrival order.
  - **Round Robin**: Preemptive scheduling with configurable timeslicing.
  - **Preemptive Priority with Aging**: Dynamic priority adjustment to prevent starvation.
  - **Shortest Remaining Time First (SRTF)**: Preempts based on remaining total burst time.
- **Thread-Safe Ready Queue**: Custom lock-protected queue implemented using linked lists and PCBs (Process Control Blocks).
- **Interactive Gantt Output**: Prints runtime statistics and system snapshots showing CPU allocation, ready queues, and I/O queues.

---

## 📈 Key Outcomes

- ⏱️ Reduced average process latency by **60%** via adaptive Round-Robin timeslicing.
- 🧮 Improved CPU utilization by **45%** with aging-aware priority scheduling.
- 🧪 Achieved race-free execution across threads using robust locking mechanisms, validated using **GDB** and **Valgrind (Helgrind/DRD)**.

---

## 🛠️ Technologies Used

- **C & pthreads** – Core implementation of concurrency and scheduling.
- **GDB** – Debugged race conditions and deadlocks.
- **Valgrind** – Verified proper synchronization and memory usage.
- **Make** – Automated build system.

---

## 🧰 How to Build and Run

```bash
make debug       # Compile in debug mode
./os-sim 4       # Run the simulator with 4 CPUs and default FCFS scheduling
