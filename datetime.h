#pragma once

#include <string>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>

namespace crow
{
    // code from http://stackoverflow.com/questions/2838524/use-boost-date-time-to-parse-and-create-http-dates
    class DateTime
    {
        public:
            DateTime()
                : m_dt(boost::local_time::local_sec_clock::local_time(boost::local_time::time_zone_ptr()))
            {
            }
            DateTime(const std::string& path)
                : DateTime()
            {
                from_file(path);
            }

            // return datetime string
            std::string str()
            {
                static const std::locale locale_(std::locale::classic(), new boost::local_time::local_time_facet("%a, %d %b %Y %H:%M:%S GMT") );
                std::string result;
                try
                {
                    std::stringstream ss;
                    ss.imbue(locale_);
                    ss << m_dt;
                    result = ss.str();
                }
                catch (std::exception& e)
                {
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
                return result;
            }

            // update datetime from file mod date
            std::string from_file(const std::string& path)
            {
                try
                {
                    boost::filesystem::path p(path);
                    boost::posix_time::ptime pt = boost::posix_time::from_time_t(
                            boost::filesystem::last_write_time(p));
                    m_dt = boost::local_time::local_date_time(pt, boost::local_time::time_zone_ptr());
                }
                catch (std::exception& e)
                {
                    std::cout << "Exception: " << e.what() << std::endl;
                }
                return str();
            }

            // parse datetime string
            void parse(const std::string& dt)
            {
                static const std::locale locale_(std::locale::classic(), new boost::local_time::local_time_facet("%a, %d %b %Y %H:%M:%S GMT") );
                std::stringstream ss(dt);
                ss.imbue(locale_);
                ss >> m_dt;
            }

            // boolean equal operator
            friend bool operator==(const DateTime& left, const DateTime& right)
            {
                return (left.m_dt == right.m_dt);
            }

        private:
            boost::local_time::local_date_time m_dt;
    };
}
