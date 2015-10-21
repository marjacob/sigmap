# sigconv
A simple Unix signals converter.

## Usage
The `-m` (or `--map=`) option is used to specify the source and destination signal, while the program to be executed comes last with its argument list. Because `execve` is used to execute the program, `<program>` must be a fully qualified or relative path.
```c
sigconv -m <from-signal>:<to-signal> <program>
```

## Example
Receive `SIGWINCH` and forward it to the child process `/bin/sleep` as `SIGINT`.
```c
sigconv -m 28:2 /bin/sleep 30
```
