#include "GameCtrl.h"
#include "util.h"
#include <stdexcept>
#include <cstdio>
#include <chrono>
#include <cstdlib>
#ifdef _WIN32
#include <Windows.h>
#endif
#pragma warning(disable:4996)

using std::string;
using std::list;

const string GameCtrl::MSG_BAD_ALLOC = "Not enough memory to run the game.";
const string GameCtrl::MSG_LOSE = "Oops! You lose!";
const string GameCtrl::MSG_WIN = "Congratulations! You Win!";
const string GameCtrl::MSG_ESC = "Game ended.";
const string GameCtrl::MAP_INFO_FILENAME = "movements.txt";

GameCtrl::GameCtrl() {}

GameCtrl::~GameCtrl() {
    delete map;
    map = nullptr;
    if (movementFile) {
        fclose(movementFile);
        movementFile = nullptr;
    }
}

GameCtrl* GameCtrl::getInstance() {
    static GameCtrl instance;
    return &instance;
}

void GameCtrl::setFPS(const double fps_) {
    fps = fps_;
}

void GameCtrl::setEnableAI(const bool enableAI_) {
    enableAI = enableAI_;
}

void GameCtrl::setEnableHamilton(const bool enableHamilton_) {
    enableHamilton = enableHamilton_;
}

void GameCtrl::setMoveInterval(const long ms) {
    moveInterval = ms;
}

void GameCtrl::setRecordMovements(const bool b) {
    recordMovements = b;
}

void GameCtrl::setRunTest(const bool b) {
    runTest = b;
}

void GameCtrl::setMapRow(const SizeType n) {
    mapRowCnt = n;
}

void GameCtrl::setMapCol(const SizeType n) {
    mapColCnt = n;
}

int GameCtrl::run() {
    try {
        init();
        if (runTest) {
            test();
        }
        while (runMainThread) {}
        return 0;
    } catch (const std::exception &e) {
        exitGameErr(e.what());
        return -1;
    }
}

void GameCtrl::sleepFPS() const {
    util::sleep((long)((1.0 / fps) * 1000));
}

void GameCtrl::exitGame(const std::string &msg) {
    mutexExit.lock();
    if (runMainThread) {
        util::sleep(100);
        runSubThread = false;
        util::sleep(100);
        printMsg(msg);
    }
    mutexExit.unlock();
    runMainThread = false;
}

void GameCtrl::exitGameErr(const std::string &err) {
    exitGame("ERROR: " + err);
}

void GameCtrl::printMsg(const std::string &msg) {
    Console::setCursor(0, (int)mapRowCnt);
    Console::writeWithColor(msg + "\n", ConsoleColor(WHITE, BLACK, true, false));
}

void GameCtrl::moveSnake() {
    mutexMove.lock();
    if (map->isAllBody()) {
        mutexMove.unlock();
        exitGame(MSG_WIN);
    } else if (snake.isDead()) {
        mutexMove.unlock();
        exitGame(MSG_LOSE);
    } else {
        try {
            snake.move();
            if (recordMovements && snake.getDirection() != NONE) {
                writeMapToFile();
            }
            if (!map->hasFood()) {
                map->createRandFood();
            }
            mutexMove.unlock();
        } catch (const std::exception) {
            mutexMove.unlock();
            throw;
        }
    }
}

void GameCtrl::writeMapToFile() const {
    if (!movementFile) {
        return;
    }
    SizeType rows = map->getRowCount(), cols = map->getColCount();
    for (SizeType i = 0; i < rows; ++i) {
        for (SizeType j = 0; j < cols; ++j) {
            switch (map->getPoint(Pos(i, j)).getType()) {
                case Point::Type::EMPTY:
                    fwrite("  ", sizeof(char), 2, movementFile); break;
                case Point::Type::WALL:
                    fwrite("# ", sizeof(char), 2, movementFile); break;
                case Point::Type::FOOD:
                    fwrite("F ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_HEAD:
                    fwrite("H ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_BODY:
                    fwrite("B ", sizeof(char), 2, movementFile); break;
                case Point::Type::SNAKE_TAIL:
                    fwrite("T ", sizeof(char), 2, movementFile); break;
                default:
                    break;
            }
        }
        fwrite("\n", sizeof(char), 1, movementFile);
    }
    fwrite("\n", sizeof(char), 1, movementFile);
}

void GameCtrl::init() {
    Console::clear();
    initMap();
    if (!runTest) {
        initSnake();
        if (recordMovements) {
            initFiles();
        }
    }
    startThreads();
}

void GameCtrl::initMap() {
    if (mapRowCnt < 5 || mapColCnt < 5) {
        string msg = "GameCtrl.initMap(): Map size at least 5*5. Current size "
            + util::toString(mapRowCnt) + "*" + util::toString(mapColCnt) + ".";
        throw std::range_error(msg.c_str());
    }
    map = new Map(mapRowCnt, mapColCnt);
    if (!map) {
        exitGameErr(MSG_BAD_ALLOC);
    } else {
        // Add some extra walls manully
    }
}

void GameCtrl::initSnake() {
    snake.setMap(map);
    snake.addBody(Pos(1, 3));
    snake.addBody(Pos(1, 2));
    snake.addBody(Pos(1, 1));
    if (enableHamilton) {
        snake.enableHamilton();
    }
}

void GameCtrl::initFiles() {
    movementFile = fopen(MAP_INFO_FILENAME.c_str(), "w");
    if (!movementFile) {
        throw std::runtime_error("GameCtrl.initFiles(): Fail to open file: " + MAP_INFO_FILENAME);
    } else {
        // Write content description to the file
        string str = "Content description:\n";
        str += "#: wall\nH: snake head\nB: snake body\nT: snake tail\nF: food\n\n";
        str += "Movements:\n\n";
        fwrite(str.c_str(), sizeof(char), str.length(), movementFile);
    }
}

void GameCtrl::startThreads() {
    runSubThread = true;
    drawThread = std::thread(&GameCtrl::draw, this);
    drawThread.detach();
    keyboardThread = std::thread(&GameCtrl::keyboard, this);
    keyboardThread.detach();
    if (!runTest) {
        moveThread = std::thread(&GameCtrl::autoMove, this);
        moveThread.detach();
    }
}

void GameCtrl::draw() {
    try {
        while (runSubThread) {
            drawMapContent();
            sleepFPS();
        }
    } catch (const std::exception &e) {
        exitGameErr(e.what());
    }
}

void GameCtrl::drawMapContent() const {
    Console::setCursor();
    SizeType row = map->getRowCount(), col = map->getColCount();
    for (SizeType i = 0; i < row; ++i) {
        for (SizeType j = 0; j < col; ++j) {
            const Point &point = map->getPoint(Pos(i, j));
            switch (point.getType()) {
                case Point::Type::EMPTY:
                    Console::writeWithColor("  ", ConsoleColor(BLACK, BLACK));
                    break;
                case Point::Type::WALL:
                    Console::writeWithColor("  ", ConsoleColor(WHITE, WHITE, true, true));
                    break;
                case Point::Type::FOOD:
                    Console::writeWithColor("  ", ConsoleColor(YELLOW, YELLOW, true, true));
                    break;
                case Point::Type::SNAKE_HEAD:
                    Console::writeWithColor("  ", ConsoleColor(RED, RED, true, true));
                    break;
                case Point::Type::SNAKE_BODY:
                    Console::writeWithColor("  ", ConsoleColor(GREEN, GREEN, true, true));
                    break;
                case Point::Type::SNAKE_TAIL:
                    Console::writeWithColor("  ", ConsoleColor(BLUE, BLUE, true, true));
                    break;
                case Point::Type::TEST_VISIT:
                    drawTestPoint(point, ConsoleColor(BLUE, GREEN, true, true));
                    break;
                case Point::Type::TEST_PATH:
                    drawTestPoint(point, ConsoleColor(BLUE, RED, true, true));
                    break;
                default:
                    break;
            }
        }
        Console::write("\n");
    }
}

void GameCtrl::drawTestPoint(const Point &p, const ConsoleColor &consoleColor) const {
    string pointStr = "";
    if (p.getDist() == Point::MAX_VALUE) {
        pointStr = "In";
    } else if (p.getDist() == EMPTY_VALUE) {
        pointStr = "  ";
    } else {
        Point::ValueType dist = p.getDist();
        pointStr = util::toString(p.getDist());
        if (dist / 10 == 0) {
            pointStr.insert(0, " ");
        } 
    }
    Console::writeWithColor(pointStr, consoleColor);
}

void GameCtrl::keyboard() {
    try {
        while (runSubThread) {
            if (Console::kbhit()) {
                switch (Console::getch()) {
					case 'w': 
					case 'W':
                        keyboardMove(snake, Direction::UP);
                        break;
					case 'a': 
					case 'A':
                        keyboardMove(snake, Direction::LEFT);
                        break;
					case 's': 
					case 'S':
                        keyboardMove(snake, Direction::DOWN);
                        break;
					case 'd':
					case 'D':
                        keyboardMove(snake, Direction::RIGHT);
                        break;
                    case ' ':
                        pause = !pause;  // Pause or resume game
                        break;
                    case 27:  // Esc
                        exitGame(MSG_ESC);
                        break;
                    default:
                        break;
                }
            }
            sleepFPS();
        }
    } catch (const std::exception &e) {
        exitGameErr(e.what());
    }
}

void GameCtrl::keyboardMove(Snake &s, const Direction d) {
    if (pause) {
        s.setDirection(d);
        moveSnake();
    } else if (!enableAI) {
        if (s.getDirection() == d) {
            moveSnake();  // Accelerate
        } else {
            s.setDirection(d);
        }
    }
}

void GameCtrl::autoMove() {
    try {
        while (runSubThread) {
            util::sleep(moveInterval);
            if (!pause) {
                if (enableAI) {
                    snake.decideNext();
                }
                moveSnake();
            }
        }
    } catch (const std::exception &e) {
        exitGameErr(e.what());
    }
}

void GameCtrl::test() {
    //testFood();
    testSearch();
    //testHamilton();
}

void GameCtrl::testFood() {
    SizeType cnt = 0;
    while (runMainThread && cnt++ < map->getSize()) {
        map->createRandFood();
        sleepFPS();
    }
    exitGame("testFood() finished.");
}

void GameCtrl::testSearch() {
    if (mapRowCnt != 20 || mapColCnt != 20) {
        throw std::range_error("GameCtrl.testSearch(): Require map size 20*20.");
    }

    list<Direction> path;
    snake.setMap(map);

    // Add walls for testing
    for (int i = 4; i < 16; ++i) {
        map->getPoint(Pos(i, 9)).setType(Point::Type::WALL);   // vertical
        map->getPoint(Pos(4, i)).setType(Point::Type::WALL);   // horizontal #1
        map->getPoint(Pos(15, i)).setType(Point::Type::WALL);  // horizontal #2
    }
   
    Pos from(6, 7), to(14, 13);
    snake.testMinPath(from, to, path);
    //snake.testMaxPath(from, to, path);

    // Print path info
    string info = "Path from " + from.toString() + " to " + to.toString()
        + " of length " + util::toString(path.size()) + ":\n";
    for (const Direction &d : path) {
        switch (d) {
            case LEFT:
                info += "L "; break;
            case UP:
                info += "U "; break;
            case RIGHT:
                info += "R "; break;
            case DOWN:
                info += "D "; break;
            case NONE:
            default:
                info += "NONE "; break;
        }
    }
    info += "\ntestSearch() finished.";
    exitGame(info);
}

void GameCtrl::testHamilton() {
    snake.setMap(map);
    snake.addBody(Pos(1, 3));
    snake.addBody(Pos(1, 2));
    snake.addBody(Pos(1, 1));
    snake.testHamilton();
    exitGame("testHamilton() finished.");
}
