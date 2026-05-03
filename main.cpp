#include <iostream>
#include <cstdlib>
#include <ctime>

#include "Arena.h"
#include "RobotBase.h"

// If you are loading real .so files later, you would use dlopen here.
// For now, this assumes you are manually adding robots via create_robot().

extern RobotBase* create_robot();  // from robot files (factory function)

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    int rows = 10;
    int cols = 10;

    Arena arena(rows, cols);

    std::cout << "=== RobotWarz Starting ===\n";

    // -----------------------------
    // Add robots (manual test setup)
    // -----------------------------
    RobotBase* r1 = create_robot();
    RobotBase* r2 = create_robot();

    arena.addRobot(r1, "Alpha");
    arena.addRobot(r2, "Beta");

    // -----------------------------
    // Run simulation
    // -----------------------------
    arena.run();

    std::cout << "=== Simulation Complete ===\n";

    return 0;
}
