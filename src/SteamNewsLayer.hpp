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
    void scrollToTop(CCObject* sender);

    struct NewsItem {
        std::string gid;
        std::string title;
        std::string content;
        std::string date;
    };

    CREATE_FUNC(SteamNewsLayer);

protected:
    virtual void registerWithTouchDispatcher() override;

private:
    std::vector<NewsItem> parseNewsItems(const std::string& response);
    void createScrollView(const std::vector<NewsItem>& newsItems);
    cocos2d::CCNode* createNewsItem(const std::string& title, const std::string& content, const std::string& date);
    std::string removeUnwantedParts(const std::string& text, const std::string& gid);
    std::string wrapText(const std::string& text, float maxWidth, const char* fontFile);

    geode::EventListener<geode::utils::web::WebTask> m_listener;
    geode::LoadingSpinner* m_loadingSpinner;
    cocos2d::extension::CCScrollView* m_scrollView = nullptr;  // for tracking the scroll view currently

    virtual void scrollViewDidScroll(cocos2d::extension::CCScrollView* view) override {}
    virtual void scrollViewDidZoom(cocos2d::extension::CCScrollView* view) override {}
};
