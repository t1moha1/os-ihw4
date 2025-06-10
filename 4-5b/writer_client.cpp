#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#pragma pack(push,1)
struct Request {
    uint8_t  type;   
    uint32_t pid;
    uint32_t index;
    uint32_t value;
};

struct ReplyWrite {
    uint8_t  type;
    uint32_t pid;
    uint32_t index;
    uint32_t old_value;
    uint32_t new_value;
};
#pragma pack(pop)

int main(int argc, char* argv[]) {
    if (argc != 7 ||
        std::string(argv[1]) != "-h" ||
        std::string(argv[3]) != "-p" ||
        std::string(argv[5]) != "-k") {
        std::cerr << "Usage: " << argv[0]
                  << " -h SERVER_IP -p PORT -k N_WRITERS\n";
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[2];
    int port             = std::atoi(argv[4]);
    int n_writers        = std::atoi(argv[6]);
    constexpr uint32_t DB_DUMMY = 1000;

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    std::srand(static_cast<unsigned>(std::time(nullptr)) ^ getpid());

    for (int i = 0; i < n_writers; ++i) {
        if (fork() == 0) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { perror("socket"); _exit(EXIT_FAILURE); }

            while (true) {
                Request req{1,
                            static_cast<uint32_t>(getpid()),
                            static_cast<uint32_t>(std::rand() % DB_DUMMY),
                            static_cast<uint32_t>(1 + std::rand() % 10000)};
                sendto(sock, &req, sizeof(req), 0,
                       reinterpret_cast<sockaddr*>(&servaddr),
                       sizeof(servaddr));

                ReplyWrite resp;
                if (recvfrom(sock, &resp, sizeof(resp), 0, nullptr, nullptr) < 0) {
                    perror("recvfrom");
                    break;
                }
                std::cout << "Writer " << resp.pid
                          << ": idx=" << resp.index
                          << ", old=" << resp.old_value
                          << ", new=" << resp.new_value
                          << "\n";
                sleep(1 + std::rand() % 5);
            }

            close(sock);
            _exit(EXIT_FAILURE);
        }
    }

    while (wait(nullptr) > 0);
    return EXIT_SUCCESS;
}