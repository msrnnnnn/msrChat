/**
 * @file    VerifyGrpcClient.h
 * @brief   gRPC 验证服务客户端声明
 * @author  msr
 *
 * @details 封装了与 VerifyServer 通信的 gRPC 客户端，包含连接池实现。
 */

#pragma once

#include "Singleton.h"
#include "message.grpc.pb.h"
#include <atomic>
#include <condition_variable>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using message::GetVerifyRequest;
using message::GetVerifyResponse;
using message::VerifyService; // 注意这里修正了拼写 Varify -> Verify

/**
 * @class   RPConPool
 * @brief   gRPC 连接池
 *
 * @details 维护一组 gRPC Stub 连接，支持多线程安全地获取和归还连接。
 */
class RPConPool
{
public:
    RPConPool(size_t poolSize, std::string host, std::string port)
        : poolSize_(poolSize),
          host_(host),
          port_(port),
          b_stop_(false)
    {
        for (size_t i = 0; i < poolSize_; ++i)
        {
            // 创建 Channel
            std::shared_ptr<Channel> channel =
                grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());
            // 创建 Stub 并存入队列
            connections_.push(VerifyService::NewStub(channel));
        }
    }

    ~RPConPool()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
        while (!connections_.empty())
        {
            connections_.pop();
        }
    }

    std::unique_ptr<VerifyService::Stub> getConnection()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(
            lock,
            [this]
            {
                if (b_stop_)
                    return true;
                return !connections_.empty();
            });
        if (b_stop_)
            return nullptr;
        auto context = std::move(connections_.front());
        connections_.pop();
        return context;
    }

    void returnConnection(std::unique_ptr<VerifyService::Stub> context)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_)
            return;
        connections_.push(std::move(context));
        cond_.notify_one();
    }

    void Close()
    {
        b_stop_ = true;
        cond_.notify_all();
    }

private:
    std::atomic<bool> b_stop_;
    size_t poolSize_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<VerifyService::Stub>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

/**
 * @class   VerifyGrpcClient
 * @brief   gRPC 验证服务客户端 (Singleton)
 *
 * @details 提供 GetVerifyCode 接口，通过 gRPC 向 VerifyServer 请求验证码。
 */
class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
{
    friend class Singleton<VerifyGrpcClient>;

public:
    /**
     * @brief   获取验证码
     * @param   email 邮箱地址
     * @return  GetVerifyResponse gRPC 响应对象
     */
    GetVerifyResponse GetVerifyCode(std::string email);

private:
    VerifyGrpcClient();

    // 【新增】成员变量：连接池指针
    std::unique_ptr<RPConPool> pool_;
};
