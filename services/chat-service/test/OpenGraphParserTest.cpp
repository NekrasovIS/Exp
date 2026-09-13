#include "OpenGraphParser.h"

#include <gtest/gtest.h>

namespace chat_service {
namespace {

TEST(OpenGraphParserTest, ExtractsTitleDescriptionAndImage) {
    const std::string html = R"(<html><head>
        <meta property="og:title" content="Example Article">
        <meta property="og:description" content="A short summary.">
        <meta property="og:image" content="https://example.test/cover.png">
    </head></html>)";

    const auto metadata = open_graph::parse(html, "https://example.test/article");

    EXPECT_EQ(metadata.title, "Example Article");
    EXPECT_EQ(metadata.description, "A short summary.");
    EXPECT_EQ(metadata.imageUrl, "https://example.test/cover.png");
}

TEST(OpenGraphParserTest, WorksRegardlessOfAttributeOrder) {
    const std::string html = R"(<meta content="Reordered" property="og:title">)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.title, "Reordered");
}

TEST(OpenGraphParserTest, AcceptsNameAttributeAsAnAlternativeToProperty) {
    const std::string html = R"(<meta name="og:title" content="Via name attribute">)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.title, "Via name attribute");
}

TEST(OpenGraphParserTest, FallsBackToTitleTagWhenOgTitleIsMissing) {
    const std::string html = R"(<html><head><title>Plain Title</title></head></html>)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.title, "Plain Title");
}

TEST(OpenGraphParserTest, OgTitleTakesPrecedenceOverTitleTag) {
    const std::string html =
        R"(<html><head><title>Plain Title</title><meta property="og:title" content="OG Title"></head></html>)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.title, "OG Title");
}

TEST(OpenGraphParserTest, ResolvesProtocolRelativeImageUrl) {
    const std::string html = R"(<meta property="og:image" content="//cdn.example.test/img.png">)";

    const auto metadata = open_graph::parse(html, "https://example.test/article");

    EXPECT_EQ(metadata.imageUrl, "https://cdn.example.test/img.png");
}

TEST(OpenGraphParserTest, ResolvesRootRelativeImageUrlAgainstTheOrigin) {
    const std::string html = R"(<meta property="og:image" content="/static/img.png">)";

    const auto metadata = open_graph::parse(html, "https://example.test/blog/article");

    EXPECT_EQ(metadata.imageUrl, "https://example.test/static/img.png");
}

TEST(OpenGraphParserTest, LeavesAbsoluteImageUrlUnchanged) {
    const std::string html = R"(<meta property="og:image" content="https://other.test/cover.png">)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.imageUrl, "https://other.test/cover.png");
}

TEST(OpenGraphParserTest, ReturnsEmptyMetadataForHtmlWithoutAnyRecognizedTags) {
    const std::string html = R"(<html><head></head><body>No metadata here.</body></html>)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_TRUE(metadata.title.empty());
    EXPECT_TRUE(metadata.description.empty());
    EXPECT_TRUE(metadata.imageUrl.empty());
}

TEST(OpenGraphParserTest, HandlesEmptyHtmlWithoutCrashing) {
    const auto metadata = open_graph::parse("", "https://example.test/");

    EXPECT_TRUE(metadata.title.empty());
}

TEST(OpenGraphParserTest, SingleQuotedAttributesAreAlsoRecognized) {
    const std::string html = R"(<meta property='og:title' content='Single Quoted'>)";

    const auto metadata = open_graph::parse(html, "https://example.test/");

    EXPECT_EQ(metadata.title, "Single Quoted");
}

}  // namespace
}  // namespace chat_service
