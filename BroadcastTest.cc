#include <iostream>
#include <fstream>
#include <mpi.h>
#include <ctime>
#include <assert.h>
#include <vector>

#define LINE_SIZE 100
#define INPUT_PATH "./Input/Input10000"

using namespace std;

typedef vector< unsigned char* > LineList;

void testBroadcastRoot(int, LineList&, MPI_Comm&);
void testBroadcast(int, MPI_Comm&);
void testUnicastRoot(int, LineList&, MPI_Comm&);
void testUnicast(int, MPI_Comm&);
void testScatterRoot(int, LineList&, MPI_Comm&);
void testScatter(int, MPI_Comm&);

int main( int argc, char* argv[] )
{
  clock_t time;
  LineList lineList;
  
  MPI_Init(NULL, NULL);
  MPI_Comm bcComm;
  MPI_Comm_dup(MPI_COMM_WORLD, &bcComm);
  int rank;
  MPI_Comm_rank(bcComm, &rank);
  int numNode;
  MPI_Comm_size(bcComm, &numNode);

  LineList mlineList;
  
  // Prepare inputlist
  // For node 0 (root of broadcast) and node 1 (root of multicast)
  if( rank < 2 ) {
    ifstream inputFile( INPUT_PATH, ios::in | ios::binary | ios::ate );
    if ( !inputFile.is_open() ) {
      cout << "Cannot open input file.\n";
      assert( false );
    }
  
    unsigned long long int fileSize = inputFile.tellg();
    unsigned long long int numLine = fileSize / LINE_SIZE;
    inputFile.seekg( 0 );

    for( unsigned long long i = 0; i < numLine; i++ ) {
      unsigned char* buff = new unsigned char[ LINE_SIZE ];
      inputFile.read( ( char* ) buff, LINE_SIZE );
      lineList.push_back( buff );

      if( rank == 0 ) {
	unsigned char* mbuff = new unsigned char[ LINE_SIZE * numNode ];
	for( int i = 0; i < numNode; i++ ) {
	  memcpy( mbuff + i*LINE_SIZE, buff, LINE_SIZE );
	}
	mlineList.push_back( mbuff );
      }
      else {
	// Skip multicast implementation for now
      }
    }
    inputFile.close();
  }


// Broadcast measurement
  if( rank == 0 ) {
    MPI_Barrier(bcComm);
    time = clock();
    testBroadcastRoot(numNode, lineList, bcComm);
    MPI_Barrier(bcComm);
    time = clock() - time;
    cout << rank << ": Broadcast time is " << double(time) / CLOCKS_PER_SEC << " seconds.\n";
  }
  else {
    MPI_Barrier(bcComm);
    testBroadcast(rank, bcComm);
    MPI_Barrier(bcComm);
  }

  
 // Unicast measurement
  if( rank == 0 ) {
    MPI_Barrier(bcComm);
    time = clock();
    testUnicastRoot(numNode, lineList, bcComm);
    MPI_Barrier(bcComm);
    time = clock() - time;
    cout << rank << ": Unicast time is " << double(time) / CLOCKS_PER_SEC << " seconds.\n";
  }
  else {
    MPI_Barrier(bcComm);
    testUnicast(rank, bcComm);
    MPI_Barrier(bcComm);
  }

  // Scatter measurement
  if( rank == 0 ) {
    MPI_Barrier(bcComm);
    time = clock();
    testScatterRoot(numNode, mlineList, bcComm);
    MPI_Barrier(bcComm);
    time = clock() - time;
    cout << rank << ": Scatter time is " << double(time) / CLOCKS_PER_SEC << " seconds.\n";
  }
  else {
    MPI_Barrier(bcComm);
    testScatter(rank, bcComm);
    MPI_Barrier(bcComm);
  }


  // // Multicast
  // MPI::Intracomm mcComm = bcComm.Split( ( rank > 0 ) , rank );
  // int newRank = mcComm.Get_rank();
  // int newNumNode = mcComm.Get_size();
  // if( rank > 0 ) {
  //   if( newRank == 0 ) {
  //     //cout << rank << ": wait for barrier\n";      
  //     mcComm.Barrier();
  //     //cout << rank << ": start sending\n";      
  //     time = clock();
  //     testBroadcastRoot( newNumNode, lineList, mcComm );
  //     mcComm.Barrier();  
  //     time = clock() - time;
  //     cout << rank << ": Multicast time is " << double( time ) / CLOCKS_PER_SEC << " seconds.\n";
  //   }
  //   else {
  //     //cout << rank << ": wait for barrier\n";
  //     mcComm.Barrier();
  //     testBroadcast( newRank, mcComm );
  //     //cout << rank << ": received data\n";      
  //     mcComm.Barrier();
  //   }
  // }
    
  // Clean up
  if( rank == 0 ) {
    for( auto lit = lineList.begin(); lit != lineList.end(); lit++ ) {
      delete [] *lit;
    }
  }

  MPI_Finalize();

  return 0;
}

void testBroadcastRoot(int numNode, LineList& lineList, MPI_Comm comm)
{
  unsigned long long numLine = lineList.size();
  MPI_Bcast(&numLine, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
  for (auto it = lineList.begin(); it != lineList.end(); it++) {
  MPI_Bcast(*it, LINE_SIZE, MPI_UNSIGNED_CHAR, 0, comm);
  }
}



void testBroadcast(int rank, MPI_Comm comm)
{
  unsigned long long numLine;
  MPI_Bcast(&numLine, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
  for (unsigned long long i = 0; i < numLine; i++) {
  unsigned char t[LINE_SIZE];
  MPI_Bcast(t, LINE_SIZE, MPI_UNSIGNED_CHAR, 0, comm);
  }
}

void testUnicastRoot( int numNode, LineList& lineList, MPI_Comm comm )
{
  unsigned long long numLine = lineList.size();
  for( int nid = 1; nid < numNode; nid++ ) {
  MPI_Send( &numLine, 1, MPI_UNSIGNED_LONG_LONG, nid, 0, comm );
  for( auto it = lineList.begin(); it != lineList.end(); it++ ) {
  MPI_Send( ( char* ) *it, LINE_SIZE, MPI_UNSIGNED_CHAR, nid, 0, comm );
  }
  }
}

void testUnicast(int rank, MPI_Comm comm)
{
  unsigned long long numLine;
  MPI_Recv(&numLine, 1, MPI_UNSIGNED_LONG_LONG, 0, 0, comm, MPI_STATUS_IGNORE);
  for (unsigned long long i = 0; i < numLine; i++) {
  unsigned char t[LINE_SIZE];
  MPI_Recv(t, LINE_SIZE, MPI_UNSIGNED_CHAR, 0, 0, comm, MPI_STATUS_IGNORE);
  }
}

void testScatterRoot(int numNode, LineList& mlineList, MPI_Comm comm)
{
  unsigned long long numLine[numNode];
  for (int i = 0; i < numNode; i++) {
  numLine[i] = mlineList.size();
  }
  unsigned long long nl;

  MPI_Scatter(numLine, 1, MPI_UNSIGNED_LONG_LONG, &nl, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
  for (auto it = mlineList.begin(); it != mlineList.end(); it++) {
  unsigned char t[LINE_SIZE];
  MPI_Scatter(*it, LINE_SIZE, MPI_UNSIGNED_CHAR, t, LINE_SIZE, MPI_UNSIGNED_CHAR, 0, comm);
  }
}


void testScatter(int rank, MPI_Comm comm)
{
unsigned long long numLine;
MPI_Scatter(NULL, 0, MPI_UNSIGNED_LONG_LONG, &numLine, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
for (unsigned long long i = 0; i < numLine; i++) {
unsigned char t[LINE_SIZE];
MPI_Scatter(NULL, 0, MPI_UNSIGNED_CHAR, t, LINE_SIZE, MPI_UNSIGNED_CHAR, 0, comm);
}
}








