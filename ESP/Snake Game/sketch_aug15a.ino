/*
 * ESP32 Snake Game for 1.8" ST7735 TFT Display
 * Compatible with NodeMCU-32S
 * Updated Version with Button Debug and Both Game Modes
 * 
 * Pin Connections:
 * TFT Display (ST7735):
 * - VCC -> 3.3V
 * - GND -> GND
 * - CLK -> GPIO18 (SCK)
 * - DIN -> GPIO23 (MOSI)
 * - CS  -> GPIO5
 * - D/C -> GPIO2
 * - RST -> GPIO4
 * - BL  -> 3.3V
 * 
 * 4-Pin Push Buttons:
 * Each button has 4 pins in 2 pairs. Connect one pin from each pair:
 * - UP    -> GPIO12 (one pin) + GND (opposite pair pin)
 * - DOWN  -> GPIO14 (one pin) + GND (opposite pair pin)
 * - LEFT  -> GPIO27 (one pin) + GND (opposite pair pin)
 * - RIGHT -> GPIO26 (one pin) + GND (opposite pair pin)
 * 
 * Required Libraries:
 * - Adafruit_GFX
 * - Adafruit_ST7735
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// TFT Display pins
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2

// Button pins
#define BTN_UP    12
#define BTN_DOWN  14
#define BTN_LEFT  27
#define BTN_RIGHT 26

// Game settings
#define GRID_SIZE 8
#define GRID_WIDTH (128 / GRID_SIZE)
#define GRID_HEIGHT (160 / GRID_SIZE)
#define MAX_SNAKE_LENGTH 50

// Game mode selection
#define MANUAL_MODE 1    // Snake moves only when button pressed
#define AUTO_MODE 0      // Snake moves automatically
#define GAME_MODE AUTO_MODE  // Traditional snake game with continuous movement

// Colors
#define BLACK    0x0000
#define GREEN    0x07E0
#define RED      0xF800
#define BLUE     0x001F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF
#define GRAY     0x8410
#define DARK_GREEN 0x03E0

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Game variables
struct Point {
  int x, y;
};

Point snake[MAX_SNAKE_LENGTH];
Point food;
int snakeLength = 3;
int direction = 0; // 0=right, 1=down, 2=left, 3=up
int score = 0;
bool gameOver = false;
bool gameStarted = false;

// Timing variables
unsigned long lastMoveTime = 0;
unsigned long moveInterval = 300; // milliseconds between auto moves (faster)
unsigned long lastButtonRead = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Snake Game Starting...");
  
  // Initialize TFT
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  
  // Initialize buttons with internal pull-up resistors
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  Serial.println("Buttons initialized with pull-up resistors");
  Serial.println("Button Test: Press buttons to see readings...");
  
  // Initialize game
  initGame();
  showStartScreen();
}

void loop() {
  // Debug button readings (comment out after testing works)
  // debugButtons();
  
  if (!gameStarted) {
    if (readButtons() != -1) {
      gameStarted = true;
      initGame();
      drawGame();
      Serial.println("Game Started!");
    }
    return;
  }
  
  if (gameOver) {
    if (readButtons() != -1) {
      gameOver = false;
      gameStarted = true;
      initGame();
      drawGame();
      Serial.println("Game Restarted!");
    }
    return;
  }
  
  // TRADITIONAL SNAKE GAME: Continuous movement with direction changes
  
  // Read button input to change direction (buttons don't move, just change direction)
  int input = readButtons();
  if (input != -1) {
    updateDirection(input);
  }
  
  // Snake moves automatically at regular intervals
  if (millis() - lastMoveTime >= moveInterval) {
    lastMoveTime = millis();
    
    // Move snake automatically
    moveSnake();
    
    // Check collisions
    checkCollisions();
    
    // Update display
    if (!gameOver) {
      drawGame();
      
      // Increase speed as score increases (make game harder)
      if (score >= 100) moveInterval = 150;        // Very fast
      else if (score >= 50) moveInterval = 200;    // Fast  
      else if (score >= 20) moveInterval = 250;    // Medium-fast
    } else {
      showGameOver();
    }
  }
  
  delay(5); // Very small delay for button responsiveness
}

void debugButtons() {
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) { // Print every second
    lastDebug = millis();
    Serial.print("Buttons - UP:");
    Serial.print(digitalRead(BTN_UP) ? "1" : "0");
    Serial.print(" DOWN:");
    Serial.print(digitalRead(BTN_DOWN) ? "1" : "0");
    Serial.print(" LEFT:");
    Serial.print(digitalRead(BTN_LEFT) ? "1" : "0");
    Serial.print(" RIGHT:");
    Serial.println(digitalRead(BTN_RIGHT) ? "1" : "0");
  }
}

void initGame() {
  // Initialize snake in center, moving right
  snakeLength = 3;
  snake[0] = {GRID_WIDTH/2, GRID_HEIGHT/2};       // Head
  snake[1] = {GRID_WIDTH/2 - 1, GRID_HEIGHT/2};   // Body
  snake[2] = {GRID_WIDTH/2 - 2, GRID_HEIGHT/2};   // Tail
  
  direction = 0; // Start moving right
  score = 0;
  gameOver = false;
  moveInterval = 300; // Reset to normal speed
  lastMoveTime = millis(); // Initialize timing
  
  // Generate first food
  generateFood();
  
  // Clear screen
  tft.fillScreen(BLACK);
  
  Serial.println("Game initialized - Snake will move continuously!");
}

void showStartScreen() {
  tft.fillScreen(BLACK);
  
  // Title
  tft.setTextColor(GREEN);
  tft.setTextSize(3);
  tft.setCursor(15, 30);
  tft.print("SNAKE");
  
  // Mode indicator
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 65);
  tft.print("CONTINUOUS SNAKE");
  
  // Instructions
  tft.setTextColor(WHITE);
  tft.setCursor(10, 85);
  tft.print("Press any button");
  tft.setCursor(25, 100);
  tft.print("to start");
  
  // Controls
  tft.setTextColor(BLUE);
  tft.setCursor(5, 120);
  tft.print("Buttons change");
  tft.setCursor(15, 135);
  tft.print("direction only");
  
  Serial.println("Start screen displayed");
}

void showGameOver() {
  tft.fillScreen(BLACK);
  
  // Game Over text
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.setCursor(15, 40);
  tft.print("GAME");
  tft.setCursor(20, 60);
  tft.print("OVER");
  
  // Score
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 90);
  tft.print("Final Score: ");
  tft.print(score);
  
  tft.setCursor(30, 105);
  tft.print("Length: ");
  tft.print(snakeLength);
  
  // Restart instruction
  tft.setCursor(10, 125);
  tft.print("Press any button");
  tft.setCursor(15, 140);
  tft.print("to restart");
  
  Serial.print("Game Over! Final Score: ");
  Serial.println(score);
}

int readButtons() {
  if (millis() - lastButtonRead < 100) return -1; // Slightly longer debounce for direction changes
  lastButtonRead = millis();
  
  if (digitalRead(BTN_UP) == LOW) {
    Serial.println("Direction: UP");
    return 3; // Up
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    Serial.println("Direction: DOWN");
    return 1; // Down
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    Serial.println("Direction: LEFT");
    return 2; // Left
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    Serial.println("Direction: RIGHT");
    return 0; // Right
  }
  
  return -1; // No button pressed
}

void updateDirection(int newDir) {
  // Prevent immediate reverse direction (snake can't go backward into itself)
  if ((direction == 0 && newDir == 2) || // Right -> Left
      (direction == 2 && newDir == 0) || // Left -> Right
      (direction == 1 && newDir == 3) || // Down -> Up
      (direction == 3 && newDir == 1)) { // Up -> Down
    Serial.println("Reverse direction blocked");
    return;
  }
  
  direction = newDir;
  Serial.print("Direction changed to: ");
  String dirs[] = {"RIGHT", "DOWN", "LEFT", "UP"};
  Serial.println(dirs[direction]);
}

void moveSnake() {
  // Move body segments (start from tail)
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  
  // Move head based on current direction
  switch (direction) {
    case 0: snake[0].x++; break; // Right
    case 1: snake[0].y++; break; // Down
    case 2: snake[0].x--; break; // Left
    case 3: snake[0].y--; break; // Up
  }
}

void checkCollisions() {
  // Wall collision
  if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH || 
      snake[0].y < 0 || snake[0].y >= GRID_HEIGHT) {
    gameOver = true;
    Serial.println("Wall collision!");
    return;
  }
  
  // Self collision
  for (int i = 1; i < snakeLength; i++) {
    if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
      gameOver = true;
      Serial.println("Self collision!");
      return;
    }
  }
  
  // Food collision
  if (snake[0].x == food.x && snake[0].y == food.y) {
    score += 10;
    snakeLength++;
    Serial.print("Food eaten! Score: ");
    Serial.print(score);
    Serial.print(", Length: ");
    Serial.println(snakeLength);
    
    if (snakeLength >= MAX_SNAKE_LENGTH) {
      gameOver = true; // Win condition
      Serial.println("Maximum length reached - You Win!");
      return;
    }
    generateFood();
  }
}

void generateFood() {
  bool validPosition = false;
  int attempts = 0;
  
  while (!validPosition && attempts < 100) {
    food.x = random(0, GRID_WIDTH);
    food.y = random(0, GRID_HEIGHT);
    
    validPosition = true;
    // Check if food spawns on snake
    for (int i = 0; i < snakeLength; i++) {
      if (food.x == snake[i].x && food.y == snake[i].y) {
        validPosition = false;
        break;
      }
    }
    attempts++;
  }
  
  Serial.print("New food at: (");
  Serial.print(food.x);
  Serial.print(", ");
  Serial.print(food.y);
  Serial.println(")");
}

void drawGame() {
  // Clear screen
  tft.fillScreen(BLACK);
  
  // Draw game border
  tft.drawRect(0, 0, 128, GRID_HEIGHT * GRID_SIZE, WHITE);
  
  // Draw snake
  for (int i = 0; i < snakeLength; i++) {
    int x = snake[i].x * GRID_SIZE + 1;
    int y = snake[i].y * GRID_SIZE + 1;
    
    if (i == 0) {
      // Head - bright green with border
      tft.fillRect(x, y, GRID_SIZE - 1, GRID_SIZE - 1, GREEN);
      tft.drawRect(x, y, GRID_SIZE - 1, GRID_SIZE - 1, WHITE);
    } else {
      // Body - darker green
      tft.fillRect(x, y, GRID_SIZE - 1, GRID_SIZE - 1, DARK_GREEN);
    }
  }
  
  // Draw food with animation effect
  int fx = food.x * GRID_SIZE + 1;
  int fy = food.y * GRID_SIZE + 1;
  tft.fillRect(fx, fy, GRID_SIZE - 1, GRID_SIZE - 1, RED);
  tft.drawRect(fx, fy, GRID_SIZE - 1, GRID_SIZE - 1, YELLOW);
  
  // Draw score and info
  int infoY = GRID_HEIGHT * GRID_SIZE + 5;
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  
  tft.setCursor(5, infoY);
  tft.print("Score: ");
  tft.print(score);
  
  tft.setCursor(70, infoY);
  tft.print("Len: ");
  tft.print(snakeLength);
  
  // Game mode indicator
  tft.setCursor(5, infoY + 12);
  tft.setTextColor(YELLOW, BLACK);
  tft.print("SPEED:");
  
  // Speed indicator based on current moveInterval
  tft.setCursor(70, infoY + 12);
  if (moveInterval <= 150) tft.print("VERY FAST");
  else if (moveInterval <= 200) tft.print("FAST");
  else if (moveInterval <= 250) tft.print("MEDIUM");
  else tft.print("NORMAL");
}