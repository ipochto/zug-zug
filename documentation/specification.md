# StrataGGus
# Technical Specification

## 1. Introduction
### 1.1 Project Goal
The goal of the project is to create a platform (game engine, server backend, and development tools) required for the implementation of [WarGGus]() — a multiplayer-oriented classic 2D RTS set in the Warcraft II universe. The project focuses on improving balance, introducing new game mechanics, and enhancing variability for deeper and more engaging online gameplay.

### 1.2 Motivation for Development
- The need for a fully-featured solution for modding the original game and convenient tooling for realizing its competitive potential, still in demand by an active community.
- Gaining development experience.
- Fun, at least.

### 1.3 General Description
The project is a modular platform that includes a game engine, client and server components, and development/debugging tools.

The main focus is multiplayer functionality: matchmaking system, ranking, tools for organizing tournaments, replay viewing and analysis, and flexible scenario/rule/map configuration.

The engine architecture features built-in advanced script support, representing most of the game logic. This enables a full-featured modding system.

## 2. General Information
- 2.1 System Purpose
- 2.2 Brief Function Overview
- 2.3 Target Audience / Users
- 2.4 Constraints and Assumptions

## 3. Functional Requirements
- 3.1 Key Features List
    - 3.1.1 Isolated Lua States/Sandboxes
- 3.2 Use Cases (User Stories)
- 3.3 Data / Interaction Flows (optional)

## 4. Interface
- 4.1 User Interface (if applicable)
- 4.2 API or Commands (if CLI / embedded scripting)
- 4.3 External Component Integration (if applicable)

## 5. Technical Requirements
- 5.1 Languages and Technologies
    * C++20
    * Lua 5.1 / LuaJIT
    * dasLang (optional)
- 5.2 Dependencies and Third-Party Libraries
    * `cxxopts` – command-line argument parsing
    * `sol2` – integration with Lua code
    * `doctest` – unit testing
    * `entt` – ECS framework
    * `Dear ImGui` – UI for editor and debugging tools
    * `SFML` – graphics rendering
    * `fmt` – text formatting
    * `spdlog` – logging
    * `range-v3` – ranges implementation
- 5.3 Target Platforms (OS, architecture, etc.)
    * Windows
    * Linux
    * macOS

## 6. Non-Functional Requirements (optional)
- 6.1 Performance
- 6.2 Security
- 6.3 Portability
- 6.4 Reliability / Stability

## 7. Development Plan
- 7.1 Major Stages
- 7.2 MVP: Minimum Viable Product
- 7.3 Task Prioritization

## 8. Appendices (optional)
- 8.1 Glossary of Terms
- 8.2 Layouts, Diagrams, Sketches
- 8.3 Example Commands / Scripts

# Technical Specification
## for Developing a Game Engine for Classic 2D RTS Games
