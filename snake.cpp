#include <iostream>
#include <cstdlib>
#include <unistd.h>    // For read()
#include <deque>
#include <utility>
#include <termios.h>   // For setting terminal attributes
#include <csignal>
#include <fcntl.h>

using namespace std;

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #include <unistd.h>
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
    system("clear");
    cout << "Terminating program" << endl;
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

void setNonBlockingInput() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void moveCursorTo(int row, int col) {
    cout << "\033[" << row << ";" << col << "H";
}

// Global Variables
int rows = 0, cols = 0;
int fx = 0, fy = 0;
char dir = 'd';
bool run = true;
deque<pair<int, int>> snake;

void CreateFoodPosition(int row, int col){
    fx = rand() % col;
    fy = rand() % row;
}

void CreateFood (){
    moveCursorTo(fy, fx);
    cout << "@" << flush;
}

void DrawBorders(int row, int col){
    int width = 60;
    int height = 20;

    int top = (row - height) / 2;  // Center vertically
    int left = (col - width) / 2;  // Center horizontally

    // Draw top border
    moveCursorTo(top-1, left);
    for (int i = 0; i < width; i++) {
        cout << "_" << flush;
    }

    // Draw bottom border
    moveCursorTo(top + height, left);
    for (int i = 0; i < width; i++) {
        cout << "_" << flush;
    }

    // Draw left and right borders
    for (int i = 0; i <= height; i++) {
        moveCursorTo(top + i, left);
        cout << "|" << flush;
        moveCursorTo(top + i, left + width);
        cout << "|" << flush;
    }
}

void DrawSnake() {
    for (const auto &part : snake) {
        moveCursorTo(part.first, part.second);
        cout << "S";
    }
    cout.flush();
}

void UpdateSnake() {
    int currRow = snake.front().first;
    int currCol = snake.front().second;

    switch (dir) {
        case 'd': currCol++; break;
        case 'a': currCol--; break;
        case 'w': currRow--; break;
        case 's': currRow++; break;
    }

    // Add new head to the snake
    snake.push_front({currRow, currCol});

    // Check if food was eaten
    if (currRow == fy && currCol == fx) {
        // Create new food
        CreateFoodPosition(rows, cols);
        CreateFood();
    } else {
        // Remove the last tail position (shrink)
        pair<int, int> tail = snake.back();
        snake.pop_back();
        moveCursorTo(tail.first, tail.second);
        cout << " ";  // Erase last part
    }
    DrawSnake();
}

int main() {
    system("clear");
    enableRawMode();
    setNonBlockingInput();

    getTerminalSize(rows, cols);

    int midRow = rows / 2;
    int midCol = cols / 2;

    DrawBorders(rows, cols);

    snake.push_back({midRow, midCol});

    //Snake Starting Point
    moveCursorTo(midRow, midCol);
    cout << "S" << endl;

    srand(static_cast<unsigned int>(time(0)));

    //Create First Food
    CreateFoodPosition(rows, cols);
    CreateFood();

    //Main Game Loop
    while (run) {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) {  // Read key press
            switch (ch) {
                case 'd': if(dir != 'a') dir = 'd'; break;
                case 'a': if(dir != 'd') dir = 'a'; break;
                case 'w': if(dir != 's') dir = 'w'; break;
                case 's': if(dir != 'w') dir = 's'; break;
                case 'q': run = false; break;
            }
        }

        UpdateSnake();

        usleep(150000); // Delay for movement speed 150ms
    }

    return 0;
}