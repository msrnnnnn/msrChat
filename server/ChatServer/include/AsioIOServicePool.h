#pragma once
#include "Singleton.h"
#include <atomic>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

class AsioIOServicePool : public Singleton<AsioIOServicePool>
{
    friend Singleton<AsioIOServicePool>;

public:
    using IOService = boost::asio::io_context;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool &) = delete;
    AsioIOServicePool &operator=(const AsioIOServicePool &) = delete;

    std::shared_ptr<IOService> GetIOService();
    void Stop();

private:
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());

    std::vector<std::shared_ptr<IOService>> ioServices_;
    std::vector<WorkGuard> works_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> nextIOService_;
};
