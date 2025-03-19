# Snake Game in C++

A simple and interactive snake game implemented in C++. This game works cross-platform, meaning it runs on both Windows and UNIX-like systems (Linux/macOS). The game uses basic terminal control techniques for rendering and user input.

## **Features**

- Cross-platform compatibility: Windows and UNIX-based systems (Linux/macOS)
- Terminal-based interface with customizable snake and food colors
- Real-time gameplay with arrow key controls for moving the snake
- Automatic snake movement, food spawning, and border handling
- Non-blocking input to ensure smooth game mechanics
- Ability to quit the game by pressing 'q'

## **Requirements**

- Unix system (Linux/macOS), Windows OS
- A terminal with support for ANSI escape codes

<br>

## **Installation**

1. Go to the [Releases page](https://github.com/D4rkN00dl3s/Snake/releases) of this repository.

2. Choose the appropriate `.tar` archive for your operating system:
    - For Windows: `snake_game_win.tar`
    - For Linux/macOS: `snake_game_linux.tar`

3. Download and extract the `.tar` archive:
    ```bash
    tar -xvf snake_game_win.tar    # For Windows
    tar -xvf snake_game_linux.tar  # For Linux/macOS
    ```

4. Run the game:
    - **Windows**: Execute `snake_game_win.exe`.
    - **Linux/macOS**: Execute `./snake_game_linux`.

<br>

## **Controls**

- **W** – Move Up
- **A** – Move Left
- **S** – Move Down
- **D** – Move Right
- **Q** – Quit the Game

## **Game Preview**

Here’s what the game might look like when played in the terminal:

![snake](https://github.com/user-attachments/assets/8e404f70-5eac-41bf-9f2b-b0b1c0523b88)


- **S** represents the snake's head, and **@** represents the food.
- The snake moves and grows as it eats the food.
