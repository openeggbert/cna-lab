# CNA Lab - Experimental Repositories

This repository contains experimental CNA (C++ reimplementation of XNA 4.0) related repositories. Projects are organized here for collaborative development and testing, integrated using Git subtrees to preserve their complete commit history.

## Structure

Each subdirectory in this repository corresponds to an experimental CNA-related project, integrated as a Git subtree from its original repository. This approach allows:

- **Complete history preservation**: All commits from the original repositories are maintained
- **Centralized access**: Related projects are organized in a single location
- **Experimental development**: Organized workspace for testing and collaboration

## Working with Subtrees

To update a specific subtree from its upstream repository, use:

```bash
git subtree pull --prefix=<subtree-path> <remote-url> <branch>
```

To view the history of a specific subtree:

```bash
git log <subtree-path>
```

## Repository Management

For more information about managing Git subtrees, refer to the [Git documentation on subtrees](https://git-scm.com/book/en/v2/Git-Tools-Subtrees).

---

**Last updated**: 2026-09-01
