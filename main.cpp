// Kenny Le
// PA3
// DUE: 4/29/2024

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <semaphore.h>
#include "pthread.h"

using namespace std;

// Declare utility functions
int calcHyperPeriod(const vector<int>& periods);
double calcUtilization(const vector<int>& WCET, const vector<int>& period);
bool schedule(double utilization, int n);

// Struct for CPU
struct CPUInfo {
  vector<string> tasks;
  vector<int> WCET;
  vector<int> period;

  int hyperPeriod;
  double utilization;
  bool schedulable;

  pthread_mutex_t *mutex;
  pthread_cond_t *condition;

  int *counter;
  int CPUnumber;
};

// Find greatest common divisor
int getGCD(int a, int b) {
  while (b != 0) {
    int temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

// Find least common multiple
int getLCM(int a, int b) {
  return (a * b) / getGCD(a, b);
}

// Calculate hyperperiod
int calcHyperPeriod(const vector<int>& periods) {
  return accumulate(periods.begin(), periods.end(), 1, getLCM);
}

// Calculate total CPU utilization
double calcUtilization(const vector<int>& WCET, const vector<int>& period) {
  double utilization = 0;
  for (size_t i = 0; i < WCET.size(); ++i) {
    utilization += static_cast<double>(WCET[i]) / period[i];
  }
  return utilization;
}

// Check if schedulable
bool schedule(double utilization, int n) {
  double bound = n * (pow(2.0, 1.0 / n) - 1);
  return utilization <= bound;
}

// Print task schedule
void printTaskSchedule(const vector<string>& tasks, const vector<int>& WCET, const vector<int>& period) {
  cout << "Task scheduling information: ";
  for (size_t i = 0; i < tasks.size(); ++i) {
    cout << tasks[i] << " (WCET: " << WCET[i] << ", Period: " << period[i] << ")";
    if (i < tasks.size() - 1) {
      cout << ", ";
    }
  }
  cout << endl;
}

// Print scheduling diagram rate monotonic scheduling for every CPU
void printRMS(const vector<string>& tasks, const vector<int>& WCET, const vector<int>& period, int hyperperiod, int CPUnumber) {
  
  // Vectors for task indices and execution times
  vector<int> indices(tasks.size());
  vector<int> taskNextBegin(tasks.size(), 0); 
  vector<int> taskExecutionLeft = WCET; 

  // Update indices and reorder them accordingly
  iota(indices.begin(), indices.end(), 0); 
  sort(indices.begin(), indices.end(), [&](int a, int b) { return period[a] < period[b]; }); 

  cout << "Scheduling Diagram for CPU " << CPUnumber << ": ";
  int currentTime = 0;

  // Reach hyperperiod
  while (currentTime < hyperperiod) {
    bool foundTask = false;
    
    // Find task for current time
    for (size_t idx = 0; idx < indices.size() && !foundTask; ++idx) {
      size_t i = indices[idx];

      // Check if task can begin or continue
      if (currentTime >= taskNextBegin[i] && taskExecutionLeft[i] > 0) {
        
        // Look for pre-emption
        int executionTime = min(taskExecutionLeft[i], period[i] - (currentTime % period[i]));
        for (size_t j = 0; j < indices.size(); ++j) {
          if (taskNextBegin[indices[j]] > currentTime && taskNextBegin[indices[j]] < currentTime + executionTime) {
            executionTime = taskNextBegin[indices[j]] - currentTime;
            break;
          }
        }
        
        // Print task execution
        cout << tasks[i] << "(" << executionTime << "), ";
        currentTime += executionTime; 
        taskExecutionLeft[i] -= executionTime; 
        
        // If finished -> schedule next begin and reset execution time
        if (taskExecutionLeft[i] == 0) { 
          taskNextBegin[i] += period[i]; 
          taskExecutionLeft[i] = WCET[i];
        }
        foundTask = true;
      }
    }
    
    // If not finished, calculate next idle time
    if (!foundTask) {
      int nextTaskTime = *min_element(taskNextBegin.begin(), taskNextBegin.end(), [currentTime, hyperperiod](int a, int b) {
        if (a > currentTime) {
          return a - currentTime;
        } 
        else {
          return hyperperiod - currentTime;
        }
      });
      
      // If no tasks are scheduled until hyperperiod:
      if (nextTaskTime <= currentTime || nextTaskTime == hyperperiod) 
        nextTaskTime = hyperperiod;
      for (size_t i = 0; i < tasks.size(); ++i) {
        if (taskNextBegin[i] > currentTime && taskNextBegin[i] < nextTaskTime) {
          nextTaskTime = taskNextBegin[i];
        }
      }
      
      // If no tasks are ready -> calculate and print idle time
      int idleTime = nextTaskTime - currentTime;
      if (idleTime > 0 && idleTime != hyperperiod) {
        cout << "Idle(" << idleTime << ")";
        currentTime += idleTime; 
        if (currentTime < hyperperiod) {
          cout << ", ";
        }
      } 
      else {
        break; 
      }
    }
  }
  cout << endl;
}

// POSIX to process CPU scheduling
void* scheduleCPU(void *arg) {
  pair<string, CPUInfo*>* data = static_cast<pair<string, CPUInfo*>*>(arg);
  string taskInfo = data->first;
  CPUInfo& cpuData = *(data->second);

  pthread_mutex_lock(cpuData.mutex);
  
  istringstream stream(taskInfo);
  string task;
  int WCET, period;
  
  while (stream >> task >> WCET >> period) {
    cpuData.tasks.push_back(task);
    cpuData.WCET.push_back(WCET);
    cpuData.period.push_back(period);
  }

  while (*(cpuData.counter) != cpuData.CPUnumber) {
    pthread_cond_wait(cpuData.condition, cpuData.mutex);
  }
  
  pthread_mutex_unlock(cpuData.mutex);

  // Scheduling information
  cpuData.utilization = calcUtilization(cpuData.WCET, cpuData.period);
  cpuData.hyperPeriod = calcHyperPeriod(cpuData.period);
  cpuData.schedulable = schedule(cpuData.utilization, cpuData.tasks.size());

  // Print
  cout << "CPU " << (cpuData.CPUnumber + 1) << endl;
  printTaskSchedule(cpuData.tasks, cpuData.WCET, cpuData.period);
  
  cout << fixed << setprecision(2) << "Task set utilization: " << cpuData.utilization << endl;
  cout << "Hyperperiod: " << cpuData.hyperPeriod << endl;
  cout << "Rate Monotonic Algorithm execution for CPU" << (cpuData.CPUnumber + 1) << ": " << endl;
  
  if (cpuData.schedulable) {
    printRMS(cpuData.tasks, cpuData.WCET, cpuData.period, cpuData.hyperPeriod, cpuData.CPUnumber + 1);
  } 
  else if (cpuData.utilization > 1){
    cout << "The task set is not schedulable" << endl;
  } 
  else {
    cout << "Task set schedulability is unknown" << endl;
  }
  cout << endl;

  // Signal next and increase counter++
  pthread_mutex_lock(cpuData.mutex);
  (*cpuData.counter)++;
  pthread_cond_broadcast(cpuData.condition);
  pthread_mutex_unlock(cpuData.mutex);

  return nullptr;
}

int main() {
  vector<string> cpuInputs;
  string line;
  
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
  
  int counter = 0;

  while (getline(cin, line) && !line.empty()) {
    cpuInputs.push_back(line);
  }

  vector<CPUInfo> cpuInfos(cpuInputs.size());
  vector<pthread_t> threads(cpuInputs.size());
  vector<pair<string, CPUInfo*>> threadArgs(cpuInputs.size());

  // Setup CPUInfo with primitives
  for (size_t i = 0; i < cpuInfos.size(); ++i) {
    cpuInfos[i].mutex = &mutex;
    cpuInfos[i].condition = &condition;
    cpuInfos[i].counter = &counter;
    cpuInfos[i].CPUnumber = i;
    threadArgs[i] = make_pair(cpuInputs[i], &cpuInfos[i]);
    pthread_create(&threads[i], NULL, scheduleCPU, &threadArgs[i]);
  }

  // Join threads
  for (size_t i = 0; i < threads.size(); ++i) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}