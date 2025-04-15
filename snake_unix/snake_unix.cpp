#include <iostream>
#include <vector>
#include <unordered_set>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace std;

const int MAX_SNAKE_LENGTH = 1000;
int rows = 0, cols = 0;
int fx = 0, fy = 0;
int borderWidth = 60;
int borderHeight = 20;
unsigned int score = 0;
enum class Direction { UP, DOWN, LEFT, RIGHT };
Direction dir = Direction::RIGHT; // initial direction
bool run = true;

pair<int, int> snakeBuffer[MAX_SNAKE_LENGTH];
int head = 0, tail = 0, snakeSize = 0;
struct pair_hash {
    size_t operator()(const pair<int, int>& p) const {
        return hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
    }
};

unordered_set<pair<int, int>, pair_hash> snakeBody;

int mod(int x) {
    return (x + MAX_SNAKE_LENGTH) % MAX_SNAKE_LENGTH;
}

void push_front(pair<int, int> pos) {
    head = mod(head - 1);
    snakeBuffer[head] = pos;
    snakeSize++;
    snakeBody.insert(pos);
}

void pop_back() {
    tail = mod(tail - 1);
    snakeBody.erase(snakeBuffer[tail]);
    snakeSize--;
}

pair<int, int> get_front() {
    return snakeBuffer[head];
}

pair<int, int> get_back() {
    return snakeBuffer[mod(tail - 1)];
}

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
    printf("\033[?25h");
    fflush(stdout);
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
    do {
        fx = left + 1 + rand() % (borderWidth - 2);
        fy = top + 1 + rand() % (borderHeight - 2);
    } while (snakeBody.count({fy, fx}) > 0);

    moveCursorTo(fy, fx);
    printf("\033[31m@\033[0m");
}

void DrawSnake() {
    for (int i = 0; i < snakeSize; ++i) {
        auto pos = snakeBuffer[mod(head + i)];
        moveCursorTo(pos.first, pos.second);
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

void pauseMenu() {
    clearTerminal();
    moveCursorTo(rows / 2 - 1, cols / 2 - 10); cout << "=== GAME PAUSED ===";
    moveCursorTo(rows / 2, cols / 2 - 10);     cout << "1. Continue";
    moveCursorTo(rows / 2 + 1, cols / 2 - 10); cout << "2. Settings";
    moveCursorTo(rows / 2 + 2, cols / 2 - 10); cout << "3. Exit";
    cout.flush();

    while (true) {
        char ch = getInput();
        if (ch == '1') {
            clearTerminal();
            DrawBorders((rows - borderHeight) / 2, (cols - borderWidth) / 2);
            DrawSnake();
            moveCursorTo(fy, fx);
            printf("\033[31m@\033[0m");
            fflush(stdout);
            break; // Continue game
        } else if (ch == '2') {
            clearTerminal();
            moveCursorTo(rows / 2, cols / 2 - 10);
            cout << "[Settings not implemented yet. Press any key to go back]";
            cout.flush();
            while (getInput() == '\0') usleep(10000);
            return pauseMenu(); // Go back to pause menu
        } else if (ch == '3') {
            run = false;
            break; // Exit game
        }
        usleep(10000); // Small delay to prevent CPU overuse
    }
}

void handleInput(char ch) {
    if (ch == '\033') { // ESC key
        pauseMenu();
        return;
    }

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
        case Direction::UP:    dx = -1; break;
        case Direction::DOWN:  dx = 1;  break;
        case Direction::LEFT:  dy = -1; break;
        case Direction::RIGHT: dy = 1;  break;
    }

    pair<int, int> currentHead = get_front();
    int newRow = currentHead.first + dx;
    int newCol = currentHead.second + dy;
    pair<int, int> newHead = {newRow, newCol};

    // Collision
    if (newRow < top || newRow >= top + borderHeight ||
        newCol <= left || newCol >= left + borderWidth ||
        snakeBody.count(newHead)) {
        run = false;
        return;
    }

    // Move
    push_front(newHead);

    if (newRow == fy && newCol == fx) {
        score++;
        CreateFood(top, left);
    } else {
        moveCursorTo(get_back().first, get_back().second);
        cout << " ";
        pop_back();
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
    getTerminalSize(rows, cols);
    top = (rows - borderHeight) / 2;
    left = (cols - borderWidth) / 2;

    DrawBorders(top, left);

    int midRow = rows / 2;
    int midCol = cols / 2;
    pair<int, int> start = {midRow, midCol};

    push_front(start);
    moveCursorTo(midRow, midCol); cout << "S" << endl;

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