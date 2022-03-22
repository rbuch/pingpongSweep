#include <charm++.h>
#include <chrono>
#include <string.h>  // for strlen, and strcmp

// To get hostname
#if defined(_WIN32) || defined(_WIN64)
#  include <Winsock2.h>
#else
#  include <limits.h>  // For HOST_NAME_MAX
#  include <unistd.h>
#endif

#ifndef HOST_NAME_MAX
// Apple seems to not adhere to POSIX and defines this non-standard variable
// instead of HOST_NAME_MAX
#  ifdef MAXHOSTNAMELEN
#    define HOST_NAME_MAX MAXHOSTNAMELEN
// Windows docs say 256 bytes (so 255 + 1 added at use) will always be sufficient
#  else
#    define HOST_NAME_MAX 255
#  endif
#endif

#define NITER 10000
#define NWARMUP 100
#define MAX_SIZE 1024

#include "pingpong.decl.h"
class PingMsg : public CMessage_PingMsg
{
public:
  char* x;
};

CProxy_main mainProxy;
CProxy_Coordinator coordProxy;
CProxy_PingPong pingpongProxy;

#define P1 0
#define P2 1 % CkNumPes()

class main : public CBase_main
{
public:
  main(CkMigrateMessage* m) {}
  main(CkArgMsg* m)
  {
    int iterations = NITER;
    int warmUpIterations = NWARMUP;
    size_t maxSize = MAX_SIZE;
    if (m->argc > 1) maxSize = atoi(m->argv[1]);
    if (m->argc > 2) iterations = atoi(m->argv[2]);
    if (m->argc > 3) warmUpIterations = atoi(m->argv[3]);
    if (m->argc > 4)
    {
      CkPrintf(
          "Usage: pingpong +pN [max_size] [iterations] [warm_up_iterations] \n Where "
          "max_size (default "
          "%d) is integer >0, iterations (default %d) is integer >0, warm_upiterations "
          "(default %d) is integer >0 ",
          MAX_SIZE, NITER, NWARMUP);
      CkExit(1);
    }
    if (CkNumNodes() < 2)
    {
      CkPrintf("Must run with at least two nodes!\n");
      CkExit(2);
    }
    // write timestamp
    using std::chrono::system_clock;
    const time_t now = system_clock::to_time_t(system_clock::now());
    struct tm currentTime;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&currentTime, &now);
#else
    gmtime_r(&now, &currentTime);
#endif
    CkPrintf("\nStarting run at:\n");
    char timeBuffer[100];
    strftime(timeBuffer, sizeof(timeBuffer), "%c %Z", std::localtime(&now));
    CkPrintf("%s\n", timeBuffer);
    strftime(timeBuffer, sizeof(timeBuffer), "%FT%TZ", &currentTime);
    CkPrintf("(ISO 8601: %s)\n\n", timeBuffer);

    CkPrintf("Pingpong with maxSize: %zu iterations: %d (+ %d warm up)\n", maxSize,
             iterations, warmUpIterations);
    mainProxy = thishandle;
    coordProxy = CProxy_Coordinator::ckNew(iterations, warmUpIterations, maxSize);
    pingpongProxy = CProxy_PingPong::ckNew();
    CkStartQD(CkCallback(CkIndex_main::maindone(), mainProxy));
    delete m;
  };

  void maindone(void) { pingpongProxy.getNodeInfo(); };
};

class Coordinator : public CBase_Coordinator
{
  Coordinator_SDAG_CODE;
  size_t size = 0;
  int source = 0, destination = 1;
  int numNodes;
  int iterations;
  int warmUpIterations;
  size_t maxSize;
  std::vector<std::string> nodeHostnames;
  int numCheckedIn = 0;

public:
  Coordinator(int iterations, int warmUpIterations, size_t maxSize)
      : iterations(iterations),
        warmUpIterations(warmUpIterations),
        maxSize(maxSize),
        numNodes(CkNumNodes())
  {
    nodeHostnames.resize(numNodes);
  }

  Coordinator(CkMigrateMessage* m) {}

  void reportNodeInfo(int index, std::string hostname)
  {
    nodeHostnames[index] = hostname;
    numCheckedIn++;
    if (numCheckedIn == numNodes)
    {
      CkPrintf("List of node hostnames:\n");
      for (int i = 0; i < numNodes; i++)
      {
        CkPrintf("%d: %s\n", i, nodeHostnames[i].c_str());
      }
      CkPrintf("\n");
      thisProxy.start();
    }
  }

  void start()
  {
    pingpongProxy[source].configSender(destination, iterations, warmUpIterations, size);
    pingpongProxy[destination].configReceiver(source);
    thisProxy.triggerWhenReady();
  }

  void done(double elapsed)
  {
    CkPrintf("Avg roundtrip time is %lf us (size %zu (%d, %d))\n", elapsed, size, source,
             destination);

    if (size < maxSize)
    {
      size = (size == 0) ? 1 : size * 2;
      thisProxy.start();
    }
    else
    {
      size = 0;
      CkPrintf("\n");
      if (destination < numNodes - 1)
      {
        destination++;
        thisProxy.start();
      }
      else if (source < numNodes - 2)
      {
        source++;
        destination = source + 1;
        thisProxy.start();
      }
      else
      {
        CkExit();
      }
    }
  }
};

class PingPong : public CBase_PingPong
{
private:
  bool isSender = false;
  int other = 0;
  int iterations = 0;
  int warmUpIterations = 0;
  int counter = 0;
  size_t size = 0;

  double start_time, end_time;

public:
  PingPong() {}

  void getNodeInfo()
  {
    // Add 1 for null terminator
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX + 1);
    coordProxy.reportNodeInfo(thisIndex, std::string(hostname));
  }

  void configSender(int destination, int iterations, int warmUpIterations, size_t size)
  {
    isSender = true;
    other = destination;
    this->iterations = iterations + warmUpIterations;
    this->warmUpIterations = warmUpIterations;
    this->counter = 0;
    this->size = size;
    coordProxy.ready();
  }

  void configReceiver(int source)
  {
    isSender = false;
    other = source;
    coordProxy.ready();
  }

  void start() { thisProxy[other].recv(new (size) PingMsg); }

  void recv(PingMsg* msg)
  {
    if (isSender)
    {
      counter++;
      if (counter == warmUpIterations)
      {
        start_time = CkWallTimer();
        thisProxy[other].recv(msg);
      }
      else if (counter == iterations)
      {
        end_time = CkWallTimer();
        const double elapsed = 1.0e6 * (end_time - start_time) / iterations;
        coordProxy.done(elapsed);
      }
      else
      {
        thisProxy[other].recv(msg);
      }
    }
    else
    {
      thisProxy[other].recv(msg);
    }
  }
};

#include "pingpong.def.h"
