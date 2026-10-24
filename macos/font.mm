// SPDX-License-Identifier: MIT
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#include "font.h"
#include "machine.h"
#include <cstring>

static NSString* fontName;
static bool fontShift;
static NSString* sjis(const uint8_t* bytes, size_t n) {
    CFStringRef s = CFStringCreateWithBytes(nullptr, bytes, n,
        kCFStringEncodingDOSJapanese, false);
    return CFBridgingRelease(s);
}
static NSString* jisString(uint16_t jis) {
    int row = (jis >> 8) - 0x21, cell = (jis & 255) - 0x21;
    if (row < 0 || row >= 94 || cell < 0 || cell >= 94) return nil;
    int lead = row / 2 + 0x81;
    if (lead > 0x9F) lead += 0x40;
    int trail = (row & 1) ? cell + 0x9F : cell + 0x40;
    if (!(row & 1) && trail >= 0x7F) ++trail;
    uint8_t bytes[] = {(uint8_t)lead, (uint8_t)trail};
    return sjis(bytes, 2);
}
static void raster(NSString* text, int width, uint8_t* out) {
    memset(out, 0, width * 2);
    if (!text.length) return;
    @autoreleasepool {
        uint8_t pixels[16 * 16] = {};
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
        CGContextRef ctx = CGBitmapContextCreate(pixels, 16, 16, 8, 16, cs, kCGImageAlphaNone);
        CGColorSpaceRelease(cs);
        if (!ctx) return;
        CTFontRef font = CTFontCreateWithName((__bridge CFStringRef)fontName, 16, nullptr);
        NSDictionary* attrs = @{(__bridge NSString*)kCTFontAttributeName: (__bridge id)font,
            (__bridge NSString*)kCTForegroundColorFromContextAttributeName: @YES};
        NSAttributedString* str = [[NSAttributedString alloc] initWithString:text attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)str);
        double advance = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
        double available = width - ((fontShift && width == 8) ? 1 : 0);
        double sx = advance > available ? available / advance : 1;
        CGContextSetShouldAntialias(ctx, false);
        CGContextSetShouldSmoothFonts(ctx, false);
        CGContextSetGrayFillColor(ctx, 1, 1);
        CGContextScaleCTM(ctx, sx, 1);
        CGContextSetTextPosition(ctx, (fontShift && width == 8) ? 1 / sx : 0, 3);
        CTLineDraw(line, ctx);
        // Bitmap storage begins with the top scanline.
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < width; ++x)
                if (pixels[y * 16 + x] >= 128)
                    out[y * (width / 8) + x / 8] |= (uint8_t)(0x80 >> (x & 7));
        CFRelease(line); CFRelease(font); CGContextRelease(ctx);
    }
}
static void kanji(void*, uint16_t jis, uint8_t out[32]) { raster(jisString(jis), 16, out); }
static void ank(void*, uint8_t c, uint8_t out[16]) {
    raster(c == 0x5C ? @"¥" : sjis(&c, 1), 8, out);
}
static void narrow(void*, uint16_t jis, uint8_t out[16]) { raster(jisString(jis), 8, out); }
void mac_install_font(const std::string& name, bool shift) {
    fontName = name.empty() ? @"HiraginoSans-W3" : [NSString stringWithUTF8String:name.c_str()];
    fontShift = shift;
    video_set_font({kanji, ank, narrow, nullptr});
}
