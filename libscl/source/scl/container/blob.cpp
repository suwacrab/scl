#include <cstdio>

#include <scl/container/blob.hpp>

namespace scl {

auto Blob::file_load(const std::string& filename, bool strict) -> bool {
	auto file = std::fopen(filename.c_str(),"rb");
	if(!file) {
		if(strict) {
			std::printf("Blob::file_load(): error: unable to load file %s\n",
				filename.c_str()
			);
			std::terminate();
		}
		return false;
	}
	// get file size ----------------------------@/
	std::fseek(file,0,SEEK_END);
	const std::size_t fsize = std::ftell(file);
	std::rewind(file);

	// write to Blob ----------------------------@/
	std::vector<uint8_t> buffer(fsize);
	std::fread(buffer.data(),1,buffer.size(),file);
	std::fclose(file);
	mData.insert(mData.end(),buffer.begin(),buffer.end());
	return true;
}
auto Blob::file_send(const std::string& filename, bool strict) const -> bool {
	auto file = std::fopen(filename.c_str(),"wb");
	if(!file) {
		if(strict) {
			std::printf(
				"Blob::file_send(): error: unable to send to file %s\n",
				filename.c_str()
			);
			std::terminate();
		}
		return false;
	}
	std::fwrite(mData.data(),sizeof(char),size(),file);
	std::fclose(file);
	return true;
}

} // namespace scl

