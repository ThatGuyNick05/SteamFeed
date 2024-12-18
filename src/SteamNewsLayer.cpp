#include "SteamNewsLayer.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <rapidjson/document.h>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/ui/Layout.hpp>
#include <ctime>

using namespace cocos2d;
using namespace rapidjson;
using namespace geode::prelude;

bool SteamNewsLayer::init() {
    if (!FLAlertLayer::init(180)) { // Initialized with half opacity
        return false;
    }

    this->setContentSize(CCDirector::sharedDirector()->getWinSize());
    this->setTouchEnabled(true);

    // Side-overlay for the new button area
    auto overlayWidth = 60.0f;
    auto overlayHeight = this->getContentSize().height;
    auto buttonOverlay = CCLayerColor::create(ccc4(0, 0, 0, 128), overlayWidth, overlayHeight);
    buttonOverlay->setPosition(ccp(0, 0));
    this->addChild(buttonOverlay, 5);

    // the close button setup
    auto closeBtnSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(closeBtnSprite, closeBtnSprite, this, menu_selector(SteamNewsLayer::closePopup));
    if (closeBtn) {
        closeBtn->setPosition(ccp(30, this->getContentSize().height - 120));

        auto menu = CCMenu::create(closeBtn, nullptr);
        menu->setID("close-button-menu");
        menu->setPosition(CCPointZero);
        this->addChild(menu, 15);
    }

    // the loading spinner setup
    m_loadingSpinner = geode::LoadingSpinner::create(50.0f);
    CC_SAFE_RETAIN(m_loadingSpinner);
    m_loadingSpinner->setPosition(this->getContentSize() / 2);
    this->addChild(m_loadingSpinner, 15);

    // the back-to-top arrow button setup
    auto upArrowSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    upArrowSprite->setRotation(90.0f);
    auto upArrowBtn = CCMenuItemSpriteExtra::create(upArrowSprite, upArrowSprite, this, menu_selector(SteamNewsLayer::scrollToTop));
    upArrowBtn->setPosition(ccp(30, this->getContentSize().height - 190));

    auto upArrowMenu = CCMenu::create(upArrowBtn, nullptr);
    upArrowMenu->setPosition(CCPointZero);
    this->addChild(upArrowMenu, 15);

    fetchNewsItems();
    return true;
}

void SteamNewsLayer::scrollToTop(CCObject* sender) {
    if (m_scrollView) {
        m_scrollView->setContentOffset(ccp(0, m_scrollView->getViewSize().height - m_scrollView->getContentSize().height), true);
    }
}

void SteamNewsLayer::registerWithTouchDispatcher() {
    CCTouchDispatcher* touchDispatcher = CCDirector::sharedDirector()->getTouchDispatcher();
    touchDispatcher->addTargetedDelegate(this, touchDispatcher->getTargetPrio(), true);
}

void SteamNewsLayer::closePopup(CCObject* sender) {
    this->removeAllChildrenWithCleanup(true);
    this->removeFromParentAndCleanup(true);
}

void SteamNewsLayer::fetchNewsItems() {
    geode::log::info("Fetching the SteamNews items...");

    std::string url = "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=322170&count=300";

    auto req = geode::utils::web::WebRequest();
    m_listener.bind([this](web::WebTask::Event* e) {
        if (auto res = e->getValue()) {
            auto response = res->string().unwrapOr("");
            if (response.empty()) {
                return;
            }

            auto newsItems = parseNewsItems(response);
            Loader::get()->queueInMainThread([this, newsItems]() {
                this->removeChild(m_loadingSpinner, true); // removing the loading spinner
                createScrollView(newsItems);
                });
        }
        });

    auto task = req.get(url);
    m_listener.setFilter(task);
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
            // skipping duplicate articles based on the gid
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

            // converting the current date format to readable date
            if (newsItem.HasMember("date")) {
                time_t rawTime = newsItem["date"].GetInt64();
                struct tm* timeInfo = localtime(&rawTime);
                char buffer[11];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeInfo);
                item.date = buffer;
            }

            if (gid != "5410576585126249016") {
                newsItems.push_back(item);
            }
        }
    }

    // For reversing the order to show the most recent news on top
    std::reverse(newsItems.begin(), newsItems.end());

    return newsItems;
}

void SteamNewsLayer::createScrollView(const std::vector<NewsItem>& newsItems) {
    auto scrollLayer = CCLayer::create();
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    float totalHeight = 0;
    for (const auto& item : newsItems) {
        auto newsItem = createNewsItem(item.title, item.content, item.date);
        if (newsItem) {
            size_t newlineCount = std::count(item.content.begin(), item.content.end(), '\n');
            float spacing = newlineCount >= 5 ? 80 : 40;
            newsItem->setPosition(ccp(40, totalHeight));
            totalHeight += newsItem->getContentSize().height + spacing;
            scrollLayer->addChild(newsItem);
        }
    }

    scrollLayer->setContentSize(CCSizeMake(winSize.width, totalHeight));

    m_scrollView = cocos2d::extension::CCScrollView::create(winSize, scrollLayer);
    m_scrollView->setDirection(cocos2d::extension::kCCScrollViewDirectionVertical);
    m_scrollView->setPosition(CCPointZero);
    m_scrollView->setContentOffset(ccp(0, winSize.height - totalHeight));
    m_scrollView->setTouchEnabled(true);

    this->addChild(m_scrollView);
}

CCNode* SteamNewsLayer::createNewsItem(const std::string& title, const std::string& content, const std::string& date) {
    auto node = CCNode::create();
    float width = CCDirector::sharedDirector()->getWinSize().width - 150; // For avoiding arrow overlap
    float height = 50;
    float padding = 40;

    // Calculating the needed height for the content
    auto tempLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
    height += tempLabel->getContentSize().height;

    node->setContentSize(CCSizeMake(width, height));

    std::string wrappedTitle = wrapText(title, width - 2 * padding, "goldFont.fnt");

    // Making the title label with a drop shadow
    auto titleLabel = CCLabelBMFont::create(wrappedTitle.c_str(), "goldFont.fnt");
    if (titleLabel) {
        titleLabel->setAnchorPoint(ccp(0, 1));
        titleLabel->setPosition(ccp(padding, node->getContentSize().height - padding));
        titleLabel->setScale(0.8);

        auto shadowTitleLabel = CCLabelBMFont::create(wrappedTitle.c_str(), "goldFont.fnt");
        shadowTitleLabel->setAnchorPoint(ccp(0, 1));
        shadowTitleLabel->setPosition(ccp(padding + 2, node->getContentSize().height - padding - 2)); // The shadow effect
        shadowTitleLabel->setScale(0.8);
        shadowTitleLabel->setColor(ccc3(0, 0, 0));
        shadowTitleLabel->setOpacity(100);

        node->addChild(shadowTitleLabel, -1); // Placing the shadow behind the title
        node->addChild(titleLabel);
    }

    // Calculating the vertical position for the date based on the title's height number
    float titleHeight = titleLabel->getContentSize().height * titleLabel->getScale();
    float datePositionY = node->getContentSize().height - padding - titleHeight - 10;

    // Making the date label
    auto dateLabel = CCLabelBMFont::create(date.c_str(), "bigFont.fnt");
    if (dateLabel) {
        dateLabel->setAnchorPoint(ccp(0, 1));
        dateLabel->setPosition(ccp(padding, datePositionY)); // To position it below the title
        dateLabel->setScale(0.4);
        dateLabel->setOpacity(128);

        auto shadowDateLabel = CCLabelBMFont::create(date.c_str(), "bigFont.fnt");
        shadowDateLabel->setAnchorPoint(ccp(0, 1));
        shadowDateLabel->setPosition(ccp(padding + 2, datePositionY - 2)); // The shadow effect
        shadowDateLabel->setScale(0.4);
        shadowDateLabel->setColor(ccc3(0, 0, 0));
        shadowDateLabel->setOpacity(100);

        node->addChild(shadowDateLabel, -1); // Put the shadow behind the given date
        node->addChild(dateLabel);
    }

    // Adjustment for the content position based on the line count inside each title and date
    size_t titleLines = std::count(wrappedTitle.begin(), wrappedTitle.end(), '\n') + 1;
    float contentYOffset = 80 + (titleLines - 1) * 20;
    contentYOffset -= 20; // To maintain their original position

    // Make the Content label with drop shadow
    auto contentLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
    if (contentLabel) {
        contentLabel->setAnchorPoint(ccp(0, 1));
        contentLabel->setPosition(ccp(padding, node->getContentSize().height - padding - contentYOffset));
        contentLabel->setScale(0.8);

        auto shadowContentLabel = CCLabelBMFont::create(content.c_str(), "chatFont.fnt", width, kCCTextAlignmentLeft);
        shadowContentLabel->setAnchorPoint(ccp(0, 1));
        shadowContentLabel->setPosition(ccp(padding + 2, node->getContentSize().height - padding - contentYOffset - 2)); // The shadow effect
        shadowContentLabel->setScale(0.8);
        shadowContentLabel->setColor(ccc3(0, 0, 0));
        shadowContentLabel->setOpacity(100);

        node->addChild(shadowContentLabel, -1); // Place the shadow behind the content
        node->addChild(contentLabel);
    }

    return node;
}

std::string SteamNewsLayer::removeUnwantedParts(const std::string& text, const std::string& gid) {
    std::string result = text;
    size_t pos;

    // The removed portions of text
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

    // Removing [, and ]
    result.erase(std::remove(result.begin(), result.end(), '['), result.end());
    result.erase(std::remove(result.begin(), result.end(), ']'), result.end());

    // Removing the occurrence of "/list"
    while ((pos = result.find("/list")) != std::string::npos) {
        result.erase(pos, 5);
    }

    // Removing duplicate article with gid "5218041989051270041"
    if (gid == "5218041989051270041") {
        result.erase(std::remove(result.begin(), result.end(), '/'), result.end());
    }

    // Removing duplicate article with gid "5124585319850001325"
    if (gid == "5124585319850001325") {
        while ((pos = result.find("[img]{STEAM_CLAN_IMAGE}/7432088/4fcada2e76dd5b2839d84e420a53315d8e078f98.png")) != std::string::npos) {
            result.erase(pos, 74);
        }
    }

    // Replacing the words "/Ru" or "/Rub" with "/RubRub" for full text string.
    std::istringstream stream(result);
    std::string word;
    std::string finalResult;

    while (stream >> word) {
        if (word.find("/Ru") != std::string::npos || word.find("/Rub") != std::string::npos) {
            word = "/RubRub";
        }
        finalResult += word + " ";
    }

    // Trimmed trailing spacing.
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
    float buffer = -100;  // Setting the buffer for longer lines before wrapping happens.

    while (wordStream >> word) {
        auto tempLabel = CCLabelBMFont::create(word.c_str(), fontFile);
        float wordWidth = tempLabel->getContentSize().width;
        tempLabel->cleanup();
        tempLabel->release();

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
