#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "SteamNewsLayer.hpp"

using namespace geode::prelude;

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto bottomMenu = getChildByID("bottom-menu");
        if (bottomMenu) {
            bottomMenu->addChild(CCMenuItemExt::createSpriteExtra(CircleButtonSprite::createWithSprite("steam_news_button.png"_spr, 1.0f,
                CircleBaseColor::Green, CircleBaseSize::MediumAlt), [](auto) {
                    // Add the functionality for the Steam News button click here
                    geode::log::info("Steam Feed: Steam News Button clicked");
                    auto layer = SteamNewsLayer::create();
                    if (layer) {
                        CCDirector::sharedDirector()->getRunningScene()->addChild(layer, 100); // Ensure it's on top
                        geode::log::info("Steam Feed: SteamNewsLayer successfully created and added to scene");
                    }
                    else {
                        geode::log::error("Steam Feed: Failed to create SteamNewsLayer");
                    }
                }));
            bottomMenu->updateLayout();
        }
        else {
            geode::log::error("Steam Feed: Bottom menu not found");
        }

        return true;
    }
};
