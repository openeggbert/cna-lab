// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Core/EditorCommand.hpp
 * @brief The command pattern every document mutation goes through, and the undo/redo stack.
 *
 * Nothing mutates a document directly. Every change -- from the inspector, from a gizmo drag, from
 * a plugin, from the runtime bridge -- is an EditorCommand pushed through CommandHistory. That is
 * a hard rule, decided up front (ANALYSIS.md decision D-06) precisely because retrofitting undo
 * onto direct mutation is the failure mode that kills editors: by the time you notice, every call
 * site is a place undo silently does not work.
 */

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace CNA::Editor
{
    /**
     * @brief One reversible document mutation.
     *
     * A command captures everything it needs to undo itself at construction or at execute() time.
     * It must be re-executable: CommandHistory calls execute() once when pushed and again on every
     * redo, so execute() has to be idempotent with respect to its own undo().
     */
    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;

        /** @brief Applies the change. Called once on push and again on each redo. */
        virtual void execute() = 0;

        /** @brief Reverses the change, restoring exactly the state execute() found. */
        virtual void undo() = 0;

        /** @brief Human-readable label, shown in the undo history panel and the console. */
        [[nodiscard]] virtual std::string getDescription() const = 0;

        /**
         * @brief Identifies what this command targets, for merging.
         *
         * Two commands may only merge when their merge keys are equal. For SetPropertyCommand the
         * key is entity id + component + property name, so dragging one gizmo collapses into a
         * single undo step while alternating between two objects does not.
         *
         * The default empty key means "never merges".
         */
        [[nodiscard]] virtual std::string getMergeKey() const { return {}; }

        /**
         * @brief Folds @p newer into this command, so the pair becomes one undo step.
         *
         * Only called when the merge keys match. The receiver keeps its own *original* undo state
         * and adopts @p newer's *final* state -- that is what makes a thousand mouse-move events
         * during a gizmo drag collapse into one entry that undoes back to where the drag started.
         *
         * @return False when this command declines to merge; the caller then pushes @p newer
         *         as a separate entry.
         */
        virtual bool mergeWith(const EditorCommand& newer) { (void)newer; return false; }
    };

    /** @brief Whether a pushed command may collapse into the previous one. */
    enum class MergePolicy
    {
        /** @brief Always a new undo entry. The default, and correct for discrete edits. */
        NewEntry,

        /**
         * @brief Fold into the previous entry when the merge keys match.
         *
         * Used for continuous input: gizmo drags, slider scrubs, colour-picker motion.
         */
        MergeWithPrevious
    };

    /**
     * @brief The undo/redo stack for one document.
     *
     * Redo is discarded on any new push, which is the behaviour every editor user already expects.
     * The stack is bounded (see setLimit) so that a long session of gizmo dragging cannot grow
     * memory without limit; dropping the oldest entries is safe because they are past the point
     * anyone will realistically undo to.
     */
    class CommandHistory
    {
    public:
        CommandHistory() = default;
        CommandHistory(const CommandHistory&) = delete;
        CommandHistory& operator=(const CommandHistory&) = delete;

        /**
         * @brief Executes @p command and records it.
         *
         * @param command The command; ownership is taken. A null command is ignored.
         * @param policy Whether to attempt a merge with the previous entry.
         */
        void execute(std::unique_ptr<EditorCommand> command, MergePolicy policy = MergePolicy::NewEntry);

        /** @brief Returns true when there is at least one entry to undo. */
        [[nodiscard]] bool canUndo() const { return cursor_ > 0; }

        /** @brief Returns true when there is at least one undone entry to redo. */
        [[nodiscard]] bool canRedo() const { return cursor_ < commands_.size(); }

        /** @brief Undoes the most recent entry. No-op when canUndo() is false. */
        bool undo();

        /** @brief Redoes the most recently undone entry. No-op when canRedo() is false. */
        bool redo();

        /** @brief Returns the label of the entry undo() would reverse, or an empty string. */
        [[nodiscard]] std::string getUndoDescription() const;

        /** @brief Returns the label of the entry redo() would apply, or an empty string. */
        [[nodiscard]] std::string getRedoDescription() const;

        /**
         * @brief Returns the label of entry @p index, or an empty string when out of range.
         *
         * Indices run oldest first, and an entry keeps its index whether or not it is currently
         * applied -- which is what lets the history panel show the undone tail rather than hiding
         * the very entries a user is trying to get back to.
         */
        [[nodiscard]] std::string getDescriptionAt(std::size_t index) const;

        /**
         * @brief Returns entry @p index, or nullptr when out of range.
         *
         * Needed by anything that has to act on *which* change happened rather than on its label:
         * mirroring an undo into a running player is the case, since an undo is a document change
         * like any other and the player has to see it.
         */
        [[nodiscard]] const EditorCommand* getCommandAt(std::size_t index) const;

        /**
         * @brief Returns the cursor position the document was last saved at, or -1 for none.
         *
         * Negative when the saved state was discarded -- either by a new command overwriting the
         * redo tail it lived in, or by the retention limit dropping it off the front. Both mean the
         * same thing to a reader: no position in this history is the file on disk.
         */
        [[nodiscard]] std::ptrdiff_t getSavedCursor() const { return savedCursor_; }

        /** @brief Drops every entry. Called when a document is closed or reloaded. */
        void clear();

        /** @brief Returns the number of recorded entries, undone ones included. */
        [[nodiscard]] std::size_t getCount() const { return commands_.size(); }

        /** @brief Returns the index of the next entry to redo, i.e. the number of applied entries. */
        [[nodiscard]] std::size_t getCursor() const { return cursor_; }

        /**
         * @brief Marks the current position as the last-saved state.
         *
         * Called by the document save path. isDirty() then reports whether the user has moved away
         * from that position -- in *either* direction, so undoing back past a save also marks the
         * document dirty, which is correct and is a detail editors routinely get wrong.
         */
        void markSaved() { savedCursor_ = static_cast<std::ptrdiff_t>(cursor_); }

        /**
         * @brief Declares that no position in this history is the file on disk.
         *
         * Used after a crash-recovery restore: the document now holds work that was never saved,
         * and a fresh history whose starting point claimed to be "saved" would tell the user the
         * opposite of the truth.
         */
        void markUnsaved() { savedCursor_ = -1; }

        /** @brief Returns true when the document differs from its last-saved state. */
        [[nodiscard]] bool isDirty() const { return static_cast<std::ptrdiff_t>(cursor_) != savedCursor_; }

        /** @brief Sets the maximum number of retained entries. Zero means unbounded. */
        void setLimit(std::size_t limit);

        /** @brief Returns the current retention limit; zero means unbounded. */
        [[nodiscard]] std::size_t getLimit() const { return limit_; }

    private:
        void trimToLimit();

        std::vector<std::unique_ptr<EditorCommand>> commands_;
        std::size_t cursor_ = 0;
        std::ptrdiff_t savedCursor_ = 0;
        std::size_t limit_ = 512;
    };
}
