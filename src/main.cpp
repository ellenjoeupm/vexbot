#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

static bool g_botOn = false;
static bool g_botUsed = false;
static bool g_holding = false;

class $modify(VexKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        if (down && !repeat && key == KEY_F2) {
            g_botOn = !g_botOn;
            Notification::create(
                g_botOn ? "VexBot: ON" : "VexBot: OFF",
                NotificationIcon::Info
            )->show();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
    }
};

class $modify(VexGameLayer, GJBaseGameLayer) {
    bool shouldJump() {
        auto p = m_player1;
        if (!p || p->m_isDead) return false;

        float px = p->getPositionX();
        float py = p->getPositionY();

        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (obj->m_objectType != GameObjectType::Hazard) continue;
            float dx = obj->getPositionX() - px;
            float dy = std::abs(obj->getPositionY() - py);
            if (dx > 0.f && dx < 45.f && dy < 30.f) return true;
        }
        return false;
    }

    void processCommands(float dt) {
        if (g_botOn && PlayLayer::get()) {
            bool want = shouldJump();
            if (want != g_holding) {
                g_holding = want;
                g_botUsed = true;
                this->handleButton(want, 1, true);
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

class $modify(VexPlayLayer, PlayLayer) {
    void resetLevel() {
        g_botUsed = false;
        g_holding = false;
        PlayLayer::resetLevel();
    }

    void levelComplete() {
        bool safe = Mod::get()->getSettingValue<bool>("safe-mode");
        if (g_botUsed && safe) {
            m_isTestMode = true;
        }

        PlayLayer::levelComplete();

        if (g_botUsed) {
            auto win = CCDirector::get()->getWinSize();
            auto label = CCLabelBMFont::create("VexBot", "bigFont.fnt");
            label->setAnchorPoint({1.f, 0.f});
            label->setPosition({win.width - 8.f, 8.f});
            label->setScale(0.5f);
            label->setOpacity(200);
            this->addChild(label, 1000);
        }
    }
};
