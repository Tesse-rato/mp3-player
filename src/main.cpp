#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Audio.h>

// 'fill_heart', 8x8px
const unsigned char bmp_fill_heart [] PROGMEM = {
	0x66, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x18
};
// 'ouline_heart', 8x8px
const unsigned char bmp_ouline_heart [] PROGMEM = {
	0x66, 0x99, 0x81, 0x81, 0x81, 0x42, 0x24, 0x18
};
// 'pause', 8x8px
const unsigned char bmp_pause [] PROGMEM = {
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
};
// 'play', 8x8px
const unsigned char bmp_play [] PROGMEM = {
	0xe0, 0xf8, 0xfe, 0xff, 0xff, 0xfe, 0xf8, 0xe0
};
// 'replay', 8x8px
const unsigned char bmp_replay [] PROGMEM = {
	0x80, 0xc0, 0xff, 0x00, 0x00, 0xff, 0x06, 0x04
};
// 'random_all', 8x8px
const unsigned char bmp_random_all [] PROGMEM = {
	0xfc, 0x80, 0x86, 0x84, 0x21, 0x61, 0x01, 0x3f
};
// 'random_folder', 8x8px
const unsigned char bmp_random_folder [] PROGMEM = {
	0xe0, 0x80, 0xc6, 0x84, 0x81, 0x01, 0x01, 0x3f
};
// 'repeat', 8x8px
const unsigned char bmp_repeat [] PROGMEM = {
	0x18, 0x24, 0x02, 0x01, 0xc1, 0x82, 0x24, 0x18
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 256)
const int bmp_allArray_LEN = 8;
const unsigned char* bmp_allArray[8] = {
	bmp_fill_heart,
	bmp_ouline_heart,
	bmp_pause,
	bmp_play,
	bmp_random_all,
	bmp_random_folder,
	bmp_repeat,
	bmp_replay
};

// Tamanho da tela OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Pino de reset da tela OLED
#define SCREEN_ADDRESS 0x3C // Endereço do protocolo SPI para tela OLED

// Pinos para audio i2s
#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

#define SD_ROOT -1
#define FILE_ROOT 0
#define maxFileNameSize 128

#define letterWidth 6
#define letterHeight 8

#define displayLineOne 0
#define displayLineTwo 8
#define displayLineThree 16
#define displayLineFor 24
#define displayLineFive 32
#define displayLineSix 40
#define displayLineSeven 48
#define displayLineEight 56

#define AMP_REM_PIN 15

#define PLAY_PIN 13
#define FORWARD_PIN 12
#define BACKWARD_PIN 14
#define VOLUME_UP_PIN 33
#define VOLUME_DOWN_PIN 32

#define NO_BTN_EVENT 0
#define PLAY_PAUSE_SONG_EVENT 1
#define NEXT_SONG_EVENT 2
#define PREVIUS_SONG_EVENT 3
#define ROOT_SONG_EVENT 4
#define LAST_SONG_EVENT 5
#define NEXT_FOLDER_EVENT 6
#define PREVIUS_FOLDER_EVENT 7
#define VOLUME_UP_EVENT 8
#define VOLUME_DOWN_EVENT 9

uint8_t button_event = NO_BTN_EVENT;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Audio audio;

char **folders = NULL;
uint16_t folderCounter = 0;
int16_t folderIndex = SD_ROOT;
char **files = NULL;
uint16_t fileCounter = 0;
uint16_t fileIndex = 0;

char *currentFolder = NULL;
char *currentFile = NULL;
char *filePath = NULL;
char *extension = (char*)malloc(sizeof (char*) * 4);
bool pauseResumeStatus = 0;
uint8_t volume = 1;

int setUpSSD1306Display(void);
int setUpSdCard(void);
void readSdFolders(const char *path);
void readSdFiles(const char *path);
void setFileExtension(char*);
void updateDisplay(void);
void formatSeconds(char *timeBuffer, uint32_t seconds);
void checkPins(void);
void loadSD(uint16_t _fileIndex, int32_t _folderIndex);
void watchTrackPlaying(void);
void nextSong() { button_event = NEXT_SONG_EVENT; loadSD(fileIndex + 1, folderIndex); }
void nextFolder() { button_event = NEXT_FOLDER_EVENT; loadSD(FILE_ROOT, folderIndex + 1); }
void rootSong() { button_event = ROOT_SONG_EVENT; loadSD(FILE_ROOT, SD_ROOT); }
void playResume() { button_event = PLAY_PAUSE_SONG_EVENT; audio.pauseResume(); pauseResumeStatus = !pauseResumeStatus; }
void previusSong() { button_event = PREVIUS_SONG_EVENT; loadSD(fileIndex - 1, folderIndex); }
void previusFolder() { button_event = PREVIUS_FOLDER_EVENT; loadSD(FILE_ROOT, folderIndex - 1); }
void lastSong() { button_event = LAST_SONG_EVENT; loadSD(FILE_ROOT, folderCounter - 1); }
void mountSdStruct(void);

void setup() {
  Serial.begin(9600);
  pinMode(AMP_REM_PIN, OUTPUT);
  digitalWrite(AMP_REM_PIN, LOW);

  pinMode(PLAY_PIN, INPUT_PULLDOWN);
  pinMode(FORWARD_PIN, INPUT_PULLDOWN);
  pinMode(BACKWARD_PIN, INPUT_PULLDOWN);
  pinMode(VOLUME_UP_PIN, INPUT_PULLDOWN);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLDOWN);

  if(!setUpSSD1306Display()) return;
  if(!setUpSdCard()) return;
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume); // default 0...21
  
  loadSD(FILE_ROOT, SD_ROOT);
  mountSdStruct();
}



void mountSdStruct() {
  Serial.println("\nMONTANDO A ESTRUTURA DO CARTÃO SD;\n");

  struct Folder {
    char* name;
    char** files;
    uint16_t fileCounter;
  };

  File root = SD.open("/");
  File file = root.openNextFile();

  uint16_t folderCounter = 1;
  struct Folder *_folders = (struct Folder*)malloc(sizeof(struct Folder*));
  _folders[0].name = (char*)malloc(sizeof(char*) * 2);
  _folders[0].name[0] = '/';
  _folders[0].name[1] = '\0';

  for (int i = 0; file; i++) {
    char fileName[maxFileNameSize];
    strncpy(fileName, file.name(), maxFileNameSize);
    String _n = file.name();
    int nameSize = _n.length();

    if(!_n.startsWith(".")) {
      if(file.isDirectory()) {
        folderCounter++;
        _folders = (struct Folder*)realloc(_folders, sizeof (struct Folder*));
        _folders[folderCounter - 1].name = (char*)malloc(sizeof (char*) * (nameSize + 1));
        strcpy(_folders[folderCounter - 1].name, "/");
        strcat(_folders[folderCounter - 1].name, fileName);
      }
    }
    file = root.openNextFile();
  }

  for(uint16_t folder = 0; folder < folderCounter; folder++) {
    File root = SD.open(_folders[folder].name);
    File file = root.openNextFile();
    uint16_t fileCounter = 0;
    _folders[folder].files = (char**)malloc(sizeof(char**));
    for(int j = 0; file; j++) {
      String name = file.name();
      if(!file.isDirectory() && !name.startsWith(".")) {
        setFileExtension((char*)file.name());
        uint8_t mp3 = strcmp(extension, "mp3");
        uint8_t wav = strcmp(extension, "wav");
        if(mp3 == 0 || wav == 0) {
          fileCounter++;
          _folders[folder].files = (char**)realloc(_folders[folder].files, sizeof(char**) * fileCounter);
          if(folder == 0) {
            _folders[folder].files[fileCounter - 1] = (char*)malloc(sizeof(char*) * (name.length()));
            strcpy(_folders[folder].files[fileCounter - 1], file.name());
          }
          else {
            _folders[folder].files[fileCounter - 1] = (char*)malloc(sizeof(char*) * (name.length() + 1));
            strcpy(_folders[folder].files[fileCounter - 1], "/");
            strcat(_folders[folder].files[fileCounter - 1], file.name());
          }
        }
      }
      _folders[folder].fileCounter = fileCounter;
      file = root.openNextFile();
    }
  }

  for(int i = 0; i < folderCounter; i++) {
    struct Folder f = _folders[i];
    Serial.printf("*Folder: %s, File_Count %d\n", f.name, f.fileCounter);
    for(int j = 0; j < f.fileCounter; j++) {
      Serial.printf("\t*File: %s%s\n", f.name, f.files[j]);
    }
  }
}

void loop() {
  checkPins();
  updateDisplay();
  watchTrackPlaying();
  audio.loop();
}

int setUpSSD1306Display() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("ERR: SSD1306 Alocacao falhou!"));
    return 0;
  }
  display.display();
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextWrap(false);
  display.display();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  return 1;
}

int setUpSdCard() {
  display.println("Montando cartao SD...");
  Serial.println("\nMontando cartao SD...");
  
  if(!SD.begin(5)) {
    Serial.println("ERR: Cartao nao montado!");
    display.println(" ");
    display.println("Nao foi possivel");
    display.println("montar o leitor de");
    display.println("cartao SD!");
    display.display();
    return 0;
  }

  Serial.println("Cartao montado com sucesso!");
  display.println("SD com sucesso!");
  
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE) {
    Serial.println("Insira um cartao SD no leitor!");  
    display.println("Insira um cartao SD no leitor!");  
  } else if (cardType == CARD_MMC){
    Serial.println("\nTipo cartao: MMC");
    display.println("\nTipo cartao:  MMC");
  } else if(cardType == CARD_SD){
    Serial.println("\nTipo cartao: SDSC");
    display.println("\nTipo cartao:  SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("\nTipo cartao: SDHC");
    display.println("\nTipo cartao:  SDHC");
  } else {
    Serial.println("\nTipo cartao: UNKNOWN");
    display.println("\nTipo cartao:  UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t totalSize = SD.totalBytes() / (1024 * 1024);
  uint64_t usedSize = SD.usedBytes() / (1024 * 1024);
  Serial.printf("Tamanho SD: %lluMB\n", cardSize);
  display.printf("Tamanho SD:   %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", totalSize);
  display.printf("Espaco livre: %lluMB\n", totalSize);
  Serial.printf("Used space: %lluMB\n\n", usedSize);
  display.printf("Espaco usado: %lluMB\n\n", usedSize);
  display.display();
  return 1;
}

void readSdFolders(const char *path) {
  File root = SD.open(path);
  File file = root.openNextFile();
  folderCounter = 0;

  for (int i = 0; file; i++) {
    char fileName[maxFileNameSize];
    strncpy(fileName, file.name(), maxFileNameSize);
    String _n = file.name();
    int nameSize = _n.length();

    if(!_n.startsWith(".")) {
      if(file.isDirectory()) {
        folders = (char**)realloc(folders, sizeof (char**) * (++folderCounter));
        folders[folderCounter - 1] = (char*)malloc(sizeof(char*) * nameSize + 1);
        strcpy(folders[folderCounter - 1], fileName);
      }
    }
    file = root.openNextFile();
  }
}

void readSdFiles(const char *path) {
  File root = SD.open(path);
  File file = root.openNextFile();
  fileCounter = 0;

  for (int i = 0; file; i++) {
    char fileName[maxFileNameSize] = "";
    strncpy(fileName, file.name(), maxFileNameSize);
    String _n = file.name();
    int nameSize = _n.length();

    if(
      !_n.startsWith(".") && 
      !file.isDirectory()
    ) {
      setFileExtension(fileName);
      uint8_t isMp3 = strcmp(extension, "mp3");
      uint8_t isWav = strcmp(extension, "wav");
      if(isMp3 == 0 || isWav == 0) {
        fileCounter++;
        files = (char**)realloc(files, sizeof (char**) * (fileCounter));
        files[fileCounter - 1] = (char*)malloc(sizeof(char*) * nameSize + 1);
        strcpy(files[fileCounter - 1], fileName);
      }
    }

    file = root.openNextFile();
  }
}

/**
 * @arg _fileIndex // 0 ... MAX_uint16_t
 * @arg _folderIndex // -1 ... MAX_int16_t, Valor -1 significa a raiz da pasta
*/
void loadSD(uint16_t _fileIndex, int32_t _folderIndex) {
  
  if(_fileIndex < 0) return loadSD(0, _folderIndex);
  if(_folderIndex < -1) return loadSD(_fileIndex, -1);
  digitalWrite(AMP_REM_PIN, LOW);

  folderIndex = _folderIndex;
  fileIndex = _fileIndex;

  free(currentFolder);
  currentFolder = NULL;
  currentFolder = (char*)malloc(sizeof (char*) + 1);
  strcpy(currentFolder, "/");

  readSdFolders(currentFolder);

  if(folderIndex > SD_ROOT && folderCounter > 0) {
    free(currentFolder);
    currentFolder = NULL;
    currentFolder = (char*)malloc(sizeof (char*) * strlen(folders[folderIndex]) + 2);
    strcpy(currentFolder, "/");
    strcat(currentFolder, folders[folderIndex]);
    readSdFolders(currentFolder);
  }

  readSdFiles(currentFolder);
  
  if(!fileCounter && button_event == PREVIUS_FOLDER_EVENT) return loadSD(_fileIndex, _folderIndex - 1);
  if(!fileCounter && button_event == LAST_SONG_EVENT) return loadSD(_fileIndex, _folderIndex - 1);
  else if(!fileCounter) return loadSD(_fileIndex, _folderIndex + 1);
 
  free(currentFile);
  currentFile = NULL;
  free(filePath);
  filePath = NULL;

  currentFile = (char*)malloc(sizeof(char*) * strlen(files[fileIndex]));
  if(button_event == PREVIUS_FOLDER_EVENT) currentFile = files[fileIndex = fileCounter - 1];
  else if(button_event == LAST_SONG_EVENT) currentFile = files[fileIndex = fileCounter - 1];
  else currentFile = files[fileIndex];
  filePath = (char*)malloc(sizeof(char*) * (strlen(currentFile) + strlen(currentFolder) + 2));
  
  Serial.printf("Load SD\n\t_fileIndex: %d\n\t_folderIndex: %d\n\tbutton_event: %d\n\tcurrentFile: %s\n\tcurrentFolder: %s\n\n", _fileIndex, _folderIndex, button_event, currentFile, currentFolder);

  if(folderIndex > SD_ROOT) {
    // Se folderIndex for maior que SD_ROOT
    // quer dizer que o usuário abriu alguma pasta do SD
    // então é concatenado as barras no inincio e após o nome da pasta
    strcpy(filePath, currentFolder);
    strcat(filePath, "/");
    strcat(filePath, currentFile);
  }
  else {
    // Se o index das pastas for igual a raiz do SD
    // apenas concatena uma barra ao inicio do nome do arquivo
    strcpy(filePath, "/");
    strcat(filePath, currentFile);
  }

  if(!filePath || strlen(filePath) < 1) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Nao foi possivel");
    display.println("encontrar uma faixa");
    display.println("de música");
    display.display();
    for(;;);
  }
  else {
    audio.connecttoFS(SD, filePath);
    digitalWrite(AMP_REM_PIN, HIGH);
    pauseResumeStatus = 1;
  }
  button_event = NO_BTN_EVENT;
}

void setFileExtension(char *fileName) {
  // Remover do programa #1
  uint8_t count = 0;
  bool e = 0;
  for(uint8_t i = 0; i < strlen(fileName); i++) {
    if(e == 1) extension[count++] = fileName[i];
    else if(fileName[i] == '.') e = 1;
  }
  extension[3] = '\0';
}

void getFileExtension(char *buf, char *file) {
  uint8_t count = 0;
  bool e = 0;
  buf = (char*)malloc(sizeof(char*) + 1);
  for(uint8_t i = 0; i < strlen(file); i++) {
    if(e == 1) buf[count++] = file[i];
    else if(file[i] == '.') e = 1;
  }
  buf[3] = '\0';
}

uint32_t g_DisplayTime = millis();
uint32_t g_SerialTime = millis();
uint8_t y_offset = 0;
int16_t xPosName = -SCREEN_WIDTH;

void updateDisplay(void) {
  uint32_t crr_DisplayTime = millis();
  uint32_t crr_SerialTime = millis();
  uint8_t pixelSpeed = 5; // Number of pixel per cycle

  if((crr_DisplayTime - g_DisplayTime) > 250) {
    g_DisplayTime = crr_DisplayTime;

    uint16_t fileSize = strlen(currentFile);
    uint16_t maxXPosName = (letterWidth * fileSize);
    uint16_t audioFileDuration = audio.getAudioFileDuration();
    uint16_t audioCurrentTime = audio.getAudioCurrentTime();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.printf("%d de %d", fileIndex + 1, fileCounter);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(SCREEN_WIDTH - (11 * letterWidth), 0);
    char v[3];
    itoa(volume, v, 10);
    if (volume < 10) { v[1] = v[0]; v[0] = '0'; v[2] = '\0'; }
    display.printf("V:%s  ", v);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.printf(" %s \n", extension);
    display.setTextColor(SSD1306_WHITE);

    y_offset = 5;
    display.setCursor(0, displayLineTwo + y_offset);
    display.print("Pasta: ");
    display.println(currentFolder);

    y_offset = 10;
    display.setCursor(-xPosName, displayLineThree + y_offset);
    xPosName = xPosName + pixelSpeed;
    display.println(currentFile);
    if(xPosName > maxXPosName) xPosName = -SCREEN_WIDTH;

    y_offset = 15;
    char *played = (char*)malloc(sizeof(char*) * 9);
    formatSeconds(played, audioCurrentTime);
    display.setCursor(0, displayLineFor + y_offset);
    display.print(played);
    free(played);

    char *total = (char*)malloc(sizeof(char*) * 9);
    formatSeconds(total, audioFileDuration);
    display.setCursor(SCREEN_WIDTH - ((strlen(total))  * letterWidth), displayLineFor + y_offset);
    display.print(total);
    free(total);
    display.drawBitmap((SCREEN_WIDTH / 2) - 4 - 17, displayLineFor + y_offset, bmp_replay, 8, 8, SSD1306_WHITE);
    
    if(pauseResumeStatus) {
      display.drawBitmap((SCREEN_WIDTH / 2) - 4, displayLineFor + y_offset, bmp_pause, 8, 8, SSD1306_WHITE);
    }
    else {
      display.drawBitmap((SCREEN_WIDTH / 2) - 4, displayLineFor + y_offset, bmp_play, 8, 8, SSD1306_WHITE);
    }

    display.drawBitmap((SCREEN_WIDTH / 2) - 4 + 17, displayLineFor + y_offset, bmp_fill_heart, 8, 8, SSD1306_WHITE);

    if(audioCurrentTime != 0 && audioFileDuration != 0) {
      y_offset = 25;
      uint8_t circleRadius = 4;
      uint8_t maxWidth = SCREEN_WIDTH - (2 * circleRadius);
      float diff = (float)audioCurrentTime / (float)audioFileDuration;
      uint8_t circleXPos = (diff * maxWidth) + circleRadius;
      display.drawFastHLine(0, displayLineFive + y_offset, 127, SSD1306_WHITE);
      display.fillCircle(circleXPos, displayLineFive + y_offset, circleRadius, SSD1306_WHITE);
    }

    display.display();
  }
  if((crr_SerialTime - g_SerialTime) > 1000) {
    g_SerialTime = crr_SerialTime;
    
    // char *total;
    // char *played;
    // formatSeconds(total, audio.getAudioFileDuration());
    // formatSeconds(played, audio.getAudioCurrentTime());
    // Serial.printf("Total: %s -> played %s\n", total, played);
    // free(total);
    // free(played);
  }
}

void formatSeconds(char *timeBuffer, uint32_t seconds) {

  // timeBuffer = (char*)malloc(sizeof(char*) * 9);
  uint32_t h, m, s, resto;
  
  h = seconds / 3600;
  resto = seconds % 3600;
  m = resto / 60;
  s = resto % 60;

  char hour[3] = "00";
  char min[3] = "00";
  char sec[3] = "00";

  itoa(h, hour, 10);
  itoa(m, min, 10);
  itoa(s, sec, 10);

  if(h < 10) {
    hour[1] = hour[0];
    hour[0] = '0';
  }
  if(m < 10) {
    min[1] = min[0];
    min[0] = '0';
  }
  if(s < 10) {
    sec[1] = sec[0];
    sec[0] = '0';
  }

  if(h > 0) {
    timeBuffer[0] = hour[0];
    timeBuffer[1] = hour[1];
    timeBuffer[2] = ':';
    timeBuffer[3] = min[0];
    timeBuffer[4] = min[1];
    timeBuffer[5] = ':';
    timeBuffer[6] = sec[0];
    timeBuffer[7] = sec[1];
    timeBuffer[8] = '\0';
  }
  else {
    timeBuffer[0] = min[0];
    timeBuffer[1] = min[1];
    timeBuffer[2] = ':';
    timeBuffer[3] = sec[0];
    timeBuffer[4] = sec[1];
    timeBuffer[5] = '\0';
  }
}

uint32_t g_checkPinsTime = millis();
bool playPinPressed = 0;
bool forwardPinPressed = 0;
bool backwardPinPressed = 0;

void checkPins() {
  uint32_t crr_checkPinsTime = millis();
  if(crr_checkPinsTime - g_checkPinsTime > 200) {
    g_checkPinsTime = crr_checkPinsTime;
    bool playPin = digitalRead(PLAY_PIN);
    bool forwardPin = digitalRead(FORWARD_PIN);
    bool backwardPin = digitalRead(BACKWARD_PIN);
    bool volumeUpPin = digitalRead(VOLUME_UP_PIN);
    bool volumeDownPin = digitalRead(VOLUME_DOWN_PIN);
    
    if(forwardPin && !forwardPinPressed) {
      Serial.println("forwardPin");
      forwardPinPressed = 1;
      if(fileCounter > fileIndex + 1) nextSong();
      else if(folderCounter > folderIndex) nextFolder();
      else rootSong();
    }
    else if (!forwardPin && forwardPinPressed) forwardPinPressed = 0;

    if(playPin && !playPinPressed) {
      Serial.println("playPin");
      playPinPressed = 1;
      playResume();
    }
    else if (!playPin && playPinPressed) playPinPressed = 0;

    if(backwardPin && !backwardPinPressed) {
      Serial.println("backwardPin");
      backwardPinPressed = 1;
      if(fileIndex > FILE_ROOT) previusSong();
      else if (folderIndex > SD_ROOT) previusFolder();
      else lastSong();
    }
    else if (!backwardPin && backwardPinPressed) backwardPinPressed = 0;
    
    if(volumeUpPin && volume < 21) {
      Serial.println("Volume up");
      button_event = VOLUME_UP_EVENT;
      volume++;
      audio.setVolume(volume);
    }

    if(volumeDownPin && volume > 0) {
      Serial.println("Volume down");
      button_event = VOLUME_DOWN_EVENT;
      volume--;
      audio.setVolume(volume);
    }
    
    button_event = NO_BTN_EVENT;
  }
}

uint32_t lastAudioCurrentTime = 0;
uint32_t g_watchTrackPlaying = millis();
void watchTrackPlaying() {
  uint32_t crr_watchTrackPlaying = millis();
  if(pauseResumeStatus && crr_watchTrackPlaying - g_watchTrackPlaying > 1000) {
    g_watchTrackPlaying = millis();
    uint32_t audioCurrentTime = audio.getAudioCurrentTime();
    if(audioCurrentTime > 0) {
      if(lastAudioCurrentTime == audioCurrentTime) {
        if(fileCounter > fileIndex + 1) nextSong();
        else if(folderCounter > folderIndex) nextFolder();
        else rootSong();
      };
      lastAudioCurrentTime = audioCurrentTime;
    }
  }
}