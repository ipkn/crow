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

            using key = std::pair<dumb_timer_queue*, int>;

            void cancel(key& k)
            {
                auto self = k.first;
                k.first = nullptr;
                if (!self)
                    return;
                self->mutex_.lock();
                unsigned int index = (unsigned int)(k.second - self->step_);
                if (index < self->dq_.size())
                    self->dq_[index].second = nullptr;
                self->mutex_.unlock();
            }

            key add(std::function<void()> f)
            {
                mutex_.lock();
                dq_.emplace_back(std::chrono::steady_clock::now(), std::move(f));
                int ret = step_+dq_.size()-1;
                mutex_.unlock();
                CROW_LOG_DEBUG << "timer add inside: " << this << ' ' << ret ;
                return {this, ret};
            }

            void process()
            {
                if (!io_service_)
                    return;
                mutex_.lock();
                auto now = std::chrono::steady_clock::now();
                while(!dq_.empty())
                {
                    auto& x = dq_.front();
                    if (now - x.first < std::chrono::seconds(tick))
                        break;
                    if (x.second)
                    {
                        CROW_LOG_DEBUG << "timer call: " << this << ' ' << step_;
                        //io_service_->post(std::move(x.second));
                        x.second();
                    }
                    dq_.pop_front();
                    step_++;
                }
                mutex_.unlock();
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
            std::mutex mutex_;
            int step_{};
        };
    }
}
