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

struct ReplyRead {
    uint8_t  type;
    uint32_t pid;
    uint32_t index;
    uint32_t value;
    uint64_t computed;
};
#pragma pack(pop)

int main(int argc, char* argv[]) {
    if (argc != 7 ||
        std::string(argv[1]) != "-h" ||
        std::string(argv[3]) != "-p" ||
        std::string(argv[5]) != "-n") {
        std::cerr << "Usage: " << argv[0]
                  << " -h SERVER_IP -p PORT -n N_READERS\n";
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[2];
    int port             = std::atoi(argv[4]);
    int n_readers        = std::atoi(argv[6]);
    constexpr uint32_t DB_DUMMY = 1000;

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    std::srand(static_cast<unsigned>(std::time(nullptr)) ^ getpid());

    for (int i = 0; i < n_readers; ++i) {
        if (fork() == 0) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { perror("socket"); _exit(EXIT_FAILURE); }

            while (true) {
                Request req{0,
                            static_cast<uint32_t>(getpid()),
                            static_cast<uint32_t>(std::rand() % DB_DUMMY),
                            0u};
                sendto(sock, &req, sizeof(req), 0,
                       reinterpret_cast<sockaddr*>(&servaddr),
                       sizeof(servaddr));

                ReplyRead resp;
                if (recvfrom(sock, &resp, sizeof(resp), 0, nullptr, nullptr) < 0) {
                    perror("recvfrom");
                    break;
                }
                std::cout << "Reader " << resp.pid
                          << ": idx=" << resp.index
                          << ", val=" << resp.value
                          << ", comp=" << resp.computed
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