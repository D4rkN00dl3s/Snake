#include <iostream>
#include <deque>
#include <ctime>
#include <unistd.h>  // For read() and usleep()
#include <termios.h> // For terminal settings
#include <fcntl.h>
#include <sys/ioctl.h>

using namespace std;

// Cross-platform function to get terminal size
void getTerminalSize(int &rows, int &cols)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        cols = w.ws_col;
        rows = w.ws_row;
    }
}

// Cross-platform function to move cursor
void moveCursorTo(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

struct termios original_termios;

void restoreTerminalSettings()
{
    printf("\033[H\033[J");
    cout << "Terminating program" << endl;
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
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

// Global Variables
int rows = 0, cols = 0;
int fx = 0, fy = 0;
int borderWidth = 60;
int borderHeight = 20;
char dir = 'd';
bool run = true;
deque<pair<int, int>> snake;

// Food placement
void CreateFood(int top, int left)
{
    fx = left + 1 + rand() % (borderWidth - 2);
    fy = top + 1 + rand() % (borderHeight - 2);
    moveCursorTo(fy, fx);
    printf("\033[31m@\033[0m");
}

// Draw game borders
void DrawBorders(int top, int left)
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

// Draw snake
void DrawSnake()
{
    for (const auto &part : snake)
    {
        moveCursorTo(part.first, part.second);
        printf("\033[32mS\033[0m");
    }
    cout.flush();
}

// Update snake position
void UpdateSnake(int top, int left)
{
    static int dx[] = {-1, 1, 0, 0};
    static int dy[] = {0, 0, -1, 1};
    static char dirs[] = {'w', 's', 'a', 'd'};

    int dirIndex = 0;
    for (; dirIndex < 4; ++dirIndex)
        if (dir == dirs[dirIndex])
            break;

    int currRow = snake.front().first + dx[dirIndex];
    int currCol = snake.front().second + dy[dirIndex];

    if (currRow < top || currRow >= top + borderHeight ||
        currCol <= left || currCol >= left + borderWidth)
    {
        run = false;
        return;
    }

    snake.push_front({currRow, currCol});
    if (currRow == fy && currCol == fx)
    {
        CreateFood(top, left);
    }
    else
    {
        moveCursorTo(snake.back().first, snake.back().second);
        cout << " ";
        snake.pop_back();
    }
    DrawSnake();
}

// Read keyboard input (cross-platform)
char getInput()
{
    char ch;
    if (read(STDIN_FILENO, &ch, 1) > 0)
    {
        return ch;
    }
    return '\0';
}

int main()
{
    printf("\033[H\033[J");

    enableRawMode();
    setNonBlockingInput();

    getTerminalSize(rows, cols);

    int top = (rows - borderHeight) / 2;
    int left = (cols - borderWidth) / 2;
    int midRow = rows / 2;
    int midCol = cols / 2;

    DrawBorders(top, left);

    snake.push_back({midRow, midCol});
    moveCursorTo(midRow, midCol);
    cout << "S" << endl;

    srand(static_cast<unsigned int>(time(0)));
    CreateFood(top, left);

    // Main Game Loop
    while (run)
    {
        char ch = getInput();
        if (ch)
        { // If key is pressed
            switch (ch)
            {
            case 'd':
                if (dir != 'a')
                    dir = 'd';
                break;
            case 'a':
                if (dir != 'd')
                    dir = 'a';
                break;
            case 'w':
                if (dir != 's')
                    dir = 'w';
                break;
            case 's':
                if (dir != 'w')
                    dir = 's';
                break;
            case 'q':
                run = false;
                break;
            }
        }

        UpdateSnake(top, left);

        usleep(150000); // Linux/macOS
    }
    return 0;
}
