#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sendfile.h>

#define STAT_SIZE (8192)

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    printf(
      "raceread: file permission race condition proof of concept\n\n"
      "Usage: raceread <filename>\n\n"
      "Attempts to open a file until it succeeds, then prints file stats and content\n"
      "on each stat change.\n");
    exit(2);
  }

  printf("Attempting to repeatedly open '%s' until successful...\n", argv[1]);

  FILE* fp = NULL;
  int errno;
  int last_errno = -1;

  while (fp == NULL) {
    fp = fopen(argv[1], "r");
    if (fp == NULL && errno != last_errno) {
      last_errno = errno;
      perror(NULL);
    }
  }


  printf("File is open!\n");

  struct stat curr_stat;
  struct stat last_stat;

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
      // This may be more performant and easier to write, but
      //   a) sendfile is not portable
      //   b) if you forget the sys/sendfile.h import, the code will build but
      //      will set O_APPEND on stdout, which will then make the correct
      //      version also fail on that terminal with EINVAL!
      // I've decided sendfile is not worth the limitations and pain.
      off_t input_offset = 0;
      size_t copied = sendfile(STDOUT_FILENO, fileno(fp), &input_offset, 0x7fffffff);
      if ((int)copied >= 0) {
        printf("-- end of content (%ld bytes) --\n", copied);
      } else {
        perror("sendfile");
      }
    }
  }
}
