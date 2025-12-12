// FileScanDemo.cpp : 只保留清晰的线程启动与参数解析逻辑。

#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "FileWatchers.hpp"


class PrintEachLine {
public:
    void onRow(const std::string& filepath, const std::vector<std::string>& row) {
        std::cout << "[" << filepath << "] ";
        for (size_t j = 0; j < row.size(); ++j) std::cout << "col" << j << "=" << row[j] << '|';
        std::cout << std::endl;
    }
};



int main(int argc, char** argv)
{
    // Usage:
    //   -a <filePath.csv>             监听单个CSV文件，增量按行解析
    //   -n <dirPath> <prefix>         每5秒读取目录下前缀匹配的最新CSV并全量解析
    if (argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "  FileScanDemo.exe -a <filePath.csv>\n";
        std::cout << "  FileScanDemo.exe -n <dirPath> <prefix>\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "-a")
    {
        if (argc != 3)
        {
            std::cerr << "参数错误: -a 需要 1 个参数 <filePath.csv>\n";
            return 1;
        }
        std::string filePath = argv[2];
        PrintEachLine printer;
        auto t = FileWatchers::CreateWatchAppend(filePath,
            [&printer](const std::string& fp, const std::vector<std::string>& row){ printer.onRow(fp, row); },
            /*waitSeconds=*/5);
        t.join();
    }
    else if (mode == "-n")
    {
        if (argc != 4)
        {
            std::cerr << "参数错误: -n 需要 2 个参数 <dirPath> <prefix>\n";
            return 1;
        }
        std::string dir = argv[2];
        std::string prefix = argv[3];
        PrintEachLine printer;
        auto t = FileWatchers::CreateWatchNew(dir, prefix,
            [&printer](const std::string& fp, const std::vector<std::string>& row){ printer.onRow(fp, row); },
            /*waitSeconds=*/5);
        t.join();
    }
    else
    {
        std::cerr << "未知参数: " << mode << "\n";
        std::cout << "Usage:\n";
        std::cout << "  FileScanDemo.exe -a <filePath.csv>\n";
        std::cout << "  FileScanDemo.exe -n <dirPath> <prefix>\n";
        return 1;
    }

    return 0;
}
