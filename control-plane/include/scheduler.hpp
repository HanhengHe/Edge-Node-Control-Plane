#pragma once

namespace proxy_scheduler
{
    class Scheduler : public std::enable_shared_from_this<Scheduler>
    {
    public:
        Scheduler(
            boost::asio::io_context &io);

    private:
    };
}