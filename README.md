# Fusion Engine

Fusion Engine is a custom 2D game engine designed for building lightweight, reinforcement learning (RL) training environments. The engine uses python as its native scripting language with direct integration with Gymnasium and Stable-Baseline3 for RL training. The core architecture of the engine is inspired by Unity and Godot engine but designed with the main goal of creating RL environments instead of games. 

<img width="1913" height="986" alt="Fusion Engine editor showing a project being edited" src="https://github.com/user-attachments/assets/45fedd5e-b4b0-4758-b92b-3718e8a44d3f" />

## Platform Support

> **Note:** Fusion Engine currently only supports **Windows**. There is no macOS or Linux support at this time.

## Installation

### Option 1: Official Release (compiled executable)

Precompiled builds are published as zipped executables under the [Releases](../../releases) section of this repository.

1. Go to the [Releases](../../releases) page.
2. Download the latest `.zip` archive for the engine.
3. Extract the archive to a folder of your choice.
4. Run the engine executable `FusionApp.exe` to launch the editor.

No additional build tools are required to use the compiled release.

### Option 2: Build from Source

Building from source requires **Visual Studio** (the project is developed and maintained as a Visual Studio solution/project and is not currently configured for other IDEs or build systems).

1. Clone the repository:

   ```
   git clone https://github.com/DevPhusion/FusionEngine.git
   ```

2. Open the cloned folder in **Visual Studio**.
3. Build the solution.
4. Run the resulting executable to launch the editor.

## Architecture

Fusion Engine follows an Entity-Component System (ECS) inspired by Unity, with a modular scene node system inspired by Godot. An entity performs no behavior on its own; components are attached to add or modify behavior. Scenes can be nested within other scenes, and instanced scenes stay synchronized across all their occurrences.

The engine core is organized into the following systems:

| System | Responsibility |
|---|---|
| Rendering Engine | OpenGL-based rasterization renderer |
| Physics Engine | Rigid body, soft body, and fluid simulation |
| Object and Component Manager | Manages entities and their components (ECS) |
| Editor | ImGui-based editor UI, windows, and gizmos |
| File and Project Manager | Project/scene serialization, resource paths, and export |
| Script Manager | C++/Python bindings and script bridging |

## Physics Engine

The physics engine is the core of Fusion Engine. Unlike most 2D engines, which simulate rigid bodies only, Fusion Engine is a multi-physics engine that simulates **rigid bodies, soft bodies, and fluids**, either standalone or combined in a single unified simulation (for example, a rigid boat floating on a fluid surface, or a soft body compressing under a rigid weight).

**Demo:** Multiphysics simulation of rigid body, soft body and fluid dynamics

https://github.com/user-attachments/assets/8f7cdf5f-0487-47e7-92c1-7d028b895245

Each physics domain uses a different, purpose-built solver, bridged together through an impulse-based coupling layer:

- **Rigid body — Projected Gauss Seidel (PGS) solver.** Constraints are described using Jacobian matrices, the same general approach used by Box2D. Supports contact constraints as well as 3-DOF joints such as Weld, Revolute, and Prismatic constraints, with contact caching for solver stability.
- **Soft body — Extended Position Based Dynamics (XPBD).** Chosen over a pure force-based mass-spring approach for its stability. Supports distance and area constraints, damping, and inflatable objects modeled with the ideal gas law.
- **Fluids — Position Based Fluids (PBF).** Chosen because it operates in position space (integrating naturally with XPBD) while remaining fast. Includes viscosity, vorticity confinement, and neighbor queries accelerated by a spatial hash grid.

Collision handling varies by object pairing:

- **Rigid–rigid:** Separating Axis Theorem (SAT) for narrow-phase detection, Sutherland–Hodgman line clipping for contact point generation, and impulse-based resolution.
- **Rigid–soft and soft–soft:** Ray casting for collision detection, with direct positional correction for soft–soft collisions and a mixed impulse/positional approach for mixed cases.
- **Fluid–rigid and fluid–soft:** Ray casting-based detection, combined with buoyancy handling for fluid–rigid interactions.

Broad-phase collision culling uses a Bounding Area Hierarchy (BAH). All three solvers (PGS, XPBD, PBF) run within a shared physics processing loop each frame, with the solvers unified through the impulse bridge described above.

Rigid bodies also support **fracture physics**, using Voronoi cell generation to break objects apart on impact.

## Scripting

The engine core is coded in C++, but nearly all functionality is exposed to Python via pybind11, allowing rapid iteration and direct use of the Python ecosystem.

- **Python version:** 3.11 (later versions are also expected to work).
- **Bridging:** pybind11 provides the C++ ↔ Python translation layer, allowing C++ to call Python functions and vice versa.
- **Editor tooling:** On project setup, alongside the resources folder, a Python virtual environment is configured. The engine compiles a Python module (`.pyd`) linking the C++ core to Python, then generates a `.pyi` stub file (via `pybind-stubgen`) into a `typings` folder so editors such as Visual Studio Code get accurate autocomplete and syntax highlighting for the engine's Python API. VS Code settings are written automatically to link the interpreter and stub path.
- **Script components:** Behavior is attached to entities through script components, with a component registry and support for exporting script properties to the inspector.

Once the environment is set up, standard Python packages (Gymnasium, Stable-Baselines3, NumPy, PyTorch, etc.) can be installed directly into the project's virtual environment to begin building and training RL agents.

## AI Training Features

Reinforcement learning support is delivered as an installable **package** rather than a core feature, keeping the base engine lightweight. Selecting the RL package from the project configuration screen pulls in both the required Python libraries (e.g. NumPy, PyTorch, Gymnasium) and engine-side features (a dedicated `fusionRL` Python API and additional editor windows). Installed and selected packages are synced automatically in the background when a project is opened.

**Demo:** RL agent learning to play Flappy Bird

https://github.com/user-attachments/assets/18549414-6475-4695-be27-09421f8e3032

Key RL capabilities:

- **Agent** Attaching an agent component to an entity marks it as trainable. Combined with a script, it exposes observations, rewards, and actions through both the Python API and the editor inspector. Action and observation spaces follow Gymnasium's space types — `Discrete`, `MultiDiscrete`, `Box`, and `MultiBinary` — with `Box` used as the default observation space.
- **Headless training.** During training, rendering and standard physics/editor processing are disabled ("headless mode") so the simulation isn't capped by OpenGL's 60 FPS rendering limit, significantly speeding up training. The editor instead displays a training monitor (refreshed at a fixed 30 Hz) showing live training statistics. Communication between the training thread and the main thread is synchronized using mutexes for thread safety.
- **Live view.** An optional live view lets you watch the agent train in real time; this can slow down training and is intended for debugging/observation rather than production training runs.
- **Environment implementation.** Any scene built in the engine can act as a Gymnasium-compatible environment. The engine implements the standard `init`, `step`, and `reset` functions: `init` applies the agent's configured action/observation spaces, `step` advances the simulation and accumulates reward and observations, and `reset` reloads the scene.
- **Model training.** Training itself is handled through Stable-Baselines3, letting you choose the algorithm, policy, number of timesteps, save location, and whether to resume from an existing model, directly from the editor.

## Project Files

| Extension | Description |
|---|---|
| `.fusion` | Project save file (settings, main scene path) |
| `.fscene` | Serialized scene file (objects and components) |

Exported projects encrypt Python scripts and project files using a per-export XOR key:

```
final_binary = secret_key XOR raw_binary
raw_binary   = secret_key XOR final_binary
```

## Documentation

The full design document covers the engine's mathematics, physics solvers, ECS, editor, file management, scripting, and reinforcement learning integration in detail.

[Read the full design document](./Fusion_Engine_Design_Document.pdf)

## Status

Fusion Engine is under active development. Interfaces, including the editor UI, are subject to change.
