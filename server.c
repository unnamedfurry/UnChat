#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mysql.h>
#include <openssl/sha.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

#define BRED    "\033[1;31m"
#define BGREEN  "\033[1;32m"
#define BYELLOW "\033[1;33m"
#define BBLUE   "\033[1;34m"
#define BMAGENTA "\033[1;35m"
#define BCYAN   "\033[1;36m"
#define BWHITE  "\033[1;37m"

int port = 63321;
#define BUFFER_SIZE 2077
int server_fd, new_socket;
struct sockaddr_in address;
int addrlen = sizeof(address);
char incomingBuffer[BUFFER_SIZE] = {0};
char outgoingBuffer[BUFFER_SIZE] = {0};
static pthread_t thread_id;

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
Config config = {0};
typedef struct {
    char name[MAX_NAME+1];
    long userId;
    char profileDescription[MAX_DESC+1];
    char avatarUrl[MAX_AVATAR+1];
} Friend;
typedef struct {
    char messageId[11];
    char message[2049];
} Message;
MYSQL *conn;

//
//      DATABASE STRUCTRURE
//
//        UnChat Database
//       /               \
// users table       messages table
//
//        users table:
// | userId | userName | email | passwordHash | avatarUrl | profileDescription |
//   long     char       char    char           char        char
//
//        messages table:
// | messageId | messageContent |
//   long        char
//
//        friends table:
// | userId | relatedUsersIds |
//   long     long,long,long...
//

/* void safe_write(MYSQL *conn, const char *user_input) {
    // buffer for screened string
    // size: original size * 2 + 1 byte for \0
    char escaped_input[strlen(user_input) * 2 + 1];

    // screening symbols
    // params: sonnection, where to write to, where to take from, original string length
    unsigned long len = mysql_real_escape_string(conn, escaped_input, user_input, strlen(user_input));

    // creating SQL request
    char query[1024];
    sprintf(query, "INSERT INTO tags (name) VALUES ('%s')", escaped_input);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "\n" RED "error saving to database: %s" RESET, mysql_error(conn));
    } else {
        printf("\n" GREEN "successfully saved data" RESET);
    }
} */

void getFriends(MYSQL *conn, long user_id, int sock) {
    char response[BUFFER_SIZE * 4] = {0};
    int offset = snprintf(response, sizeof(response), "getFriendsList/%ld\x1E", user_id);

    char query[256];
    snprintf(query, sizeof(query), "SELECT relatedUserIds FROM friends WHERE userId = %ld", user_id);
    if (mysql_query(conn, query)) {
        printf(RED "\nfailed to get friends for user %ld" RESET, user_id);
        send(sock, "getFriendsList/error", 21, 0);
        return;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        send(sock, "getFriendsList/empty", 21, 0);
        return;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row == NULL || row[0] == NULL || strlen(row[0]) == 0) {
        send(sock, "getFriendsList/empty", 21, 0);
        return;
    }

    char *id_token = strtok(row[0], ",");
    mysql_free_result(res);
    while (id_token != NULL) {
        long friend_id = strtol(id_token, NULL, 10);
        if (friend_id <= 0) {
            id_token = strtok(NULL, ",");
            continue;
        }

        snprintf(query, sizeof(query), "SELECT username, avatarurl, profiledesc FROM users WHERE userId = %ld", friend_id);
        if (mysql_query(conn, query) == 0) {
            MYSQL_RES *fres = mysql_store_result(conn);
            if (fres) {
                MYSQL_ROW frow = mysql_fetch_row(fres);
                if (frow && frow[0]) {
                    offset += snprintf(response+offset, sizeof(response)-offset, "%s\x1F%ld\x1F%s\x1F%s\x1E");
                }
                mysql_free_result(fres);
            }
        }
        id_token = strtok(NULL, ",");
    }

    if (offset > 15) {
        send(sock, response, strlen(response), 0);
        printf(GREEN "\ngetFriend: sent %d bytes for user %ld" RESET, (int)strlen(response), user_id);
    } else {
        send(sock, "getFriendsList/empty", 21, 0);
    }
}

void getUsers(MYSQL *conn) {
    if (mysql_query(conn, "SELECT userid, username FROM users")) {
        fprintf(stderr, "\n" RED "SELECT err: %s" RESET, mysql_error(conn));
    } else {
        MYSQL_RES *res = mysql_store_result(conn); // loading result ro memory
        if (res == NULL) return;

        MYSQL_ROW row; // line array (char *)
        int num_fields = mysql_num_fields(res); // number of columns

        while ((row = mysql_fetch_row(res))) {
            for(int i = 0; i < num_fields; i++) {
                printf("%s ", row[i] ? row[i] : "NULL");
            }
            printf("\n");
        }

        mysql_free_result(res); // free memory
    }
}

void* acceptMessage(void *arg) {
    int sock = *(int*)arg;
    free(arg);
    char buffer[BUFFER_SIZE] = {0};
    char response[128] = {0};

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE - 1);

        if (valread <= 0) {
            printf("\n" YELLOW "client disconnected (sock %d)" RESET, sock);
            break;
        }

        buffer[valread] = '\0';
        printf("\n" YELLOW "received from client: %s" RESET, buffer);

        if (strcmp(buffer, "test/") == 0) {
            strcpy(response, "ok");
        }
        else if (strncmp(buffer, "receive-message/", 16) == 0) {
            printf("\n" GREEN "saving message: %s" YELLOW, buffer);
            // TODO save to database
            strcpy(response, "ok");
        }
        else if (strncmp(buffer, "createId/", 9) == 0) {
            srand(time(nullptr) + clock());
            long id = rand()%2147483647;
            sprintf(response, "\n" GREEN "newId generated: %ld" RESET, id);
        }
        else if (strncmp(buffer, "save-profile/", 13) == 0) {
            printf("\n" GREEN "save-profile received: %s\n" RESET, buffer);

            char *parts[10] = {0};
            int count = 0;
            char *token = strtok(buffer + 13, "\x1E");

            while (token && count < 10) {
                parts[count++] = token;
                token = strtok(nullptr, "\x1E");
            }

            // TODO save to database
            if (count >= 6) {
                printf(MAGENTA"  UserID:       %s\n", parts[0]);
                printf("  Name:         %s\n", parts[1]);
                printf("  Email:        %s\n", parts[2]);
                printf("  PasswordHash: %s\n", parts[3]);
                printf("  AvatarUrl:    %s\n", parts[4]);
                printf("  ProfileDesc:  %s\n" RESET, parts[5]);
            }

            strcpy(response, "ok");
        }
        else if (strncmp(buffer, "getFriendsList/", 15) == 0) {
            long user_id = strtol(buffer+15, NULL, 10);
            if (user_id > 0) {
                getFriends(conn, user_id, sock);
                return NULL;
            }
        }

        send(sock, response, strlen(response), 0);
        printf("\n" YELLOW "sent responce for request: %s -> %s" RESET, buffer, response);
    }

    close(sock);
    return NULL;
}

int main(void) {
    // if we don't connect to database, chat probably won't work
    printf("\n" YELLOW "connecting ro mysql" RESET);
    conn = mysql_init(NULL);

    if (mysql_real_connect(conn, "localhost", "root", "null", "unchat", 0, NULL, 0) == NULL) {
        fprintf(stderr, "\n" RED "failed to connect to database: %s" RESET, mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    printf("\n" YELLOW "connected to database successfully" RESET);

    // we also need to initialize tables
    const char *queries[] = {
        // users
        "CREATE TABLE IF NOT EXISTS users ("
            "userId BIGINT UNSIGNED NOT NULL PRIMARY KEY,"          // main column
            "username VARCHAR(24) NOT NULL,"
            "email VARCHAR(24) NOT NULL UNIQUE,"                    // email (unique)
            "passwordHash VARCHAR(64) NOT NULL,"                    // SHA-256 in hex = 64 syms
            "avatarUrl VARCHAR(64) NOT NULL DEFAULT '',"
            "profileDesc VARCHAR(1025) NOT NULL DEFAULT ''"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",

        // friends (many-to-many)
        "CREATE TABLE IF NOT EXISTS friends ("
            "userId BIGINT UNSIGNED NOT NULL,"
            "relatedUserId BIGINT UNSIGNED NOT NULL,"
            "PRIMARY KEY (userId, relatedUserId),"                  // composite key
            "FOREIGN KEY (userId) REFERENCES users(userId) ON DELETE CASCADE,"
            "FOREIGN KEY (relatedUserId) REFERENCES users(userId) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;",

        // messages (lite version)
        "CREATE TABLE IF NOT EXISTS messages ("
            "messageId BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "senderId BIGINT UNSIGNED NOT NULL,"
            "receiverId BIGINT UNSIGNED NOT NULL,"
            "message TEXT NOT NULL,"
            "sentAt DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY (senderId) REFERENCES users(userId),"
            "FOREIGN KEY (receiverId) REFERENCES users(userId)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"
    };

    for (int i = 0; i < 3; i++) {
        if (mysql_query(conn, queries[i])) {
            fprintf(stderr, RED "Ошибка создания таблицы %d: %s\n" RESET, i, mysql_error(conn));
        } else {
            printf(GREEN "Таблица %d создана успешно!\n" RESET, i);
        }
    }

    int num_queries = sizeof(queries) / sizeof(queries[0]);

    for (int i = 0; i < num_queries; i++) {
        if (mysql_query(conn, queries[i])) {
            fprintf(stderr, "\n" RED "error creating table %d: %s" RESET, i, mysql_error(conn));
        } else {
            printf("\n" GREEN "table %d created successfully!" RESET, i);
        }
    }

    // and then network
    // Create socket v4
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons(port);

    // Bind port
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    // Start listening
    listen(server_fd, 10);
    printf("\n" YELLOW "server is listening on port %d" YELLOW, port);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        // CRITICAL: We must allocate memory for the socket ID so it isn't
        // overwritten by the next accept() before the thread starts!
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = new_socket;

        if (pthread_create(&thread_id, nullptr, acceptMessage, client_sock_ptr) == 0) {
            // Tell the OS to reclaim thread resources automatically on exit
            pthread_detach(thread_id);
        } else {
            close(new_socket);
            free(client_sock_ptr);
        }
    }
    return 0;

    // Cleanup
    close(new_socket);
    close(server_fd);
    printf("\n");
    return 0;
}