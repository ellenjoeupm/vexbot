#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <sstream>

using namespace geode::prelude;

struct Inp { uint32_t tick; bool down; };

static std::vector<Inp> g_rec;    // lo que se está grabando
static std::vector<Inp> g_play;   // la macro a reproducir
static size_t g_idx = 0;
static bool g_recording = false;
static bool g_playing = false;
static bool g_botUsed = false;
static bool g_injecting = false;
static bool g_modded = false;

static double g_origInterval = -1.0;
static float g_lastSpeed = 1.f;
static bool g_lastOverride = false;
static int g_lastFps = 0;

static void applyTiming() {
    auto mod = Mod::get();
    float speed = static_cast<float>(mod->getSettingValue<double>("speed"));
    bool ov = mod->getSettingValue<bool>("override-fps");
    int fps = static_cast<int>(mod->getSettingValue<int64_t>("fps"));

    if (g_origInterval < 0.0) g_origInterval = CCDirector::get()->getAnimationInterval();
    if (speed != g_lastSpeed) {
        CCDirector::get()->getScheduler()->setTimeScale(speed);
        g_lastSpeed = speed;
    }
    if (ov != g_lastOverride || fps != g_lastFps) {
        CCApplication::sharedApplication()->setAnimationInterval(ov ? 1.0 / fps : g_origInterval);
        g_lastOverride = ov;
        g_lastFps = fps;
    }
    g_modded = (speed != 1.f) || ov;
}

static void resetTiming() {
    if (g_origInterval > 0.0) CCApplication::sharedApplication()->setAnimationInterval(g_origInterval);
    CCDirector::get()->getScheduler()->setTimeScale(1.f);
    g_lastSpeed = 1.f;
    g_lastOverride = false;
    g_lastFps = 0;
    g_modded = false;
}

static uint32_t tickOf(GJBaseGameLayer* l) {
    return l->m_gameState.m_currentProgress / 2 + 1;
}

static std::filesystem::path macroPath(PlayLayer* pl) {
    return Mod::get()->getSaveDir() / fmt::format("macro_{}.txt", pl->m_level->m_levelID.value());
}

static void saveMacro(PlayLayer* pl) {
    std::string out;
    for (auto& i : g_play) out += fmt::format("{} {}\n", i.tick, i.down ? 1 : 0);
    (void)utils::file::writeString(macroPath(pl), out);
}

static bool loadMacro(PlayLayer* pl) {
    auto r = utils::file::readString(macroPath(pl));
    if (r.isErr()) return false;
    g_play.clear();
    std::istringstream ss(r.unwrap());
    uint32_t t; int d;
    while (ss >> t >> d) g_play.push_back({t, d != 0});
    return !g_play.empty();
}

class $modify(VexKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
            if (key == KEY_F2) {
                g_playing = !g_playing;
                if (g_playing) {
                    g_recording = false;
                    auto pl = PlayLayer::get();
                    if (pl && !loadMacro(pl)) {
                        g_playing = false;
                        Notification::create("VexBot: no hay macro para este nivel", NotificationIcon::Error)->show();
                        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
                    }
                }
                Notification::create(g_playing ? "VexBot: reproduciendo" : "VexBot: OFF",
                                     NotificationIcon::Info)->show();
            } else if (key == KEY_F3) {
                g_recording = !g_recording;
                if (g_recording) g_playing = false;
                Notification::create(g_recording ? "VexBot: grabando" : "VexBot: grabación OFF",
                                     NotificationIcon::Info)->show();
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

class $modify(VexGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_recording && !g_injecting && button == 1 && isPlayer1 && PlayLayer::get()) {
            g_rec.push_back({tickOf(this), down});
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (PlayLayer::get()) {
            applyTiming();
            if (g_playing) {
                auto tick = tickOf(this);
                while (g_idx < g_play.size() && g_play[g_idx].tick <= tick) {
                    g_injecting = true;
                    GJBaseGameLayer::handleButton(g_play[g_idx].down, 1, true);
                    g_injecting = false;
                    g_botUsed = true;
                    g_idx++;
                }
            }
        }
    }
};

class $modify(VexPlayLayer, PlayLayer) {
    void resetLevel() {
        g_idx = 0;
        g_botUsed = false;
        if (g_recording) g_rec.clear();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        resetTiming();
        g_playing = false;
        g_recording = false;
        PlayLayer::onQuit();
    }

    void levelComplete() {
        if (g_recording && !m_isPracticeMode && !g_botUsed && !g_rec.empty()) {
            g_play = g_rec;
            saveMacro(this);
            Notification::create("VexBot: macro guardada", NotificationIcon::Success)->show();
        }

        bool safe = Mod::get()->getSettingValue<bool>("safe-mode");
        if ((g_botUsed || g_modded) && safe) {
            m_isTestMode = true;
        }

        PlayLayer::levelComplete();

        if (g_botUsed) {
            auto name = Mod::get()->getSettingValue<std::string>("bot-name");
            auto win = CCDirector::get()->getWinSize();
            auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            label->setAnchorPoint({1.f, 0.f});
            label->setPosition({win.width - 8.f, 8.f});
            label->setScale(0.5f);
            label->setOpacity(200);
            this->addChild(label, 1000);
        }
    }
};

class $modify(VexPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto win = CCDirector::get()->getWinSize();
        auto spr = ButtonSprite::create("VexBot");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(VexPause::onVexBot));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({win.width - 70.f, 35.f});
        this->addChild(menu, 100);
    }

    void onVexBot(CCObject*) {
        openSettingsPopup(Mod::get());
    }
};
