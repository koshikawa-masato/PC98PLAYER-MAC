// SPDX-License-Identifier: MIT
#include "player.h"
#include "hostfs.h"
#include "opna_renderer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unistd.h>

static void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
int main(int argc,char** argv) {
    char temporary[] = "/tmp/pc98player-test-XXXXXX";
    if (!mkdtemp(temporary)) return 1;
    std::filesystem::path root=std::filesystem::path(temporary)/"cache";
    std::filesystem::create_directories(root);
    bool overlay=argc>1 && std::string(argv[1])=="--overlay";
    std::unique_ptr<Player> p;
    try {
        // A real 8086 COM: fill text VRAM with a green 'A', then loop.
        // This exercises loading, CPU instructions, video and snapshot restoration.
        const unsigned char program[] = {
            0xB8,0x00,0xA0,0x8E,0xC0, // mov ax,A000; mov es,ax
            0x31,0xFF,0xB8,0x41,0x00,0xB9,0xD0,0x07,0xF3,0xAB,
            0xBF,0x00,0x20,0xB8,0x81,0x00,0xB9,0xD0,0x07,0xF3,0xAB,
            0xEB,0xFE
        };
        FILE* f = fopen((root/"TEST.COM").c_str(),"wb"); check(f,"open COM");
        check(fwrite(program,1,sizeof(program),f)==sizeof(program),"write COM"); fclose(f);
        f = fopen((root/"PC98PLAYER.INI").c_str(),"wb"); check(f,"open INI");
        fputs("[PC98PLAYER]\nStart=TEST.COM\nMIDI=0\nRhythmROM=0\n",f); fclose(f);
        Ini ini; check(ini.load((root/"PC98PLAYER.INI").string()),"INI read");
        PlayerSettings s; check(player_settings_from_ini(ini,root.string(),&s),"INI settings");
        if(overlay) hostfs::set_overlay(root.string(),(std::filesystem::path(temporary)/"saves").string());
        p = std::make_unique<Player>(); std::string error;
        check(p->init(s,&error),"player init");
        for (int i=0;i<5;++i) p->run_frame();
        check(p->m->quit==0,"COM still running");
        check(p->m->tvram[0]=='A' && p->m->tvram[0x2000]==0x81,"COM wrote video RAM");
        check(!p->audio.empty() && p->audio.size()%2==0,"stereo PCM generation");
        if (OpnaRenderer::available()) {
            // SSG channel A: audible square wave; confirms the linked ymfm synthesizer.
            OpnaRenderer synth; synth.init(MASTER_CLOCK,44100);
            synth.write(0,0,0x80); synth.write(0,1,0);
            synth.write(0,7,0x3E); synth.write(0,8,15);
            double energy=0;
            for(int i=0;i<2048;++i) { float l=0,r=0; synth.render_one(&l,&r); energy+=std::abs(l)+std::abs(r); }
            check(energy>0.01,"ymfm SSG produces nonzero samples");
        }
        machine_key(p->m,0x1D,true); check(p->m->kb_down[0x1D],"key down");
        machine_key(p->m,0x1D,false); check(!p->m->kb_down[0x1D],"key up");
        machine_mouse(p->m,3,-2,1);
        check(p->save_state(0,&error),"save state");
        p->m->ram[0x90000]=123;
        check(p->load_state(0,&error),"load state");
        check(p->m->ram[0x90000]!=123,"RAM restored");
        p->m->tvram[0]='Z'; check(p->load_state(0,&error),"reload state");
        check(p->m->tvram[0]=='A',"VRAM restored");
        const std::string japanese = "日本語.TXT";
        check(hostfs::from_sjis(hostfs::to_sjis(japanese))==japanese,"CP932 filename roundtrip");
        p->shutdown(); p.reset();
        if(overlay) check(!std::filesystem::exists(root/"PC98PLAYER.SAV"),"state stored outside disposable cache");
        hostfs::set_overlay("",""); std::filesystem::remove_all(temporary);
        puts("PASS: DOS COM, CPU, VRAM, PCM, keyboard, mouse, state save/load, CP932"); return 0;
    } catch(const std::exception& e) {
        fprintf(stderr,"FAIL: %s\n",e.what());
        if(p) p->shutdown(); hostfs::set_overlay("",""); std::filesystem::remove_all(temporary); return 1;
    }
}
