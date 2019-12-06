#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <values.h>
#include <dirent.h>
#include <ctype.h>
#include <wait.h>
#include <time.h>
#include <sys/socket.h>

bool found;

void HelpOutput (char *txtName, char *output)
{
    int bytesRead,txt_fd;
    char c;
    if (-1 == (txt_fd = open(txtName, O_RDONLY, 0600)))
    {
        fprintf(stderr, "[spawn_child] Error at opening \"%s\" file.\n", txtName);
        exit(3);
    }

    while(1)
    {
        bytesRead = read(txt_fd, &c, sizeof(char));
        if (bytesRead < 0) {
            fprintf(stderr, "Error at reading from \"%s\".\n", txtName);
            exit(5);
        }
        if (bytesRead == 0)
            break;
        sprintf(output, "%s%c", output, c);
    }
}
void PopulateArg(char *source, char *arg[])
{
    char *pointer;
    int argCount = 0;
    pointer = strtok(source, " ");

    while(pointer != NULL)
    {
        arg[argCount++] = pointer;
        pointer = strtok(NULL, " ");
    }

    arg[argCount] = NULL;
}
char* toLower (char* string)
{
    int k=0;
    while (string[k] != '\0')
    {
        if(string[k] >= 'A' && string[k] <= 'Z')
            string[k] += 32;
        k++;
    }
    return string;
}
void GetUserLoginData(char *user, char *pass)
{
    printf("Enter username: ");
    scanf("%s", user);
    getchar();
    printf("Enter password: ");
    scanf("%s", pass);
    getchar();



}
void GetFileLoginData(char *realUser, char *realPass)
{
    int login_fd, readBytes, len = 0;
    char c;
    if ( -1 == (login_fd = open ("login.txt", O_RDONLY)))
    {
        perror("Error at opening login.txt\n");
        exit (1);
    }

    while(1) {
        readBytes = read(login_fd, &c, sizeof(char));
        if (readBytes == 0 || c == '\n')
            break;
        realUser[len++] = c;
    }
    realUser[len] = '\0';
    len = 0;
    while(1)
    {
        readBytes = read(login_fd, &c, sizeof(char));
        if (readBytes == 0 || c == '\n')
            break;
        realPass[len++] = c;
    }
    realPass[len] = '\0';
    close(login_fd);

}
bool VerifyLoginData (char *user, char *pass, char *realUser, char *realPass)
{
    char answer;
    if (strcmp(user, realUser) != 0 || strcmp(pass, realPass) != 0)
    {
        printf("Username or password incorrect. Try again? [y/n]\n");
        scanf("%s", &answer);

        while(answer != 'y' && answer != 'n')
        {
            printf("[y/n]\n");
            scanf("%s", &answer);
        }
        if(answer == 'n')
            exit(1);
        return false;
    }
    return true;

}
void MyStat(char *path, char *output){


    struct stat st;
    char date[30];
    if (stat(path, &st) != 0)
    {
        perror("Error at stat function");
        return;
    }

    strcpy(output, "\t");

    sprintf(output, "%sFile: %s\n\t\t",output, path);
    sprintf(output, "%sSize: %ld\t\t", output, st.st_size);
    sprintf(output, "%sBlocks: %ld\t\t",output, st.st_blocks);
    sprintf(output, "%sIO Blocks: %ld\t\t",output, st.st_blksize);
    sprintf(output, "%sInode: %ld\n\t\t",output, st.st_ino);

    strftime(date, 20, "%Y-%m-%d %H:%M:%S", localtime(&(st.st_ctime)));         // FUNCTIE ADAPTATA DUPA O LINIE DE COD GASITA PE STACK OVERFLOW
    sprintf(output, "%sModify: %s\n",output, date);


}
void RecursiveFind(char *path, char *toFind, char *findOutput) {        // FUNCTIE PRELUATA SI ADAPTATA DE PE SITE-UL DE SISTEME DE OPERARE 2019
    DIR *dir;
    struct dirent *file;
    struct stat st;
    char path_name[PATH_MAX], myStatOutput[1000];

    if (stat(path, &st) != 0)
    {
        fprintf(stderr, "Error at stat function on file: %s\n", path);
        return;
    }
    if (!S_ISDIR(st.st_mode))                                                //if not a directory, return and continue with next file
    {
        return;
    }
    if ( NULL == (dir = opendir(path)))
    {
        fprintf(stderr, "Error at opening dir: %s", path);
        return;
    }
    while (NULL != (file = readdir(dir)))                                    // going through every file of dir
    {
        if (strcmp(file->d_name, ".") && strcmp(file->d_name, ".."))
        {
            sprintf(path_name, "%s/%s", path, file->d_name);
            if (strstr(toLower(file->d_name), toLower(toFind)) != NULL) {
                MyStat(path_name, myStatOutput);                             // getting requested information about found file using MyStat
                sprintf(findOutput, "%s\n%s", findOutput, myStatOutput);     // creating output string to be written to socketpair
                found = true;
            }
            RecursiveFind(path_name, toFind, findOutput);
        }

    }

    closedir(dir);



}

int main(int argc, char **argv) {
    char userName[20], password[20], realUserName[20], realPassword[20],command[50], c;
    int fifoW, fifoR, commandLength, sockp[2], bytesRead, outputLength;
    pid_t pid_main_child;

    // ----- LOGGING IN ----- //    user: Sudo
                              //    pass: IShallPass

    GetUserLoginData(userName, password);
    GetFileLoginData(realUserName,realPassword);
    while(1)
    {
        if (VerifyLoginData(userName,password,realUserName,realPassword))
            break;
        GetUserLoginData(userName, password);

    }


    // ----- LOGGED IN ----- //

    if(-1 == mkfifo("my_fifo", 0600) ) {
        if (errno == EEXIST) {
            printf("Using already existent fifo: \"my_fifo\" ...\n");
        } else {
            perror("Error at creating \"my_fifo\" file.\n");
            exit(1);
        }

    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)
    {
        perror("Error at creating socketpair\n");
        exit(1);
    }
    if (-1 == (pid_main_child = fork())) {
        perror("Error at creating main child.\n");
        exit(3);
    }

    if(pid_main_child) // parent
    {
        close(sockp[1]);

        while(1)
        {

            printf("MyConsole: ");

            fgets(command, 50, stdin);
            fflush(stdout);

            if (-1 == (fifoW = open("my_fifo", O_WRONLY)))
            {
                perror("Error at opening \"my_fifo\"\n");
                exit(4);
            }

            commandLength = strlen(command);
            commandLength--;                                        // without '/n'
            if (-1 == write(fifoW, &commandLength, sizeof(int)))
            {
                perror("Error at writing in \"my_fifo\" file.\n");
                exit(5);

            }

            if (-1 == write(fifoW, command, commandLength))
            {
                perror("Error at writing in \"my_fifo\" file.\n");
                exit(5);
            }

            close(fifoW);

            if (strncmp(command, "exit",4) == 0)
                break;

            if( -1 == read(sockp[0], &outputLength, sizeof(int)))
            {
                perror("Error at reading from socketpair.\n");
                exit(5);
            }

            while(outputLength > 0)
            {
                bytesRead = read(sockp[0], &c, sizeof(char));
                if (bytesRead < 0) {
                    perror("Error at reading from sockerpair.\n");
                    exit(5);
                }
                printf("%c", c);

                outputLength--;

            }
            fflush(stdout);


        }

    }
    else // [spawn_child]
    {
        close(sockp[0]);
        char infoFromFifo[200], expression[20], output[1000000];
        int infoLength, outputLength1;
        pid_t pid_child;

        while(1) {

            if (-1 == (fifoR = open("my_fifo", O_RDONLY))) {
                perror("[spawn_child] Error at opening\"my_fifo\"\n");
                exit(4);
            }

            if (-1 == read(fifoR, &infoLength, sizeof(int))) {
                perror("[spawn_child] Error at reading from \"my_fifo\"\n");
                exit(5);
            }
            if (-1 == read(fifoR, infoFromFifo, infoLength)) {
                perror("[spawn_child] Error at reading from \"my_fifo\"\n");
                exit(5);
            }
            infoFromFifo[infoLength] = '\0'; //eliminating '\n'

            close(fifoR);

            if(strncmp(infoFromFifo, "exit", 4) == 0)
            {
                break;
            }
            else if (strcmp(infoFromFifo, "find --help") == 0)
            {
                HelpOutput("find_help.txt", output);
            }
            else if(strncmp(infoFromFifo, "find ", 5) == 0) {

                strcpy(infoFromFifo, infoFromFifo + 5);      // eliminating 'find '
                printf("%s", infoFromFifo);
                int k = 0;
                if(strstr(infoFromFifo, " ") != NULL) {
                    while (infoFromFifo[k++] != ' ');            // finding ' ' position
                    strcpy(expression, infoFromFifo + k);        // everything after ' ' is copied in expression
                    infoFromFifo[k - 1] = '\0';                  // infoFromFifo becomes the path
                } else{
                    strcpy(expression, infoFromFifo);
                    strcpy(infoFromFifo, ".");

                }


                found = false;
                RecursiveFind(infoFromFifo, expression, output);
                if (!found)
                {
                    sprintf(output, "No file or directory found containing expression \"%s\"\n", expression);
                }



            }

            else if (strcmp(infoFromFifo, "stat --help") == 0)
            {
                HelpOutput("stat_help.txt", output);

            }
            else if(strncmp(infoFromFifo, "stat ", 5) == 0){

                strcpy(infoFromFifo, infoFromFifo + 5);      // eliminating 'stat '-> infoFromFifo becomes the path
                MyStat(infoFromFifo, output);

            }

            else if (strstr(infoFromFifo, "--help") != NULL){
                HelpOutput("exec_help.txt", output);
            }

            else{

                int sc_to_exec[2], exec_to_sc[2];

                if (pipe(sc_to_exec) == -1)
                {
                    perror("[spawn_child] Error at creating pipe.\n");
                    exit(2);
                }
                if (pipe(exec_to_sc) == -1)
                {
                    perror("[spawn_child] Error at creating pipe\n.");
                    exit(2);
                }

                if (-1 == ( pid_child = fork() )){
                    perror("[spawn_child] Error at second fork\n");
                    exit(3);
                }

                if (pid_child > 0) // [spawn_child]
                {


                    close(sc_to_exec[0]);
                    close(exec_to_sc[1]);
                    if (-1 == write(sc_to_exec[1], &infoLength, sizeof(int))) {
                        perror("[spawn_child] Error at writing in pipe.\n");
                    }
                    if (-1 == write(sc_to_exec[1], infoFromFifo, infoLength)) {
                        perror("[spawn_child] Error at writing in pipe.\n");
                    }

                    close(sc_to_exec[1]);
                    wait(NULL);


                    if (-1 == read(exec_to_sc[0], output, sizeof(output))){
                        perror("[spawn_child] Error at reading from pipe.\n");
                    }
                    close(exec_to_sc[0]);




                } else{ // [exec_child]

                    char infoFromMainChild[200];
                    int infoFromMainChildLength;
                    close(exec_to_sc[0]);
                    close(sc_to_exec[1]);
                    if (-1 == read(sc_to_exec[0], &infoFromMainChildLength, sizeof(int))){
                        perror("[exec_child] Error at reading from pipe.\n");
                    }
                    if (-1 == read(sc_to_exec[0], infoFromMainChild, infoFromMainChildLength)){
                        perror("[exec_child] Error at reading from pipe.\n");
                    }
                    close(sc_to_exec[0]);

                    char *arg[infoFromMainChildLength];

                    close(1);
                    if (dup2 (exec_to_sc[1],1) != 1)                               // stdout redirected to write in writing end of exec_to_sc  * CODUL SI FUNCTIONALITATEA FUNCTIEI DUP() PRELUATE DE PE PAGINA COMPUTER NETWORKS ( WHO_WC.C )
                    {
                        perror("[exec_child] Error at dup function\n");
                    }

                    close(exec_to_sc[1]);


                    PopulateArg(infoFromMainChild, arg);

                    execvp(arg[0], arg);
                    perror("[exec_child] Error at execvp function. ");
                }
            }

            fflush(stdout);
            outputLength1 = strlen(output);
            if (-1 == write(sockp[1], &outputLength1, sizeof(int))) {               // writing output to parent for printing
                perror("[spawn_child] Error at writing in socketpair.\n");
                exit(4);
            }
            if (-1 == write(sockp[1], output, outputLength1)) {
                perror("[spawn_child] Error at writing in socketpair.\n");
                exit(4);
            }
            fflush(stdout);
            output[0] = '\0';                                                   // resetting strings for next command
            infoFromFifo[0] = '\0';
            expression[0] = '\0';
        }
    }




    return 0;
}