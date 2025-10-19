#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

class SimpleHttpServer {
public:
    void run() {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);
        
        // Создаем сокет
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            spdlog::error("Socket creation failed");
            return;
        }
        
        // Настройка сокета
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            spdlog::error("Setsockopt failed");
            return;
        }
        
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(9999);
        
        // Привязываем сокет
        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            spdlog::error("Bind failed");
            return;
        }
        
        // Слушаем
        if (listen(server_fd, 3) < 0) {
            spdlog::error("Listen failed");
            return;
        }
        
        spdlog::info("Simple HTTP server listening on port 9999");
        
        while (true) {
            // Принимаем соединение
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                spdlog::error("Accept failed");
                continue;
            }
            
            spdlog::info("New connection accepted");
            
            // Читаем запрос
            char buffer[1024] = {0};
            read(new_socket, buffer, 1024);
            spdlog::info("Received request:\n{}", buffer);
            
            // Простой HTTP ответ
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Hello World!";
            
            // Отправляем ответ
            send(new_socket, response.c_str(), response.length(), 0);
            spdlog::info("Response sent");
            
            // Закрываем соединение
            close(new_socket);
            spdlog::info("Connection closed");
        }
        
        close(server_fd);
    }
};