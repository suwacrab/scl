#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>
#include <bit>
#include <exception>

namespace scl {

class blob {
	public:
		std::vector<uint8_t> m_data;

		auto write_blob(const blob& source) -> blob& {
			m_data.insert(m_data.end(),source.m_data.begin(),source.m_data.end());
			return *this;
		}
		auto write_raw(const void* source, size_t len) -> void {
			if(len == 0) return;
			if(source == nullptr) {
				std::puts("Blob::write_raw(): error: attempt to write from null source");
				std::terminate();
			}

			auto src_bytes = static_cast<const uint8_t*>(source);
			m_data.insert(m_data.end(),src_bytes,src_bytes + len);
		}
		constexpr auto write_u8(uint32_t n) -> void { m_data.push_back(n); }
		constexpr auto write_u16(uint32_t n) -> void { write_u8(n); write_u8(n>>8); }
		constexpr auto write_u32(uint32_t n) -> void { write_u16(n); write_u16(n>>16); }
		constexpr auto write_u64(uint64_t n) -> void { write_u32(n); write_u32(n>>32); }
		constexpr auto write_be_u16(uint16_t n) -> void { write_u16(std::byteswap(n)); }
		constexpr auto write_be_u32(uint32_t n) -> void { write_u32(std::byteswap(n)); }
		constexpr auto write_be_u64(uint64_t n) -> void { write_u64(std::byteswap(n)); }
		auto write_str(std::string_view str, bool no_terminator=false) -> void {
			for(size_t i=0; i<str.size(); i++) {
				char chr = str.at(i);
				write_u8(chr);
			}
			if(!no_terminator) {
				write_u8(0);
			}
		}

		auto pad(int divisor, int padbyte = 0xAB) -> void {
			while((size()%divisor) != 0) {
				write_u8(padbyte);
			}
		}
		auto clear() -> void {
			m_data.clear();
		}
		
		auto file_load(const std::string& filename, bool strict=true) -> bool {
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
			const size_t fsize = std::ftell(file);
			std::rewind(file);

			// write to blob ----------------------------@/
			std::vector<uint8_t> buffer(fsize);
			std::fread(buffer.data(),1,buffer.size(),file);
			std::fclose(file);
			m_data.insert(m_data.end(),buffer.begin(),buffer.end());
			return true;
		}
		auto file_send(const std::string& filename, bool strict=true) const -> bool {
			auto file = std::fopen(filename.c_str(),"wb");
			if(!file) {
				if(strict) {
					std::printf("Blob::file_send(): error: unable to send to file %s\n",
						filename.c_str()
					);
					std::terminate();
				}
				return false;
			}
			std::fwrite(m_data.data(),sizeof(char),size(),file);
			std::fclose(file);
			return true;
		}
		auto compress(const bool do_compress=true) -> blob;

		template<typename T=void*> auto data() -> T {
			return reinterpret_cast<T>(m_data.data());
		}
		constexpr auto size() const -> size_t { return m_data.size(); }
		constexpr auto at(const size_t idx) -> uint8_t& { return m_data.at(idx); }

		blob() {
			clear();
		}
		blob(const blob& orig) {
			m_data = orig.m_data;
		}
		blob(const std::vector<uint8_t>& data) {
			m_data = data;
		}
};

} // namespace scl


