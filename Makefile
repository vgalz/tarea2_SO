CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread

.PHONY: all test clean

all: rw

rw: rw.cpp ControlRW.hpp
	$(CXX) $(CXXFLAGS) rw.cpp -o rw

test: rw
	bash tests/test_r1.sh

clean:
	rm -f rw
