#pragma once

#include <boost/asio.hpp>
#include <boost/thread/tss.hpp>
#include <deque>
#include <functional>
#include <chrono>
#include <thread>

#include "logging.h"

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
                static boost::thread_specific_ptr<dumb_timer_queue> q;
                if ( !q.get() )
                {
                    q.reset(new dumb_timer_queue);
                }
                return *q;
            }

            using key = std::pair<dumb_timer_queue*, int>;

            void cancel(key& k)
            {
                auto self = k.first;
                k.first = nullptr;
                if (!self)
                    return;

                unsigned int index = (unsigned int)(k.second - self->step_);
                if (index < self->dq_.size())
                    self->dq_[index].second = nullptr;
            }

            key add(std::function<void()> f)
            {
                dq_.emplace_back(std::chrono::steady_clock::now(), std::move(f));
                int ret = step_+dq_.size()-1;

                CROW_LOG_DEBUG << "timer add inside: " << this << ' ' << ret ;
                return {this, ret};
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
                        CROW_LOG_DEBUG << "timer call: " << this << ' ' << step_;
                        // we know that timer handlers are very simple currenty; call here
                        x.second();
                    }
                    dq_.pop_front();
                    step_++;
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
            int step_{};
        };
    }
}
