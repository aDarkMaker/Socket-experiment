#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <dirent.h>

using namespace std;

class ServerConfig {
private:
    string listen_address;
    int listen_port;
    string web_root;
    string config_file;
    time_t last_modify_time;

public:
    ServerConfig() : listen_address("0.0.0.0"), listen_port(8080), web_root("www"), last_modify_time(0) {}
    
    bool isConfigModified(const string& filename) {
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0) {
            return false;
        }
        
        if (file_stat.st_mtime != last_modify_time) {
            last_modify_time = file_stat.st_mtime;
            return true;
        }
        
        return false;
    }
    
    bool loadFromFile(const string& filename) {
        config_file = filename;
        
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0) {
            cerr << "警告: 无法打开配置文件 " << filename << "，使用默认配置" << endl;
            return false;
        }
        
        last_modify_time = file_stat.st_mtime;
        
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "警告: 无法打开配置文件 " << filename << "，使用默认配置" << endl;
            return false;
        }
        
        string new_listen_address = listen_address;
        int new_listen_port = listen_port;
        string new_web_root = web_root;
        
        string line;
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find('=');
            if (pos == string::npos) continue;
            
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);
            
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            if (key == "listen_address") {
                new_listen_address = value;
            } else if (key == "listen_port") {
                try {
                    new_listen_port = stoi(value);
                } catch (...) {
                    cerr << "警告: 无效的端口号 " << value << "，保持当前配置" << endl;
                    continue;
                }
            } else if (key == "web_root") {
                new_web_root = value;
            }
        }
        
        file.close();
        
        if (new_listen_address != listen_address) {
            cout << "[配置更新] 监听地址: " << listen_address << " -> " << new_listen_address << endl;
            listen_address = new_listen_address;
        }
        if (new_listen_port != listen_port) {
            cout << "[配置更新] 监听端口: " << listen_port << " -> " << new_listen_port << endl;
            listen_port = new_listen_port;
        }
        if (new_web_root != web_root) {
            cout << "[配置更新] Web根目录: " << web_root << " -> " << new_web_root << endl;
            web_root = new_web_root;
        }
        
        return true;
    }
    
    string getListenAddress() const { return listen_address; }
    int getListenPort() const { return listen_port; }
    string getWebRoot() const { return web_root; }
    string getConfigFile() const { return config_file; }
};

class HTTPResponse {
public:
    static string getStatusMessage(int code) {
        static map<int, string> statusMessages = {
            {200, "OK"},
            {400, "Bad Request"},
            {404, "Not Found"},
            {403, "Forbidden"},
            {500, "Internal Server Error"},
            {501, "Not Implemented"}
        };
        
        if (statusMessages.find(code) != statusMessages.end()) {
            return statusMessages[code];
        }
        return "Unknown";
    }
    
    static string getContentType(const string& filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == string::npos) {
            return "application/octet-stream";
        }
        
        string ext = filename.substr(pos + 1);
        map<string, string> mimeTypes = {
            {"html", "text/html; charset=utf-8"},
            {"htm", "text/html; charset=utf-8"},
            {"css", "text/css"},
            {"js", "application/javascript"},
            {"json", "application/json"},
            {"png", "image/png"},
            {"jpg", "image/jpeg"},
            {"jpeg", "image/jpeg"},
            {"gif", "image/gif"},
            {"svg", "image/svg+xml"},
            {"ico", "image/x-icon"},
            {"txt", "text/plain"},
            {"pdf", "application/pdf"},
            {"xml", "application/xml"},
            {"zip", "application/zip"}
        };
        
        for (char& c : ext) {
            c = tolower(c);
        }
        
        if (mimeTypes.find(ext) != mimeTypes.end()) {
            return mimeTypes[ext];
        }
        return "application/octet-stream";
    }
};

class WebServer {
private:
    ServerConfig config;
    int server_fd;
    struct sockaddr_in server_addr;
    int old_port;
    
    bool reinitialize() {
        int new_port = config.getListenPort();
        
        if (new_port == old_port) {
            return true;
        }
        
        cout << "[配置热重载] 检测到端口改变，正在重新绑定socket..." << endl;
        
        if (server_fd >= 0) {
            close(server_fd);
        }
        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            cerr << "无法创建 socket" << endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(config.getListenAddress().c_str());
        server_addr.sin_port = htons(config.getListenPort());
        
        if (::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "无法绑定地址 " << config.getListenAddress() 
            << ":" << config.getListenPort() << endl;
            close(server_fd);
            server_fd = -1;
            return false;
        }
        
        if (listen(server_fd, 10) < 0) {
            cerr << "无法监听" << endl;
            close(server_fd);
            server_fd = -1;
            return false;
        }
        
        old_port = new_port;
        cout << "[配置热重载] 服务器已重新绑定到端口 " << new_port << endl;
        return true;
    }
    
    string getCurrentTime() {
        time_t now = time(nullptr);
        string result(30, '\0');
        strftime(&result[0], result.size(), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
        return result.substr(0, result.find('\0'));
    }
    
    bool fileExists(const string& filename) {
        ifstream file(filename);
        return file.good();
    }
    
    size_t getFileSize(const string& filename) {
        ifstream file(filename, ios::binary | ios::ate);
        if (!file.is_open()) return 0;
        return file.tellg();
    }
    
    string readFile(const string& filename) {
        ifstream file(filename, ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }
    
    bool parseRequest(const string& request, string& method, string& path) {
        istringstream iss(request);
        iss >> method >> path;
        
        if (method.empty() || path.empty()) {
            return false;
        }
        
        return true;
    }
    
    string generateResponseHeader(int statusCode, const string& contentType, size_t contentLength) {
        ostringstream header;
        header << "HTTP/1.1 " << statusCode << " " << HTTPResponse::getStatusMessage(statusCode) << "\r\n";
        header << "Server: WebServer-Experimental\r\n";
        header << "Date: " << getCurrentTime() << "\r\n";
        header << "Content-Type: " << contentType << "\r\n";
        header << "Content-Length: " << contentLength << "\r\n";
        header << "Connection: close\r\n";
        header << "\r\n";
        
        return header.str();
    }
    
    string generateErrorPage(int code, const string& message) {
        ostringstream html;
        html << "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='UTF-8'>";
        html << "<title>错误 " << code << "</title></head><body>";
        html << "<h1>错误 " << code << "</h1>";
        html << "<p>" << message << "</p>";
        html << "<hr><p><em>Web Server Experimental</em></p></body></html>";
        return html.str();
    }
    
public:
    WebServer() : server_fd(-1), old_port(0) {}
    
    bool initialize() {
        config.loadFromFile("server_config.txt");
        old_port = config.getListenPort();
        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            cerr << "无法创建 socket" << endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(config.getListenAddress().c_str());
        server_addr.sin_port = htons(config.getListenPort());
        
        if (::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "无法绑定地址 " << config.getListenAddress() 
            << ":" << config.getListenPort() << endl;
            close(server_fd);
            return false;
        }
        
        if (listen(server_fd, 10) < 0) {
            cerr << "无法监听" << endl;
            close(server_fd);
            return false;
        }
        
        return true;
    }
    
    void run() {
        cout << "========================================" << endl;
        cout << "Web 服务器已启动" << endl;
        cout << "监听地址: " << config.getListenAddress() << endl;
        cout << "监听端口: " << config.getListenPort() << endl;
        cout << "Web 根目录: " << config.getWebRoot() << endl;
        cout << "========================================" << endl;
        cout << "访问地址: http://localhost:" << config.getListenPort() << endl;
        cout << "提示: 修改 server_config.txt 可实时更新配置（无需重启）" << endl;
        cout << "按 Ctrl+C 停止服务器" << endl;
        cout << "========================================" << endl;
        
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        while (true) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server_fd, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            
            int select_result = select(server_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (select_result == 0) {
                if (config.isConfigModified(config.getConfigFile())) {
                    cout << "[配置检测] 检测到配置文件变化，正在重新加载..." << endl;
                    config.loadFromFile(config.getConfigFile());
                    
                    if (config.getListenPort() != old_port) {
                        int saved_port = old_port;
                        if (!reinitialize()) {
                            cerr << "[配置热重载] 重新绑定失败，保持旧端口配置" << endl;
                            old_port = saved_port;
                        } else {
                            cout << "[配置热重载] 配置已成功更新！" << endl;
                            cout << "新的访问地址: http://localhost:" << config.getListenPort() << endl;
                        }
                    } else {
                        cout << "[配置热重载] 配置已成功更新！" << endl;
                    }
                }
                continue;
            }
            
            if (select_result < 0) {
                cerr << "select 错误" << endl;
                continue;
            }
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd < 0) {
                cerr << "接受连接失败" << endl;
                continue;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            
            char buffer[8192] = {0};
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                close(client_fd);
                continue;
            }
            
            string request(buffer);
            
            string method, path;
            bool parse_success = parseRequest(request, method, path);
            
            cout << "[" << getCurrentTime() << "] ";
            cout << client_ip << ":" << client_port << " ";
            cout << method << " " << path;
            
            if (!parse_success) {
                cout << " [400 Bad Request]" << endl;
                
                string errorPage = generateErrorPage(400, "请求格式错误");
                string header = generateResponseHeader(400, "text/html; charset=utf-8", errorPage.length());
                string response = header + errorPage;
                send(client_fd, response.c_str(), response.length(), 0);
                
                close(client_fd);
                continue;
            }
            
            if (method != "GET") {
                cout << " [501 Not Implemented]" << endl;
                
                string errorPage = generateErrorPage(501, "不支持的方法");
                string header = generateResponseHeader(501, "text/html; charset=utf-8", errorPage.length());
                string response = header + errorPage;
                send(client_fd, response.c_str(), response.length(), 0);
                
                close(client_fd);
                continue;
            }
            
            string full_path = config.getWebRoot() + path;
            
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                full_path += "/index.html";
            }
            
            if (full_path.find("..") != string::npos) {
                cout << " [403 Forbidden]" << endl;
                
                string errorPage = generateErrorPage(403, "禁止访问");
                string header = generateResponseHeader(403, "text/html; charset=utf-8", errorPage.length());
                string response = header + errorPage;
                send(client_fd, response.c_str(), response.length(), 0);
                
                close(client_fd);
                continue;
            }
            
            if (!fileExists(full_path)) {
                cout << " [404 Not Found]" << endl;
                
                string errorPage = generateErrorPage(404, "文件未找到");
                string header = generateResponseHeader(404, "text/html; charset=utf-8", errorPage.length());
                string response = header + errorPage;
                send(client_fd, response.c_str(), response.length(), 0);
                
                close(client_fd);
                continue;
            }
            
            string file_content = readFile(full_path);
            if (file_content.empty()) {
                cout << " [500 Internal Server Error]" << endl;
                
                string errorPage = generateErrorPage(500, "读取文件失败");
                string header = generateResponseHeader(500, "text/html; charset=utf-8", errorPage.length());
                string response = header + errorPage;
                send(client_fd, response.c_str(), response.length(), 0);
                
                close(client_fd);
                continue;
            }
            
            string content_type = HTTPResponse::getContentType(full_path);
            string header = generateResponseHeader(200, content_type, file_content.length());
            string response = header + file_content;
            
            send(client_fd, response.c_str(), response.length(), 0);
            
            cout << " [200 OK]" << endl;
            
            close(client_fd);
        }
    }
    
    ~WebServer() {
        if (server_fd >= 0) {
            close(server_fd);
        }
    }
};

int main() {
    WebServer server;
    
    if (!server.initialize()) {
        cerr << "初始化失败" << endl;
        return 1;
    }
    
    server.run();
    
    return 0;
}

