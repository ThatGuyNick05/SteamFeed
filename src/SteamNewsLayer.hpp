#pragma once

#include <Geode/loader/Log.hpp>
#include <Geode/modify/FLAlertLayer.hpp>
#include <cocos2d.h>
#include <string>
#include <vector>
#include <rapidjson/document.h>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/ui/LoadingSpinner.hpp>

class SteamNewsLayer : public FLAlertLayer, public cocos2d::extension::CCScrollViewDelegate {
public:
    virtual bool init() override;
    void closePopup(cocos2d::CCObject* sender);
    void fetchNewsItems();

    struct NewsItem {
        std::string gid;
        std::string title;
        std::string content;
    };

    CREATE_FUNC(SteamNewsLayer);

protected:
    virtual void registerWithTouchDispatcher() override;

private:
    std::vector<NewsItem> parseNewsItems(const std::string& response);
    void createScrollView(const std::vector<NewsItem>& newsItems);
    cocos2d::CCNode* createNewsItem(const std::string& title, const std::string& content);
    std::string removeUnwantedParts(const std::string& text, const std::string& gid);
    std::string wrapText(const std::string& text, float maxWidth, const char* fontFile);

    geode::EventListener<geode::utils::web::WebTask> m_listener;
    geode::LoadingSpinner* m_loadingSpinner;

    // Implement virtual function from CCScrollViewDelegate
    virtual void scrollViewDidScroll(cocos2d::extension::CCScrollView* view) override {}
    virtual void scrollViewDidZoom(cocos2d::extension::CCScrollView* view) override {}
};