# pingpongSweep
Does a ping pong test between all pairs of allocated nodes. In general, this should be run with one task/rank per host so that ping pongs aren't sent between processes on the same physical host.

>Note: these are _logical_ nodes from the Charm++ perpspective, which are really processes. When running in SMP mode, one has `+p` / `++ppn` number of nodes. In non-SMP mode, one has `+p` nodes (as each PE is an independent process)).

## Usage
`./pingpong <upper limit of message size in bytes> <number of iterations>`

For each pair of nodes, sweep through message payload sizes from 0 bytes to the specified upper limit (1,024 bytes by default), doubling each time, and for each size, do the specified number of round trip ping pong iterations, then report the average time each round trip took.

Or, in pseudocode:

    for i in 0..numNodes
      for j in i + 1..numNodes
        for size in 0..maxSize // 0, 1, 2, 4, ..., maxSize
          startTimer
          for iteration in 0..numIterations
            send message of specified size from node_i to node_j and then back to node_i from node_j
          endTimer
          report average roundtrip time, or (endTimer - startTimer) / numIterations
