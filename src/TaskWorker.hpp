#pragma once
#include <functional>
#if !defined(METAMODULE)
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

namespace Venom {

#if defined(METAMODULE)

// MetaModule is single threaded, so instead of handing the task to a worker
// thread we queue it and let the module widget run it from step(), which keeps
// it off the audio thread just the same. poll() must be called from step().
struct TaskWorker {
  std::function<void()> workerTask;
  volatile bool workerDoProcess = false;

  void work(std::function<void()> task) {
    workerTask = task;
    workerDoProcess = true;
  }

  void poll() {
    if (workerDoProcess) {
      workerDoProcess = false;
      workerTask();
    }
  }
};

#else

// This code comes directly from Stoermelder PackOne helpers/TaskWorker.hpp

struct TaskWorker {
  std::mutex workerMutex;
  std::condition_variable workerCondVar;
  std::thread* worker;
  Context* workerContext;
  bool workerIsRunning = true;
  bool workerDoProcess = false;
  int workerPreset = -1;
  std::function<void()> workerTask;

  TaskWorker() {
    workerContext = contextGet();
    worker = new std::thread(&TaskWorker::processWorker, this);
  }

  ~TaskWorker() {
    workerIsRunning = false;
    workerDoProcess = true;
    workerCondVar.notify_one();
    worker->join();
    workerContext = NULL;
    delete worker;
  }

  void processWorker() {
    contextSet(workerContext);
    while (true) {
      std::unique_lock<std::mutex> lock(workerMutex);
      workerCondVar.wait(lock, std::bind(&TaskWorker::workerDoProcess, this));
      if (!workerIsRunning) return;
      workerTask();
      workerDoProcess = false;
    }
  }

  void work(std::function<void()> task) {
    workerTask = task;
    workerDoProcess = true;
    workerCondVar.notify_one();
  }
};

#endif

}