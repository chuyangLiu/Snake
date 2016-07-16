#include "GameCtrl.h"

int main() {
    auto game = GameCtrl::getInstance();

    // Set map's size. Default is 20*20
    // The minimum size is 4*4.
    game->setMapRow(9);
    game->setMapColumn(9);

    // Set FPS. Default is 60.0
    game->setFPS(59.0);

    // Set whether to make the snake automove. Default is true
    game->setAutoMoveSnake(true);

    // Set interval time(ms) for automove. Default is 200 ms.
    // If setAutoMoveSnake(false), this code is useless.
    game->setAutoMoveInterval(50);

    // Set whether to enable the second snake. Default is false
    game->setEnableSecondSnake(false);

    // Set whether to enable the snake AI. Default is false
    // If setAutoMoveSnake(false), this code is useless.
    game->setEnableAI(true);

    // Set whether to run the test program. Default is false
    game->setRunTest(false);

    // Set whether to write the map content to the file. Default is false
    // If set this attribute to true, the game map content will be written
    // to a file named "movements.txt" after each snake's movement.
    // PS: This is designed for debugging. Open this method may make the
    // snake move slower.
    game->setWriteToFile(false);

    return game->run();
}
