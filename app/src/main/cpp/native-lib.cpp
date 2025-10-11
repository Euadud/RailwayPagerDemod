#include <jni.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>
#include "demod.h"

#define BUF_SIZE 4096

static std::thread workerThread;
static std::atomic<bool> running(false);
static std::mutex msgMutex;
static std::vector<std::string> messageBuffer;

// JNI 环境全局变量
static JavaVM* g_vm = nullptr;
static jobject g_obj = nullptr; // Java 的 MainActivity 实例（全局引用）

extern "C" JNIEXPORT void JNICALL
Java_com_example_railwaypagerdemod_MainActivity_nativeStopClient(JNIEnv*, jobject) {
    running = false;
}

// worker线程：负责连接 TCP 并持续接收/解码
void clientThread(std::string host, int port) {
    int sockfd;
    struct sockaddr_in servaddr;
    int8_t buffer[BUF_SIZE];
    ssize_t n;

    lowpassBaud.create(301, SAMPLE_RATE, BAUD_RATE * 5.0f);
    phaseDiscri.setFMScaling(SAMPLE_RATE / (2.0f * DEVIATION));

    // 1. socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::lock_guard<std::mutex> lock(msgMutex);
        messageBuffer.emplace_back("socket() failed\n");
        return;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        std::lock_guard<std::mutex> lock(msgMutex);
        messageBuffer.emplace_back("connect() failed\n");
        close(sockfd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(msgMutex);
        messageBuffer.emplace_back("Connected to " + host + ":" + std::to_string(port) + "\n");
    }

    running = true;

    const int DECIM = 5;
    int decim_counter = 0;
    int32_t acc_i = 0, acc_q = 0;

    while (running && (n = read(sockfd, buffer, BUF_SIZE)) > 0) {
//        for (int j = 0; j < BUF_SIZE; j += 2) {
//            int8_t i = (int8_t)(buffer[j] - 128);
//            int8_t q = (int8_t)(buffer[j+1] - 128);
//            processOneSample(i, q);
//        }

        for (int j = 0; j < n; j += 2) {
            int8_t i = buffer[j];
            int8_t q = buffer[j+1];

            acc_i += i;
            acc_q += q;
            decim_counter++;

            if (decim_counter == DECIM) {
                int8_t i_ds = acc_i / DECIM;
                int8_t q_ds = acc_q / DECIM;
                processOneSample(i_ds, q_ds);
                acc_i = acc_q = 0;
                decim_counter = 0;
            }
        }

        if (is_message_ready) {
            std::ostringstream ss;
            char addr_buf[32];
            snprintf(addr_buf, sizeof(addr_buf), "%010d ", address);  // 10 + 10位补零
            ss << "[MSG] " << addr_buf << numeric_msg;
            {
                std::lock_guard<std::mutex> lock(msgMutex);
                messageBuffer.push_back(ss.str());
            }
            is_message_ready = false;
        }
    }

    if (n < 0) {
        std::lock_guard<std::mutex> lock(msgMutex);
        messageBuffer.emplace_back("read() error\n");
    }

    close(sockfd);
    running = false;

    std::lock_guard<std::mutex> lock(msgMutex);
    messageBuffer.emplace_back("Connection closed\n");
}

// === JNI: 启动客户端 ===
extern "C"
JNIEXPORT void JNICALL
Java_com_example_railwaypagerdemod_MainActivity_startClientAsync(
        JNIEnv* env, jobject thiz, jstring host_, jstring port_) {

    const char* host = env->GetStringUTFChars(host_, nullptr);
    const char* portStr = env->GetStringUTFChars(port_, nullptr);
    int port = atoi(portStr);

    // 保存 Java 对象引用
    if (g_obj == nullptr) {
        env->GetJavaVM(&g_vm);
        g_obj = env->NewGlobalRef(thiz);
    }

    if (running) {
        // 已经在运行，忽略重复启动
        env->ReleaseStringUTFChars(host_, host);
        env->ReleaseStringUTFChars(port_, portStr);
        return;
    }

    workerThread = std::thread(clientThread, std::string(host), port);
    workerThread.detach();

    env->ReleaseStringUTFChars(host_, host);
    env->ReleaseStringUTFChars(port_, portStr);
}

// === JNI: 拉取一批最新消息 ===
extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_railwaypagerdemod_MainActivity_pollMessages(JNIEnv* env, jobject /*this*/) {
    std::lock_guard<std::mutex> lock(msgMutex);
    if (messageBuffer.empty())
        return env->NewStringUTF("");

    std::ostringstream ss;
    for (auto& msg : messageBuffer) ss << msg << "\n";
    messageBuffer.clear();

    return env->NewStringUTF(ss.str().c_str());
}
