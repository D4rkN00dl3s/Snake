// Windows Version (Uses Windows Console API)
#include <iostream>
#include <deque>
#include <ctime>
#include <conio.h>
#include <windows.h>

using namespace std;

// Global Variables
int rows = 25, cols = 90;
int fx = 0, fy = 0;
int borderWidth = 60;
int borderHeight = 20;
char dir = 'd';
bool run = true;
deque<pair<int, int>> snake;

// Function to get terminal size
void getTerminalSize(int &rows, int &cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
}

// Function to move cursor using Windows API
void moveCursorTo(int row, int col) {
    COORD coord;
    coord.X = col;
    coord.Y = row;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// Function to set console text color
void setTextColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Function to clear the screen
void clearScreen() {
    system("cls");
}

// Read keyboard input (Windows specific)
char getInput() {
    if (_kbhit()) {
        return _getch();
    }
    return '\0';
}

// Food placement
void CreateFood(int top, int left) {
    fx = left + 1 + rand() % (borderWidth - 2);
    fy = top + 1 + rand() % (borderHeight - 2);
    moveCursorTo(fy, fx);
    setTextColor(12); // Red color
    cout << "@";
    setTextColor(7); // Reset to default
}

// Draw game borders
void DrawBorders(int top, int left) {
    moveCursorTo(top - 1, left);
    cout << string(borderWidth, '_');
    moveCursorTo(top + borderHeight, left);
    cout << string(borderWidth, '_');

    for (int i = 0; i <= borderHeight; i++) {
        moveCursorTo(top + i, left);
        cout << "|";
        moveCursorTo(top + i, left + borderWidth);
        cout << "|";
    }
}

// Draw snake
void DrawSnake() {
    for (const auto &part : snake) {
        moveCursorTo(part.first, part.second);
        setTextColor(10); // Green color
        cout << "S";
    }
    setTextColor(7); // Reset color
}

// Update snake position
void UpdateSnake(int top, int left) {
    static int dx[] = {-1, 1, 0, 0};
    static int dy[] = {0, 0, -1, 1};
    static char dirs[] = {'w', 's', 'a', 'd'};

    int dirIndex = 0;
    for (; dirIndex < 4; ++dirIndex)
        if (dir == dirs[dirIndex]) break;

    int currRow = snake.front().first + dx[dirIndex];
    int currCol = snake.front().second + dy[dirIndex];

    if (currRow < top || currRow >= top + borderHeight ||
        currCol <= left || currCol >= left + borderWidth) {
        run = false;
        return;
    }

    snake.push_front({currRow, currCol});
    if (currRow == fy && currCol == fx) {
        CreateFood(top, left);
    } else {
        moveCursorTo(snake.back().first, snake.back().second);
        cout << " ";
        snake.pop_back();
    }
    DrawSnake();
}

int main() {
    clearScreen();
    getTerminalSize(rows, cols);

    int top = (rows - borderHeight) / 2;
    int left = (cols - borderWidth) / 2;
    int midRow = rows / 2;
    int midCol = cols / 2;

    DrawBorders(top, left);

    snake.push_back({midRow, midCol});
    moveCursorTo(midRow, midCol);
    cout << "S";

    srand(static_cast<unsigned int>(time(0)));
    CreateFood(top, left);

    while (run) {
        char ch = getInput();
        if (ch) {
            switch (ch) {
                case 'd': if (dir != 'a') dir = 'd'; break;
                case 'a': if (dir != 'd') dir = 'a'; break;
                case 'w': if (dir != 's') dir = 'w'; break;
                case 's': if (dir != 'w') dir = 's'; break;
                case 'q': run = false; break;
            }
        }
        UpdateSnake(top, left);
        Sleep(150);
    }
    return 0;
}
