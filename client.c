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

#define RESET   "\033[0m"
#define cRED     "\033[31m"
#define cGREEN   "\033[32m"
#define cYELLOW  "\033[33m"
#define cBLUE    "\033[34m"
#define cMAGENTA "\033[35m"
#define cCYAN    "\033[36m"

#define BRED    "\033[1;31m"
#define BGREEN  "\033[1;32m"
#define BYELLOW "\033[1;33m"
#define BBLUE   "\033[1;34m"
#define BMAGENTA "\033[1;35m"
#define BCYAN   "\033[1;36m"
#define BWHITE  "\033[1;37m"

typedef struct {
    bool isFirstUsed;
    long userId;
    char userName[MAX_NAME+1];
    char email[MAX_EMAIL+1];
    unsigned char passwordHash[SHA256_DIGEST_LENGTH];
    char avatarUrl[MAX_AVATAR+1];
    char profileDescription[MAX_DESC+1];
} Config;
Config config = {0};
typedef struct {
    char name[MAX_NAME+1];
    long userId;
    char profileDescription[MAX_DESC+1];
    char avatarUrl[MAX_AVATAR+1];
} Friend;
Friend friends[100] = {0};
typedef struct {
    char messageId[11];
    char message[2049];
} Message;
Message messages[1000000] = {0};
long randomId = 0L;
bool finishedResponse = false;
long currentFriendId = 0L;


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
    char localBuf[BUFFER_SIZE];

    while (connected) {
        finishedResponse = false;
        memset(localBuf, 0, sizeof(localBuf));

        int answerByte = (int)read(sock, localBuf, sizeof(localBuf)-1);
        if (answerByte <= 0) {
            connected = false;
            printf("\n" cYELLOW "connection closed" RESET);
            break;
        }
        localBuf[answerByte] = '\0';

        printf("\n" cYELLOW "got from server: %s" RESET, localBuf);

        strncpy(buf, localBuf, sizeof(buf)-1);
        buf[sizeof(buf)-1] = '\0';

        if (strncmp(localBuf, "save-profile/", 13) == 0) {
            printf("\n" cGREEN "profile successfully saved on server" RESET);
        }
        else if (strncmp(localBuf, "createId/user", 13) == 0) {
            long newId = atol(localBuf + 13);   // исправил +9 → +13
            if (newId > 0) {
                config.userId = newId;
                printf("\n" cGREEN "got new id for user: %ld" RESET, newId);
            }
        }
        else if (strncmp(localBuf, "createId/message", 16) == 0) {
            long newId = atol(localBuf + 16);
            if (newId > 0) {
                randomId = newId;
                printf("\n" cGREEN "got new id for message: %ld" RESET, newId);
            }
        }
        else if (strncmp(localBuf, "getFriendsList/", 15) == 0) {
            int count = 0;
            char *token = strtok(localBuf + 15, "\x1E");

            while (token && count < 100) {
                char *parts[4] = {0};
                int count2 = 0;
                char *token2 = strtok(token, "\x1F");
                while (token2 && count2 < 4) {
                    parts[count2++] = token2;
                    token2 = strtok(NULL, "\x1F");
                }
                Friend friend_;

                // Защита от NULL
                if (parts[0]) strcpy(friend_.name, parts[0]);
                else friend_.name[0] = '\0';

                friend_.userId = (parts[1]) ? strtol(parts[1], NULL, 10) : 0;

                if (parts[2]) strcpy(friend_.profileDescription, parts[2]);
                else friend_.profileDescription[0] = '\0';

                if (parts[3]) strcpy(friend_.avatarUrl, parts[3]);
                else friend_.avatarUrl[0] = '\0';

                friends[count] = friend_;
                count++;

                token = strtok(NULL, "\x1E");
            }
            printf("\n" cGREEN "received friends list (%d friends)" RESET, count);
        }
        else if (strncmp(localBuf, "getChatHistory/", 15) == 0) {
            // TODO
        }
        else if (strncmp(localBuf, "err", 3) == 0) {
            printf("\n" cRED "Server returned error" RESET);
        }

        finishedResponse = true;
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
        printf("\n" cRED "failed to connect to server" RESET "\n\n");
        return false;
    }
    connected=true;
    // Create a listener
    if (pthread_create(&thread_id, nullptr, recieveMessage, NULL) != 0) {
        perror("\n" cRED "failed to create listener thread" RESET "\n\n");
    }
    return true;
}
void sendMessage(const char *message) {
    if (!connected) {
        if (!initNetwork()) return;
    }
    if (send(sock, message, strlen(message), 0) < 0) {
        printf("\n" cRED "error sending message: %s" RESET "\n", message);
        connected = false;
    } else {
        printf("\n" cGREEN "sent successfully: %s" RESET "\n", message);
        finishedResponse=false;
    }
}


//
//             CONFIG LOAD
//


bool loadConfig(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        TraceLog(LOG_WARNING, "\n\n" cYELLOW "conf.txt не найден или поврежден. conft.txt будет пересоздан." RESET "\n\n");
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
                TraceLog(LOG_WARNING, "\n\n" cRED "некорректный userId: %s" RESET "\n\n", value);
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
    SetTraceLogLevel(LOG_WARNING);
    SetExitKey(KEY_NULL);

    Font font = LoadFontEx("Pixellari.ttf", 64, codepoints, count);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(font);
    GuiSetStyle(DEFAULT, 16, 24);

    bool initedNetwork = initNetwork();
    sendMessage("test/");

    bool loadedConf = loadConfig(&config);
    if (!loadedConf || config.isFirstUsed) {
        strcpy(config.userName, "");
        strcpy(config.email, "");
        strcpy(config.profileDescription, "");
        config.isFirstUsed=true;
    } else {
        char message[27] = {0};
        sprintf(message, "getFriendsList/%ld/", config.userId);
        sendMessage(message);
    }
    char passwordInput[MAX_PASS+1] = {0};
    static int activeField=-1;
    char newDesc[1025] = "";
    char message[2049] = "";
    bool isAddingFriend = false;
    char userId[10] = "";
    static Texture2D userAvatarTexture = {0};
    static char avatarPathInput[256] = {0};
    if (strlen(config.avatarUrl) != 0) {
        strncmp(avatarPathInput, config.avatarUrl, sizeof(config.avatarUrl));
        Image img = LoadImage(avatarPathInput);
        userAvatarTexture = LoadTextureFromImage(img);
        UnloadImage(img);
    }

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
                sendMessage("createId/user");

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
            DrawLine(1, 80, 1600, 80, GRAY);

            Rectangle avatarRect = {1320, 90, 128, 128};
            DrawRectangleRec(avatarRect, DARKGRAY);

            if (userAvatarTexture.id != 0) {
                DrawTexturePro(userAvatarTexture,
                               (Rectangle){0, 0, 128, 128},
                               avatarRect,
                               (Vector2){0, 0}, 0.0f, WHITE);
            } else {
                DrawRectangleLinesEx(avatarRect, 4, LIGHTGRAY);
                DrawTextEx(font, "нет\nаватарки", (Vector2){1333, 120}, 24, 2, GRAY);
            }
            DrawTextEx(font, TextFormat("%s", config.userName), (Vector2){1320, 230}, 24, 1.0f, WHITE);
            //DrawRectangleLines(1320, 270, 260, 400, GRAY);
            Rectangle textBounds = { 1326, 276, 248, 388 };
            if (GuiTextBox((Rectangle){1320, 270, 260, 400}, newDesc, MAX_DESC, activeField==4)) {
                activeField = (activeField == 4) ? -1 : 4;
            } else {
                DrawTextBoxed(font, config.profileDescription, textBounds, 16, 1.0f, WHITE);
            }
            if (GuiButton((Rectangle){1320, 680, 200, 50}, "Обновить")) {
                newDesc[1024]='\0';
                strcpy(config.profileDescription, newDesc);
                saveConfig(&config);
                loadConfig(&config);
            }

            DrawTextEx(font, "Путь к аватарке:", (Vector2){1320, 760}, 20, 2, LIGHTGRAY);
            if (GuiTextBox((Rectangle){1320, 790, 260, 40}, avatarPathInput, 255, activeField == 7)) {
                activeField = (activeField == 7) ? -1 : 7;
            }

            if (GuiButton((Rectangle){1320, 840, 200, 50}, "Загрузить")) {
                if (strlen(avatarPathInput) > 3) {
                    Image img = LoadImage(avatarPathInput);

                    if (img.data != NULL) {
                        // square 128 by 128
                        int side = (img.width < img.height) ? img.width : img.height;   // taking smallest side

                        // crop to square
                        Rectangle cropRect = {
                            (float)(img.width - side) / 2.0f,      // x
                            (float)(img.height - side) / 2.0f,     // y
                            (float)side,                           // width
                            (float)side                            // height
                        };

                        ImageCrop(&img, cropRect);
                        ImageResize(&img, 128, 128);        // resize to 128x128

                        // saving near config file
                        const char *savePath = TextFormat("avatars/%ld.png", config.userId);

                        // folder is not exist
                        system("mkdir -p avatars");   // Linux + Windows

                        if (ExportImage(img, savePath)) {
                            printf(cGREEN "\navatar was cropped and saved: %s" RESET, savePath);

                            // updating config
                            snprintf(config.avatarUrl, MAX_AVATAR, "%ld.png", config.userId);

                            // refreshing texture
                            if (userAvatarTexture.id != 0) UnloadTexture(userAvatarTexture);
                            userAvatarTexture = LoadTextureFromImage(img);

                            saveConfig(&config);        // save and pull to server
                        } else {
                            printf(cRED "\nfailed to save avatar" RESET);
                        }

                        UnloadImage(img);
                        memset(avatarPathInput, 0, sizeof(avatarPathInput));
                    } else {
                        printf(cRED "\nfailed to load image: %s" RESET, avatarPathInput);
                    }
                }
            }

            if (GuiTextBox((Rectangle){300, 839, 861, 60}, message, MAX_MESS, activeField==5)) {
                activeField = (activeField == 5) ? -1 : 5;
            }
            if (GuiButton((Rectangle){1141, 839, 160, 60}, "Отправить") || IsKeyPressed(KEY_ENTER)) {
                if (strlen(message) != 0) {
                    sendMessage("createId/message");
                    if (!finishedResponse) usleep(1000);
                    message[2048]='\0';
                    char parsed[BUFFER_SIZE] = {0};
                    snprintf(parsed, sizeof(parsed), "receive-message/%ld\x1E%ld\x1E%ld\x1E%s", randomId, config.userId, currentFriendId, message);

                    sendMessage(parsed);
                    memset(message, 0, sizeof(message));
                    memset(parsed, 0, strlen(parsed));
                }
            }
            if (GuiButton((Rectangle){20, 45, 100, 30}, "+ Друг")) {
                isAddingFriend=true;
            }
            if (GuiButton((Rectangle){155, 45, 120, 30}, "+ Группа")) {

            }
            if (isAddingFriend == true) {
                if (IsKeyPressed(KEY_ESCAPE)) isAddingFriend=false;
                DrawRectangle(1600/2-200, 900/2-200, 400, 400, GRAY);
                DrawRectangleLines(1600/2-200, 900/2-200, 400, 400, WHITE);
                DrawRectangleLines(1600/2-190, 900/2-140, 381, 61, WHITE);
                DrawTextEx(font, "Введи айди пользователя:", (Vector2){1600/2-190, 900/2-180}, 20, 2, WHITE);
                if (GuiTextBox((Rectangle){1600/2-190, 900/2-140, 380, 60}, userId, 10, activeField==6)) {
                    activeField = (activeField == 6) ? -1 : 6;
                }
                if (GuiButton((Rectangle){1600/2+96, 900/2+156, 100, 40}, "Далее")) {
                    if (strlen(userId) == 0) continue;
                    char parsed[21] = {0};
                    snprintf(parsed, sizeof(parsed), "addFriend/%ld\x1E%s", config.userId, userId);
                    sendMessage(parsed);
                    isAddingFriend=false;
                }
            }
            // TODO рендер друзей. нужно перебрать массив friends и отрисовать каждый слот в левой части.
            int startY = 90;
            for (int i = 0; i < 100 && friends[i].userId != 0; i++) {
                Rectangle friendRect = { 10, startY, 280, 70 };

                if (CheckCollisionPointRec(GetMousePosition(), friendRect)) {
                    DrawRectangleRec(friendRect, (Color){60, 60, 70, 255});
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        currentFriendId = friends[i].userId;
                        // TODO: загрузить историю чата с этим другом
                    }
                } else {
                    DrawRectangleRec(friendRect, (Color){50, 50, 60, 255});
                }
                DrawRectangleLinesEx(friendRect, 2, GRAY);

                // friend avatar
                Rectangle avatarRect = { 20, startY + 8, 54, 54 };
                DrawRectangleRec(avatarRect, DARKGRAY);
                DrawRectangleLinesEx(avatarRect, 2, LIGHTGRAY);

                // name + description
                DrawTextEx(font, friends[i].name, (Vector2){85, startY + 12}, 24, 2, WHITE);

                if (strlen(friends[i].profileDescription) > 0) {
                    char shortDesc[80];
                    strncpy(shortDesc, friends[i].profileDescription, 70);
                    shortDesc[70] = '\0';
                    if (strlen(friends[i].profileDescription) > 70) strcat(shortDesc, "...");
                    DrawTextEx(font, shortDesc, (Vector2){85, startY + 42}, 18, 2, LIGHTGRAY);
                }

                startY += 80;
            }


            // TODO сделать группы
        }

        if (initedNetwork == false) {
            DrawRectangle(1, 900/2-100, 1600, 200, GRAY);
            DrawRectangleLines(1, 900/2-100, 1599, 199, RED);
            DrawTextEx(font, "Потеряно соединение с сервером!", (Vector2){1600/2-470, 900/2-20}, 60, 2, RED);
        }

        EndDrawing();
    }
    // Close connection
    close(sock);

    UnloadTexture(userAvatarTexture);
    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}