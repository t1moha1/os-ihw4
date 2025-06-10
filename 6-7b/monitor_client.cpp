#include <iostream>
#include <cstdlib>
#include <cstring>
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

int main(int argc, char *argv[])
{
    if (argc != 5 ||
        std::string(argv[1]) != "-h" ||
        std::string(argv[3]) != "-p") {
        std::cerr << "Usage: " << argv[0]
                  << " -h SERVER_IP -p PORT\n";
        return EXIT_FAILURE;
    }

    const char *serverIp = argv[2];
    int port = std::atoi(argv[4]);

    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    inet_pton(AF_INET, serverIp, &servAddr.sin_addr);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return EXIT_FAILURE; }

    Request sub{2, static_cast<uint32_t>(getpid()), 0, 0};
    sendto(sock, &sub, sizeof(sub), 0,
           reinterpret_cast<sockaddr*>(&servAddr), sizeof(servAddr));

    std::cout << "Monitor subscribed to " << serverIp << ":" << port << "\n";

    while (true) {
        uint8_t buf[64];
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n <= 0) continue;

        uint8_t type = buf[0];
        if (type == 0 && n >= sizeof(ReplyRead)) {
            auto *r = reinterpret_cast<ReplyRead*>(buf);
            std::cout << "[READ]  pid=" << r->pid
                      << " idx=" << r->index
                      << " val=" << r->value
                      << " comp=" << r->computed
                      << "\n";
        }
        else if (type == 1 && n >= sizeof(ReplyWrite)) {
            auto *w = reinterpret_cast<ReplyWrite*>(buf);
            std::cout << "[WRITE] pid=" << w->pid
                      << " idx=" << w->index
                      << " old=" << w->old_value
                      << " new=" << w->new_value
                      << "\n";
        }
    }

    close(sock);
    return EXIT_SUCCESS;
}