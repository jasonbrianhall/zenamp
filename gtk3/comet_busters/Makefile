CC = gcc
CXX = g++
CFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
CXXFLAGS = -Wall -O2 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread -lm

# Targets
all: comet_main

comet_main: comet_main.o cometbuster.o
	$(CXX) -o cometbuster comet_main.o cometbuster.o $(LDFLAGS)

comet_main.o: comet_main.cpp cometbuster.h
	$(CXX) $(CXXFLAGS) -c comet_main.cpp

cometbuster.o: cometbuster.cpp cometbuster.h visualization.h
	$(CXX) $(CXXFLAGS) -c cometbuster.cpp

clean:
	rm -f *.o cometbuster

.PHONY: all clean
