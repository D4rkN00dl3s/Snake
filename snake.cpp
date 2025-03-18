#include <iostream>
#include <cstdlib>
#include <unistd.h>    // For read()
#include <termios.h>   // For setting terminal attributes
#include <csignal>

using namespace std;

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
#include "snake.h"
#endif

void getTerminalSize(int &rows, int &cols) {
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }
    #else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            cols = w.ws_col;
            rows = w.ws_row;
        }
    #endif
}

struct termios original_termios;

void restoreTerminalSettings() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &original_termios);  // Get current settings
    struct termios raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode & echo
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);  // Apply changes

    // Ensure restoration on exit
    atexit(restoreTerminalSettings);
}

void moveCursorTo(int row, int col) {
    cout << "\033[" << row << ";" << col << "H";
}

void CreateFoodPosition(int row, int col, int &fx, int &fy){
    fx = rand() % col;
    fy = rand() % row;
}

void CreateFood (int fx, int fy){
    moveCursorTo(fy, fx);
    cout << "F" << endl;
}

void UpdateLastPosition(int &lastRow, int &lastCol, int currRow, int currCol){
    lastRow = currRow;
    lastCol = currCol;
}

void UpdateSnake(int lastRow, int lastCol, int currRow, int currCol)
{
    moveCursorTo(lastRow, lastCol);
    cout << " " << flush;
    moveCursorTo(currRow, currCol);
    cout << "S" << flush;
}

int main() {
    system("clear");
    enableRawMode();

    int rows = 0, cols = 0;
    getTerminalSize(rows, cols);

    // Global Variables
    int midRow = rows / 2;
    int midCol = cols / 2;
    int currRow = midRow, currCol = midCol;
    int lastRow, lastCol;
    int fx = 0, fy = 0;
    char ch;
    bool run = true;

    //Snake Starting Point
    moveCursorTo(midRow, midCol);
    cout << "S" << endl;

    srand(static_cast<unsigned int>(time(0)));

    //Create First Food
    CreateFoodPosition(rows, cols, fx, fy);
    CreateFood(fx, fy);

    //Main Game Loop
    while (run) {
        if (read(STDIN_FILENO, &ch, 1) > 0) {  // Read key press
            UpdateLastPosition(lastRow, lastCol, currRow, currCol);
            switch (ch)
            {
            case 'd':
                currCol++;
                break;
            case 'a':
                currCol--;
                break;
            case 'w':
                currRow--;
                break;
            case 's':
                currRow++;
                break;
            case 'q':
                run = false;
                break;
            default:
                break;
            }
            UpdateSnake(lastRow, lastCol, currRow, currCol);
        }
    }

    system("clear");
    cout << "Terminating program" << endl;
    return 0;
}