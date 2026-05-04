# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -fPIC

# Targets
all: test_robot RobotWarz robots

# =========================
# Core engine
# =========================

RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp -o RobotBase.o

Arena.o: Arena.cpp Arena.h RobotBase.h
	$(CXX) $(CXXFLAGS) -c Arena.cpp -o Arena.o

# =========================
# Test program
# =========================

test_robot: test_robot.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

# =========================
# Main executable
# =========================

RobotWarz: main.cpp Arena.o RobotBase.o
	$(CXX) $(CXXFLAGS) main.cpp Arena.o RobotBase.o -ldl -o RobotWarz


# =========================
# Cleanup
# =========================

clean:
	rm -f *.o *.so test_robot RobotWarz
