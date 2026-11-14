CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread
	
all: rw

rw: rw.cpp ControlRW.hpp
	$(CXX) $(CXXFLAGS) rw.cpp -o rw

clean:
	rm -f rw
