#pragma once
#include "DisplayManager.h"
#include "IconManager.h" 
#include <map>

extern IconManager iconManager;

// --- FARBEN ---
#define COL_WHITE      0xFFFF
#define COL_BLACK      0x0000
#define COL_RED        0xF800
#define COL_GREEN      0x07E0
#define COL_BLUE       0x001F

// Funktionale Farben
#define COL_HIGHLIGHT  0xFD20 
#define COL_WARN       0xF800 
#define COL_SUCCESS    0x07E0 
#define COL_INFO       0x03EF 
#define COL_MUTED      0x8410 
#define COL_WARM       0xFE60 
#define COL_COLD       0x841F 

// --- PREMIUM PALETTE ---
#define COL_GOLD       0xFEA0 
#define COL_SILVER     0x9492 

// --- NEUE NEON PALETTE ---
#define COL_NEON_PINK   0xF81F  
#define COL_NEON_CYAN   0x07FF  
#define COL_NEON_GREEN  0x07E0  
#define COL_PURPLE      0x780F  
#define COL_ORANGE      0xFD20  
#define COL_MAGENTA     0xF81F  

// --- SOFT PALETTE (Pastell) ---
#define COL_SOFT_ROSE     0xFDB8  
#define COL_SOFT_SKY      0x867D  
#define COL_SOFT_MINT     0x9FF3  
#define COL_SOFT_LAVENDER 0xE73F  
#define COL_SOFT_PEACH    0xFED6  
#define COL_SOFT_LEMON    0xFFF4  

struct FontPair {
    const uint8_t* regular;
    const uint8_t* bold;
    int8_t iconOffsetY;  
    uint8_t lineHeight;
    uint8_t baselineOffset; // Dient uns als Versalhöhe (Cap Height)
};

struct RenderState {
    uint16_t color;
    bool bold;
    bool underlined;
};

class RichText {
private:
    const uint8_t* iconFont = u8g2_font_unifont_t_symbols;

    FontPair getFontByName(String name) {
        if (name.equalsIgnoreCase("Small")) {
            return { u8g2_font_helvR10_tf, u8g2_font_helvB10_tf, -1, 14, 11 };
        }
        if (name.equalsIgnoreCase("Medium")) {
            return { u8g2_font_helvR12_tf, u8g2_font_helvB12_tf, -2, 16, 13 };
        }
        if (name.equalsIgnoreCase("Large")) {
            return { u8g2_font_helvR18_tf, u8g2_font_helvB18_tf, -4, 24, 19 };
        }
        return { u8g2_font_helvR12_tf, u8g2_font_helvB12_tf, -2, 16, 13 };
    }

    uint16_t parseHexColor(DisplayManager& d, String hex) {
        if (hex.startsWith("#")) hex.remove(0, 1);
        long number = strtol(hex.c_str(), NULL, 16);
        return d.color565((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
    }

    uint16_t getColorByName(DisplayManager& d, String name) {
        if (name.startsWith("#")) return parseHexColor(d, name);
        if (name == "white")   return COL_WHITE;
        if (name == "red")     return COL_RED;
        if (name == "green")   return COL_GREEN;
        if (name == "blue")    return COL_BLUE;
        
        if (name == "highlight") return COL_HIGHLIGHT;
        if (name == "warn")      return COL_WARN;
        if (name == "success")   return COL_SUCCESS;
        if (name == "info")      return COL_INFO;
        if (name == "muted")     return COL_MUTED;
        if (name == "warm")      return COL_WARM;
        if (name == "cold")      return COL_COLD;
        
        if (name == "gold")      return COL_GOLD;
        if (name == "silver")    return COL_SILVER;
        
        if (name == "pink")      return COL_NEON_PINK;
        if (name == "cyan")      return COL_NEON_CYAN;
        if (name == "lime")      return COL_NEON_GREEN;
        if (name == "purple")    return COL_PURPLE;
        if (name == "orange")    return COL_ORANGE;
        if (name == "magenta")   return COL_MAGENTA;
        
        if (name == "rose")      return COL_SOFT_ROSE;
        if (name == "sky")       return COL_SOFT_SKY;
        if (name == "mint")      return COL_SOFT_MINT;
        if (name == "lavender")  return COL_SOFT_LAVENDER;
        if (name == "peach")     return COL_SOFT_PEACH;
        if (name == "lemon")     return COL_SOFT_LEMON;

        return COL_WHITE;
    }

    String getIconCode(String name) {
        // --- KLIMA & WETTER ---
        if (name == "sun")      return "\u2600";
        if (name == "cloud")    return "\u2601";
        if (name == "rain")     return "\u2602";
        if (name == "snow")     return "\u2603";
        if (name == "zap")      return "\u26A1";
        
        // --- FIXES ---
        if (name == "water")    return "\u2614";
        if (name == "drop")     return "\u2614";
        
        if (name == "co2")      return "\u2622";
        if (name == "gas")      return "\u2622";
        if (name == "bio")      return "\u2623";
        
        if (name == "temp")     return "\u263C";
        if (name == "flame")    return "\u263C";

        // --- HAUS & LEBEN ---
        if (name == "house")    return "\u2302";
        if (name == "coffee")   return "\u2615";
        if (name == "cup")      return "\u2615";
        if (name == "music")    return "\u266B";
        if (name == "heart")    return "\u2665";
        if (name == "star")     return "\u2605";
        
        // --- TECHNIK & GERÄTE ---
        if (name == "phone")      return "\u260E";
        if (name == "smartphone") return "\u260E";
        if (name == "print")      return "\u2709";
        if (name == "printer")    return "\u2709";
        if (name == "bulb")       return "\u25CF";
        if (name == "light")      return "\u263C";
        if (name == "wifi")       return "\u260E";
        if (name == "power")      return "\u23E9";
        if (name == "car")        return "\u2638";
        if (name == "battery")    return "\u25A4";
        if (name == "gear")       return "\u2699";
        if (name == "switch")     return "\u2611";
        if (name == "siren")      return "\u26A0";
        if (name == "stopwatch")  return "\u231B";

        // --- PERSONEN ---
        if (name == "man")      return "\u2642";
        if (name == "woman")    return "\u2640";
        if (name == "person")   return "\u265F";
        if (name == "smile")    return "\u263A";

        // --- PFEILE ---
        if (name == "arrow_u")  return "\u2191"; 
        if (name == "arrow_d")  return "\u2193"; 
        if (name == "arrow_l")  return "\u2190"; 
        if (name == "arrow_r")  return "\u2192"; 
        if (name == "check")    return "\u2713"; 

        return "?"; 
    }

    String processTag(DisplayManager& d, String tag, RenderState& state, bool& isIcon, bool& isBitmapIcon, String& bitmapName) {
        isIcon = false;
        isBitmapIcon = false;
        bitmapName = "";

        if (tag == "b") { state.bold = !state.bold; return ""; }
        if (tag == "u") { state.underlined = !state.underlined; return ""; }
        
        if (tag.startsWith("c:")) { 
            state.color = getColorByName(d, tag.substring(2)); 
            return ""; 
        }

        if (tag.startsWith("icon:")) {
            isBitmapIcon = true;
            bitmapName = tag.substring(5);
            return "";
        }

        isIcon = true;
        return getIconCode(tag);
    }

    int drawPart(DisplayManager& d, int x, int y, String text, bool isIcon, bool isBitmapIcon, String bitmapName, FontPair fonts, RenderState state) {
        d.setTextColor(state.color);
        
        if (isBitmapIcon) {
            // --- NEU: Dynamische Zentrierung ---
            int iconW = iconManager.getIconWidth(bitmapName);
            int iconH = iconManager.getIconHeight(bitmapName);
            
            // Berechnung:
            // 1. fonts.baselineOffset ist die Höhe eines Großbuchstabens über der Baseline (z.B. 13px)
            // 2. Wir wollen die Mitte des Icons auf die Mitte des Buchstabens legen.
            // 3. Mitte Buchstabe = y - (baselineOffset / 2)
            // 4. Oberkante Icon = Mitte Buchstabe - (iconH / 2)
            
            int yCentered = y - (fonts.baselineOffset / 2) - (iconH / 2);
            
            iconManager.drawIcon(d, x, yCentered, bitmapName);
            return iconW + 1; // Dynamische Breite + 1px Padding
        }
        else if (isIcon) {
            d.setU8g2Font(iconFont);
            d.drawString(x, y + fonts.iconOffsetY, text, state.color);
            return d.getTextWidth(text);
        } else {
            d.setU8g2Font(state.bold ? fonts.bold : fonts.regular);
            d.drawString(x, y, text, state.color);
            int w = d.getTextWidth(text);
            if (state.underlined) d.drawFastHLine(x, y + 2, w, state.color);
            return w;
        }
    }

    int measurePart(DisplayManager& d, String text, bool isIcon, bool isBitmapIcon, String bitmapName, FontPair fonts, bool bold) {
        if (isBitmapIcon) {
            // --- NEU: Dynamische Breite abfragen ---
            return iconManager.getIconWidth(bitmapName) + 1;
        }
        if (isIcon) {
            d.setU8g2Font(iconFont);
        } else {
            d.setU8g2Font(bold ? fonts.bold : fonts.regular);
        }
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
                bool isBitmapIcon;
                String bitmapName;
                
                String content = processTag(d, tag, state, isIcon, isBitmapIcon, bitmapName);
                
                if(isIcon || isBitmapIcon) {
                    totalW += measurePart(d, content, isIcon, isBitmapIcon, bitmapName, fonts, state.bold);
                }
                i = end + 1;
            } else {
                int nextTag = text.indexOf('{', i);
                if(nextTag == -1) nextTag = len;
                String part = text.substring(i, nextTag);
                totalW += measurePart(d, part, false, false, "", fonts, state.bold);
                i = nextTag;
            }
        }
        return totalW;
    }

    void drawCentered(DisplayManager& d, int y, String text, String fontName, uint16_t defaultColor = COL_WHITE) {
        int totalW = getTextWidth(d, text, fontName);
        drawString(d, (M_WIDTH - totalW) / 2, y, text, fontName, defaultColor);
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
                bool isBitmapIcon;
                String bitmapName;
                
                String content = processTag(d, tag, state, isIcon, isBitmapIcon, bitmapName);
                if(content != "" || isBitmapIcon) {
                     cursorX += drawPart(d, cursorX, y, content, isIcon, isBitmapIcon, bitmapName, fonts, state);
                }
                i = end + 1;
            } else {
                int nextTag = text.indexOf('{', i);
                if(nextTag == -1) nextTag = len;
                String part = text.substring(i, nextTag);
                cursorX += drawPart(d, cursorX, y, part, false, false, "", fonts, state);
                i = nextTag;
            }
        }
    }
    
    void drawBox(DisplayManager& d, int x, int y, int width, String text, String fontName, uint16_t defaultColor = COL_WHITE) {
        FontPair fonts = getFontByName(fontName);
        RenderState state = {defaultColor, false, false};
        int startX = x;
        int cursorX = x;
        int cursorY = y;
        int len = text.length();
        int i = 0;
        while(i < len) {
            if(text[i] == '{') {
                int end = text.indexOf('}', i);
                if(end == -1) break;
                String tag = text.substring(i+1, end);
                
                bool isIcon;
                bool isBitmapIcon;
                String bitmapName;

                String content = processTag(d, tag, state, isIcon, isBitmapIcon, bitmapName);
                if(isIcon || isBitmapIcon) {
                    int w = measurePart(d, content, isIcon, isBitmapIcon, bitmapName, fonts, state.bold);
                    if (cursorX + w > startX + width) { cursorX = startX; cursorY += fonts.lineHeight; }
                    drawPart(d, cursorX, cursorY, content, isIcon, isBitmapIcon, bitmapName, fonts, state);
                    cursorX += w;
                }
                i = end + 1;
            } else {
                int nextTag = text.indexOf('{', i);
                int nextSpace = text.indexOf(' ', i);
                int endOfWord = len;
                if (nextTag != -1 && nextTag < endOfWord) endOfWord = nextTag;
                if (nextSpace != -1 && nextSpace < endOfWord) endOfWord = nextSpace;
                bool isSpace = (endOfWord == nextSpace);
                String word = text.substring(i, endOfWord);
                
                int w = measurePart(d, word, false, false, "", fonts, state.bold);
                if (cursorX + w > startX + width) { cursorX = startX; cursorY += fonts.lineHeight; }
                drawPart(d, cursorX, cursorY, word, false, false, "", fonts, state);
                cursorX += w;
                
                if (isSpace) {
                     int spaceW = measurePart(d, " ", false, false, "", fonts, state.bold);
                     if (cursorX + spaceW <= startX + width) cursorX += spaceW;
                     i = endOfWord + 1;
                } else i = endOfWord;
            }
        }
    }
};