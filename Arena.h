#pragma once

#include "RadarObj.h"
#include "RobotBase.h"
#include <string>
#include <vector>

class Arena {
public:
  Arena(int r, int c);

  // robot management
  void addRobot(RobotBase *robot, const std::string &name);

  // main loop
  void run();

private:
  struct RobotState {
    RobotBase *robot;
    int row;
    int col;
    int health;
    int armor;
    int move_speed;
    WeaponType weapon;
    bool alive;
    std::string name;
  };

  int rows;
  int cols;

  std::vector<RobotState> robots;

  // 2D board representation (you are using this in cpp)
  std::vector<std::vector<char>> board;

  // core simulation steps
  void processTurn(RobotState &r);
  void handleMove(RobotState &r, int dir, int dist);
  void handleShoot(RobotState &shooter, int target_row, int target_col);
  std::vector<RadarObj> performRadar(RobotState &r, int direction);

  // helpers
  bool inBounds(int r, int c) const;
  RobotState *getRobotAt(int r, int c);
  void printState();
};
