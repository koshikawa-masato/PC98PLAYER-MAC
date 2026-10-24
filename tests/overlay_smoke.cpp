// SPDX-License-Identifier: MIT
#include "hostfs.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
namespace fs=std::filesystem;
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static std::string read(const fs::path& path) {
    void* h=hostfs::open(path.string(),0,false,false); if(!h) return "";
    char data[256]; int count=hostfs::read(h,data,sizeof data); hostfs::close(h);
    return count<0 ? "" : std::string(data,count);
}
int main() {
    char tmp[]="/tmp/pc98-overlay-XXXXXX"; if(!mkdtemp(tmp)) return 1;
    fs::path root(tmp),base=root/"cache",saved=root/"saves";
    try {
        fs::create_directories(base/"folder");
        std::ofstream(base/"GAME.DAT")<<"original";
        std::ofstream(base/"folder"/"A.DAT")<<"alpha";
        std::ofstream(base/"folder"/"B.DAT")<<"beta";
        hostfs::set_overlay(base.string(),saved.string());
        check(!hostfs::mkdir((base/"missing"/"nested").string()),"mkdir must not invent missing guest parents");
        check(!hostfs::open((base/"missing"/"NEW.DAT").string(),1,true,true),"create must not invent missing guest parents");
        check(!hostfs::rename((base/"folder").string(),(base/"folder"/"inside").string()),"reject moving directory into itself");
        void* reader=hostfs::open((base/"GAME.DAT").string(),0,false,false); check(reader,"open reader");
        void* h=hostfs::open((base/"GAME.DAT").string(),2,false,false); check(h,"open rw");
        check(!fs::exists(saved/"GAME.DAT"),"readwrite open must not retain unmodified game data");
        check(hostfs::seek(h,0,2)==8,"seek end"); check(hostfs::write(h,"!",1)==1,"copy-on-write"); hostfs::close(h);
        char b[16]={}; check(hostfs::read(reader,b,16)==9 && std::string(b,9)=="original!","existing reader follows promoted file"); hostfs::close(reader);
        check(read(base/"GAME.DAT")=="original!","read updated");
        check(hostfs::remove((base/"folder"/"A.DAT").string()),"delete file");
        std::vector<HostDirEntry> entries; check(hostfs::list_dir((base/"folder").string(),entries),"parent stays visible");
        check(entries.size()==1 && entries[0].name=="B.DAT","merge and whiteout");
        check(hostfs::rename((base/"folder").string(),(base/"renamed").string()),"rename merged directory");
        HostDirEntry st; check(!hostfs::stat((base/"folder").string(),st),"old directory hidden");
        check(read(base/"renamed"/"B.DAT")=="beta","renamed child readable");
        check(hostfs::mkdir((base/"folder").string()),"recreate deleted directory");
        entries.clear(); check(hostfs::list_dir((base/"folder").string(),entries) && entries.empty(),"old contents must not reappear");
        check(hostfs::remove((base/"GAME.DAT").string()),"delete modified file");
        h=hostfs::open((base/"GAME.DAT").string(),1,true,true); check(h,"recreate file");
        check(hostfs::write(h,"save",4)==4,"write recreated"); hostfs::close(h);
        hostfs::set_overlay("","");
        check(read(base/"GAME.DAT")=="original","source data unchanged");
        check(read(base/"folder"/"A.DAT")=="alpha","deleted source unchanged");
        fs::remove_all(base); fs::create_directories(base); // cache evicted, then fetched again
        std::ofstream(base/"GAME.DAT")<<"original";
        hostfs::set_overlay(base.string(),saved.string());
        check(read(base/"GAME.DAT")=="save","saves survive cache eviction and refetch");
        hostfs::set_overlay("",""); fs::remove_all(root);
        puts("PASS: lazy copy-on-write, existing handles, whiteouts, directory rename, cache eviction, source preservation"); return 0;
    } catch(const std::exception& e) { fprintf(stderr,"FAIL: %s\n",e.what()); hostfs::set_overlay("",""); fs::remove_all(root); return 1; }
}
