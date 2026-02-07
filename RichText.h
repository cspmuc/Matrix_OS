#pragma once
#include "DisplayManager.h"
#include <map>
#include "EmojiEngine.h"
extern EmojiEngine emojiEngine; 

// --- FARBEN ---
#define COL_WHITE 0xFFFF
#define COL_BLACK 0x0000
#define COL_RED   0xF800
#define COL_GREEN 0x07E0
#define COL_BLUE  0x001F
#define COL_GOLD  0xFEA0 
#define COL_SILVER 0x9492 

// Neon & Soft Farben aus deinem Code hier denken... (um Platz zu sparen)
#define COL_HIGHLIGHT 0xFD20 
#define COL_WARN 0xF800 
#define COL_SUCCESS 0x07E0 
#define COL_INFO 0x03EF 
#define COL_MUTED 0x8410 
#define COL_WARM 0xFE60 
#define COL_COLD 0x841F 

struct FontPair { const uint8_t* regular; const uint8_t* bold; int8_t iconOffsetY; uint8_t lineHeight; uint8_t baselineOffset; };
struct RenderState { uint16_t color; bool bold; bool underlined; };

class RichText {
private:
    FontPair getFontByName(String name) {
        if (name.equalsIgnoreCase("Small"))  return { u8g2_font_helvR10_tf, u8g2_font_helvB10_tf, -1, 14, 11 };
        if (name.equalsIgnoreCase("Medium")) return { u8g2_font_helvR12_tf, u8g2_font_helvB12_tf, -2, 16, 13 };
        return { u8g2_font_helvR12_tf, u8g2_font_helvB12_tf, -2, 16, 13 };
    }

    uint16_t parseHexColor(DisplayManager& d, String hex) {
        if (hex.startsWith("#")) hex.remove(0, 1);
        long number = strtol(hex.c_str(), NULL, 16);
        return d.color565((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
    }

    uint16_t getColorByName(DisplayManager& d, String name) {
        if (name.startsWith("#")) return parseHexColor(d, name);
        if (name == "white") return COL_WHITE;
        if (name == "red") return COL_RED;
        if (name == "gold") return COL_GOLD;
        if (name == "highlight") return COL_HIGHLIGHT;
        // ... weitere Farben ...
        return COL_WHITE;
    }

    String processTag(DisplayManager& d, String tag, RenderState& state, bool& isIcon) {
        isIcon = false;
        if (tag == "b") { state.bold = !state.bold; return ""; }
        if (tag == "u") { state.underlined = !state.underlined; return ""; }
        if (tag.startsWith("c:")) { state.color = getColorByName(d, tag.substring(2)); return ""; }
        
        // ALLES was kein Format-Tag ist, ist jetzt ein Emoji-Name!
        isIcon = true;
        return tag; // Gibt z.B. "smile" zurück
    }

    int drawPart(DisplayManager& d, int x, int y, String text, bool isIcon, FontPair fonts, RenderState state) {
        d.setTextColor(state.color);
        if (isIcon) {
            // Hier wird die Bitmap Engine gerufen
            emojiEngine.drawIcon(d, text, x, y + fonts.iconOffsetY);
            return 17; // Platzhalter Breite 16px + 1px Space
        } else {
            d.setU8g2Font(state.bold ? fonts.bold : fonts.regular);
            d.drawString(x, y, text, state.color);
            int w = d.getTextWidth(text);
            if (state.underlined) d.drawFastHLine(x, y + 2, w, state.color);
            return w;
        }
    }

    int measurePart(DisplayManager& d, String text, bool isIcon, FontPair fonts, bool bold) {
        if (isIcon) return 17;
        d.setU8g2Font(bold ? fonts.bold : fonts.regular);
        return d.getTextWidth(text);
    }

public:
    int getLineHeight(String fontName) { return getFontByName(fontName).lineHeight; }
    int getBaselineOffset(String fontName) { return getFontByName(fontName).baselineOffset; }

    int getTextWidth(DisplayManager& d, String text, String fontName) {
        FontPair fonts = getFontByName(fontName);
        int totalW = 0;
        RenderState state = {COL_WHITE, false, false};
        int len = text.length();
        int i = 0;
        while(i < len) {
            if(text[i] == '{') {
                int end = text.indexOf('}', i);
                if(end == -1) break;
                String tag = text.substring(i+1, end);
                bool isIcon;
                String content = processTag(d, tag, state, isIcon);
                if(isIcon) totalW += measurePart(d, content, true, fonts, state.bold);
                i = end + 1;
            } else {
                int nextTag = text.indexOf('{', i);
                if(nextTag == -1) nextTag = len;
                String part = text.substring(i, nextTag);
                totalW += measurePart(d, part, false, fonts, state.bold);
                i = nextTag;
            }
        }
        return totalW;
    }

    void drawString(DisplayManager& d, int x, int y, String text, String fontName, uint16_t defaultColor = COL_WHITE) {
        FontPair fonts = getFontByName(fontName);
        RenderState state = {defaultColor, false, false};
        int cursorX = x;
        int len = text.length();
        int i = 0;
        while(i < len) {
            if(text[i] == '{') {
                int end = text.indexOf('}', i);
                if(end == -1) break;
                String tag = text.substring(i+1, end);
                bool isIcon;
                String content = processTag(d, tag, state, isIcon);
                if(content != "") cursorX += drawPart(d, cursorX, y, content, true, fonts, state);
                i = end + 1;
            } else {
                int nextTag = text.indexOf('{', i);
                if(nextTag == -1) nextTag = len;
                String part = text.substring(i, nextTag);
                cursorX += drawPart(d, cursorX, y, part, false, fonts, state);
                i = nextTag;
            }
        }
    }
    
    void drawCentered(DisplayManager& d, int y, String text, String fontName, uint16_t defaultColor = COL_WHITE) {
        int totalW = getTextWidth(d, text, fontName);
        drawString(d, (M_WIDTH - totalW) / 2, y, text, fontName, defaultColor);
    }
};