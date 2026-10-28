<div align="center">
  <img src="docs/images/NJE_logo.png" alt="NJE logo" width="220">
  <h1>NJE — NoJobEngine</h1>
  <p><strong>A native C++ engine for building, training and inspecting game AI.</strong></p>
  <p>
    <img alt="Stage: Public Alpha" src="https://img.shields.io/badge/stage-Public_Alpha-60a5fa?style=flat-square">
    <img alt="Platform: Windows x64" src="https://img.shields.io/badge/platform-Windows_x64-38bdf8?style=flat-square">
    <img alt="Language: C++20" src="https://img.shields.io/badge/C%2B%2B-20-a78bfa?style=flat-square">
  </p>
  <p><a href="https://github.com/{owner}/{repository}/releases"><strong>Download NJE</strong></a> · <a href="{site}/">Website</a> · <a href="https://github.com/{owner}/{repository}/issues">Report an issue</a></p>
</div>

---

## From learning to visible decisions

NJE brings native game AI into a visual C++ editor. Train an agent, load its
saved policy, compare it with baselines and inspect its decisions while the
scene runs.

| Explore | What you can see |
| :--- | :--- |
| **Reinforcement learning** | Independent training window, live simulation and saved policies |
| **Learning evaluation** | Held-out scenarios, comparison with random actions and CSV reports |
| **AI Debugger** | Observations, selected actions, Q-values and neural activations |
| **Navigation and behavior** | NavMesh navigation, visual Behavior Trees and agent Blackboards |
| **Live examples** | X/Z target tracking, obstacle avoidance and supervised neural demos |

## Install and try it

1. Open [Releases](https://github.com/{owner}/{repository}/releases) and download **{installer}** from the Alpha release assets.
2. Run the installer. It installs for your Windows account; administrator access is not required.
3. Launch **NJE** from the Start Menu and press **Play**. The orange target moves automatically while the agent follows it.
4. Open **AI → AI Debugger** to inspect the agent. Use **Examples** to explore the other scenes.
5. Open **AI → Reinforcement Learning → Open Training Window** to start an independent training run.

> **Requirements:** Windows 10/11 x64 and a graphics driver supporting OpenGL 4.6.
> Running the supplied examples does not require Visual Studio, CMake or Python.

## Your work stays with you

| Location | Contents |
| :--- | :--- |
| `%LOCALAPPDATA%/Programs/NJE` | Installed application |
| `%LOCALAPPDATA%/NJE/Projects/Default` | Editable scenes, models, training histories and reports |
| `%LOCALAPPDATA%/NJE/Settings` | Editor layout and update preferences |

Updates and uninstall preserve your user-data folder. Back up your projects as
usual. The application copies missing samples on launch and preserves files
that already exist.

## Updates

Use **Help → Check for updates**, or leave automatic checks enabled. NJE checks
the Alpha channel at most once every 24 hours. Installers downloaded by the
updater are verified with an embedded EdDSA public key.

Before installing, NJE asks you to save the scene. Stop Play, finish editor jobs
and close other NJE trainer/runtime windows first.

The installer currently has no Windows Authenticode publisher certificate.
An operating-system reputation warning may appear; EdDSA update verification
and Windows publisher certification serve different purposes.

## Alpha feedback

This is an early public Alpha. Start with the included examples and keep backups
of important work. Fresh-machine compatibility and the first hosted upgrade
still require validation. The included learned policies demonstrate the supplied
environments; they do not promise general navigation in arbitrary scenes.

When [reporting an issue](https://github.com/{owner}/{repository}/issues), include
Windows version, GPU/driver, NJE version, the example scene and steps to reproduce.
Attach a screenshot or a short recording when it helps.

## About this repository

This repository distributes **NJE installers, release notes and the signed update
feed**. It does not contain the engine development source. Download the installer
from release assets; GitHub's source-code ZIP is only this distribution website.

Use is governed by the **LICENSE bundled with the installer**. Third-party
notices are included with the application.

<div align="center">
  <sub>Created by Francisco Ramon Asensi Domingo · NJE — NoJobEngine</sub>
</div>
