#pragma once

#include <string>
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "config.h"

#define BUFFER_SIZE 512 * 1024 * 1024

class Encoder
{
private:
	uint64_t chunkSize;
	std::string outDirectory;
	std::string outBaseName;
	char* buffer;

	int64_t currentChunkIndex = 0;
	std::ofstream currentChunkStream;

	void StartNextChunk()
	{
		if (currentChunkStream)
		{
			currentChunkStream.close();
		}

		currentChunkIndex++;

		std::filesystem::path dir = outDirectory;
		std::filesystem::path chunkPath = dir / (outBaseName + "." + std::to_string(currentChunkIndex) + "." + SPLITTED_EXTENSION);
		currentChunkStream = std::ofstream(chunkPath, std::ios::binary);
	}

	void StartFirstChunk()
	{
		currentChunkIndex = -1;
		StartNextChunk();
	}

	void WriteByte(char b)
	{
		WriteBuffer(&b, 1);
	}
	
	void WriteBuffer(const char* buffer, uint64_t count)
	{
 		if (!currentChunkStream.is_open())
		{
			StartFirstChunk();
		}

		uint64_t currentChunkPos = currentChunkStream.tellp();

		uint64_t beforeEndOfChunk = chunkSize - currentChunkPos;

		if (count < beforeEndOfChunk)
		{
			currentChunkStream.write(buffer, count);
			return;
		}
		
		currentChunkStream.write(buffer, beforeEndOfChunk);		

		currentChunkPos = currentChunkStream.tellp();

		if (currentChunkPos >= chunkSize)
		{
			assert(currentChunkPos == chunkSize);

			StartNextChunk();
		}
		
		if (count - beforeEndOfChunk > 0)
		{
			WriteBuffer(buffer + beforeEndOfChunk, count - beforeEndOfChunk);
		}
	}

	template<typename T>
	void WriteValue(T value)
	{
		char* bytes = reinterpret_cast<char*>(&value);		
		WriteBuffer(bytes, sizeof(value));		
	}

	void WriteString(const std::string& str)
	{
		uint64_t strSize = str.size();
		WriteValue(strSize);

		const char* c_str = str.c_str();
		WriteBuffer(c_str, str.size());
	}

public:

	~Encoder()
	{
		currentChunkStream.close();

		delete[] buffer;
	}

	Encoder(const std::string& outDirectory, const std::string& outBaseName, uint64_t chunkSize)
	{
		if (!std::filesystem::exists(outDirectory))
		{
			std::filesystem::create_directories(outDirectory);
		}

		if (!std::filesystem::is_directory(outDirectory))
		{
			throw std::exception("Path already exists!");
		}
		
		this->outBaseName = outBaseName;
		assert(chunkSize > 0);
		this->chunkSize = chunkSize;

		this->outDirectory = outDirectory;

		buffer = new char[BUFFER_SIZE];
	}

	void EncodeFile(const std::string& filePath, const std::string& relativeTo)
	{
		std::filesystem::path relativePath = std::filesystem::relative(filePath, relativeTo);

		std::cout << "Encoding relative file path: " << relativePath << std::endl;
		WriteString(relativePath.string());

		std::ifstream infile(filePath, std::ios::binary);

		// Get length of file
		infile.seekg(0, std::ios::end);
		size_t length = infile.tellg();
		infile.seekg(0, std::ios::beg);

		size_t readBytesTotal = 0;

		std::cout << "Encoding file size: " << length << std::endl;
		WriteValue(length);
		
		while(readBytesTotal < length)
		{
			infile.read(buffer, BUFFER_SIZE);

			uint64_t readBytes = infile.gcount();

			assert(readBytes <= BUFFER_SIZE);
			
			std::cout << "Encoding buffer of size " << readBytes << std::endl;
			WriteBuffer(buffer, readBytes);

			readBytesTotal += readBytes;

			if (infile.eof())
			{
				break;
			}
		}

		assert(readBytesTotal == length);
	}
};

