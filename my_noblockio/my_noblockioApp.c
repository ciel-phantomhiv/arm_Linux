#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

int main(int argc, char *argv[])
{
    fd_set readfds;
    int key_val;
    int fd;
    int ret;

    /* 判断传参个数是否正确 */
    if(2 != argc) {
        printf("Usage:\n"
               "\t./keyApp /dev/key\n"
              );
        return -1;
    }

    /* 打开设备 */
    fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if(0 > fd) {
        printf("ERROR: %s file open failed!\n", argv[1]);
        return -1;
    }

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    /* 循环轮训读取按键数据 */
    for ( ; ; ) {

        ret = select(fd + 1, &readfds, NULL, NULL, NULL);
        switch (ret) {

        case 0:     // 超时
            /* 用户自定义超时处理 */
            break;

        case -1:        // 错误
            /* 用户自定义错误处理 */
            break;

        default:
            if(FD_ISSET(fd, &readfds)) {
                read(fd, &key_val, sizeof(int));
                if (0 == key_val)
                    printf("Key Press\n");
                else if (1 == key_val)
                    printf("Key Release\n");
            }

            break;
        }
    }

    /* 关闭设备 */
    close(fd);
    return 0;
}

