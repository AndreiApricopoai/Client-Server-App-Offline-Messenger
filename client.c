// CLIENT

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <termios.h>

#define CLEAR_SCREEN() system("clear") // curatare ecran

// coduri pentru text colorat in terminal
#define WHT "\e[0;37m"
#define ANSI_BOLD "\033[1m%c\033[32m"
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define WHT "\e[0;37m"

#define PORT 3000
#define IP "127.0.0.1" // localhost

char USER[100] = "\0"; // aici stocam numele clientului dupa ce se conecteaza la server

// afiseaza pe ecran userul conectat
void SHOW_CONNECTED_AS(char *username)
{
    printf(ANSI_COLOR_MAGENTA "CONNECTED AS : %s" ANSI_COLOR_RESET, username);
    fflush(stdout);
}
// afiseaza pe ecran WELCOME TO : OFFLINE MESSENGER
void SHOW_LOGO()
{
    printf(ANSI_COLOR_YELLOW "WELCOME TO :" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN " OFFLINE MESSENGER" ANSI_COLOR_RESET);
    fflush(stdout);
}
// afiseaza pe ecran panoul de comenzi dupa ce ne logam cu succes
void SHOW_PANEL_MAIN_COMMANDS(char *username)
{
    SHOW_CONNECTED_AS(username);
    printf(ANSI_COLOR_CYAN "\n\n->  show-all-users\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "->  show-online-users\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "->  show-offline-users\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "->  show-notifications\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_CYAN "->  all-commands\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_GREEN "->  start:<user>\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_GREEN "->  reply:<msg-number>:<text>  (only in chat)\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_RED "->  exit-app\n" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_RED "->  exit-chat  (only in chat)\n" ANSI_COLOR_RESET);
    fflush(stdout);
}
// afiseaza pe ecran panoul LOGIN/REGISTER/EXIT inainte de conectare
void SHOW_OPTIONS()
{
    printf("\n\n\nInput options:\n\nPress < 1 > for LOGIN\nPress < 2 > for REGISTER\nPress < 3 > for EXIT\n\n");
    fflush(stdout);
}
// aici citim user-ul si parola de la tastatura, le trimitem catre server si asteptam raspuns inapoi pentru logare
int TRY_LOGIN(int socket)
{
    // pregatim interfata
    CLEAR_SCREEN();
    SHOW_LOGO();
    printf(ANSI_COLOR_GREEN "\n\n\nUSERNAME: " ANSI_COLOR_RESET);
    fflush(stdout);

    // citim username-ul de la tastatura
    char username[100];
    bzero(username, 100);

    int username_len = read(0, username, 100);
    if (username_len == -1)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare citire username!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    printf(ANSI_COLOR_GREEN "PASSWORD: " ANSI_COLOR_RESET);
    fflush(stdout);

    // citim password de la tastatura
    char password[100];
    bzero(password, 100);

    int pass_len = read(0, password, 100);
    if (pass_len == -1)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare citire password!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // trimitem cererea de LOGIN
    if (write(socket, "LOGIN", 50) <= 0)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare la trimiterea request login!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // asteptam raspuns pentru a incepe LOGIN

    char response[50] = "\0";
    bzero(response, 50);

    int response_len;
    response_len = read(socket, response, 50);
    if (response_len == -1)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare la citirea raspunsului primit!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }
    response[response_len] = '\0';

    // daca primim NO atunci a fost refuzata cererea
    if (strcmp(response, "NO") == 0)
    {
        perror(ANSI_COLOR_RED "LOGIN : Am fost refuzati!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // NU AM FOST REFUZATI SI CONTINUAM
    // generam sirul care contine credentialele
    char credentials[250] = "\0";
    bzero(credentials, 250);

    username[username_len - 1] = '\0';
    password[pass_len - 1] = '\0';
    strcat(credentials, username);
    strcat(credentials, "¥");
    strcat(credentials, password);

    // trimitem sirul catre server care contine user-ul si parola
    if (write(socket, credentials, 250) <= 0)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare la trimiterea credentialelor!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // daca am ajuns aici inseamna ca am trimis cu succes credentialele
    // acum citim raspunsul final -> daca s-a reusit conectarea sau nu
    bzero(response, 50);
    response_len = read(socket, response, 50);
    if (response_len == -1)
    {
        perror(ANSI_COLOR_RED "LOGIN : Eroare la citirea raspunsului final!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }
    response[response_len] = '\0';

    if (strcmp(response, "OK") == 0)
    {
        strcpy(USER, username); // copiem in USER numele cu care am reusit sa ne conectam
        return 1;               // returnam 1 adica am reusit conectarea
    }
    else
    {
        return -1; // returnam -1 adica conectarea a esuat
    }
}
// aici citim user-ul si parola de la tastatura, le trimitem catre server si asteptam raspuns inapoi pentru inregistrare
int TRY_REGISTER(int socket)
{
    // pregatim interfata
    CLEAR_SCREEN();
    SHOW_LOGO();
    printf(ANSI_COLOR_GREEN "\n\n\nUSERNAME: " ANSI_COLOR_RESET);
    fflush(stdout);

    // citim username-ul de la tastatura
    char username[100];
    bzero(username, 100);
    int username_len = read(0, username, 100);
    if (username_len == -1)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare citire username!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    printf(ANSI_COLOR_GREEN "PASSWORD: " ANSI_COLOR_RESET);
    fflush(stdout);

    // citim password de la tastatura
    char password[100];
    bzero(password, 100);
    int pass_len = read(0, password, 100);
    if (pass_len == -1)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare citire password!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // trimitem cererea de REGISTER
    if (write(socket, "REGISTER", 50) <= 0)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare citire username!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // asteptam raspuns pentru a incepe REGISTER
    char response[50];
    bzero(response, 50);
    int response_len;
    response_len = read(socket, response, 50);
    if (response_len == -1)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare la citirea raspunsului primit!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }
    response[response_len] = '\0';
    // daca primim code100 atunci a fost refuzata cererea
    if (strcmp(response, "NO") == 0)
    {
        perror(ANSI_COLOR_RED "REGISTER : Am fost refuzati!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // nu am fost refuzati si continuam
    // generam sirul care contine credentialele si le trimitem serverului pentru REGISTER

    char credentials[250];
    bzero(credentials, 250);

    username[username_len - 1] = '\0';
    password[pass_len - 1] = '\0';
    strcat(credentials, username);
    strcat(credentials, "¥");
    strcat(credentials, password);

    // trimitem sirul catre server
    if (write(socket, credentials, 250) <= 0)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare la trimiterea credentialelor!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }

    // daca am ajuns aici inseamna ca am trimis cu succes credentialele
    // acum citim raspunsul final -> daca s-a reusit inregistrarea sau nu
    bzero(response, 50);
    response_len = read(socket, response, sizeof(response));
    if (response_len == -1)
    {
        perror(ANSI_COLOR_RED "REGISTER : Eroare la citirea raspunsului final!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }
    response[response_len] = '\0';

    if (strcmp(response, "OK") == 0)
    {
        strcpy(USER, username); // copiem in USER numele cu care am reusit sa ne conectam
        return 1;               // returnam 1 adica am reusit inregistrarea
    }
    else
    {
        return -1; // returnam -1 adica inregistrarea a esuat
    }
}
// citim textulu chat-ului trimis de server si il afisam formatat (culori,daca e reply sau msg, etc)
int READ_WHOLE_CHAT_AND_PRINT(int socket)
{
    char chat[10000] = "\0";
    bzero(chat, 10000);

    int chat_len = read(socket, chat, 10000);

    if (chat_len <= 0)
    {
        perror(ANSI_COLOR_RED "READ-CHAT : Eroare la citirea chat-ului!" ANSI_COLOR_RESET);
        close(socket);
        exit(0);
    }
    chat[chat_len] = '\0';

    char nr[100] = "\0";   // numarul mesajului
    char user[100] = "\0"; // nummele celui care a trimis mesajul
    char msg[100] = "\0";  // mesajul

    char *end_chat;
    char *token = strtok_r(chat, "\n", &end_chat);

    while (token != NULL)
    {
        char *end_token;
        char *token2 = strtok_r(token, "¥", &end_token);
        strcpy(nr, token2);

        while (token2 != NULL)
        {
            token2 = strtok_r(NULL, "¥", &end_token);
            if (token2 != NULL)
            {
                strcpy(user, token2);
            }
            else
            {
                break;
            }

            token2 = strtok_r(NULL, "¥", &end_token);
            if (token2 != NULL)
            {
                strcpy(msg, token2);
            }
            else
            {
                break;
            }

            if (strcmp(USER, user) == 0)
            {
                if (strcmp(nr, "R") == 0)
                {
                    printf(WHT "(%s) [YOU] : %s\n" ANSI_COLOR_RESET, nr, msg);
                    fflush(stdout);
                }
                else
                {
                    printf(ANSI_COLOR_YELLOW "(%s) [YOU] : %s\n" ANSI_COLOR_RESET, nr, msg);
                    fflush(stdout);
                }
            }
            else
            {
                if (strcmp(nr, "R") == 0)
                {
                    printf(WHT "(%s) [%s] : %s\n" ANSI_COLOR_RESET, nr, user, msg);
                    fflush(stdout);
                }
                else
                {
                    printf(ANSI_COLOR_BLUE "(%s) [%s] : %s\n" ANSI_COLOR_RESET, nr, user, msg);
                    fflush(stdout);
                }
            }
        }

        token = strtok_r(NULL, "\n", &end_chat);
    }

    return 1;
}

int LOGGED_IN = 0;         // tinem cont daca suntem logati sau nu (initial nu suntem)
extern int errno;          // codul de eroare returnat de anumite apeluri
int socket_descriptor;     // descriptorul de socket
struct sockaddr_in server; // structura folosita pentru conectare
pthread_t tid;             // id-ul thread-ului

void *receive_MSG_THREAD(void *socket_desc)
{

    int sock = *(int *)socket_desc;
    char messages[600];
    char option[10];
    while (1)
    {

        bzero(messages, 600);
        bzero(option, 10);

        int option_len = read(sock, option, 10);
        if (option_len <= 0)
        {
            perror("RECEIVE_MSG_THREAD : Eroare la citirea optiunii!");
            close(sock);
            pthread_exit(NULL);
        }
        option[option_len] = '\0';

        if (strcmp(option, "msg") == 0) // optiunea e msg
        {
            int msg_len = read(sock, messages, 600);
            if (msg_len <= 0)
            {
                perror("RECEIVE_MSG_THREAD : Eroare la citirea mesajului!");
                close(sock);
                pthread_exit(NULL);
            }
            messages[msg_len] = '\0'; // mesajul intreg primit
            char nr[100] = "\0";      // numarul mesajului
            char user[100] = "\0";    // numele celui care a trimis mesajul
            char msg[100] = "\0";     // mesajul propriu-zis

            char *p = strtok(messages, "¥");
            strcpy(nr, p);

            p = strtok(NULL, "¥");
            strcpy(user, p);

            p = strtok(NULL, "¥");
            strcpy(msg, p);

            if (strcmp(user, USER) == 0)
            {
                printf(ANSI_COLOR_YELLOW "(%s) [YOU] : %s\n" ANSI_COLOR_RESET, nr, msg);
                fflush(stdout);
            }
            else
            {
                printf(ANSI_COLOR_BLUE "(%s) [%s] : %s\n" ANSI_COLOR_RESET, nr, user, msg);
                fflush(stdout);
            }
        }

        else if (strcmp(option, "reply") == 0)
        {
            int reply_len = read(sock, messages, 600);
            if (reply_len <= 0)
            {
                perror("RECEIVE_MSG_THREAD : Eroare la citirea reply-ului!");
                close(sock);
                pthread_exit(NULL);
            }
            messages[reply_len] = '\0';
            char nr[100] = "\0";
            char user[100] = "\0";
            char msg[100] = "\0";

            char ultima[300] = "\0";
            char penultima[300] = "\0";

            char *p = strtok(messages, "Ø");
            strcpy(penultima, p);
            p = strtok(NULL, "Ø");
            strcpy(ultima, p);

            p = strtok(penultima, "¥");
            strcpy(nr, p);

            p = strtok(NULL, "¥");
            strcpy(user, p);

            p = strtok(NULL, "¥");
            strcpy(msg, p);

            if (strcmp(user, USER) == 0)
            {
                printf(WHT "(%s) [YOU] : %s\n" ANSI_COLOR_RESET, nr, msg);
                fflush(stdout);
            }
            else
            {
                printf(WHT "(%s) [%s] : %s\n" ANSI_COLOR_RESET, nr, user, msg);
                fflush(stdout);
            }

            // ultimul mesaj
            p = strtok(ultima, "¥");
            strcpy(nr, p);

            p = strtok(NULL, "¥");
            strcpy(user, p);

            p = strtok(NULL, "¥");
            strcpy(msg, p);
            if (strcmp(user, USER) == 0)
            {
                printf(ANSI_COLOR_YELLOW "(%s) [YOU] : %s\n" ANSI_COLOR_RESET, nr, msg);
                fflush(stdout);
            }
            else
            {
                printf(ANSI_COLOR_BLUE "(%s) [%s] : %s\n" ANSI_COLOR_RESET, nr, user, msg);
                fflush(stdout);
            }
        }

        else if (strcmp(option, "invalid") == 0)
        {
            printf(ANSI_COLOR_RED "Invalid Message/Reply!\n" ANSI_COLOR_RESET);
            fflush(stdout);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{

    // afisam meniul de la inceputul clientului
    CLEAR_SCREEN();
    SHOW_LOGO();
    SHOW_OPTIONS();

    // Creare Socket
    if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror(ANSI_COLOR_RED "CLIENT MAIN : EROARE LA CREARE SOCKET!\n" ANSI_COLOR_RESET);
        return errno;
    }

    // Umplem structura folosita pentru realizarea conexiunii
    server.sin_family = AF_INET;            // Protocol IPV4
    server.sin_addr.s_addr = inet_addr(IP); // IP-ul dispozitivului(serverului) (cazul nostru local)
    server.sin_port = htons(PORT);          // Portul la care ne conectam

    // Incercam conectarea la server
    if (connect(socket_descriptor, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror(ANSI_COLOR_RED "CLIENT MAIN : EROARE LA CONECTAREA LA SERVER!\n" ANSI_COLOR_RESET);
        return errno;
    }

    // LOGIN / REGISTER / EXIT
    while (1)
    {
        // aici scriem optiunea <1> <2> sau <3>
        char option;
        option = getchar();
        int result;

        if (option == '1')
        {
            result = TRY_LOGIN(socket_descriptor); // incercam LOGIN
            if (result == 1)                       // conectare cu succes
            {
                CLEAR_SCREEN();
                SHOW_PANEL_MAIN_COMMANDS(USER);
                LOGGED_IN = 1;
                break;
            }
            else // conectare esuata
            {
                CLEAR_SCREEN();
                printf(ANSI_COLOR_RED "LOGIN FAILED!\n\n" ANSI_COLOR_RESET);
                SHOW_LOGO();
                SHOW_OPTIONS();
                LOGGED_IN = 0;
            }
        }
        else if (option == '2')
        {
            result = TRY_REGISTER(socket_descriptor); // incercam REGISTER
            if (result == 1)                          // inregistrare cu succes
            {
                CLEAR_SCREEN();
                SHOW_PANEL_MAIN_COMMANDS(USER);
                LOGGED_IN = 1;
                break;
            }
            else // inregistrare esuata
            {
                CLEAR_SCREEN();
                printf(ANSI_COLOR_RED "FAILED TO REGISTER\n\n" ANSI_COLOR_RESET);
                SHOW_LOGO();
                SHOW_OPTIONS();
                LOGGED_IN = 0;
            }
        }
        else if (option == '3') // incercam inchiderea clientului
        {
            // inchidem clientul
            if (write(socket_descriptor, "EXIT", 50) <= 0)
            {
                perror(ANSI_COLOR_RED "EXIT : Eroare la inchiderea clientului!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }
            CLEAR_SCREEN();
            printf(ANSI_COLOR_RED "\nCLIENT CLOSED!\n" ANSI_COLOR_RESET);
            close(socket_descriptor);
            exit(0);
        }
        else if ((option != ' ') && (option != '\n')) // comanda invalida si reluam
        {
            CLEAR_SCREEN();
            SHOW_LOGO();
            SHOW_OPTIONS();
        }
    }
    // SUNTEM CONECTATI SAU INREGISTRATI!
    while (LOGGED_IN == 1)
    {

        // MENIUL DE COMENZI
        char command[100];
        bzero(command, 100);
        int command_len;

        printf("\nCOMMAND: ");
        fflush(stdout);

        command_len = read(0, command, 100);
        if (command_len == -1)
        {
            perror(ANSI_COLOR_RED "COMMAND : Eroare la citirea comenzii din panoul principal!" ANSI_COLOR_RESET);
            close(socket_descriptor);
            return errno;
        }
        command[command_len - 1] = '\0';

        if (strcmp(command, "show-all-users") == 0)
        {
            CLEAR_SCREEN();
            SHOW_CONNECTED_AS(USER);
            printf(ANSI_COLOR_YELLOW " : ALL USERS\n\n" ANSI_COLOR_RESET);

            // aici trimitem comanda
            if (write(socket_descriptor, "show-all-users", 100) == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la trimiterea comenzii 'show-all-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            // aici primim lista cu useri
            char users[1000] = "\0";
            bzero(users, 1000);

            int len_users = read(socket_descriptor, users, 1000);
            if (len_users == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la citirea raspunsului 'show-all-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }
            users[len_users] = '\0';

            printf(ANSI_COLOR_YELLOW "--------------------------------\n" ANSI_COLOR_RESET);

            char *user = strtok(users, " ¥\n");
            while (user != NULL)
            {

                printf(ANSI_COLOR_YELLOW "%s\n" ANSI_COLOR_RESET, user);
                fflush(stdout);
                user = strtok(NULL, "¥\n");
            }
            printf(ANSI_COLOR_YELLOW "--------------------------------\n" ANSI_COLOR_RESET);

            continue;
        }
        else if (strcmp(command, "show-online-users") == 0)
        {
            CLEAR_SCREEN();
            SHOW_CONNECTED_AS(USER);
            printf(ANSI_COLOR_GREEN " : ONLINE USERS\n\n" ANSI_COLOR_RESET);

            // aici trimitem comanda
            if (write(socket_descriptor, "show-online-users", 100) == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la trimiterea comenzii 'show-online-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            // aici primim lista cu useri
            char online_users[1000] = "\0";
            bzero(online_users, 1000);
            int len_online_users = read(socket_descriptor, online_users, 1000);
            if (len_online_users == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la citirea raspunsului 'show-online-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }
            online_users[len_online_users] = '\0';

            printf(ANSI_COLOR_GREEN "-----------------------------------\n" ANSI_COLOR_RESET);

            char *user = strtok(online_users, " ¥\n");
            while (user != NULL)
            {

                printf(ANSI_COLOR_GREEN "%s\n" ANSI_COLOR_RESET, user);
                fflush(stdout);
                user = strtok(NULL, "¥\n");
            }
            printf(ANSI_COLOR_GREEN "-----------------------------------\n" ANSI_COLOR_RESET);

            continue;
        }
        else if (strcmp(command, "show-offline-users") == 0)
        {
            CLEAR_SCREEN();
            SHOW_CONNECTED_AS(USER);
            printf(ANSI_COLOR_RED " : OFFLINE USERS\n\n" ANSI_COLOR_RESET);

            // aici trimitem comanda
            if (write(socket_descriptor, "show-offline-users", 100) == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la trimiterea comenzii 'show-offline-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            // aici primim lista cu useri
            char offline_users[1000] = "\0";
            bzero(offline_users, 1000);
            int len_offline_users = read(socket_descriptor, offline_users, 1000);
            if (len_offline_users == -1)
            {
                perror(ANSI_COLOR_RED "COMMAND : Eroare la citirea raspunsului 'show-offline-users'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }
            offline_users[len_offline_users] = '\0';

            char *user = strtok(offline_users, " ¥\n");

            printf(ANSI_COLOR_RED "------------------------------------\n" ANSI_COLOR_RESET);
            while (user != NULL)
            {

                printf(ANSI_COLOR_RED "%s\n" ANSI_COLOR_RESET, user);
                fflush(stdout);
                user = strtok(NULL, "¥\n");
            }
            printf(ANSI_COLOR_RED "------------------------------------\n" ANSI_COLOR_RESET);

            continue;
        }
        else if (strcmp(command, "show-notifications") == 0)
        {
            CLEAR_SCREEN();
            SHOW_CONNECTED_AS(USER);
            printf(ANSI_COLOR_CYAN " : NOTIFICATIONS\n\n" ANSI_COLOR_RESET);

            // aici trimitem comanda
            if (write(socket_descriptor, "show-notifications", 100) == -1)
            {
                perror("eroare trimitere show-notifications");
            }

            // aici primim lista cu useri
            char notifications[1000] = "\0";
            int len_notifications = read(socket_descriptor, notifications, 1000);
            if (len_notifications == -1)
            {
                perror("Eroare citire users!");
            }
            notifications[len_notifications] = '\0';

            char *user = strtok(notifications, " ¥\n");

            printf(ANSI_COLOR_BLUE "------------------------------------\n" ANSI_COLOR_RESET);

            while (user != NULL)
            {
                printf("Ai primit notificare de la ");
                printf(ANSI_COLOR_CYAN "%s\n" ANSI_COLOR_RESET, user);
                fflush(stdout);
                user = strtok(NULL, "¥\n");
            }
            printf(ANSI_COLOR_BLUE "------------------------------------\n" ANSI_COLOR_RESET);

            continue;
        }
        else if (strcmp(command, "all-commands") == 0)
        {
            CLEAR_SCREEN();
            SHOW_PANEL_MAIN_COMMANDS(USER);
            continue;
        }
        else if (strcmp(command, "exit-app") == 0)
        {
            CLEAR_SCREEN();
            printf(ANSI_COLOR_RED ": CLIENT CLOSED!\n" ANSI_COLOR_RESET);

            // aici trimitem comanda
            if (write(socket_descriptor, "exit-app", 100) == -1)
            {
                perror(ANSI_COLOR_RED "EXIT-APP : Eroare la trimiterea comenzii 'exit-app'!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            break; // iesim din loop si mergem la pthread_join
        }

        // CHAT---------------------------------------------
        else if (strstr(command, "start:") != NULL)
        {
            // aici trimitem cererea
            if (write(socket_descriptor, command, 100) == -1)
            {
                perror(ANSI_COLOR_RED "START:CLIENT : Eroare la  trimiterea comenzii!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            // aici primim raspunul daca comanda si userul sunt valide
            char start_chat_response[10];
            bzero(start_chat_response, 10);
            int len_start_chat_response = read(socket_descriptor, start_chat_response, 2);
            if (len_start_chat_response == -1)
            {
                perror(ANSI_COLOR_RED "START:CLIENT : Eroare la  citirea raspunsului!" ANSI_COLOR_RESET);
                close(socket_descriptor);
                return errno;
            }

            start_chat_response[len_start_chat_response] = '\0';

            // avem o comanda trimisa valida de chat
            if (strcmp(start_chat_response, "OK") == 0)
            {

                char *p = strtok(command, " \n:");
                p = strtok(NULL, " \n:"); // luam destinatarul

                CLEAR_SCREEN();
                printf(ANSI_COLOR_YELLOW "[%s] : " ANSI_COLOR_RESET, USER);
                printf(ANSI_COLOR_GREEN "SEND MESSAGE TO : " ANSI_COLOR_RESET);
                printf(ANSI_COLOR_BLUE "[%s]\n\n" ANSI_COLOR_RESET, p);
                printf(ANSI_COLOR_GREEN "------------------------------\n" ANSI_COLOR_RESET);
                fflush(stdout);

                // aici citim citim si afisam intreg chatu-ul pana la acel moment

                READ_WHOLE_CHAT_AND_PRINT(socket_descriptor);

                int ssocket = socket_descriptor;
                pthread_create(&tid, NULL, &receive_MSG_THREAD, (void *)&ssocket); // aici se creeaza thread-ul pentru chat

                // continuare main thread (aici doar trimitem mesaje)
                char msg[200] = "\0";

                while (1)
                {

                    bzero(msg, 200);

                    int msg_len = read(0, msg, 200);

                    printf("\033[F\r"); // stergem sirul citit imediat dupa afisare

                    //citim mesajul de la tastatura
                    if (msg_len == -1)
                    {
                        perror(ANSI_COLOR_RED "CLIENT-TRIMITERE-MESAJ : Eroare la  citirea raspunsului!" ANSI_COLOR_RESET);
                        close(socket_descriptor);
                        return errno;
                    }
                    msg[msg_len - 1] = '\0';

                    //trimitem meajul serverului pentru procesare
                    if (write(socket_descriptor, msg, 200) <= 0)
                    {
                        perror("Eroare la trimiterea credentials!\n");
                        close(socket_descriptor);
                        return errno;
                    }

                    if (strcmp(msg, "exit-chat") == 0)
                    {
                        CLEAR_SCREEN();
                        SHOW_PANEL_MAIN_COMMANDS(USER);
                        pthread_cancel(tid);
                        break;
                    }
                }
            }
            else
            {
                printf(ANSI_COLOR_RED "<Eroare la start:user> INVALID" ANSI_COLOR_RESET);
                fflush(stdout);
                continue;
            }
        }
        // CHAT---------------------------------------------

        else // cazul in care comanda este invalida
        {
            CLEAR_SCREEN();
            SHOW_PANEL_MAIN_COMMANDS(USER);
            printf(ANSI_COLOR_RED "\nInvalid Command!" ANSI_COLOR_RESET);
            fflush(stdout);
        }
    }

    pthread_join(tid, NULL);  // main thread-ul asteapta ca celalat thread sa se finalizeze
    close(socket_descriptor); // inchidem conexiunea cu serverul

    return 0;
}