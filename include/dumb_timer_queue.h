#pragma once

#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <chrono>
#include <thread>

namespace crow
{
    namespace detail 
    {
        // fast timer queue for fixed tick value.
        class dumb_timer_queue
        {
        public:
            // tls based queue to avoid locking
            static dumb_timer_queue& get_current_dumb_timer_queue()
            {
                thread_local dumb_timer_queue q;
                return q;
            }

            void add(std::function<void()> f)
            {
                dq_.emplace_back(std::chrono::steady_clock::now(), std::move(f));
            }

            void process()
            {
                if (!io_service_)
                    return;
                auto now = std::chrono::steady_clock::now();
                while(!dq_.empty())
                {
                    auto& x = dq_.front();
                    if (now - x.first < std::chrono::seconds(tick))
                        break;
                    if (x.second)
                    {
                        io_service_->post(std::move(x.second));
                    }
                    dq_.pop_front();
                }
            }

            void set_io_service(boost::asio::io_service& io_service)
            {
                io_service_ = &io_service;
            }

        private:
            dumb_timer_queue() noexcept
            {
            }

            int tick{5};
            boost::asio::io_service* io_service_{};
            std::deque<std::pair<decltype(std::chrono::steady_clock::now()), std::function<void()>>> dq_;
        };
    }
}
