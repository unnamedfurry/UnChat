/*
 *  |         Task          |         Java          |                C               |
 *  ----------------------------------------------------------------------------------
 *  |  Read File            |  new File()           |  fopen()                       |
 *  |  Close File           |  auto                 |  fclose()                      |
 *  |  Read Text            |  Files.readString()   |  fgets(), fread(), getc()      |
 *  |  Write Text           |  Files.writeString()  |  fprintf(). fputs(), fwrite()  |
 *  |  R/W binary           |  FileInputStream      |  fread() / fwrite()            |
 *  |  Check for existence  |  file.exists()        |  access(), stat()              |
 *  |  Get size, time etc.  |  file.length() etc.   |  stat()                        |
 */

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/sha.h>

#define CONFIG_FILE "conf.txt"
#define MAX_NAME 23
#define MAX_EMAIL 23
#define MAX_PASS 23
#define MAX_AVATAR 64

typedef struct {
    bool isFirstUsed;
    long userId;
    char userName[MAX_NAME+1];
    char email[MAX_EMAIL+1];
    unsigned char passwordHash[SHA256_DIGEST_LENGTH];
    char avatarUrl[MAX_AVATAR+1];
} Config;

bool loadConfig(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return false;
    char line[512];
    *cfg = (Config){.isFirstUsed = true};

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "isFirstUsed=", 12) == 0) cfg->isFirstUsed=(strstr(line, "=true") != NULL);
        else if (strncmp(line, "userId=", 7) == 0) cfg->userId=atoll(line+7);
        // else if (strncmp(line, "userId=", 7) == 0) strtol();
        else if (strncmp(line, "userName=", 9) == 0) strncpy(cfg->userName, line+9, MAX_NAME);
        else if (strncmp(line, "email=", 6) == 0) strncpy(cfg->email, line+6, MAX_EMAIL);
        else if (strncmp(line, "passwordHash=", 13) == 0) {
            const char *hex = line + 13;
            for (int i=0; i<SHA256_DIGEST_LENGTH; i++) {
                sscanf(hex + i*2, "%2hhx", cfg->passwordHash[i]);
            }
        }
        else if (strncmp(line, "avatarUrl=", 10) == 0) strncpy(cfg->avatarUrl, line+10, MAX_AVATAR);
    }
    fclose(f);
    return true;
}

bool saveConfig(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return false;
    fprintf(f, "isFirstUsed=%s\n", cfg->isFirstUsed ? "true" : "false");
    fprintf(f, "userId=%s\n", cfg->userId==0 ? "0000000000" : cfg->userName);
    fprintf(f, "userName=%s\n", cfg->userName);
    fprintf(f, "email=%s\n", cfg->email);
    fprintf(f, "passwordHash=");
    for (int i=0; i<SHA256_DIGEST_LENGTH; i++) {
        fprintf(f, "%02x", cfg->passwordHash[i]);
    }
    fprintf(f, "\n");
    fprintf(f, "avatarUrl=%s\n", cfg->avatarUrl);
    fclose(f);
    return true;
}

void HashPassword(const char* password, unsigned char* outHash) {
    SHA256((const unsigned char*)password, strlen(password), outHash);
}

int main(void) {
    InitWindow(1600, 900, "UnChat - BETA 1.0");
    int codepoints[2048] = {0};
    int count = 0;
    for (int i = 32; i < 128; i++) codepoints[count++] = i;
    for (int i = 0x0400; i <= 0x04FF; i++) codepoints[count++] = i;
    InitAudioDevice();
    SetTargetFPS(60);

    Font font = LoadFontEx("NotoSans-Regular.ttf", 32, codepoints, count);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);

    Config config = {0};
    bool loadedConf = loadConfig(&config);
    if (!loadedConf || config.isFirstUsed) {
        strcpy(config.userName, "");
        strcpy(config.email, "");
        config.isFirstUsed=true;
    }
    char passwordInput[MAX_PASS+1] = {0};
    int activeField=-1;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        if (config.isFirstUsed) {
            DrawTextEx(font, "Добро пожаловать. Пройди настройку профиля:", (Vector2){100, 50}, 40, 3, WHITE);
            GuiTextBox((Rectangle){100, 150, 400, 40}, config.userName, MAX_NAME, activeField==0);
            GuiTextBox((Rectangle){100, 220, 400, 40}, config.email, MAX_EMAIL, activeField==1);
            GuiTextBox((Rectangle){100, 290, 400, 40}, passwordInput, MAX_PASS, activeField==2);
            DrawTextEx(font,"Юзернейм", (Vector2){520, 160}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Email", (Vector2){520, 230}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Пароль", (Vector2){520, 300}, 20, 3, LIGHTGRAY);

            if (GuiButton((Rectangle){100, 380, 200, 50}, "Сохранить и продолжить")) {
                HashPassword(passwordInput, config.passwordHash);
                config.isFirstUsed=false;
                saveConfig(&config);
            } else {
                DrawTextEx(font, TextFormat("Привет, %s", config.userName), (Vector2){100, 100}, 40, 3, WHITE);
            }
        }

        EndDrawing();
    }
    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}