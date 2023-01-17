// SERVER
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define PORT 3000

// coduri de culoare
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

extern int errno;          // codul de eroare returnat de anumite apeluri
struct sockaddr_in server; // structura folosita de server
struct sockaddr_in from;   // aici vedem informatiile despre clientul conectat
int socket_descriptor;     // descriptorul de socket

int i = 0; // counter pentru thread-uri

struct client
{
    int socket_descriptor;
    int online;
    char username[100];
    char speaking_to[100]; // user-ul cu care ne aflam in chat

} clients[100];

pthread_t threads[100];
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

// verificam daca user-ul este online dupa nume in clients[]
int check_user_online(char *username)
{

    for (int k = 0; k < i; k++)
    {
        if (strcmp(clients[k].username, username) == 0)
        {
            if (clients[k].online == 1)
                return 1;
        }
    }
    return -1;
}
// functia care creeaza baza de date / fisierele necesare
void CREATE_DATABASE()
{
    FILE *fp;

    fp = fopen("login_users.txt", "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CREATE_DATABASE : EROARE LA CREARE 'login_users.txt'!\n" ANSI_COLOR_RESET);
        exit(0);
    }
    fclose(fp);

    fp = fopen("notifications.txt", "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CREATE_DATABASE : EROARE LA CREARE 'notifications.txt'!\n" ANSI_COLOR_RESET);
        exit(0);
    }
    fclose(fp);

    fp = fopen("online_status.txt", "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CREATE_DATABASE : EROARE LA CREARE 'online_status.txt'!\n" ANSI_COLOR_RESET);
        exit(0);
    }
    fclose(fp);

    fp = fopen("all_users.txt", "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CREATE_DATABASE : EROARE LA CREARE 'all_users.txt'!\n" ANSI_COLOR_RESET);
        exit(0);
    }
    fclose(fp);
}
// functia sterge notificarea primita de 'user' de la 'from'
int DELETE_NOTIFICATION(char *user, char *from)
{
    char file_text[5000] = "\0";
    char compare_text[200] = "\0";

    // aici se creeaza notificare pe care o folosim la comparare : ex : user¥user2
    strcat(compare_text, user);
    strcat(compare_text, "¥");
    strcat(compare_text, from);

    FILE *fp = fopen("notifications.txt", "r+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "DELETE_NOTIFICATION : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char chunk[200];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {

        char *p = strtok(chunk, " \n");

        if (strcmp(p, compare_text) != 0)
        {
            strcat(file_text, p);
            strcat(file_text, "\n");
        }
    }

    fp = freopen(NULL, "w", fp);

    fputs(file_text, fp); // rescriem notificarile fara cea pe care am sters-o

    fclose(fp);
    return 1;
}
// functia adauga in fisier notificarea de la 'from' catre 'user'
int ADD_NOTIFICATION(char *user, char *from)
{
    char notifcation[200] = "\0";

    FILE *fp = fopen("notifications.txt", "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "ADD_NOTIFICATION : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char chunk[200];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, " ¥\n");

        if (strcmp(p, user) == 0)
        {
            p = strtok(NULL, " ¥\n");
            if (strcmp(p, from) == 0)
            {
                return -1;
            }
        }
    }

    strcat(notifcation, user);
    strcat(notifcation, "¥");
    strcat(notifcation, from);

    fputs(notifcation, fp);
    fputs("\n", fp);

    fclose(fp);
    return 1;
}
// functia care scrie catre client lista useri de la care are notificari
void WRITE_NOTIFICATIONS_TO_CLIENT(int socket, char *username)
{

    char notifications[1000] = "\0";

    FILE *fp = fopen("notifications.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "WRITE_NOTIFICATIONS_TO_CLIENT : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    char chunk[128];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, " ¥\n");
        if (p == NULL)
            continue;

        if (strcmp(p, username) == 0)
        {
            p = strtok(NULL, " ¥\n");
            strcat(notifications, p);
            strcat(notifications, "¥");
        }
    }

    if (write(socket, notifications, 1000) <= 0)
    {
        perror(ANSI_COLOR_RED "WRITE_NOTIFICATIONS_TO_CLIENT : Eroare la trimiterea notificarilor!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    fclose(fp);
}
// functia care scrie catre client lista cu useri offline
void WRITE_OFFLINE_USER_LIST_TO_CLIENT(int socket)
{

    char users[1000] = "\0";

    FILE *fp = fopen("online_status.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "WRITE_OFFLINE_USER_LIST_TO_CLIENT : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    char chunk[120];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {

        // verificam pointerii sa nu fie NULL
        char *p = strtok(chunk, " ¥\n");
        if (p == NULL)
            continue;
        char *status = strtok(NULL, " ¥\n");
        if (status == NULL)
            continue;

        // daca status-ul este 0 atunci vom atasa doar userii offline
        if (strcmp(status, "0") == 0)
        {
            strcat(users, p);
            strcat(users, "¥");
        }
    }

    if (write(socket, users, 1000) <= 0)
    {
        perror(ANSI_COLOR_RED "WRITE_NOTIFICATIONS_TO_CLIENT : Eroare la trimiterea listei cu userii offline!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    fclose(fp);
}
// functia care scrie catre client lista cu useri online
void WRITE_ONLINE_USER_LIST_TO_CLIENT(int socket)
{

    char users[1000] = "\0";

    FILE *fp = fopen("online_status.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "WRITE_ONLINE_USER_LIST_TO_CLIENT : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    char chunk[120];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        // aici verificam pointerii sa nu fie NULL
        char *p = strtok(chunk, " ¥\n");
        if (p == NULL)
            continue;
        char *status = strtok(NULL, " ¥\n");
        if (status == NULL)
            continue;

        if (strcmp(status, "1") == 0)
        {
            strcat(users, p);
            strcat(users, "¥");
        }
    }

    if (write(socket, users, 1000) <= 0)
    {
        perror(ANSI_COLOR_RED "WRITE_NOTIFICATIONS_TO_CLIENT : Eroare la trimiterea listei cu userii online!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    fclose(fp);
}
// functia care scrie catre client lista cu toti userii
void WRITE_USER_LIST_TO_CLIENT(int socket)
{

    char users[1000] = "\0";

    FILE *fp = fopen("all_users.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "WRITE_USER_LIST_TO_CLIENT : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    char chunk[128];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, " ¥\n");
        if (p == NULL)
            continue;

        strcat(users, p);
        strcat(users, "¥");
    }

    if (write(socket, users, 1000) <= 0)
    {
        perror(ANSI_COLOR_RED "WRITE_USER_LIST_TO_CLIENT : Eroare la trimiterea listei cu userii!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    fclose(fp);
}
// verificam user si parola in baza de date
int CHECK_LOGIN(char *username, char *password)
{
    if (strcmp(username, "\0") == 0 || strcmp(password, "\0") == 0 || strstr(username, " ") != NULL || strstr(password, " ") != NULL)
        return -1;

    if (check_user_online(username) == 1) // daca contul respectiv este deja conectat atunci nu ne mai putem conecta din nou
        return -1;

    FILE *fp = fopen("login_users.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CHECK_LOGIN : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }
    char chunk[250];

    // dupa ce gasim un user care se potriveste, verificam si parola
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, "¥\n");

        if (strcmp(p, username) != 0)
        {
            continue;
        }

        p = strtok(NULL, "¥\n");

        if (strcmp(p, password) == 0)
        {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return -1;
}
// verificam daca user-ul exista deja in baza de date, daca nu il adaugam
int CHECK_REGISTER(char *username, char *password)
{

    if (strcmp(username, "\0") == 0 || strcmp(password, "\0") == 0 || strstr(username, " ") != NULL || strstr(password, " ") != NULL)
        return -1;

    FILE *fp1 = fopen("login_users.txt", "a+");
    FILE *fp2 = fopen("all_users.txt", "a+");
    if (fp1 == NULL)
    {
        perror(ANSI_COLOR_RED "CHECK_REGISTER : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    if (fp2 == NULL)
    {
        perror(ANSI_COLOR_RED "CHECK_REGISTER : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }
    char chunk[250];
    while (fgets(chunk, sizeof(chunk), fp2) != NULL)
    {
        char *p = strtok(chunk, "\n");

        if (strcmp(p, username) == 0) // daca exista deja user-ul, nu ne putem inregistra
        {
            fclose(fp1);
            fclose(fp2);
            return -1;
        }
    }
    // adaugam in fisier noul user
    fputs(username, fp2);
    fputs("\n", fp2);
    fputs(username, fp1);
    fputs("¥", fp1);
    fputs(password, fp1);
    fputs("\n", fp1);

    fclose(fp1);
    fclose(fp2);
    return 1;
}
// verificam in baza de date daca user-ul este online sau offline, iar daca este offline il punem online
int SHOW_ONLINE(char *username)
{
    char file_text[5000] = "\0";

    char compare_online[120] = "\0";
    strcat(compare_online, username);
    strcat(compare_online, "¥");
    strcat(compare_online, "1");

    char compare_offline[120] = "\0";
    strcat(compare_offline, username);
    strcat(compare_offline, "¥");
    strcat(compare_offline, "0");

    FILE *fp = fopen("online_status.txt", "r+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "SHOW_ONLINE : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }
    char chunk[120];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {

        char *p = strtok(chunk, " \n");

        if (strcmp(p, compare_online) == 0)
        {
            fclose(fp);
            return 1;
        }
        else if (strcmp(p, compare_offline) != 0)
        {
            strcat(file_text, p);
            strcat(file_text, "\n");
        }
    }

    fp = freopen(NULL, "w", fp);

    strcat(file_text, compare_online);
    strcat(file_text, "\n");
    fputs(file_text, fp);

    fclose(fp);
    return 1;
}
// verificam in baza de date daca user-ul este online sau offline, iar daca este offline il punem offline
int SHOW_OFFLINE(char *username)
{
    char file_text[5000] = "\0";

    char compare_online[120] = "\0";
    strcat(compare_online, username);
    strcat(compare_online, "¥");
    strcat(compare_online, "1");

    char compare_offline[120] = "\0";
    strcat(compare_offline, username);
    strcat(compare_offline, "¥");
    strcat(compare_offline, "0");

    FILE *fp = fopen("online_status.txt", "r+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "SHOW_OFFLINE : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }
    char chunk[120];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {

        char *p = strtok(chunk, " \n");

        if (strcmp(p, compare_offline) == 0)
        {
            fclose(fp);
            return 1;
        }
        else if (strcmp(p, compare_online) != 0)
        {
            strcat(file_text, p);
            strcat(file_text, "\n");
        }
    }

    fp = freopen(NULL, "w", fp);

    strcat(file_text, compare_offline);
    strcat(file_text, "\n");
    fputs(file_text, fp);

    fclose(fp);
    return 1;
}
// verificam daca user-ul exista in baza de date
int CHECK_USER_EXISTS(char *username)
{

    FILE *fp = fopen("all_users.txt", "r");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "CHECK_USER_EXISTS : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }
    char chunk[120];
    bzero(chunk, 120);
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, " \n");

        if (strcmp(p, username) == 0)
        {
            fclose(fp);
            return 1;
        }
        bzero(chunk, 120);
    }
    fclose(fp);
    return -1;
}
// aceasta functie trimite istoricul de mesaje catre client
int SEND_WHOLE_CHAT(char *expeditor, char *destinatar, int socket)
{

    // deschidem fisierul aici
    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }

    char chat[10000] = "\0";

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "SEND_WHOLE_CHAT : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    char chunk[250];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        strcat(chat, chunk); // construim tot textul
    }

    if (write(socket, chat, 10000) <= 0)
    {
        perror(ANSI_COLOR_RED "SEND_WHOLE_CHAT : Eroare la trimiterea intregului istoric de chat!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }

    fclose(fp);

    return 1;
}
// aceasta functie scrie in chat ultimul mesaj trimis
void UPDATE_CHAT_NEW_MSG(char *expeditor, char *destinatar, char *mesaj)
{
    // deschidem fisierul aici

    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }

    char new_msg[250] = "\0";
    char nr_msg[10] = "\0";

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "UPDATE_CHAT_NEW_MSG : Eroare la deschiderea fisierului!" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    // aici verificam ultimul nr de mesaj
    char chunk[250];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        char *p = strtok(chunk, "¥");
        if (strcmp(p, "R") != 0)
        {
            strcpy(nr_msg, p);
        }
    }

    // crestem nr de mesaj cu 1
    int n = atoi(nr_msg);
    n = n + 1;
    sprintf(nr_msg, "%d", n);

    strcat(new_msg, "¥");
    strcat(new_msg, nr_msg);
    strcat(new_msg, "¥");
    strcat(new_msg, expeditor);
    strcat(new_msg, "¥");
    strcat(new_msg, mesaj);
    strcat(new_msg, "¥");
    fputs(new_msg, fp);
    fputs("\n", fp);

    fclose(fp);
}
// aceasta functie scrie in chat ultimul mesaj trimis si reply-ul
void UPDATE_CHAT_NEW_REPLY(char *expeditor, char *destinatar, char *mesaj)
{
    // aici deschidem fisierul
    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }

    char msg_to_reply[250] = "\0";
    char nr_to_reply[10] = "\0";
    char reply[250] = "\0";
    char reply_msg[250] = "\0";

    char *t = strtok(mesaj, ":");
    t = strtok(NULL, ":");
    strcpy(nr_to_reply, t);
    t = strtok(NULL, ":");
    strcpy(reply_msg, t);

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "UPDATE_CHAT_NEW_REPLY : Eroare la deschiderea fisierului!" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char aux[250] = "\0";
    char chunk[250];
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        strcpy(aux, chunk);
        char *p = strtok(chunk, "¥");
        if (strcmp(p, nr_to_reply) == 0)
        {
            break;
        }
    }

    char msg[250] = "\0";
    char reply_to[200] = "\0";

    char *s = strtok(aux, "¥");
    s = strtok(NULL, "¥");
    strcpy(reply_to, s);
    s = strtok(NULL, "¥");
    strcpy(msg, s);

    fclose(fp);

    fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "UPDATE_CHAT_NEW_REPLY : Eroare la deschiderea fisierului!" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char last_msg_number[10] = "\0";

    char chunkk[250];
    while (fgets(chunkk, sizeof(chunkk), fp) != NULL)
    {
        char *p = strtok(chunkk, "¥");
        if (strcmp(p, "R") != 0)
        {
            strcpy(last_msg_number, p);
        }
    }

    int n = atoi(last_msg_number);
    n = n + 1;

    sprintf(last_msg_number, "%d", n);

    // construim mesajul la care dam reply
    strcat(msg_to_reply, "¥");
    strcat(msg_to_reply, "R");
    strcat(msg_to_reply, "¥");
    strcat(msg_to_reply, reply_to);
    strcat(msg_to_reply, "¥");
    strcat(msg_to_reply, msg);
    strcat(msg_to_reply, "¥");

    // construim reply-ul
    strcat(reply, "¥");
    strcat(reply, last_msg_number);
    strcat(reply, "¥");
    strcat(reply, expeditor);
    strcat(reply, "¥");
    strcat(reply, reply_msg);
    strcat(reply, "¥");

    fputs(msg_to_reply, fp);
    fputs("\n", fp);
    fputs(reply, fp);
    fputs("\n", fp);

    fclose(fp);
}
// aici trimitem optiunea de invalid clientului pentru a-l informa ca mesajul este gresit
void INVALIDATE(int socket)
{
    if (write(socket, "invalid", 10) <= 0)
    {
        perror(ANSI_COLOR_RED "INVALIDATE : Eroare la trimiterea optiunii 'invalid'!" ANSI_COLOR_RESET);
        close(socket);
        pthread_exit(NULL);
    }
}
// aici verificam daca reply-ul este unul valid
int VALIDATE_REPLY(char *reply, char *expeditor, char *destinatar)
{
    // aici accesam fisierul chat-ului
    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }
    char rep[200] = "\0";
    strcpy(rep, reply);

    char nr_to_reply[10] = "\0";
    char msg_to_reply[150] = "\0";

    char *t = strtok(rep, ":");

    if (strcmp(t, "reply") != 0)
        return -1;
    t = strtok(NULL, ":");
    if (t == NULL)
        return -1;
    strcpy(nr_to_reply, t);
    t = strtok(NULL, ":");
    if (t == NULL)
        return -1;
    strcpy(msg_to_reply, t);

    // aici verificam daca mesajul este unul valid
    if (strcmp(msg_to_reply, "") == 0)
        return -1;
    if (strcmp(msg_to_reply, "\0") == 0)
        return -1;
    if (strcmp(msg_to_reply, "\n") == 0)
        return -1;
    if (strstr(msg_to_reply, "¥") != NULL)
        return -1;

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "VALIDATE_REPLY : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char chunk[250] = "\0";
    bzero(chunk, 250);
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        // aici verificam daca numarul mesajului la care dam reply exista
        char *p = strtok(chunk, "¥");
        if (strcmp(p, nr_to_reply) == 0)
        {
            fclose(fp);
            return 1;
        }
    }

    return -1;
    fclose(fp);
}
// aici verificam daca mesajul este unul valid
int VALIDATE_MSG(char *mesaj)
{
    if (strcmp(mesaj, "\n") == 0)
        return -1;
    if (strstr(mesaj, "¥") != NULL)
        return -1;
    if (strcmp(mesaj, "\0") == 0)
        return -1;
    return 1;
}
// functie care arata daca cei doi useri se afla in acelasi chat sau nu
int user1_user2_same_chat(char *user1, char *user2)
{

    int pos_user1 = -1;
    int pos_user2 = -1;

    for (int k = 0; k < i; k++)
    {
        if (strcmp(clients[k].username, user1) == 0)
        {
            pos_user1 = k; // vedem pe ce pozitie in clients[] se afla user 1
        }
        if (strcmp(clients[k].username, user2) == 0)
        {
            pos_user2 = k; // vedem pe ce pozitie in clients[] se afla user 2
        }
    }
    if (pos_user1 == -1 || pos_user2 == -1)
        return -1;

    if ((strcmp(clients[pos_user1].speaking_to, user2) == 0) && (strcmp(clients[pos_user2].speaking_to, user1) == 0))
        return 1;

    return -1;
    // -1 inseamna ca cei doi useri nu se afla in acelasi chat, 1 altfel
}
// functie care seteaza pentru un client numele user-ului cu care se afla in acelasi chat
int set_speaking_to_client_struct(char *s, int cl_sock)
{
    for (int k = 0; k < i; k++)
    {
        if (clients[k].socket_descriptor == cl_sock)
        {
            strcpy(clients[k].speaking_to, s);
            return 1;
        }
    }
    return -1;
}
// functie care seteaza pentru un client numele clientului in clients[]
int set_username_to_client_struct(char *s, int cl_sock)
{
    for (int k = 0; k < i; k++)
    {
        if (clients[k].socket_descriptor == cl_sock)
        {
            strcpy(clients[k].username, s);
            return 1;
        }
    }
    return -1;
}
// functie care seteaza pentru un client statusul, online sau offline in clients[]
int set_status_to_client_struct(int status, int cl_sock)
{
    for (int k = 0; k < i; k++)
    {
        if (clients[k].socket_descriptor == cl_sock)
        {
            clients[k].online = status;
            return 1;
        }
    }

    return -1;
}
// prin aceasta functie trimitem mesajul inpoi la cel care a trimis pentru a fi afisat pe ecran, dar si la destinatar
void send_msg_to_destinatar(char *expeditor, char *destinatar, char *mesaj)
{
    // aici accesam fisierul pentru chat
    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror(ANSI_COLOR_RED "send_msg_to_destinatar : EROARE LA DESCHIDERE FISIER!\n" ANSI_COLOR_RESET);
        pthread_exit(NULL);
    }

    char chunk[600] = "\0";
    char sent[600] = "\0";
    bzero(chunk, 600);
    bzero(sent, 600);

    // aici citim pentru a lua ultimul mesaj scris in fisier
    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        strcpy(sent, chunk);
        bzero(chunk, 600);
    }

    // send message to self
    for (int k = 0; k < i; k++)
    {
        if (strcmp(clients[k].username, expeditor) == 0)
        {
            write(clients[k].socket_descriptor, "msg", 10);
            write(clients[k].socket_descriptor, sent, 600);
            break;
        }
    }

    // send message to destinatar
    if (user1_user2_same_chat(expeditor, destinatar) == 1 && check_user_online(destinatar) == 1)
    {
        for (int k = 0; k < i; k++)
        {
            if (strcmp(clients[k].username, destinatar) == 0)
            {
                write(clients[k].socket_descriptor, "msg", 10);
                write(clients[k].socket_descriptor, sent, 600);
                break;
            }
        }
    }

    else
    {
        // in cazul in care destinatarul nu este in acelasi chat ce expeditorul, adaugam notificare
        ADD_NOTIFICATION(destinatar, expeditor);
    }
}
// prin aceasta functie trimitem mesajul inpoi la cel care a trimis pentru a fi afisat pe ecran, dar si la destinatar impreuna cu reply-ul
void send_reply_to_destinatar(char *expeditor, char *destinatar, char *mesaj)
{
    // aici accesam fisierul chat-ului
    char file_name1[200] = "\0";
    char file_name2[200] = "\0";
    char file_name_final[200] = "\0";

    strcat(file_name1, expeditor);
    strcat(file_name1, "-");
    strcat(file_name1, destinatar);
    strcat(file_name1, ".txt");

    strcat(file_name2, destinatar);
    strcat(file_name2, "-");
    strcat(file_name2, expeditor);
    strcat(file_name2, ".txt");

    if (access(file_name1, F_OK) == 0)
    {
        strcpy(file_name_final, file_name1);
    }
    else if (access(file_name2, F_OK) == 0)
    {
        strcpy(file_name_final, file_name2);
    }
    else
    {
        strcpy(file_name_final, file_name1);
    }

    FILE *fp = fopen(file_name_final, "a+");
    if (fp == NULL)
    {
        perror("Unable to open file!");
        exit(1);
    }

    char chunk[300] = "\0";
    char ultima[300] = "\0";
    char penultima[300] = "\0";
    char sent[600] = "\0";

    while (fgets(chunk, sizeof(chunk), fp) != NULL)
    {
        strcpy(penultima, ultima);
        strcpy(ultima, chunk);
    }
    strcat(sent, penultima);
    strcat(sent, "Ø");
    strcat(sent, ultima);

    // trimitem reply-ul la mesajul destinatarului dar si mesajul lui catre el
    if (user1_user2_same_chat(expeditor, destinatar) == 1 && check_user_online(destinatar) == 1)
    {
        for (int k = 0; k < i; k++)
        {
            if (strcmp(clients[k].username, destinatar) == 0)
            {
                write(clients[k].socket_descriptor, "reply", 10);
                write(clients[k].socket_descriptor, sent, 600);
                break;
            }
        }
    }
    else
    {
        // nu se afla in acelasi chat -> notificare
        ADD_NOTIFICATION(destinatar, expeditor);
    }

    // trimitem reply-ul la mesajul destinatarului dar si mesajul lui catre self

    for (int k = 0; k < i; k++)
    {
        if (strcmp(clients[k].username, expeditor) == 0)
        {
            write(clients[k].socket_descriptor, "reply", 10);
            write(clients[k].socket_descriptor, sent, 600);
            break;
        }
    }
}
// stergem clientul din clients[] folosind un algoritm de mutare spre stanga a elementelor
int delete_client_from_struct(int cl_sock, int *count)
{
    int position = -1;
    for (int k = 0; k < (*count); k = k + 1)
    {
        if (clients[k].socket_descriptor == cl_sock)
        {
            position = k;
            break;
        }
    }

    if (position != -1)
    {

        for (int j = position; j < (*count) - 1; j++)
        {
            clients[j].socket_descriptor = clients[j + 1].socket_descriptor;
            clients[j].online = clients[j + 1].online;
            strcpy(clients[j].username, clients[j + 1].username);
            strcpy(clients[j].speaking_to, clients[j + 1].speaking_to);
            threads[j] = threads[j + 1];
        }

        (*count)--;
        clients[(*count)].socket_descriptor = 0;
        clients[(*count)].online = -1;
        strcpy(clients[(*count)].username, "\0");
        strcpy(clients[(*count)].speaking_to, "\0");

        return 1;
    }
    else
    {
        return -1;
    }
}

// functia executata de thread-uri
static void *thread_func(void *socket_desc)
{

    int client_socket = *(int *)socket_desc;
    pthread_mutex_lock(&mut);

    char USERNAME[100] = "\0"; // username-ul clientului logat pe acest thread
    bzero(USERNAME, 100);
    int LOGGED_IN = 0; // initial clientul nu este logat

    // while pentru LOGIN/REGISTER/EXIT
    while (1)
    {
        char request[50] = "\0"; // request-ul de login, register sau exit
        bzero(request, 50);

        // citim cererea
        int request_len = read(client_socket, request, 50);
        if (request_len == -1)
        {
            // eroare -> nu putem efectua conectarea
            perror(ANSI_COLOR_RED "THREAD : Eroare la request LOGIN/REGISTER/EXIT!" ANSI_COLOR_RESET);
            write(client_socket, "NO", 50);
            break;
        }
        request[request_len] = '\0';

        // am ajuns aici -> putem efectua cererea si trimitem OK inapoi pentru a se efectua login sau register
        if (write(client_socket, "OK", 50) <= 0)
        {
            perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere 'OK'!" ANSI_COLOR_RESET);
            close(client_socket);
            break;
        }

        // procesare login credentials
        if (strcmp(request, "LOGIN") == 0 && LOGGED_IN == 0)
        {
            char credentials[250] = "\0";
            char username[100] = "\0";
            char password[100] = "\0";

            bzero(credentials, 250);
            bzero(username, 100);
            bzero(password, 100);

            int credentials_len = read(client_socket, credentials, 250);
            if (credentials_len == -1)
            {
                perror(ANSI_COLOR_RED "THREAD : Eroare la citire credentials!" ANSI_COLOR_RESET);
                close(client_socket);
                break;
            }
            credentials[credentials_len] = '\0';

            char *p = strtok(credentials, "¥\n");
            if (p != NULL)
                strcpy(username, p);

            p = strtok(NULL, "¥\n");
            if (p != NULL)
                strcpy(password, p);

            // verificare login
            if (CHECK_LOGIN(username, password) == 1)
            {
                if (write(client_socket, "OK", 50) <= 0)
                {
                    perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere 'OK'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }
                LOGGED_IN = 1;
                bzero(USERNAME, 100);
                strcpy(USERNAME, username);
                SHOW_ONLINE(USERNAME);
                set_status_to_client_struct(1, client_socket);          // acum online
                set_username_to_client_struct(USERNAME, client_socket); // aici setam username-ul la client in struct
                break;                                                  // dam break si mergem la [ while(logged_in == 1) ]
            }
            else
            {
                if (write(client_socket, "NO", 50) <= 0)
                {
                    perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere 'NO'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }

                LOGGED_IN = 0;
                set_status_to_client_struct(0, client_socket);
                continue; // daca LOGIN a esuat dam continue sa mai incercam login sau register
            }
        }

        else if (strcmp(request, "REGISTER") == 0 && LOGGED_IN == 0)
        {
            char credentials[250] = "\0";
            char username[100] = "\0";
            char password[100] = "\0";

            bzero(credentials, 250);
            bzero(username, 100);
            bzero(password, 100);

            int credentials_len = read(client_socket, credentials, 250);
            if (credentials_len == -1)
            {
                perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere citire credentials!" ANSI_COLOR_RESET);
                close(client_socket);
                break;
            }
            credentials[credentials_len] = '\0';

            char *p = strtok(credentials, "¥\n");
            if (p != NULL)
                strcpy(username, p);

            p = strtok(NULL, "¥\n");
            if (p != NULL)
                strcpy(password, p);

            // verificare register
            if (CHECK_REGISTER(username, password) == 1)
            {
                if (write(client_socket, "OK", 50) <= 0)
                {
                    perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere 'OK'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }
                LOGGED_IN = 1;
                bzero(USERNAME, 100);
                strcpy(USERNAME, username);
                SHOW_ONLINE(USERNAME);
                set_status_to_client_struct(1, client_socket);          // acum online
                set_username_to_client_struct(USERNAME, client_socket); // aici setam username-ul la client in struct
                break;                                                  // dam break si mergem la [ while(logged_in == 1) ]
            }
            else
            {

                if (write(client_socket, "NO", 50) <= 0)
                {
                    perror(ANSI_COLOR_RED "THREAD : Eroare la trimitere 'NO'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }
                LOGGED_IN = 0;
                set_status_to_client_struct(0, client_socket);
                continue; // daca REGISTER a esuat dam continue sa mai incercam login sau register
            }
        }

        else if (strcmp(request, "EXIT") == 0)
        {
            LOGGED_IN = 0;                                // setam pe 0
            delete_client_from_struct(client_socket, &i); // stergem clientul curent din clients[]
            close(client_socket);                         // inchidem conexiunea
            break;                                        // iesim din loop si mergem la pthread_exit()
        }
    }

    // aici se proceseaza comenzile clientului in cazul in care LOGGED_IN == 1

    while (LOGGED_IN == 1)
    {
        // aici primim comanda de la client

        char command[100];
        bzero(command, 100);
        int command_len;
        command_len = read(client_socket, command, 100);
        if (command_len == -1)
        {
            perror(ANSI_COLOR_RED "LOGGED_IN : Eroare la citire comanda!" ANSI_COLOR_RESET);
            close(client_socket);
            break;
        }
        command[command_len] = '\0';

        printf("comanda primita este: %s\n", command);
        fflush(stdout);

        // aici actionam in functie de fiecare comanda

        if (strcmp(command, "show-all-users") == 0)
        {
            WRITE_USER_LIST_TO_CLIENT(client_socket); // aici trimitem lista cu toti userii
        }

        if (strcmp(command, "show-online-users") == 0)
        {
            WRITE_ONLINE_USER_LIST_TO_CLIENT(client_socket); // aici trimitem lista cu userii online
        }

        if (strcmp(command, "show-offline-users") == 0)
        {
            WRITE_OFFLINE_USER_LIST_TO_CLIENT(client_socket); // aici trimitem lista cu userii offline
        }

        if (strcmp(command, "show-notifications") == 0)
        {
            WRITE_NOTIFICATIONS_TO_CLIENT(client_socket, USERNAME); // aici trimitem lista cu notificari
        }

        if (strstr(command, "start:") != NULL)
        {
            char destinatar[100] = "\0";
            char mesaj[200] = "\0";

            char *p = strtok(command, " \n:");
            p = strtok(NULL, " \n:");

            if (p == NULL)
            {
                perror(ANSI_COLOR_RED "START:CHAT : Eroare pointer NULL!" ANSI_COLOR_RESET);
                close(client_socket);
                break;
            }
            strcpy(destinatar, p);

            if (CHECK_USER_EXISTS(destinatar) == -1 || strcmp(USERNAME, destinatar) == 0) // daca destinatarul nu exista sau incercam sa ne trimitem noua -> eroare
            {

                if (write(client_socket, "NO", 2) <= 0)
                {
                    perror(ANSI_COLOR_RED "START:CHAT : Eroare la trimitere 'NO'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }
                continue;
            }
            else
            {
                if (write(client_socket, "OK", 2) <= 0)
                {
                    perror(ANSI_COLOR_RED "START:CHAT : Eroare la trimitere 'OK'!" ANSI_COLOR_RESET);
                    close(client_socket);
                    break;
                }
            }

            // am ajuns aici -> putem da start la chat
            set_speaking_to_client_struct(destinatar, client_socket); // aici setam chat speaking_to
            SEND_WHOLE_CHAT(USERNAME, destinatar, client_socket);     // aici trimitem tot chat-ul
            DELETE_NOTIFICATION(USERNAME, destinatar);                // acum stergem notificarea pentru ca a fost afisat chat-ul

            // aici primim mesaje de la client pentru a le trimite mai departe catre destinatar
            while (1)
            {
                bzero(mesaj, 200);
                int len_mesaj = read(client_socket, mesaj, 200);
                if (len_mesaj == -1)
                {
                    perror(ANSI_COLOR_RED "START:CHAT : Eroare la citire mesaj!" ANSI_COLOR_RESET);
                    close(client_socket);
                    pthread_exit(NULL);
                }
                mesaj[len_mesaj] = '\0';

                if (strcmp(mesaj, "exit-chat") == 0)
                {
                    set_speaking_to_client_struct("\0", client_socket);
                    break;
                }

                // reply la mesaje
                if (strstr(mesaj, "reply:") != NULL)
                {
                    if (VALIDATE_REPLY(mesaj, USERNAME, destinatar) == 1)
                    {
                        UPDATE_CHAT_NEW_REPLY(USERNAME, destinatar, mesaj);
                        send_reply_to_destinatar(USERNAME, destinatar, mesaj); // reply
                    }
                    else
                    {
                        INVALIDATE(client_socket); // trimitem la client faptul ca reply-ul nu este bun
                    }
                }

                else
                {
                    // trimitem mesaje la client
                    if (VALIDATE_MSG(mesaj) == 1)
                    {

                        UPDATE_CHAT_NEW_MSG(USERNAME, destinatar, mesaj);
                        send_msg_to_destinatar(USERNAME, destinatar, mesaj); // msg
                    }
                    else
                    {
                        INVALIDATE(client_socket); // trimitem la client faptul ca mesajul nu este bun
                    }
                }
            }
        }

        if (strcmp(command, "exit-app") == 0) // aici inchidem clientul
        {
            LOGGED_IN = 0;
            delete_client_from_struct(client_socket, &i);
            close(client_socket);
            break;
        }
    }

    SHOW_OFFLINE(USERNAME); // afisam user-ul offline la final de thread
    pthread_mutex_unlock(&mut);
    pthread_exit(NULL);
}

int main()
{

    // Creare Socket al serverului
    if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror(ANSI_COLOR_RED "SERVER MAIN : EROARE LA CREARE SOCKET!\n" ANSI_COLOR_RESET);
        return errno;
    }

    CREATE_DATABASE(); // Aici se creeaza fisierele care tin loc de baza de date, serverul se inchide la eroare

    // Aici bifam optiunea ca serverul sa refoloseasca adresa la care face bind
    int on = 1;
    setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Pregatirea structurilor de date
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    // Umplem structura folosita pentru server
    {
        server.sin_family = AF_INET;                // Protocol IPV4
        server.sin_addr.s_addr = htonl(INADDR_ANY); // Primeste request de la orice IP
        server.sin_port = htons(PORT);
    }

    // Atasam socketul
    if (bind(socket_descriptor, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror(ANSI_COLOR_RED "SERVER MAIN : EROARE LA BIND!\n" ANSI_COLOR_RESET);
        return errno;
    }

    // Ascultam pentru clienti
    if (listen(socket_descriptor, 5) == -1)
    {
        perror(ANSI_COLOR_RED "SERVER MAIN : EROARE LA LISTEN!\n" ANSI_COLOR_RESET);
        return errno;
    }

    // Servim in mod concurent clientii
    while (1)
    {
        printf(ANSI_COLOR_GREEN "Listening to PORT: %d for clients...\n" ANSI_COLOR_RESET, PORT);
        fflush(stdout);

        // Acceptam cate un client la un moment dat, stare blocanta!
        int client_socket; // socket-ul clientului care se conecteaza
        int from_length = sizeof(from);

        if ((client_socket = accept(socket_descriptor, (struct sockaddr *)&from, &from_length)) < 0)
        {
            perror(ANSI_COLOR_RED "SERVER MAIN : EROARE LA ACCEPTARE CLIENT!\n" ANSI_COLOR_RESET);
            return errno;
        }

        // Creare mutex pentru a nu aparea accesarea memoriei de mai multe thread-uri in acelasi timp
        if (pthread_mutex_init(&mut, NULL) != 0)
        {
            perror(ANSI_COLOR_RED "SERVER MAIN : EROARE LA INIT_MUTEX!\n" ANSI_COLOR_RESET);
            close(client_socket);
            return errno;
        }

        int cl = client_socket;
        clients[i].socket_descriptor = client_socket; // setam socket-ul fiecarui client care se conecteaza in struct clienti
        clients[i].online = 0;                        // initial fiecare client este offline pana se logheaza cu succes la cont
        strcpy(clients[i].username, "\0");            // initial niciun client nu are nume pana cand nu se conecteaza
        strcpy(clients[i].speaking_to, "\0");         // intial niciun client nu are partener de chat

        pthread_create(&threads[i], NULL, &thread_func, (void *)&cl); // aici se creeaza thread-ul
        i = i + 1;                                                    // incrementam nr de clienti / thread-uri
    }

    // aici facem ca main thread-ul din server sa astepte ca toate celelalte thread-uri create sa se termine
    for (int j = 0; j < i; j++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mut); // inchidem mutex-ul
    close(socket_descriptor);    // inchidem socket-ul serverului
    return 0;
}