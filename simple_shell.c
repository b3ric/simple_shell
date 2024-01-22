#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Prototypes */
void clean_mem(char *input, char **args);
int builtin_proc_err();
void print_to_stdout(char *path);
int builtin_exit(char *input, char **args);
int builtin_proc(char *input, char **args);
int is_empty(const char *s);

/* Builtin functions <exit> and <proc> */
int (*builtin_func[])(char *, char **) = {
    &builtin_exit,
    &builtin_proc,
};

/* Built-in proc was called incorrectly */

int builtin_proc_err() {
  perror("simple_shell: proc command must be like in the following format:\n\n\
			proc <folder>/<file>\
			\n\nPlease try again...");
  return 1;
}

/* Prints file specified in path to stdout */

void print_to_stdout(char *path) {
  int c;
  FILE *file;
  file = fopen(path, "r");
  if (file) {
    while ((c = getc(file)) != EOF)
      putchar(c);
    fclose(file);
  } else {
    fprintf(stderr, "simple_shell: File not found.\n");
  }
  printf("\n");
}

/*
	Checks if input is blank
*/
int is_empty(const char *input) {
  while (*input != '\0') {
    if (!isspace((unsigned char)*input))
      return 0;
    input++;
  }
  return 1;
}

/*
    Free memory
*/
void clean_mem(char *input, char **args) {
  free(input);
  free(args);
}

/* Builtin exit function */

int builtin_exit(char *input, char **args) {
  int args_len = -1;
  // count args
  while (args[++args_len] != NULL) { /* do nothing */ }

  // Check if more than 1 arg for <exit>
  // eg: "exit 9 hello"
  if (args_len > 2) {
    fprintf(stderr, "simple_shell: Too many arguments for <exit> command!\n");
    return 1;
  }

  if (args_len == 2) {
    char *end = args[1];

    // Try to parse int
    long int status = strtol(args[1], &end, 10);

    // Parsing failed, return 1 to keep shell running
    if (*end) {
      return 1;
    } else { // Parsing succeeded
      clean_mem(input, args);
      exit(status);
    }
  }

  return 0;
}

/*
    Built-in proc command
    Checks for arg count, then checks whether pid is involved or not
    eg: 'proc 1/status' and 'proc cpuinfo' will be treated differently
*/
#define PATH_SIZE 64
#define PROC_DELIM "/"
#define BUFSIZE 64
#define REQ_FILES_PROC 4

int builtin_proc(char *input, char **args) {

  int bufsize = BUFSIZE, index = 0, args_len = -1, i;
  bool in_pid = false;
  char *required_files[] = {"cpuinfo", "loadavg", "filesystems", "mounts"};

  while (args[++args_len] != NULL) { /* do nothing */ }

  // Command not entered correctly
  if (args_len != 2) {
    return builtin_proc_err();
  }

  // If a "/" is in the argument
  // The file likely lives in one of the pid folders
  // else, it's likely a file directly under proc (eg: cpuinfo)
  for (i = 0; i < (int) strlen(args[1]); i++) {
    if (args[1][i] == '/') {
      in_pid = true;
    }
  }

  if (in_pid) {
    char **tokens = malloc(bufsize * sizeof(char *));

    // Split using '/' as delim
    char *path = strtok(args[1], "/");

    while (path != NULL) {
      tokens[index] = path;
      index++;
      path = strtok(NULL, "/");
    }
    tokens[index] = NULL;

    // eg: proc 1/status -> 1 is our pid
    char *pid = tokens[0];

    // Concat the command to generate a valid file path
    char file_path[PATH_SIZE];
    strcpy(file_path, "/proc/");
    strcat(file_path, pid);
    strcat(file_path, "/");
    strcat(file_path, tokens[1]);

    print_to_stdout(file_path);
    free(tokens);
  } else { // a file under /proc/ was called (eg: cpuinfo)

    for (int i = 0; i < REQ_FILES_PROC; i++) {
      // check if file sent is one of the required files
      if (strcmp(args[1], required_files[i]) == 0) {
        char file_path[PATH_SIZE];
        // prepare file path
        strcpy(file_path, "/proc/");
        strcat(file_path, args[1]);
        print_to_stdout(file_path);
        return 1;
      } 
    }
	fprintf(stderr, "simple_shell: Files supported by proc are <cpuinfo> "
                        "<loadavg> <filesystems> <mounts>\n");
    return 1;
  }
  return 1;
}

/*
    Forks and tries to execvp as per passed args
    Will return 1 so shell continues to execute (see status in main)
*/
int execute(char **args) {

  pid_t pid;
  int status;

  pid = fork();

  // Child returns 0
  if (pid == 0) {
    if (execvp(args[0], args) == -1) {
      // No file or directory error
      perror("simple_shell");
    }
    exit(1);
  } else if (pid < 0) {
    // Fork error
    perror("simple_shell");
  } else {
    // Parent process
    do {
      // Wait for child to die
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}

/*
    Either calls one of the built-in functions
    If not built-in, sends args to execute function
    Which will call execvp
*/
#define BUILTIN_FUNC_COUNT 2
int load(char *input, char **args) {

  // Arr of builtin funcs
  char *builtin_arr[] = {"exit", "proc"};

  // Check if 1st arg is part of builtin
  for (int i = 0; i < BUILTIN_FUNC_COUNT; i++) {
    if (strcmp(args[0], builtin_arr[i]) == 0) {
      // Send args to respective builtin func
      return (*builtin_func[i])(input, args);
    }
  }
  // Not builtin, call execute
  return execute(args);
}

/*
    Read line from stdin
*/
char *read_line(void) {
  char *line = NULL;
  size_t len = 0;
  ssize_t buf = 0;
  buf = getline(&line, &len, stdin);
  return line;
}

/*
   Split line into separate arguments.
   Array ends with NULL
   'ls -la' yields {'ls','-la',NULL}
*/
#define BUFSIZE 64
#define ARG_DELIM " \t\r\n\a"
char **split_line(char *line) {
	
  int bufsize = BUFSIZE, index = 0, args_len = -1;
  ;
  char **args = malloc(bufsize * sizeof(char *));
  // args_back in case of reallocation error.
  // unescape_bkp to free mem returned from foreign unescape.
  char *arg, **args_bkp, *unescape_bkp;
    
  arg = strtok(line, ARG_DELIM);
  while (arg != NULL) {
    args[index] = arg;
    index++;
    // Need more memory?
    if (index >= bufsize) {
      bufsize += BUFSIZE;
      args_bkp = args;
      args = realloc(args, bufsize * sizeof(char *));
      if (!args) {
        free(args_bkp);
        fprintf(stderr, "simple_shell: reallocation error\n");
        exit(1);
      }
    }
    arg = strtok(NULL, ARG_DELIM);
  }
  args[index] = NULL;

  while (args[++args_len] != NULL) { /* do nothing */ }

  // If 'echo' arguments need to be unescaped.
  if (strcmp(args[0], "echo") == 0) {
    for (int i = 1; i < args_len; i++) {
      unescape_bkp = unescape(args[i], stderr);
      if (unescape_bkp) {
        strcpy(args[i], unescape_bkp);
        free(unescape_bkp);
      }
    }
  }
  return args;
}

/*
    Loop to read arguments and execute them
*/
void shell_loop(void) {
  char *input;
  char **args;
  int status;

  do {
    printf("$ ");
    input = read_line();
	
	// Check if cmd was empty.
	if (is_empty(input)){
		free(input);
		exit(1);
	}
	
	args = split_line(input);
    status = load(input, args);
	
    clean_mem(input, args);
  } while (status);
}

/*
    Main function, calls shell_loop to read args and execute
    If any args are passed to ./simple_shell return 1 and print to stderr
*/
int main(int argc, char **argv) {

  if (argc > 1) {
    fprintf(stderr,
            "simple_shell: Simple Shell takes not arguments!\nExiting...\n");
    return 1;
  }

  shell_loop();

  return 0;
}