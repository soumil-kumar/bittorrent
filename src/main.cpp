#include <bits/stdc++.h>
#include "lib/nlohmann/json.hpp"
#include "lib/sha1.hpp"
//#include "lib/curl/curl.h"
#include <curl/curl.h>

using namespace std;

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

string get_info_hash_bytes(string &buffer) {
    int info_idx = buffer.find("4:info") + strlen("4:info");
    auto info_coded = buffer.substr(info_idx, buffer.size() - info_idx - 1);
    auto hex_hash = sha1(info_coded);
    return hex_to_bytes(hex_hash);
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
        out += to_string((uint8_t)peers_bin[i] + '.');
        out += to_string((uint8_t)peers_bin[i + 1] + '.');
        out += to_string((uint8_t)peers_bin[i + 2] + '.');
        out += to_string((uint8_t)peers_bin[i + 3] + ':');     
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
        // You can use print statements as follows for debugging, they'll be visible when running tests.
        std::cerr << "Logs from your program will appear here!" << std::endl;
        int offset = 0;
        // TODO: Uncomment the code below to pass the first stage
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
            std::cerr << "Usage: " << argv[0] << " peers <file>" << std::endl;
            return 1;
        }
        string file_name = argv[2];
        auto buffer = process_torrent_file(file_name);
        auto out = get_peers(buffer);
        cout << out << '\n';
    }else {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
