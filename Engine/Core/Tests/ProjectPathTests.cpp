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
    const auto exported=base/"Exported game";
    fs::create_directories(exported/"Assets");std::ofstream(exported/"Game.nojobproject")<<"test";
    std::ofstream(exported/"NJE.runtime")<<"NJE_RUNTIME 1";
    require(FindProjectRootFrom(source,exported)==exported);
    require(!IsProject(empty));
    require(FindProjectRootFrom({},{}).empty());
    require(!HasSourceTools(portable));
    fs::create_directories(source/"Engine");fs::create_directories(source/"Editor");std::ofstream(source/"CMakeLists.txt")<<"test";
    require(HasSourceTools(source));
    // An installer update can add templates, but cannot replace modified projects or models.
    const auto app=base/"Installed templates";const auto profile=base/"User profile";
    fs::create_directories(app/"Assets/Neural");std::ofstream(app/"NJE.nojobproject")<<"project";
    std::ofstream(app/"Assets/Neural/policy.njn")<<"factory";
    const auto workspace=EnsureWorkspaceFrom(app,profile);
    std::ofstream(workspace/"Assets/Neural/policy.njn")<<"user trained model";
    std::ofstream(app/"Assets/Neural/policy.njn")<<"new factory";
    std::ofstream(app/"Assets/Neural/new.njn")<<"new sample";
    require(EnsureWorkspaceFrom(app,profile)==workspace);
    std::ifstream saved(workspace/"Assets/Neural/policy.njn");std::string content;std::getline(saved,content);
    require(content=="user trained model");require(fs::is_regular_file(workspace/"Assets/Neural/new.njn"));
    require(!HasSourceTools(workspace));
    std::cout<<"PASS: relocated package, spaces, unrelated/foreign cwd, source precedence and portable source-tool detection\n";
}
