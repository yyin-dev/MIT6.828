# Lec 1

Analysis on `sh.c` can be closely related to a later homework.

Important points:

- I/O redirections are implemented by setting file descriptors between `fork` and `execvp`.
- Pipelines are implemented using the pipe abstraction provided by the kernel
- What happens when executing `ls | wc -l`.