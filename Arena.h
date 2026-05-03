#ifndef ARENA_H
#define ARENA_H

#include <vector>
#include <string>
#include "RobotBase.h"
#include "RadarObj.h"

struct RobotState {
    RobotBase* robot;

    int row;
    int col;

    int health;
    bool alive;

    std::string name;
};

class Arena {
private:
    int rows;
    int cols;

    std::vector<RobotState> robots;

public:
    Arena(int r, int c);

    void addRobot(RobotBase* robot, const std::string& name);
    void run();

private:
    void processTurn(RobotState& r);

    // Phases
    std::vector<RadarObj> performRadar(RobotState& r, int direction);
    void handleShoot(RobotState& shooter, int target_row, int target_col);
    void handleMove(RobotState& r, int direction, int distance);

    // Helpers
    bool inBounds(int row, int col);
    RobotState* getRobotAt(int row, int col);

    bool checkWinCondition();
    void printState();
};

#endif
