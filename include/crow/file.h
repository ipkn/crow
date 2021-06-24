#pragma once
#include <fstream>
#include <string>
#include <iterator>

namespace crow {
  namespace file {
	using namespace std;
	string read_all(const string& filename) {
	  ifstream is(filename);
	  return {istreambuf_iterator<char>(is), istreambuf_iterator<char>()};
	}
  }
}