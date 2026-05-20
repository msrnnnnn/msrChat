#ifndef TCP_PROTOCOL_PARSER_H
#define TCP_PROTOCOL_PARSER_H

#include "Global.h"
#include "ProtocolStructs.h"
#include <QByteArray>

class TcpMgr;

class TcpProtocolParser
{
public:
    explicit TcpProtocolParser(TcpMgr &tcpMgr) : _tcpMgr(tcpMgr) {}

    void parseLoginPacket(RequestType req_type, const QByteArray &data);
    void parseChatPacket(RequestType req_type, const QByteArray &data);

private:
    TcpMgr &_tcpMgr;
};

#endif
