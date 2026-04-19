#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>

#define CONFIG_FILE "conf.txt"
#define MAX_NAME 23
#define MAX_EMAIL 23
#define MAX_PASS 23
#define MAX_AVATAR 64
#define MAX_DESC 1024
#define MAX_MESS 2048

typedef struct {
    bool isFirstUsed;
    long userId;
    char userName[MAX_NAME+1];
    char email[MAX_EMAIL+1];
    unsigned char passwordHash[SHA256_DIGEST_LENGTH];
    char avatarUrl[MAX_AVATAR+1];
    char profileDescription[MAX_DESC+1];
} Config;
typedef struct {
    char name[MAX_NAME+1];
    long userId;
    char profileDescription[MAX_DESC+1];
    char avatarUrl[MAX_AVATAR+1];
} Friend;
Friend Friends[100];
Config config = {0};


//
//             NETWORK COMMUNICATION
//


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 63321
#define BUFFER_SIZE 2077
static pthread_t thread_id;
static int sock = -1;
static struct sockaddr_in serv_addr;
bool connected = false;
char buf[BUFFER_SIZE];

void* recieveMessage(void* arg) {
    while (connected) {
        int answerByte = read(sock, buf, sizeof(buf)-1);
        if (answerByte <= 0) {
            connected = false;
            printf("\nconnection closed\n");
            break;
        }
        buf[answerByte] = 0;
        printf("\ngot from server: %s\n", buf);

        if (strncmp(buf, "save-profile/", 13) == 0) {
            printf("\nprofile successfully saved on server");
        }
        else if (strlen(buf) > 5 && isdigit(buf[0])) {
            // скорее всего createId/
            long newId = atol(buf);
            if (newId > 0) {
                config.userId = newId;
                printf("\ngot new id: %ld", newId);
            }
        }
    }
    return NULL;
}
bool initNetwork(void) {
    if (connected) return true;
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    // Define server target
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nfailed to connect to server\n\n");
        return false;
    }
    connected=true;
    // Create a listener
    if (pthread_create(&thread_id, NULL, recieveMessage, NULL) != 0) {
        perror("\nfailed to create listener thread\n\n");
    }
    return true;
}
void sendMessage(const char *message) {
    if (!connected) {
        if (!initNetwork()) return;
    }
    if (send(sock, message, strlen(message), 0) < 0) {
        printf("\nerror sending message: %s\n", message);
        connected = false;
    } else {
        printf("\nsent successfully: %s\n", message);
    }
}

void renderFriends(void) {

}


//
//             CONFIG LOAD
//


bool loadConfig(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        TraceLog(LOG_WARNING, "\n\nconf.txt не найден или поврежден. conft.txt будет пересоздан.\n\n");
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
            char *endptr = nullptr;
            errno = 0;
            cfg->userId=strtol(value, &endptr, 10);
            if (errno !=0 || endptr == value) {
                TraceLog(LOG_WARNING, "\n\nнекорректный userId: %s\n\n", value);
                cfg->isFirstUsed=true;
                return false;
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
        else if (strcmp(key, "avatarUrl") == 0) {
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
    fprintf(f, "userId=%ld\n", cfg->userId);
    fprintf(f, "userName=%s\n", cfg->userName);
    fprintf(f, "email=%s\n", cfg->email);

    fprintf(f, "passwordHash=");
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        fprintf(f, "%02x", cfg->passwordHash[i]);
    }
    fprintf(f, "\n");

    fprintf(f, "avatarUrl=%s\n", cfg->avatarUrl);
    fprintf(f, "profileDescription=%s\n", cfg->profileDescription);
    fclose(f);

    char hashHex[65] = {0};
    for (int i = 0; i < 32; i++) {
        sprintf(hashHex + i*2, "%02x", cfg->passwordHash[i]);
    }

    char message[2048] = {0};
    snprintf(message, sizeof(message),
             "save-profile/%ld\x1E%s\x1E%s\x1E%s\x1E%s\x1E%s",
             cfg->userId, cfg->userName, cfg->email, hashHex,
             cfg->avatarUrl, cfg->profileDescription);

    sendMessage(message);
    return true;
}

void HashPassword(const char* password, unsigned char* outHash) {
    SHA256((const unsigned char*)password, strlen(password), outHash);
}


//
//             BOXED TEXT RENDERING
//


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


//
//             MAIN METHOD
//


int main(void) {
    InitWindow(1600, 900, "UnChat - BETA 1.0");
    int codepoints[1024] = {0};
    int count = 0;
    for (int i = 32; i < 128; i++) codepoints[count++] = i;
    for (int i = 0x0400; i <= 0x04FF; i++) codepoints[count++] = i;
    InitAudioDevice();
    SetTargetFPS(60);
    SetTraceLogLevel(LOG_NONE);

    Font font = LoadFontEx("Pixellari.ttf", 64, codepoints, count);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);
    GuiSetStyle(DEFAULT, 16, 24);

    initNetwork();
    sendMessage("test/");

    bool loadedConf = loadConfig(&config);
    if (!loadedConf || config.isFirstUsed) {
        strcpy(config.userName, "");
        strcpy(config.email, "");
        strcpy(config.profileDescription, "");
        config.isFirstUsed=true;
    }
    char passwordInput[MAX_PASS+1] = {0};
    static int activeField=-1;
    char newDesc[1025] = "";
    char message[2049] = "";

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 40, 40, 40, 255 });

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
            if (GuiTextBox((Rectangle){100, 360, 400, 40}, config.profileDescription, MAX_DESC, activeField==3)) {
                activeField = (activeField == 3) ? -1 : 3;
            }
            DrawTextEx(font,"Юзернейм", (Vector2){520, 160}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Email", (Vector2){520, 230}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Пароль", (Vector2){520, 300}, 20, 3, LIGHTGRAY);
            DrawTextEx(font, "Описание профиля (опционально)", (Vector2){520, 370}, 20, 3, LIGHTGRAY);

            if (GuiButton((Rectangle){100, 450, 200, 50}, "Сохранить и продолжить")) {
                if (strlen(config.userName) < 3) {
                    // log error?
                    continue;
                }

                HashPassword(passwordInput, config.passwordHash);
                config.isFirstUsed = false;
                sendMessage("createId/");

                // awaiting for responce
                sleep(1);        // or through flag

                saveConfig(&config);
            }
        } else {
            DrawRectangleLines(1, 1, 300, 899, GRAY);
            DrawRectangleLines(301, 1, 1000, 899, GRAY);
            DrawRectangleLines(1301, 1, 299, 899, GRAY);
            DrawLine(1, 40, 1600, 40, GRAY);
            DrawTextEx(font, "Знакомые", (Vector2){87, 10}, 24, 2, WHITE);
            DrawTextEx(font, "Чат", (Vector2){760, 10}, 24, 2, WHITE);
            DrawTextEx(font, "Профиль", (Vector2){1400, 10}, 24, 2, WHITE);

            DrawRectangleLines(1320, 90, 128, 128, GRAY);
            DrawTextEx(font, TextFormat("%s", config.userName), (Vector2){1320, 230}, 24, 1.0f, WHITE);
            //DrawRectangleLines(1320, 270, 260, 400, GRAY);
            Rectangle textBounds = { 1326, 276, 248, 388 };
            if (GuiTextBox((Rectangle){1320, 270, 260, 400}, newDesc, MAX_DESC, activeField==4)) {
                activeField = (activeField == 4) ? -1 : 4;
            } else {
                DrawTextBoxed(font, config.profileDescription, textBounds, 16, 1.0f, WHITE);
            }
            if (GuiButton((Rectangle){1320, 700, 200, 50}, "Обновить")) {
                newDesc[1024]='\0';
                strcpy(config.profileDescription, newDesc);
                saveConfig(&config);
                loadConfig(&config);
                memset(newDesc, 0, sizeof(newDesc));
            }
            if (GuiTextBox((Rectangle){300, 839, 861, 60}, message, MAX_MESS, activeField==5)) {
                activeField = (activeField == 5) ? -1 : 5;
            }
            if (GuiButton((Rectangle){1141, 839, 160, 60}, "Отправить") || IsKeyPressed(KEY_ENTER)) {
                message[2048]='\0';
                // TODO: send message
                if (strlen(message) == 0) continue;
                char parsed[BUFFER_SIZE] = {0};
                snprintf(parsed, sizeof(parsed), "recieve-message/%ld\x1E%s", config.userId, message);

                sendMessage(parsed);
                memset(message, 0, sizeof(message));
                memset(parsed, 0, strlen(parsed));
                memset(buf, 0, sizeof(buf));
            }
        }

        EndDrawing();
    }
    // Close connection
    close(sock);

    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}