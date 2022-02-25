#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_READ (1024*1024)

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    printf(
      "raceread: file permission race condition proof of concept\n\n"
      "Usage: raceread <filename>\n\n"
      "Attempts to open a file until it succeeds, then prints file stats and content\n"
      "on each stat change. A copy of the most recent content is written to\n"
      "raceread.out in the current directory.\n");
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
  char buf[MAX_READ];

  while (1) {
    // fetch and print stat and content each time it changes
    if (fstat(fileno(fp), &curr_stat) != 0) {
      // Typically does not happen.
      // Deleting the file does NOT affect the open file handle or its stat!
      perror("fstat");
      exit(1);
    }

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

      if (copied > 0) {
        // Write copy
        umask(0077);  // Let's not make the same mistake ;)
        FILE* outfile = fopen("raceread.out", "wb");
        if (outfile != NULL) {
          if (fwrite(buf, copied, 1, outfile) == 1) {
            printf("-- wrote copy into raceread.out --\n");
          } else {
            printf("-- fwrite() into raceread.out failed --\n");
          }
          fclose(outfile);
        } else {
          perror("fopen(\"raceread.out\")");
        }
      }
    }
  }
}
