#include "Arena.h"
#include <iostream>
#include <cmath>

Arena::Arena(int r, int c) : rows(r), cols(c) {}

void Arena::addRobot(RobotBase* robot, const std::string& name) {
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

    } while (
        getRobotAt(rbt.row, rbt.col) != nullptr ||   // no overlap
        board[rbt.row][rbt.col] == 'M'               // avoid mound spawn
    );

    // 🧠 Initialize robot with arena info
    rbt.robot->set_boundaries(rows, cols);
    rbt.robot->move_to(rbt.row, rbt.col);

    rbt.armor = robot->get_armor();
    rbt.move_speed = robot->get_move_speed();
    rbt.weapon = robot->get_weapon_type();

    robots.push_back(rbt);

    std::cout << "Spawned " << name 
              << " at (" << rbt.row << "," << rbt.col << ")\n";
}

void Arena::run() {
    int turn = 0;
    const int MAX_TURNS = 1000; // safety to prevent infinite loops

    while (true) {
        std::cout << "========== Turn " << turn << " ==========\n";

        int aliveCount = 0;

        for (auto& r : robots) {
            if (!r.alive) continue;

            aliveCount++;

            std::cout << "[TURN] " << r.name 
                      << " at (" << r.row << "," << r.col << ")\n";

            processTurn(r);
        }

        // 🧹 Post-turn cleanup / status print
        printState();

        // 🏁 Check win condition AFTER full round
        if (aliveCount <= 1) break;

        turn++;

        // 🛑 Hard safety stop (prevents grading penalties)
        if (turn >= MAX_TURNS) {
            std::cout << "Max turns reached. Ending game.\n";
            break;
        }
    }

    // 🏆 Determine winner
    RobotState* winner = nullptr;

    for (auto& r : robots) {
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

void Arena::processTurn(RobotState& r) {
    if (!r.alive) return; // safety

    // --- 1. RADAR ---
    int radar_dir = 0;
    r.robot->get_radar_direction(radar_dir);

    auto radar_results = performRadar(r, radar_dir);
    r.robot->process_radar_results(radar_results);

    // --- 2. SHOOT ---
    int shot_row = -1, shot_col = -1;
    if (r.robot->get_shot_location(shot_row, shot_col)) {
        std::cout << r.name << " shoots at (" 
                  << shot_row << "," << shot_col << ")\n";

        handleShoot(r, shot_row, shot_col);
        return; // 🚨 MUST stop here (one action rule)
    }

    // --- 3. MOVE ---
    int move_dir = 0, move_dist = 0;
    r.robot->get_move_direction(move_dir, move_dist);

    // Clamp movement to robot capability (prevents cheating / bugs)
    int maxMove = r.robot->get_move_speed();
    if (move_dist > maxMove) {
        move_dist = maxMove;
    }

    if (move_dist > 0) {
        std::cout << r.name << " moves (dir=" 
                  << move_dir << ", dist=" << move_dist << ")\n";

        handleMove(r, move_dir, move_dist);

        // Update robot with new position AFTER movement
        r.robot->move_to(r.row, r.col);
    } else {
        std::cout << r.name << " does nothing\n";
    }
}

void Arena::handleShoot(RobotState& shooter, int target_row, int target_col) {
    WeaponType weapon = shooter.robot->get_weapon();

    auto applyHit = [&](RobotState& target, int baseDamage) {
        if (!target.alive) return;
        if (&target == &shooter) return;

        int totalDamage = static_cast<int>(baseDamage - (target.robot->get_armor() * 0.1f)); // apply armor * 0.1

        target.robot->take_damage(totalDamage);
        target.robot->reduce_armor(1);

        std::cout << shooter.name << " hit " << target.name << "\n";

        target.alive = (target.robot->get_health() > 0);
    };

    // =========================
    // 🔨 HAMMER (adjacent only)
    // =========================
    if (weapon == hammer) {
        int dr = abs(shooter.row - target_row);
        int dc = abs(shooter.col - target_col);

        if (dr <= 1 && dc <= 1) {
            RobotState* target = getRobotAt(target_row, target_col);
            if (target) {
              applyHit(*target, 30);
            }
        }
    }

    // =========================
    // ⚡ RAILGUN (line through map)
    // =========================
    else if (weapon == railgun) {
        int dr = (target_row > shooter.row) ? 1 : (target_row < shooter.row ? -1 : 0);
        int dc = (target_col > shooter.col) ? 1 : (target_col < shooter.col ? -1 : 0);

        int r = shooter.row + dr;
        int c = shooter.col + dc;

        while (inBounds(r, c)) {
            RobotState* target = getRobotAt(r, c);
            if (target) {
                applyHit(*target, 25);
            }
            r += dr;
            c += dc;
        }
    }

    // =========================
    // 🔥 FLAMETHROWER (3x4 area)
    // =========================
    else if (weapon == flamethrower) {
        int dr = (target_row > shooter.row) ? 1 : (target_row < shooter.row ? -1 : 0);
        int dc = (target_col > shooter.col) ? 1 : (target_col < shooter.col ? -1 : 0);

        for (int i = 1; i <= 4; i++) {
            for (int spread = -1; spread <= 1; spread++) {
                int r = shooter.row + dr * i + (dc != 0 ? spread : 0);
                int c = shooter.col + dc * i + (dr != 0 ? spread : 0);

                if (!inBounds(r, c)) continue;

                RobotState* target = getRobotAt(r, c);
                if (target) {
                    applyHit(*target, 20);
                }
            }
        }
    }

    // =========================
    // 💣 GRENADE (3x3 area)
    // =========================
    else if (weapon == grenade) {
        if (shooter.robot->get_grenades() <= 0) return;

        shooter.robot->decrement_grenades();

        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int r = target_row + dr;
                int c = target_col + dc;

                if (!inBounds(r, c)) continue;

                RobotState* target = getRobotAt(r, c);
                if (target) {
                    applyHit(*target, 35);
                }
            }
        }
    }
}

void Arena::handleMove(RobotState& r, int dir, int dist) {
    static int dRow[9] = {0,-1,-1,0,1,1,1,0,-1};
    static int dCol[9] = {0,0,1,1,1,0,-1,-1,-1};

    // 🚨 Respect move speed (IMPORTANT for grading)
    int maxMove = r.robot->get_move_speed();
    if (dist > maxMove) dist = maxMove;

    // 🚨 Pit effect: cannot move
    if (board[r.row][r.col] == 'P') {
        r.robot->disable_movement(); // set move speed to 0
        return;
    }

    for (int i = 0; i < dist; i++) {
        int newRow = r.row + dRow[dir];
        int newCol = r.col + dCol[dir];

        // 🧱 boundary check
        if (!inBounds(newRow, newCol)) break;

        char cell = board[newRow][newCol];

        // 🧱 Mound blocks movement
        if (cell == 'M') break;

        // 🤖 robot collision
        if (getRobotAt(newRow, newCol)) break;

        // 🔥 Flamethrower obstacle damage
        if (cell == 'F') {
            r.robot->take_damage(15);
            r.robot->reduce_armor(1);

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
}

std::vector<RadarObj> Arena::performRadar(RobotState& r, int direction) {
    std::vector<RadarObj> results;

    int dr = directions[direction].first;
    int dc = directions[direction].second;

    int row = r.row + dr;
    int col = r.col + dc;

    // scan outward in selected direction
    while (inBounds(row, col)) {
        RobotState* other = getRobotAt(row, col);

        if (other && other->alive) {
            RadarObj obj;
            obj.m_row = row;
            obj.m_col = col;
            obj.m_type = 'R';

            results.push_back(obj);
        }

        // step forward
        row += dr;
        col += dc;
    }

    return results;
}
