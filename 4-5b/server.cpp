#include <iostream>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#pragma pack(push,1)
struct Request {
    uint8_t  type;    
    uint32_t pid;
    uint32_t index;
    uint32_t value;   
};

struct ReplyRead {
    uint8_t  type;   
    uint32_t pid;
    uint32_t index;
    uint32_t value;
    uint64_t computed; 
};

struct ReplyWrite {
    uint8_t  type;    
    uint32_t pid;
    uint32_t index;
    uint32_t old_value;
    uint32_t new_value;
};
#pragma pack(pop)

static std::vector<int> db;
static size_t db_size = 0;
static std::mutex mtx_readcount;
static std::condition_variable cv_readers_done;
static std::mutex mtx_writer;
static int reader_count = 0;

void handle_request(int sock, Request req, sockaddr_in client, socklen_t client_len) {
    if (req.type == 0) {
        {
            std::lock_guard<std::mutex> lock(mtx_readcount);
            ++reader_count;
        }

        uint32_t idx  = req.index % db_size;
        uint32_t val  = db[idx];
        uint64_t comp = static_cast<uint64_t>(val) * idx;
        std::this_thread::sleep_for(std::chrono::microseconds(100000 + std::rand() % 200000));

        {
            std::lock_guard<std::mutex> lock(mtx_readcount);
            if (--reader_count == 0)
                cv_readers_done.notify_one();
        }

        ReplyRead resp{0, req.pid, idx, val, comp};
        sendto(sock, &resp, sizeof(resp), 0,
               reinterpret_cast<sockaddr*>(&client), client_len);

    } else {
        std::unique_lock<std::mutex> wlock(mtx_writer);
        std::unique_lock<std::mutex> rlock(mtx_readcount);
        cv_readers_done.wait(rlock, []{ return reader_count == 0; });
        rlock.unlock();

        uint32_t idx  = req.index % db_size;
        uint32_t oldv = db[idx];
        uint32_t newv = req.value;
        db[idx] = newv;
        std::sort(db.begin(), db.end());
        std::this_thread::sleep_for(std::chrono::microseconds(200000 + std::rand() % 200000));

        ReplyWrite resp{1, req.pid, idx, oldv, newv};
        sendto(sock, &resp, sizeof(resp), 0,
               reinterpret_cast<sockaddr*>(&client), client_len);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " -p PORT -s DB_SIZE\n";
        return EXIT_FAILURE;
    }

    int port = 0;
    for (int i = 1; i < argc; i += 2) {
        if (std::strcmp(argv[i], "-p") == 0)
            port = std::atoi(argv[i+1]);
        else if (std::strcmp(argv[i], "-s") == 0)
            db_size = std::atoi(argv[i+1]);
    }

    if (port <= 0 || db_size == 0) {
        std::cerr << "Invalid port or DB size\n";
        return EXIT_FAILURE;
    }

    db.resize(db_size);
    for (size_t i = 0; i < db_size; ++i)
        db[i] = static_cast<int>(i + 1);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    sockaddr_in servaddr{};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on UDP port " << port
              << ", DB size " << db_size << "\n";
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    while (true) {
        Request req;
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        if (recvfrom(sock, &req, sizeof(req), 0,
                     reinterpret_cast<sockaddr*>(&client), &len) < 0) {
            perror("recvfrom");
            continue;
        }
        std::thread t(handle_request, sock, req, client, len);
        t.detach();
    }

    close(sock);
    return EXIT_SUCCESS;
}