/** @file ProjectPathTests.cpp @brief Portable root selection under relocation and unrelated working directories. */
#include "Engine/Core/ProjectPaths.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char**argv) {
    namespace fs=std::filesystem;using namespace NoJob::Paths;
    const auto base=fs::absolute(argv[1]);const auto portable=base/"Relocated Alpha with spaces";
    const auto source=base/"Other project";const auto empty=base/"unrelated";
    for(const auto& p:{portable,source}){fs::create_directories(p/"Assets");std::ofstream(p/"Example.nojobproject")<<"test";}
    fs::create_directories(empty);std::ofstream(portable/"NJE.alpha")<<"ALPHA 1";
    auto require=[](bool ok){if(!ok)throw std::runtime_error("Project path regression");};
    require(FindProjectRootFrom(empty,portable)==portable);
    require(FindProjectRootFrom(source,portable)==portable);
    require(FindProjectRootFrom(source,empty)==source);
    require(!IsProject(empty));
    require(FindProjectRootFrom({},{}).empty());
    require(!HasSourceTools(portable));
    fs::create_directories(source/"Engine");fs::create_directories(source/"Editor");std::ofstream(source/"CMakeLists.txt")<<"test";
    require(HasSourceTools(source));
    std::cout<<"PASS: relocated package, spaces, unrelated/foreign cwd, source precedence and portable source-tool detection\n";
}
