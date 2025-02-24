import os
import sys
import subprocess

def find_files(path, extension):
    """
    Recursively search for files with the given extension in the specified directory.
    """
    for root, dirs, files in os.walk(path):
        for file in files:
            if file.endswith(extension):
                yield os.path.join(root, file)

def remove_extension(filename, extension):
    """
    Return the filename without the specified extension.
    """
    if filename.endswith(extension):
        return filename[:-len(extension)]
    return filename

def build_project(project_dir):
    """
    Build the Geant4phspMerger project using CMake and Make.
    Assumes that project_dir is the directory containing the CMakeLists.txt.
    """
    build_dir = os.path.join(project_dir, "build")
    os.makedirs(build_dir, exist_ok=True)

    # Run CMake configuration
    print("Running CMake configuration...")
    result = subprocess.run(["cmake", ".."], cwd=build_dir)
    if result.returncode != 0:
        print("Error: CMake configuration failed.")
        sys.exit(1)

    # Run Make build
    print("Building the project with Make...")
    result = subprocess.run(["make"], cwd=build_dir)
    if result.returncode != 0:
        print("Error: Building the project failed.")
        sys.exit(1)
    
    return build_dir

if __name__ == "__main__":
    # Check command line arguments for directory to search
    if len(sys.argv) != 2:
        print("Usage: python wywolaj_merger.py <directory_path>")
        sys.exit(1)
    
    # Determine the directory where the script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Define the project directory (relative to the script location)
    project_dir = os.path.join(script_dir, "Geant4phspMerger")
    build_dir = os.path.join(project_dir, "build")
    # Define the name of the executable (assumed to be 'Geant4phspMerger')
    executable_name = "Geant4phspMerger"
    program_path = os.path.join(build_dir, executable_name)

    # Check if the executable exists and is executable
    if not (os.path.exists(program_path) and os.access(program_path, os.X_OK)):
        print("Executable not found or not executable. Initiating build process...")
        build_dir = build_project(project_dir)
        program_path = os.path.join(build_dir, executable_name)
        if not (os.path.exists(program_path) and os.access(program_path, os.X_OK)):
            print("Error: Built executable not found after build process.")
            sys.exit(1)
    else:
        print("Found existing built executable.")

    # Search for files with the .IAEAheader extension in the provided directory
    folder = sys.argv[1]
    ext = ".IAEAheader"
    
    found_files = list(find_files(folder, ext))
    if not found_files:
        print(f"No files found with extension {ext}")
        sys.exit(1)
    
    # Remove the extension from each file path
    input_files = [remove_extension(file, ext) for file in found_files]

    # Define the output path (this can be configurable as needed)
    output_path = "merged"

    # Build the command: executable followed by input file paths and the output path
    cmd = [program_path] + input_files + [output_path]
    
    print("Executing command:", " ".join(cmd))
    
    # Execute the program using subprocess
    result = subprocess.run(cmd)
    
    if result.returncode == 0:
        print("Program executed successfully.")
    else:
        print("An error occurred while executing the program.")
