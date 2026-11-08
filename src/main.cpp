#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

static bool g_botOn = false;
static bool g_botUsed = false;
static bool g_modded = false;
static bool g_holding = false;
static int g_holdLeft = 0;

static double g_origInterval = -1.0;
static float g_lastSpeed = 1.f;
static bool g_lastOverride = false;
static int g_lastFps = 0;

static void applyTiming() {
    auto mod = Mod::get();
    float speed = static_cast<float>(mod->getSettingValue<double>("speed"));
    bool ov = mod->getSettingValue<bool>("override-fps");
    int fps = static_cast<int>(mod->getSettingValue<int64_t>("fps"));

    if (g_origInterval < 0.0) {
        g_origInterval = CCDirector::get()->getAnimationInterval();
    }
    if (speed != g_lastSpeed) {
        CCDirector::get()->getScheduler()->setTimeScale(speed);
        g_lastSpeed = speed;
    }
    if (ov != g_lastOverride || fps != g_lastFps) {
        CCApplication::sharedApplication()->setAnimationInterval(
            ov ? 1.0 / fps : g_origInterval
        );
        g_lastOverride = ov;
        g_lastFps = fps;
    }
    g_modded = (speed != 1.f) || ov;
}

static void resetTiming() {
    if (g_origInterval > 0.0) {
        CCApplication::sharedApplication()->setAnimationInterval(g_origInterval);
    }
    CCDirector::get()->getScheduler()->setTimeScale(1.f);
    g_lastSpeed = 1.f;
    g_lastOverride = false;
    g_lastFps = 0;
    g_modded = false;
}

class $modify(VexKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && key == KEY_F2) {
            g_botOn = !g_botOn;
            Notification::create(
                g_botOn ? "VexBot: ON" : "VexBot: OFF",
                NotificationIcon::Info
            )->show();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

class $modify(VexGameLayer, GJBaseGameLayer) {
    bool shouldJump() {
        auto p = m_player1;
        if (!p || p->m_isDead) return false;

        float look = static_cast<float>(Mod::get()->getSettingValue<double>("lookahead"));
        float yTol = static_cast<float>(Mod::get()->getSettingValue<double>("y-tolerance"));

        float px = p->getPositionX();
        float py = p->getPositionY();

        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (obj->m_objectType != GameObjectType::Hazard) continue;
            float dx = obj->getPositionX() - px;
            float dy = std::abs(obj->getPositionY() - py);
            if (dx > 0.f && dx < look && dy < yTol) return true;
        }
        return false;
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (PlayLayer::get()) {
            applyTiming();

            if (g_botOn) {
                int holdTicks = static_cast<int>(Mod::get()->getSettingValue<int64_t>("hold-ticks"));
                if (shouldJump()) g_holdLeft = holdTicks;

                bool press = g_holdLeft > 0;
                if (g_holdLeft > 0) g_holdLeft--;

                if (press != g_holding) {
                    g_holding = press;
                    g_botUsed = true;
                    this->handleButton(press, 1, true);
                }
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(VexPlayLayer, PlayLayer) {
    void resetLevel() {
        g_botUsed = false;
        g_holding = false;
        g_holdLeft = 0;
        PlayLayer::resetLevel();
    }

    void onQuit() {
        resetTiming();
        g_botOn = false;
        PlayLayer::onQuit();
    }

    void levelComplete() {
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
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(VexPause::onVexBot)
        );
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({win.width - 70.f, 35.f});
        this->addChild(menu, 100);
    }

    void onVexBot(CCObject*) {
        openSettingsPopup(Mod::get());
    }
};
