#include "SteamNewsLayer.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <rapidjson/document.h>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/ui/LoadingSpinner.hpp>

using namespace cocos2d;
using namespace rapidjson;
using namespace geode::prelude;

bool SteamNewsLayer::init() {
    geode::log::info("Initializing SteamNewsLayer...");

    if (!FLAlertLayer::init(180)) {  // Initialized with needed opacity
        geode::log::error("Failed to initialize FLAlertLayer");
        return false;
    }

    this->setContentSize(CCDirector::sharedDirector()->getWinSize());
    this->setTouchEnabled(true);

    auto closeBtnSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    auto closeBtn = CCMenuItemSprite::create(closeBtnSprite, closeBtnSprite, this, menu_selector(SteamNewsLayer::closePopup));
    if (closeBtn) {
        closeBtn->setPosition(ccp(50, this->getContentSize().height - 50));
        auto menu = CCMenu::create(closeBtn, nullptr);
        menu->setPosition(CCPointZero);
        this->addChild(menu);
    }
    else {
        geode::log::error("Failed to create close button");
    }

    m_loadingSpinner = geode::LoadingSpinner::create(50.0f);
    m_loadingSpinner->setPosition(this->getContentSize() / 2);
    this->addChild(m_loadingSpinner);

    fetchNewsItems();

    return true;
}

void SteamNewsLayer::registerWithTouchDispatcher() {
    CCTouchDispatcher* touchDispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
    touchDispatcher->addTargetedDelegate(this, touchDispatcher->getTargetPrio(), true);
}

void SteamNewsLayer::closePopup(CCObject* sender) {
    this->removeFromParentAndCleanup(true);
}

void SteamNewsLayer::fetchNewsItems() {
    geode::log::info("Fetching news items...");

    std::string url = "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=322170&count=300";

    auto req = geode::utils::web::WebRequest();
    m_listener.bind([this](geode::utils::web::WebTask::Event* e) {
        if (auto res = e->getValue()) {
            auto response = res->string().unwrapOr("");
            if (response.empty()) {
                geode::log::error("Failed to fetch news items");
                return;
            }

            auto newsItems = parseNewsItems(response);
            Loader::get()->queueInMainThread([this, newsItems]() {
                this->removeChild(m_loadingSpinner, true); // Remove loading spinner

                createScrollView(newsItems);
                });
        }
        else if (auto progress = e->getProgress()) {
            geode::log::info("Fetching in progress...");
        }
        else if (e->isCancelled()) {
            geode::log::info("Fetching was cancelled.");
        }
        });
    m_listener.setFilter(req.get(url));
}

std::vector<SteamNewsLayer::NewsItem> SteamNewsLayer::parseNewsItems(const std::string& response) {
    std::vector<NewsItem> newsItems;

    Document document;
    document.Parse(response.c_str());

    if (!document.HasParseError() && document.IsObject()) {
        const auto& appNews = document["appnews"];
        const auto& newsItemsArray = appNews["newsitems"];
        for (auto& newsItem : newsItemsArray.GetArray()) {
            NewsItem item;
            std::string gid = newsItem["gid"].GetString();
            // Skip articles based on gid
            static const std::set<std::string> skipGids = {
                "5410576585124650573", "2436926440562370340", "2284879949508460627",
                "2163281492537211231", "2152021858901922963", "2152021858894636598",
                "2486412956120074597", "3044845282402408345", "4249665521681179987",
                "4249665521681180090", "4249665521681180188", "295352659733029280",
                "377538916270267899", "378660375673380952", "371902438121350491",
                "405678801273981156", "409053962649582461", "518256108464071258",
                "517128413774920296", "517127039058316220", "515998514243691390",
                "517122602985592706", "517122602980080996", "511492468646310449",
                "521624142113331755"
            };
            if (skipGids.find(gid) != skipGids.end()) {
                continue;
            }
            item.gid = gid;
            item.title = newsItem["title"].GetString();
            item.content = removeUnwantedParts(newsItem["contents"].GetString(), gid);
            if (gid != "5410576585126249016") {
                newsItems.push_back(item);
            }
        }
    }

    // Reverse the order to show the most recent news at the top
    std::reverse(newsItems.begin(), newsItems.end());

    return newsItems;
}

void SteamNewsLayer::createScrollView(const std::vector<NewsItem>& newsItems) {
    geode::log::info("Creating scroll view...");

    auto scrollLayer = CCLayer::create();
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    float totalHeight = 0;
    for (const auto& item : newsItems) {
        auto newsItem = createNewsItem(item.title, item.content);
        if (newsItem) {
            size_t newlineCount = std::count(item.content.begin(), item.content.end(), '\n');
            float spacing;
            if (newlineCount >= 10) {
                spacing = 125;
            }
            else if (newlineCount >= 5) {
                spacing = 90;
            }
            else if (newlineCount >= 2) {
                spacing = 40;
            }
            else {
                spacing = 25;
            }
            newsItem->setPosition(ccp(40, totalHeight));
            totalHeight += newsItem->getContentSize().height + spacing; // Adjust spacing between items
            scrollLayer->addChild(newsItem);
        }
    }

    scrollLayer->setContentSize(CCSizeMake(winSize.width, totalHeight));

    auto scrollView = cocos2d::extension::CCScrollView::create(winSize, scrollLayer);
    scrollView->setDirection(cocos2d::extension::kCCScrollViewDirectionVertical);
    scrollView->setPosition(CCPointZero);
    scrollView->setContentOffset(ccp(0, winSize.height - totalHeight));
    scrollView->setTouchEnabled(true);
    scrollView->setDelegate(this);

    this->addChild(scrollView);
}

CCNode* SteamNewsLayer::createNewsItem(const std::string& title, const std::string& content) {
    auto node = CCNode::create();
    float width = CCDirector::sharedDirector()->getWinSize().width - 150; // Shifted right to avoid arrow overlap
    float height = 50; // Base height
    float padding = 40;

    // Calculate required height for content
    auto tempLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
    height += tempLabel->getContentSize().height;
    tempLabel->cleanup(); // Cleanup tempLabel
    tempLabel->release();  // Release tempLabel

    node->setContentSize(CCSizeMake(width, height));

    std::string wrappedTitle = wrapText(title, width - 2 * padding, "goldFont.fnt");

    // Create the title label with a drop shadow
    auto titleLabel = CCLabelBMFont::create(wrappedTitle.c_str(), "goldFont.fnt");
    if (titleLabel) {
        titleLabel->setAnchorPoint(ccp(0, 1));
        titleLabel->setPosition(ccp(padding, node->getContentSize().height - padding));
        titleLabel->setScale(0.8);

        auto shadowTitleLabel = CCLabelBMFont::create(wrappedTitle.c_str(), "goldFont.fnt");
        shadowTitleLabel->setAnchorPoint(ccp(0, 1));
        shadowTitleLabel->setPosition(ccp(padding + 2, node->getContentSize().height - padding - 2)); // Slightly offset for shadow effect
        shadowTitleLabel->setScale(0.8);
        shadowTitleLabel->setColor(ccc3(0, 0, 0)); // Black color for shadow
        shadowTitleLabel->setOpacity(100); // Transparent shadow

        node->addChild(shadowTitleLabel, -1); // Add shadow behind title
        node->addChild(titleLabel);
    }

    // Adjust content position based on the number of lines in the wrapped title
    size_t titleLines = std::count(wrappedTitle.begin(), wrappedTitle.end(), '\n') + 1;
    float contentYOffset = 40 + (titleLines - 1) * 20; // Adjust contentYOffset based on number of title lines

    // Create the content label with a drop shadow
    auto contentLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
    if (contentLabel) {
        contentLabel->setAnchorPoint(ccp(0, 1));
        contentLabel->setPosition(ccp(padding, node->getContentSize().height - padding - contentYOffset));
        contentLabel->setScale(0.8);

        auto shadowContentLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
        shadowContentLabel->setAnchorPoint(ccp(0, 1));
        shadowContentLabel->setPosition(ccp(padding + 2, node->getContentSize().height - padding - contentYOffset - 2)); // Slightly offset for shadow effect
        shadowContentLabel->setScale(0.8);
        shadowContentLabel->setColor(ccc3(0, 0, 0)); // Black color for shadow
        shadowContentLabel->setOpacity(100); // Transparent shadow

        node->addChild(shadowContentLabel, -1); // Add shadow behind content
        node->addChild(contentLabel);
    }

    return node;
}

std::string SteamNewsLayer::removeUnwantedParts(const std::string& text, const std::string& gid) {
    std::string result = text;
    size_t pos;

    // General unwanted parts removal
    while ((pos = result.find("previewyoutube=")) != std::string::npos) {
        size_t endPos = result.find(" ", pos);
        result.erase(pos, endPos - pos + 1);
    }

    while ((pos = result.find("url=")) != std::string::npos) {
        size_t endPos = result.find(" ", pos);
        result.erase(pos, endPos - pos + 1);
    }
    while ((pos = result.find("/url")) != std::string::npos) {
        size_t endPos = result.find(" ", pos);
        result.erase(pos, endPos - pos + 1);
    }
    // Remove [, and ]
    result.erase(std::remove(result.begin(), result.end(), '['), result.end());
    result.erase(std::remove(result.begin(), result.end(), ']'), result.end());

    // Specific removal for article with gid "5218041989051270041"
    if (gid == "5218041989051270041") {
        result.erase(std::remove(result.begin(), result.end(), '/'), result.end());
    }

    // Specific removal for article with gid "5124585319850001325"
    if (gid == "5124585319850001325") {
        while ((pos = result.find("[img]{STEAM_CLAN_IMAGE}/7432088/4fcada2e76dd5b2839d84e420a53315d8e078f98.png")) != std::string::npos) {
            result.erase(pos, 74);
        }
    }

    // Replace full words containing "/Ru" or "/Rub" with "/RubRub"
    std::istringstream stream(result);
    std::string word;
    std::string finalResult;

    while (stream >> word) {
        if (word.find("/Ru") != std::string::npos || word.find("/Rub") != std::string::npos) {
            word = "/RubRub";
        }
        finalResult += word + " ";
    }

    // Trim the trailing space
    if (!finalResult.empty()) {
        finalResult.pop_back();
    }

    return finalResult;
}

std::string SteamNewsLayer::wrapText(const std::string& text, float maxWidth, const char* fontFile) {
    std::stringstream wrappedText;
    std::stringstream lineStream;
    std::istringstream wordStream(text);
    std::string word;
    float lineWidth = 0;
    float buffer = -100;  // Adding a buffer to allow for longer lines before wrapping

    while (wordStream >> word) {
        auto tempLabel = CCLabelBMFont::create(word.c_str(), fontFile);
        float wordWidth = tempLabel->getContentSize().width;
        tempLabel->cleanup(); // Cleanup tempLabel
        tempLabel->release();  // Release tempLabel

        if (lineWidth + wordWidth + buffer > maxWidth) {
            wrappedText << lineStream.str() << '\n';
            lineStream.str("");
            lineWidth = 0;
        }

        lineStream << word << ' ';
        lineWidth += wordWidth;
    }

    wrappedText << lineStream.str();

    return wrappedText.str();
}
