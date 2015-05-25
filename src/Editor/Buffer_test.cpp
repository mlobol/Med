#include "Buffer.h"

#include <QtCore/QTextStream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Med {
namespace Editor {

class BufferTest : public ::testing::Test {
protected:
  void InitBuffer(const char* string) {
    QTextStream stream(string);
    buffer.InitFromStream(&stream, "test");
  }

  Buffer buffer;
};

TEST_F(BufferTest, IterateFromLineNumber) {
  InitBuffer(
    R"(first line
    second line
    third line
    fourth line)");
  EXPECT_EQ("test", buffer.name());

  Buffer::Point from(&buffer);
  from.setLineNumber(2);
  ASSERT_TRUE(from.isValid());
  {
    std::vector<std::pair<int, std::string>> lines;
    for (Buffer::Point::UnsafeLine line : from.linesForwards()) {
      lines.emplace_back(line.lineNumber, line.lineContent->trimmed().toStdString());
    }
    EXPECT_THAT(lines,
                testing::ElementsAre(testing::Pair(2, "second line"),
                                     testing::Pair(3, "third line"),
                                     testing::Pair(4, "fourth line")));
  }
  {
    std::vector<std::pair<int, std::string>> lines;
    for (Buffer::Point::UnsafeLine line : from.linesForwards()) {
      lines.emplace_back(line.lineNumber, line.lineContent->trimmed().toStdString());
      // Breaking before reaching the end of the iterator should work.
      if (lines.size() == 2) break;
    }
    EXPECT_THAT(lines,
                testing::ElementsAre(testing::Pair(2, "second line"),
                                     testing::Pair(3, "third line")));
  }
}

}  // namespace Editor
}  // namespace Med
