#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>

#define DECODER_BUFFER_SIZE 512UL * 1024 * 1024

class Decoder
{
private:	
	int64_t currentChunkIndex;
	std::vector<std::string> chunkFiles;
	std::string outputDir;

	char* buffer = nullptr;

	std::ifstream currentChunkStream;	

	template<typename T>
	T ReadValue()
	{
		T value;
		uint64_t readCount = ReadBytes(reinterpret_cast<char*>(&value), sizeof(T));

		if (readCount != sizeof(T))
		{
			throw std::exception("Cannot Read value!");
		}

		return value;
	}

	std::string ReadString()
	{
		uint64_t strSize = ReadValue<uint64_t>();

		std::string str;

		char* data = new char[strSize + 1];

		uint64_t readCount = ReadBytes(data, strSize);

		if (readCount != strSize)
		{
			throw std::exception("Cannot Read String!");
		}

		data[strSize] = '\0';

		str.assign(data);

		delete[] data;

		return str;
	}

	bool OpenNextChunkStream()
	{
		currentChunkIndex++;

		if (currentChunkStream.is_open())
		{
			currentChunkStream.close();
		}

		if (currentChunkIndex < chunkFiles.size())
		{
			currentChunkStream = std::ifstream(chunkFiles[currentChunkIndex], std::ios::binary);
			return true;
		}

		return false;
	}

	uint64_t ReadBytes(char* buffer, uint64_t count)
	{
		if (!currentChunkStream.is_open())
		{
			throw std::exception("Failed to read bytes: chunk stream closed");
		}

		uint64_t totalReadBytes = 0;

		while (totalReadBytes < count)
		{
			currentChunkStream.read(buffer + totalReadBytes, count - totalReadBytes);
			uint64_t readBytes = currentChunkStream.gcount();			

			totalReadBytes += readBytes;

			if (!currentChunkStream)
			{
				bool nextChunkLoaded = OpenNextChunkStream();
				if (!nextChunkLoaded)
				{
					break;
				}
			}
		}

		return totalReadBytes;
	}
	
public:

	~Decoder()
	{
		delete[] buffer;
	}

	Decoder(std::vector<std::string> chunkFiles, std::string outputDir)
	{
		this->chunkFiles = chunkFiles;
		this->outputDir = outputDir;
		currentChunkIndex = -1;

		buffer = new char[DECODER_BUFFER_SIZE];

		OpenNextChunkStream();
	}

	std::filesystem::path GetFileOutputPath(const std::string& relativePath)
	{
		std::filesystem::path filePath(outputDir);
		filePath = filePath / relativePath;
		return filePath;
	}

	bool DecodeNextFile()
	{
		if (currentChunkIndex >= chunkFiles.size())
		{
			return false;
		}

		if (!currentChunkStream.is_open())
		{
			return false;
		}

		if (currentChunkStream.peek() == EOF)
		{
			return false;
		}

		std::string filePathRelativeStr = ReadString();
		uint64_t fileSize = ReadValue<uint64_t>();

		std::cout << "[Decoder] Decoding file " << filePathRelativeStr << " with size " << fileSize << std::endl;

		std::filesystem::path filePath = GetFileOutputPath(filePathRelativeStr);

		if (!std::filesystem::exists(filePath.parent_path()))
		{
			std::filesystem::create_directories(filePath.parent_path());
		}

		std::ofstream fileStream = std::ofstream(filePath, std::ios::binary);

		uint64_t totalReadBytes = 0;
		while (totalReadBytes < fileSize)
		{
			uint64_t bytesToRead = std::min(fileSize, (uint64_t)DECODER_BUFFER_SIZE);
			uint64_t readBytes = ReadBytes(buffer, bytesToRead);

			if (readBytes != bytesToRead)
			{
				throw std::exception("Failed to decode file!");
			}

			fileStream.write(buffer, bytesToRead);
			totalReadBytes += readBytes;
		}

		fileStream.close();
	}

	void DecodeAllFiles()
	{
		while (true)
		{
			if (!DecodeNextFile())
			{
				break;
			}
		}
	}
};

