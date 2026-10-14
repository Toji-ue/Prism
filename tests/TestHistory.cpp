#include "history/History.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

/// Test double that records undo/redo calls on a shared log.
class StringCommand final : public prism::core::Command {
public:
    StringCommand(std::string label, std::vector<std::string>& log)
        : m_label(std::move(label)), m_log(log) {}

    const char* description() const override { return m_label.c_str(); }

    void undo() override { m_log.push_back("undo:" + m_label); }
    void redo() override { m_log.push_back("redo:" + m_label); }

private:
    std::string m_label;
    std::vector<std::string>& m_log;
};

TEST(HistoryTest, ExecuteRunsRedoAndTracksUndoability) {
    prism::core::History history;
    std::vector<std::string> log;

    EXPECT_FALSE(history.canUndo());
    history.execute(std::make_unique<StringCommand>("add layer", log));
    EXPECT_TRUE(history.canUndo());
    EXPECT_FALSE(history.canRedo());
    ASSERT_EQ(log.size(), std::size_t{1});
    EXPECT_EQ(log[0], "redo:add layer");
    EXPECT_STREQ(history.undoDescription(), "add layer");
}

TEST(HistoryTest, UndoRedoRoundTrip) {
    prism::core::History history;
    std::vector<std::string> log;

    history.execute(std::make_unique<StringCommand>("a", log));
    history.execute(std::make_unique<StringCommand>("b", log));

    history.undo(); // b
    EXPECT_TRUE(history.canRedo());
    EXPECT_STREQ(history.redoDescription(), "b");
    history.undo(); // a
    EXPECT_FALSE(history.canUndo());

    history.redo(); // a
    history.redo(); // b
    EXPECT_FALSE(history.canRedo());
    EXPECT_EQ(log, (std::vector<std::string>{"redo:a", "redo:b", "undo:b", "undo:a", "redo:a",
                                             "redo:b"}));
}

TEST(HistoryTest, NewActionDropsRedoTail) {
    prism::core::History history;
    std::vector<std::string> log;

    history.execute(std::make_unique<StringCommand>("a", log));
    history.execute(std::make_unique<StringCommand>("b", log));
    history.undo();                                             // b undone
    history.execute(std::make_unique<StringCommand>("c", log)); // forks timeline

    EXPECT_FALSE(history.canRedo());
    EXPECT_EQ(history.size(), std::size_t{2});
    EXPECT_STREQ(history.undoDescription(), "c");
}

TEST(HistoryTest, SavedCheckpointSurvivesUndoRedoAndBranches) {
    prism::core::History history;
    std::vector<std::string> log;
    EXPECT_FALSE(history.isModified());
    history.execute(std::make_unique<StringCommand>("paint", log));
    const auto saved = history.stateId();
    history.markSaved(saved);
    EXPECT_FALSE(history.isModified());
    history.execute(std::make_unique<StringCommand>("paint again", log));
    EXPECT_TRUE(history.isModified());
    history.undo();
    EXPECT_FALSE(history.isModified());
    history.redo();
    EXPECT_TRUE(history.isModified());
    history.undo();
    history.execute(std::make_unique<StringCommand>("new branch", log));
    EXPECT_NE(history.stateId(), saved);
    EXPECT_TRUE(history.isModified());
}

TEST(HistoryTest, EvictingHistoryCannotMistakeANewStateForTheSavedState) {
    prism::core::History history;
    std::vector<std::string> log;
    for (int i = 0; i < 120; ++i)
        history.execute(std::make_unique<StringCommand>("paint", log));
    history.markSaved(history.stateId());
    history.execute(std::make_unique<StringCommand>("after save", log));
    EXPECT_TRUE(history.isModified());
    history.undo();
    EXPECT_FALSE(history.isModified());
    history.clear();
    EXPECT_FALSE(history.isModified());
    history.markUnsaved();
    EXPECT_TRUE(history.isModified());
}

} // namespace
