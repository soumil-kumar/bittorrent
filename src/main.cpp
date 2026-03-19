#include<bits/stdc++.h>
#include "lib/nlohmann/json.hpp"
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
    file.read(&buffer[0], file_size);
    file.close();
    return buffer;
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
        std::string encoded_value = argv[2];
        json decoded_value = decode_bencoded_value(encoded_value, offset);
        std::cout << decoded_value.dump() << std::endl;
    } else if (command == "info") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
            return 1;
        }
        std:: string file_name = argv[2];
        auto buffer = process_torrent_file(file_name);
        int offset = 0;
        json decoded_value =  decode_bencoded_value(buffer, offset);
        std::cout << "Tracker URL: " << decoded_value["announce"].get<string>()<<'\n';
        std::cout << "Length: " << decoded_value["info"]["length"]<<'\n';

    }else {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
