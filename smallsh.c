#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE 2048
#define MAX_ARGS 512

int lastTerm = -1;
pid_t bgpidArray[50];
int arrayindex = 0;
bool isBg = false;
int sigcheck = 0;

void catchSIGINT(int);
void catchSIGTSTP(int);

void setSigHanglers()
{
  struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
  SIGINT_action.sa_handler = catchSIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = SA_RESTART;
  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = SA_RESTART;

  sigaction(SIGINT, &SIGINT_action, NULL);
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

void catchSIGINT(int signo)
{
  char * message = "Caught SIGINT\n";
  write(STDOUT_FILENO, message, 16);
}

void catchSIGTSTP(int signo)
{
  char * message = "Caught SIGTSTP\n";
  char * message2 = "Entering foreground only mode. (& is ignored)\n";
  char * message3 = "Exiting foreground only mode.\n";
  write(STDOUT_FILENO, message, 17);
  if (sigcheck == 0) {
    sigcheck = 1;
    write(1, message2, 46);
  }
  else {
    sigcheck = 0;
    write(1, message3, 30);
  }
}

//gets input from user and checks if comment or empty
char * getInput()
{
  int childExitMethod, i;
  char *line = NULL;
  size_t buffsize = 0;
  pid_t childPID;
  do {
      //printf("idx: %d\n", arrayindex);
      for (i = 0; i < arrayindex; i++) {
        //printf("checking %d\n", bgpidArray[i]);
        childPID = waitpid(bgpidArray[i], &childExitMethod, WNOHANG);
        //printf("BGchildExit: %d\n", childExitMethod);
        //printf("childPID: %d\n", childPID);
      }
    printf("  : ");
    getline(&line, &buffsize, stdin);
    if (buffsize <= 0) {
      printf("here\n");
      clearerr(stdin);
      memset(line, '\0', 2049);
    }
    line[strcspn(line, "\n")] = '\0'; //replace \n with \0
  } while (line[0] == ' ' || line[0] == '#' || line[0] == '\0');

  return line;
}
//returns array of strings from command line
char **parseCommands(char *line)
{
  char **commands = malloc(MAX_ARGS * sizeof(char*));
  char *cmd;
  char *addpid;
  char *temp;
  char *dollar = "$$";
  int idx;

  cmd = strtok(line, " ");
  while(cmd != NULL)
  {
    if (strstr(cmd, dollar) != NULL) {
      addpid = strtok(cmd, dollar);
      //printf("addpid: %s\n", addpid);
      temp = strtok(NULL, dollar);
      //printf("temp: %s\n", temp);
      cmd = malloc(500);
        sprintf(cmd, "%s%d%s", addpid, getpid(), temp);
      //strcat(cmd, temp);
      printf("cmd: %s\n", cmd);
    }
    commands[idx] = cmd;
    idx++;
    cmd = strtok(NULL, " ");
  }
  if (strcmp(commands[idx - 1], "&") == 0 && !sigcheck) {
    isBg = true;
    commands[idx - 1] = NULL; //remove unwanted &
  }
  else {
    isBg = false;
    if (sigcheck) {
      commands[idx - 1] = NULL; //remove unwanted &
    }
    commands[idx] = NULL; //show where command line ends
  }


  return commands;
}

//spawns new child process and waits on child to exit
int processFork(char **args)
{
  pid_t spawnPID = -5, childPID;
  int childExitMethod = -5, childReturn;
  int fileDescriptor1, fileDescriptor2;
  int check = 0;

  spawnPID = fork();
  if (spawnPID == -1)
  {
    perror("HULL BREACH");
    return 0;
  }
  else if (spawnPID == 0)
  {
    //printf("\nIn Child.\nPID: %d\n", getpid());
    //Handle "<" Redirection
    if (redirIn(args))
    {
      fileDescriptor1 = open(args[2], O_RDONLY);
      if (fileDescriptor1 == -1) { perror("open()"); exit(1); }
      //printf("fileDescriptor1 == %d\n", fileDescriptor1);
      int result = dup2(fileDescriptor1, STDIN_FILENO);
      if (result == -1) { perror("dup2"); exit(2); }
      check = 1;
    }
    //Handle only ">" Redirection
    if (redirOut(args) == 1)
    {
      fileDescriptor2 = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fileDescriptor2 == -1) { perror("open()"); exit(1); }
      //printf("fileDescriptor2 == %d\n", fileDescriptor2);
      //printf("args[2]: %s\n", args[2]);
      int result = dup2(fileDescriptor2, STDOUT_FILENO);
      if (result == -1) { perror("dup2"); exit(2); }
      check = 1;
    }
    //Handle both
    if (redirOut(args) == 2)
    {
      fileDescriptor2 = open(args[4], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fileDescriptor2 == -1) { perror("open()"); exit(1); }
      //printf("fileDescriptor2 == %d\n", fileDescriptor2);
      int result = dup2(fileDescriptor2, STDOUT_FILENO);
      if (result == -1) { perror("dup2"); exit(2); }
      check = 1;
    }
    //
    if (isBg) {
      fileDescriptor1 = open("/dev/null", O_WRONLY);
      if (fileDescriptor1 == -1) { perror("open()"); exit(1); }
      //printf("fileDescriptor1 == %d\n", fileDescriptor1);
      int result = dup2(fileDescriptor1, STDIN_FILENO);
      if (result == -1) { perror("dup2"); exit(2); }
      int result2 = dup2(fileDescriptor1, STDOUT_FILENO);
      if (result2 == -1) { perror("dup2"); exit(2); }
    }
    //if & add the childs PID to the array
    //execute any other process
    if (check == 0) { //if no redirection
      if (execvp(args[0], args) == -1) {
      perror("Execute Error");
      exit(1);
      }
    }
    else {
      if (execlp(args[0], args[0], NULL) == -1) {
      perror("Execute Error");
      exit(1);
      }
    }
  }
  //If process is the Parent process...
  //printf("\nIn Parent. \nPID: %d\nWaiting...\n", getpid());
  if (isBg) {
    printf("isbg\n");
    bgpidArray[arrayindex] = spawnPID;
    printf("bgpid: %d\n", bgpidArray[arrayindex]);
    arrayindex++;
    return 1;
    //childPID = waitpid(spawnPID, &childExitMethod, WNOHANG);
  }
  childPID = waitpid(spawnPID, &childExitMethod, 0);
  childExitMethod = WEXITSTATUS(childExitMethod);
  lastTerm = childExitMethod;
  //printf("childExit: %d\n", lastTerm);
  //printf("In Parent. \nChild process %d terminated.\nParent Exiting...\n", childPID);
  return 1;
}

//returns 1 if "<" is in args array
int redirIn(char ** args)
{
  int idx = 0;
   while(args[idx] != NULL)
   {
     if (strcmp(args[idx], "<") == 0) {
       return 1;
     }
     idx++;
   }
   return 0;
}

//returns 0 if no stdout Redirection
//returns 1 if only stdout Redirection
//returns 2 if both stdout and stdin are Redirected
int redirOut(char ** args)
{
  int idx = 0;
  while (args[idx] != NULL)
  {
    if (strcmp(args[idx], ">") == 0)
    {
      if (idx == 1) { //only stdout redirection
        return 1;
      }
      else if (idx == 3) {
        return 2;
      }
    }
    idx++;
  }
  return 0;
}

//check parsed args for a Built In Command
bool isBuiltIn(char **args)
{
  char *bltinArray[] = {"exit", "cd", "status"};
  int i;
  for (i = 0; i < 3; i++)
  {
    if (strcmp(args[0], bltinArray[i]) == 0) {
      return true;
    }
  }
  return false;
}

//executes Built In commands
int runBuiltIn(char **args)
{
  //char dirpath[100];
  //EXIT
  if (strcmp(args[0], "exit") == 0)
  {
    return 0;
  }
  //CD
  else if (strcmp(args[0], "cd") == 0)
  {
    if (args[1] == NULL)
    {
      chdir(getenv("HOME"));
      return 1;
    }
    else
    {
      chdir(args[1]);
      return 1;
    }
    return 0;
  }
  //STATUS
  else if (strcmp(args[0], "status") == 0)
  {
    printf("status: %d\n", lastTerm);
    return 1;
  }
  //ERROR
  else
  {
    perror("BUILT_IN ERROR");
    return 0;
  }
}

int main(int argc, char *argv[])
{
  setSigHanglers();
  char **cmdArray;
  char *cmdline;
  int smallshell;
  do {
  //get command line as one string
    cmdline = getInput();
  //parse the cmdline
    cmdArray = parseCommands(cmdline);
  //check for Built in Commands
    if (isBuiltIn(cmdArray)) {
      smallshell = runBuiltIn(cmdArray);
    }
  //run process
    else {
      smallshell = processFork(cmdArray);
    }
  }while(smallshell != 0);

free(cmdline);
free(cmdArray);

return 0;
}
