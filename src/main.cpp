/**
 * Projeto de MP3 player com controle remoto via rádio
 * Desenvolvido por Bruno França
 * 
 * Pinos usados:
 *  Pino remote:      15; #define AMP_REM_PIN 15
 *  Pino Play/Pause:  13; #define PLAY_PIN 13
 *  Pino Próximo:     12; #define FORWARD_PIN 12
 *  Pino Anterior:    14; #define BACKWARD_PIN 14
 *  Pino Volume+:     33; #define VOLUME_UP_PIN 33
 *  Pino Volume-:     32; #define VOLUME_DOWN_PIN 32
 *  Pino Repeat:      04; #define REPEAT_PIN 4
 *
 *  Pino i2s: I2S_DOUT  25
 *  Pino i2s: I2S_BCLK  27
 *  Pino i2s: I2S_LRC   26
 * 
 *  Pino i2c SCL:   22
 *  Pino i2c SDA:   21
 * 
 *  Pino SPI MOSI:  23
 *  Pino SPI MISO:  19
 *  Pino SPI SCK:   18
 *  Pino SPI SS:    5
 * 
 * Bugs e anotacoes
 * OK - Tem um bug em modo aleatorio na pasta, ao voltar a faixa backfoard button
*/

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Audio.h>

// 'fill_heart', 8x8px - Música curtida
const unsigned char bmp_fill_heart [] PROGMEM = {
	0x66, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c, 0x18
};
// 'ouline_heart', 8x8px - Música não curtida
const unsigned char bmp_ouline_heart [] PROGMEM = {
	0x66, 0x99, 0x81, 0x81, 0x81, 0x42, 0x24, 0x18
};
// 'pause', 8x8px - Música parada
const unsigned char bmp_pause [] PROGMEM = {
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
};
// 'play', 8x8px - Reproduzindo
const unsigned char bmp_play [] PROGMEM = {
	0xe0, 0xf8, 0xfe, 0xff, 0xff, 0xfe, 0xf8, 0xe0
};
// 'replay', 8x8px - Ao final do numero de música, retorna a primeira musica
const unsigned char bmp_replay [] PROGMEM = {
	0x80, 0xc0, 0xff, 0x00, 0x00, 0xff, 0x06, 0x04
};
// 'random_all', 8x8px - Aleatória para todas músicas
const unsigned char bmp_random_all [] PROGMEM = {
	0xfc, 0x80, 0x86, 0x84, 0x21, 0x61, 0x01, 0x3f
};
// 'random_folder', 8x8px - Aleatório dentro de uma pasta
const unsigned char bmp_random_folder [] PROGMEM = {
	0xe0, 0x80, 0xc6, 0x84, 0x81, 0x01, 0x01, 0x3f
};
// 'repeat', 8x8px - Repete a mesma música
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

#define SD_ROOT 0
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

#define AMP_REM_PIN 33

#define PLAY_PIN 13
#define FORWARD_PIN 12
#define BACKWARD_PIN 14
#define VOLUME_UP_PIN 39
#define VOLUME_DOWN_PIN 34
#define REPEAT_PIN 4
#define HC12_SET_PIN 32

#define NO_BTN_EVENT 0
#define PLAY_PAUSE_SONG_EVENT 1
#define NEXT_SONG_EVENT 2
#define PREVIUS_SONG_EVENT 3
// #define ROOT_SONG_EVENT 4
// #define LAST_SONG_EVENT 5
// #define NEXT_FOLDER_EVENT 6
// #define PREVIUS_FOLDER_EVENT 7
#define VOLUME_UP_EVENT 8
#define VOLUME_DOWN_EVENT 9
#define RANDOM_EVENT 10
#define MAIN_MENU_EVENT 11

#define RANDOM_NORMAL 0
#define RANDOM_IN_FOLDER 1
#define RANDOM_ALL_SONGS 2
#define REPEAT_SONG 3

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Audio audio;
HardwareSerial HC12 = Serial2;
TaskHandle_t radioTaskHandler;
uint8_t RadioButtonEvent = NO_BTN_EVENT;

struct Folder {
  char* name;
  char** files;
  uint16_t fileCounter = 0;
  uint16_t *randomFileStack;
  int16_t randomFileStackPos = 0;
};
struct Folder *folders;
uint16_t folderCounter = 0;
int16_t folderIndex = SD_ROOT;
uint16_t fileIndex = FILE_ROOT;
uint16_t **randomFileStack = NULL;
uint16_t randomFileStackPos = 0;
uint32_t randomFileStackSize = 0;

char *extension = (char*)malloc(sizeof (char*) * 4); // REMOVER DO PROGRAMA
bool pauseResumeStatus = 0; // 1 -> Play; 0 -> Pause
uint8_t volume = 2;
uint8_t randomMode = RANDOM_NORMAL;
uint8_t button_event = NO_BTN_EVENT;

int setUpSSD1306Display(void);
int setUpSdCard(void);
void setFileExtension(char*);
void updateDisplay(void);
void formatSeconds(char *timeBuffer, uint32_t seconds);
void checkHardwarePins(void);
void checkRadioPins(void);
void loadSD(int16_t _fileIndex, int16_t _folderIndex);
void watchTrackPlaying(void);
void playResume() { button_event = PLAY_PAUSE_SONG_EVENT; audio.pauseResume(); pauseResumeStatus = !pauseResumeStatus; }
void mountSdStruct(void);
void initRandomFileStack(void);
void nextSong(void);
void previusSong(void);
void volumeUp(void);
void volumeDown(void);
void changeRandomMode(void);
void setUpRadioTransmitter(void);
void radioLoop(void* pvParameters);
void runRadioCommands(String command);


void setup() {
  Serial.begin(9600);
  HC12.begin(9600);

  pinMode(HC12_SET_PIN, OUTPUT);
  pinMode(AMP_REM_PIN, OUTPUT);
  
  digitalWrite(AMP_REM_PIN, LOW);
  digitalWrite(HC12_SET_PIN, LOW);
  delay(80);
  digitalWrite(AMP_REM_PIN, HIGH);
  // setUpRadioTransmitter();
 
  pinMode(PLAY_PIN, INPUT_PULLDOWN);
  pinMode(FORWARD_PIN, INPUT_PULLDOWN);
  pinMode(BACKWARD_PIN, INPUT_PULLDOWN);
  pinMode(VOLUME_UP_PIN, INPUT_PULLDOWN);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLDOWN);
  pinMode(REPEAT_PIN, INPUT_PULLDOWN);

  if(!setUpSSD1306Display()) return;
  if(!setUpSdCard()) return;

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume); // default 0...21
  
  mountSdStruct();
  initRandomFileStack();
  loadSD(SD_ROOT, FILE_ROOT);

  xTaskCreatePinnedToCore(
    radioLoop,
    "Radio-Task",
    1000,
    NULL,
    0,
    &radioTaskHandler,
    PRO_CPU_NUM
  );
}

void loop(){
  checkHardwarePins();
  checkRadioPins();
  updateDisplay();
  watchTrackPlaying();
  audio.loop();
};

void radioLoop(void* pvParameters) {
  for(;;) {
    if(Serial.available()) {
      Serial.printf("Serial-PC");
      HC12.print(Serial.readString());
    }
    if(HC12.available()) {
      // Serial.printf("TAM: %d -> ", HC12.available());
      
      String conteudo = "";
      char caractere;

      while(HC12.available() > 0) {
        caractere = HC12.read();
        if (caractere != '\n' && caractere != '\r'){
          conteudo.concat(caractere);
        }
        delay(10);
      }

      // Serial.printf("Recebido no radio: %s\n", conteudo.c_str());

      if(!strcmp("NEXT_SONG", conteudo.c_str())) {
        // Serial.printf("\tNEXT_SONG_BUTTON()\n");
        RadioButtonEvent = NEXT_SONG_EVENT;
      }
      if(!strcmp("PREVIUS_SONG", conteudo.c_str())) {
        // Serial.printf("\tPREVIUS_SONG_BUT()\n");
        RadioButtonEvent = PREVIUS_SONG_EVENT;
      }
      if(!strcmp("VOL_U`", conteudo.c_str())) {
        // Serial.printf("\tVOLUME_UP_BUTTON()\n");
        RadioButtonEvent = VOLUME_UP_EVENT;
      }
      if(!strcmp("VOL_D", conteudo.c_str())) {
        // Serial.printf("\tVOLUME_DOWN_BUTT()\n");
        RadioButtonEvent = VOLUME_DOWN_EVENT;
      }
      if(!strcmp("RANDOM_MODE", conteudo.c_str())) {
        // Serial.printf("\tRANDOM_MODE_BUTT()\n");
        RadioButtonEvent = RANDOM_EVENT;
      }
      if(!strcmp("PLAY_PAUSE", conteudo.c_str())) {
        // Serial.printf("\tPLAY_PAUSE_BUTTO()\n");
        RadioButtonEvent = PLAY_PAUSE_SONG_EVENT;
      }
      if(!strcmp("MAIN_MENU", conteudo.c_str())) {
        // Serial.printf("\tMAIN_MENU_BUTTON()\n");
        RadioButtonEvent = MAIN_MENU_EVENT;
      }
      HC12.flush();
    }
    vTaskDelay(80);
  }
}

void mountSdStruct() {
  File root = SD.open("/");
  File file = root.openNextFile();

  char **foldersArray = (char **)malloc(sizeof(char **));
  folderCounter = 1;
  foldersArray[0] = (char*)malloc(sizeof(char*) * 2);
  strcpy(foldersArray[0], "/");

  for(uint16_t i = 0; file; i++) {
    String name = file.name();
    if(file.isDirectory() && !name.startsWith(".")) {
      foldersArray = (char**)realloc(foldersArray, sizeof(char**) * ++folderCounter);
      foldersArray[folderCounter - 1] = (char*)malloc(sizeof(char*) * (name.length() + 1));
      strcpy(foldersArray[folderCounter - 1], file.name());
    }
    file = root.openNextFile();
  }

  free(folders);
  folders = (struct Folder*)malloc(sizeof(struct Folder*) * (folderCounter + 1));

  for(uint16_t i = 0; i < folderCounter; i++) {
    folders[i].name = (char*)malloc(sizeof(char*) * (strlen(foldersArray[i]) + 1));
    strcpy(folders[i].name, "");
    if(i > 0) strcat(folders[i].name, "/");
    strcat(folders[i].name, foldersArray[i]);
  }

  free(foldersArray);
  foldersArray = NULL;

  for(uint16_t folder = 0; folder < folderCounter; folder++) {
    File root = SD.open(folders[folder].name);
    File file = root.openNextFile();
    folders[folder].files = (char**)malloc(sizeof(char**));
    folders[folder].fileCounter = 0;
    uint16_t fc = 0;
    for(int j = 0; file; j++) {
      String name = file.name();
      if(!file.isDirectory() && !name.startsWith(".")) {
        setFileExtension((char*)file.name());

        if(
          strcmp(extension, "mp3") == 0 ||
          strcmp(extension, "wav") == 0 ||
          strcmp(extension, "aac") == 0 ||
          strcmp(extension, "m4a") == 0
        ) {
          fc++;
          folders[folder].files = (char**)realloc(folders[folder].files, sizeof(char**) * fc);
          if(folder == 0) {
            folders[folder].files[fc - 1] = (char*)malloc(sizeof(char*) * (name.length()));
            strcpy(folders[folder].files[fc - 1], file.name());
          }
          else {
            folders[folder].files[fc - 1] = (char*)malloc(sizeof(char*) * (name.length() + 1));
            strcpy(folders[folder].files[fc - 1], "/");
            strcat(folders[folder].files[fc - 1], file.name());
          }
        }
      }
      file = root.openNextFile();
    }
    folders[folder].fileCounter = fc;
    folders[folder].randomFileStack = (uint16_t*)malloc(sizeof(uint16_t*) * fc);
    folders[folder].randomFileStackPos = 0;
    for(uint16_t i = 0; i < fc; i++) {
      folders[folder].randomFileStack[i] = i;
    }
    for(uint16_t i = 0; i < fc; i++) {
      uint16_t a = random(fc);
      uint16_t b = random(fc);
      if(a == b) b = random(fc);
      uint16_t tmp = folders[folder].randomFileStack[a];
      folders[folder].randomFileStack[a] = folders[folder].randomFileStack[b];
      folders[folder].randomFileStack[b] = tmp;
    }
  }
}

void initRandomFileStack() {
  uint16_t c = 0;
  free(randomFileStack);
  randomFileStack = (uint16_t**) malloc(sizeof (uint16_t**));

  for(uint16_t i = 0; i < folderCounter; i++) {
    for(uint16_t j = 0; j < folders[i].fileCounter; j++) {
      randomFileStack = (uint16_t**) realloc(randomFileStack, sizeof (uint16_t**) * (c + 1));
      randomFileStack[c] = (uint16_t*) malloc(sizeof (uint16_t*) * 2);
      randomFileStack[c][0] = i;
      randomFileStack[c][1] = j;
      c++;
    }
  }

  for(uint16_t i = 0; i < c; i ++) {
    uint16_t *tmp;
    uint16_t a, b;
    a = random(c);
    b = random(c);
    if(a == b) b = random(c);
    tmp = randomFileStack[a];
    randomFileStack[a] = randomFileStack[b];
    randomFileStack[b] = tmp;
  }

  randomFileStackSize = c;
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

void loadSD(int16_t _fileIndex, int16_t _folderIndex) {
  if(
    _folderIndex > folderCounter - 1 ||
    _folderIndex < 0
  ){
    printf("Tentando acessar pasta que não existe");
    return;
  }

  digitalWrite(AMP_REM_PIN, LOW);
  folderIndex = _folderIndex;
  fileIndex = _fileIndex;

  struct Folder folder = folders[folderIndex];

  char* file = folder.files[fileIndex];
  char* path = (char*)malloc(sizeof (char*) * (strlen(folder.name) + strlen(file)));
  strcpy(path, folder.name);
  strcat(path, file);

  audio.connecttoFS(SD, (const char*)path);
  digitalWrite(AMP_REM_PIN, HIGH);
  pauseResumeStatus = 1;

  free(path);
  path = NULL;
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

    uint16_t fileSize = strlen(folders[folderIndex].files[fileIndex]);
    uint16_t maxXPosName = (letterWidth * fileSize);
    uint16_t audioFileDuration = audio.getAudioFileDuration();
    uint16_t audioCurrentTime = audio.getAudioCurrentTime();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.printf("%d de %d", fileIndex + 1, folders[folderIndex].fileCounter);
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
    display.println(folders[folderIndex].name);

    y_offset = 10;
    display.setCursor(-xPosName, displayLineThree + y_offset);
    xPosName = xPosName + pixelSpeed;
    display.println(folders[folderIndex].files[fileIndex]);
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

    if(randomMode == RANDOM_NORMAL)
      display.drawBitmap((SCREEN_WIDTH / 2) - 4 - 17, displayLineFor + y_offset, bmp_replay, 8, 8, SSD1306_WHITE);
    if(randomMode == RANDOM_IN_FOLDER)
      display.drawBitmap((SCREEN_WIDTH / 2) - 4 - 17, displayLineFor + y_offset, bmp_random_folder, 8, 8, SSD1306_WHITE);
    if(randomMode == RANDOM_ALL_SONGS)
      display.drawBitmap((SCREEN_WIDTH / 2) - 4 - 17, displayLineFor + y_offset, bmp_random_all, 8, 8, SSD1306_WHITE);
    if(randomMode == REPEAT_SONG)
      display.drawBitmap((SCREEN_WIDTH / 2) - 4 - 17, displayLineFor + y_offset, bmp_repeat, 8, 8, SSD1306_WHITE);
    
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

void nextSong() { 
  button_event = NEXT_SONG_EVENT;

  if(randomMode == RANDOM_NORMAL) {
    if(fileIndex + 1 > folders[folderIndex].fileCounter - 1) {
      if(!folders[folderIndex + 1].fileCounter) {
        if(folderIndex + 2 > folderCounter - 1) loadSD(FILE_ROOT, SD_ROOT);
        else loadSD(FILE_ROOT, folderIndex + 2);
      }
      else if(folderIndex + 1 > folderCounter - 1) loadSD(FILE_ROOT, SD_ROOT);
      else loadSD(FILE_ROOT, folderIndex + 1);
    }
    else loadSD(fileIndex + 1, folderIndex);
  }
  else if(randomMode == REPEAT_SONG) loadSD(fileIndex, folderIndex);
  else if (randomMode == RANDOM_IN_FOLDER) {
    folders[folderIndex].randomFileStackPos++;
    if(folders[folderIndex].randomFileStackPos > folders[folderIndex].fileCounter - 1) {
      folders[folderIndex].randomFileStackPos = 0;
    }
    uint16_t pos = folders[folderIndex].randomFileStackPos;
    loadSD(folders[folderIndex].randomFileStack[pos], folderIndex);
  }
  else if(randomMode == RANDOM_ALL_SONGS) {
    if(randomFileStackPos >= randomFileStackSize - 1) randomFileStackPos = 0;
    else randomFileStackPos += 1;
    loadSD(randomFileStack[randomFileStackPos][1], randomFileStack[randomFileStackPos][0]);
  }
}

void previusSong() {
  button_event = PREVIUS_SONG_EVENT;

  if(randomMode == RANDOM_NORMAL){
    if(fileIndex - 1 < 0) {
      if(!folders[folderIndex - 1].fileCounter) {
        if(folderIndex - 2 < 0) loadSD(folders[folderCounter - 1].fileCounter - 1, folderCounter - 1);
        else loadSD(folders[folderIndex - 2].fileCounter - 1, folderIndex - 2);
      }
      else if(folderIndex - 1 < 0) loadSD(folders[folderCounter - 1].fileCounter - 1, folderCounter - 1);
      else loadSD(folders[folderIndex - 1].fileCounter - 1, folderIndex - 1);
    }
    else loadSD(fileIndex - 1, folderIndex);  
  }
  else if(randomMode == REPEAT_SONG) loadSD(fileIndex, folderIndex);
  else if (randomMode == RANDOM_IN_FOLDER) {
    if(folders[folderIndex].randomFileStackPos > 0) {
      folders[folderIndex].randomFileStackPos--;
    }
    else {
      folders[folderIndex].randomFileStackPos = folders[folderIndex].fileCounter - 1;
    }
    uint16_t pos = folders[folderIndex].randomFileStackPos;
    loadSD(folders[folderIndex].randomFileStack[pos], folderIndex);
  }
  else if(randomMode == RANDOM_ALL_SONGS) {
    if(randomFileStackPos <= 0) randomFileStackPos = randomFileStackSize - 1;
    else randomFileStackPos -= 1;
    loadSD(randomFileStack[randomFileStackPos][1], randomFileStack[randomFileStackPos][0]);
  }
}

void volumeUp() {
  button_event = VOLUME_UP_EVENT;
  volume++;
  audio.setVolume(volume);
}

void volumeDown() {
  button_event = VOLUME_DOWN_EVENT;
  volume--;
  audio.setVolume(volume);
}

void changeRandomMode() {
  button_event = RANDOM_EVENT;
  switch (randomMode) {
    case RANDOM_NORMAL: { randomMode = RANDOM_IN_FOLDER; break;}
    case RANDOM_IN_FOLDER: { randomMode = RANDOM_ALL_SONGS; break;}
    case RANDOM_ALL_SONGS: { randomMode = REPEAT_SONG; break;}
    case REPEAT_SONG: { randomMode = RANDOM_NORMAL; break;}
    default: break;
  }
}

uint32_t g_checkPinsTime = millis();
bool playPinPressed = 0;
bool forwardPinPressed = 0;
bool backwardPinPressed = 0;
bool repeatPinPressed = 0;
void checkHardwarePins() {
  uint32_t crr_checkPinsTime = millis();
  if(crr_checkPinsTime - g_checkPinsTime > 200) {
    g_checkPinsTime = crr_checkPinsTime;
    bool playPin = digitalRead(PLAY_PIN);
    bool forwardPin = digitalRead(FORWARD_PIN);
    bool backwardPin = digitalRead(BACKWARD_PIN);
    bool volumeUpPin = digitalRead(VOLUME_UP_PIN);
    bool volumeDownPin = digitalRead(VOLUME_DOWN_PIN);
    bool repeatPin = digitalRead(REPEAT_PIN);
    
    if(forwardPin && !forwardPinPressed) {
      Serial.println("ForwardPin");
      forwardPinPressed = 1;
      nextSong();
    }
    else if (!forwardPin && forwardPinPressed) forwardPinPressed = 0;
    else if (forwardPin && forwardPinPressed) return;

    if(playPin && !playPinPressed) {
      Serial.println("playPin");
      playPinPressed = 1;
      playResume();
    }
    else if (!playPin && playPinPressed) playPinPressed = 0;
    else if (playPin && playPinPressed) return;

    if(backwardPin && !backwardPinPressed) {
      Serial.println("backwardPin");
      backwardPinPressed = 1;
      previusSong();
    }
    else if (!backwardPin && backwardPinPressed) backwardPinPressed = 0;
    else if (backwardPin && backwardPinPressed) return;
    
    if(volumeUpPin && volume < 21) {
      Serial.println("Volume up");
      volumeUp();
    }

    if(volumeDownPin && volume > 0) {
      Serial.println("Volume down");
      volumeDown();
    }

    if(repeatPin && !repeatPinPressed) {
      Serial.printf("Repeat pin %d\n", randomMode);
      changeRandomMode();
    }
    else if (!repeatPin && repeatPinPressed) repeatPinPressed = 0;
    else if (repeatPin && repeatPinPressed) return;
    
    button_event = NO_BTN_EVENT;
  }
}

void checkRadioPins() {
  switch (RadioButtonEvent)
  {
    case NEXT_SONG_EVENT: { nextSong(); break; }
    case PREVIUS_SONG_EVENT: { previusSong(); break; }
    case VOLUME_UP_EVENT: { volumeUp(); break; }
    case VOLUME_DOWN_EVENT: { volumeDown(); break; }
    case RANDOM_EVENT: { changeRandomMode(); break; }
    case PLAY_PAUSE_SONG_EVENT: { playResume(); break; }
    case MAIN_MENU_EVENT: { Serial.printf("MAIN_MENU: Não implementado\n"); break; }
  }
  RadioButtonEvent = NO_BTN_EVENT;
}

uint32_t lastAudioCurrentTime = 0;
uint32_t g_watchTrackPlaying = millis();
void watchTrackPlaying() {
  uint32_t crr_watchTrackPlaying = millis();
  uint32_t audioCurrentTime = audio.getAudioCurrentTime();
  
  if(pauseResumeStatus && crr_watchTrackPlaying - g_watchTrackPlaying > 1100) {
    g_watchTrackPlaying = millis();
    if(lastAudioCurrentTime > audioCurrentTime) lastAudioCurrentTime = 0;
    if(audioCurrentTime > 0 && lastAudioCurrentTime == audioCurrentTime) {
      printf("Caiu na verificacao de faixa\n lastAudio: %d, currentTime: %d\n", lastAudioCurrentTime, audioCurrentTime);
      lastAudioCurrentTime = 0;
      nextSong();
    }
    lastAudioCurrentTime = audioCurrentTime;
  }
}

void runRadioCommands(String command) {
  digitalWrite(HC12_SET_PIN, LOW);
  if(HC12.available()) {
    printf("Lixo da serial: %s\n", HC12.readString());
  }
  HC12.print(command);
  while(!HC12.available());
  if(HC12.available()) {
    printf("Data da serial: %s\n", HC12.readString());
  }
  // HC12.flush();
  // digitalWrite(HC12_SET_PIN, HIGH);AT+DEFAULT AT+RX
}

void setUpRadioTransmitter() {
  // runRadioCommands("AT+DEFAULT");NEXT_SONG
  // runRadioCommands("AT+B4800");
  // runRadioCommands("AT+C013");
  // runRadioCommands("AT+FU2");
  // runRadioCommands("AT+RX");

  // HC12.readString();

  // HC12.print("AT");

  // while(!HC12.available());

  // printf("Count: %d\n", HC12.available());
  // printf("Read: %d\n", HC12.read());
  // printf("Count: %d\n", HC12.available());

  
}
