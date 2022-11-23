#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <chrono>
#include "config.h"
#include "Encoder.h"
#include "Decoder.h"

using namespace std;

template<typename Action>
void TraverseDirectory(const string& path, Action action)
{
	if (!filesystem::is_directory(path))
	{
		return;
	}

	for (const auto& entry : filesystem::directory_iterator(path))
	{
		if (entry.is_directory())
		{
			TraverseDirectory(entry.path().string(), action);
		}
		else
		{
			action(entry.path().string());
		}
	}
}

void GetEncoderInputFiles(const string& inputPath, vector<string>& inputFiles)
{
	if (std::filesystem::is_directory(inputPath))
	{
		TraverseDirectory(inputPath, [&inputFiles](const string& p) {inputFiles.push_back(p); });
	}
	else
	{
		inputFiles.push_back(inputPath);
	}
}

int GetStringSegments(const string& str, vector<string>& segments, char separator = '.')
{
	if (str.empty())
	{
		return 0;
	}
	
	string segment = "";
	for (int i = 0; i < str.size(); i++)
	{
		if (str[i] == separator)
		{
			segments.push_back(segment);
			segment = "";
		}
		else
		{
			segment += str[i];
		}
	}

	return static_cast<int>(segments.size());
}

int GetDecoderSplitIndex(const string& filePath)
{
	vector<string> inputFileNameSegments;
	if (GetStringSegments(filePath, inputFileNameSegments) < 2)
	{
		cerr << "Failed to get list of encoded files: invalid file name" << endl;
		return -1;
	}

	string splitIndexStr = inputFileNameSegments[inputFileNameSegments.size() - 1];

	int splitIndex = std::stoi(splitIndexStr);

	return splitIndex;
}

void GetDecoderInputFiles(const string& inputPathStr, vector<string>& inputFiles)
{
	if (std::filesystem::is_directory(inputPathStr))
	{
		return;
	}

	filesystem::path inputPath = inputPathStr;	
	filesystem::path inputPathParent = inputPath.parent_path();
	
	for (const auto& entry : filesystem::directory_iterator(inputPathParent))
	{
		if (entry.is_directory())
		{
			continue;
		}
		
		filesystem::path entryPath = entry.path();
		if (entryPath.extension() == "." SPLITTED_EXTENSION)
		{
			inputFiles.push_back(entryPath.string());
		}
	}

	auto comparator = [](const string& a, const string& b)
	{
		int aIndex = GetDecoderSplitIndex(a);
		int bIndex = GetDecoderSplitIndex(b);
		return aIndex < bIndex;
	};

	std::sort(inputFiles.begin(), inputFiles.end(), comparator);
}

int WorkAsDecoder(int argc, char** argv)
{
	string inputPathStr = argv[1];
	std::filesystem::path inputPath = inputPathStr;

	string outputPathStr = argv[2];

	vector<string> inputFiles;

	GetDecoderInputFiles(inputPathStr, inputFiles);

	cout << "Encoded files detected: " << endl;
	for (std::string filePath : inputFiles)
	{
		cout << filePath << endl;
	}

	filesystem::create_directories(outputPathStr);

	Decoder decoder(inputFiles, outputPathStr);
	decoder.DecodeAllFiles();

	cout << "Decoding finished!" << endl;

	return 0;
}

int WorkAsEncoder(int argc, char** argv)
{
	string inputPathStr = argv[1];
	std::filesystem::path inputPath = inputPathStr;

	string outputPathStr = argv[2];

	if (argc < 4)
	{
		cout << "Invalid number of arguments" << endl;
		return -1;
	}

	string chunksCountStr = argv[3];

	int chunksCount = std::stoi(chunksCountStr);

	vector<string> inputFiles;
	
	GetEncoderInputFiles(inputPathStr, inputFiles);

	uint64_t totalSize = 0;

	for (const string& file : inputFiles)
	{
		totalSize += filesystem::file_size(file);
	}

	cout << "[Encoder] Total size: " << totalSize << endl;

	uint64_t chunkSize = totalSize / chunksCount;

	cout << "[Encoder] Chunk Size: " << chunkSize << endl;

	Encoder encoder(outputPathStr, inputPath.filename().string(), chunkSize);

	string relativeTo;
	if (filesystem::is_directory(inputPathStr))
	{
		relativeTo = inputPathStr;
	}
	else
	{
		relativeTo = filesystem::path(inputPathStr).parent_path().string();
	}

	auto t1 = std::chrono::high_resolution_clock::now();

	for (const string& file : inputFiles)
	{
		cout << "[Encoder] Writing " << file << "..." << endl;
		encoder.EncodeFile(file, relativeTo);
	}

	auto t2 = std::chrono::high_resolution_clock::now();

	auto executionSeconds = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);

	cout << "Encoding finished in " << executionSeconds << endl;

	cout << "Average speed: " << (double)(totalSize) / executionSeconds.count() / 1024 / 1024 << " mb/s" << endl;

	return 0;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		cout << "Invalid number of arguments" << endl;
		return -1;
	}

	string inputPathStr = argv[1];	

	if (!filesystem::exists(inputPathStr))
	{
		cerr << "Path " << inputPathStr << " does not exist!" << endl;
		return -1;
	}

	filesystem::path inputPath = inputPathStr;

	filesystem::path extension = inputPath.extension();

	bool hasSplitterExtension = extension == "." SPLITTED_EXTENSION;

	if (filesystem::is_directory(inputPath) || !hasSplitterExtension)
	{
		return WorkAsEncoder(argc, argv);
	}
	else
	{
		return WorkAsDecoder(argc, argv);
	}
}
