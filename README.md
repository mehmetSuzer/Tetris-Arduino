# Tetris-Arduino


TETRIS GAME

A 0.96" 128x64 OLED SSD1306 display, a joystick and a button are used for this poject.
 
The screen is divided into 4x4 squares.

So the game table is formed by a 32x16 matrix.
 
If we used 32x16 byte matrix to store the table values,

We would use 512 bytes which correspond to 25 percent of the memory.

There would not be enough space for functions to run.

For instance, the display wouldn't start.
 
To eliminate this problem, I used bitwise operations.

Since table only store 4 different values (0,1,2,3),

2 bits are enough to store them, which is one forth of a byte.

I initialised the game table as 32x4 byte 2D array.

As a result, I used only 128 bytes to store the table, not 512 bytes.

To get a value and write to the table, 2 functions are defined.


![oled-ssd1306](https://user-images.githubusercontent.com/93345336/139813862-700aecab-98c9-4fd1-9157-747a7bf4e91c.jpg)
