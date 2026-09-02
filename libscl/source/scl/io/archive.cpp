#define SCL_USE_ZLIB
#include <scl/basis/errhandle.hpp>
#include <scl/container/blob.hpp>
#include <scl/io/archive.hpp>

#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <ranges>
#include <filesystem>

#include <clocale>

namespace stdfs = std::filesystem;

namespace scl {
namespace io {
namespace archive {

// ==========================================================================@/
// metadata's folder                                                         @/
// ==========================================================================@/
auto MetadataFolder::add_file(MetadataFile file) -> void {
	file.mParentID = mID;
	mFiles.push_back(file);
}
auto MetadataFolder::add_folder(MetadataFolder fldr) -> void {
	fldr.mParentID = mID;
	mFolders.push_back(fldr);
}

// ==========================================================================@/
// record                                                                    @/
// ==========================================================================@/
RecordFile::RecordFile(Record* record, const std::string& filename) {
	mRecordCurrent = record;
}

auto RecordInfo_Folder::add_file(const RecordInfo_File& file) -> void {
	SCL_ASSERT_MSG(!mReadonly,"folder %p: tried to add file in readonly state",this);
	mTableFile.push_back(file);
}
auto RecordInfo_Folder::add_folder(const RecordInfo_Folder& folder) -> void {
	SCL_ASSERT_MSG(!mReadonly,"folder %p: tried to add folder in readonly state",this);
	mTableFolder.push_back(folder);
}
auto RecordInfo_Folder::mark_readonly() -> void {
	mReadonly = true;
	for(auto& infofolder : mTableFolder) {
		infofolder.mark_readonly();
	}
}

auto Record::file_open(const std::string& filename) -> RecordFile {
	return RecordFile(this,filename);
}

auto Record::load_file(const std::string& src_filename, bool strict) -> void {
	auto file = std::fopen(src_filename.c_str(),"rb");
	SCL_ASSERT_MSG(file,"record %p: unable to open file %s",this, src_filename.c_str());

	std::vector<std::size_t> IDtable_file;
	std::vector<std::size_t> IDtable_folder;

	// read the header ----------------------------------@/
	SARFile_Header header = {};
	std::fread(&header,1,sizeof(header),file);

	auto read_str = [&](size_t idx,size_t len) {
		std::string str;
		std::fseek(file, header.offset_segString + idx + 4, SEEK_SET);
		for(size_t i=0; i<len; i++) {
			char c = 0;
			std::fread(&c,1,sizeof(c),file);
			str += c;
		}
		return str;
	};

	// read the two ID tables ---------------------------@/
	std::fseek(file,header.offset_segTblFileID + 4,SEEK_SET);
	for(size_t i=0; i<header.num_files; i++) {
		uint32_t ID = 0;
		std::fread(&ID,1,sizeof(ID),file);
		IDtable_file.push_back(ID);
	}

	std::fseek(file,header.offset_segTblFolderID + 4,SEEK_SET);
	for(size_t i=0; i<header.num_folders; i++) {
		uint32_t ID = 0;
		std::fread(&ID,1,sizeof(ID),file);
		IDtable_folder.push_back(ID);
	}

	// read all the folders -----------------------------@/
	std::function<RecordInfo_Folder(size_t)> iter_folders = [&](size_t cur_folderID) {
		std::fseek(file,header.offset_segTblFolder + (cur_folderID*sizeof(SARFile_EntryFolder)) + 4,SEEK_SET);
		SARFile_EntryFolder folderentry = {};
		std::fread(&folderentry,1,sizeof(folderentry),file);

		// create new folder ----------------------------@/
		auto subfolder_name = read_str(folderentry.name_idx,folderentry.name_len);
		RecordInfo_Folder subfolder(cur_folderID,subfolder_name);
		subfolder.mParentID = folderentry.parentID;
		
		for(size_t i=0; i<folderentry.num_files; i++) {
			size_t fileID = IDtable_file.at(folderentry.childID_file + i);
			SARFile_EntryFile fileentry = {};
			std::fseek(file,header.offset_segTblFile + (fileID*sizeof(SARFile_EntryFile)) + 4,SEEK_SET);
			std::fread(&fileentry,1,sizeof(fileentry),file);

			auto infofile_name = read_str(fileentry.name_idx,fileentry.name_len);
			RecordInfo_File infofile(fileID,infofile_name);
			infofile.mDataIdx = fileentry.data_idx;
			infofile.mDataLen = fileentry.data_len;

			subfolder.add_file(infofile);
		}

		for(size_t i=0; i<folderentry.num_folders; i++) {
			size_t folderID = IDtable_folder.at(folderentry.childID_folder + i);
			auto infofolder = iter_folders(folderID);
			subfolder.add_folder(infofolder);
		}

		return subfolder;
	};
	mFolderRoot = iter_folders(FolderID::Root);

	std::function<void(RecordInfo_Folder&,int)> iter_view = [&](RecordInfo_Folder& cur_folder, int depth) {
		// create new folder ----------------------------@/
		const std::string fillerchar = "\t";
		for(int i=0; i<depth; i++) {
			std::cout << fillerchar;
		}

		std::cout << std::format("{0}/\n",cur_folder.name());
		for(auto& infofile : cur_folder.mTableFile) {
			for(int i=0; i<depth+1; i++) {
				std::cout << fillerchar;
			}
			std::cout << std::format("* {0:8.4f}MB {1}\n",
				infofile.data_lenMB(),infofile.name()
			);
		}	

		for(auto& folder : cur_folder.mTableFolder) {
			iter_view(folder,depth+1);
		}
	};
	iter_view(mFolderRoot,0);

	std::fclose(file);
}
auto Record::from_file(const std::string& filename, bool strict) -> std::shared_ptr<Record> {
	auto rec = std::make_shared<Record>();
	rec->load_file(filename,strict);
	return rec;
}

// ==========================================================================@/
// misc fns                                                                  @/
// ==========================================================================@/
static scl::blob create(MetadataFolder& metafolder_root) {
	std::vector<std::vector<std::size_t>> IDtables_folder;
	std::vector<std::vector<std::size_t>> IDtables_file;

	size_t total_numFiles = 0;
	size_t total_numFolders = 0;

	// get number of files & folders --------------------@/
	std::function<void(MetadataFolder&)> iter_getNums = [&](MetadataFolder& cur_folder) {
		total_numFolders++;

		total_numFiles += cur_folder.mFiles.size();
		for(auto& folder : cur_folder.mFolders) {
			iter_getNums(folder);
		}
	};
	iter_getNums(metafolder_root);

	for(size_t i=0; i<total_numFolders; i++) {
		std::vector<std::size_t> idtable;
		IDtables_folder.push_back(idtable);
	}
	for(size_t i=0; i<total_numFiles; i++) {
		std::vector<std::size_t> idtable;
		IDtables_file.push_back(idtable);
	}
	
	// assign IDs to each file and folder ---------------@/
	size_t baseID_file = 0;
	size_t baseID_folder = 0;

	std::function<void(MetadataFolder&)> iter_setIDs = [&](MetadataFolder& cur_folder) {
		cur_folder.mID = baseID_folder++;
		auto& cur_IDtableFolder = IDtables_folder.at(cur_folder.mID);
		auto& cur_IDtableFile = IDtables_file.at(cur_folder.mID);

		for(auto& file : cur_folder.mFiles) {
			file.mID = baseID_file++;
			file.mParentID = cur_folder.ID();
			cur_IDtableFile.push_back(file.mID);
		}
		for(auto& folder : cur_folder.mFolders) {
			iter_setIDs(folder);
			folder.mParentID = cur_folder.ID();
			cur_IDtableFolder.push_back(folder.ID());
		}
	};
	iter_setIDs(metafolder_root);

	// begin writing actual file tables -----------------@/
	scl::blob blob_segHeader;
	scl::blob blob_segTblFolder;
	scl::blob blob_segTblFile;
	scl::blob blob_segTblFolderID;
	scl::blob blob_segTblFileID;
	scl::blob blob_segString;
	scl::blob blob_segFiledata;

	std::function<void(MetadataFolder&)> iter_final = [&](MetadataFolder& cur_folder) {
		auto cur_IDtableFolder = IDtables_folder.at(cur_folder.mID);
		auto cur_IDtableFile = IDtables_file.at(cur_folder.mID);

		// write to folder table ------------------------@/
		SARFile_EntryFolder entry_folder = {};
		entry_folder.parentID = cur_folder.mParentID;
		entry_folder.childID_file = blob_segTblFileID.size() / sizeof(uint32_t);
		entry_folder.childID_folder = blob_segTblFolderID.size() / sizeof(uint32_t);
		entry_folder.num_files = cur_folder.mFiles.size();
		entry_folder.num_folders = cur_folder.mFolders.size();
		entry_folder.name_idx = blob_segString.size();
		entry_folder.name_len = cur_folder.name().size();
		blob_segTblFolder.write_raw(&entry_folder,sizeof(entry_folder));
		blob_segString.write_str(cur_folder.name());

		// write ID tables ------------------------------@/
		for(const auto ID : cur_IDtableFile) {
			blob_segTblFileID.write_u32(ID);
		}
		for(const auto ID : cur_IDtableFolder) {
			blob_segTblFolderID.write_u32(ID);
		}

		// iter through files ---------------------------@/
		for(const auto& metafile : cur_folder.mFiles) {
			// write to file table ----------------------@/
			SARFile_EntryFile entry_file = {};
			entry_file.name_idx = blob_segString.size();
			entry_file.name_len = metafile.name().size();
			entry_file.data_idx = blob_segFiledata.size();

			// write filedata ---------------------------@/
			auto source = metafile.source();
			scl::Blob sourceblob;
			sourceblob.file_load(source);
			sourceblob = sourceblob.compress();
			entry_file.data_len = sourceblob.size();

			blob_segTblFile.write_raw(&entry_file,sizeof(entry_file));
			blob_segFiledata.write_blob(sourceblob);
			blob_segString.write_str(metafile.name());
		}
		/*
		*/

		for(auto& metafolder : cur_folder.mFolders) {
			iter_final(metafolder);
		}
	};
	iter_final(metafolder_root);

	// add segment headers & padding --------------------@/
	const size_t pad_amount = 0x10;
	blob_segTblFolder = Blob::from_str("FL ").write_blob(blob_segTblFolder).pad(pad_amount);
	blob_segTblFile = Blob::from_str("FI ").write_blob(blob_segTblFile).pad(pad_amount);
	blob_segTblFolderID = Blob::from_str("FLI").write_blob(blob_segTblFolderID).pad(pad_amount);
	blob_segTblFileID = Blob::from_str("FII").write_blob(blob_segTblFileID).pad(pad_amount);
	blob_segString = Blob::from_str("STR").write_blob(blob_segString).pad(pad_amount);
	blob_segFiledata = Blob::from_str("DAT").write_blob(blob_segFiledata).pad(pad_amount);

	// create header ------------------------------------@/
	const size_t header_size = 0x40;
	size_t offset_segHeader = 0;
	size_t offset_segTblFolder   = offset_segHeader + header_size;
	size_t offset_segTblFile     = offset_segTblFolder + blob_segTblFolder.size();
	size_t offset_segTblFolderID = offset_segTblFile + blob_segTblFile.size();
	size_t offset_segTblFileID   = offset_segTblFolderID + blob_segTblFolderID.size();
	size_t offset_segString      = offset_segTblFileID + blob_segTblFileID.size();
	size_t offset_segFiledata    = offset_segString + blob_segString.size();

	SARFile_Header header = {};
	header.magic[0] = 'S';
	header.magic[1] = 'A';
	header.magic[2] = 'R';
	header.num_files = total_numFiles;
	header.num_folders = total_numFolders;
	header.offset_segTblFolder = offset_segTblFolder;
	header.offset_segTblFile = offset_segTblFile;
	header.offset_segTblFolderID = offset_segTblFolderID;
	header.offset_segTblFileID = offset_segTblFileID;
	header.offset_segString = offset_segString;
	header.offset_segFiledata = offset_segFiledata;

	blob_segHeader.write_raw(&header,sizeof(header));
	blob_segHeader.pad(header_size);

	// write to final blob ------------------------------@/
	scl::blob blob_all;
	
	blob_all.write_blob(blob_segHeader)
		.write_blob(blob_segTblFolder)
		.write_blob(blob_segTblFile)
		.write_blob(blob_segTblFolderID)
		.write_blob(blob_segTblFileID)
		.write_blob(blob_segString)
		.write_blob(blob_segFiledata);

	return blob_all;
}
scl::blob create_file(const std::string& src_filename) {
	MetadataFolder metafolder_root("_root");

	std::setlocale(LC_ALL, "en_us.utf8");

	auto basepath = stdfs::path(src_filename);
	std::printf("foldername: %s\n",basepath.string().c_str());

	std::function<void(const stdfs::directory_entry,MetadataFolder&)> check_entry = [&](const stdfs::directory_entry entry, MetadataFolder& parentfolder) {
		auto path = entry.path();
		if(entry.is_regular_file()) {
		//	auto proxim = stdfs::proximate(path,basepath);
		//	std::printf("loading file arc::/%s\n",proxim.string().c_str());
			
			// making new metadata file -----------------@/
			auto file = MetadataFile(path.filename().string(),path.string());
			parentfolder.add_file(file);
		} else if(entry.is_directory()) {
			auto fldr_name = path.stem().string();
			
			// making new metadata folder ---------------@/
			auto folder = MetadataFolder(fldr_name);
			for (const auto& e_recurs : stdfs::directory_iterator(entry.path())) {
				check_entry(e_recurs, folder);
			}
			parentfolder.add_folder(folder);
		}
	};

	for (const auto& entry : stdfs::directory_iterator(basepath)) {
		check_entry(entry,metafolder_root);
	}

	return create(metafolder_root);
}

}; // namespace archive
}; // namespace io
}; // namespace scl

