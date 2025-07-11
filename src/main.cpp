#include <assert.h>
#include <fcntl.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <linux/memfd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

int cal() {
  auto cal_rec = [](auto self, int n) -> int {
    if (n <= 1)
      return n;
    return self(self, n - 1) * self(self, n - 1) * self(self, n - 1);
  };
  // random int in [10, 15)
  auto random_n = 5 + rand() % 10;
  return cal_rec(cal_rec, random_n);
}

bool data_available(int reader_fd, int writer_fd) {
  off_t read_pos = lseek(reader_fd, 0, SEEK_CUR);
  off_t writer_pos = lseek(writer_fd, 0, SEEK_CUR);
  if (read_pos == -1 || writer_pos == -1) {
    perror("lseek failed");
    exit(1);
  }

  assert(writer_pos >= read_pos && "should be in order");
  return read_pos != writer_pos;
}

bool process_finished(int pid) {
  static bool finished = false;
  if (finished) {
    return true; // Already checked and finished
  }
  int status;
  pid_t result = waitpid(pid, &status, WNOHANG);
  if (result == -1) {
    perror("waitpid failed");
    exit(1);
  }
  if (result == 0) {
    // Process is still running
    return false;
  } else {
    // Process has finished
    finished = WIFEXITED(status) || WIFSIGNALED(status);
    return finished;
  }
}

auto read_from_fake_file(int reader_fd, int writer_fd) {
  off_t read_pos = lseek(reader_fd, 0, SEEK_CUR);
  off_t writer_pos = lseek(writer_fd, 0, SEEK_CUR);
  if (read_pos == -1 || writer_pos == -1) {
    perror("lseek failed");
    exit(1);
  }

  assert(writer_pos >= read_pos && "should be in order");
  assert(read_pos != writer_pos && "read_pos == writer_pos");

  size_t delta = writer_pos - read_pos;
  char *read_buf = (char *)malloc(delta);
  ssize_t n = read(reader_fd, read_buf, delta);
  if (n < 0) {
    perror("read failed");
    exit(1);
  }
  assert(n == (ssize_t)delta && "read incomplete");

  if (n < 50) {
    printf("read bytes: [%" PRId64 ", %" PRId64 "): ", (int64_t)read_pos,
           (int64_t)(read_pos + n));
    fwrite(read_buf, 1, n, stdout);
    printf("\n");
  } else {
    printf("read bytes: [%" PRId64 ", %" PRId64 "): ", (int64_t)read_pos,
           (int64_t)(read_pos + n));
    fwrite(read_buf, 1, 25, stdout);
    printf("...");
    fwrite(read_buf + n - 25, 1, 25, stdout);
    printf("\n");
  }

  // Simulate "next_round_write_pos"
  off_t next_round_write_pos = lseek(writer_fd, -writer_pos, SEEK_CUR);
  if (next_round_write_pos == -1) {
    perror("writer_fd seek failed");
    exit(1);
  }

  char *extra_buf = NULL;
  ssize_t m = 0;
  if (next_round_write_pos != 0) {
    extra_buf = (char *)malloc(next_round_write_pos);
    m = read(reader_fd, extra_buf, next_round_write_pos);
    if (m < 0) {
      perror("read extra failed");
      exit(1);
    }

    if (m < 50) {
      printf("read extra bytes: [%" PRId64 ", %" PRId64 "): ",
             (int64_t)writer_pos, (int64_t)(writer_pos + m));
      fwrite(extra_buf, 1, m, stdout);
      printf("\n");
    } else {
      printf("read extra bytes: [%" PRId64 ", %" PRId64 "): ",
             (int64_t)writer_pos, (int64_t)(writer_pos + m));
      fwrite(extra_buf, 1, 25, stdout);
      printf("...");
      fwrite(extra_buf + m - 25, 1, 25, stdout);
      printf("\n");
    }
  }

  // Seek reader_fd back to next_round_write_pos
  if (lseek(reader_fd, next_round_write_pos, SEEK_SET) == -1) {
    perror("reset reader_fd failed");
    exit(1);
  }
  auto re = string(read_buf, n) + (extra_buf ? string(extra_buf, m) : "");
  // Clean up
  free(read_buf);
  if (extra_buf)
    free(extra_buf);
  return re;
}

auto check_and_read(int reader_fd, int writer_fd) {
  if (!data_available(reader_fd, writer_fd)) {
    // No data available, sleep for a while
    sleep(1);
    return string("");
  } else {
    // Data is available, read it
    return read_from_fake_file(reader_fd, writer_fd);
  }
}

int main() {
  // create mem file

  // int memfd = memfd_create("my_memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  int memfd = open("./file", O_RDWR | O_CREAT);
  if (memfd < 0) {
    perror("memfd_create failed");
    exit(1);
  }

  // create reader and writer fd
  auto fd_path = "/proc/self/fd/" + std::to_string(memfd);
  int reader_fd = open(fd_path.c_str(), O_RDONLY | O_CLOEXEC);
  if (reader_fd < 0) {
    perror("open reader_fd failed");
    exit(1);
  }
  int writer_fd = memfd;
  int pid = fork();
  if (pid < 0) {
    perror("fork failed");
    exit(1);
  } else if (pid == 0) {
    // Child process: simulate producer
    // create the new process, pass the writer fd, start it

    close(reader_fd); // Close reader in child
    for (uint64_t i = 0; i < 100000; i++) {
      cal();
      auto data = string("{c: ") + std::to_string(i) + "}\n";
      ssize_t written = write(writer_fd, data.c_str(), data.size());
    }
    exit(0); // Exit child process
  }

  // Parent process: simulate consumer
  auto re = string();
  while (!process_finished(pid) || data_available(reader_fd, writer_fd)) {
    auto data = check_and_read(reader_fd, writer_fd);
    if (!data.empty()) {
      re += data;
    }
  }
  {
    auto fs = fstream("log/collected_data.txt", ios::out);
    if (!fs.is_open()) {
      perror("Failed to open log file");
      exit(1);
    }
    fs << re;
  }
  {
    auto fs = fstream("log/standard_data.txt", ios::out);
    if (!fs.is_open()) {
      perror("Failed to open standard log file");
      exit(1);
    }
    for (uint64_t i = 0; i < 100000; i++) {
      auto data = string("{c: ") + std::to_string(i) + "}\n";
      fs << data;
    }
  }

  // in a loop, read data, checking the producer is finished and buffer is empty
}