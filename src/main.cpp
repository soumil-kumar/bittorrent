#include <bits/stdc++.h>
#include "lib/nlohmann/json.hpp"
#include "lib/sha1.hpp"
//#include "lib/curl/curl.h"
#include <curl/curl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

using namespace std;

enum MessageID{
    Choke_MSG = 0x0,
    Unchoke_MSG,
    Interested_MSG,
    Not_MSGInterested,
    Have_MSG,
    Bitfield_MSG,
    Request_MSG,
    Piece_MSG,
    Cancel_MSG,
};

using json = nlohmann::json;

json decode_bencoded_value(const std::string& encoded_value, int &offset) {
    if (std::isdigit(encoded_value[0])) {
        // Example: "5:hello" -> "hello"
        size_t colon_index = encoded_value.find(':');
        if (colon_index != std::string::npos) {
            std::string number_string = encoded_value.substr(0, colon_index);
            int64_t number = std::atoll(number_string.c_str());
            offset = colon_index + 1 + number;
            std::string str = encoded_value.substr(colon_index + 1, number);
            return json(str);
        } else {
            throw std::runtime_error("Invalid encoded value: " + encoded_value);
        }
    }else if (encoded_value[0] == 'i') {
        offset = encoded_value.find('e') + 1;   
        return json(stol(encoded_value.substr(1, offset)));
    }else if (encoded_value[0] == 'l') {
        auto res = json::array({});
        int beg = 1;
        int end = 0;
        while(beg+1 < encoded_value.length()) {
            if(encoded_value[beg] == 'e') break;
            auto str = encoded_value.substr(beg);
            auto e = decode_bencoded_value(str, end);
            //if(end == -1) break;
            res.push_back(e);
            beg += end;
        }
        offset = beg + 1;
        return res;
    }else if(encoded_value[0] == 'd'){
        auto res = json({});
        int beg = 1;
        int end = 0;
        while (beg + 1 < encoded_value.length()){
            if(encoded_value[beg] == 'e') break;
            auto key_str = encoded_value.substr(beg);
            auto key = decode_bencoded_value(key_str, end);
            beg += end;
            if(encoded_value[beg] == 'e') break;
            auto value_str = encoded_value.substr(beg);
            auto value = decode_bencoded_value(value_str, end);
            beg += end;
            res[key] = value;
        }
        offset = beg + 1;
        return res;
    }else {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}   

string process_torrent_file(string &file_name){
    ifstream file = ifstream(file_name, ios::binary);
    if(!file){
        cerr << "Error opening file \n";
        return "";
    }
    file.seekg(0, ios::end);
    long long file_size = file.tellg();
    file.seekg(0, ios::beg);
    string buffer;
    buffer.resize(file_size);
    file.read(buffer.data(), file_size);
    file.close();
    return buffer;
}

string hex_to_bytes(string &hex) {
    string bytes;
    for(int i = 0; i < hex.size(); i+=2) {
        string hex_substring = hex.substr(i, 2);
        uint8_t byte = (uint8_t)(stoi(hex_substring, nullptr, 16));
        bytes.push_back(byte);   
    }
    return bytes;
}

string bytes_to_hex(unsigned char *bytes, size_t length) {
    string hex;
    hex.reserve(length*2);
    for(size_t i=0; i<length; i++) {
        hex += format("{:02x}", bytes[i]);
    }
    return hex;
}

string url_encode_binary(string &binary_data) {
    ostringstream encoded;
    encoded.fill('0');
    encoded << hex << uppercase;
    for(uint8_t c : binary_data) {
        encoded << '%' << setw(2) << static_cast<int>(c);
    }
    return encoded.str();
}

string url_decode(const string &encoded){
    string decoded;
    decoded.reserve(encoded.size());
    for(char *chr = (char *)encoded.data(); *chr; chr++){
        if((*chr) == '%'){
            stringstream hex_stream;
            hex_stream << *(chr+1);
            hex_stream << *(chr+2);
            int hex_value;
            if (hex_stream >> hex >> hex_value) {
                decoded += (char)hex_value;
                chr += 2;
            }else decoded += (*chr);
        }else decoded += (*chr);
    }
    return decoded;
}

string get_info_hash_bytes(string &buffer) {
    int info_idx = buffer.find("4:info") + strlen("4:info");
    auto info_coded = buffer.substr(info_idx, buffer.size() - info_idx - 1);
    auto hex_hash = sha1(info_coded);
    return hex_to_bytes(hex_hash);
}


static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
{
    userp->append((char *)contents, size * nmemb);
    return size * nmemb;
}

string get_peers(string &tracker_url, string &info_hash_byte, int length=1) {
    string url_encoded_hash = url_encode_binary(info_hash_byte);
    auto url = format("{}?info_hash={}&peer_id=THIS_IS_SOUMILooo_10&port=6881&uploaded=0&downloaded=0&left={}&compact=1",
        tracker_url, url_encoded_hash, length);
    cerr << "url: " << url << '\n';
    string response;
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        cerr << "curl easy perform failed: " << curl_easy_strerror(res) << '\n';
    }
    int offset = 0;
    json decoded_resp = decode_bencoded_value(response, offset);
    auto peers_bin = decoded_resp["peers"].get<string>();
    string out = "";
    for(int i=0; i<peers_bin.length(); i+=6) {
        uint8_t b = peers_bin[i];
        out += to_string((uint8_t)peers_bin[i]) + '.';
        out += to_string((uint8_t)peers_bin[i + 1]) + '.';
        out += to_string((uint8_t)peers_bin[i + 2]) + '.';
        out += to_string((uint8_t)peers_bin[i + 3]) + ':';     
        uint16_t port = ((uint8_t)peers_bin[i + 4] << 8) | (uint8_t)peers_bin[i + 5];
        out += to_string(port) + '\n';   
    }
    return out.substr(0, out.length()-1);
}

string get_peers(string &buffer) {
    int offset = 0;
    json decoded_value = decode_bencoded_value(buffer, offset);
    string tracker_url = decoded_value["announce"].get<string>();
    int length = decoded_value["info"]["length"].get<int>();
    string info_hash_byte = get_info_hash_bytes(buffer);
    return get_peers(tracker_url, info_hash_byte, length);
}

struct PeerInfo {
    int sock_fd = -1;
    uint8_t ut_metadata = 0;
    bool extension = false;
};

PeerInfo handshake(string &peer, string &info_hash_bytes, bool extension = false) {
    //connect
    string peer_ip = peer.substr(0, peer.find(':'));
    int port = atoi(peer.substr(peer_ip.length() + 1).c_str());
    PeerInfo res;
    res.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in peer_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {htonl(INADDR_ANY)},
    };
    inet_pton(AF_INET, peer_ip.c_str(), &peer_addr.sin_addr);
    connect(res.sock_fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));

    //68-byte handshake 
    char handshake_buf[68];
    handshake_buf[0] = 19;
    memcpy(handshake_buf + 1, "BitTorrent protocol", 19);
    memset(handshake_buf + 20, 0, 8);                           //reserved bytes
    memcpy(handshake_buf + 28, info_hash_bytes.c_str(), 20);    //SHA1 hash    
    memcpy(handshake_buf + 48, "THIS_IS_SOUMIL_KUMAR", 20);     //peer identifier
    if(extension) handshake_buf[25] = 0x10;
    ssize_t bytes_sent = write(res.sock_fd, handshake_buf, 68);
    char response[68];
    ssize_t bytes_read = read(res.sock_fd, response, 68);

    //connect                                                           
    if(bytes_read == 68 && response[0] == 19) {
        if (memcmp(response + 28, info_hash_bytes.c_str(), 20) == 0) {
            auto peer_id = bytes_to_hex((uint8_t *)(response + 48), 20);
            cout<<"Peer ID: "<<peer_id<<'\n';
            if(response[25] == 0x10){
                cerr<<"-------------[ Extension Supported ]----------"<<'\n';
                std::string out = "d1:md11:ut_metadatai1e6:ut_pexi2ee1:pi6881ee";
                uint32_t len = out.length() + /* message id */ 1 + /* ext message id */ 1;
                len = htonl(len);
                write(res.sock_fd, &len, 4);
                write(res.sock_fd, (char[]){20}, 1);
                write(res.sock_fd, (char[]){0}, 1);
                write(res.sock_fd, out.c_str(), out.length());
                //--[ BitField]--------------------
                uint32_t message_len;
                read(res.sock_fd, &message_len, 4);
                message_len = ntohl(message_len);
                read(res.sock_fd, response, message_len);
                //--[ Receive Extension Handshake ]--------------------
                read(res.sock_fd, &message_len, 4);
                message_len = ntohl(message_len);
                read(res.sock_fd, response, 2); // Message ID and Ext Message ID
                message_len -= 2;
                std::string bufstr("", message_len);
                size_t bytes_read = read(res.sock_fd, bufstr.data(), message_len);
                int offset = 0;
                auto decoded_value = decode_bencoded_value(bufstr, offset);
                uint8_t ut_metadata = decoded_value["m"]["ut_metadata"].get<uint8_t>();
                std::cout << "Peer Metadata Extension ID: " << (int)ut_metadata << '\n';

                res.ut_metadata = ut_metadata ;
                res.extension = true;
            }
        }else res.sock_fd = -1;
    }
    return res;
}

vector<int> get_peers_connections(string &buffer){
    auto info_hash_bytes = get_info_hash_bytes(buffer);
    string peers_str = get_peers(buffer);
    vector<string> peers;
    for(auto peer : std::views::split(peers_str, '\n')){
        peers.push_back({peer.begin(), peer.end()});
    }
    vector<int> sockets;
    sockets.reserve(peers.size());
    vector<std::thread> threads;
    threads.reserve(peers.size());
    for(auto &peer : peers){
        threads.emplace_back([&]() {
            sockets.push_back(handshake(peer, info_hash_bytes).sock_fd);
        });
    }
    for(auto &thread : threads){
        thread.join();
    }
    return sockets;
}

constexpr uint32_t CHUNK_SIZE = 16 * 1024;
constexpr size_t RESPONSE_BUFFER_SIZE = 1024;

size_t read_exactly(int sock_fd, void *buf, size_t bytes_to_read, int timeout_ms = 1000){
    size_t bytes_read = 0;
    while(bytes_read < bytes_to_read){
        struct pollfd pfd;
        pfd.fd = sock_fd;
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, timeout_ms);
        if(ret <= 0){
            fprintf(stderr, "DISCONNECT: sock_fd: %d, ret_val: %d\n", sock_fd, ret);
            close(sock_fd);
            return 0;
        }
        size_t remaining_bytes = bytes_to_read - bytes_read;
        size_t n = read(sock_fd, (uint8_t*)buf+bytes_read, remaining_bytes);
        if(n <= 0){
            fprintf(stderr, "DISCONNECT: sock_fd: %d, bytes_read: %lu\n", sock_fd, n);
            close(sock_fd);
            return 0;
        }
        bytes_read += n;
    }
    return bytes_read;
}

int download_piece(int sock_fd, uint8_t *piece_buffer, size_t piece_size, int piece_index, bool magnet = false){
    uint8_t response[RESPONSE_BUFFER_SIZE];
    //part 1: handle bitfield messages
    uint32_t message_len_network, message_len;
    if(magnet){
        write(sock_fd, (char[]){0, 0, 0, 1, Interested_MSG}, 5);
    }else{
        if(not read_exactly(sock_fd, &message_len_network, 4)) return -1;
        message_len = ntohl(message_len_network);
        if(not read_exactly(sock_fd, response, message_len)) return -1;
        if(response[0] == Bitfield_MSG){
            write(sock_fd, (char[]){0, 0, 0, 1, Interested_MSG}, 5);
        }
    }
    //part 2: handle unchoke and send request
    uint32_t chunk_count = (piece_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if(not read_exactly(sock_fd, &message_len_network, 4)) return -1;
    message_len = ntohl(message_len_network);
    if(not read_exactly(sock_fd, response, message_len)) return -1;
    if(response[0] == Unchoke_MSG){
        uint32_t piece_index_network = htonl(piece_index);
        uint32_t msg_len_network = htonl(13);
        for(int chunk_idx = 0; chunk_idx < chunk_count; chunk_idx++){
            uint32_t begin = htonl(chunk_idx * CHUNK_SIZE);
            uint32_t len = htonl(min(CHUNK_SIZE, (uint32_t)(piece_size - chunk_idx * CHUNK_SIZE)));
            write(sock_fd, &msg_len_network, 4);
            write(sock_fd, (char[]){Request_MSG}, 1);
            write(sock_fd, &piece_index_network, 4);
            write(sock_fd, &begin, 4);
            write(sock_fd, &len, 4);
        } 
    }
    //part 3: Download chunks
    for(int chunk_idx = 0; chunk_idx < chunk_count; chunk_idx++){
        if(not read_exactly(sock_fd, &message_len_network, 4)) return -1;
        message_len = ntohl(message_len_network);
        uint8_t rec_id;
        uint32_t rec_piecie_idx;
        uint32_t byte_offset_network;
        if(not read_exactly(sock_fd, &rec_id, 1)) return -1;
        if(not read_exactly(sock_fd, &rec_piecie_idx, 4)) return -1;
        if(not read_exactly(sock_fd, &byte_offset_network, 4)) return -1;
        uint32_t byte_offset = ntohl(byte_offset_network);
        size_t block_len =  message_len - 9;
        if(not read_exactly(sock_fd, piece_buffer + byte_offset, block_len)) return -1;
    }
    close(sock_fd);
    return piece_index;
}

pair<string,string> magnet_ip_and_hash(string &magnet_link) {
    auto pos = magnet_link.find("xt=urn:btih:") + strlen("xt=urn:btih:");
    string info_hash = magnet_link.substr(pos, 40);
    pos = magnet_link.find("tr=") + strlen("tr=");
    string tracker_url = url_decode(magnet_link.substr(pos, magnet_link.length() - pos));
    cout << "Info Hash: "<< info_hash<<'\n';
    cout << "Tracker URL: " << tracker_url << '\n';
    string bytes_info_hash = hex_to_bytes(info_hash);
    string peers = get_peers(tracker_url, bytes_info_hash);
    return {peers, bytes_info_hash};
}

PeerInfo magnet_handshake(string &magnet_link) {
    auto [peers, bytes_info_hash] = magnet_ip_and_hash(magnet_link);
    return handshake(peers, bytes_info_hash, true);
}

PeerInfo magnet_handshake(int argc, char* argv[]){
    if(argc < 3){
        cerr << "Usage: " << argv[0] << " magnet_handshake <magnet-link>" << endl;
        return {};
    }
    string magnet_link = argv[2];
    return magnet_handshake(magnet_link);
}

json get_magnet_metadata(PeerInfo &peer) {
    string payload = "d8:msg_typei0e5:piecei0ee";
    uint32_t len = payload.length() + 2;
    len = htonl(len);
    write(peer.sock_fd, &len, 4);
    write(peer.sock_fd, (char[]){20}, 1);
    write(peer.sock_fd, &peer.ut_metadata, 1);
    write(peer.sock_fd, payload.c_str(), payload.length());
    char response[1024];
    read(peer.sock_fd, &len, 4);
    len = ntohl(len);
    read(peer.sock_fd, response, 2);
    len -= 2;
    string bufstr("", len);
    size_t bytes_read = read(peer.sock_fd, bufstr.data(), len);
    int offset = 0, offset2=0;
    auto decoded_value = decode_bencoded_value(bufstr, offset);
    auto decoded_value2 = decode_bencoded_value(bufstr.substr(offset), offset2);
    return decoded_value2;
}

vector<PeerInfo> magnet_get_peers(string &magnet_link){
    auto [peers_str, bytes_info_hash] = magnet_ip_and_hash(magnet_link);
    vector<string> peers;
    for(auto peer : view::split(peers_str, '\n')) peer.push_back({peer.begin(), peer.end()});
    vector<PeerInfo> sockets;
    sockets.reserve(peers.size());
    vector<thread> threads;
    threads.reserve(peers.size());
    for(auto &peer : peers){
        threads.emplace_back([&](){
            sockets.push_back(handshake(peer, bytes_info_hash, true));
        })
    }
    for(auto &thread : threads) thread.join();
    return sockets;
}

int main(int argc, char* argv[]) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
        return 1;
    }  

    std::string command = argv[1];

    if (command == "decode") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }
        std::cerr << "Logs from your program will appear here!" << std::endl;
        int offset = 0;
        string encoded_value = argv[2];
        json decoded_value = decode_bencoded_value(encoded_value, offset);
        cout << decoded_value.dump() << std::endl;
    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
            return 1;
        }
        string file_name = argv[2];
        auto buffer = process_torrent_file(file_name);
        int offset = 0;
        json decoded_value =  decode_bencoded_value(buffer, offset);
        cout << "Tracker URL: " << decoded_value["announce"].get<string>()<<'\n';
        cout << "Length: " << decoded_value["info"]["length"]<<'\n';
        int info_idx = buffer.find("4:info") + strlen("4:info");
        auto info_coded = buffer.substr(info_idx, buffer.size() - info_idx - 1);
        cout << "Info Hash: " << sha1(info_coded) << '\n';
        cout << "Piece Length: " << decoded_value["info"]["piece length"]<<'\n';
        string pieces_str = decoded_value["info"]["pieces"].get<string>();
        cout<<"Pieces Hashes: \n";
        int i=0;
        for(uint8_t byte : pieces_str){
            printf("%02x", byte);
            i++;
            if(i%20 == 0) cout<<'\n';
        }
        cout<<'\n';
    }else if(command == "peers"){
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " peers <file>" << std::endl;
            return 1;
        }
        string file_name = argv[2];
        auto buffer = process_torrent_file(file_name);
        auto out = get_peers(buffer);
        cout << out << '\n';
    }else if(command == "handshake"){
        if (argc < 3) {
            cerr << "Usage: " << argv[0] << " handshake sample.torrent <peer_ip>:<peer:port>" <<endl;
            return 1;
        }
        string file_name = argv[2];
        string peer = argv[3];
        auto buffer = process_torrent_file(file_name);
        int offset = 0;
        json decoded_value = decode_bencoded_value(buffer, offset);
        auto info_hash_bytes = get_info_hash_bytes(buffer);
        handshake(peer, info_hash_bytes);
    }else if(command == "download_piece"){
        if (argc < 6) {
            cerr << "Usage: " << argv[0] << " download_piece -o out_file sample.torrent <piece_index>" << endl;
            return 1;
        }
        string out_file = argv[3];
        string file_name = argv[4];
        int piece_index = atoi(argv[5]);
        cerr << "out_file: " << out_file <<'\n';
        cerr << "file_name: " << file_name <<'\n';
        cerr << "piece_index: " << piece_index <<'\n';
        auto buffer = process_torrent_file(file_name);
        int offset = 0;
        json decoded_value = decode_bencoded_value(buffer, offset);
        size_t standard_piece_len = decoded_value["info"]["piece length"];
        size_t total_size = decoded_value["info"]["length"];
        size_t piece_count = total_size / standard_piece_len;
        size_t used_len = piece_count * standard_piece_len;
        size_t piece_size = (piece_index < piece_count) ? standard_piece_len : max(total_size - used_len, (size_t)0);
        uint8_t *piece_buffer = (uint8_t *)malloc(piece_size);
        while(true){
            vector<int> sockets = get_peers_connections(buffer);
            for(auto socket : sockets){
                if(download_piece(socket, piece_buffer, piece_size, piece_index) != -1){
                    ofstream file = ofstream(out_file, std::ios::binary);
                    if(file){
                        file.write((const char *)piece_buffer, piece_size);
                        file.close();
                    }
                    return 0;   
                }
            }
        }
    } else if (command == "download") {
        if(argc < 5) {
            cerr << "Usage: " << argv[0] << "download -o out_file sample.torrent" << endl;
            return 1;
        }
        string out_file = argv[3];
        string file_name = argv[4];
        auto buffer = process_torrent_file(file_name);
        int offset = 0;
        json decoded_value = decode_bencoded_value(buffer, offset);
        size_t standard_piece_size = decoded_value["info"]["piece length"];
        size_t total_size = decoded_value["info"]["length"];
        int piece_count = (total_size) / standard_piece_size;
        size_t used_len = piece_count * standard_piece_size;
        uint8_t *file_buffer = (uint8_t *)malloc(total_size);
        cout<<"piece_count : " << piece_count << '\n';
        unordered_set<int> pieces;
        for(int i=0; i<=piece_count; i++) pieces.insert(i);
        while(!pieces.empty()){
            vector<int> sockets = get_peers_connections(buffer);
            vector<future<int>> futures;
            int tasks = min(sockets.size(), pieces.size());
            auto it = pieces.begin();
            for(int i=0; i<tasks; i++, it++){
                int sock_fd = sockets[i];
                int piece_index = *it;
                size_t piece_size = (piece_index < piece_count) ? standard_piece_size : (total_size > used_len ? total_size - used_len : 0);
                uint8_t *piece_buffer = file_buffer + (piece_index * standard_piece_size);
                futures.push_back(async(launch::async, download_piece, sock_fd, piece_buffer, piece_size, piece_index, false));
            }
            for(auto &f : futures){
                auto res = f.get();
                if(res == -1) continue;
                cout << "Download piece_index: " << res << '\n';
                pieces.erase(res);
            }
        }
        ofstream file = ofstream(out_file, ios::binary);
        if(file) {
            file.write((const char *)file_buffer, total_size);
            file.close();
        }
    }else if (command == "magnet_parse") {
        if(argc < 3) {
            cerr << "Usage: " << argv[0] << " magnet_parse <magnet-ling>" << endl;
            return 1;
        }
        string magnet_link = argv[2];
        auto pos = magnet_link.find("xt=urn:btih:") + strlen("xt=urn:btih:");
        string info_hash = magnet_link.substr(pos, 40);
        pos = magnet_link.find("tr=") + strlen("tr=");
        string url = magnet_link.substr(pos, magnet_link.length() - pos);
        cout << "Info Hash: "<< info_hash<<'\n';
        cout << "Tracker URL: " << url_decode(url) << '\n';
    }else if (command  == "magnet_handshake") {
        magnet_handshake(argc, argv);
    }else if (command == "magnet_info") {
        PeerInfo peer = magnet_handshake(argc, argv);
        auto metadata = get_magnet_metadata(peer);
        int length = metadata["length"];
        int piece_length = metadata["piece length"];
        cout << "Length: " << length << '\n';
        cout << "Piece Length: " << piece_length << '\n';
        string pieces_hash_bytes = metadata["pieces"].get<string>();
        string pieces_hash = bytes_to_hex((uint8_t *)pieces_hash_bytes.c_str(), pieces_hash_bytes.length());
        for(int i=0; i<pieces_hash.length(); i+=40){
            cout << pieces_hash.substr(i, 40) << '\n';
        }
    }else if (command = "magent_download_piece") {
        if(argc < 6) {
            cerr << "Usage: " << argv[0] << " magnet_download_piece -o out_file <magnet-link> <piece_index>" << endl;
            return 1;
        }
        string out_file = argv[3];
        string magnet_link = argv[4];
        int piece_index = atoi(argv[5]);
        uint8_t *piece_buffer = 0;
        size_t piece_size = 0;
        while(true) {
            vector<PeerInfo> peers = magnet_get_peers(magnet_link);
            if(piece_buffer == 0) {
                auto metadata = get_magnet_metadata(peers[0]);
                int total_size = metadata["length"];
                int std_piece_len = metadata["piece length"];
                size_t piece_count = total_size / std_piece_len;
                size_t used_len = piece_count * std_piece_len;
                piece_size = (piece_index < piece_count) ? std_piece_len : (total_size > used_len ? total_size - used_len : 0);
                piece_buffer = (uint8_t *)malloc(piece_size);
            }
            for(auto [socket,] : peers){
                if(download_piece(socket, piece_buffer, piece_size, piece_index,true) != -1){
                    ofstream file = ofstream(out_file, ios::binary);
                    if(file) {
                        file.write((const char *)piece_buffer, piece_size);
                        file.close();
                        return 0;
                    }
                }
            }
        }
    }else {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }
    return 0;
}
