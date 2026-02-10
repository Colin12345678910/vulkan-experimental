#pragma once
#include <cstdint>
#include <vector>
#include <iostream>
#include <fstream>
/*
Author: Colin D.
Date: 2026-02-10

TBin:
A single header library for binary file storage in C++, it provides a simple builder pattern for writing binary file and will
return a structured object containing the file data with std::vector<char>, the library is designed for simplicity foremost.
*/
const uint32_t TBIN_MAGIC = 'TBin';
const uint32_t TBIN_VERSION = 1;

class TBin
{
	struct TBinHeader
	{
		uint32_t magic = TBIN_MAGIC;
		uint32_t version = TBIN_VERSION;
		uint32_t reserved = 0; // Reserved for future use, should be set to 0.
		uint32_t fileCount = 0;
	};

	struct TBinData
	{
		TBinHeader header;
		std::vector<uint32_t> fileOffsets; // Offsets to create a file table.
	};

	struct TBinFile
	{
		std::vector<char> data;

		TBinFile(const std::vector<char>& data) : data(data) 
		{ 
			this->data = data;
		}    
		TBinFile(void* data, size_t size) : data((char*)data, (char*)data + size) 
		{ 
			this->data = std::vector<char>((char*)data, (char*)data + size); 
		}

		TBinFile() {}
	};

	// Overloaded operators for serialization and deserialization.
	friend std::ostream& operator<<(std::ostream& os, const TBinData& header);
	friend std::ostream& operator<<(std::ostream& os, const TBinFile& file);

	friend std::istream& operator>>(std::istream& is, TBinData& header);
	friend std::istream& operator>>(std::istream& is, TBinFile& file);
public:
	class TBinBuilder
	{
	public:
		TBinBuilder& addFile(const std::vector<char>& data)
		{
			files.emplace_back(data);
			return *this;
		}
		TBinBuilder& addFile(void* data, size_t size)
		{
			files.emplace_back(data, size);
			return *this;
		}

		/*
		* Builds the binary file at the provided file path, automatically calculating all nessecary metadata.
		*/
		void build(std::string const filePath)
		{
			TBin::TBinData data; // Initialize header 

			//Calculate the offset caused by the header + file offsets
			data.header.fileCount = static_cast<uint32_t>(files.size());
			data.fileOffsets.reserve(files.size());
			uint32_t offset = sizeof(TBin::TBinHeader) + files.size() * sizeof(uint32_t);

			// Insert each file's offset into the header.
			for (const auto& file : files)
			{
				data.fileOffsets.push_back(offset);
				offset += static_cast<uint32_t>(file.data.size());
			}

			std::ofstream outFile(filePath, std::ios::binary);

			// Write the header and file data to the output file.
			outFile << data;

			//Write each file's data sequentially.
			for (const auto& file : files)
			{
				outFile << file;
			}

			outFile.close();
		}

	private:
		std::vector<TBin::TBinFile> files;
	};
	class TBinReader
	{
	public:
		TBinData data;
		std::vector<TBinFile> files;

		std::vector<std::vector<char>> GetFileData()
		{
			std::vector<std::vector<char>> fileData;
			for (const auto& file : files)
			{
				fileData.push_back(file.data);
			}
			return fileData;
		}

		// Helper function to calculate the size of each file based on the offsets and total file size.
		inline uint32_t GetFileSize(TBin::TBinData& data, size_t totalSize, size_t i)
		{
			uint32_t size = i < data.header.fileCount - 1 ? data.fileOffsets[i + 1] - data.fileOffsets[i] : totalSize - data.fileOffsets[i];
			return size;
		}

		/*
		* Attempts to load a TBin file from the provided file path, returns true if successful, from there
		* you can access the file data through the 'files' member, which is a vector of TBinFile, each containing a vector<char> with the file data.
		*/
		bool load(const std::string& filename)
		{
			// Attempt to open file.
			std::ifstream inFile(filename, std::ios::binary);
			if (!inFile)
				return false;

			// Read the total size of the file to calculate the size of each file later.
			uint32_t totalSize = inFile.seekg(0, std::ios::end).tellg();
			inFile.seekg(0, std::ios::beg);

			// Read the header and file offsets.
			inFile >> data;
			files.resize(data.header.fileCount);

			// Validate the header.
			if (data.header.magic != TBIN_MAGIC || data.header.version != TBIN_VERSION)
			{
				inFile.close();
				return false; // Invalid TBin file.
			}

			// Read each file from the TBin file, and store it in the 'files' member.
			for (size_t i = 0; i < data.header.fileCount; ++i)
			{
				uint32_t size = GetFileSize(data, totalSize, i);
				files[i].data.resize(size);
				inFile.seekg(data.fileOffsets[i], std::ios::beg);
				inFile.read(files[i].data.data(), size);
			}
			inFile.close();
			return true;
		}
	};
};


// Implementation of the operators for serialization and deserialization.

// 
inline std::ostream& operator<<(std::ostream& os, const TBin::TBinData& data)
{
	os.write((char*)&data.header.magic, sizeof(data.header.magic));
	os.write((char*)&data.header.version, sizeof(data.header.version));
	os.write((char*)&data.header.reserved, sizeof(data.header.reserved));
	os.write((char*)&data.header.fileCount, sizeof(data.header.fileCount));
	for (const auto& offset : data.fileOffsets)
	{
		os.write((char*)&offset, sizeof(offset));
	}
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const TBin::TBinFile& file)
{
	os.write(file.data.data(), file.data.size());
	return os;
}

inline std::istream& operator>>(std::istream& is, TBin::TBinData& data)
{
	data.header.magic = is.read((char*)&data.header.magic, sizeof(data.header.magic)) ? data.header.magic : 0;
	data.header.version = is.read((char*)&data.header.version, sizeof(data.header.version)) ? data.header.version : 0;
	data.header.reserved = is.read((char*)&data.header.reserved, sizeof(data.header.reserved)) ? data.header.reserved : 0;
	data.header.fileCount = is.read((char*)&data.header.fileCount, sizeof(data.header.fileCount)) ? data.header.fileCount : 0;
	data.fileOffsets.resize(data.header.fileCount);
	for (auto& offset : data.fileOffsets)
	{
		offset = is.read((char*)&offset, sizeof(offset)) ? offset : 0;
	}
	return is;
}

inline std::istream& operator>>(std::istream& is, TBin::TBinFile& file)
{
	file.data.clear();
	char buffer[1024];
	while (is.read(buffer, sizeof(buffer)))
	{
		file.data.insert(file.data.end(), buffer, buffer + is.gcount());
	}
	return is;
}