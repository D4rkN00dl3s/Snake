#include <iostream>
#include <vector>
#include <unordered_set>
#include <windows.h>
#include <conio.h>
#include <chrono>
#include <ctime>
#include <algorithm>

#define FOREGROUND_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_YELLOW (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN)
#define FOREGROUND_CYAN (FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_MAGENTA (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE)

using namespace std;

const int MAX_SNAKE_LENGTH = 1000;
int rows = 0, cols = 0;
int borderWidth = 60;
int borderHeight = 20;
unsigned int score = 0;
enum class Direction
{
    UP,
    DOWN,
    LEFT,
    RIGHT
};
Direction dir = Direction::RIGHT;
bool run = true;
bool playerLost = false;

int screenSwapDelayShort = 50;
int screenSwapDelayLong = 1000;

int snakeSpeed = 150;
int foodCount = 1;
int head = 0, tail = 0, snakeSize = 0;
pair<int, int> snakeBuffer[MAX_SNAKE_LENGTH];
vector<pair<int, int>> foodPositions;
WORD snakeColor = FOREGROUND_GREEN;
WORD foodColor = FOREGROUND_RED;

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
chrono::steady_clock::time_point gameStart;
chrono::steady_clock::time_point pauseStart;
chrono::steady_clock::duration totalPausedTime = chrono::seconds(0);

struct pair_hash
{
    size_t operator()(const pair<int, int> &p) const
    {
        return hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
    }
};
unordered_set<pair<int, int>, pair_hash> snakeBody;

int mod(int x) { return (x + MAX_SNAKE_LENGTH) % MAX_SNAKE_LENGTH; }

void push_front(pair<int, int> pos)
{
    head = mod(head - 1);
    snakeBuffer[head] = pos;
    snakeSize++;
    snakeBody.insert(pos);
}

void pop_back()
{
    tail = mod(tail - 1);
    snakeBody.erase(snakeBuffer[tail]);
    snakeSize--;
}

pair<int, int> get_front() { return snakeBuffer[head]; }
pair<int, int> get_back() { return snakeBuffer[mod(tail - 1)]; }

void moveCursorTo(int row, int col)
{
    COORD coord = {(SHORT)col, (SHORT)row};
    SetConsoleCursorPosition(hConsole, coord);
}

void clearScreen()
{
    system("cls");
}

void hideCursor()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;

    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE; // hide
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

void getTerminalSize(int &rows, int &cols)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
}

void setTextColor(WORD color)
{
    SetConsoleTextAttribute(hConsole, color);
}

void resizeConsoleWindow(int width, int height)
{
    HWND console = GetConsoleWindow();
    RECT r;
    GetWindowRect(console, &r); // get current window size

    // Resize window
    MoveWindow(console, r.left, r.top, width, height, TRUE);
}

void DrawBorders(int borderTop, int borderLeft)
{
    string border(static_cast<size_t>(borderWidth), '_');
    moveCursorTo(borderTop - 1, borderLeft);
    cout << border;
    moveCursorTo(borderTop + borderHeight, borderLeft);
    cout << border;

    for (int i = 0; i <= borderHeight; i++)
    {
        moveCursorTo(borderTop + i, borderLeft);
        cout << "|";
        moveCursorTo(borderTop + i, borderLeft + borderWidth);
        cout << "|";
    }
    cout.flush();
}

void drawSidebar(int borderTop, int borderLeft)
{
    moveCursorTo(borderTop, 2);
    setTextColor(FOREGROUND_INTENSITY | FOREGROUND_BLUE);
    cout << "=== INFO ===";

    moveCursorTo(borderTop + 2, 2);
    setTextColor(FOREGROUND_WHITE);
    cout << "Score: " << score;

    auto now = chrono::steady_clock::now();
    auto playTime = chrono::duration_cast<chrono::seconds>(now - gameStart - totalPausedTime);
    int minutes = playTime.count() / 60;
    int seconds = playTime.count() % 60;

    moveCursorTo(borderTop + 4, 2);
    setTextColor(FOREGROUND_WHITE);
    printf("Time: %02d:%02d", minutes, seconds);

    cout.flush();
}

void CreateFood(int borderTop, int borderLeft)
{
    int toSpawn = foodCount - static_cast<int>(foodPositions.size());
    int attempts = 0, maxAttempts = 500;

    while (toSpawn > 0 && attempts++ < maxAttempts)
    {
        int fx = borderLeft + 1 + rand() % (borderWidth - 2);
        int fy = borderTop + 1 + rand() % (borderHeight - 2);
        pair<int, int> food = {fy, fx};

        if (snakeBody.count(food) == 0 &&
            find(foodPositions.begin(), foodPositions.end(), food) == foodPositions.end())
        {
            foodPositions.push_back(food);
            moveCursorTo(fy, fx);
            setTextColor(foodColor);
            cout << "@";
            setTextColor(FOREGROUND_WHITE);
            toSpawn--;
        }
    }
}

void DrawSnake()
{
    for (int i = 0; i < snakeSize; ++i)
    {
        auto pos = snakeBuffer[mod(head + i)];
        moveCursorTo(pos.first, pos.second);
        setTextColor(snakeColor);
        cout << "S";
    }
    setTextColor(FOREGROUND_WHITE);
}

Direction charToDirection(char ch)
{
    switch (ch)
    {
    case 'w':
        return Direction::UP;
    case 's':
        return Direction::DOWN;
    case 'a':
        return Direction::LEFT;
    case 'd':
        return Direction::RIGHT;
    default:
        return dir;
    }
}

void UpdateSnake(int borderTop, int borderLeft)
{
    int dx = 0, dy = 0;
    switch (dir)
    {
    case Direction::UP:
        dx = -1;
        break;
    case Direction::DOWN:
        dx = 1;
        break;
    case Direction::LEFT:
        dy = -1;
        break;
    case Direction::RIGHT:
        dy = 1;
        break;
    }

    pair<int, int> currentHead = get_front();
    int newRow = currentHead.first + dx;
    int newCol = currentHead.second + dy;
    pair<int, int> newHead = {newRow, newCol};

    if (newRow < borderTop ||
        newRow >= borderTop + borderHeight ||
        newCol <= borderLeft ||
        newCol >= borderLeft + borderWidth ||
        snakeBody.count(newHead))
    {
        playerLost = true;
        run = false;
        return;
    }

    bool ate = false;
    for (size_t i = 0; i < foodPositions.size(); ++i)
    {
        if (newRow == foodPositions[i].first && newCol == foodPositions[i].second)
        {
            score++;
            ate = true;
            foodPositions.erase(foodPositions.begin() + i);
            break;
        }
    }

    push_front(newHead);

    if (!ate)
    {
        moveCursorTo(get_back().first, get_back().second);
        cout << " ";
        pop_back();
    }
    else
    {
        CreateFood(borderTop, borderLeft);
    }

    DrawSnake();
}

bool gameOverScreen()
{
    clearScreen();
    int centerRow = rows / 2;
    int centerCol = cols / 2 - 10;

    moveCursorTo(centerRow - 1, centerCol);
    setTextColor(FOREGROUND_RED | FOREGROUND_INTENSITY);
    cout << "=== GAME OVER ===";

    moveCursorTo(centerRow, centerCol);
    setTextColor(FOREGROUND_WHITE);
    cout << "Your final score: " << score;

    moveCursorTo(centerRow + 2, centerCol);
    setTextColor(FOREGROUND_WHITE);
    cout << "1. Restart";

    moveCursorTo(centerRow + 3, centerCol);
    setTextColor(FOREGROUND_WHITE);
    cout << "2. Exit Game";

    while (true)
    {
        char ch = _getch();
        if (ch == '1')
        {
            run = true;
            score = 0;
            dir = Direction::RIGHT;
            foodPositions.clear();
            head = tail = snakeSize = 0;
            snakeBody.clear();
            clearScreen();
            return true;
        }
        else if (ch == '2')
        {
            return false;
        }
        Sleep(screenSwapDelayShort));
    }
}

void changeSnakeSpeed()
{
    clearScreen();
    moveCursorTo(rows / 2, cols / 2 - 20);
    setTextColor(FOREGROUND_WHITE);
    cout << "Choose your speed level (1-4, 1 = slowest, 4 = fastest)): ";
    cout.flush();
    bool correctInput = false;

    while (correctInput == false)
    {
        char ch = _getch();

        switch (ch)
        {
        case '1':
            snakeSpeed = 500000;
            correctInput = true;
            break;
        case '2':
            snakeSpeed = 250000;
            correctInput = true;
            break;
        case '3':
            snakeSpeed = 100000;
            correctInput = true;
            break;
        case '4':
            snakeSpeed = 50000;
            correctInput = true;
            break;
        default:
            break;
        }

        if (!correctInput)
        {
            setTextColor(FOREGROUND_WHITE);
            cout << "Wrong Input! Try again!";
        }
    }
    moveCursorTo(rows / 2 + 1, cols / 2 - 10);
    setTextColor(FOREGROUND_WHITE);
    cout << "Speed updated to " << snakeSpeed << " ms!";
    cout.flush();
    Sleep(screenSwapDelayLong);
    clearScreen();
}

void settingsMenu()
{
    while (true)
    {
        clearScreen();

        moveCursorTo(rows / 2 - 2, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "=== SETTINGS ===";

        moveCursorTo(rows / 2 - 1, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "1. Snake Speed";

        moveCursorTo(rows / 2, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "2. Snake Color";

        moveCursorTo(rows / 2 + 1, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "3. Food Color";

        moveCursorTo(rows / 2 + 2, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "4. Food Amount (current: " << foodCount << ")";

        moveCursorTo(rows / 2 + 3, cols / 2 - 10);
        setTextColor(FOREGROUND_WHITE);
        cout << "5. Back to Pause Menu";
        cout.flush();

        char ch = _getch();

        if (ch == 27) // ESC key
            return;

        if (ch == '1')
        {
            changeSnakeSpeed();
        }
        else if (ch == '2')
        {
            clearScreen();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                setTextColor(FOREGROUND_WHITE);
                cout << "Choose Snake Color: 1=Green 2=Yellow 3=Cyan: ";
                cout.flush();
                c = _getch();
                Sleep(10);
            } while (c != '1' && c != '2' && c != '3');

            if (c == '1')
                snakeColor = FOREGROUND_GREEN;
            else if (c == '2')
                snakeColor = FOREGROUND_YELLOW;
            else if (c == '3')
                snakeColor = FOREGROUND_CYAN;
            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            setTextColor(FOREGROUND_WHITE);
            cout << "Color changed!";
            cout.flush();
            Sleep(screenSwapDelayLong);
            clearScreen();
        }
        else if (ch == '3')
        {
            clearScreen();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                setTextColor(FOREGROUND_WHITE);
                cout << "Choose Food Color: 1=Red 2=Magenta 3=Blue: ";
                cout.flush();
                c = _getch();
                Sleep(10);
            } while (c != '1' && c != '2' && c != '3');

            if (c == '1')
                foodColor = FOREGROUND_RED;
            else if (c == '2')
                foodColor = FOREGROUND_MAGENTA;
            else if (c == '3')
                foodColor = FOREGROUND_BLUE;

            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            setTextColor(FOREGROUND_WHITE);
            cout << "Color changed!";
            cout.flush();
            Sleep(screenSwapDelayLong);
            clearScreen();
        }
        else if (ch == '4')
        {
            clearScreen();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                setTextColor(FOREGROUND_WHITE);
                cout << "Enter food amount (1-3): ";
                cout.flush();
                c = _getch();
                Sleep(10);
            } while (c != '1' && c != '2' && c != '3');
            foodCount = c - '0';

            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            setTextColor(FOREGROUND_WHITE);
            cout << "Food count updated!";
            cout.flush();
            Sleep(screenSwapDelayLong);
            clearScreen();
        }
        else if (ch == '5')
            break;
        Sleep(screenSwapDelayShort);
    }
}

void pauseMenu()
{
    pauseStart = chrono::steady_clock::now();

    clearScreen();
    moveCursorTo(rows / 2 - 1, cols / 2 - 10);
    setTextColor(FOREGROUND_WHITE);
    cout << "=== GAME PAUSED ===";
    moveCursorTo(rows / 2, cols / 2 - 10);
    setTextColor(FOREGROUND_WHITE);
    cout << "1. Continue";
    moveCursorTo(rows / 2 + 1, cols / 2 - 10);
    setTextColor(FOREGROUND_WHITE);
    cout << "2. Settings";
    moveCursorTo(rows / 2 + 2, cols / 2 - 10);
    setTextColor(FOREGROUND_WHITE);
    cout << "3. Exit";
    cout.flush();

    while (true)
    {
        char ch;
        ch = _getch();

        if (ch == '1' || ch == 27)
        {
            totalPausedTime += chrono::steady_clock::now() - pauseStart;
            clearScreen();
            DrawBorders((rows - borderHeight) / 2, (cols - borderWidth) / 2);
            DrawSnake();
            for (const auto &food : foodPositions)
            {
                moveCursorTo(food.first, food.second);
                setTextColor(foodColor);
                cout << "@";
                setTextColor(FOREGROUND_WHITE);
            }
            cout.flush();
            break;
        }
        else if (ch == '2')
        {
            settingsMenu();
            return pauseMenu();
        }
        else if (ch == '3')
        {
            run = false;
            playerLost = false;
            break;
        }
        Sleep(screenSwapDelayShort);
    }
}

void handleInput()
{
    if (_kbhit())
    {
        char ch = _getch();
        if (ch == 27)
        {
            pauseMenu();
            return;
        }
        if (ch == 'q')
        {
            run = false;
        }
        Direction newDir = charToDirection(ch);
        switch (newDir)
        {
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
    }
}

void initializeGame(int &borderTop, int &borderLeft)
{
    hideCursor();
    clearScreen();

    getTerminalSize(rows, cols);
    borderTop = (rows - borderHeight) / 2;
    borderLeft = (cols - borderWidth) / 2;
    DrawBorders(borderTop, borderLeft);

    int midRow = borderTop + borderHeight / 2;
    int midCol = borderLeft + borderWidth / 2;
    pair<int, int> start = {midRow, midCol};
    push_front(start);

    moveCursorTo(midRow, midCol);
    setTextColor(snakeColor);
    cout << "S";
    setTextColor(FOREGROUND_WHITE);

    srand((unsigned int)time(0));
    CreateFood(borderTop, borderLeft);

    gameStart = chrono::steady_clock::now();
}

void gameLoop(int borderTop, int borderLeft)
{
    while (run)
    {
        handleInput();
        UpdateSnake(borderTop, borderLeft);
        drawSidebar(borderTop, borderLeft);
        Sleep(snakeSpeed);
    }
}

int main()
{

    resizeConsoleWindow(800, 600);

    while (true)
    {
        int borderTop, borderLeft;
        initializeGame(borderTop, borderLeft);
        gameLoop(borderTop, borderLeft);

        if (playerLost)
        {
            if (!gameOverScreen())
                break;
        }
        else
        {
            break;
        }
    }

    return 0;
}