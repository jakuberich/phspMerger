# Geant4phspMerger & Automation Script 🎉

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)  
![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)

Welcome to the **Geant4phspMerger** project! This repository contains:

- A **C++ tool** that merges one or more IAEA phase space (PHSP) files into a single output file. 📂
- A **Python automation script** that builds and runs the merger tool. 🤖
- A set of **IAEA libraries** (headers and source files) implementing the PHSP file format and low-level I/O routines. 🛠️

This project can be built and executed either automatically (using the provided Python script) or manually via CMake/Make.

---

## Table of Contents 📑

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Building the Project](#building-the-project)
  - [Automated Build with Python](#automated-build-with-python)
  - [Manual Build](#manual-build)
- [Usage](#usage)
  - [Python Automation Script](#python-automation-script)
  - [Merging Files](#merging-files)
- [How It Works](#how-it-works)
- [Customization](#customization)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Overview 🚀

**Geant4phspMerger** is a command-line utility written in C++ that merges multiple IAEA PHSP files into a single output file. It processes associated header files to sum statistical data (e.g., total number of histories, particle counts) and updates the output header accordingly. The tool is designed to integrate smoothly into larger workflows and can be built/executed automatically using a Python script.

---

## Features ✨

- **Multiple File Merging:**  
  Merge two or more IAEA PHSP files by providing a list of input file base names (without extensions). 📁

- **Header Unification:**  
  The tool copies the header from the first input file and updates extra data fields (e.g., extra numbers, extra types) so that the output reflects the full dataset. 📝

- **Statistical Updates:**  
  Original histories and total particle counts are summed across all inputs. 🔢

- **Error Handling:**  
  Robust error handling during record processing – individual errors are logged, and if errors exceed a set threshold, processing for that file is aborted. 🚨

- **Flexible Build Options:**  
  Build the project automatically using the provided Python automation script or manually via CMake/Make. ⚙️

- **Automation Ready:**  
  The Python script automates building, file search, and tool execution, streamlining integration into larger workflows. 🤖

---

## Requirements 📝

- **C++ Compiler** (e.g., GCC)
- **CMake** and **Make** (if using the CMake build method)
- The provided IAEA libraries:
  - `iaea_phsp.h` / `iaea_phsp.cpp`
  - `iaea_header.h` / `iaea_header.cpp`
  - `iaea_record.h` / `iaea_record.cpp`
  - `utilities.h` / `utilities.cpp`
- **Python 3.6+** (for the automation script)

---

## Building the Project 🏗️

You have two options:

### Automated Build with Python

The provided Python script (`phsp_merger_interface.py`) automates the following tasks:

1. **Build Check:**  
   It checks whether the `Geant4phspMerger` executable exists.

2. **Automated Build:**  
   If not, the script creates a build directory, runs CMake, and builds the project using Make.

3. **File Discovery & Execution:**  
   The script recursively searches for `.IAEAheader` files in a specified directory, strips their extensions, and passes the list as arguments to the merger tool, along with a designated output file base name.

To run the automation script, use:

```bash
python phsp_merger_interface.py <directory_path>
```

Example:

```bash
python phsp_merger_interface.py /home/user/input_files
```

### Manual Build

If you prefer to build the project manually, use one of the following methods:

#### With CMake/Make

1. **Create and enter the build directory:**

   ```bash
   mkdir build && cd build
   ```

2. **Run CMake:**

   ```bash
   cmake ..
   ```

3. **Build the project:**

   ```bash
   make
   ```

#### Direct Compilation with GCC

Alternatively, compile directly using:

```bash
g++ -o Geant4phspMerger Geant4phspMerger.cc iaea_phsp.cpp iaea_header.cpp iaea_record.cpp utilities.cpp -lm -lstdc++
```

Ensure that all source files are in the correct locations.

---

## Usage 🔧

### Python Automation Script

The Python automation script (`phsp_merger_interface.py`) handles:

- **Build Automation:**  
  It checks for the executable and builds the project if necessary.

- **Recursive File Search:**  
  It recursively searches a specified directory for `.IAEAheader` files and strips their extensions.

- **Execution:**  
  It calls the merger tool with the discovered input file paths and a predefined output file base name.

Run the script as follows:

```bash
python phsp_merger_interface.py <directory_path>
```

Example:

```bash
python phsp_merger_interface.py /home/user/input_files
```

### Merging Files

The merger tool is a command-line utility that accepts multiple input file base names (without extensions) followed by a single output file base name. For example:

```bash
./Geant4phspMerger inputFile1 inputFile2 inputFile3 mergedOutput
```

This command reads the following files:
- `inputFile1.IAEAheader` and `inputFile1.IAEAphsp`
- `inputFile2.IAEAheader` and `inputFile2.IAEAphsp`
- `inputFile3.IAEAheader` and `inputFile3.IAEAphsp`

... and creates the merged output:
- `mergedOutput.IAEAheader`
- `mergedOutput.IAEAphsp`

**Note:** If a file's header indicates one record more than is physically present, the tool reads one record less to avoid read errors.  
Paths containing spaces should be enclosed in quotes:

```bash
./Geant4phspMerger "path/to/My Input File" "another/inputfile" mergedOutput
```

---

## How It Works 🔍

1. **Header Processing & Unification:**  
   The tool copies the header from the first input file and then examines the extra-data settings (e.g., extra longs) from all input files. It updates the output header to use the maximum extra counts, ensuring that all data is included.

2. **Record Merging:**  
   For each input file, the tool reads (expectedRecords – 1) records (to avoid a potential last-record error) and writes them to the output file while summing statistical data (like histories and particle counts).

3. **Checksum Update:**  
   After merging, the tool calls `iaea_update_header` to recalculate the checksum and update other statistical fields in the output header.

4. **Error Handling:**  
   The tool logs individual read errors and aborts processing for a file if errors exceed a set threshold.

5. **Automation Integration:**  
   The Python script automates the entire build-and-run process, making it easy to integrate into larger workflows.

---

## Customization 🎨

- **Output Path:**  
  Modify the `output_path` variable in the Python script (`phsp_merger_interface.py`) to change where the merged output is stored.

- **File Search Settings:**  
  Adjust the recursive file search parameters in the Python script if needed.

- **Merging Logic:**  
  You can alter the C++ source code to change how records are merged or filtered. For example, you could add conditional filtering based on particle position or energy.

- **Error Threshold:**  
  The constant `ERROR_THRESHOLD` in the C++ code defines how many individual errors are tolerated before a file's processing is aborted.

---

## Troubleshooting 🛠️

- **File Size / Checksum Mismatch:**  
  If you encounter errors such as "file size or byte order mismatch," verify that all input files have consistent formats and byte order. The merged file is often smaller than the sum of the inputs because it contains only one header and removes redundant metadata.

- **Last Record Read Error:**  
  The tool intentionally reads one record less than indicated in the header to avoid reading a non-existent record. This behavior is normal and does not imply data loss.

- **Paths with Whitespaces:**  
  Enclose any file paths containing spaces in quotes.

---

## Contributing 🤝

If you have suggestions for improvements or find bugs, please open an **issue** or submit a **pull request**. Contributions are highly appreciated!

---

## License 📄

This project is licensed under the [MIT License](LICENSE).

---

Happy merging and enjoy using **Geant4phspMerger & Automation Script**! 🎊
