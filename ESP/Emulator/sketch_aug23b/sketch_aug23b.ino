#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <vector>
#include <String.h>

// ---------------- TFT PINS ----------------
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2

// ---------------- SD CARD PINS ----------------
#define SD_CS     15
#define SD_MISO   19

// ---------------- BUTTON PINS ----------------
#define BTN_UP    12
#define BTN_DOWN  14
#define BTN_LEFT  27
#define BTN_RIGHT 26  
#define BTN_SELECT 25 
#define BTN_START 33
#define BTN_BACK  32

// ---------------- DISPLAY SETUP ----------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------------- GAME TYPES ----------------
enum GameType {
  GAME_TYPE_NES,
  GAME_TYPE_BUILTIN,
  GAME_TYPE_NONE
};

// ---------------- GAME MANAGEMENT ----------------
struct GameFile {
  String filename;
  String displayName;
  unsigned long fileSize;
  GameType type;
};

std::vector<GameFile> gameList;
int currentGameIndex = 0;
int menuOffset = 0;
const int GAMES_PER_SCREEN = 5;

// ---------------- UI COLORS ----------------
#define COLOR_BG        ST77XX_BLACK
#define COLOR_HEADER    ST77XX_BLUE
#define COLOR_TEXT      ST77XX_WHITE
#define COLOR_SELECTED  ST77XX_RED
#define COLOR_BORDER    ST77XX_GREEN
#define COLOR_INFO      ST77XX_YELLOW

// ---------------- BUTTON STATE ----------------
struct ButtonState {
  bool current;
  bool previous;
  unsigned long lastPress;
  bool pressed() {
    return current && !previous;
  }
  bool held() {
    return current && (millis() - lastPress > 500);
  }
};

ButtonState buttons[7];

// ---------------- EMULATOR STATE ----------------
enum EmulatorState {
  STATE_INIT,
  STATE_MENU,
  STATE_LOADING,
  STATE_PLAYING,
  STATE_ERROR,
  STATE_SD_DEBUG
};

EmulatorState currentState = STATE_INIT;
String errorMessage = "";

// ---------------- GAME VARIABLES ----------------
// Snake Game
struct Point {
  int x, y;
};
std::vector<Point> snake;
Point food;
int snakeDirection = 0;
int snakeScore = 0;
bool gameRunning = false;

// Pong Game
int paddleY = 60;
int ballX = 64, ballY = 40;
int ballDX = 1, ballDY = 1;
int pongScore = 0;

// Tetris pieces
int tetrisGrid[20][10];
int currentPiece[4][4];
int pieceX = 3, pieceY = 0;
int tetrisScore = 0;
int linesCleared = 0;

// Function declarations
void playBreakoutGame();
void playTetrisGame();
void initTetrisGame();
void generateTetrisPiece();
bool canMoveTetrisPiece(int deltaX, int deltaY);
void rotateTetrisPiece();
void placeTetrisPiece();
void checkTetrisLines();
void drawTetrisGame();
void showTetrisGameOver();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 Game Console Starting...");
  
  if (!initDisplay()) {
    Serial.println("Display initialization failed!");
    while(1) {
      delay(1000);
      Serial.println("Display failed - check connections!");
    }
  }
  
  initButtons();
  showBootScreen();
  delay(2000);
  
  if (!initSDAdvanced()) {
    Serial.println("SD card not found - using built-in games only");
    currentState = STATE_MENU;
  } else {
    loadGameList();
    currentState = STATE_MENU;
  }
  
  addBuiltInGames();
  drawMenu();
  Serial.println("System initialized successfully!");
}

void loop() {
  updateButtons();
  
  switch (currentState) {
    case STATE_INIT:
      break;
      
    case STATE_MENU:
      handleMenuInput();
      break;
      
    case STATE_LOADING:
      delay(1000);
      startSelectedGame();
      break;
      
    case STATE_PLAYING:
      break;
      
    case STATE_ERROR:
      if (buttons[6].pressed()) {
        currentState = STATE_MENU;
        drawMenu();
      }
      break;
      
    case STATE_SD_DEBUG:
      handleSDDebugInput();
      break;
  }
  
  delay(10);
}

bool initDisplay() {
  Serial.println("Initializing ST7735 Display...");
  
  try {
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.println("Display Test");
    
    Serial.println("Display initialized successfully");
    delay(500);
    return true;
  } catch (...) {
    return false;
  }
}

void showBootScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.println("ESP32");
  tft.setCursor(10, 50);
  tft.println("CONSOLE");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(25, 80);
  tft.println("Game System v2.0");
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(30, 100);
  tft.println("Loading...");
  
  for (int i = 0; i < 8; i++) {
    tft.fillRect(10 + i * 12, 120, 10, 6, COLOR_SELECTED);
    delay(150);
  }
}

void initButtons() {
  int buttonPins[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SELECT, BTN_START, BTN_BACK};
  
  for (int i = 0; i < 7; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttons[i].current = false;
    buttons[i].previous = false;
    buttons[i].lastPress = 0;
  }
}

bool initSDAdvanced() {
  Serial.println("Initializing SD Card...");
  
  SPI.begin();
  delay(100);
  
  if (SD.begin(SD_CS)) {
    Serial.println("SD card initialized successfully");
    return testSDCard();
  }
  
  if (SD.begin(SD_CS, SPI, 1000000)) {
    Serial.println("SD card initialized with 1MHz");
    return testSDCard();
  }
  
  Serial.println("SD card initialization failed");
  return false;
}

bool testSDCard() {
  if (!SD.exists("/NES")) {
    SD.mkdir("/NES");
  }
  return true;
}

void addBuiltInGames() {
  GameFile snake = {"snake", "Snake Game", 0, GAME_TYPE_BUILTIN};
  GameFile pong = {"pong", "Pong Game", 0, GAME_TYPE_BUILTIN};
  GameFile tetris = {"tetris", "Tetris Game", 0, GAME_TYPE_BUILTIN};
  GameFile demo = {"demo", "Graphics Demo", 0, GAME_TYPE_BUILTIN};
  
  gameList.insert(gameList.begin(), demo);
  gameList.insert(gameList.begin(), tetris);
  gameList.insert(gameList.begin(), pong);
  gameList.insert(gameList.begin(), snake);
}

void loadGameList() {
  if (!SD.exists("/NES")) return;
  
  File nesFolder = SD.open("/NES");
  if (!nesFolder) return;
  
  File file = nesFolder.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".nes") || filename.endsWith(".NES")) {
        GameFile game;
        game.filename = filename;
        game.displayName = filename.substring(0, filename.lastIndexOf('.'));
        game.fileSize = file.size();
        game.type = GAME_TYPE_NES;
        gameList.push_back(game);
      }
    }
    file.close();
    file = nesFolder.openNextFile();
  }
  nesFolder.close();
}

void updateButtons() {
  int buttonPins[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SELECT, BTN_START, BTN_BACK};
  
  for (int i = 0; i < 7; i++) {
    buttons[i].previous = buttons[i].current;
    buttons[i].current = !digitalRead(buttonPins[i]);
    
    if (buttons[i].pressed()) {
      buttons[i].lastPress = millis();
    }
  }
}

void handleMenuInput() {
  if (buttons[0].pressed()) {
    currentGameIndex--;
    if (currentGameIndex < 0) {
      currentGameIndex = gameList.size() - 1;
    }
    updateMenuScroll();
    drawMenu();
  }
  
  if (buttons[1].pressed()) {
    currentGameIndex++;
    if (currentGameIndex >= gameList.size()) {
      currentGameIndex = 0;
    }
    updateMenuScroll();
    drawMenu();
  }
  
  if (buttons[4].pressed() || buttons[5].pressed()) {
    if (gameList.size() > 0) {
      currentState = STATE_LOADING;
      drawLoadingScreen();
    }
  }
  
  if (buttons[6].pressed()) {
    showSystemInfo();
  }
}

void updateMenuScroll() {
  if (currentGameIndex < menuOffset) {
    menuOffset = currentGameIndex;
  } else if (currentGameIndex >= menuOffset + GAMES_PER_SCREEN) {
    menuOffset = currentGameIndex - GAMES_PER_SCREEN + 1;
  }
}

void drawMenu() {
  tft.fillScreen(COLOR_BG);
  
  tft.fillRect(0, 0, 128, 15, COLOR_HEADER);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(35, 5);
  tft.println("GAMES");
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(5, 20);
  tft.println("Total: " + String(gameList.size()));
  
  int yPos = 35;
  for (int i = 0; i < GAMES_PER_SCREEN && (menuOffset + i) < gameList.size(); i++) {
    int gameIndex = menuOffset + i;
    
    if (gameIndex == currentGameIndex) {
      tft.fillRect(2, yPos - 2, 124, 12, COLOR_SELECTED);
      tft.setTextColor(COLOR_BG);
    } else {
      if (gameList[gameIndex].type == GAME_TYPE_BUILTIN) {
        tft.setTextColor(COLOR_INFO);
      } else {
        tft.setTextColor(COLOR_TEXT);
      }
    }
    
    tft.setCursor(5, yPos);
    String displayText = gameList[gameIndex].displayName;
    if (displayText.length() > 18) {
      displayText = displayText.substring(0, 15) + "...";
    }
    tft.println(displayText);
    
    yPos += 15;
  }
  
  tft.setTextColor(COLOR_BORDER);
  tft.setTextSize(1);
  tft.setCursor(5, 140);
  tft.println("U/D:Select START:Play");
  tft.setCursor(5, 150);
  tft.println("BACK:System Info");
}

void drawLoadingScreen() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(25, 40);
  tft.println("LOADING GAME");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(5, 60);
  String gameName = gameList[currentGameIndex].displayName;
  if (gameName.length() > 16) {
    gameName = gameName.substring(0, 13) + "...";
  }
  tft.println(gameName);
  
  for (int i = 0; i < 6; i++) {
    tft.fillRect(15 + i * 15, 80, 12, 6, COLOR_SELECTED);
    delay(200);
  }
}

void startSelectedGame() {
  if (gameList.size() == 0) return;
  
  GameFile& game = gameList[currentGameIndex];
  
  if (game.type == GAME_TYPE_BUILTIN) {
    if (game.filename == "snake") {
      playSnakeGame();
    } else if (game.filename == "pong") {
      playPongGame();
    } else if (game.filename == "tetris") {
      playTetrisGame();
    } else if (game.filename == "demo") {
      playGraphicsDemo();
    }
  } else if (game.type == GAME_TYPE_NES) {
    showComingSoon();
  }
}

void showComingSoon() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(20, 40);
  tft.println("NES EMULATION");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(25, 60);
  tft.println("Coming Soon!");
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(10, 80);
  tft.println("NES ROM detected but");
  tft.setCursor(10, 90);
  tft.println("emulator core needs");
  tft.setCursor(10, 100);
  tft.println("to be integrated.");
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("Any button to return");
  
  while (true) {
    updateButtons();
    for (int i = 0; i < 7; i++) {
      if (buttons[i].pressed()) {
        currentState = STATE_MENU;
        drawMenu();
        return;
      }
    }
    delay(50);
  }
}

// ============== SNAKE GAME ==============
void playSnakeGame() {
  currentState = STATE_PLAYING;
  initSnakeGame();
  
  unsigned long lastUpdate = 0;
  const unsigned long gameSpeed = 200;
  
  while (currentState == STATE_PLAYING) {
    updateButtons();
    
    if (buttons[6].pressed()) {
      currentState = STATE_MENU;
      drawMenu();
      return;
    }
    
    if (buttons[0].pressed() && snakeDirection != 1) snakeDirection = 3; // UP
    if (buttons[1].pressed() && snakeDirection != 3) snakeDirection = 1; // DOWN
    if (buttons[2].pressed() && snakeDirection != 0) snakeDirection = 2; // LEFT
    if (buttons[3].pressed() && snakeDirection != 2) snakeDirection = 0; // RIGHT
    
    if (millis() - lastUpdate > gameSpeed) {
      updateSnakeGame();
      drawSnakeGame();
      lastUpdate = millis();
      
      if (!gameRunning) {
        showSnakeGameOver();
        return;
      }
    }
    
    delay(10);
  }
}

void initSnakeGame() {
  snake.clear();
  snake.push_back({5, 5});
  snake.push_back({4, 5});
  snake.push_back({3, 5});
  
  snakeDirection = 0;
  snakeScore = 0;
  gameRunning = true;
  
  placeSnakeFood();
  
  tft.fillScreen(COLOR_BG);
  tft.drawRect(0, 15, 128, 120, COLOR_BORDER);
  
  tft.setTextColor(COLOR_INFO);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Snake - Score: " + String(snakeScore));
}

void placeSnakeFood() {
  bool validPos = false;
  while (!validPos) {
    food.x = random(1, 15);
    food.y = random(2, 12);
    
    validPos = true;
    for (Point& segment : snake) {
      if (segment.x == food.x && segment.y == food.y) {
        validPos = false;
        break;
      }
    }
  }
}

void updateSnakeGame() {
  Point newHead = snake[0];
  
  switch (snakeDirection) {
    case 0: newHead.x++; break;
    case 1: newHead.y++; break;
    case 2: newHead.x--; break;
    case 3: newHead.y--; break;
  }
  
  if (newHead.x < 1 || newHead.x > 15 || newHead.y < 2 || newHead.y > 12) {
    gameRunning = false;
    return;
  }
  
  for (Point& segment : snake) {
    if (segment.x == newHead.x && segment.y == newHead.y) {
      gameRunning = false;
      return;
    }
  }
  
  snake.insert(snake.begin(), newHead);
  
  if (newHead.x == food.x && newHead.y == food.y) {
    snakeScore += 10;
    placeSnakeFood();
  } else {
    snake.pop_back();
  }
}

void drawSnakeGame() {
  tft.fillRect(1, 16, 126, 118, COLOR_BG);
  
  tft.setTextColor(COLOR_SELECTED);
  for (Point& segment : snake) {
    int x = segment.x * 8;
    int y = segment.y * 10;
    tft.fillRect(x, y, 6, 8, COLOR_SELECTED);
  }
  
  int fx = food.x * 8;
  int fy = food.y * 10;
  tft.fillRect(fx, fy, 6, 8, COLOR_INFO);
  
  tft.fillRect(0, 0, 128, 14, COLOR_BG);
  tft.setTextColor(COLOR_INFO);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Snake - Score: " + String(snakeScore));
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("D-PAD:Move BACK:Exit");
}

void showSnakeGameOver() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_SELECTED);
  tft.setTextSize(2);
  tft.setCursor(15, 40);
  tft.println("GAME OVER");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(30, 70);
  tft.println("Final Score: " + String(snakeScore));
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(20, 90);
  tft.println("Well played!");
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("Any button to return");
  
  while (true) {
    updateButtons();
    for (int i = 0; i < 7; i++) {
      if (buttons[i].pressed()) {
        currentState = STATE_MENU;
        drawMenu();
        return;
      }
    }
    delay(50);
  }
}

// ============== PONG GAME ==============
void playPongGame() {
  currentState = STATE_PLAYING;
  initPongGame();
  
  unsigned long lastUpdate = 0;
  const unsigned long gameSpeed = 50;
  
  while (currentState == STATE_PLAYING) {
    updateButtons();
    
    if (buttons[6].pressed()) {
      currentState = STATE_MENU;
      drawMenu();
      return;
    }
    
    if (millis() - lastUpdate > gameSpeed) {
      updatePongGame();
      drawPongGame();
      lastUpdate = millis();
    }
    
    delay(10);
  }
}

void initPongGame() {
  paddleY = 60;
  ballX = 64;
  ballY = 40;
  ballDX = 2;
  ballDY = 1;
  pongScore = 0;
  
  tft.fillScreen(COLOR_BG);
  tft.drawRect(5, 15, 118, 120, COLOR_BORDER);
  
  tft.setTextColor(COLOR_INFO);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Pong - Score: " + String(pongScore));
}

void updatePongGame() {
  if (buttons[0].current && paddleY > 20) paddleY -= 3;
  if (buttons[1].current && paddleY < 120) paddleY += 3;
  
  ballX += ballDX;
  ballY += ballDY;
  
  if (ballY <= 18 || ballY >= 130) {
    ballDY = -ballDY;
  }
  
  if (ballX <= 8) {
    ballDX = -ballDX;
  }
  
  if (ballX >= 115 && ballX <= 120 && ballY >= paddleY - 15 && ballY <= paddleY + 15) {
    ballDX = -ballDX;
    pongScore += 10;
  }
  
  if (ballX >= 125) {
    ballX = 64;
    ballY = 40;
    ballDX = -2;
    ballDY = random(0, 2) ? 1 : -1;
  }
}

void drawPongGame() {
  tft.fillRect(6, 16, 116, 118, COLOR_BG);
  
  tft.fillRect(115, paddleY - 15, 4, 30, COLOR_SELECTED);
  tft.fillRect(ballX, ballY, 4, 4, COLOR_INFO);
  
  for (int i = 20; i < 130; i += 10) {
    tft.fillRect(63, i, 2, 5, COLOR_BORDER);
  }
  
  tft.fillRect(0, 0, 128, 14, COLOR_BG);
  tft.setTextColor(COLOR_INFO);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("Pong - Score: " + String(pongScore));
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("U/D:Paddle BACK:Exit");
}

// ============== TETRIS GAME ==============
void playTetrisGame() {
  currentState = STATE_PLAYING;
  initTetrisGame();
  
  unsigned long lastDrop = 0;
  const unsigned long dropSpeed = 800;
  
  while (currentState == STATE_PLAYING) {
    updateButtons();
    
    if (buttons[6].pressed()) {
      currentState = STATE_MENU;
      drawMenu();
      return;
    }
    
    if (buttons[0].pressed()) {
      rotateTetrisPiece();
    }
    if (buttons[1].pressed()) {
      if (canMoveTetrisPiece(0, 1)) {
        pieceY++;
      } else {
        placeTetrisPiece();
      }
    }
    if (buttons[2].pressed()) {
      if (canMoveTetrisPiece(-1, 0)) {
        pieceX--;
      }
    }
    if (buttons[3].pressed()) {
      if (canMoveTetrisPiece(1, 0)) {
        pieceX++;
      }
    }
    
    if (millis() - lastDrop > dropSpeed) {
      if (canMoveTetrisPiece(0, 1)) {
        pieceY++;
      } else {
        placeTetrisPiece();
      }
      lastDrop = millis();
    }
    
    drawTetrisGame();
    delay(50);
  }
}

void initTetrisGame() {
  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 10; x++) {
      tetrisGrid[y][x] = 0;
    }
  }
  
  tetrisScore = 0;
  linesCleared = 0;
  pieceX = 3;
  pieceY = 0;
  
  generateTetrisPiece();
  
  tft.fillScreen(COLOR_BG);
  tft.drawRect(5, 15, 82, 120, COLOR_BORDER);
  
  tft.setTextColor(COLOR_INFO);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("TETRIS");
  
  tft.setCursor(90, 20);
  tft.println("Score:");
  tft.setCursor(90, 35);
  tft.println("Lines:");
}

void generateTetrisPiece() {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      currentPiece[y][x] = 0;
    }
  }
  
  int pieceType = random(0, 4);
  
  switch (pieceType) {
    case 0: // I piece
      currentPiece[1][0] = 1;
      currentPiece[1][1] = 1;
      currentPiece[1][2] = 1;
      currentPiece[1][3] = 1;
      break;
      
    case 1: // O piece
      currentPiece[0][1] = 1;
      currentPiece[0][2] = 1;
      currentPiece[1][1] = 1;
      currentPiece[1][2] = 1;
      break;
      
    case 2: // T piece
      currentPiece[0][1] = 1;
      currentPiece[1][0] = 1;
      currentPiece[1][1] = 1;
      currentPiece[1][2] = 1;
      break;
      
    case 3: // L piece
      currentPiece[0][0] = 1;
      currentPiece[1][0] = 1;
      currentPiece[1][1] = 1;
      currentPiece[1][2] = 1;
      break;
  }
}

bool canMoveTetrisPiece(int deltaX, int deltaY) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (currentPiece[y][x]) {
        int newX = pieceX + x + deltaX;
        int newY = pieceY + y + deltaY;
        
        if (newX < 0 || newX >= 10 || newY >= 20) {
          return false;
        }
        
        if (newY >= 0 && tetrisGrid[newY][newX]) {
          return false;
        }
      }
    }
  }
  return true;
}

void rotateTetrisPiece() {
  int rotated[4][4];
  
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      rotated[x][3-y] = currentPiece[y][x];
    }
  }
  
  int tempPiece[4][4];
  memcpy(tempPiece, currentPiece, sizeof(currentPiece));
  memcpy(currentPiece, rotated, sizeof(currentPiece));
  
  if (!canMoveTetrisPiece(0, 0)) {
    memcpy(currentPiece, tempPiece, sizeof(currentPiece));
  }
}

void placeTetrisPiece() {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (currentPiece[y][x]) {
        int gridY = pieceY + y;
        int gridX = pieceX + x;
        
        if (gridY >= 0 && gridY < 20 && gridX >= 0 && gridX < 10) {
          tetrisGrid[gridY][gridX] = 1;
        }
      }
    }
  }
  
  checkTetrisLines();
  
  pieceX = 3;
  pieceY = 0;
  generateTetrisPiece();
  
  if (!canMoveTetrisPiece(0, 0)) {
    showTetrisGameOver();
  }
}

void checkTetrisLines() {
  int linesFound = 0;
  
  for (int y = 19; y >= 0; y--) {
    bool fullLine = true;
    for (int x = 0; x < 10; x++) {
      if (!tetrisGrid[y][x]) {
        fullLine = false;
        break;
      }
    }
    
    if (fullLine) {
      for (int moveY = y; moveY > 0; moveY--) {
        for (int x = 0; x < 10; x++) {
          tetrisGrid[moveY][x] = tetrisGrid[moveY-1][x];
        }
      }
      
      for (int x = 0; x < 10; x++) {
        tetrisGrid[0][x] = 0;
      }
      
      linesFound++;
      y++;
    }
  }
  
  if (linesFound > 0) {
    linesCleared += linesFound;
    tetrisScore += linesFound * 100;
  }
}

void drawTetrisGame() {
  tft.fillRect(6, 16, 80, 118, COLOR_BG);
  
  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 10; x++) {
      if (tetrisGrid[y][x]) {
        int screenX = 8 + x * 8;
        int screenY = 18 + y * 6;
        tft.fillRect(screenX, screenY, 6, 4, COLOR_SELECTED);
      }
    }
  }
  
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (currentPiece[y][x]) {
        int screenX = 8 + (pieceX + x) * 8;
        int screenY = 18 + (pieceY + y) * 6;
        if (screenY >= 18) {
          tft.fillRect(screenX, screenY, 6, 4, COLOR_INFO);
        }
      }
    }
  }
  
  tft.fillRect(90, 50, 35, 30, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(90, 50);
  tft.println(String(tetrisScore));
  tft.setCursor(90, 65);
  tft.println(String(linesCleared));
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("U:Rotate L/R:Move");
  tft.setCursor(5, 150);
  tft.println("D:Drop BACK:Exit");
}

void showTetrisGameOver() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_SELECTED);
  tft.setTextSize(2);
  tft.setCursor(15, 40);
  tft.println("GAME OVER");
  
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(20, 70);
  tft.println("Score: " + String(tetrisScore));
  tft.setCursor(20, 85);
  tft.println("Lines: " + String(linesCleared));
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(25, 105);
  tft.println("Great job!");
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 140);
  tft.println("Any button to return");
  
  while (true) {
    updateButtons();
    for (int i = 0; i < 7; i++) {
      if (buttons[i].pressed()) {
        currentState = STATE_MENU;
        drawMenu();
        return;
      }
    }
    delay(50);
  }
}

// ============== GRAPHICS DEMO ==============
void playGraphicsDemo() {
  currentState = STATE_PLAYING;
  
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(20, 10);
  tft.println("GRAPHICS DEMO");
  
  for (int i = 0; i < 8; i++) {
    uint16_t color = tft.color565(i * 32, i * 16, i * 32);
    tft.fillRect(i * 16, 30, 15, 20, color);
  }
  
  int ballX = 20, ballY = 60;
  int dx = 2, dy = 1;
  
  for (int frame = 0; frame < 200; frame++) {
    updateButtons();
    if (buttons[6].pressed()) break;
    
    tft.fillCircle(ballX, ballY, 5, COLOR_BG);
    
    ballX += dx;
    ballY += dy;
    
    if (ballX <= 5 || ballX >= 123) dx = -dx;
    if (ballY <= 55 || ballY >= 125) dy = -dy;
    
    tft.fillCircle(ballX, ballY, 5, COLOR_SELECTED);
    
    delay(50);
  }
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(10, 135);
  tft.println("Demo complete!");
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 150);
  tft.println("Any button to return");
  
  while (true) {
    updateButtons();
    for (int i = 0; i < 7; i++) {
      if (buttons[i].pressed()) {
        currentState = STATE_MENU;
        drawMenu();
        return;
      }
    }
    delay(50);
  }
}

void showSystemInfo() {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(25, 10);
  tft.println("SYSTEM INFO");
  
  tft.setTextColor(COLOR_TEXT);
  int y = 30;
  
  tft.setCursor(5, y);
  tft.println("ESP32 Game Console v2");
  y += 12;
  
  tft.setCursor(5, y);
  tft.println("Display: ST7735 128x160");
  y += 12;
  
  tft.setCursor(5, y);
  tft.println("Games: " + String(gameList.size()));
  y += 12;
  
  tft.setCursor(5, y);
  uint32_t freeHeap = ESP.getFreeHeap() / 1024;
  tft.println("Free RAM: " + String(freeHeap) + "KB");
  y += 12;
  
  tft.setCursor(5, y);
  tft.println("CPU Freq: " + String(ESP.getCpuFreqMHz()) + "MHz");
  y += 12;
  
  tft.setTextColor(COLOR_INFO);
  tft.setCursor(5, y);
  tft.println("Built-in Games: 4");
  y += 12;
  
  tft.setCursor(5, y);
  tft.println("- Snake (Working)");
  y += 10;
  
  tft.setCursor(5, y);
  tft.println("- Pong (Working)");
  y += 10;
  
  tft.setCursor(5, y);
  tft.println("- Tetris (Working)");
  y += 10;
  
  tft.setCursor(5, y);
  tft.println("- Demo (Working)");
  
  tft.setTextColor(COLOR_BORDER);
  tft.setCursor(5, 145);
  tft.println("Any button to return");
  
  while (true) {
    updateButtons();
    for (int i = 0; i < 7; i++) {
      if (buttons[i].pressed()) {
        drawMenu();
        return;
      }
    }
    delay(50);
  }
}

void handleSDDebugInput() {
  if (buttons[0].pressed()) {
    if (initSDAdvanced()) {
      loadGameList();
      currentState = STATE_MENU;
      drawMenu();
    }
  }
  
  if (buttons[6].pressed()) {
    currentState = STATE_MENU;
    drawMenu();
  }
}