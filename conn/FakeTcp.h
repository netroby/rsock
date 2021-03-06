//
// Created by System Administrator on 1/16/18.
//

#ifndef RSOCK_FAKETCP_H
#define RSOCK_FAKETCP_H


#include "INetConn.h"
#include "../bean/TcpInfo.h"

class FakeTcp : public INetConn {
public:
    FakeTcp(uv_stream_t *tcp, const std::string &key);

    int Init() override;

    int Output(ssize_t nread, const rbuf_t &rbuf) override;

    int OnRecv(ssize_t nread, const rbuf_t &buf) override;

    void Close() override;

    ConnInfo *GetInfo() override;

    bool Alive() override;

    void SetISN(uint32_t isn);

    void SetAckISN(uint32_t isn);

    bool IsUdp() override;

private:
    void onTcpError(FakeTcp *conn, int err);

    static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

private:
    uv_stream_t *mTcp = nullptr;
    TcpInfo mInfo;
    bool mAlive = true;
};

#endif //RSOCK_FAKETCP_H
