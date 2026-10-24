// SPDX-License-Identifier: MIT
#import "Keyboard.h"
#include <map>
#include <vector>
#include <algorithm>

#include <set>
#import <QuartzCore/QuartzCore.h>
struct Key { const char* name; int scan; float width=1; };
static const std::vector<std::vector<Key>> rows = {
    {{"ESC",0},{"F1",0x62},{"F2",0x63},{"F3",0x64},{"F4",0x65},{"F5",0x66},{"F6",0x67},{"F7",0x68},{"F8",0x69},{"F9",0x6A},{"F10",0x6B},{"STOP",0x60},{"COPY",0x61}},
    {{"1",1},{"2",2},{"3",3},{"4",4},{"5",5},{"6",6},{"7",7},{"8",8},{"9",9},{"0",10},{"-",11},{"^",12},{"¥",13},{"BS",14,1.4}},
    {{"TAB",15,1.4},{"Q",16},{"W",17},{"E",18},{"R",19},{"T",20},{"Y",21},{"U",22},{"I",23},{"O",24},{"P",25},{"@",26},{"[",27},{"RET",28,1.4}},
    {{"CTRL",0x74,1.4},{"A",29},{"S",30},{"D",31},{"F",32},{"G",33},{"H",34},{"J",35},{"K",36},{"L",37},{";",38},{":",39},{"]",40},{"RET",28,1.4}},
    {{"SHIFT",0x70,1.8},{"Z",41},{"X",42},{"C",43},{"V",44},{"B",45},{"N",46},{"M",47},{",",48},{".",49},{"/",50},{"_",51},{"SHIFT",0x70,1.8}},
    {{"CAPS",0x71},{"かな",0x72},{"GRPH",0x73},{"NFER",0x51},{"SPACE",0x34,4},{"XFER",0x35},{"↑",0x3A},{"←",0x3B},{"↓",0x3D},{"→",0x3C}},
    {{"HELP",0x3F},{"HOME",0x3E},{"INS",0x38},{"DEL",0x39},{"R↓",0x37},{"R↑",0x36},{"7",0x42},{"8",0x43},{"9",0x44},{"/",0x41},{"*",0x45},{"-",0x40}},
    {{"4",0x46},{"5",0x47},{"6",0x48},{"+",0x49},{"1",0x4A},{"2",0x4B},{"3",0x4C},{"=",0x4D},{"0",0x4E},{",",0x4F},{".",0x50},{"RET",28}}
};
@interface PC98KeyButton : UIButton
@property(copy) BOOL (^activate)(void);
@end
@implementation PC98KeyButton
- (BOOL)accessibilityActivate { return self.activate ? self.activate() : NO; }
@end
@interface PC98Keyboard () {
    std::map<UITouch*,UIButton*> touches;
    std::map<int,int> counts;
    std::set<int> latched;
    std::map<int,double> repeatAt;
}
@property(strong) NSArray<NSArray<UIButton*>*>* keys;
@end
@implementation PC98Keyboard
- (instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])) {
        self.multipleTouchEnabled=YES; self.backgroundColor=[UIColor colorWithWhite:0.10 alpha:1];
        NSMutableArray* groups=[NSMutableArray new];
        for(const auto& row:rows) {
            NSMutableArray* group=[NSMutableArray new];
            for(const auto& k:row) {
                PC98KeyButton* b=[PC98KeyButton buttonWithType:UIButtonTypeCustom]; b.tag=k.scan;
                [b setTitle:[NSString stringWithUTF8String:k.name] forState:UIControlStateNormal];
                b.titleLabel.font=[UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightSemibold];
                b.titleLabel.adjustsFontSizeToFitWidth=YES; b.titleLabel.minimumScaleFactor=0.5;
                b.backgroundColor=[UIColor colorWithWhite:0.24 alpha:1]; b.layer.cornerRadius=5;
                b.userInteractionEnabled=NO; b.accessibilityLabel=[NSString stringWithFormat:@"PC98 %@",b.currentTitle];
                b.isAccessibilityElement=YES; b.accessibilityTraits=UIAccessibilityTraitKeyboardKey;
                __weak PC98Keyboard* weak=self; __weak UIButton* weakButton=b;
                b.activate=^BOOL { PC98Keyboard* keyboard=weak; UIButton* button=weakButton; if(!keyboard || !button) return NO; [keyboard updateKey:button down:YES]; [keyboard updateKey:button down:NO]; return YES; };
                [self addSubview:b]; [group addObject:b];
            }
            [groups addObject:group];
        }
        self.keys=groups;
    }
    return self;
}
- (void)layoutSubviews {
    [super layoutSubviews]; CGFloat gap=4, h=(self.bounds.size.height-gap*(rows.size()+1))/rows.size();
    for(size_t r=0;r<rows.size();++r) {
        float units=0; for(auto k:rows[r]) units+=k.width;
        CGFloat unit=(self.bounds.size.width-gap*(rows[r].size()+1))/units, x=gap;
        for(size_t c=0;c<rows[r].size();++c) {
            CGFloat w=unit*rows[r][c].width;
            self.keys[r][c].frame=CGRectMake(x,gap+r*(h+gap),w,h); x+=w+gap;
        }
    }
}
- (BOOL)isLatch:(int)scan { return scan==0x70 || scan==0x71 || scan==0x72 || scan==0x73 || scan==0x74; }
- (void)updateKey:(UIButton*)key down:(BOOL)down {
    int scan=(int)key.tag;
    if([self isLatch:scan]) {
        if(!down) return;
        if(latched.count(scan)) latched.erase(scan); else latched.insert(scan);
        if(self.keyChanged) self.keyChanged(scan,latched.count(scan)>0);
    } else {
        int previous=counts[scan]; counts[scan]=std::max(0,previous+(down?1:-1));
        if(!previous && down) repeatAt[scan]=CACurrentMediaTime()+0.45;
        if(!counts[scan]) repeatAt.erase(scan);
        if(self.keyChanged && ((previous==0)!=(counts[scan]==0))) self.keyChanged(scan,counts[scan]>0);
    }
    for(NSArray* row in self.keys) for(UIButton* b in row) if(b.tag==scan)
        b.backgroundColor=(counts[scan]>0 || latched.count(scan)) ? UIColor.systemBlueColor : [UIColor colorWithWhite:0.24 alpha:1];
}
- (void)touchesBegan:(NSSet<UITouch*>*)set withEvent:(UIEvent*)event {
    for(UITouch* t in set) for(NSArray* row in self.keys) for(UIButton* b in row)
        if(CGRectContainsPoint(b.frame,[t locationInView:self])) { touches[t]=b; [self updateKey:b down:YES]; }
}
- (void)touchesEnded:(NSSet<UITouch*>*)set withEvent:(UIEvent*)event {
    for(UITouch* t in set) { auto it=touches.find(t); if(it!=touches.end()) { [self updateKey:it->second down:NO]; touches.erase(it); } }
}
- (void)touchesCancelled:(NSSet<UITouch*>*)set withEvent:(UIEvent*)event { [self touchesEnded:set withEvent:event]; }
- (void)repeatHeldKeys {
    double now=CACurrentMediaTime();
    for(auto& pair:repeatAt) if(now>=pair.second) { if(self.keyChanged) self.keyChanged(pair.first,YES); pair.second=now+0.05; }
}
#ifdef PC98PLAYER_TESTING
- (BOOL)tapScanForTesting:(int)scan {
    for(NSArray* row in self.keys) for(UIButton* b in row) if(b.tag==scan) return [b accessibilityActivate];
    return NO;
}
#endif
- (void)releaseAll {
    std::set<int> down=latched; for(auto pair:counts) if(pair.second) down.insert(pair.first);
    for(int scan:down) if(self.keyChanged) self.keyChanged(scan,NO);
    touches.clear(); counts.clear(); latched.clear(); repeatAt.clear();
    for(NSArray* row in self.keys) for(UIButton* b in row) b.backgroundColor=[UIColor colorWithWhite:0.24 alpha:1];
}
@end
int pc98_scan_from_hid(NSInteger u) {
    static const int letters[]={29,45,43,31,18,32,33,34,23,35,36,37,47,46,24,25,16,19,30,20,22,44,17,42,21,41};
    if(u>=4 && u<=29) return letters[u-4];
    if(u>=30 && u<=38) return (int)u-29;
    if(u==39) return 10;
    if(u>=58 && u<=67) return 0x62+(int)u-58;
    switch(u) {
        case 40:case 88:return 28; case 41:return 0; case 42:return 14; case 43:return 15;
        case 44:return 0x34; case 45:return 11; case 46:return 12; case 47:return 26; case 48:return 27;
        case 49:return 40; case 51:return 38; case 52:return 39; case 54:return 48; case 55:return 49; case 56:return 50;
        case 57:return 0x71; case 70:return 0x61; case 72:return 0x60;
        case 73:return 0x38; case 74:return 0x3E; case 75:return 0x37; case 76:return 0x39; case 77:return 0x3F; case 78:return 0x36;
        case 79:return 0x3C; case 80:return 0x3B; case 81:return 0x3D; case 82:return 0x3A;
        case 84:return 0x41; case 85:return 0x45; case 86:return 0x40; case 87:return 0x49;
        case 89:return 0x4A; case 90:return 0x4B; case 91:return 0x4C; case 92:return 0x46; case 93:return 0x47; case 94:return 0x48;
        case 95:return 0x42; case 96:return 0x43; case 97:return 0x44; case 98:return 0x4E; case 99:return 0x50;
        case 135:return 0x33; case 137:return 13; case 138:return 0x35; case 139:return 0x51;
        case 144:return 0x72; case 145:return 0x51;
        case 224:case 228:return 0x74; case 225:case 229:return 0x70; case 226:case 230:return 0x73;
        default:return -1;
    }
}
