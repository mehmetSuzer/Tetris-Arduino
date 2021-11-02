/*
 * TETRIS GAME
 * 
 * A 0.96" 128x64 OLED SSD1306 display, a joystick and a button are used for this poject
 * 
 * The screen is divided into 4x4 squares
 * So the game table is formed by a 32x16 matrix
 * 
 * If we used 32x16 byte matrix to store the table values,
 * We would use 512 bytes which correspond to 25 percent of the memory
 * There would not be enough space for functions to run
 * For instance, the display wouldnt start
 * 
 * To eliminate this problem, I used bitwise operations
 * Since table only store 4 different values (0,1,2,3)
 * 2 bits are enough to store them, which is one forth of a byte
 * I declared the game table as 32x4 byte 2D array
 * As a result, I used only 128 bytes to store the table, not 512 bytes
 * 
 * To get a value and write to the table, 2 functions are defined
 * They are get_table_value and write_table
 * 
 * @author MEHMET SUZER
 */

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Screen will be rotated, so the width matches with the short side
// While height matches with the long side
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 128

#define TABLE_WIDTH SCREEN_WIDTH/4
#define TABLE_HEIGHT SCREEN_HEIGHT/4

// Screen will be refreshed when Arduino is reset
#define OLED_RESET -1

// Although screen is rotated, we should give the width and height as if there is no rotation
// Parameters:            (<width>,       <height>,   &Wire,   <reset>)
Adafruit_SSD1306 display(SCREEN_HEIGHT, SCREEN_WIDTH, &Wire, OLED_RESET);

// Joystick is used to shift the blocks (x axis) and to activate the quick falling (+y direction)
// Button is used to rotate the block
#define JOYSTICK A0
#define QUICK_FALL A1
#define ROTATE_BUTTON 2

#define LEFT -1
#define RIGHT 1

// Values for table
#define EMPTY 0
#define BLOCK 1
#define FALLING_BLOCK 2
#define WALL 3

// Block types
#define SQUARE 0
#define LONG   1
#define TSHAPE 2
#define LSHAPE 3
#define ZSHAPE 4

// You can shift the block 2 units, while it falls one unit
#define SHIFT_TO_FALL_RATE 2

// After you rotate the block, rotate function is blocked for 3 iterations of the game loop
// To keep the block duration constant, rate doubles when quick falling is active
#define ROTATE_COUNT_MAX 3
#define QUICK_ROTATE_COUNT_MAX 6

// Sleep for every iteration
// The speed of the block doubles when quick falling is active
#define DELAY 40
#define QUICK_DELAY 20

// Block structure
// Blocks are formed by 4 4x4 squares
typedef struct {
  byte xValues[4]; 
  byte yValues[4];
  byte type;
  byte rotateState;
  boolean falling;
} Block;

// Global variables
Block *currBlock = malloc(sizeof(Block));
byte table[TABLE_HEIGHT][TABLE_WIDTH/4];
byte fallCount = 0;
byte rotateCount = 0;
boolean quick_falling = false;

void setup() {
  // Set the button 
  pinMode(ROTATE_BUTTON, INPUT);
  
  // In case Arduino doesnt start the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // Loop forever
  }

  // Rotate the screen by 90 degrees in the counter-clockwise direction
  display.setRotation(3);
  
  // Clear display and set the text size and color
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X scale text
  display.setTextColor(WHITE);

  // Write TETRIS on the screen
  display.setCursor(15, 30);
  display.print(F("TET"));
  display.setCursor(15, 50);
  display.print(F("RIS"));
  display.display();
  delay(1500);
  
  // Initialize the table
  for (int row = 0; row < TABLE_HEIGHT; row++) {
    for (int col = 0; col < TABLE_WIDTH; col++) {
      if (col == 0 || col == TABLE_WIDTH-1 || row == TABLE_HEIGHT-1)
        write_table(row, col, WALL); // side and bottom walls
      else
        write_table(row, col, EMPTY);
    }
  }

  // Set the seed and create the first block
  randomSeed(analogRead(A0));
  create_block(random(0,5), TABLE_WIDTH/2-1, 0);
}


void loop() {
  check_if_block_sits();

  // If block havent sit on the bottom wall or another block, then it falls
  // If not, check if the game is over
  if (currBlock->falling) {
    fallCount++;
    if (fallCount == SHIFT_TO_FALL_RATE) {
      fall();
      fallCount = 0;
    }
  }
  else {
    if (game_over()) {
      display.clearDisplay();
      display.setCursor(6, 30);
      display.print(F("GAME"));
      display.setCursor(6, 50);
      display.print(F("OVER"));
      display.display();
      while (1); // Loop forever
    }
    // If the game is not over, create another falling block
    // And check if a line is full or not
    create_block(random(0,5), TABLE_WIDTH/2-1, 0);
    check_full_line();
    quick_falling = false;
  }

  // Shift the block
  if (analogRead(JOYSTICK) < 350)
    shift(LEFT);
  else if (analogRead(JOYSTICK) > 650)
    shift(RIGHT);

  if (rotateCount > 0)
    rotateCount--;

  // Rotate the block 
  if (digitalRead(ROTATE_BUTTON) == HIGH && rotateCount == 0) {
    if (rotate()) {
      if (quick_falling)
        rotateCount = QUICK_ROTATE_COUNT_MAX;
      else
        rotateCount = ROTATE_COUNT_MAX;
    }
  }

  // Check whether quick falling is activated
  if (analogRead(QUICK_FALL) > 700)
    quick_falling = true;

  // Draw the table and wait for a while
  draw_table();
  if (quick_falling)
    delay(QUICK_DELAY);
  else
    delay(DELAY);
}


// get table[row][col]
byte get_table_value(byte row, byte col) {
  byte matrixValue = table[row][col/4];
  byte shift_num = 2*(3-col%4);
  return (matrixValue>>shift_num) & 3;
}


// write to table[row][col]
void write_table(byte row , byte col, byte value) {
  byte shift_num = 2*(3-col%4);
  switch (value) {
    case 0:
      table[row][col/4] &= ~(1 << shift_num);
      table[row][col/4] &= ~(1 << (shift_num+1));
      break;
    case 1:
      table[row][col/4] |= (1 << shift_num);
      table[row][col/4] &= ~(1 << (shift_num+1));
      break;
    case 2:
      table[row][col/4] &= ~(1 << shift_num);
      table[row][col/4] |= (1 << (shift_num+1));
      break;
    case 3:
      table[row][col/4] |= (1 << shift_num);
      table[row][col/4] |= (1 << (shift_num+1));
  }
}


void create_block(int type, byte top_left_x, byte top_left_y) {
  currBlock->type = type;
  currBlock->falling = true;
  currBlock->rotateState = 0;
  switch (type) {
    case SQUARE:
      currBlock->xValues[0] = top_left_x;
      currBlock->yValues[0] = top_left_y;
      currBlock->xValues[1] = top_left_x+1;
      currBlock->yValues[1] = top_left_y;
      currBlock->xValues[2] = top_left_x;
      currBlock->yValues[2] = top_left_y+1;
      currBlock->xValues[3] = top_left_x+1;
      currBlock->yValues[3] = top_left_y+1;
      break;
    case LONG:
      currBlock->xValues[0] = top_left_x;
      currBlock->yValues[0] = top_left_y;
      currBlock->xValues[1] = top_left_x+1;
      currBlock->yValues[1] = top_left_y;
      currBlock->xValues[2] = top_left_x+2;
      currBlock->yValues[2] = top_left_y;
      currBlock->xValues[3] = top_left_x+3;
      currBlock->yValues[3] = top_left_y;
      break;
    case TSHAPE:
      currBlock->xValues[0] = top_left_x;
      currBlock->yValues[0] = top_left_y;
      currBlock->xValues[1] = top_left_x-1;
      currBlock->yValues[1] = top_left_y+1;
      currBlock->xValues[2] = top_left_x;
      currBlock->yValues[2] = top_left_y+1;
      currBlock->xValues[3] = top_left_x+1;
      currBlock->yValues[3] = top_left_y+1;
      break;
    case LSHAPE:
      currBlock->xValues[0] = top_left_x;
      currBlock->yValues[0] = top_left_y;
      currBlock->xValues[1] = top_left_x;
      currBlock->yValues[1] = top_left_y+1;
      currBlock->xValues[2] = top_left_x+1;
      currBlock->yValues[2] = top_left_y+1;
      currBlock->xValues[3] = top_left_x+2;
      currBlock->yValues[3] = top_left_y+1;
      break;
    case ZSHAPE:
      currBlock->xValues[0] = top_left_x;
      currBlock->yValues[0] = top_left_y;
      currBlock->xValues[1] = top_left_x+1;
      currBlock->yValues[1] = top_left_y;
      currBlock->xValues[2] = top_left_x+1;
      currBlock->yValues[2] = top_left_y+1;
      currBlock->xValues[3] = top_left_x+2;
      currBlock->yValues[3] = top_left_y+1;
  }
}


void draw_table() {
  display.clearDisplay();

  // Draw the table
  // Walls are drawn with full squares
  // Blocks are drawn with empty squares
  for (int row = 0; row < TABLE_HEIGHT; row++) {
    for (int col = 0; col < TABLE_WIDTH; col++) {
      if (get_table_value(row, col) == WALL)
          display.fillRect(col*4, row*4, 4, 4, WHITE);
      else if (get_table_value(row, col) == BLOCK || get_table_value(row, col) == FALLING_BLOCK)
          display.drawRect(col*4, row*4, 4, 4, WHITE);
    }
  }
  display.display();
}


// Clear the data of the falling block from the table
void clear_falling_block() {
  for (int i = 0; i < 4; i++)
    write_table(currBlock->yValues[i], currBlock->xValues[i], EMPTY);
}


// Add the data of the block to the table
void add_falling_block() {
  for (int i = 0; i < 4; i++)
    write_table(currBlock->yValues[i], currBlock->xValues[i], FALLING_BLOCK);
}


void fall() {
  clear_falling_block();
  for (int i = 0; i < 4; i++) 
    currBlock->yValues[i]++;
  add_falling_block();
}


void shift(int dir) {
  // If there is wall or block, then dont shift
  for (int i = 0; i < 4; i++) {
    if (get_table_value(currBlock->yValues[i], currBlock->xValues[i]+dir) == BLOCK ||
        get_table_value(currBlock->yValues[i], currBlock->xValues[i]+dir) == WALL)
      return;
  }
  // If not, shift each square of the block one unit
  clear_falling_block();
  for (int i = 0; i < 4; i++)
    currBlock->xValues[i] += dir;
  add_falling_block();
}


void check_if_block_sits() {
  // If there is wall or another block in the next row at the same column, then the falling block stops
  for (int i = 0; i < 4; i++) {
    if (get_table_value(currBlock->yValues[i]+1, currBlock->xValues[i]) == BLOCK ||
        get_table_value(currBlock->yValues[i]+1, currBlock->xValues[i]) == WALL) {
      currBlock->falling = false;
      break;
    }
  }
  if (!currBlock->falling) {
    for (int i = 0; i < 4; i++) {
      write_table(currBlock->yValues[i], currBlock->xValues[i], BLOCK);
    }
  }
}


void check_full_line() {
  // Check whether lines are full or not
  // If there is one, then shift the blocks to down
  // Then check again, starting from the bottom
  boolean line_is_full;
  int row = TABLE_HEIGHT-2;
  int col;
  while (row > 0) {
    line_is_full = true;
    for (col = 1; col < TABLE_WIDTH-1; col++) {
      if (get_table_value(row, col) == EMPTY) {
        line_is_full = false;
        break;
      }
    }
    if (line_is_full) {
      for (col = 1; col < TABLE_WIDTH-1; col++) {
        write_table(row, col, EMPTY);
        shift_squares_down(col);
      }
      row = TABLE_HEIGHT-2;
    }
    else
      row--;
  }
}


// Helper function of the check_full_line
void shift_squares_down(int colNum) {
  int row = TABLE_HEIGHT-2;
  int i;
  while (row > 0) {
    if (get_table_value(row, colNum) == BLOCK) {
      i = 1;
      while (get_table_value(row+i, colNum) != WALL && get_table_value(row+i, colNum) != BLOCK)
        i++;
      write_table(row, colNum, EMPTY);
      write_table(row+i-1, colNum, BLOCK);
    }
    row--;
  }
}


boolean rotate() {
  // Store the initial positions and the rotate state
  byte initial_xValues[4] = {currBlock->xValues[0], currBlock->xValues[1], currBlock->xValues[2], currBlock->xValues[3]};
  byte initial_yValues[4] = {currBlock->yValues[0], currBlock->yValues[1], currBlock->yValues[2], currBlock->yValues[3]};
  byte initial_rotateState = currBlock->rotateState;
  boolean rotated = true;
  clear_falling_block();
  switch (currBlock->type) {
    case SQUARE:
      break;
    case LONG:
      if (currBlock->rotateState == 0) {
        currBlock->xValues[0]++;
        currBlock->xValues[2]--;
        currBlock->xValues[3] -= 2;   
        currBlock->yValues[1]++;
        currBlock->yValues[2] += 2;
        currBlock->yValues[3] += 3;
        currBlock->rotateState = 2;
      }
      else if (currBlock->rotateState == 2) {
        currBlock->xValues[0]--;
        currBlock->xValues[2]++;
        currBlock->xValues[3] += 2;
        currBlock->yValues[1]--;
        currBlock->yValues[2] -= 2;
        currBlock->yValues[3] -= 3;
        currBlock->rotateState = 0;
      }
      break;
    case TSHAPE:
      if (currBlock->rotateState == 0) {
        currBlock->xValues[1]++;
        currBlock->xValues[2]++;
        currBlock->xValues[3]--;
        currBlock->yValues[3]++;
        currBlock->rotateState = 1;
      }
      else if (currBlock->rotateState == 1) {
        currBlock->xValues[0]--;
        currBlock->yValues[0]++;
        currBlock->rotateState = 2;
      }
      else if (currBlock->rotateState == 2) {
        currBlock->xValues[0]++;
        currBlock->xValues[1]--;
        currBlock->xValues[2]--;
        currBlock->yValues[0]--;
        currBlock->rotateState = 3;
      }
      else if (currBlock->rotateState == 3) {    
        currBlock->xValues[3]++;
        currBlock->yValues[3]--;
        currBlock->rotateState = 0;
      }
      break;
    case ZSHAPE:
      if (currBlock->rotateState == 0) {
        currBlock->xValues[0]++;
        currBlock->xValues[1]--;
        currBlock->xValues[3] -= 2;   
        currBlock->yValues[0]--;
        currBlock->yValues[2]--;
        currBlock->rotateState = 2;
      }
      else if (currBlock->rotateState == 2) {
        currBlock->xValues[0]--;
        currBlock->xValues[1]++;
        currBlock->xValues[3] += 2; 
        currBlock->yValues[0]++;
        currBlock->yValues[2]++;
        currBlock->rotateState = 2;
        currBlock->rotateState = 0;
      }
      break;
    case LSHAPE:
      if (currBlock->rotateState == 0) {
        currBlock->xValues[1]++;
        currBlock->xValues[2]--;
        currBlock->xValues[3] -= 2;

        currBlock->yValues[0]--;
        currBlock->yValues[1] -= 2;
        currBlock->yValues[2]--;
        currBlock->rotateState = 1;
      }
      else if (currBlock->rotateState == 1) {
        currBlock->xValues[2] += 2;
        currBlock->xValues[3] += 2;

        currBlock->yValues[0]++;
        currBlock->yValues[1]++;
        currBlock->rotateState = 2;
      }
      else if (currBlock->rotateState == 2) {
        currBlock->xValues[0]++;
        currBlock->xValues[2] -= 2;
        currBlock->xValues[3]--;

        currBlock->yValues[0]--;
        currBlock->yValues[2]++;
        currBlock->rotateState = 3;
      }
      else if (currBlock->rotateState == 3) {    
        currBlock->xValues[0]--;
        currBlock->xValues[1]--;
        currBlock->xValues[2]++;
        currBlock->xValues[3]++;

        currBlock->yValues[0]++;
        currBlock->yValues[1]++;
        currBlock->rotateState = 0;
      }
  }
  // If it collides with walls or another block after the rotation,
  // Then write the previous values back
  if (there_is_collusion()) {
    for (int i = 0; i < 4; i++) {
      currBlock->xValues[i] = initial_xValues[i];
      currBlock->yValues[i] = initial_yValues[i];
    }
    currBlock->rotateState = initial_rotateState;
    rotated = false;
  }
  add_falling_block();
  return rotated;
}


boolean there_is_collusion() {
  for (int i = 0; i < 4; i++) {
    if (get_table_value(currBlock->yValues[i], currBlock->xValues[i]) ==  WALL ||
        get_table_value(currBlock->yValues[i], currBlock->xValues[i]) ==  BLOCK)
        return true;
  }
  return false;
}


// Check if the game is over
boolean game_over() {
  boolean result = false;
  for (byte col = 1; col < TABLE_WIDTH-1; col++) {
    if (get_table_value(1, col) == BLOCK)
      result = true;
  }
  return result;
}
