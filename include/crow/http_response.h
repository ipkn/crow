#pragma once
#include <string>
#include <unordered_map>
#include "crow/json.h"
#include "crow/http_request.h"
#include "crow/any_types.h"
#include "crow/ci_map.h"
//response
namespace crow {
  template <typename Adaptor,typename Handler,typename ... Middlewares>
  class Connection;
  struct Res {
	template <typename Adaptor,typename Handler,typename ... Middlewares>
	friend class crow::Connection;

	int code{200};
	std::string body;
	json::value json_value;

	// `headers' stores HTTP headers.
	ci_map headers;
#ifdef CROW_ENABLE_COMPRESSION
	bool compressed=true; //< If compression is enabled and this is false, the individual response will not be compressed.
#endif
	bool is_head_response=false; //< Whether this is a response to a HEAD request.
	bool manual_length_header=false; //< Whether Crow should automatically add a "Content-Length" header.

	void set_header(std::string key,std::string value) {
	  headers.erase(key);
	  headers.emplace(std::move(key),std::move(value));
	}
	void add_header(std::string key,std::string value) {
	  headers.emplace(std::move(key),std::move(value));
	}

	const std::string& get_header_value(const std::string& key) {
	  return crow::get_header_value(headers,key);
	}


	Res() {}
	explicit Res(int code): code(code) {}
	Res(std::string body): body(std::move(body)) {}
	Res(json::value&& json_value): json_value(std::move(json_value)) {
	  set_header("Content-Type","application/json");
	}
	Res(int code,std::string body): code(code),body(std::move(body)) {}
	Res(const json::value& json_value): body(json::dump(json_value)) {
	  set_header("Content-Type","application/json");
	}
	Res(int code,const json::value& json_value): code(code),body(json::dump(json_value)) {
	  set_header("Content-Type","application/json");
	}

	Res(Res&& r) {
	  *this=std::move(r);
	}

	Res& operator = (const Res& r)=delete;

	Res& operator = (Res&& r) noexcept {
	  body=std::move(r.body);
	  json_value=std::move(r.json_value);
	  code=r.code;
	  headers=std::move(r.headers);
	  completed_=r.completed_;
	  return *this;
	}

	bool is_completed() const noexcept {
	  return completed_;
	}

	void clear() {
	  body.clear();
	  json_value.clear();
	  code=200;
	  headers.clear();
	  completed_=false;
	  file_info=static_file_info{};
	}

	void redirect(const std::string& location) {
	  code=301;
	  set_header("Location",location);
	}

	void write(const std::string& body_part) {
	  body+=body_part;
	}

	void end() {
	  if (!completed_) {
		completed_=true;
		if (is_head_response) {
		  set_header("Content-Length",std::to_string(body.size()));
		  body="";
		  manual_length_header=true;
		}
		if (complete_request_handler_) {
		  complete_request_handler_();
		}
	  }
	}

	void end(const std::string& body_part) {
	  body+=body_part;
	  end();
	}

	bool is_alive() {
	  return is_alive_helper_&&is_alive_helper_();
	}

	/// Check whether the response has a static file defined.
	bool is_static_type() {
	  return file_info.path.size();
	}

	struct static_file_info {
	  std::string path="";
	  struct stat statbuf;
	  int statResult;
	};

	///Return a static file as the response body
	void set_static_file_info(std::string path) {
	  file_info.path=path;
	  file_info.statResult=stat(file_info.path.c_str(),&file_info.statbuf);
#ifdef CROW_ENABLE_COMPRESSION
	  compressed=false;
#endif
	  if (file_info.statResult==0) {
		code=200;
		std::size_t last_dot=path.find_last_of(".");
		std::string extension=path.substr(last_dot+1);
		this->add_header("Content-length",std::to_string(file_info.statbuf.st_size));
		if (extension!="")
		  this->add_header("Content-Type",any_types[extension]);
	  } else {
		code=404;this->end();
	  }
	}
	/// Stream a static file.
	template<typename Adaptor>
	void do_stream_file(Adaptor& adaptor) {
	  if (file_info.statResult==0) {
		std::ifstream is(file_info.path.c_str(),std::ios::in|std::ios::binary);
		write_streamed(is,adaptor);
	  }
	}

	/// Stream the response body (send the body in chunks).
	template<typename Adaptor>
	void do_stream_body(Adaptor& adaptor) {
	  if (body.length()>0) {
		write_streamed_string(body,adaptor);
	  }
	}

	private:
	bool completed_{};
	static_file_info file_info;
	std::function<void()> complete_request_handler_;
	std::function<bool()> is_alive_helper_;
	template<typename Stream,typename Adaptor>
	void write_streamed(Stream& is,Adaptor& adaptor) {
	  char buf[16384];
	  while (is.read(buf,sizeof(buf)).gcount()>0) {
		std::vector<asio::const_buffer> buffers;
		buffers.push_back(boost::asio::buffer(buf));
		write_buffer_list(buffers,adaptor);
	  }
	}

	//THIS METHOD DOES MODIFY THE BODY, AS IN IT EMPTIES IT
	template<typename Adaptor>
	void write_streamed_string(std::string& is,Adaptor& adaptor) {
	  std::string buf;
	  std::vector<asio::const_buffer> buffers;

	  while (is.length()>16384) {
		//buf.reserve(16385);
		buf=is.substr(0,16384);
		is=is.substr(16384);
		push_and_write(buffers,buf,adaptor);
	  }
	  //Collect whatever is left (less than 16KB) and send it down the socket
	  //buf.reserve(is.length());
	  buf=is;
	  is.clear();
	  push_and_write(buffers,buf,adaptor);
	}

	template<typename Adaptor>
	inline void push_and_write(std::vector<asio::const_buffer>& buffers,std::string& buf,Adaptor& adaptor) {
	  buffers.clear();
	  buffers.push_back(boost::asio::buffer(buf));
	  write_buffer_list(buffers,adaptor);
	}

	template<typename Adaptor>
	inline void write_buffer_list(std::vector<asio::const_buffer>& buffers,Adaptor& adaptor) {
	  boost::asio::write(adaptor.socket(),buffers,[this](std::error_code ec,std::size_t) {
		if (!ec) {
		  return false;
		} else {
		  CROW_LOG_ERROR<<ec<<" - happened while sending buffers";
		  this->end();
		  return true;
		}
	  });
	}
  };
}
