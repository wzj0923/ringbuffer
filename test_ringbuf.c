#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define DEVICE_PATH "/dev/ringbuf"
#define BUFFER_SIZE 4096

// 写进程
void writer_process(const char *message, int count)
{
    int fd;
    int i;
    char buffer[256];
    
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Writer: open failed");
        exit(1);
    }
    
    for (i = 0; i < count; i++) {
        snprintf(buffer, sizeof(buffer), "%s %d", message, i);
        ssize_t ret = write(fd, buffer, strlen(buffer));
        if (ret < 0) {
            perror("Writer: write failed");
            break;
        }
        printf("Writer: wrote %ld bytes: %s\n", ret, buffer);
        usleep(100000); // 等待100ms
    }
    
    close(fd);
    printf("Writer: finished\n");
}

// 读进程
void reader_process(int count)
{
    int fd;
    int i;
    char buffer[256];
    
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Reader: open failed");
        exit(1);
    }
    
    for (i = 0; i < count; i++) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t ret = read(fd, buffer, sizeof(buffer) - 1);
        if (ret < 0) {
            perror("Reader: read failed");
            break;
        } else if (ret == 0) {
            printf("Reader: no data available\n");
            usleep(50000);
            continue;
        }
        printf("Reader: read %ld bytes: %s\n", ret, buffer);
        usleep(50000);
    }
    
    close(fd);
    printf("Reader: finished\n");
}

// 综合测试：先写后读
void test_write_read(void)
{
    int fd;
    char write_buf[] = "Hello, Ring Buffer Driver!";
    char read_buf[100];
    ssize_t ret;
    
    printf("\n=== Test 1: Write then Read ===\n");
    
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("open failed");
        return;
    }
    
    // 写入数据
    ret = write(fd, write_buf, strlen(write_buf));
    if (ret < 0) {
        perror("write failed");
        close(fd);
        return;
    }
    printf("Write: %ld bytes written: %s\n", ret, write_buf);
    
    // 读取数据
    memset(read_buf, 0, sizeof(read_buf));
    ret = read(fd, read_buf, sizeof(read_buf) - 1);
    if (ret < 0) {
        perror("read failed");
    } else {
        printf("Read: %ld bytes read: %s\n", ret, read_buf);
        
        // 验证数据一致性
        if (strcmp(write_buf, read_buf) == 0)
            printf("Result: PASS - Data matches!\n");
        else
            printf("Result: FAIL - Data mismatch!\n");
    }
    
    close(fd);
}

// 环形缓冲区满测试
void test_buffer_full(void)
{
    int fd;
    char buf[512];
    int i;
    ssize_t ret;
    
    printf("\n=== Test 2: Buffer Full Test ===\n");
    
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open failed");
        return;
    }
    
    // 写入超过缓冲区大小的数据
    for (i = 0; i < 10; i++) {
        memset(buf, 'A' + (i % 26), sizeof(buf));
        ret = write(fd, buf, sizeof(buf));
        printf("Write %d: wrote %ld bytes\n", i, ret);
    }
    
    close(fd);
    printf("Test completed\n");
}

// 并发读写测试（多进程）
void test_concurrent(void)
{
    pid_t writer_pid, reader_pid;
    
    printf("\n=== Test 3: Concurrent Read/Write ===\n");
    
    writer_pid = fork();
    if (writer_pid == 0) {
        writer_process("Message from writer", 20);
        exit(0);
    }
    
    reader_pid = fork();
    if (reader_pid == 0) {
        reader_process(25);
        exit(0);
    }
    
    wait(NULL);
    wait(NULL);
    
    printf("Concurrent test completed\n");
}

int main(int argc, char *argv[])
{
    printf("=== Ring Buffer Driver Test Suite ===\n");
    printf("Device: %s\n", DEVICE_PATH);
    printf("Buffer size: %d bytes\n\n", BUFFER_SIZE);
    
    // 运行测试
    test_write_read();
    test_buffer_full();
    test_concurrent();
    
    printf("\n=== All tests completed ===\n");
    return 0;
