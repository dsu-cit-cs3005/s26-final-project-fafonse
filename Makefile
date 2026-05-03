# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic

# Targets
all: test_robot Arena.so

RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

test_robot: test_robot.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

Arena.so: Arena.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) -c Arena.cpp RobotBase.o

RobotWarz: main.cpp Arena.so RobotBase.o
	$(CXX) $(CXXFLAGS) Arena.so RobotBase.o main.cpp -ldl -o RobotWarz

clean:
	rm -f *.o test_robot *.so
