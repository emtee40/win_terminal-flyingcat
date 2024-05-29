#include "precomp.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <format>
#include <chrono>
#include <array>
#include <optional>
#include <memory>
#include <random>

#include <span>
#include <vector>
#include <string>
#include <string_view>

#include "../parser/stateMachine.hpp"
#include "../parser/OutputStateMachineEngine.hpp"

#include "DataSource.h"
#include "dispatch.h"
#include "v1/Parser_v1.h"
#include "v2/Parser_v2.h"
#include "v2/OutputEngine.h"

using namespace vt;
using namespace Microsoft::Console::VirtualTerminal;

static double Measure(auto&& fn)
{
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = finish - start;
    return elapsed.count();
}

static auto Read(auto&& f)
{
    std::ifstream fs(f, std::ios::binary);
    return std::string{ std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>() };
}

static void Check(const std::filesystem::path& path)
{
    const auto content = Read(path);
    const std::string_view ct{ content };

    auto fileSize = std::filesystem::file_size(path);
    auto sizeLabel = (double)fileSize / 1024;
    std::wstring postfix = L"KB";
    if (sizeLabel > 1024)
    {
        sizeLabel = sizeLabel / 1024;
        postfix = L"MB";
    }

    std::wcout << std::format(L"> {} : {:.2f} {}\t", path.filename().wstring(), sizeLabel, postfix);

    LogData logData_0;
    auto fn_0 = [&](std::string_view src) {
        DataSource vtds;
        StringStream ss{ src };
        StateMachine stateMachine{ std::make_unique<OutputStateMachineEngine>(std::make_unique<DispLogger>(logData_0)), false };
        return Measure([&]() {
            while (vtds.ReadFrom(ss))
            {
                stateMachine.ProcessString(vtds.Data());
            }
        });
    };
    fn_0(ct);

    LogData logData_1;
    auto fn_1 = [&](std::string_view src) {
        DataSource vtds;
        StringStream ss{ src };
        v1::Parser stateMachine{ std::make_unique<OutputStateMachineEngine>(std::make_unique<DispLogger>(logData_1)), false };
        return Measure([&]() {
            while (vtds.ReadFrom(ss))
            {
                stateMachine.ProcessString(vtds.Data());
            }
        });
    };
    fn_1(ct);
    if (logData_1 != logData_0)
    {
        std::cout << "v1 failed." << std::endl;
        return;
    }

    LogData logData_2;
    auto fn_2 = [&](std::string_view src) {
        DataSource vtds;
        StringStream ss{ src };
        v2::Parser<v2::OutputEngine<DispLogger>, false> stateMachine{ logData_2 };
        return Measure([&]() {
            while (vtds.ReadFrom(ss))
            {
                stateMachine.ProcessString(vtds.Data());
            }
        });
    };
    fn_2(ct);
    if (logData_2 != logData_0)
    {
        std::cout << "v2 failed." << std::endl;
        return;
    }

    std::cout << "Passed." << std::endl;

#ifdef NDEBUG
    auto bench = [&](auto... fn) {
        constexpr auto size = sizeof...(fn);
        std::array fns = { std::function(fn)... };
        std::array<double, size> times;
        std::array<size_t, size> indexs;
        for (size_t i = 0; i < size; i++)
        {
            times[i] = 0;
            indexs[i] = i;
        }

        std::random_device rand_dev;
        std::mt19937 generator(rand_dev());

        auto dump = [&](size_t n) {
            n = std::max((size_t)1, n);
            const size_t size = 1024 * 64 *  n;
            std::uniform_int_distribution<int> distr(1, 127);
            std::string vec;
            vec.resize(size);
            for (size_t i = 0; i < size; i++)
            {
                vec[i] = (char)distr(generator);
            }
            return vec;
        };

        size_t count = std::max((100 * 1024 * 1024) / fileSize, (size_t)16);
        std::uniform_int_distribution<size_t> distr(8, 16);
        for (size_t i = 0; i < count; i++)
        {
            std::shuffle(indexs.begin(), indexs.end(), generator);
            for (auto index : indexs)
            {
                fns[index](dump(distr(generator)));
                times[index] += fns[index](ct);
                fns[index](dump(distr(generator)));
            }
        }
        auto t0 = times[0] / count;
        std::cout << std::format("parser 0: {:.2f} us", t0) << std::endl;
        for (size_t i = 1; i < size; i++)
        {
            auto t = times[i] / count ;
            std::cout << std::format("parser {}: {:.2f} us, {:+.1f}%", i, t, (t0 / t - 1) * 100) << std::endl;
        }
    };

    bench(fn_0, fn_1, fn_2);
#endif // !NDEBUG
        
    std::cout << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
    std::vector<std::filesystem::path> paths;
    if (argc < 2)
    {
        std::filesystem::path exePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
        exePath.remove_filename();
        paths.emplace_back(exePath / L"VT_EN_P");
        paths.emplace_back(exePath / L"VT_EN_V");
        paths.emplace_back(exePath / L"VT_CN_P");
        paths.emplace_back(exePath / L"VT_CN_V");
    }
    else
    {
        for (int i = 1; i < argc; i++)
        {
            paths.emplace_back(argv[i]);
        }
    }

    for (auto&& f : paths)
    {
        if (!std::filesystem::exists(f))
        {
            std::wcout << std::format(L"File \"{}\" not exists", f.wstring()) << std::endl;
            return 0;
        }
    }

    for (auto&& f : paths)
    {
        Check(f);
    }

    return 0;
}
