#include <charm++.h>
#include <string.h>  // for strlen, and strcmp

#define NITER 10000
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
    size_t maxSize = MAX_SIZE;
    if (m->argc > 1) maxSize = atoi(m->argv[1]);
    if (m->argc > 2) iterations = atoi(m->argv[2]);
    if (m->argc > 3)
    {
      CkPrintf(
          "Usage: pingpong +pN [max_size] [iterations]\n Where max_size (default "
          "%d) is integer >0 iterations (default %d) is integer >0 ",
          MAX_SIZE, NITER);
      CkExit(1);
    }
    if (CkNumNodes() < 2)
    {
      CkPrintf("Must run with at least two nodes!\n");
      CkExit(2);
    }
    CkPrintf("Pingpong with maxSize: %zu iterations: %d\n", maxSize, iterations);
    mainProxy = thishandle;
    coordProxy = CProxy_Coordinator::ckNew(iterations, maxSize);
    pingpongProxy = CProxy_PingPong::ckNew();
    CkStartQD(CkCallback(CkIndex_main::maindone(), mainProxy));
    delete m;
  };

  void maindone(void)
  {
    coordProxy.start();
  };
};

class Coordinator : public CBase_Coordinator
{
  Coordinator_SDAG_CODE
  size_t size = 0;
  int source = 0, destination = 1;
  int numNodes;
  int iterations;
  size_t maxSize;
public:
  Coordinator(int iterations, size_t maxSize)
    : iterations(iterations), maxSize(maxSize), numNodes(CkNumNodes()) {}

  Coordinator(CkMigrateMessage* m) {}

  void start()
  {
    pingpongProxy[source].configSender(destination, iterations, size);
    pingpongProxy[destination].configReceiver(source);
    thisProxy.triggerWhenReady();
  }

  void done(double elapsed)
  {
    CkPrintf("Avg roundtrip time is %lf us (size %zu (%d, %d))\n", elapsed, size,
	     source, destination);

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
      else { CkExit(); }
    }
  }
};

class PingPong : public CBase_PingPong
{
private:
  bool isSender = false;
  int other = 0;
  int iterations = 0;
  int counter = 0;
  size_t size = 0;

  double start_time, end_time;

public:
  PingPong() {}

  void configSender(int destination, int iterations, size_t size)
  {
    isSender = true;
    other = destination;
    this->iterations = iterations;
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

  void start()
  {
    start_time = CkWallTimer();
    thisProxy[other].recv(new (size) PingMsg);
  }

  void recv(PingMsg* msg)
  {
    if (isSender)
    {
      counter++;
      if (counter == iterations)
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
