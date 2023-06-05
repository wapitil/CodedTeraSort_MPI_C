#include <iostream>
#include <assert.h>
#include <mpi.h>
#include <iomanip>

#include "CodedMaster.h"
#include "Common.h"
#include "CodedConfiguration.h"
#include "PartitionSampling.h"

using namespace std;

void CodedMaster::run()
{
  if ( totalNode != 1 + conf.getNumReducer() ) {
    cout << "The number of workers mismatches the number of processes.\n";
    assert( false );
  }

  // GENERATE LIST OF PARTITIONS.
  PartitionSampling partitioner;
  partitioner.setConfiguration( &conf );
  PartitionList* partitionList = partitioner.createPartitions();


  // BROADCAST CONFIGURATION TO WORKERS
  MPI_Bcast(&conf, sizeof(Configuration), MPI_CHAR, 0, MPI_COMM_WORLD);
  // Note: this works because the number of partitions can be derived from the number of workers in the configuration.


  // BROADCAST PARTITIONS TO WORKERS
  for (auto it = partitionList->begin(); it != partitionList->end(); it++) {
    unsigned char* partition = *it;
    MPI_Bcast(partition, conf.getKeySize() + 1, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
  }


  // TIME BUFFER
  int numWorker = conf.getNumReducer();
  double rcvTime[ numWorker + 1 ];  
  double rTime = 0;
  double avgTime;
  double maxTime;


  // COMPUTE CODE GENERATION TIME  
 MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  avgTime = 0;
  maxTime = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    avgTime += rcvTime[ i ];
    maxTime = max( maxTime, rcvTime[ i ] );
  }
  cout << rank
       << ": CODEGEN | Avg = " << setw(10) << avgTime/numWorker
       << "   Max = " << setw(10) << maxTime << endl;
      

  // COMPUTE MAP TIME
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  avgTime = 0;
  maxTime = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    avgTime += rcvTime[ i ];
    maxTime = max( maxTime, rcvTime[ i ] );
  }
  cout << rank
       << ": MAP     | Avg = " << setw(10) << avgTime/numWorker
       << "   Max = " << setw(10) << maxTime << endl;  

  
  // COMPUTE ENCODE TIME
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  avgTime = 0;
  maxTime = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    avgTime += rcvTime[ i ];
    maxTime = max( maxTime, rcvTime[ i ] );
  }
  cout << rank
       << ": ENCODE  | Avg = " << setw(10) << avgTime/numWorker
       << "   Max = " << setw(10) << maxTime << endl;  

  // COMPUTE SHUFFLE TIME
  avgTime = 0;
  double txRate = 0;
  double avgRate = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    MPI_Recv(&rTime, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    avgTime += rTime;    
    MPI_Recv(&txRate, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    avgRate += txRate;
  }
  cout << rank
       << ": SHUFFLE | Sum = " << setw(10) << avgTime
       << "   Rate = " << setw(10) << avgRate/numWorker << " Mbps" << endl;


  // COMPUTE DECODE TIME
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); 
  avgTime = 0;
  maxTime = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    avgTime += rcvTime[ i ];
    maxTime = max( maxTime, rcvTime[ i ] );
  }
  cout << rank
       << ": DECODE  | Avg = " << setw(10) << avgTime/numWorker
       << "   Max = " << setw(10) << maxTime << endl;  

  // COMPUTE REDUCE TIME
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  avgTime = 0;
  maxTime = 0;
  for( int i = 1; i <= numWorker; i++ ) {
    avgTime += rcvTime[ i ];
    maxTime = max( maxTime, rcvTime[ i ] );
  }
  cout << rank
       << ": REDUCE  | Avg = " << setw(10) << avgTime/numWorker
       << "   Max = " << setw(10) << maxTime << endl;      
  

  // CLEAN UP MEMORY
  for ( auto it = partitionList->begin(); it != partitionList->end(); it++ ) {
    delete [] *it;
  }
}
