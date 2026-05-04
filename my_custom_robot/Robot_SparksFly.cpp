#include "RobotBase.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

class Robot_SparksFly : public RobotBase {
private:
  int target_row = -1;
  int target_col = -1;
  bool has_target = false;

  std::set<std::pair<int, int>> known_obstacles;

  int scan_dir = 1;
  int wander_dir = 1;
  int wander_steps_left = 0;

  // movement memory
  int last_dir = 1;
  std::deque<std::pair<int, int>> recent_positions;

  int manhattan(int r1, int c1, int r2, int c2) {
    return std::abs(r1 - r2) + std::abs(c1 - c2);
  }

  void dirToDelta(int dir, int &dr, int &dc) {
    dr = dc = 0;
    switch (dir) {
    case 1:
      dr = -1;
      break;
    case 2:
      dr = -1;
      dc = 1;
      break;
    case 3:
      dc = 1;
      break;
    case 4:
      dr = 1;
      dc = 1;
      break;
    case 5:
      dr = 1;
      break;
    case 6:
      dr = 1;
      dc = -1;
      break;
    case 7:
      dc = -1;
      break;
    case 8:
      dr = -1;
      dc = -1;
      break;
    }
  }

  // =========================
  // obstacle proximity penalty
  // =========================
  int obstacle_penalty(int r, int c) {
    int penalty = 0;
    for (auto &obs : known_obstacles) {
      int d = manhattan(r, c, obs.first, obs.second);
      if (d <= 2)
        penalty += (3 - d) * 2;
    }
    return penalty;
  }

  bool near_obstacle(int r, int c) {
    for (auto &obs : known_obstacles) {
      if (manhattan(r, c, obs.first, obs.second) <= 1)
        return true;
    }
    return false;
  }

public:
  Robot_SparksFly() : RobotBase(2, 5, flamethrower) {}

  // =========================
  // RADAR
  // =========================
  void get_radar_direction(int &radar_direction) override {
    radar_direction = scan_dir;
    scan_dir = (scan_dir % 8) + 1;
  }
  void
  process_radar_results(const std::vector<RadarObj> &radar_results) override {
    int my_r, my_c;
    get_current_location(my_r, my_c);

    int best_dist = std::numeric_limits<int>::max();

    int new_target_r = -1;
    int new_target_c = -1;

    // scan everything
    for (const auto &obj : radar_results) {

      // track obstacles
      if (obj.m_type == 'M' || obj.m_type == 'F') {
        known_obstacles.insert({obj.m_row, obj.m_col});
      }

      // ONLY consider alive robots
      if (obj.m_type == 'R') {
        int d = manhattan(my_r, my_c, obj.m_row, obj.m_col);

        if (d < best_dist) {
          best_dist = d;
          new_target_r = obj.m_row;
          new_target_c = obj.m_col;
        }
      }
    }

    // ✅ ALWAYS switch to closest visible robot
    if (new_target_r != -1) {
      target_row = new_target_r;
      target_col = new_target_c;
      has_target = true;
    } else {
      // ❌ nothing visible → drop target completely
      has_target = false;
      target_row = -1;
      target_col = -1;
    }
  }
  // =========================
  // SHOOT
  // =========================
  bool get_shot_location(int &shot_row, int &shot_col) override {
    if (!has_target)
      return false;

    int my_r, my_c;
    get_current_location(my_r, my_c);

    if (std::abs(target_row - my_r) <= 4 && std::abs(target_col - my_c) <= 3) {
      shot_row = target_row;
      shot_col = target_col;
      return true;
    }

    return false;
  }

  // =========================
  // A* PATHFINDING
  // =========================
  std::vector<std::pair<int, int>> find_path(int sr, int sc, int tr, int tc) {
    using Node = std::pair<int, int>;

    std::set<Node> closed;
    std::map<Node, Node> parent;
    std::map<Node, int> g_score;

    auto cmp = [&](auto &a, auto &b) { return a.first > b.first; };
    std::priority_queue<std::pair<int, Node>, std::vector<std::pair<int, Node>>,
                        decltype(cmp)>
        open(cmp);

    Node start = {sr, sc};
    Node goal = {tr, tc};

    g_score[start] = 0;
    open.push({manhattan(sr, sc, tr, tc), start});

    while (!open.empty()) {
      Node cur = open.top().second;
      open.pop();

      if (cur == goal) {
        std::vector<Node> path;
        while (cur != start) {
          path.push_back(cur);
          cur = parent[cur];
        }
        std::reverse(path.begin(), path.end());
        return path;
      }

      if (closed.count(cur))
        continue;
      closed.insert(cur);

      for (int dir = 1; dir <= 8; dir++) {
        int dr, dc;
        dirToDelta(dir, dr, dc);

        Node nxt = {cur.first + dr, cur.second + dc};

        if (nxt.first < 0 || nxt.first >= m_board_row_max || nxt.second < 0 ||
            nxt.second >= m_board_col_max)
          continue;

        if (known_obstacles.count(nxt))
          continue;

        int tentative =
            g_score[cur] + 1 + obstacle_penalty(nxt.first, nxt.second);

        if (!g_score.count(nxt) || tentative < g_score[nxt]) {
          g_score[nxt] = tentative;
          parent[nxt] = cur;

          int f = tentative + manhattan(nxt.first, nxt.second, tr, tc);
          open.push({f, nxt});
        }
      }
    }

    return {};
  }
  // =========================
  // MOVEMENT
  // =========================
  void get_move_direction(int &move_direction, int &move_distance) override {
    int my_r, my_c;
    get_current_location(my_r, my_c);

    recent_positions.push_back({my_r, my_c});
    if (recent_positions.size() > 6)
      recent_positions.pop_front();

    int speed = get_move_speed();
    int max_step = std::min(2, speed);
    // =========================
    // HARD ATTACK LOCK
    // =========================
    if (has_target) {
      int shot_r, shot_c;

      if (get_shot_location(shot_r, shot_c)) {
        // Stay still and keep firing
        move_direction = 0;
        move_distance = 0;
        return;
      }
    }

    // =========================
    // STUCK DETECTION
    // =========================
    int repeat = 0;
    for (auto &p : recent_positions)
      if (p.first == my_r && p.second == my_c)
        repeat++;

    bool stuck = (repeat >= 3);

    // =========================
    // TARGET LOGIC
    // =========================
    if (has_target) {
      int dr = target_row - my_r;
      int dc = target_col - my_c;

      int adr = std::abs(dr);
      int adc = std::abs(dc);

      // =========================
      // 1. BLIND SPOT FIX (2,3)
      // =========================
      if ((adr == 2 && adc == 3) || (adr == 3 && adc == 2)) {
        int step_r = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
        int step_c = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);

        if (adr > adc) {
          move_direction = (step_r < 0) ? 1 : 5;
        } else {
          move_direction = (step_c > 0) ? 3 : 7;
        }

        move_distance = 1;
        return;
      }

      // =========================
      // 2. PRE-SHOOT ALIGNMENT
      // =========================
      if (adr <= 4 && adc <= 4) {
        for (int dir = 1; dir <= 8; dir++) {
          int ddr, ddc;
          dirToDelta(dir, ddr, ddc);

          int nr = my_r + ddr;
          int nc = my_c + ddc;

          // simulate shot from new position
          for (int d = 1; d <= 8; d++) {
            int bdr, bdc;
            dirToDelta(d, bdr, bdc);

            for (int i = 1; i <= 4; i++) {
              for (int spread = -1; spread <= 1; spread++) {

                int r = nr + bdr * i + (bdc != 0 ? spread : 0);
                int c = nc + bdc * i + (bdr != 0 ? spread : 0);

                if (r == target_row && c == target_col) {
                  move_direction = dir;
                  move_distance = 1;
                  return;
                }
              }
            }
          }
        }
      }

      // =========================
      // 3. PATHFINDING
      // =========================
      if (!stuck) {
        auto path = find_path(my_r, my_c, target_row, target_col);

        if (!path.empty()) {
          int steps = std::min((int)path.size(), max_step);

          int nr = path[steps - 1].first;
          int nc = path[steps - 1].second;

          int dr2 = nr - my_r;
          int dc2 = nc - my_c;

          int dir = 0;
          if (dr2 < 0 && dc2 == 0)
            dir = 1;
          else if (dr2 < 0 && dc2 > 0)
            dir = 2;
          else if (dr2 == 0 && dc2 > 0)
            dir = 3;
          else if (dr2 > 0 && dc2 > 0)
            dir = 4;
          else if (dr2 > 0 && dc2 == 0)
            dir = 5;
          else if (dr2 > 0 && dc2 < 0)
            dir = 6;
          else if (dr2 == 0 && dc2 < 0)
            dir = 7;
          else if (dr2 < 0 && dc2 < 0)
            dir = 8;

          last_dir = dir;
          move_direction = dir;
          move_distance = steps;
          return;
        }
      }
    }
    // =========================
    // WANDER / SAFE EXPLORE
    // =========================

    // try to move toward unexplored space by biasing scan direction
    int start = scan_dir;

    for (int i = 0; i < 8; i++) {
      int dir = ((start + i - 1) % 8) + 1;

      int dr, dc;
      dirToDelta(dir, dr, dc);

      int r = my_r + dr;
      int c = my_c + dc;

      // ONLY safety checks we are allowed to use
      if (r >= m_board_row_max || r < 0 || c >= m_board_col_max || c < 0)
        continue;

      if (known_obstacles.count({r, c}))
        continue;

      // optional: avoid immediate looping
      bool recently_visited = false;
      for (auto &p : recent_positions) {
        if (p.first == r && p.second == c) {
          recently_visited = true;
          break;
        }
      }
      if (recently_visited)
        continue;

      move_direction = dir;
      move_distance = 1;
      return;
    }

    // last resort: if fully boxed in, just rotate scan and stay safe
    scan_dir = (scan_dir % 8) + 1;
    move_direction = scan_dir;
    move_distance = 0;
    return;
  }
};

// =========================
// FACTORY
// =========================
extern "C" RobotBase *create_robot() { return new Robot_SparksFly(); }

extern "C" const char *robot_summary() {
  return "Improved hammer bot with A*, obstacle avoidance, anti-stuck logic, "
         "and smart wandering.";
}
