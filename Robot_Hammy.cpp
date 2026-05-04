#include "RobotBase.h"
#include <cmath>
#include <limits>
#include <vector>

class Robot_HammerZigZag : public RobotBase {
private:
  int target_row = -1;
  int target_col = -1;
  int scan_dir = 1;

  bool zig_toggle = false; // alternates movement pattern

  void clear_target() {
    target_row = -1;
    target_col = -1;
  }

public:
  // hammer weapon instead of railgun
  Robot_HammerZigZag() : RobotBase(3, 5, hammer) {}

  // Scan in all directions (simple sweep)
  void get_radar_direction(int &radar_direction) override {
    radar_direction = scan_dir;
    scan_dir = (scan_dir % 8) + 1;
  }

  void
  process_radar_results(const std::vector<RadarObj> &radar_results) override {
    bool found = false;

    for (const auto &obj : radar_results) {
      if (obj.m_type == 'R') {
        found = true;
      }
    }

    if (!found) {
      clear_target();
    }

    int my_r, my_c;
    get_current_location(my_r, my_c);

    int best_dist = std::numeric_limits<int>::max();

    for (const auto &obj : radar_results) {
      if (obj.m_type == 'R') {
        int dist = abs(obj.m_row - my_r) + abs(obj.m_col - my_c);

        if (dist < best_dist) {
          best_dist = dist;
          target_row = obj.m_row;
          target_col = obj.m_col;
        }
      }
    }
  }

  // Hammer = only attack if adjacent
  bool get_shot_location(int &shot_row, int &shot_col) override {
    if (target_row == -1)
      return false;

    int my_r, my_c;
    get_current_location(my_r, my_c);

    int dr = abs(target_row - my_r);
    int dc = abs(target_col - my_c);

    // Only attack if adjacent
    if (dr <= 1 && dc <= 1) {
      shot_row = target_row;
      shot_col = target_col;
      return true;
    }

    return false;
  }

  void get_move_direction(int &move_direction, int &move_distance) override {
    int my_r, my_c;
    get_current_location(my_r, my_c);

    int speed = get_move_speed();

    if (target_row == -1) {
      // no target → wander
      move_direction = rand() % 8 + 1;
      move_distance = 1;
      return;
    }

    int dr = target_row - my_r;
    int dc = target_col - my_c;

    // Zig-zag logic: alternate priority
    zig_toggle = !zig_toggle;

    int step_r = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
    int step_c = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);

    if (zig_toggle) {
      if (dc != 0)
        move_direction = (dc > 0) ? 3 : 7;
      else if (dr != 0)
        move_direction = (dr > 0) ? 5 : 1;
    } else {
      if (dr != 0)
        move_direction = (dr > 0) ? 5 : 1;
      else if (dc != 0)
        move_direction = (dc > 0) ? 3 : 7;
    }
    // Factory
    extern "C" RobotBase *create_robot() { return new Robot_HammerZigZag(); }

    extern "C" const char *robot_summary() {
      return "Zig-zag hammer bot that hunts nearest enemy.";
    }
  }
};
