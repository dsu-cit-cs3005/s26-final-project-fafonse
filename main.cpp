#include "Arena.h"
#include "RobotBase.h"

#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Config {
  int rows = 20;
  int cols = 20;
};

Config loadConfig(const std::string &file) {
  Config cfg;
  std::ifstream in(file);
  std::string line;

  while (std::getline(in, line)) {
    std::stringstream ss(line);
    std::string key, val;

    std::getline(ss, key, ':');
    std::getline(ss, val);

    if (key == "Arena_Size") {
      std::stringstream v(val);
      v >> cfg.rows >> cfg.cols;
    }
  }

  return cfg;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: ./RobotWarz config.txt\n";
    return 1;
  }

  srand(time(nullptr));

  Config cfg = loadConfig(argv[1]);

  Arena arena(cfg.rows, cfg.cols);

  std::vector<void *> handles;

  // ==============================
  // 1. Find robot source files
  // ==============================
  std::vector<std::string> robotFiles;

  for (auto &entry : fs::directory_iterator("robots")) {
    if (entry.path().extension() == ".cpp") {
      robotFiles.push_back(entry.path().string());
    }
  }

  // ==============================
  // 2. Compile + load each robot
  // ==============================
  for (auto &filename : robotFiles) {
    std::string baseName = fs::path(filename).stem().string();
    std::string sharedLib = "robots/" + baseName + ".so";

    // compile
    std::string compileCmd = "g++ -shared -fPIC -o " + sharedLib + " " +
                             filename + " RobotBase.o -I. -std=c++20";

    std::cout << "Compiling " << filename << "\n";

    if (std::system(compileCmd.c_str()) != 0) {
      std::cerr << "Compile failed: " << filename << "\n";
      continue;
    }

    // load .so
    void *handle = dlopen(sharedLib.c_str(), RTLD_LAZY);

    if (!handle) {
      std::cerr << "dlopen failed: " << dlerror() << "\n";
      continue;
    }

    handles.push_back(handle);

    // find factory function
    RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");

    if (!create_robot) {
      std::cerr << "Missing create_robot in " << sharedLib << "\n";
      dlclose(handle);
      continue;
    }

    // create robot
    RobotBase *robot = create_robot();

    if (!robot) {
      std::cerr << "Robot creation failed\n";
      continue;
    }

    // name from file
    std::string robotName = baseName;

    arena.addRobot(robot, robotName);
  }

  // ==============================
  // 3. Run game
  // ==============================
  arena.run();

  // cleanup
  for (auto h : handles)
    dlclose(h);

  return 0;
}
