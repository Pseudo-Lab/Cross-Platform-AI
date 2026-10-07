# 00. Environment settings

[한국어](README.md)

## Purpose

Use this lesson to install the tools for the Vulkan compute tutorials.
The C++ example uses Vulkan-Hpp to display the Vulkan loader version.
CMake compiles a simple compute shader into a SPIR-V file.

Use `vulkaninfo` to examine the devices and installed validation layers.

## Required software

| Item | Requirement |
|---|---|
| C++ compiler | Support for the C++20 compiler mode |
| CMake | Version 3.24 or later |
| Vulkan SDK | Version 1.3 or later, including Vulkan headers, Vulkan-Hpp, and `glslc` |
| Vulkan loader | Version 1.3 or later |
| GPU driver | Vulkan 1.3 support |
| Vulkan tools | `vulkaninfo` and `VK_LAYER_KHRONOS_validation` |

## Install the tools

### Windows

#### Install the C++ development tools

Visual Studio includes an IDE and development tools.
Visual Studio Build Tools installs development tools without an IDE.
The installer for either product opens Visual Studio Installer to select components.

1. Download the installer for [Visual Studio 2022 or later, or the corresponding Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/).
2. Run the installer.
3. Select the **Desktop development with C++** workload.

   - MSVC build tools for x64/x86
   - Windows SDK
   - C++ CMake tools for Windows

If you installed **C++ CMake tools for Windows**, a separate CMake installation is not necessary.
For a separate installation, use the [CMake installer](https://cmake.org/download/).

#### Use CMake in a standard terminal

`PATH` is a list of directories that the system searches to find an executable by name.
Add the directory that contains `cmake.exe` to run `cmake` from any working directory.
Relative paths such as `build` still refer to the current working directory.

1. Run this command in a new PowerShell terminal.

   ```powershell
   cmake --version
   ```

2. If the command is not found, find the installed `cmake.exe` file.

Visual Studio includes CMake in this directory below its installation directory.
Use the installation location at the bottom of the installer window to find it.

```text
Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

If you used a separate CMake installer, check the installation location set in that installer.

3. Open **Edit the system environment variables** from Windows Search.
4. Select **Environment Variables**.
5. Select `Path` under **User variables**, then select **Edit**.
6. Select **New** and add the full path to the directory that contains `cmake.exe`.
7. Select **OK** in each window to save the changes.
8. Close the terminal application completely, then open it again.
9. Run `cmake --version` again.

If the command in step 1 succeeds, skip steps 2 to 9.
For an integrated terminal, also restart its application, such as VS Code.

#### Install and check the Vulkan SDK

1. Install the GPU driver from the GPU manufacturer's website.
2. Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) for Windows.
3. Close the terminal application completely, then open it again.
4. Check the SDK path and shader compiler in PowerShell.

   ```powershell
   $env:VULKAN_SDK
   glslc --version
   ```

The default Vulkan SDK installation sets the system environment variable `VULKAN_SDK` and adds the SDK's `Bin` directory to `PATH`.
`VULKAN_SDK` identifies the SDK installation directory.
If the correct path and compiler version appear, no additional environment variable settings are necessary.
See the [Windows SDK guide](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html) for installation details.

#### If environment variables are missing

If the SDK path is empty or `glslc` is not found in a new terminal, check the saved environment variables.

1. Open **Environment Variables** from **Edit the system environment variables**.
2. Find `VULKAN_SDK` under **User variables** and **System variables**.
3. If the variable is missing, add this value under **User variables**.

   | Variable name | Example value |
   |---|---|
   | `VULKAN_SDK` | `C:\VulkanSDK\1.4.341.1` |

4. If `Path` does not include the SDK's `Bin` directory, add its full path to the user `Path` variable.

   ```text
   C:\VulkanSDK\1.4.341.1\Bin
   ```

5. Select **OK** in each window to save the changes.
6. Restart the terminal application.
7. Check the SDK path and the output of `glslc --version` again.

Replace the version in the example with your installed version.
If an existing variable identifies an incorrect path, correct its value.

The PowerShell commands below affect only the current terminal and its child processes.
They do not save permanent settings.

```powershell
$env:VULKAN_SDK = 'C:\VulkanSDK\1.4.341.1'
$env:Path = "$env:VULKAN_SDK\Bin;$env:Path"
```

### Ubuntu 24.04

1. Update the package list.

   ```sh
   sudo apt update
   ```

2. Install the compiler, CMake, and Vulkan development packages.

   ```sh
   sudo apt install build-essential cmake libvulkan-dev vulkan-tools glslc vulkan-validationlayers
   ```

3. Install the Vulkan driver for your GPU.

Ubuntu provides Mesa Vulkan drivers for supported Intel and AMD GPUs.
NVIDIA GPUs require a compatible NVIDIA driver.
Use the driver installation procedure for your Ubuntu release and GPU.

4. Open a new terminal.

For other Linux systems, use the [LunarG Linux SDK guide](https://vulkan.lunarg.com/doc/view/latest/linux/getting_started.html).

## Check the tools

1. Make sure that CMake reports version 3.24 or later.

   ```sh
   cmake --version
   ```

2. Display the shader compiler version.

   ```sh
   glslc --version
   ```

3. Get the Vulkan report.

   ```sh
   vulkaninfo --summary
   ```

4. Find your GPU name and confirm that its `apiVersion` is 1.3 or later.
5. Find `VK_LAYER_KHRONOS_validation` in the layer list.

## Build and run the example

1. Open a terminal in this example's directory (`examples/00-env-settings`).
   From the repository root, use this command to change directories.

   ```sh
   cd examples/00-env-settings
   ```

2. Configure the project in a new build directory.

   `-S .` sets the current directory as the source directory.
   This example builds independently with the `CMakeLists.txt` in its directory.

   On Windows, use this command:

   ```sh
   cmake -S . -B build
   ```

   On Linux, use this command:

   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```

3. Build the example.

   ```sh
   cmake --build build --config Debug --target env_check
   ```

CMake builds `main.cpp` and compiles `check.comp` with `glslc`.
The shader output is `build/check.comp.spv`.
A compiler error stops the build.

4. Run the program.

   For a Windows Visual Studio build, use this command:

   ```powershell
   .\build\Debug\env_check.exe
   ```

   For a Linux Makefiles or Ninja build, use this command:

   ```sh
   ./build/env_check
   ```

The program prints `Vulkan loader version:` and a version number.
If the loader version is below 1.3, it reports an error and returns exit code 1.
`vk::raii::Context` provides access to the loader functions.
`enumerateInstanceVersion()` returns the loader API version without creating an instance.

## Completion criteria

| Check | Required result |
|---|---|
| Device discovery | `vulkaninfo` lists your GPU with API version 1.3 or later |
| Layer installation | `vulkaninfo` lists `VK_LAYER_KHRONOS_validation` |
| C++ build and execution | `env_check` prints a loader version of 1.3 or later and returns exit code 0 |
| Shader compilation | The build creates `check.comp.spv` without compiler errors |

These results confirm the tool installation.
They do not confirm GPU calculations or the device features for later lessons.

## Compile the shader separately

1. Open a terminal in this example's directory (`examples/00-env-settings`).
2. Compile the shader after you complete the build procedure above.

   ```sh
   glslc --target-env=vulkan1.3 check.comp -o build/check-manual.spv
   ```

The `.comp` extension identifies the compute shader stage.
The target option selects Vulkan 1.3 for this shader.
A successful command creates the output file and returns exit code 0.

## Troubleshooting

| Problem | Action |
|---|---|
| `cmake` or `glslc` is missing | Install the tool. Add its directory to `PATH`. Open a new terminal. |
| CMake cannot find a C++ compiler | Install the C++ workload or compiler for your operating system. |
| CMake cannot find Vulkan | Set the SDK path. On Linux, install the development packages. |
| The validation layer is missing | Install the SDK validation layer or the Linux validation package. |
| No Vulkan device appears | Install a compatible GPU driver. |
| CMake reports a generator mismatch | Select an unused build directory for the new generator. |

## References

- [Vulkan Tutorial: Development environment](https://docs.vulkan.org/tutorial/latest/02_Development_environment.html)
- [CMake: FindVulkan](https://cmake.org/cmake/help/v3.24/module/FindVulkan.html)
- [ASD-STE100: Simplified Technical English](https://www.asd-ste100.org/)
