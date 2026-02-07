#pragma once
#include <string>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>

// Constants for protocol
const int HEAD_TOTAL_LEN = 4;
const int HEAD_ID_LEN = 2;
const int HEAD_DATA_LEN = 2;
const int MAX_LENGTH = 1024 * 2; // 2KB max message length

// Message ID
const short MSG_CHAT_LOGIN = 1001; // Example ID

class RecvNode {
public:
    RecvNode(short max_len, short msg_id) : _total_len(max_len), _msg_id(msg_id), _cur_len(0) {
        _data = new char[_total_len + 1]();
        _data[_total_len] = '\0';
    }

    ~RecvNode() {
        delete[] _data;
    }

    void Clear() {
        ::memset(_data, 0, _total_len);
        _cur_len = 0;
    }

    short _msg_id;
    int _total_len;
    int _cur_len;
    char* _data;
};

class MsgNode {
public:
    MsgNode(short max_len, short msg_id) : _total_len(max_len), _msg_id(msg_id), _cur_len(0) {
        _data = new char[_total_len + 1]();
        _data[_total_len] = '\0';
    }
    
    MsgNode(const char* msg, short max_len, short msg_id) : _total_len(max_len), _msg_id(msg_id), _cur_len(max_len) {
        _data = new char[_total_len + 1]();
        memcpy(_data, msg, max_len);
        _data[_total_len] = '\0';
    }

    ~MsgNode() {
        delete[] _data;
    }

    void Clear() {
        ::memset(_data, 0, _total_len);
        _cur_len = 0;
    }

    short _msg_id;
    int _total_len;
    int _cur_len;
    char* _data;
};
