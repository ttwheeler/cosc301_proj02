/* Tom Wheeler and Zach Schneiwiess
 * Zach modified tokenify code to work on cleancommand, freecommands, and
 * parsecommands, Tom wrote the execute and (faulty) mode methods.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>

void freecommands(char **commands)
{
    if(commands==NULL)return;
    int i = 0;
    while (commands[i] != NULL) {
        free(commands[i]); // free each command
        i++;
    }
    free(commands); // then free the array
}

/* Puts commands in proper form for execv to run them. Takes an array of strings
 * returns a heap allocated array of arrays of strings (for use with execv), and
 * frees the original array. Returns NULL if no command is present.
*/
char ** cleancommand(char *rawcommand)
{
    if(rawcommand==NULL) return NULL;//NULL
    int j=0;
    while(j<strlen(rawcommand)&&isspace(rawcommand[j]))j++;
    if(j==strlen(rawcommand)) return NULL;//All white space
    char copy[strlen(rawcommand)];
    strcpy(copy,rawcommand);//Make a copy of string
    char *test=strtok(copy," \t");//Test=first token
    int count=0;
    while(test!=NULL)//Loop counts the number of tokens
    {
        count++;
        if(test[0]=='\"')//If quotation marks exist, skip to end quotes
        {
            test=strtok(NULL,"\"");
        }
        else test=strtok(NULL," \t");
    }
    strcpy(copy,rawcommand);//Replace copy after strtok butchered it
    test=strtok(copy," \t");
    char **cleaned=malloc((count+1)*sizeof(char*));//Allocate space for array
    for(count=0;test!=NULL;count++)
    {
        if(test[0]=='\"')//Handles argument in quotation marks
        {
            if(test[strlen(test)-1]=='\"')
            {
                char clean[strlen(test)];
                strcpy(clean,&test[1]);//Cut off leading "
                clean[(strlen(clean)-1)]='\0';//Cut off trailing "
                cleaned[count]=strdup(clean);
            }
            else
            {
                char *testTwo=strtok(NULL,"\"");
                if(testTwo==NULL)//No end quote
                {
                    printf("Improperly formatted string argument");
                    fflush(stdout);
                    if(cleaned!=NULL)freecommands(cleaned);
                    return NULL;
                }
                char full[(strlen(test)+strlen(testTwo))];
                strcpy(full,&(test[1]));//Cut off leading "
                strcat(full," ");//Add back missing space (skipped by strtok)
                strcat(full,testTwo);//Concatenate the two strings
                cleaned[count]=strdup(full);
            }
        }
        else
        {
            cleaned[count]=strdup(test);//If not an arg in quotations, no issue
        }
        test=strtok(NULL, " \t");
    }
    cleaned[count]=NULL;
    return cleaned;
}

/* Parses the commands out of user input. Takes one line of user input, returns
 * a heap allocated array of strings as delimited by semi-colons, without
 * any comments. Adds a space on to the end of each string to help cleancommands
*/
char ** parsecommands(char *input)
{
    for(int i=0;i<strlen(input);i++)//Eliminate any comments
    {
        if(input[i]=='#')
        {
            input [i]='\0';
            break;
        }
    }
    char copy[strlen(input)];
    strcpy(copy,input);//Make a copy of comment-free input for use with strtok
    char *test=strtok(copy,";\n");//Test=first token
    int count=0;
    while(test!=NULL)//Loop counts the number of tokens
    {
        count++;
        test=strtok(NULL,";\n");
    }
    strcpy(copy,input);//Replace copy after strtok butchered it
    test=strtok(copy,";\n");
    char **rawcommands=malloc((count+1)*sizeof(char*));//Heap allocate array
    for(count=0;test!=NULL;count++)
    {
        rawcommands[count]=strdup(test);
        test=strtok(NULL, ";\n");
    }
    rawcommands[count]=NULL;
    return rawcommands;
}

void modehandle(char **command,int *mode)
{
    if(!strcmp(command[1],"sequential")||(!strcmp(command[1],"s")&&strlen(command[1])==1))
    {
        printf("Mode is sequential");
        *mode=0;
        return;
    }
    else if(!strcmp(command[1],"parallel")||(!strcmp(command[1],"p")&&strlen(command[1])==1))
    {
        *mode=1;
        printf("Mode is parallel");
        return;
    }
    printf("Invalid mode. Please input parallel or sequential (p or s)\n");
    fflush(stdout);
    return;
}

void executeS(char **rawcommands,int *mode)
{
    int i=0;
    int pid;
    char **command;
    if(rawcommands==NULL)return;
    while(rawcommands[i]!=NULL)
    {
        command=cleancommand(rawcommands[i]);
        while(command==NULL&&rawcommands[i]!=NULL)//Check for null commands
        {
            free(command);
            i++;
            command=cleancommand(rawcommands[i]);
        }
        if(rawcommands[i]==NULL)//If all commands were null, break loop
        {
            freecommands(command);
            break;
        }
        if(!strcmp(command[0],"exit"))
        {
            if(rawcommands[i+1]==NULL)
            {
                freecommands(command);
                *mode=-1;//Indicates exit
                return;
            }
            int z=i;
            free(rawcommands[z]);
            while(rawcommands[z]!=NULL)
            {
                rawcommands[z]=rawcommands[z+1];
                z++;
            }
            free(rawcommands[z-1]);
            freecommands(command);
            rawcommands[z-1]=strdup(" exit");
            command=cleancommand(rawcommands[i]);
        }
        if(!strcmp(command[0],"mode"))
        {
            modehandle(command,mode);
            freecommands(command);
            if(rawcommands[i+1]==NULL)return;
            else
            {
                i++;
                command=cleancommand(rawcommands[i]);
            }
        }
        pid=fork();
        if(pid==0)
        {
            if(execv(command[0],command)==-1)
            {
                printf("\nError executing %s.\n",command[0]);
                fflush(stdout);
                freecommands(command);//Free everything before exiting
                free(mode);
                exit(1);
            }
        }
        else if(pid!=0)
        {
            wait(&pid);
        }
        freecommands(command);
        i++;
    }
    return;
}

void executeP(char **command)
{
    if(execv(command[0],command)==-1)
    {
        printf("\nError executing %s.\n",command[0]);
        fflush(stdout);
        freecommands(command);
        exit(1);
    }
    fflush(stdout);
    return;
}

int main(int argc, char **argv)
{
    int *mode=malloc(sizeof(int));
    *mode=0;//Default is sequetial, represented by 0
    printf("\nprompt>");
    fflush(stdout);
    char **testArray;
    char buffer[1024];
    while (fgets(buffer, 1024, stdin) != NULL)
    {
            testArray=parsecommands(buffer);
            if(*mode==0)
            {
                executeS(testArray,mode);
                freecommands(testArray);
                if(*mode==-1)break;
            }
            else if(*mode==1)
            {
                char **com;
                int num=0;
                while(testArray[num]!=NULL)num++;
                int pids[num];
                for(int z=0;testArray[z]!=NULL;z++)
                {
                    com=cleancommand(testArray[z]);
                    if(com!=NULL)
                    {
                        if(!strcmp(com[0],"mode"))
                        {
                            modehandle(com,mode);
                            freecommands(com);
                        }
                        else if(!strcmp(com[0],"exit"))
                        {
                            *mode=-1;
                            freecommands(com);
                        }
                        else
                        {
                            pids[z]=fork();
                            if(pids[z]==0)executeP(com);
                            freecommands(com);
                        }
                    }
                }
                for(int j=0;j<num;j++)
                {
                    
                    wait(&pids[j]);
                }
                freecommands(testArray);
                if(*mode==-1)
                {
                    free(mode);
                    return 0;
                }
            }
            fflush(stdout);
            printf("\nprompt>");
            fflush(stdout);
    }
    free(mode);
    return 0;
}
