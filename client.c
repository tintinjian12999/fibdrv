#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[256];
    char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    struct timespec t1, t2;
    FILE *fptr = fopen("output.txt", "w");
    for (int i = 0; i <= offset; i++) {
        printf("Writing to " FIB_DEV ", returned the sequence 1\n");
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        uint64_t sz = read(fd, buf, sizeof(buf));
        clock_gettime(CLOCK_MONOTONIC, &t2);
        long long krtime = write(fd, write_buf, strlen(write_buf));
        long long user_time =
            (t2.tv_sec - t1.tv_sec) * 1E9 + (t2.tv_nsec - t1.tv_nsec);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        fprintf(fptr, "%d %lld %lld %lld \n", i, krtime, user_time,
                user_time - krtime);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        uint64_t sz = read(fd, buf, sizeof(buf));
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }


    close(fd);
    return 0;
}
