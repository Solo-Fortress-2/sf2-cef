# Contributing to our open-source projects

Hello! First off we want to say thank you for taking your time to contribute to our open-source components!

We welcome pull requests, bug reports, and ideas that help us improve Solo Fortress 2.

> Solo Fortress 2 is built on the **Source SDK 2013 Multiplayer** (Team Fortress 2) branch, this implementation can be implemented/tested in non-TF2 mods.

----

## How to contribute?

### 1. Reporting issues

- Use the [**Issues**](https://github.com/Solo-Fortress-2/sf2-cef/issues) tab to report issues, or suggest improvements.
- If possible, include steps to reproduce the issue, the expected behavior, and provide your system information (or engine branch).
- Attach debugging output or media content if relevant.

### 2. Submitting changes

- Create a new fork of this repository, and create a new branch if you really need to:
  
```bash
git clone https://github.com/<my-username-or-organization>/sf2-cef.git
cd sf2-cef

# optional
git checkout -b fix/browser-paint-bug
```

- Make your changes.
- Ensure that the code compiles (at least) on the Release configuration.
- Test in-game before submitting your changes.

When ready, open a pull request with a clear description of:

- What you have changed
- Why the change is needed/important
- Any testing done or remaining issues

## Code style guidelines

### General

The style of the source file must be:

- Readable (ex. minimal nesting)
- (Optional) Comment anything non-obvious

### C++ (for SF2 components, Source Engine-related components)

- If possible, make use of Valve's STL/base functions. (`CUtlVector`, `CUtlMap`, `Q_strncpy`, etc.)
- Prioritize proper garbage collection, without using raw `delete` keywords unless required.

### Commits

- Use descriptive commit messages like:
  
```plain
fix: resolved weird artifacts in the browser view
```

## Testing

Before your open a pull request, make sure that:

- The game runs without crashes.
- Unnecessary pointers are cleaned up (no memory leaks).
- If rendering-related: Verify that the shader or panel renders correctly
- If rendering-related: Test in both windowed and full-screen modes.

## Communication

Teamwork is key, discuss with other people about the changes in the issue before starting.

----

Happy coding, and welcome to the Solo Fortress 2 community!

Thank you from the Solo Fortress 2 team.
