#include <iostream>
#include <vector>
#include <unordered_set>
#include <deque>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <algorithm>
// persistence
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

// Optional SDL2 audio support. Define USE_SDL_AUDIO when compiling to enable.
#ifdef USE_SDL_AUDIO
#include <SDL2/SDL.h>
static bool audioInited = false;
static SDL_AudioDeviceID audioDevice = 0;
static SDL_AudioSpec eatSpec, crashSpec;
static Uint8 *eatBuf = nullptr;
static Uint32 eatLen = 0;
static Uint8 *crashBuf = nullptr;
static Uint32 crashLen = 0;

bool initAudio()
{
    if (SDL_Init(SDL_INIT_AUDIO) != 0)
        return false;

    // load WAVs from assets/ (user must add these files)
    if (SDL_LoadWAV("assets/eat.wav", &eatSpec, &eatBuf, &eatLen) == NULL)
    {
        // continue but mark uninitialized
        eatBuf = nullptr;
        eatLen = 0;
    }

    if (SDL_LoadWAV("assets/crash.wav", &crashSpec, &crashBuf, &crashLen) == NULL)
    {
        crashBuf = nullptr;
        crashLen = 0;
    }

    SDL_AudioSpec want = {0};
    if (eatBuf)
        want = eatSpec;
    else if (crashBuf)
        want = crashSpec;
    else
        want.freq = 48000, want.format = AUDIO_S16LSB, want.channels = 2, want.samples = 4096;

    audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (audioDevice == 0)
    {
        // audio not available
        if (eatBuf) SDL_FreeWAV(eatBuf);
        if (crashBuf) SDL_FreeWAV(crashBuf);
        eatBuf = crashBuf = nullptr;
        eatLen = crashLen = 0;
        SDL_Quit();
        return false;
    }

    SDL_PauseAudioDevice(audioDevice, 0);
    audioInited = true;
    return true;
}

void playBuffer(Uint8 *buf, Uint32 len)
{
    if (!audioInited || !buf || len == 0)
        return;
    SDL_ClearQueuedAudio(audioDevice);
    SDL_QueueAudio(audioDevice, buf, len);
}

void playEatSound() { playBuffer(eatBuf, eatLen); }
void playCrashSound() { playBuffer(crashBuf, crashLen); }

void shutdownAudio()
{
    if (eatBuf) SDL_FreeWAV(eatBuf);
    if (crashBuf) SDL_FreeWAV(crashBuf);
    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();
}
#else
// Fallback audio using external players (aplay/paplay/play). Non-blocking via fork().
static bool audioFallbackAvailable = false;

bool initAudio()
{
    // check if any player is available and assets exist
    bool playerExists = (access("/usr/bin/aplay", X_OK) == 0) || (access("/usr/bin/paplay", X_OK) == 0) || (access("/usr/bin/play", X_OK) == 0);
    struct stat st;
    bool filesExist = (stat("assets/eat.wav", &st) == 0) || (stat("assets/crash.wav", &st) == 0);
    audioFallbackAvailable = playerExists && filesExist;
    return audioFallbackAvailable;
}

static void spawnPlayer(const char *file)
{
    if (!audioFallbackAvailable) return;
    pid_t pid = fork();
    if (pid == 0)
    {
        // try aplay, paplay, then play
        execlp("aplay", "aplay", "-q", file, (char *)NULL);
        execlp("paplay", "paplay", file, (char *)NULL);
        execlp("play", "play", "-q", file, (char *)NULL);
        _exit(1);
    }
}

void playEatSound() { spawnPlayer("assets/eat.wav"); }
void playCrashSound() { spawnPlayer("assets/crash.wav"); }
void shutdownAudio() { /* nothing to do for spawn fallback */ }
#endif

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
int lives = 3;
int level = 1;

// Gameplay flags
bool wrapAround = false;
unsigned int highScore = 0;
std::vector<unsigned int> highScores;
const size_t HIGH_SCORES_MAX = 5;

// forward declarations for functions used before their definitions
void saveConfig();
void clearTerminal();
void moveCursorTo(int row, int col);
char getInput();

void updateHighScores(unsigned int sc)
{
    highScores.push_back(sc);
    sort(highScores.begin(), highScores.end(), greater<unsigned int>());
    if (highScores.size() > HIGH_SCORES_MAX)
        highScores.resize(HIGH_SCORES_MAX);
    if (!highScores.empty())
        highScore = highScores[0];
    saveConfig();
}

void showHighScores()
{
    clearTerminal();
    int centerRow = rows / 2 - 3;
    int centerCol = cols / 2 - 10;
    moveCursorTo(centerRow, centerCol);
    cout << "\033[36m=== HIGH SCORES ===\033[0m";
    if (highScores.empty())
    {
        moveCursorTo(centerRow + 2, centerCol);
        cout << "(no high scores yet)";
    }
    else
    {
        for (size_t i = 0; i < highScores.size(); ++i)
        {
            moveCursorTo(centerRow + 2 + (int)i, centerCol);
            cout << (i + 1) << ". " << highScores[i];
        }
    }
    moveCursorTo(centerRow + 2 + (int)highScores.size() + 2, centerCol);
    cout << "Press any key to continue...";
    cout.flush();
    // wait for a key
    while (true)
    {
        char ch = getInput();
        if (ch)
            break;
        usleep(10000);
    }
    clearTerminal();
}

// Terminal Settings
struct termios original_termios;

// Snake Configuration
int snakeSpeed = 150000;
string snakeColor = "\033[32m";
string foodColor = "\033[31m";
int foodCount = 1;
vector<pair<int, int>> foodPositions;

// Snake Data Structures
std::deque<pair<int, int>> snake;

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

// Config filenames and persistence helpers
static const char *APP_DIR_NAME = "Snake";
static const char *CONFIG_FILE_NAME = "config.ini";

std::string getDataDir()
{
    const char *xdg = getenv("XDG_DATA_HOME");
    std::string base;
    if (xdg && xdg[0] != '\0')
        base = xdg;
    else
    {
        const char *home = getenv("HOME");
        if (!home)
            home = ".";
        base = std::string(home) + "/.local/share";
    }
    return base + "/" + APP_DIR_NAME;
}

std::string getConfigPath()
{
    return getDataDir() + "/" + CONFIG_FILE_NAME;
}

void saveConfig()
{
    std::string dir = getDataDir();
    mkdir(dir.c_str(), 0755);
    std::ofstream ofs(getConfigPath());
    if (!ofs)
        return;

    ofs << "high_score=" << highScore << "\n";
    // save list top scores
    ofs << "high_scores=";
    for (size_t i = 0; i < highScores.size(); ++i)
    {
        if (i) ofs << ",";
        ofs << highScores[i];
    }
    ofs << "\n";
    ofs << "wrap_around=" << (wrapAround ? 1 : 0) << "\n";
    ofs << "snake_speed=" << snakeSpeed << "\n";
    ofs << "food_count=" << foodCount << "\n";
    ofs << "snake_color=" << snakeColor << "\n";
    ofs << "food_color=" << foodColor << "\n";
    ofs.close();
}

void loadConfig()
{
    std::ifstream ifs(getConfigPath());
    if (!ifs)
        return;

    std::string line;
    while (std::getline(ifs, line))
    {
        std::istringstream iss(line);
        std::string key;
        if (!std::getline(iss, key, '='))
            continue;
        std::string value;
        if (!std::getline(iss, value))
            continue;

        if (key == "high_score")
            highScore = static_cast<unsigned int>(std::stoul(value));
        else if (key == "high_scores")
        {
            highScores.clear();
            std::istringstream lv(value);
            std::string tok;
            while (std::getline(lv, tok, ','))
            {
                if (!tok.empty())
                    highScores.push_back(static_cast<unsigned int>(std::stoul(tok)));
            }
        }
        else if (key == "wrap_around")
            wrapAround = (value == "1");
        else if (key == "snake_speed")
            snakeSpeed = std::stoi(value);
        else if (key == "food_count")
            foodCount = std::stoi(value);
        else if (key == "snake_color")
            snakeColor = value;
        else if (key == "food_color")
            foodColor = value;
    }
    ifs.close();
    // If the file only had a scalar high_score, ensure highScores vector is populated
    if (highScores.empty() && highScore > 0)
        highScores.push_back(highScore);
}

void push_front(pair<int, int> pos)
{
    snake.push_front(pos);
    snakeBody.insert(pos);
}

void pop_back()
{
    auto back = snake.back();
    snakeBody.erase(back);
    snake.pop_back();
}

pair<int, int> get_front() { return snake.front(); }
pair<int, int> get_back() { return snake.back(); }

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
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval tv = {0, 0}; // non-blocking poll
    int rv = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
    if (rv <= 0)
        return '\0';

    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n != 1)
        return '\0';

    if (ch == '\033') // possible ESC or arrow key
    {
        // wait briefly for the rest of the sequence
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval tv2;
        tv2.tv_sec = 0;
        tv2.tv_usec = 30000; // 30ms
        int rv2 = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv2);
        if (rv2 <= 0)
        {
            return '\033'; // standalone ESC
        }

        char seq[2] = {0, 0};
        ssize_t m = read(STDIN_FILENO, seq, 2);
        if (m == 2 && seq[0] == '[')
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

        return '\033';
    }

    return ch;
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
    // HUD in two columns: stats (left) and controls (right)
    int leftCol = 2;
    int rightCol = max(left + borderWidth + 4, leftCol + 24);

    moveCursorTo(top, leftCol);
    cout << "\033[36m=== INFO ===\033[0m";

    auto now = chrono::steady_clock::now();
    auto playTime = chrono::duration_cast<chrono::seconds>(now - gameStart - totalPausedTime);
    int minutes = playTime.count() / 60;
    int seconds = playTime.count() % 60;

    moveCursorTo(top + 1, leftCol);
    printf("Score: %u", score);
    moveCursorTo(top + 2, leftCol);
    printf("High:  %u", highScore);
    moveCursorTo(top + 3, leftCol);
    printf("Lives: %d", lives);
    moveCursorTo(top + 4, leftCol);
    printf("Level: %d", level);
    moveCursorTo(top + 5, leftCol);
    printf("Time: %02d:%02d", minutes, seconds);

    // Controls column
    moveCursorTo(top + 1, rightCol);
    cout << "Controls:";
    moveCursorTo(top + 2, rightCol);
    cout << "WASD - Move";
    moveCursorTo(top + 3, rightCol);
    cout << "Esc  - Pause";
    moveCursorTo(top + 4, rightCol);
    cout << "Q    - Quit";

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

    // update high score immediately so it's saved regardless of user's choice
    if (score > highScore)
    {
        updateHighScores(score);
    }

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
            clearTerminal();
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
            snake.clear();
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
    for (const auto &pos : snake)
    {
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
    saveConfig();
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
        cout << "5. Wrap-around (current: " << (wrapAround ? "On" : "Off") << ")";
        moveCursorTo(rows / 2 + 4, cols / 2 - 10);
        cout << "6. Back to Pause Menu";
            moveCursorTo(rows / 2 + 5, cols / 2 - 10);
            cout << "7. View High Scores";
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
            saveConfig();
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
            saveConfig();
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
            saveConfig();
            usleep(500000);
            clearTerminal();
        }
        else if (ch == '5')
        {
            wrapAround = !wrapAround;
            moveCursorTo(rows / 2 + 5, cols / 2 - 10);
            cout << "Wrap-around " << (wrapAround ? "enabled" : "disabled") << "!";
            cout.flush();
            saveConfig();
            usleep(500000);
            clearTerminal();
        }
        else if (ch == '6')
            break;
        else if (ch == '7')
        {
            showHighScores();
        }
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
    // compute interior bounds
    int minRow = top + 1;
    int maxRow = top + borderHeight - 1;
    int minCol = left + 1;
    int maxCol = left + borderWidth - 1;

    if (wrapAround)
    {
        if (newRow < minRow)
            newRow = maxRow;
        else if (newRow > maxRow)
            newRow = minRow;

        if (newCol < minCol)
            newCol = maxCol;
        else if (newCol > maxCol)
            newCol = minCol;
    }

    pair<int, int> newHead = {newRow, newCol};

    if (!wrapAround)
    {
        if (newRow < minRow || newRow > maxRow ||
            newCol < minCol || newCol > maxCol ||
            snakeBody.count(newHead))
        {
            playerLost = true;
            run = false;
            playCrashSound();
            return;
        }
    }
    else
    {
        if (snakeBody.count(newHead))
        {
            playerLost = true;
            run = false;
            playCrashSound();
            return;
        }
    }

    bool ate = false;
    for (size_t i = 0; i < foodPositions.size(); ++i)
    {
        if (newRow == foodPositions[i].first && newCol == foodPositions[i].second)
        {
            score++;
                playEatSound();
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
    loadConfig();
    initializeTerminal();
    // try to initialize audio (optional). Requires SDL2 and WAV files at assets/*.wav
    initAudio();

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

    shutdownAudio();
    return 0;
}