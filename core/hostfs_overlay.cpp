// SPDX-License-Identifier: MIT
#include "hostfs.h"
#include "hostfs_platform.h"
#include <filesystem>
#include <map>
#include <algorithm>
#include <functional>
namespace fs = std::filesystem;
namespace {
std::string sourceRoot, saveRoot;
struct Mapping { bool active=false; std::string relative, saved, deleted; };
Mapping mapping(const std::string& path) {
    Mapping m;
    if(sourceRoot.empty()) return m;
    std::string p=fs::path(path).lexically_normal().generic_string();
    if(p!=sourceRoot && p.rfind(sourceRoot+"/",0)!=0) return m;
    m.active=true; m.relative=p==sourceRoot ? "" : p.substr(sourceRoot.size()+1);
    m.saved=saveRoot+(m.relative.empty()?"":"/"+m.relative);
    m.deleted=saveRoot+".deleted"+(m.relative.empty()?"":"/"+m.relative)+"/.entry";
    return m;
}
bool exists(const std::string& p) { HostDirEntry e; return hostfs_platform::stat(p,e); }
bool baseHidden(const Mapping& m) {
    if(!m.active) return false;
    fs::path relative=m.relative;
    while(!relative.empty()) {
        if(exists(saveRoot+".deleted/"+relative.generic_string()+"/.opaque")) return true;
        relative=relative.parent_path();
    }
    return false;
}
bool hidden(const Mapping& m) {
    if(!m.active) return false;
    fs::path relative=m.relative;
    while(!relative.empty()) {
        if(exists(saveRoot+".deleted/"+relative.generic_string()+"/.entry")) return true;
        relative=relative.parent_path();
    }
    return baseHidden(m) && !exists(m.saved);
}
bool parents(const std::string& path) {
    std::error_code e; fs::create_directories(fs::path(path).parent_path(),e); return !e;
}
bool marker(const Mapping& m) {
    if(!parents(m.deleted)) return false;
    void* h=hostfs_platform::open(m.deleted,1,true,true);
    if(!h) return false;
    hostfs_platform::close(h); return true;
}
std::string readable(const std::string& p, const Mapping& m) { return m.active && exists(m.saved) ? m.saved : p; }
bool copyToSave(const std::string& p, const Mapping& m) {
    if(!parents(m.saved)) return false;
    if(exists(m.saved)) return true;
    if(hidden(m) || !exists(p)) return true;
    std::error_code e; fs::copy_file(p,m.saved,fs::copy_options::overwrite_existing,e);
    if(e) return false;
    // A source on a read-only share may have no user write bit.
    fs::permissions(m.saved,fs::perms::owner_write,fs::perm_options::add,e); return !e;
}
bool parentExists(const std::string& path) {
    std::string parent=fs::path(path).parent_path().generic_string();
    if(parent.empty()) return true;
    auto m=mapping(parent); HostDirEntry e;
    return !hidden(m) && hostfs_platform::stat(readable(parent,m),e) && e.is_dir;
}
struct File { void* raw=nullptr; std::string path; Mapping map; int mode=0; bool promoted=false; };
bool promote(File* f, bool write) {
    if(!f->map.active || f->promoted || (!write && !exists(f->map.saved))) return true;
    if(write && !copyToSave(f->path,f->map)) return false;
    int64_t offset=hostfs_platform::seek(f->raw,0,1);
    void* replacement=hostfs_platform::open(f->map.saved,f->mode,false,false);
    if(!replacement) return false;
    if(hostfs_platform::seek(replacement,offset,0)<0) { hostfs_platform::close(replacement); return false; }
    hostfs_platform::close(f->raw); f->raw=replacement; f->promoted=true; return true;
}
}
namespace hostfs {
void set_overlay(const std::string& source,const std::string& saved) {
    sourceRoot=source.empty()?"":fs::path(source).lexically_normal().generic_string();
    saveRoot=saved.empty()?"":fs::path(saved).lexically_normal().generic_string();
    if(saveRoot.empty()) sourceRoot.clear();
}
}
namespace hostfs_native {
std::string join(const std::string& a,const std::string& b) { return hostfs_platform::join(a,b); }
std::string from_sjis(const std::string& s) { return hostfs_platform::from_sjis(s); }
std::string to_sjis(const std::string& s) { return hostfs_platform::to_sjis(s); }
bool stat(const std::string& p,HostDirEntry& e) { auto m=mapping(p); return !hidden(m) && hostfs_platform::stat(readable(p,m),e); }
bool list_dir(const std::string& p,std::vector<HostDirEntry>& out) {
    auto m=mapping(p); if(!m.active) return hostfs_platform::list_dir(p,out);
    HostDirEntry st; if(!stat(p,st) || !st.is_dir) return false;
    std::vector<HostDirEntry> base,added; if(!baseHidden(m)) hostfs_platform::list_dir(p,base); hostfs_platform::list_dir(m.saved,added);
    std::map<std::string,HostDirEntry> merged;
    for(auto& e:base) merged[e.name]=e;
    for(auto& e:added) merged[e.name]=e;
    for(auto& e:merged) if(!hidden(mapping(join(p,e.first)))) out.push_back(e.second);
    return true;
}
void* open(const std::string& p,int mode,bool create,bool trunc) {
    auto m=mapping(p); bool wasHidden=hidden(m);
    if(wasHidden && !create) return nullptr;
    if(m.active && (create || trunc)) {
        if(!parentExists(p) || !parents(m.saved) || (!trunc && !copyToSave(p,m))) return nullptr;
        // A recreated file must not reveal the old cached contents.
        if(wasHidden) hostfs_platform::remove(m.saved);
    }
    std::string target=m.active && (create || trunc) ? m.saved : readable(p,m);
    void* raw=hostfs_platform::open(target,mode,create,trunc);
    if(!raw) return nullptr;
    if(m.active && (create || trunc)) hostfs_platform::remove(m.deleted);
    return new File{raw,p,m,mode,m.active && target==m.saved};
}
void close(void* h) { auto* f=static_cast<File*>(h); hostfs_platform::close(f->raw); delete f; }
int read(void* h,void* b,int n) { auto* f=static_cast<File*>(h); if(!promote(f,false)) return -1; return hostfs_platform::read(f->raw,b,n); }
int write(void* h,const void* b,int n) { auto* f=static_cast<File*>(h); if(f->mode==0 || !promote(f,true)) return -1; return hostfs_platform::write(f->raw,b,n); }
int64_t seek(void* h,int64_t o,int w) { auto* f=static_cast<File*>(h); if(!promote(f,false)) return -1; return hostfs_platform::seek(f->raw,o,w); }
bool truncate_here(void* h) { auto* f=static_cast<File*>(h); return f->mode!=0 && promote(f,true) && hostfs_platform::truncate_here(f->raw); }
bool remove(const std::string& p) {
    auto m=mapping(p); if(!m.active) return hostfs_platform::remove(p);
    HostDirEntry e; if(!stat(p,e) || e.is_dir || !marker(m)) return false;
    hostfs_platform::remove(m.saved); return true;
}
bool mkdir(const std::string& p) {
    auto m=mapping(p); if(!m.active) return hostfs_platform::mkdir(p);
    bool wasHidden=hidden(m);
    HostDirEntry e; if(stat(p,e) || !parentExists(p) || !parents(m.saved) || !hostfs_platform::mkdir(m.saved)) return false;
    if(wasHidden) {
        Mapping opaque=m; opaque.deleted=(fs::path(m.deleted).parent_path()/".opaque").generic_string();
        if(!marker(opaque)) { hostfs_platform::rmdir(m.saved); return false; }
    }
    hostfs_platform::remove(m.deleted); return true;
}
bool rmdir(const std::string& p) {
    auto m=mapping(p); if(!m.active) return hostfs_platform::rmdir(p);
    HostDirEntry e; std::vector<HostDirEntry> entries;
    if(!stat(p,e) || !e.is_dir || !list_dir(p,entries) || !entries.empty() || !marker(m)) return false;
    hostfs_platform::rmdir(m.saved); return true;
}
bool rename(const std::string& a,const std::string& b) {
    auto ma=mapping(a),mb=mapping(b);
    if(!ma.active && !mb.active) return hostfs_platform::rename(a,b);
    if(!ma.active || !mb.active) return false;
    HostDirEntry e,other; if(!stat(a,e) || stat(b,other) || !parentExists(b)) return false;
    if(e.is_dir && mb.relative.rfind(ma.relative+"/",0)==0) return false;
    if(!parents(mb.saved)) return false;
    if(e.is_dir) {
        if(mb.relative.rfind(ma.relative+"/",0)==0) return false;
        static unsigned serial=0;
        std::string stage=mb.saved+".pc98-rename-"+std::to_string(++serial);
        if(exists(stage)) return false;
        std::function<bool(const std::string&,const std::string&,int)> materialize;
        materialize=[&](const std::string& from,const std::string& to,int depth) {
            if(depth>64) return false;
            HostDirEntry st; if(!stat(from,st)) return false;
            std::error_code error;
            if(!st.is_dir) { fs::copy_file(readable(from,mapping(from)),to,fs::copy_options::none,error); return !error; }
            fs::create_directory(to,error); if(error) return false;
            std::vector<HostDirEntry> children; if(!list_dir(from,children)) return false;
            for(auto& child:children) if(!materialize(join(from,child.name),join(to,child.name),depth+1)) return false;
            return true;
        };
        std::error_code cleanup;
        if(!materialize(a,stage,0) || !marker(ma)) { fs::remove_all(stage,cleanup); return false; }
        if(!hostfs_platform::rename(stage,mb.saved)) {
            hostfs_platform::remove(ma.deleted); fs::remove_all(stage,cleanup); return false;
        }
        // Publish the destination before cleaning hidden source data.
        fs::remove_all(fs::path(mb.deleted).parent_path(),cleanup);
        Mapping opaque=mb; opaque.deleted=(fs::path(mb.deleted).parent_path()/".opaque").generic_string();
        marker(opaque);
        fs::remove_all(ma.saved,cleanup); return true;
    }
    if(!copyToSave(a,ma) || !marker(ma)) return false;
    if(!hostfs_platform::rename(ma.saved,mb.saved)) { hostfs_platform::remove(ma.deleted); return false; }
    hostfs_platform::remove(mb.deleted); return true;
}
bool set_time(void* h,uint16_t d,uint16_t t) { auto* f=static_cast<File*>(h); return promote(f,true) && hostfs_platform::set_time(f->raw,d,t); }
bool get_time(void* h,uint16_t* d,uint16_t* t) { auto* f=static_cast<File*>(h); return promote(f,false) && hostfs_platform::get_time(f->raw,d,t); }
bool set_attr(const std::string& p,uint8_t a) { auto m=mapping(p); if(hidden(m)) return false; if(m.active && !copyToSave(p,m)) return false; return hostfs_platform::set_attr(m.active?m.saved:p,a); }
}
