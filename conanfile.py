from conans import ConanFile, CMake

class CrowConan(ConanFile):
    name = "Crow"
    version = "0.1"
    url = "https://github.com/javierjeronimo/crow"
    license = "see https://github.com/ipkn/crow/blob/master/LICENSE"
    settings = "os", "compiler", "build_type", "arch"
    # No exports necessary

    def source(self):
        # this will create a hello subfolder, take it into account
        self.run("git clone https://github.com/javierjeronimo/crow.git")

    def build(self):
        cmake = CMake(self.settings)
	self.run('cmake %s/crow %s' % (self.conanfile_directory, cmake.command_line))
	self.run("cmake --build . %s" % cmake.build_config)
	self.run("make")

    def package(self):
        self.copy("*.h", dst="include", src="amalgamate")

    def package_info(self):
        self.cpp_info.libs = ["crow"]

