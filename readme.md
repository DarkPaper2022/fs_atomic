This project is used to test whether the write, read, and lseek system calls are atomic between different processes operating on the same file descriptor.

In Process A:

- lseek and read are performed on reader_fd

- lseek is performed on writer_fd

In Process B:

- write is performed on writer_fd

Processes A and B share writer_fd through fork.
writer_fd and reader_fd refer to the same file, but they do not share f_pos.