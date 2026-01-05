#define SCL_USE_ZLIB
#include <scl/container/blob.hpp>
#include <ranges>
#include <filesystem>

namespace stdfs = std::filesystem;

namespace sample_sdzarc {
	static void fn_pack() {
		const std::string entry_names[] = {
			"entry A.",
			"entry B.",
			"entry C!",
			"lastly, entry D.",
			"though, is even an entry E safe to use? not sure.",
			"maybe even an entry F, if we feel like it.",
			"also, the hidden prototype entry X."
		};

		std::vector<scl::blob> filedata_table;
		std::vector<std::string> filename_table;
		auto basepath = stdfs::path("../../guiutil/kappamap/kmap/workdata/sprite/editor/chip");
		std::printf("foldername: %s\n",basepath.string().c_str());
		for (const auto & entry : stdfs::recursive_directory_iterator(basepath)) {
			if(entry.is_regular_file()) {
				auto path = entry.path().string();
				auto proxim = stdfs::proximate(entry.path(),basepath);
				scl::blob filedata;
				std::printf("loading file arc::%s\n",proxim.string().c_str());
				filedata.file_load(path);
				filedata_table.push_back(filedata);
				filename_table.push_back(proxim.string());
			}
		}

		constexpr size_t pad_size = 256;
		scl::blob blob_all;
		scl::blob blob_segHeader;
		scl::blob blob_segEntry;
		scl::blob blob_segName;
		scl::blob blob_segData;

		std::puts("writing blobs");
		for(auto const [index,data] : std::views::enumerate(filedata_table)) {
			blob_segEntry.write_u32(index);
			blob_segEntry.write_u32(data.size());
			blob_segEntry.write_u32(blob_segName.size());
			blob_segName.write_str(filename_table.at(index));
			blob_segData.write_blob(data);
		}
		std::puts("writing final");
		blob_segEntry.pad(pad_size);
		blob_segName.pad(pad_size);
		blob_segData.pad(pad_size);

		const std::size_t offset_segEntry = pad_size;
		const std::size_t offset_segName = offset_segEntry + blob_segEntry.size();
		const std::size_t offset_segData = offset_segName + blob_segName.size();
		blob_segHeader.write_str("SDA");
		blob_segHeader.write_u32(offset_segEntry);
		blob_segHeader.write_u32(offset_segName);
		blob_segHeader.write_u32(offset_segData);
		blob_segHeader.pad(pad_size);


		blob_all.write_blob(blob_segHeader)
			.write_blob(blob_segEntry)
			.write_blob(blob_segName)
			.write_blob(blob_segData);

		blob_all.file_send("workdata/out.bin");
		blob_all.compress_full().file_send("workdata/outPacked.bin");
	}
	static void fn_unpack() {
		scl::blob filedata;
		filedata.file_load("workdata/outPacked.bin");
		auto unpacked = filedata.decompress_full();
		unpacked.file_send("workdata/outUnpacked.bin");
	}
};

int main(int argc, const char* argv[]) {
	sample_sdzarc::fn_pack();
	sample_sdzarc::fn_unpack();
}

