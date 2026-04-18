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
#include <errno.h>

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
    char profileDescription[1025];
} Config;


bool loadConfig(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        TraceLog(LOG_WARNING, "conf.txt не найден или поврежден. conft.txt будет пересоздан.");
        return false;
    }
    memset(cfg, 0, sizeof(Config));
    cfg->isFirstUsed=true;
    cfg->userId=0000000000;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0' || line[0] == '3') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *value = eq+1;

        if (strcmp(key, "isFirstUsed") == 0) {
            cfg->isFirstUsed=(strcmp(value, "true")==0);
        }
        else if (strcmp(key, "userId") == 0) {
            char *endptr = NULL;
            errno = 0;
            cfg->userId=strtol(value, &endptr, 10);
            if (errno !=0 || endptr == value) {
                TraceLog(LOG_WARNING, "некорректный userId: %s", value);
                cfg->userId=0000000000;
            }
        }
        else if (strcmp(key, "userName") == 0) {
            strncpy(cfg->userName, value, sizeof(cfg->userName) -1);
            cfg->userName[sizeof(cfg->userName)-1] ='\0';
        }
        else if (strcmp(key, "email") == 0) {
            strncpy(cfg->email, value, sizeof(cfg->email) -1);
            cfg->email[sizeof(cfg->email)-1] ='\0';
        }
        else if (strcmp(key, "passwordHash") == 0) {
            for (int i=0; i<32 && value[i*2] && value[i*2+1]; i++) {
                unsigned int byte;
                if (sscanf(value + i*2, "%2x", &byte) == 1) {
                    cfg->passwordHash[i]=(unsigned char)byte;
                }
            }
        }
        else if (strcmp(key, "avatarHash") == 0) {
            strncpy(cfg->avatarUrl, value, sizeof(cfg->avatarUrl)-1);
            cfg->avatarUrl[sizeof(cfg->avatarUrl)-1] ='\0';
        }
        else if (strcmp(key, "profileDescription") == 0) {
            strncpy(cfg->profileDescription, value, sizeof(cfg->profileDescription)-1);
            cfg->profileDescription[sizeof(cfg->profileDescription)-1] ='\0';
        }
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
    fprintf(f, "profileDescription=%s\n", cfg->profileDescription);
    fclose(f);
    return true;
}

void HashPassword(const char* password, unsigned char* outHash) {
    SHA256((const unsigned char*)password, strlen(password), outHash);
}

void DrawTextBoxed(Font font, const char *text, Rectangle container, float fontSize, float spacing, Color tint) {
    int length = (int)TextLength(text);
    float scaleFactor = fontSize / (float)font.baseSize;

    float cursorX = 0.0f;
    float cursorY = 0.0f;

    for (int i = 0; i < length; i++) {
        int byteSize = 0;
        int codepoint = GetCodepoint(&text[i], &byteSize);
        int index = GetGlyphIndex(font, codepoint);

        // Handle Manual Newlines
        if (codepoint == '\n') {
            cursorY += ((float)font.baseSize + (float)font.baseSize / 2) * scaleFactor;
            cursorX = 0;
        } else {
            // Automatic Word Wrap Check
            if ((cursorX + ((float)font.glyphs[index].advanceX * scaleFactor)) > container.width) {
                cursorY += ((float)font.baseSize + (float)font.baseSize / 2) * scaleFactor;
                cursorX = 0;
            }

            // Draw character if it fits within the vertical bounds
            if ((cursorY + ((float)font.baseSize * scaleFactor)) <= container.height) {
                DrawTextCodepoint(font, codepoint, (Vector2){ container.x + cursorX, container.y + cursorY }, fontSize, tint);
            }

            // Advance cursor
            if (font.glyphs[index].advanceX == 0) cursorX += ((float)font.recs[index].width * scaleFactor + spacing);
            else cursorX += ((float)font.glyphs[index].advanceX * scaleFactor + spacing);
        }
        i += (byteSize - 1);
    }
}

int main(void) {
    InitWindow(1600, 900, "UnChat - BETA 1.0");
    int codepoints[1024] = {0};
    int count = 0;
    for (int i = 32; i < 128; i++) codepoints[count++] = i;
    for (int i = 0x0400; i <= 0x04FF; i++) codepoints[count++] = i;
    InitAudioDevice();
    SetTargetFPS(60);

    Font font = LoadFontEx("Pixellari.ttf", 64, codepoints, count);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);
    GuiSetStyle(DEFAULT, 16, 24);

    Config config = {0};
    bool loadedConf = loadConfig(&config);
    if (!loadedConf || config.isFirstUsed) {
        strcpy(config.userName, "");
        strcpy(config.email, "");
        strcpy(config.profileDescription, "");
        config.isFirstUsed=true;
    }
    char passwordInput[MAX_PASS+1] = {0};
    static int activeField=-1;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        if (config.isFirstUsed) {
            DrawTextEx(font, "Добро пожаловать. Пройди настройку профиля:", (Vector2){100, 50}, 40, 3, WHITE);
            if (GuiTextBox((Rectangle){100, 150, 400, 40}, config.userName, MAX_NAME, activeField==0)) {
                activeField = (activeField == 0) ? -1 : 0;
            }
            if (GuiTextBox((Rectangle){100, 220, 400, 40}, config.email, MAX_EMAIL, activeField==1)) {
                activeField = (activeField == 1) ? -1 : 1;
            }
            if (GuiTextBox((Rectangle){100, 290, 400, 40}, passwordInput, MAX_PASS, activeField==2)) {
                activeField = (activeField == 2) ? -1 : 2;
            }
            if (GuiTextBox((Rectangle){100, 360, 400, 40}, config.profileDescription, MAX_PASS, activeField==3)) {
                activeField = (activeField == 3) ? -1 : 3;
            }
            DrawTextEx(font,"Юзернейм", (Vector2){520, 160}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Email", (Vector2){520, 230}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Пароль", (Vector2){520, 300}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Описание профиля (опционально)", (Vector2){520, 370}, 20, 3, LIGHTGRAY);

            if (GuiButton((Rectangle){100, 450, 200, 50}, "Сохранить и продолжить")) {
                HashPassword(passwordInput, config.passwordHash);
                config.isFirstUsed=false;
                saveConfig(&config);
            }
        } else {
            DrawRectangleLines(1, 1, 300, 899, WHITE);
            DrawRectangleLines(301, 1, 1000, 899, WHITE);
            DrawRectangleLines(1301, 1, 299, 899, WHITE);
            DrawLine(1, 40, 1600, 40, WHITE);
            DrawTextEx(font, "Знакомые", (Vector2){87, 10}, 24, 2, WHITE);
            DrawTextEx(font, "Чат", (Vector2){760, 10}, 24, 2, WHITE);
            DrawTextEx(font, "Профиль", (Vector2){1400, 10}, 24, 2, WHITE);

            DrawRectangleLines(1320, 90, 128, 128, WHITE);
            DrawTextEx(font, TextFormat("%s", config.userName), (Vector2){1320, 230}, 24, 1.0f, WHITE);
            DrawRectangleLines(1320, 270, 260, 400, GRAY);
            Rectangle textBounds = { 1326, 276, 248, 388 };
            DrawTextBoxed(font, config.profileDescription, textBounds, 16, 1.0f, WHITE);
        }

        EndDrawing();
    }
    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}