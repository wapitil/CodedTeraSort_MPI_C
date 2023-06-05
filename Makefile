CC = mpic++
DFLAGS = -std=c++11 -Wall 
DFLAGS = -std=c++11 -Wall -g -O0

all: TeraSort Splitter 

clean:
	rm -f *.o
	rm -f TeraSort CodedTeraSort Splitter Codegen BroadcastTest InputPlacement InputPlacemantRandom

cleanclean: clean
	rm -f ./Input/*_*
	rm -f ./Output/*_*
	rm -f ./Tmp/*
	rm -f *.*~
	rm -f *~


TeraSort: main.o Master.o Worker.o Trie.o Utility.o PartitionSampling.o 
	$(CC) $(DFLAGS) -o TeraSort main.o Master.o Worker.o Trie.o Utility.o PartitionSampling.o

Splitter: InputSplitter.o Configuration.h 
	$(CC) $(DFLAGS) -o Spltter Splitter.cc InputSplitter.o

Trie.o: Trie.cc Trie.h
	$(CC) $(DFLAGS) -c Trie.cc

PartitionSampling.o: PartitionSampling.cc PartitionSampling.h Configuration.h
	$(CC) $(DFLAGS) -c PartitionSampling.cc

Utility.o: Utility.cc Utility.h
	$(CC) $(DFLAGS) -c Utility.cc

InputSplitter.o: InputSplitter.cc InputSplitter.h Configuration.h 
	$(CC) $(DFLAGS) -c InputSplitter.cc

main.o: main.cc Configuration.h
	$(CC) $(DFLAGS) -c main.cc

Master.o: Master.cc Master.h Configuration.h
	$(CC) $(DFLAGS) -c Master.cc

Worker.o: Worker.cc Worker.h Configuration.h
	$(CC) $(DFLAGS) -c Worker.cc

