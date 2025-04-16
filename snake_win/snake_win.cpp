// WINDOWS VERSION WITH LINUX FEATURES
#include <iostream>
#include <vector>
#include <unordered_set>
#include <windows.h>
#include <conio.h>
#include <chrono>
#include <ctime>
#include <algorithm>

#define FOREGROUND_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

using namespace std;
using namespace chrono;

const int MAX_SNAKE_LENGTH = 1000;
int rows = 25, cols = 80;
int borderWidth = 60, borderHeight = 20;
int top = 0, left = 0;
unsigned int score = 0, highScore = 0;
enum class Direction { UP, DOWN, LEFT, RIGHT };
Direction dir = Direction::RIGHT;
bool run = true, paused = false, playerLost = false;

int snakeSpeed = 150;
int speedLevel = 0;
int foodCount = 1;
int head = 0, tail = 0, snakeSize = 0;
pair<int, int> snakeBuffer[MAX_SNAKE_LENGTH];
vector<pair<int, int>> foodPositions;
vector<char> foodSymbols = {'@', '*', '#', '$', '%'};
WORD snakeColor = FOREGROUND_GREEN;
WORD foodColor = FOREGROUND_RED;

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
steady_clock::time_point gameStart;
steady_clock::time_point pauseStart;
steady_clock::duration totalPausedTime = seconds(0);

struct pair_hash {
    size_t operator()(const pair<int, int> &p) const {
        return hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
    }
};
unordered_set<pair<int, int>, pair_hash> snakeBody;

int mod(int x) { return (x + MAX_SNAKE_LENGTH) % MAX_SNAKE_LENGTH; }

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
pair<int, int> get_front() { return snakeBuffer[head]; }
pair<int, int> get_back() { return snakeBuffer[mod(tail - 1)]; }

void moveCursorTo(int row, int col) {
    COORD coord = {(SHORT)col, (SHORT)row};
    SetConsoleCursorPosition(hConsole, coord);
}
void clearScreen() {
    system("cls");
}
void setTextColor(WORD color) {
    SetConsoleTextAttribute(hConsole, color);
}

void DrawBorders() {
    for (int i = 0; i <= borderHeight; i++) {
        moveCursorTo(top + i, left);
        cout << "|";
        moveCursorTo(top + i, left + borderWidth);
        cout << "|";
    }
    moveCursorTo(top - 1, left);
    for (int i = 0; i <= borderWidth; i++) cout << "_";
    moveCursorTo(top + borderHeight, left);
    for (int i = 0; i <= borderWidth; i++) cout << "_";
}

void drawSidebar() {
    moveCursorTo(top, 2);
    setTextColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    cout << "=== INFO ===";

    moveCursorTo(top + 2, 2);
    setTextColor(FOREGROUND_WHITE);
    cout << "Score: " << score;

    moveCursorTo(top + 3, 2);
    cout << "High: " << highScore;

    auto now = steady_clock::now();
    auto playTime = duration_cast<seconds>(now - gameStart - totalPausedTime);
    int minutes = playTime.count() / 60;
    int seconds = playTime.count() % 60;

    moveCursorTo(top + 5, 2);
    printf("Time: %02d:%02d", minutes, seconds);

    if (paused) {
        moveCursorTo(top + 7, 2);
        setTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
        cout << "Paused";
    }

    setTextColor(FOREGROUND_WHITE);
}

void CreateFood() {
    int toSpawn = foodCount - static_cast<int>(foodPositions.size());
    int attempts = 0, maxAttempts = 500;

    while (toSpawn > 0 && attempts++ < maxAttempts) {
        int fx = left + 1 + rand() % (borderWidth - 2);
        int fy = top + 1 + rand() % (borderHeight - 2);
        pair<int, int> food = {fy, fx};

        if (snakeBody.count(food) == 0 &&
            find(foodPositions.begin(), foodPositions.end(), food) == foodPositions.end()) {
            foodPositions.push_back(food);
            moveCursorTo(fy, fx);
            setTextColor(foodColor);
            cout << foodSymbols[rand() % foodSymbols.size()];
            setTextColor(FOREGROUND_WHITE);
            toSpawn--;
        }
    }
}

void DrawSnake() {
    for (int i = 0; i < snakeSize; ++i) {
        auto pos = snakeBuffer[mod(head + i)];
        moveCursorTo(pos.first, pos.second);
        setTextColor(snakeColor);
        cout << "S";
    }
    setTextColor(FOREGROUND_WHITE);
}

Direction charToDirection(char ch) {
    switch (ch) {
        case 'w': return Direction::UP;
        case 's': return Direction::DOWN;
        case 'a': return Direction::LEFT;
        case 'd': return Direction::RIGHT;
        default: return dir;
    }
}

void handleInput() {
    if (_kbhit()) {
        char ch = _getch();
        if (ch == 'p') {
            paused = !paused;
            if (paused)
                pauseStart = steady_clock::now();
            else
                totalPausedTime += steady_clock::now() - pauseStart;
        }
        Direction newDir = charToDirection(ch);
        if (!paused) {
            switch (newDir) {
                case Direction::UP: if (dir != Direction::DOWN) dir = Direction::UP; break;
                case Direction::DOWN: if (dir != Direction::UP) dir = Direction::DOWN; break;
                case Direction::LEFT: if (dir != Direction::RIGHT) dir = Direction::LEFT; break;
                case Direction::RIGHT: if (dir != Direction::LEFT) dir = Direction::RIGHT; break;
            }
        }
        if (ch == 27 || ch == 'q') run = false;
    }
}

void UpdateSnake() {
    static int moveCounter = 0;
    if (++moveCounter % 50 == 0 && snakeSpeed > 50) {
        snakeSpeed -= 10;
    }

    int dx = 0, dy = 0;
    switch (dir) {
        case Direction::UP: dx = -1; break;
        case Direction::DOWN: dx = 1; break;
        case Direction::LEFT: dy = -1; break;
        case Direction::RIGHT: dy = 1; break;
    }

    pair<int, int> currentHead = get_front();
    int newRow = currentHead.first + dx;
    int newCol = currentHead.second + dy;
    pair<int, int> newHead = {newRow, newCol};

    if (newRow < top || newRow >= top + borderHeight ||
        newCol <= left || newCol >= left + borderWidth ||
        snakeBody.count(newHead)) {
        playerLost = true;
        run = false;
        return;
    }

    bool ate = false;
    for (size_t i = 0; i < foodPositions.size(); ++i) {
        if (newRow == foodPositions[i].first && newCol == foodPositions[i].second) {
            score++;
            ate = true;
            foodPositions.erase(foodPositions.begin() + i);
            break;
        }
    }

    push_front(newHead);

    if (!ate) {
        moveCursorTo(get_back().first, get_back().second);
        cout << " ";
        pop_back();
    } else {
        CreateFood();
    }

    DrawSnake();
}

bool gameOverScreen() {
    clearScreen();
    int centerRow = rows / 2;
    int centerCol = cols / 2 - 10;

    moveCursorTo(centerRow - 1, centerCol);
    setTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
    cout << "=== GAME OVER ===";

    moveCursorTo(centerRow, centerCol);
    setTextColor(FOREGROUND_WHITE);
    cout << "Score: " << score;

    moveCursorTo(centerRow + 1, centerCol);
    cout << "High Score: " << highScore;

    moveCursorTo(centerRow + 3, centerCol);
    cout << "1. Restart  2. Exit";
    setTextColor(FOREGROUND_WHITE);

    while (true) {
        if (_kbhit()) {
            char ch = _getch();
            if (ch == '1') {
                run = true;
                score = 0;
                snakeSpeed = 150;
                dir = Direction::RIGHT;
                foodPositions.clear();
                head = tail = snakeSize = 0;
                snakeBody.clear();
                clearScreen();
                return true;
            } else if (ch == '2') {
                return false;
            }
        }
        Sleep(50);
    }
}

void initializeGame() {
    clearScreen();
    top = (rows - borderHeight) / 2;
    left = (cols - borderWidth) / 2;
    DrawBorders();

    int midRow = top + borderHeight / 2;
    int midCol = left + borderWidth / 2;
    pair<int, int> start = {midRow, midCol};
    push_front(start);

    moveCursorTo(midRow, midCol);
    setTextColor(snakeColor);
    cout << "S";
    setTextColor(FOREGROUND_WHITE);

    srand((unsigned int)time(0));
    CreateFood();

    gameStart = steady_clock::now();
    totalPausedTime = seconds(0);
    paused = false;
}

void gameLoop() {
    while (run) {
        handleInput();
        if (!paused) {
            UpdateSnake();
            drawSidebar();
        }
        Sleep(snakeSpeed);
    }
}

int main() {
    while (true) {
        initializeGame();
        gameLoop();

        if (score > highScore) highScore = score;

        if (playerLost) {
            if (!gameOverScreen()) break;
            playerLost = false;
        } else break;
    }

    return 0;
}