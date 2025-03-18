# Snake Game in C++
A simple implementation of the Snake game in C++ that runs in the terminal. The game uses raw terminal input for controlling the snake and displays the game with borders, food, and snake movement.
Features

    The snake is controlled using the keyboard arrows (w, a, s, d).
    The snake grows when it eats food, which is represented by the @ symbol.
    The game ends if the snake collides with the border.
    Terminal-based game with dynamic sizing to fit the terminal window.

Requirements

    A terminal or command-line environment.
    A Unix-like operating system (Linux, macOS) or Windows (with windows.h support).
    Basic C++ compiler support (e.g., g++).

Controls

    w – Move Up
    a – Move Left
    s – Move Down
    d – Move Right
    q – Quit the Game

How It Works

    Terminal Size: The game dynamically adjusts the game area based on the current terminal size. It centers the game in the terminal window.
    Raw Mode: The terminal is set to "raw" mode so that key presses are registered immediately without needing to press Enter.
    Game Loop: The main game loop updates the snake's position, checks for collisions, and draws the game state after each movement.
    Snake and Food: The snake is a deque data structure that grows in size when food is eaten. The food is placed randomly within the borders.

Game Logic

    Movement: The snake's direction is controlled by the w, a, s, d keys. The snake moves one unit at a time and grows if it eats food.
    Border Collision: If the snake hits the border, the game ends.
    Food Collision: If the snake eats the food (represented by @), the snake grows, and new food is placed in a random location.
