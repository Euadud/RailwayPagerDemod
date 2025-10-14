#include "railway_pager_demod.h"
#include "demod.h"

int main(void) {
    lowpassBaud.create(
        301,               // 滤波器阶数 (taps)
        SAMPLE_RATE,           // 采样率
        BAUD_RATE * 5.0f      // 截止频率
    );
    phaseDiscri.setFMScaling(SAMPLE_RATE / (2.0f * DEVIATION));


    int sockfd;
    struct sockaddr_in servaddr;
    uint8_t buffer[BUF_SIZE];
    ssize_t n;

    // 1. Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    // 2. Fill server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // 3. Connect to server
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s:%d — reading bytes...\n", SERVER_IP, SERVER_PORT);

    // // 4. Read loop
    // while ((n = read(sockfd, buffer, BUF_SIZE)) > 0) {
    //     for (int j = 0; j < BUF_SIZE; j += 2) {
    //         int8_t i = buffer[j];
    //         int8_t q = buffer[j+1];
    //         processOneSample(i, q);
    //     }
    //     if (is_message_ready) printf("[MSG] %s\n", numeric_msg.c_str());
    //     is_message_ready = false;
    // }

    const int DECIM = 5;
    int decim_counter = 0;
    uint32_t acc_i = 0, acc_q = 0;

    while ((n = read(sockfd, buffer, BUF_SIZE)) > 0) {
        for (int j = 0; j < n; j += 2) {
            acc_i += buffer[j];
            acc_q += buffer[j + 1];
            if (++decim_counter == DECIM) {
                int8_t i_ds = (int8_t)(((float) acc_i / DECIM) - 128);
                int8_t q_ds = (int8_t)(((float) acc_q / DECIM) - 128);
                // printf("%d %d\n", i_ds, q_ds);
                processOneSample(i_ds, q_ds);
                acc_i = acc_q = 0;
                decim_counter = 0;
            }
        }

        if (is_message_ready) {
            printf("[MSG] %s\n", numeric_msg.c_str());
            is_message_ready = false;
        }
    }

    if (n < 0) perror("read error");

    close(sockfd);
    return 0;
}