#include "AsioIOServicePool.h"
#include <iostream>

AsioIOServicePool::AsioIOServicePool(std::size_t size) : nextIOService_(0) {
    if (size == 0) size = 2;
    for (std::size_t i = 0; i < size; ++i) {
        ioServices_.emplace_back(std::make_shared<IOService>());
        works_.emplace_back(boost::asio::make_work_guard(ioServices_[i]->get_executor()));
        threads_.emplace_back([this, i]() { ioServices_[i]->run(); });
    }
}

AsioIOServicePool::~AsioIOServicePool() { Stop(); }

boost::asio::io_context& AsioIOServicePool::GetIOService() {
    std::size_t index = nextIOService_.fetch_add(1, std::memory_order_relaxed);
    return *(ioServices_[index % ioServices_.size()]);
}

void AsioIOServicePool::Stop() {
    for (auto& work : works_) work.reset();
    for (auto& service : ioServices_) service->stop();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}