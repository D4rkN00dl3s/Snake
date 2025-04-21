#include <iostream>
#include <vector>
#include <unordered_set>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <algorithm>

using namespace std;

// Constants
constexpr int MAX_SNAKE_LENGTH = 1000;
constexpr int DEFAULT_BORDER_WIDTH = 60;
constexpr int DEFAULT_BORDER_HEIGHT = 20;
constexpr int MAX_ATTEMPTS = 500;

// Game State
int borderWidth = DEFAULT_BORDER_WIDTH;
int borderHeight = DEFAULT_BORDER_HEIGHT;
int rows = 0, cols = 0;
unsigned int score = 0;
bool run = true, playerLost = false;

// Terminal Settings
struct termios original_termios;

// Snake Configuration
int snakeSpeed = 150000;
string snakeColor = "\033[32m";
string foodColor = "\033[31m";
int foodCount = 1;
vector<pair<int, int>> foodPositions;

// Snake Data Structures
pair<int, int> snakeBuffer[MAX_SNAKE_LENGTH];
int head = 0, tail = 0, snakeSize = 0;

enum class Direction{ UP, DOWN, LEFT, RIGHT };
Direction dir = Direction::RIGHT;

// Clock For Timer
chrono::steady_clock::time_point gameStart;
chrono::steady_clock::time_point pauseStart;
chrono::steady_clock::duration totalPausedTime = chrono::seconds(0);

struct pairHash
{
    size_t operator()(const pair<int, int> &p) const
    {
        return hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
    }
};

unordered_set<pair<int, int>, pairHash> snakeBody;

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

// Terminal Control
void clearTerminal() { printf("\033[H\033[J"); }
void moveCursorTo(int row, int col) { printf("\033[%d;%dH", row, col); }
void hideCursor(){ printf("\033[?25l"); fflush(stdout);}
void showCursor() { printf("\033[?25h"); fflush(stdout); }

void getTerminalSize(int &rows, int &cols)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        cols = w.ws_col;
        rows = w.ws_row;
    }
}

void restoreTerminalSettings()
{
    clearTerminal();
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    printf("\033[?25h");
    fflush(stdout);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restoreTerminalSettings);
}

void setNonBlockingInput()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

char getInput()
{
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == 1)
    {
        if (ch == '\033') // possible ESC or arrow key
        {
            // Wait 30ms to see if more characters follow
            usleep(30000);

            char seq[2];
            int n = read(STDIN_FILENO, seq, 2);

            if (n == 0)
            {
                return '\033'; // ESC key pressed
            }
            else if (n == 2 && seq[0] == '[')
            {
                switch (seq[1])
                {
                case 'A':
                    return 'w'; // Up
                case 'B':
                    return 's'; // Down
                case 'C':
                    return 'd'; // Right
                case 'D':
                    return 'a'; // Left
                }
            }
        }
        return ch;
    }
    return '\0';
}

void drawBorders(int top, int left)
{
    string border(static_cast<size_t>(borderWidth), '_');
    moveCursorTo(top - 1, left);
    cout << border;
    moveCursorTo(top + borderHeight, left);
    cout << border;

    for (int i = 0; i <= borderHeight; i++)
    {
        moveCursorTo(top + i, left);
        cout << "|";
        moveCursorTo(top + i, left + borderWidth);
        cout << "|";
    }
    cout.flush();
}

void drawSidebar(int top, int left)
{
    moveCursorTo(top, 2);
    cout << "\033[36m=== INFO ===\033[0m";

    moveCursorTo(top + 2, 2);
    cout << "Score: " << score;

    auto now = chrono::steady_clock::now();
    auto playTime = chrono::duration_cast<chrono::seconds>(now - gameStart - totalPausedTime);
    int minutes = playTime.count() / 60;
    int seconds = playTime.count() % 60;

    moveCursorTo(top + 4, 2);
    printf("Time: %02d:%02d", minutes, seconds);

    cout.flush();
}

bool gameOverScreen()
{
    clearTerminal();
    int centerRow = rows / 2;
    int centerCol = cols / 2 - 10;

    // Blinking GAME OVER animation
    for (int i = 0; i < 6; ++i)
    {
        moveCursorTo(centerRow, centerCol);
        if (i % 2 == 0)
            cout << "\033[5;31m=== GAME OVER ===\033[0m";
        else
            cout << "                  ";

        cout.flush();
        usleep(300000);
    }

    moveCursorTo(centerRow - 2, centerCol);
    cout << "\033[5;31m=== GAME OVER ===\033[0m";

    moveCursorTo(centerRow, centerCol);
    cout << "Your final score: " << score;

    moveCursorTo(centerRow + 2, centerCol);
    cout << "1. Restart";

    moveCursorTo(centerRow + 3, centerCol);
    cout << "2. Exit Game";

    cout.flush();

    while (true)
    {
        char ch = getInput();
        if (ch == '1')
        {
            // Countdown animation
            for (int i = 3; i >= 1; --i)
            {
                moveCursorTo(centerRow + 5, centerCol);
                cout << "\033[33mRestarting in " << i << "...\033[0m ";
                cout.flush();
                usleep(1000000);
            }

            run = true;
            score = 0;
            dir = Direction::RIGHT;
            foodPositions.clear();
            head = tail = snakeSize = 0;
            snakeBody.clear();
            clearTerminal();
            return true; // Restart
        }
        else if (ch == '2')
        {
            return false; // Exit
        }
        usleep(10000);
    }
}

void createFood(int top, int left)
{
    int toSpawn = foodCount - static_cast<int>(foodPositions.size());
    int attempts = 0;

    while (toSpawn > 0 && attempts < MAX_ATTEMPTS)
    {
        int fx = left + 1 + rand() % (borderWidth - 2);
        int fy = top + 1 + rand() % (borderHeight - 2);
        pair<int, int> food = {fy, fx};

        if (snakeBody.find(food) == snakeBody.end() &&
            find(foodPositions.begin(), foodPositions.end(), food) == foodPositions.end())
        {
            foodPositions.push_back(food);
            moveCursorTo(fy, fx);
            cout << foodColor << "@" << "\033[0m";
            toSpawn--;
        }

        attempts++;
    }

    if (toSpawn > 0)
    {
        moveCursorTo(top + borderHeight + 2, left);
        cout << "\033[31m[!] Warning: Could not place all food. Board may be too full.\033[0m";
    }

    cout.flush();
}

void drawSnake()
{
    for (int i = 0; i < snakeSize; ++i)
    {
        auto pos = snakeBuffer[mod(head + i)];
        moveCursorTo(pos.first, pos.second);
        cout << snakeColor << "S" << "\033[0m";
    }
    cout.flush();
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

int getRawNumberInput(int min, int max)
{
    string input;
    char ch;
    while (true)
    {
        ch = getInput();
        if (ch >= '0' && ch <= '9')
        {
            input += ch;
            cout << ch;
            cout.flush();
        }
        else if (ch == '\n' || ch == '\r')
        {
            if (!input.empty())
            {
                int value = stoi(input);
                if (value >= min && value <= max)
                {
                    return value;
                }
                else
                {
                    input.clear();
                    cout << "\nInvalid range. Try again: ";
                    cout.flush();
                }
            }
        }
        else if (ch == 127 || ch == '\b')
        { // handle backspace
            if (!input.empty())
            {
                input.pop_back();
                cout << "\b \b";
                cout.flush();
            }
        }
        usleep(10000); // allow time to process key
    }
}

void changeSnakeSpeed()
{
    clearTerminal();
    moveCursorTo(rows / 2, cols / 2 - 20);
    cout << "Choose your speed level (1-4, 1 = slowest, 4 = fastest)): ";
    cout.flush();
    bool correctInput = false;

    while (correctInput == false)
    {
        int value = getRawNumberInput(1, 4);
        usleep(10000);
        switch (value)
        {
        case 1:
            snakeSpeed = 500000;
            correctInput = true;
            break;
        case 2:
            snakeSpeed = 250000;
            correctInput = true;
            break;
        case 3:
            snakeSpeed = 100000;
            correctInput = true;
            break;
        case 4:
            snakeSpeed = 50000;
            correctInput = true;
            break;
        default:
            cout << "Wrong Input! Try again!";
            break;
        }
    }

    moveCursorTo(rows / 2 + 1, cols / 2 - 10);
    cout << "Speed updated to " << snakeSpeed << " ms!";
    cout.flush();
    usleep(500000);
    clearTerminal();
}

void settingsMenu()
{
    while (true)
    {
        clearTerminal();
        moveCursorTo(rows / 2 - 2, cols / 2 - 10);
        cout << "=== SETTINGS ===";
        moveCursorTo(rows / 2 - 1, cols / 2 - 10);
        cout << "1. Snake Speed";
        moveCursorTo(rows / 2, cols / 2 - 10);
        cout << "2. Snake Color";
        moveCursorTo(rows / 2 + 1, cols / 2 - 10);
        cout << "3. Food Color";
        moveCursorTo(rows / 2 + 2, cols / 2 - 10);
        cout << "4. Food Amount (current: " << foodCount << ")";
        moveCursorTo(rows / 2 + 3, cols / 2 - 10);
        cout << "5. Back to Pause Menu";
        cout.flush();

        char ch = getInput();

        if (ch == '\033') // ESC key
            return;

        if (ch == '1')
        {
            changeSnakeSpeed();
        }
        else if (ch == '2')
        {
            clearTerminal();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                cout << "Choose Snake Color: 1=Green 2=Yellow 3=Cyan: ";
                cout.flush();
                c = getInput();
                usleep(10000);
            } while (c != '1' && c != '2' && c != '3');

            if (c == '1')
                snakeColor = "\033[32m";
            else if (c == '2')
                snakeColor = "\033[33m";
            else if (c == '3')
                snakeColor = "\033[36m";

            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            cout << "Color changed!";
            cout.flush();
            usleep(500000);
            clearTerminal();
        }
        else if (ch == '3')
        {
            clearTerminal();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                cout << "Choose Food Color: 1=Red 2=Magenta 3=Blue: ";
                cout.flush();
                c = getInput();
                usleep(10000);
            } while (c != '1' && c != '2' && c != '3');

            if (c == '1')
                foodColor = "\033[31m";
            else if (c == '2')
                foodColor = "\033[35m";
            else if (c == '3')
                foodColor = "\033[34m";

            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            cout << "Color changed!";
            cout.flush();
            usleep(500000);
            clearTerminal();
        }
        else if (ch == '4')
        {
            clearTerminal();
            char c = '\0';
            do
            {
                moveCursorTo(rows / 2, cols / 2 - 20);
                cout << "Enter food amount (1-3): ";
                cout.flush();
                c = getInput();
                usleep(10000);
            } while (c != '1' && c != '2' && c != '3');
            foodCount = c - '0';

            moveCursorTo(rows / 2 + 1, cols / 2 - 10);
            cout << "Food count updated!";
            cout.flush();
            usleep(500000);
            clearTerminal();
        }
        else if (ch == '5')
            break;
        usleep(200000);
    }
}

void pauseMenu()
{
    pauseStart = chrono::steady_clock::now();

    clearTerminal();
    moveCursorTo(rows / 2 - 1, cols / 2 - 10);
    cout << "=== GAME PAUSED ===";
    moveCursorTo(rows / 2, cols / 2 - 10);
    cout << "1. Continue";
    moveCursorTo(rows / 2 + 1, cols / 2 - 10);
    cout << "2. Settings";
    moveCursorTo(rows / 2 + 2, cols / 2 - 10);
    cout << "3. Exit";
    cout.flush();

    while (true)
    {
        char ch = getInput();
        if (ch == '1' || ch == '\033')
        {
            totalPausedTime += chrono::steady_clock::now() - pauseStart;
            clearTerminal();
            drawBorders((rows - borderHeight) / 2, (cols - borderWidth) / 2);
            drawSnake();
            for (const auto &food : foodPositions)
            {
                moveCursorTo(food.first, food.second);
                cout << foodColor << "@" << "\033[0m";
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
        usleep(10000);
    }
}

void handleInput(char ch)
{
    if (ch == '\033')
    {
        pauseMenu();
        return;
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

    if (ch == 'q')
        run = false;
}

void updateSnake(int top, int left)
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

    if (newRow < top || newRow >= top + borderHeight ||
        newCol <= left || newCol >= left + borderWidth ||
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
            foodPositions.erase(foodPositions.begin() + i); // Remove eaten food
            break;
        }
    }

    push_front(newHead);

    if (ate)
    {
        createFood(top, left);
    }
    else
    {
        moveCursorTo(get_back().first, get_back().second);
        cout << " ";
        pop_back();
    }

    drawSnake();
}

void initializeTerminal()
{
    clearTerminal();
    enableRawMode();
    setNonBlockingInput();
    hideCursor();
    getTerminalSize(rows, cols);
}

void initializeGame(int &top, int &left)
{
    getTerminalSize(rows, cols);
    top = (rows - borderHeight) / 2;
    left = (cols - borderWidth) / 2;

    drawBorders(top, left);

    int midRow = rows / 2;
    int midCol = cols / 2;
    pair<int, int> start = {midRow, midCol};

    push_front(start);
    moveCursorTo(midRow, midCol);
    cout << "S" << endl;

    srand(static_cast<unsigned int>(time(0)));
    createFood(top, left);

    gameStart = chrono::steady_clock::now();
}

void gameLoop(int top, int left)
{
    while (run)
    {
        char ch = getInput();
        if (ch)
            handleInput(ch);

        updateSnake(top, left);
        drawSidebar(top, left);
        usleep(snakeSpeed);
    }
}

int main()
{
    initializeTerminal();

    while (true)
    {
        int top, left;
        initializeGame(top, left);
        gameLoop(top, left);

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