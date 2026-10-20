#include "Engine/Assets/Project.h"
#include <fstream>
#include <iomanip>
namespace NoJob
{
 bool Project::Save(const ProjectConfig& c,const std::filesystem::path& p){std::ofstream o(p);if(!o)return false;o<<"NOJOB_PROJECT 1\n"<<std::quoted(c.Name)<<'\n'<<std::quoted(c.AssetDirectory.generic_string())<<'\n'<<std::quoted(c.StartScene.generic_string())<<'\n';return true;}
 bool Project::Load(ProjectConfig& c,const std::filesystem::path& p){std::ifstream i(p);if(!i)return false;std::string h,a,s;if(!std::getline(i,h)||h.rfind("NOJOB_PROJECT",0)!=0)return false;i>>std::quoted(c.Name)>>std::quoted(a)>>std::quoted(s);c.AssetDirectory=a;c.StartScene=s;return true;}
}
