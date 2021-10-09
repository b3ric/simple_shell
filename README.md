# Simple Shell

Simple Shell is a C program that works as a simple nix shell. 

## Usage
```bash
make simple_shell
./simple_shell
$ ...
```
## Approaches
### Built-in exit command

This simple while loop helps count how many elements in array. Which allow for quick detection of weather an exit command is valid or not.

```c
while (args[++args_len] != NULL) { /* do nothing */ }
```

If only "exit" is entered, returns normally with signal 0.

if "exit" is followed by an argument, try to parse argument using ```strtol``` function. If parsing succeeds, use that integer as return signal. If parsing fails (eg: exit hello), prompt user again for another command (not necessarily an exit command). 

### Built-in proc command

Check first if proc is being followed by exactly one argument. If not, print message to ```stderr``` and prompt user again. Acceptable inputs are of the following nature: 

```bash
proc 1/status
proc cpuinfo
```

If proc is followed by exactly one argument, check if that argument contains the '/'. This indicates that the file we're looking for is likely inside a pid folder. 

If we're dealing with a file inside a pid folder (eg: proc 1/status). We first split the argument using '/' as a delimiter, then concatenate all strings to generate a valid file path. That is, from ```proc 1/status```, we build ```'/proc/1/status'```. This new string (file path) is then sent to ```print_to_stdout()``` function.

Files that do not live in a pid folder will be treated similarly, except we first check if the argument followed by proc is part of: 

```c
char *required_files[] = {"cpuinfo", "loadavg", "filesystems", "mounts"};
```
And then we concatenate as such : ```/proc/loadavg```

### Execute function

Starts out by calling fork(). If 0 is returned, we know it's the child, so we can run execvp as such: 
```c
execvp(args[0], args) 
```
If that returns -1, an error occurred and we let the user know (eg: No file or directory). We then exit with signal 1 to keep the shell running. 

If ```fork()``` returns negative, an error occurred and we let the user know. 

At the end of this block we call ```waitpid(pid, &status, WUNTRACED)``` to wait for child to die before returning to caller.

### Load function

Before calling ```execute(args)``` we must first check whether the user's input is actually a built-in command (exit or proc). If the user is calling a built-in command, we return:

```c
(*builtin_func[i])(input, args);
```

which has been declared as such:
```c
int (*builtin_func[])(char *, char **) = {
    &builtin_exit,
    &builtin_proc,
};
```
If not a built-in command, then we may go ahead and return ```execute(args)```

### Reading a line
Using function 
```c
getline(&line, &len, stdin)
```
to read input from stdin.

### Splitting a line

This function is needed after the entire line has been read.

Inputs such as:
```bash
echo Hello World !
ls -la 
```
will be split into:
```c
{"echo", "Hello", "World", "!", NULL}
{"ls", "-la", NULL}
```
At this point the program does not "know" how big the input is. Therefore we malloc an array of strings with fixed size at first, call ```arg = strtok(line, ARG_DELIM)``` to split the line and, with a while loop, start placing these tokens in the array of strings. 

During the while loop we must check if we've gone over the initial malloc'd buffer size. If so, we duplicate it. This way we don't run out of memory or cause a seg fault if the user decides to, for example, echo a very large string. 

We use ```realloc``` for these cases, and check for reallocation errors by having a back up variable in place: 

```c
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
```

Once this is done, we must check if the command is ```echo```. If so, the string needs to be treated before we send it for execution. We do so by calling ```unescape(args[i], stderr)``` once we detect ```echo``` was the input. 

Since ```unescape(args[i], stderr)``` allocates memory when a non-NULL value is returned, we must also use a backup variable, and free it once we've used it. In this case we use the return in ```strcpy()```, as such: 

```c
unescape_bkp = unescape(args[i], stderr);
      if (unescape_bkp) {
        strcpy(args[i], unescape_bkp);
        free(unescape_bkp);
      }
```

### Shell Loop

To keep the shell always running, we implement a ```do-while``` loop, which runs as long as status is being returned as 1. This is also where we send arguments to be read and split into separate tokens. ```clean_mem(input, args)``` is called at the end of every iteration to ensure proper garbage collection. 


