#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <vector>
#include <WiFi.h>

// ---------------- DISPLAY PINS (ST7735) ----------------
#define TFT_CS    5
#define TFT_RST   4 
#define TFT_DC    2

// ---------------- SD CARD PINS ----------------
#define SD_CS     15
#define SD_MOSI   23
#define SD_MISO   19
#define SD_SCK    18

// ---------------- BUTTON PINS ----------------
#define BTN_UP    12
#define BTN_DOWN  14
#define BTN_LEFT  27
#define BTN_RIGHT 26
#define BTN_A     25
#define BTN_B     33
#define BTN_SELECT 32
#define BTN_START  13

// ---------------- DISPLAY SETUP ----------------
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ---------------- NES EMULATION CORE ----------------
// Basic 6502 CPU State
struct CPU6502 {
  uint16_t PC;        // Program Counter
  uint8_t A;          // Accumulator
  uint8_t X;          // X Register
  uint8_t Y;          // Y Register
  uint8_t SP;         // Stack Pointer
  uint8_t P;          // Status Register
  
  bool flag_C;        // Carry
  bool flag_Z;        // Zero
  bool flag_I;        // Interrupt Disable
  bool flag_D;        // Decimal Mode
  bool flag_B;        // Break
  bool flag_V;        // Overflow
  bool flag_N;        // Negative
};

// NES PPU (Picture Processing Unit) State
struct PPU {
  uint8_t ctrl;       // PPUCTRL ($2000)
  uint8_t mask;       // PPUMASK ($2001)
  uint8_t status;     // PPUSTATUS ($2002)
  uint8_t oamaddr;    // OAMADDR ($2003)
  uint8_t scroll_x;
  uint8_t scroll_y;
  uint16_t vram_addr;
  bool write_toggle;
  
  uint8_t oam[256];   // Object Attribute Memory
  uint8_t vram[2048]; // Video RAM
  uint8_t palette[32]; // Palette RAM
};

// NES System State
struct NESSystem {
  CPU6502 cpu;
  PPU ppu;
  uint8_t* rom_data;
  uint32_t rom_size;
  uint8_t* prg_rom;
  uint8_t* chr_rom;
  uint16_t prg_size;
  uint16_t chr_size;
  uint8_t mapper;
  uint8_t ram[2048];  // 2KB internal RAM
  
  // Controllers
  uint8_t controller1;
  uint8_t controller2;
  uint8_t controller1_shift;
  uint8_t controller2_shift;
  
  // Timing
  uint32_t cycles;
  uint32_t frame_count;
};

NESSystem nes;

// ---------------- NES ROM STRUCTURE ----------------
struct NESHeader {
  char signature[4];    // "NES\x1A"
  uint8_t prg_rom_size; // 16KB units
  uint8_t chr_rom_size; // 8KB units  
  uint8_t flags6;
  uint8_t flags7;
  uint8_t prg_ram_size; // 8KB units
  uint8_t flags9;
  uint8_t flags10;
  char padding[5];
};

// ---------------- GAME MANAGEMENT ----------------
struct RomFile {
  String filename;
  String displayName;
  size_t fileSize;
  bool isValid;
};

std::vector<RomFile> romList;
int selectedRomIndex = 0;
int displayStartIndex = 0;
const int MAX_DISPLAY_ROMS = 6;
bool sdCardInitialized = false;

// ---------------- BUTTON HANDLING ----------------
struct ButtonState {
  bool current = false;
  bool previous = false;
  bool pressed() { return current && !previous; }
  bool held() { return current; }
};

ButtonState buttons[8];
const int buttonPins[8] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_SELECT, BTN_START};

// ---------------- SYSTEM STATES ----------------
enum GameState {
  STATE_BOOT,
  STATE_MENU,
  STATE_LOADING,
  STATE_PLAYING,
  STATE_ERROR
};

GameState currentState = STATE_BOOT;
String errorMessage = "";

// ---------------- COLORS ----------------
#define COLOR_BLACK     0x0000
#define COLOR_BLUE      0x001F
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_WHITE     0xFFFF
#define COLOR_GRAY      0x8C71

// ---------------- FUNCTION DECLARATIONS ----------------
bool initializeDisplay();
bool initializeSDCard();
void initializeButtons();
void showBootScreen();
void scanSDCardForROMs();
bool isValidNESRom(File& romFile);
void updateButtons();
void handleMenuInput();
void drawMainMenu();
void drawLoadingScreen();
void drawErrorScreen();
bool loadROMFromSD(const String& filename);
void initNESSystem();
void resetNESSystem();
uint8_t nesRead(uint16_t address);
void nesWrite(uint16_t address, uint8_t value);
void executeNESFrame();
void renderNESFrame();
void updateControllers();
void cpuStep();
void ppuStep();
uint8_t cpu6502ReadByte(uint16_t address);
void cpu6502WriteByte(uint16_t address, uint8_t value);

// ---------------- SETUP FUNCTION ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("================================");
  Serial.println("ESP32 NES Emulator v5.0");
  Serial.println("================================");
  
  // Turn off WiFi to save power
  WiFi.mode(WIFI_OFF);
  btStop();
  
  // Initialize display
  if (!initializeDisplay()) {
    Serial.println("FATAL ERROR: Display initialization failed!");
    while(1) {
      delay(1000);
    }
  }
  
  showBootScreen();
  initializeButtons();
  
  // Initialize SD card and scan for ROMs
  if (initializeSDCard()) {
    scanSDCardForROMs();
    sdCardInitialized = true;
  } else {
    Serial.println("WARNING: SD Card initialization failed");
    sdCardInitialized = false;
  }
  
  // Initialize NES system
  initNESSystem();
  
  currentState = STATE_MENU;
  drawMainMenu();
  
  Serial.println("System initialization complete!");
}

// ---------------- MAIN LOOP ----------------
void loop() {
  updateButtons();
  
  switch (currentState) {
    case STATE_BOOT:
      delay(2000);
      currentState = STATE_MENU;
      drawMainMenu();
      break;
      
    case STATE_MENU:
      handleMenuInput();
      break;
      
    case STATE_LOADING:
      if (selectedRomIndex < romList.size()) {
        String romPath = "/NES/" + romList[selectedRomIndex].filename;
        if (loadROMFromSD(romPath)) {
          currentState = STATE_PLAYING;
          Serial.println("Starting NES emulation...");
        } else {
          currentState = STATE_ERROR;
          errorMessage = "Failed to load ROM!";
          drawErrorScreen();
        }
      }
      break;
      
    case STATE_PLAYING:
      updateControllers();
      executeNESFrame();
      renderNESFrame();
      
      // Check for exit
      if (buttons[6].pressed()) { // SELECT to exit
        currentState = STATE_MENU;
        drawMainMenu();
      }
      break;
      
    case STATE_ERROR:
      if (buttons[7].pressed()) { // START to return
        currentState = STATE_MENU;
        drawMainMenu();
      }
      break;
  }
  
  delay(1);
}

// ---------------- DISPLAY INITIALIZATION ----------------
bool initializeDisplay() {
  Serial.println("Initializing ST7735 display...");
  
  try {
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.fillScreen(COLOR_BLACK);
    
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.println("Display Test OK");
    delay(500);
    
    Serial.println("Display initialized successfully");
    return true;
  } catch (...) {
    Serial.println("Display initialization failed");
    return false;
  }
}

void showBootScreen() {
  tft.fillScreen(COLOR_BLACK);
  
  // Title
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  tft.setCursor(25, 15);
  tft.println("ESP32");
  
  tft.setTextColor(COLOR_YELLOW);
  tft.setCursor(10, 35);
  tft.println("NES EMU");
  
  // Version
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(40, 60);
  tft.println("v5.0");
  
  // Loading bar
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(30, 80);
  tft.println("Loading...");
  
  for (int i = 0; i < 10; i++) {
    tft.fillRect(14 + i * 10, 100, 8, 6, COLOR_GREEN);
    delay(100);
  }
}

// ---------------- SD CARD INITIALIZATION ----------------
bool initializeSDCard() {
  Serial.println("Initializing SD card...");
  
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);
  
  if (SD.begin(SD_CS)) {
    Serial.println("SD card initialized successfully");
    
    // Create NES directory if it doesn't exist
    if (!SD.exists("/NES")) {
      Serial.println("Creating /NES directory...");
      SD.mkdir("/NES");
    }
    return true;
  }
  
  Serial.println("SD card initialization failed");
  return false;
}

void scanSDCardForROMs() {
  Serial.println("Scanning for NES ROMs...");
  romList.clear();
  
  if (!sdCardInitialized) return;
  
  File nesDir = SD.open("/NES");
  if (!nesDir) {
    Serial.println("Cannot open /NES directory");
    return;
  }
  
  File file = nesDir.openNextFile();
  int romCount = 0;
  
  while (file && romCount < 50) { // Limit to prevent memory issues
    if (!file.isDirectory()) {
      String filename = file.name();
      String lowerName = filename;
      lowerName.toLowerCase();
      
      if (lowerName.endsWith(".nes")) {
        RomFile rom;
        rom.filename = filename;
        rom.fileSize = file.size();
        rom.displayName = filename;
        
        // Remove .nes extension for display
        if (rom.displayName.endsWith(".nes")) {
          rom.displayName = rom.displayName.substring(0, rom.displayName.length() - 4);
        }
        
        // Clean up display name
        rom.displayName.replace("_", " ");
        rom.displayName.replace("-", " ");
        rom.displayName.trim();
        
        // Validate ROM
        rom.isValid = (rom.fileSize >= 16) && isValidNESRom(file);
        
        romList.push_back(rom);
        romCount++;
        
        Serial.println("Found: " + rom.displayName + " (" + String(rom.fileSize) + " bytes) " + 
                      (rom.isValid ? "[VALID]" : "[INVALID]"));
      }
    }
    file = nesDir.openNextFile();
  }
  
  nesDir.close();
  Serial.println("ROM scan complete: " + String(romList.size()) + " ROMs found");
}

bool isValidNESRom(File& romFile) {
  if (romFile.size() < 16) return false;
  
  romFile.seek(0);
  uint8_t header[4];
  if (romFile.read(header, 4) != 4) return false;
  
  // Check NES signature
  return (header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A);
}

// ---------------- BUTTON HANDLING ----------------
void initializeButtons() {
  Serial.println("Initializing buttons...");
  
  for (int i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttons[i].current = false;
    buttons[i].previous = false;
  }
}

void updateButtons() {
  for (int i = 0; i < 8; i++) {
    buttons[i].previous = buttons[i].current;
    buttons[i].current = !digitalRead(buttonPins[i]);
  }
}

// ---------------- MENU HANDLING ----------------
void handleMenuInput() {
  if (buttons[0].pressed()) { // UP
    if (romList.size() > 0) {
      selectedRomIndex--;
      if (selectedRomIndex < 0) {
        selectedRomIndex = romList.size() - 1;
      }
      updateDisplayWindow();
      drawMainMenu();
    }
  }
  
  if (buttons[1].pressed()) { // DOWN
    if (romList.size() > 0) {
      selectedRomIndex++;
      if (selectedRomIndex >= romList.size()) {
        selectedRomIndex = 0;
      }
      updateDisplayWindow();
      drawMainMenu();
    }
  }
  
  if (buttons[4].pressed() || buttons[7].pressed()) { // A or START
    if (romList.size() > 0 && romList[selectedRomIndex].isValid) {
      currentState = STATE_LOADING;
      drawLoadingScreen();
    }
  }
}

void updateDisplayWindow() {
  if (selectedRomIndex < displayStartIndex) {
    displayStartIndex = selectedRomIndex;
  } else if (selectedRomIndex >= displayStartIndex + MAX_DISPLAY_ROMS) {
    displayStartIndex = selectedRomIndex - MAX_DISPLAY_ROMS + 1;
  }
}

void drawMainMenu() {
  tft.fillScreen(COLOR_BLACK);
  
  // Header
  tft.fillRect(0, 0, 128, 18, COLOR_BLUE);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 6);
  tft.println("NES EMULATOR");
  
  // Status line
  tft.setTextColor(COLOR_YELLOW);
  tft.setCursor(5, 22);
  if (sdCardInitialized) {
    tft.println("ROMs: " + String(romList.size()));
  } else {
    tft.println("SD: Not Ready");
  }
  
  if (romList.size() == 0) {
    // No ROMs found message
    tft.setTextColor(COLOR_RED);
    tft.setCursor(15, 50);
    tft.println("No ROMs Found!");
    
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(5, 70);
    tft.println("1. Insert SD card");
    tft.setCursor(5, 85);
    tft.println("2. Create /NES folder");
    tft.setCursor(5, 100);
    tft.println("3. Add .nes files");
    
  } else {
    // ROM list
    int yPos = 40;
    for (int i = 0; i < MAX_DISPLAY_ROMS && (displayStartIndex + i) < romList.size(); i++) {
      int romIndex = displayStartIndex + i;
      
      if (romIndex == selectedRomIndex) {
        tft.fillRect(2, yPos - 2, 124, 12, COLOR_RED);
        tft.setTextColor(COLOR_WHITE);
      } else {
        if (romList[romIndex].isValid) {
          tft.setTextColor(COLOR_WHITE);
        } else {
          tft.setTextColor(COLOR_GRAY);
        }
      }
      
      tft.setCursor(5, yPos);
      String displayName = romList[romIndex].displayName;
      if (displayName.length() > 18) {
        displayName = displayName.substring(0, 15) + "...";
      }
      tft.println(displayName);
      
      yPos += 13;
    }
  }
  
  // Controls
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(5, 138);
  tft.println("U/D:Select START:Play");
}

void drawLoadingScreen() {
  tft.fillScreen(COLOR_BLACK);
  
  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(25, 30);
  tft.println("LOADING ROM...");
  
  if (selectedRomIndex < romList.size()) {
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(5, 50);
    String romName = romList[selectedRomIndex].displayName;
    if (romName.length() > 18) {
      romName = romName.substring(0, 15) + "...";
    }
    tft.println(romName);
  }
  
  // Loading animation
  for (int i = 0; i < 8; i++) {
    tft.fillRect(12 + i * 13, 90, 10, 8, COLOR_GREEN);
    delay(150);
  }
}

void drawErrorScreen() {
  tft.fillScreen(COLOR_BLACK);
  
  tft.fillRect(0, 0, 128, 18, COLOR_RED);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(1);
  tft.setCursor(45, 6);
  tft.println("ERROR");
  
  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(5, 30);
  tft.println(errorMessage);
  
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(5, 150);
  tft.println("START: Return to Menu");
}

// ---------------- NES EMULATION CORE ----------------
void initNESSystem() {
  Serial.println("Initializing NES system...");
  memset(&nes, 0, sizeof(NESSystem));
  resetNESSystem();
}

void resetNESSystem() {
  // Reset CPU
  nes.cpu.PC = 0x8000;
  nes.cpu.SP = 0xFF;
  nes.cpu.P = 0x24; // IRQ disabled
  nes.cpu.A = 0;
  nes.cpu.X = 0;
  nes.cpu.Y = 0;
  
  // Reset PPU
  memset(&nes.ppu, 0, sizeof(PPU));
  
  // Clear RAM
  memset(nes.ram, 0, sizeof(nes.ram));
  
  // Reset controllers
  nes.controller1 = 0;
  nes.controller2 = 0;
  
  // Reset timing
  nes.cycles = 0;
  nes.frame_count = 0;
}

bool loadROMFromSD(const String& filename) {
  Serial.println("Loading ROM: " + filename);
  
  File romFile = SD.open(filename, FILE_READ);
  if (!romFile) {
    Serial.println("Cannot open ROM file: " + filename);
    return false;
  }
  
  size_t romSize = romFile.size();
  if (romSize < 16) {
    Serial.println("ROM file too small");
    romFile.close();
    return false;
  }
  
  // Read NES header
  NESHeader header;
  if (romFile.read((uint8_t*)&header, sizeof(NESHeader)) != sizeof(NESHeader)) {
    Serial.println("Failed to read ROM header");
    romFile.close();
    return false;
  }
  
  // Verify NES signature
  if (header.signature[0] != 'N' || header.signature[1] != 'E' || 
      header.signature[2] != 'S' || header.signature[3] != 0x1A) {
    Serial.println("Invalid NES ROM signature");
    romFile.close();
    return false;
  }
  
  // Calculate sizes
  nes.prg_size = header.prg_rom_size * 16384; // 16KB units
  nes.chr_size = header.chr_rom_size * 8192;  // 8KB units
  nes.mapper = (header.flags6 >> 4) | (header.flags7 & 0xF0);
  
  Serial.println("PRG ROM: " + String(nes.prg_size) + " bytes");
  Serial.println("CHR ROM: " + String(nes.chr_size) + " bytes");
  Serial.println("Mapper: " + String(nes.mapper));
  
  // Allocate memory for ROM data
  if (nes.rom_data) {
    free(nes.rom_data);
  }
  
  nes.rom_size = romSize;
  nes.rom_data = (uint8_t*)malloc(romSize);
  if (!nes.rom_data) {
    Serial.println("Cannot allocate memory for ROM");
    romFile.close();
    return false;
  }
  
  // Read entire ROM into memory
  romFile.seek(0);
  size_t bytesRead = romFile.read(nes.rom_data, romSize);
  romFile.close();
  
  if (bytesRead != romSize) {
    Serial.println("Failed to read complete ROM");
    free(nes.rom_data);
    nes.rom_data = nullptr;
    return false;
  }
  
  // Set pointers to PRG and CHR ROM
  nes.prg_rom = nes.rom_data + sizeof(NESHeader);
  nes.chr_rom = nes.prg_rom + nes.prg_size;
  
  // Reset system with new ROM
  resetNESSystem();
  
  // Set PC to reset vector
  if (nes.prg_size >= 4) {
    uint16_t reset_vector = nes.prg_rom[nes.prg_size - 4] | (nes.prg_rom[nes.prg_size - 3] << 8);
    nes.cpu.PC = reset_vector;
  }
  
  Serial.println("ROM loaded successfully");
  return true;
}

// ---------------- MEMORY MAPPING ----------------
uint8_t nesRead(uint16_t address) {
  if (address < 0x2000) {
    // Internal RAM (mirrored every 2KB)
    return nes.ram[address & 0x7FF];
  } else if (address < 0x4000) {
    // PPU registers (mirrored every 8 bytes)
    return ppuRead(address & 0x2007);
  } else if (address == 0x4016) {
    // Controller 1
    uint8_t value = nes.controller1_shift & 1;
    nes.controller1_shift >>= 1;
    return value;
  } else if (address == 0x4017) {
    // Controller 2
    uint8_t value = nes.controller2_shift & 1;
    nes.controller2_shift >>= 1;
    return value;
  } else if (address >= 0x8000) {
    // ROM area
    uint16_t rom_addr = address - 0x8000;
    if (nes.prg_size == 16384) {
      // 16KB ROM, mirror it
      rom_addr &= 0x3FFF;
    }
    if (rom_addr < nes.prg_size) {
      return nes.prg_rom[rom_addr];
    }
  }
  
  return 0; // Default return for unmapped addresses
}

void nesWrite(uint16_t address, uint8_t value) {
  if (address < 0x2000) {
    // Internal RAM (mirrored every 2KB)
    nes.ram[address & 0x7FF] = value;
  } else if (address < 0x4000) {
    // PPU registers (mirrored every 8 bytes)
    ppuWrite(address & 0x2007, value);
  } else if (address == 0x4016) {
    // Controller strobe
    if (value & 1) {
      nes.controller1_shift = nes.controller1;
      nes.controller2_shift = nes.controller2;
    }
  }
  // ROM area is read-only, ignore writes
}

uint8_t ppuRead(uint16_t address) {
  switch (address) {
    case 0x2002: // PPUSTATUS
      {
        uint8_t status = nes.ppu.status;
        nes.ppu.status &= 0x7F; // Clear VBlank flag
        nes.ppu.write_toggle = false;
        return status;
      }
    case 0x2004: // OAMDATA
      return nes.ppu.oam[nes.ppu.oamaddr];
    case 0x2007: // PPUDATA
      // Simplified VRAM read
      return 0;
    default:
      return 0;
  }
}

void ppuWrite(uint16_t address, uint8_t value) {
  switch (address) {
    case 0x2000: // PPUCTRL
      nes.ppu.ctrl = value;
      break;
    case 0x2001: // PPUMASK
      nes.ppu.mask = value;
      break;
    case 0x2003: // OAMADDR
      nes.ppu.oamaddr = value;
      break;
    case 0x2004: // OAMDATA
      nes.ppu.oam[nes.ppu.oamaddr] = value;
      nes.ppu.oamaddr++;
      break;
    case 0x2005: // PPUSCROLL
      if (!nes.ppu.write_toggle) {
        nes.ppu.scroll_x = value;
      } else {
        nes.ppu.scroll_y = value;
      }
      nes.ppu.write_toggle = !nes.ppu.write_toggle;
      break;
    case 0x2006: // PPUADDR
      if (!nes.ppu.write_toggle) {
        nes.ppu.vram_addr = (nes.ppu.vram_addr & 0x00FF) | (value << 8);
      } else {
        nes.ppu.vram_addr = (nes.ppu.vram_addr & 0xFF00) | value;
      }
      nes.ppu.write_toggle = !nes.ppu.write_toggle;
      break;
    case 0x2007: // PPUDATA
      // Simplified VRAM write
      break;
  }
}

// ---------------- CONTROLLER INPUT ----------------
void updateControllers() {
  nes.controller1 = 0;
  
  if (buttons[3].held()) nes.controller1 |= 0x01; // Right
  if (buttons[2].held()) nes.controller1 |= 0x02; // Left
  if (buttons[1].held()) nes.controller1 |= 0x04; // Down
  if (buttons[0].held()) nes.controller1 |= 0x08; // Up
  if (buttons[7].held()) nes.controller1 |= 0x10; // Start
  if (buttons[6].held()) nes.controller1 |= 0x20; // Select
  if (buttons[5].held()) nes.controller1 |= 0x40; // B
  if (buttons[4].held()) nes.controller1 |= 0x80; // A
}

// ---------------- CPU EMULATION ----------------
void executeNESFrame() {
  // Execute approximately 29780 CPU cycles per frame (NTSC timing)
  for (int cycles = 0; cycles < 29780 && currentState == STATE_PLAYING; cycles++) {
    cpuStep();
    
    // Simple PPU timing - 3 PPU cycles per CPU cycle
    ppuStep();
    ppuStep();
    ppuStep();
  }
  
  nes.frame_count++;
  
  // Set VBlank flag
  nes.ppu.status |= 0x80;
}

void cpuStep() {
  if (!nes.rom_data) return;
  
  // Fetch instruction
  uint8_t opcode = nesRead(nes.cpu.PC);
  nes.cpu.PC++;
  
  // Execute instruction (simplified instruction set)
  switch (opcode) {
    case 0xA9: // LDA immediate
      {
        uint8_t value = nesRead(nes.cpu.PC++);
        nes.cpu.A = value;
        nes.cpu.flag_Z = (value == 0);
        nes.cpu.flag_N = (value & 0x80) != 0;
      }
      break;
      
    case 0xAD: // LDA absolute
      {
        uint16_t addr = nesRead(nes.cpu.PC) | (nesRead(nes.cpu.PC + 1) << 8);
        nes.cpu.PC += 2;
        uint8_t value = nesRead(addr);
        nes.cpu.A = value;
        nes.cpu.flag_Z = (value == 0);
        nes.cpu.flag_N = (value & 0x80) != 0;
      }
      break;
      
    case 0x8D: // STA absolute
      {
        uint16_t addr = nesRead(nes.cpu.PC) | (nesRead(nes.cpu.PC + 1) << 8);
        nes.cpu.PC += 2;
        nesWrite(addr, nes.cpu.A);
      }
      break;
      
    case 0xA2: // LDX immediate
      {
        uint8_t value = nesRead(nes.cpu.PC++);
        nes.cpu.X = value;
        nes.cpu.flag_Z = (value == 0);
        nes.cpu.flag_N = (value & 0x80) != 0;
      }
      break;
      
    case 0xA0: // LDY immediate
      {
        uint8_t value = nesRead(nes.cpu.PC++);
        nes.cpu.Y = value;
        nes.cpu.flag_Z = (value == 0);
        nes.cpu.flag_N = (value & 0x80) != 0;
      }
      break;
      
    case 0x4C: // JMP absolute
      {
        uint16_t addr = nesRead(nes.cpu.PC) | (nesRead(nes.cpu.PC + 1) << 8);
        nes.cpu.PC = addr;
      }
      break;
      
    case 0x10: // BPL relative
      {
        int8_t offset = (int8_t)nesRead(nes.cpu.PC++);
        if (!nes.cpu.flag_N) {
          nes.cpu.PC += offset;
        }
      }
      break;
      
    case 0x30: // BMI relative
      {
        int8_t offset = (int8_t)nesRead(nes.cpu.PC++);
        if (nes.cpu.flag_N) {
          nes.cpu.PC += offset;
        }
      }
      break;
      
    case 0xF0: // BEQ relative
      {
        int8_t offset = (int8_t)nesRead(nes.cpu.PC++);
        if (nes.cpu.flag_Z) {
          nes.cpu.PC += offset;
        }
      }
      break;
      
    case 0xD0: // BNE relative
      {
        int8_t offset = (int8_t)nesRead(nes.cpu.PC++);
        if (!nes.cpu.flag_Z) {
          nes.cpu.PC += offset;
        }
      }
      break;
      
    case 0xE8: // INX
      nes.cpu.X++;
      nes.cpu.flag_Z = (nes.cpu.X == 0);
      nes.cpu.flag_N = (nes.cpu.X & 0x80) != 0;
      break;
      
    case 0xCA: // DEX
      nes.cpu.X--;
      nes.cpu.flag_Z = (nes.cpu.X == 0);
      nes.cpu.flag_N = (nes.cpu.X & 0x80) != 0;
      break;
      
    case 0xC8: // INY
      nes.cpu.Y++;
      nes.cpu.flag_Z = (nes.cpu.Y == 0);
      nes.cpu.flag_N = (nes.cpu.Y & 0x80) != 0;
      break;
      
    case 0x88: // DEY
      nes.cpu.Y--;
      nes.cpu.flag_Z = (nes.cpu.Y == 0);
      nes.cpu.flag_N = (nes.cpu.Y & 0x80) != 0;
      break;
      
    case 0x18: // CLC
      nes.cpu.flag_C = false;
      break;
      
    case 0x38: // SEC
      nes.cpu.flag_C = true;
      break;
      
    case 0x58: // CLI
      nes.cpu.flag_I = false;
      break;
      
    case 0x78: // SEI
      nes.cpu.flag_I = true;
      break;
      
    case 0xEA: // NOP
      // Do nothing
      break;
      
    case 0x00: // BRK
      // For simplicity, treat as infinite loop
      nes.cpu.PC--;
      break;
      
    default:
      // Unknown opcode - skip
      Serial.println("Unknown opcode: 0x" + String(opcode, HEX) + " at PC: 0x" + String(nes.cpu.PC - 1, HEX));
      break;
  }
  
  nes.cycles++;
}

void ppuStep() {
  // Simplified PPU - just handle VBlank timing
  static int ppu_cycle = 0;
  ppu_cycle++;
  
  // Clear VBlank after some time
  if (ppu_cycle > 1000) {
    nes.ppu.status &= 0x7F;
    ppu_cycle = 0;
  }
}

// ---------------- GRAPHICS RENDERING ----------------
void renderNESFrame() {
  // Clear screen
  tft.fillScreen(COLOR_BLACK);
  
  // Display game status
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.println("NES RUNNING");
  
  // Display ROM info
  tft.setCursor(5, 20);
  if (selectedRomIndex < romList.size()) {
    String romName = romList[selectedRomIndex].displayName;
    if (romName.length() > 16) {
      romName = romName.substring(0, 13) + "...";
    }
    tft.println(romName);
  }
  
  // CPU state display
  tft.setTextColor(COLOR_CYAN);
  tft.setCursor(5, 35);
  tft.println("PC:" + String(nes.cpu.PC, HEX));
  tft.setCursor(65, 35);
  tft.println("A:" + String(nes.cpu.A, HEX));
  
  tft.setCursor(5, 50);
  tft.println("X:" + String(nes.cpu.X, HEX));
  tft.setCursor(35, 50);
  tft.println("Y:" + String(nes.cpu.Y, HEX));
  tft.setCursor(65, 50);
  tft.println("SP:" + String(nes.cpu.SP, HEX));
  
  // Frame counter
  tft.setTextColor(COLOR_YELLOW);
  tft.setCursor(5, 65);
  tft.println("Frame:" + String(nes.frame_count));
  
  // Simple graphics area (simulated NES screen)
  tft.drawRect(4, 80, 120, 60, COLOR_WHITE);
  
  // Simulate some basic graphics based on CPU state
  for (int y = 0; y < 6; y++) {
    for (int x = 0; x < 12; x++) {
      uint16_t color = COLOR_BLACK;
      
      // Create pattern based on CPU registers and memory
      uint8_t pattern = (nes.cpu.A + nes.cpu.X + nes.cpu.Y + x + y + (nes.frame_count % 16)) & 0xFF;
      
      if (pattern & 0x80) color = COLOR_RED;
      else if (pattern & 0x40) color = COLOR_GREEN;
      else if (pattern & 0x20) color = COLOR_BLUE;
      else if (pattern & 0x10) color = COLOR_YELLOW;
      else if (pattern & 0x08) color = COLOR_CYAN;
      else if (pattern & 0x04) color = COLOR_MAGENTA;
      
      if (color != COLOR_BLACK) {
        tft.fillRect(6 + x * 10, 82 + y * 9, 8, 7, color);
      }
    }
  }
  
  // Controller input display
  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(5, 145);
  String input = "";
  if (buttons[0].held()) input += "U";
  if (buttons[1].held()) input += "D";
  if (buttons[2].held()) input += "L";
  if (buttons[3].held()) input += "R";
  if (buttons[4].held()) input += "A";
  if (buttons[5].held()) input += "B";
  if (buttons[6].held()) input += "S";
  if (buttons[7].held()) input += "T";
  
  if (input.length() > 0) {
    tft.println("Input: " + input);
  } else {
    tft.println("SELECT: Exit");
  }
}