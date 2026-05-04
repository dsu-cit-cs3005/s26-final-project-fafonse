#include "Arena.h"
#include "RadarObj.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>

Arena::Arena(int r, int c, int max_rounds, float sleep_interval,
             int flamethrower_count, int pit_count, int mound_count)
    : rows(r), cols(c), max_rounds(max_rounds), sleep_interval(sleep_interval),
      board(rows, std::vector<char>(cols, '.')) // fill with '.'
{
  for (int i = 0; i < flamethrower_count; ++i) {
    board[rand() % rows][rand() % cols] = 'F';
  }
  for (int i = 0; i < pit_count; ++i) {
    board[rand() % rows][rand() % cols] = 'P';
  }
  for (int i = 0; i < mound_count; ++i) {
    board[rand() % rows][rand() % cols] = 'M';
  }
}

void Arena::addRobot(RobotBase *robot, const std::string &name) {
  RobotState rbt;
  rbt.robot = robot;
  rbt.health = 100;
  rbt.alive = true;
  rbt.name = name;

  // 🔒 Find a valid spawn (no robot, no blocking obstacle)
  int attempts = 0;
  do {
    rbt.row = rand() % rows;
    rbt.col = rand() % cols;
    attempts++;

    // prevent infinite loop if board is full
    if (attempts > 1000) {
      std::cerr << "Failed to place robot safely!\n";
      break;
    }

  } while (getRobotAt(rbt.row, rbt.col) != nullptr || // no overlap
           board[rbt.row][rbt.col] == 'M'             // avoid mound spawn
  );

  // 🧠 Initialize robot with arena info
  rbt.robot->set_boundaries(rows, cols);
  rbt.robot->move_to(rbt.row, rbt.col);

  rbt.armor = robot->get_armor();
  rbt.move_speed = robot->get_move_speed();
  rbt.weapon = robot->get_weapon();

  robots.push_back(rbt);

  std::cout << "Spawned " << name << " at (" << rbt.row << "," << rbt.col
            << ")\n";
}

void Arena::run() {
  int turn = 0;

  while (true) {
    std::cout << "========== Turn " << turn << " ==========\n";

    int aliveCount = 0;

    for (auto &r : robots) {
      if (!r.alive)
        continue;

      aliveCount++;

      std::cout << "[TURN] " << r.name << " at (" << r.row << "," << r.col
                << ")\n";

      processTurn(r);
    }

    // 🧹 Post-turn cleanup / status print
    printState();
    std::this_thread::sleep_for(std::chrono::duration<float>(sleep_interval));
    // 🏁 Check win condition AFTER full round
    if (aliveCount <= 1)
      break;

    turn++;

    // 🛑 Hard safety stop (prevents grading penalties)
    if (turn >= max_rounds) {
      std::cout << "Max turns reached. Ending game.\n";
      break;
    }
  }

  // 🏆 Determine winner
  RobotState *winner = nullptr;

  for (auto &r : robots) {
    if (r.alive) {
      winner = &r;
      break;
    }
  }

  std::cout << "========== Game Over ==========\n";

  if (winner) {
    std::cout << "Winner: " << winner->name << " 🎉\n";
  } else {
    std::cout << "No winner (all robots destroyed)\n";
  }
}

void Arena::processTurn(RobotState &r) {
  if (!r.alive)
    return; // safety

  // --- 1. RADAR ---
  int radar_dir = 0;
  r.robot->get_radar_direction(radar_dir);

  auto radar_results = performRadar(r, radar_dir);
  r.robot->process_radar_results(radar_results);

  // --- 2. SHOOT ---
  int shot_row = -1, shot_col = -1;
  if (r.robot->get_shot_location(shot_row, shot_col) == true) {
    handleShoot(r, shot_row, shot_col);
    std::cout << r.name << " shoots at (" << shot_row << "," << shot_col
              << ")\n";

    return; // 🚨 MUST stop here (one action rule)
  }

  // --- 3. MOVE ---
  int move_dir = 0, move_dist = 0;
  r.robot->get_move_direction(move_dir, move_dist);
  std::cout << move_dist;

  // Clamp movement to robot capability (prevents cheating / bugs)

  if (move_dist > 0) {
    std::cout << r.name << " moves (dir=" << move_dir << ", dist=" << move_dist
              << ")\n";

    handleMove(r, move_dir, move_dist);
  } else {
    std::cout << r.name << " does nothing\n";
  }
}

void Arena::handleShoot(RobotState &shooter, int target_row, int target_col) {
  WeaponType weapon = shooter.robot->get_weapon();

  // =========================
  // 🔨 HAMMER (adjacent only)
  // =========================
  if (weapon == hammer) {
    int dr = abs(shooter.row - target_row);
    int dc = abs(shooter.col - target_col);

    if (dr <= 1 && dc <= 1) {
      RobotState *target = getRobotAt(target_row, target_col);
      if (target) {
        // applyHit(&shooter, *target, rand() % 11 + 50); // 50 - 60 dmg
        applyHit(&shooter, *target, 100);
      }
    }
  }

  // =========================
  // ⚡ RAILGUN (line through map)
  // =========================
  else if (weapon == railgun) {
    int dr =
        (target_row > shooter.row) ? 1 : (target_row < shooter.row ? -1 : 0);
    int dc =
        (target_col > shooter.col) ? 1 : (target_col < shooter.col ? -1 : 0);

    int r = shooter.row + dr;
    int c = shooter.col + dc;

    while (inBounds(r, c)) {
      RobotState *target = getRobotAt(r, c);
      if (target) {
        applyHit(&shooter, *target, rand() % 11 + 20); // 10 - 20 damage
      }
      r += dr;
      c += dc;
    }
  }

  // =========================
  // 🔥 FLAMETHROWER (3x4 area)
  // =========================
  else if (weapon == flamethrower) {
    int dr =
        (target_row > shooter.row) ? 1 : (target_row < shooter.row ? -1 : 0);
    int dc =
        (target_col > shooter.col) ? 1 : (target_col < shooter.col ? -1 : 0);

    for (int i = 1; i <= 4; i++) {
      for (int spread = -1; spread <= 1; spread++) {
        int r = shooter.row + dr * i + (dc != 0 ? spread : 0);
        int c = shooter.col + dc * i + (dr != 0 ? spread : 0);

        if (!inBounds(r, c))
          continue;

        RobotState *target = getRobotAt(r, c);
        if (target) {
          applyHit(&shooter, *target, rand() % 21 + 30); // 30 - 50 damage
        }
      }
    }
  }

  // =========================
  // 💣 GRENADE (3x3 area)
  // =========================
  else if (weapon == grenade) {
    if (shooter.robot->get_grenades() <= 0)
      return;

    shooter.robot->decrement_grenades();

    for (int dr = -1; dr <= 1; dr++) {
      for (int dc = -1; dc <= 1; dc++) {
        int r = target_row + dr;
        int c = target_col + dc;

        if (!inBounds(r, c))
          continue;

        RobotState *target = getRobotAt(r, c);
        if (target) {
          applyHit(&shooter, *target, rand() % 31 + 10); // 10 - 40 damage
        }
      }
    }
  }
}

void Arena::handleMove(RobotState &r, int dir, int dist) {
  // 🚨 Respect move speed (IMPORTANT for grading)
  int maxMove = r.robot->get_move_speed();
  if (dist > maxMove)
    dist = maxMove;

  for (int i = 0; i < dist; i++) {
    // 🚨 Pit effect: cannot move
    if (board[r.row][r.col] == 'P') {
      r.robot->disable_movement(); // set move speed to 0
      break;                       // stop movement afterwards
    }

    int newRow = r.row + directions[dir].first;
    int newCol = r.col + directions[dir].second;

    // 🧱 boundary check
    if (!inBounds(newRow, newCol))
      break;

    char cell = board[newRow][newCol];

    // 🧱 Mound blocks movement
    if (cell == 'M')
      break;

    // 🤖 robot collision
    if (getRobotAt(newRow, newCol))
      break;

    // 🔥 Flamethrower obstacle damage
    if (cell == 'F') {
      applyHit(nullptr, r, rand() % 21 + 30);
      std::cout << r.name << " hit flame obstacle!\n";

      if (r.robot->get_health() <= 0) {
        r.alive = false;
        return;
      }
    }
    // 🚶 move step
    r.row = newRow;
    r.col = newCol;

    // 🔄 sync with RobotBase (IMPORTANT)
    r.robot->move_to(r.row, r.col);
  }
  return;
}

std::vector<RadarObj> Arena::performRadar(RobotState &r, int direction) {
  std::vector<RadarObj> results;

  // Lambda to handle scanning a single cell
  auto scanCell = [&](int rr, int cc) -> bool {
    if (!inBounds(rr, cc))
      return false;

    // Check for robot first (consistent across modes)
    RobotState *other = getRobotAt(rr, cc);
    if (other && other != &r) {
      RadarObj other_robot;
      other_robot.m_row = rr;
      other_robot.m_col = cc;

      if (other->alive) // specify dead or alive
        other_robot.m_type = 'R';
      else
        other_robot.m_type = 'X';
      results.push_back(other_robot);
      return false; // keep scanning
    }

    char cell = board[rr][cc];
    if (cell == '.')
      return false;

    results.push_back(RadarObj(cell, rr, cc));

    // Stop scan if mound
    return (cell == 'M');
  };

  if (direction == 0) { // nearby scan
    for (int dr = -1; dr <= 1; dr++) {
      for (int dc = -1; dc <= 1; dc++) {
        if (dr == 0 && dc == 0)
          continue;

        int rr = r.row + dr;
        int cc = r.col + dc;

        if (scanCell(rr, cc)) {
          return results; // early exit on mound
        }
      }
    }
    return results;
  }

  // Beam scan
  int dr = directions[direction].first;
  int dc = directions[direction].second;
  int pr = -dc; // perpendicular
  int pc = dr;

  int row = r.row + dr;
  int col = r.col + dc;

  while (inBounds(row, col)) {
    for (int spread = -1; spread <= 1; spread++) {
      int rr = row + pr * spread;
      int cc = col + pc * spread;

      if (scanCell(rr, cc)) {
        return results; // early exit on mound
      }
    }

    row += dr;
    col += dc;
  }

  return results;
}

Arena::RobotState *Arena::getRobotAt(int r, int c) {
  for (auto &rbt : robots) {
    if (rbt.row == r && rbt.col == c) {
      return &rbt; // return pointer to robot at location
    }
  }
  return nullptr; // nothing there
}

void Arena::printState() {
  // 1. build a fresh display grid from board
  std::vector<std::vector<char>> display = board;

  // 2. overlay robots
  for (const auto &r : robots) {
    if (r.alive) {
      display[r.row][r.col] = 'R'; // alive robot
    } else {
      display[r.row][r.col] = 'X'; // dead robot
    }
  }

  // 3. print column headers
  std::cout << "\n   ";
  for (int c = 0; c < cols; c++) {
    std::cout << c % 10 << " ";
  }
  std::cout << "\n";

  // 4. print rows
  for (int r = 0; r < rows; r++) {
    std::cout << std::setw(2) << r << " ";

    for (int c = 0; c < cols; c++) {
      std::cout << display[r][c] << " ";
    }
    std::cout << "\n";
  }

  // 5. optional robot status (VERY helpful for grading)
  std::cout << "\n--- Robot Status ---\n";
  for (const auto &r : robots) {
    std::cout << r.name << " (" << r.row << "," << r.col << ") "
              << (r.alive ? "ALIVE" : "DEAD") << " HP:" << r.robot->get_health()
              << " ARM:" << r.robot->get_armor() << "\n";
  }

  std::cout << "--------------------\n\n";
}

bool Arena::inBounds(int r, int c) const {
  return (r >= 0 && r < rows && c >= 0 && c < cols);
}

void Arena::applyHit(RobotState *shooter, RobotState &target, int baseDamage) {
  if (!target.alive)
    return;

  std::string name;
  if (shooter == nullptr)
    name = "Flame Trap"; // default bro
  else
    name = shooter->name;
  if (&target == shooter)
    return;

  float reduction = target.robot->get_armor() * 01.f;
  if (reduction > 0.9f)
    reduction = 0.9f; // clamp reduction to 90% max
  int totalDamage = static_cast<int>(baseDamage * (1.0f - reduction));

  target.robot->take_damage(totalDamage);
  target.robot->reduce_armor(1);

  std::cout << name << " hit " << target.name << "\n";

  target.alive = (target.robot->get_health() > 0);
};
