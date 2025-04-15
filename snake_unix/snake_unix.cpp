#include <iostream>
#include <deque>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace std;

int rows = 0, cols = 0;
int fx = 0, fy = 0;
int borderWidth = 60;
int borderHeight = 20;
unsigned int score = 0;
enum class Direction { UP, DOWN, LEFT, RIGHT };
Direction dir = Direction::RIGHT; // initial direction
bool run = true;
deque<pair<int, int>> snake;

struct termios original_termios;


void clearTerminal() {
    printf("\033[H\033[J");
}

void moveCursorTo(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

void hideCursor() {
    printf("\033[?25l");
    fflush(stdout);
}

void getTerminalSize(int &rows, int &cols) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        cols = w.ws_col;
        rows = w.ws_row;
    }
}

void restoreTerminalSettings() {
    clearTerminal();
    cout << "Terminating program" << endl;
    cout << "Your final score => " << score << endl;
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restoreTerminalSettings);
}

void setNonBlockingInput() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

char getInput() {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
        return ch;
    }
    return '\0';
}

void DrawBorders(int top, int left) {
    string border(static_cast<size_t>(borderWidth), '_');
    moveCursorTo(top - 1, left); cout << border;
    moveCursorTo(top + borderHeight, left); cout << border;

    for (int i = 0; i <= borderHeight; i++) {
        moveCursorTo(top + i, left); cout << "|";
        moveCursorTo(top + i, left + borderWidth); cout << "|";
    }
    cout.flush();
}

void CreateFood(int top, int left) {
    fx = left + 1 + rand() % (borderWidth - 2);
    fy = top + 1 + rand() % (borderHeight - 2);
    moveCursorTo(fy, fx);
    printf("\033[31m@\033[0m");
}

void DrawSnake() {
    for (const auto &part : snake) {
        moveCursorTo(part.first, part.second);
        printf("\033[32mS\033[0m");
    }
    cout.flush();
}

Direction charToDirection(char ch) {
    switch (ch) {
        case 'w': return Direction::UP;
        case 's': return Direction::DOWN;
        case 'a': return Direction::LEFT;
        case 'd': return Direction::RIGHT;
        default:  return dir; // unchanged
    }
}

void handleInput(char ch) {
    Direction newDir = charToDirection(ch);
    switch (newDir) {
        case Direction::UP:
            if (dir != Direction::DOWN)
                dir = Direction::UP;
            break;
        case Direction::DOWN:
            if (dir != Direction::UP)
                dir = Direction::DOWN;
            break;
        case Direction::LEFT:
            if (dir != Direction::RIGHT)
                dir = Direction::LEFT;
            break;
        case Direction::RIGHT:
            if (dir != Direction::LEFT)
                dir = Direction::RIGHT;
            break;
    }

    if (ch == 'q') run = false;
}

void UpdateSnake(int top, int left) {
    int dx = 0, dy = 0;
    switch (dir) {
        case Direction::UP: dx = -1; break;
        case Direction::DOWN: dx = 1; break;
        case Direction::LEFT: dy = -1; break;
        case Direction::RIGHT: dy = 1; break;
    }

    int newRow = snake.front().first + dx;
    int newCol = snake.front().second + dy;

    // Border collision check
    if (newRow < top || newRow >= top + borderHeight || newCol <= left || newCol >= left + borderWidth) {
        run = false;
        return;
    }

    // Self-collision check
    for (const auto &segment : snake) {
        if (segment.first == newRow && segment.second == newCol) {
            run = false;
            return;
        }
    }

    snake.push_front({newRow, newCol});

    if (newRow == fy && newCol == fx) {
        score++;
        CreateFood(top, left);
    } else {
        moveCursorTo(snake.back().first, snake.back().second);
        cout << " ";
        snake.pop_back();
    }

    DrawSnake();
}

void initializeTerminal() {
    clearTerminal();
    enableRawMode();
    setNonBlockingInput();
    hideCursor();
    getTerminalSize(rows, cols);
}

void initializeGame(int &top, int &left) {
    top = (rows - borderHeight) / 2;
    left = (cols - borderWidth) / 2;
    DrawBorders(top, left);

    int midRow = rows / 2;
    int midCol = cols / 2;
    snake.push_back({midRow, midCol});
    moveCursorTo(midRow, midCol);
    cout << "S" << endl;

    srand(static_cast<unsigned int>(time(0)));
    CreateFood(top, left);
}

void gameLoop(int top, int left) {
    while (run) {
        char ch = getInput();
        if (ch) handleInput(ch);
        UpdateSnake(top, left);
        usleep(150000);
    }
}

int main() {
    initializeTerminal();

    int top, left;
    initializeGame(top, left);

    gameLoop(top, left);

    return 0;
}